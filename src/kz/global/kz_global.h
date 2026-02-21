#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include <vendor/ixwebsocket/ixwebsocket/IXWebSocket.h>

#include "common.h"
#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/timer/announce.h"
#include "utils/json.h"
#include "messages.h"

class KZGlobalService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	enum class MessageCallbackCancelReason
	{
		Timeout,
		Disconnect,
	};

	struct PlayerIdentifier
	{
		inline static PlayerIdentifier SteamID(u64 steamID)
		{
			PlayerIdentifier identifier;
			identifier.value = std::to_string(steamID);
			return identifier;
		}

		inline static PlayerIdentifier Name(std::string_view name)
		{
			PlayerIdentifier identifier;
			identifier.value = name;
			return identifier;
		}

		inline std::string_view Value() const
		{
			return this->value;
		}

	private:
		std::string value;
	};

private:
	class MessageCallbackInternal
	{
	public:
		using CancelReason = MessageCallbackCancelReason;

		std::chrono::time_point<std::chrono::system_clock> sentAt;
		std::chrono::seconds expiresAfter = std::chrono::seconds(5);

		virtual void OnResponse(u32 messageID, const Json &payload) = 0;

		virtual void OnError(u32 messageID, const KZ::api::messages::Error &error)
		{
			META_CONPRINTF("[KZ::Global] Received error response to WebSocket message (id=%i): %s\n", messageID, error.message.c_str());
		}

		virtual void OnCancelled(u32 messageID, CancelReason reason)
		{
			const char *reasonStr = "";

			switch (reason)
			{
				case CancelReason::Timeout:
				{
					reasonStr = "timed out";
				}
				break;

				case CancelReason::Disconnect:
				{
					reasonStr = "disconnected";
				}
				break;
			}

			META_CONPRINTF("[KZ::Global] Cancelled WebSocket message (id=%i, %s)\n", messageID, reasonStr);
		}
	};

public:
	template<typename Response>
	class MessageCallback : public MessageCallbackInternal
	{
		std::function<void(const Response &)> onResponse;
		std::function<void(const KZ::api::messages::Error &)> onError;
		std::function<void(CancelReason)> onCancelled;

		void OnResponse(u32 messageID, const Json &payload) override
		{
			Response response;

			if (payload.Decode(response))
			{
				this->onResponse(response);
			}
			else
			{
				META_CONPRINTF("[KZ::Global] Received unknown payload as WebSocket response. (id=%i)\n", messageID);
			}
		}

		void OnError(u32 messageID, const KZ::api::messages::Error &error) override
		{
			MessageCallbackInternal::OnError(messageID, error);
			this->onError(error);
		}

		void OnCancelled(u32 messageID, CancelReason reason) override
		{
			MessageCallbackInternal::OnCancelled(messageID, reason);
			this->onCancelled(reason);
		}

	public:
		// clang-format off
		template<typename OnResponse, typename... Args>
		explicit MessageCallback(OnResponse &&onResponse, Args &&...args)
			: onResponse([cb = std::bind(std::move(onResponse), std::placeholders::_1, std::forward<Args>(args)...)] (const Response &response) { cb(response); })
			, onError([](const KZ::api::messages::Error &) {})
			, onCancelled([](CancelReason) {})
		// clang-format on
		{
		}

		inline void Timeout(std::chrono::seconds duration)
		{
			this->expiresAfter = duration;
		}

		template<typename CB>
		void OnError(CB &&onError)
		{
			this->onError = std::move(onError);
		}

		template<typename CB>
		void OnCancelled(CB &&onCancelled)
		{
			this->onCancelled = std::move(onCancelled);
		}
	};

private:
	enum class State
	{
		/**
		 * The state before `Init()` is called and after `Cleanup()` has been called.
		 */
		Uninitialized,

		/**
		 * The state after `Init()` has been called, when the WebSocket has been configured, but not yet started.
		 */
		Configured,

		/**
		 * The state after `WS::socket->start()` has been called.
		 */
		Connecting,

		/**
		 * The state after the WebSocket connection has been fully established.
		 */
		Connected,

		/**
		 * The state after we sent our `hello` message to the API.
		 */
		HandshakeInitiated,

		/**
		 * The state after we received a `hello_ack` response to our `hello` message from the API.
		 *
		 * This is the final "in operation" state when the connection can actually be "used" for normal message exchange.
		 */
		HandshakeCompleted,

		/**
		 * The state after we've been disconnected but haven't yet reconnected.
		 */
		Reconnecting,

		/**
		 * The state when we're not connected and don't plan on connecting anymore either.
		 */
		Disconnected,
	};

	static inline std::atomic<State> state = State::Uninitialized;

	/**
	 * API information about the current map.
	 */
	static inline struct
	{
		std::mutex mutex;
		std::optional<KZ::api::Map> info;
	} currentMap;

	/**
	 * Functions that must be executed on the main thread.
	 *
	 * These are used for lifecycle management and state transitions that need to happen on the main thread
	 * (e.g. because we need to get the curren map name).
	 */
	static inline struct
	{
		std::mutex mutex;
		std::optional<KZ::api::messages::handshake::HelloAck> helloAck;
		std::vector<std::function<void()>> queue;
	} callbacks;

	template<typename CB>
	inline static void QueueCallback(CB &&callback)
	{
		{
			std::lock_guard _guard(KZGlobalService::callbacks.mutex);
			KZGlobalService::callbacks.queue.emplace_back(std::move(callback));
		}
	}

	/**
	 * All the WebSocket-related state.
	 */
	static inline struct WS
	{
		/**
		 * A partially parsed WebSocket message.
		 */
		struct ReceivedMessage
		{
			/**
			 * The message ID.
			 *
			 * This is either the same ID as a message we sent earlier, indicating a response, or 0.
			 */
			u32 id;

			/**
			 * The message we sent earlier, to which this is the reply, triggered an error.
			 */
			bool isError;

			/**
			 * The rest of the payload, which will be parsed according to `MessageType` later.
			 */
			Json payload;
		};

		// INVARIANT: should be `nullptr` when `state == Uninitialized`, and a valid pointer otherwise
		static inline std::unique_ptr<ix::WebSocket> socket = nullptr;

		/**
		 * WebSocket messages we have received, but not yet processed.
		 *
		 * These are processed on the main thread, and potentially passed into callbacks stored in
		 * `messageCallbacks.queue`.
		 */
		static inline struct
		{
			std::mutex mutex;
			std::vector<ReceivedMessage> queue;
		} receivedMessages;

		/**
		 * Callbacks for sent messages we expect to get a reply to.
		 *
		 * The key is the message ID we used to send the original message, and the API will respond with a message that
		 * has the same ID. The callback itself is given the message data extracted when we received the message.
		 */
		static inline struct
		{
			std::mutex mutex;
			std::unordered_map<u32, std::unique_ptr<MessageCallbackInternal>> callbacks;
		} messageCallbacks;

		/**
		 * Callback for `IXWebSocket`; called on every received message.
		 */
		static void OnMessage(const ix::WebSocketMessagePtr &message);

		/**
		 * Helper function called by `OnMessage()` if we get an `Open` message.
		 */
		static void OnOpenMessage();

		/**
		 * Helper function called by `WS_OnMessage()` if we get a `Close` message.
		 */
		static void OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo);

		/**
		 * Helper function called by `WS_OnMessage()` if we get an `Error` message.
		 */
		static void OnErrorMessage(const ix::WebSocketErrorInfo &errorInfo);

		/**
		 * Initiates the WebSocket handshake with the API.
		 *
		 * This sends the `hello` message.
		 */
		static void InitiateHandshake();

		/**
		 * Completes the WebSocket handshake with the API.
		 *
		 * This is called once we have received the `hello_ack` response to our `hello` message we sent earlier in
		 * `WS_InitiateHandshake()`.
		 */
		static void CompleteHandshake(KZ::api::messages::handshake::HelloAck &&ack);

		/**
		 * Sends the given payload as a WebSocket message.
		 */
		template<typename Payload>
		static bool SendMessage(const Payload &payload)
		{
			u32 messageID;
			const char *messageType;
			Json messagePayload;
			return SendMessageImpl(messageID, messageType, messagePayload, payload);
		}

		/**
		 * Sends the given payload as a WebSocket message and queues the given `callback` to be executed when a response is received.
		 */
		template<typename Payload, typename Callback>
		static bool SendMessage(const Payload &payload, Callback &&callback)
		{
			u32 messageID;
			const char *messageType;
			Json messagePayload;

			std::unique_ptr<MessageCallbackInternal> erasedCallback = std::make_unique<Callback>(std::move(callback));

			if (!SendMessageImpl(messageID, messageType, messagePayload, payload))
			{
				return false;
			}

			erasedCallback->sentAt = std::chrono::system_clock::now();

			{
				std::lock_guard _guard(messageCallbacks.mutex);
				messageCallbacks.callbacks.emplace(messageID, std::move(erasedCallback));
			}

			return true;
		}

	private:
		static inline std::atomic<u32> nextMessageID = 1;

		static inline u32 NextMessageID()
		{
			return nextMessageID.fetch_add(1, std::memory_order_relaxed);
		}

		/**
		 * Implementation detail of `SendMessage()`.
		 *
		 * It initializes the first 3 parameters and encodes `Payload`.
		 */
		template<typename Payload>
		static bool SendMessageImpl(u32 &messageID, const char *&messageType, Json &messagePayload, const Payload &payload)
		{
			messageID = NextMessageID();
			META_CONPRINTF("[KZ::Global] assigned message ID %i\n", messageID);
			messageType = Payload::Name();

			if (!messagePayload.Set("id", messageID))
			{
				return false;
			}

			if (!messagePayload.Set("event", messageType))
			{
				return false;
			}

			if (!messagePayload.Set("data", payload))
			{
				return false;
			}

			std::string encodedPayload = messagePayload.ToString();
			socket->send(encodedPayload);

			// clang-format off
			META_CONPRINTF("[KZ::Global] Sent WebSocket message. (id=%i, type=%s)\n"
					"------------------------------------\n"
					"%s\n"
					"------------------------------------\n",
					messageID, messageType, encodedPayload.c_str());
			// clang-format on

			return true;
		}
	} ws;

	/**
	 * Information about the current player we received from the API when the player connected.
	 */
	struct
	{
		bool isBanned;
	} playerInfo;

public:
	static void Init();
	static void Cleanup();

	inline static bool IsAvailable()
	{
		return state.load() == State::HandshakeCompleted;
	}

	template<typename F>
	inline static auto WithCurrentMap(F &&f)
	{
		std::lock_guard _guard(currentMap.mutex);
		const std::optional<KZ::api::Map> &mapInfo = currentMap.info;
		return f(mapInfo);
	}

	struct GlobalModeChecksums
	{
		std::string vanilla;
		std::string classic;
	};

	struct GlobalStyleChecksum
	{
		std::string style;
		std::string checksum;
	};

	template<typename F>
	inline static auto WithGlobalModes(F &&f)
	{
		std::lock_guard _guard(globalModes.mutex);
		const GlobalModeChecksums &checksums = globalModes.checksums;
		return f(checksums);
	}

	template<typename F>
	inline static auto WithGlobalStyles(F &&f)
	{
		std::lock_guard _guard(globalStyles.mutex);
		const std::vector<GlobalStyleChecksum> &checksums = globalStyles.checksums;
		return f(checksums);
	}

	static void UpdateRecordCache();

	struct RecordData
	{
		u16 filterID;
		f64 time;
		u32 teleports;
		std::string_view modeMD5;
		std::vector<RecordAnnounce::StyleInfo> styles;
		std::string_view metadata;
	};

	enum class SubmitRecordResult
	{
		/**
		 * We are not connected to the API and also won't connect later.
		 */
		NotConnected,

		/**
		 * The player is not authenticated / does not have prime status.
		 */
		PlayerNotAuthenticated,

		/**
		 * The current map is not global.
		 */
		MapNotGlobal,

		/**
		 * We are not currently connected to the API, but we queued up the record to be submitted later (if possible).
		 */
		Queued,

		/**
		 * The record was submitted.
		 */
		Submitted,
	};

	template<typename CB>
	SubmitRecordResult SubmitRecord(const RecordData &data, CB &&callback)
	{
		if (!this->player->IsAuthenticated() && !this->player->hasPrime)
		{
			return SubmitRecordResult::PlayerNotAuthenticated;
		}

		// clang-format off
		bool currentMapIsGlobal = KZGlobalService::WithCurrentMap([](const std::optional<KZ::api::Map> &mapInfo) {
			return mapInfo.has_value();
		});
		// clang-format on

		if (!currentMapIsGlobal)
		{
			META_CONPRINTF("[KZ::Global] Cannot submit record on non-global map.\n");
			return SubmitRecordResult::MapNotGlobal;
		}

		KZ::api::messages::NewRecord message;
		message.playerID = this->player->GetSteamId64();
		message.filterID = data.filterID;
		message.modeChecksum = data.modeMD5;
		for (const RecordAnnounce::StyleInfo &style : data.styles)
		{
			message.styles.push_back({style.name, style.md5});
		}
		message.teleports = data.teleports;
		message.time = data.time;
		message.metadata = data.metadata;

		// clang-format off
		auto sendMessage = [message = std::move(message), callback = std::move(callback)]() mutable {
			KZGlobalService::WS::SendMessage(message, std::move(callback));
		};
		// clang-format on

		switch (KZGlobalService::state.load())
		{
			case KZGlobalService::State::HandshakeCompleted:
				sendMessage();
				return SubmitRecordResult::Submitted;

			case KZGlobalService::State::Disconnected:
				return SubmitRecordResult::NotConnected;

			default:
				KZGlobalService::QueueCallback(std::move(sendMessage));
				return SubmitRecordResult::Queued;
		}
	}

	struct QueryPBParams
	{
		PlayerIdentifier player;
		std::string_view map;
		std::string_view course;
		KZ::api::Mode mode;
		std::vector<std::string_view> styles;
	};

	template<typename CB>
	static bool QueryPB(const QueryPBParams &params, CB &&callback)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		KZ::api::messages::WantPersonalBest message;
		message.player = params.player.Value();
		message.map = params.map;
		message.course = params.course;
		message.mode = params.mode;
		message.styles = params.styles;

		return KZGlobalService::WS::SendMessage(message, std::move(callback));
	}

	struct QueryCourseTopParams
	{
		std::string_view map;
		std::string_view course;
		KZ::api::Mode mode;
		u32 limit;
		u32 offset;
	};

	template<typename CB>
	static bool QueryCourseTop(const QueryCourseTopParams &params, CB &&callback)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		KZ::api::messages::WantCourseTop message;
		message.map = params.map;
		message.course = params.course;
		message.mode = params.mode;
		message.limit = params.limit;
		message.offset = params.offset;

		return KZGlobalService::WS::SendMessage(std::move(message), std::move(callback));
	}

	template<typename CB>
	static bool QueryWorldRecords(std::string_view map, std::string_view course, KZ::api::Mode mode, CB &&callback)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		KZ::api::messages::WantWorldRecords message;
		message.map = map;
		message.course = course;
		message.mode = mode;

		return KZGlobalService::WS::SendMessage(std::move(message), std::move(callback));
	}

	void PrintAnnouncements();

	static void OnActivateServer();
	static void OnServerGamePostSimulate();

	static void EnforceConVars();
	static void RestoreConVars();

	void OnPlayerAuthorized();
	void OnClientDisconnect();

	/**
	 * Whether the player is globally banned.
	 */
	bool IsBanned() const
	{
		return this->playerInfo.isBanned;
	}

private:
	/**
	 * API information about modes.
	 */
	static inline struct
	{
		std::mutex mutex;
		GlobalModeChecksums checksums;
	} globalModes;

	/**
	 * API information about styles.
	 */
	static inline struct
	{
		std::mutex mutex;
		std::vector<GlobalStyleChecksum> checksums;
	} globalStyles;

	/**
	 * API announcements.
	 */
	static inline struct
	{
		std::mutex mutex;
		std::vector<KZ::api::messages::handshake::HelloAck::Announcement> announcements;
	} globalAnnouncements;

private:
	static void OnMapInfo(const std::optional<KZ::api::Map> &mapInfo);
	static void OnPlayerJoinAck(const KZ::api::messages::PlayerJoinAck &ack, u64 steamID);
	static void OnWorldRecordsForCache(const KZ::api::messages::WorldRecordsForCache &records);
};
