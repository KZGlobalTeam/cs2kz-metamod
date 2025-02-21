#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>

#include <vendor/ixwebsocket/ixwebsocket/IXWebSocket.h>

#include "utils/json.h"

#include "kz/kz.h"
#include "kz/global/api.h"
#include "kz/global/handshake.h"
#include "kz/global/events.h"
#include "kz/timer/announce.h"

class KZGlobalService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static bool IsAvailable();

	/**
	 * Executes a function with information about the current map.
	 *
	 * `F` should be a function that accepts a single argument of type `const KZ::API::Map*`.
	 */
	template<typename F>
	static auto WithCurrentMap(F &&f)
	{
		std::unique_lock lock(KZGlobalService::currentMap.mutex);
		const KZ::API::Map *currentMap = nullptr;

		if (KZGlobalService::currentMap.data.has_value())
		{
			currentMap = &*KZGlobalService::currentMap.data;
		}

		return f(currentMap);
	}

	template<typename F>
	static auto WithGlobalModes(F &&f)
	{
		std::unique_lock lock(KZGlobalService::globalModes.mutex);
		const std::vector<KZ::API::handshake::HelloAck::ModeInfo> &modes = KZGlobalService::globalModes.data;
		return f(modes);
	}

	template<typename F>
	static auto WithGlobalStyles(F &&f)
	{
		std::unique_lock lock(KZGlobalService::globalStyles.mutex);
		const std::vector<KZ::API::handshake::HelloAck::StyleInfo> &styles = KZGlobalService::globalStyles.data;
		return f(styles);
	}

	static void UpdateRecordCache();

public:
	static void Init();
	static void Cleanup();
	static void RegisterCommands();

	static void OnServerGamePostSimulate();
	static void OnActivateServer();

public:
	void OnPlayerAuthorized();
	void OnClientDisconnect();

	struct
	{
		bool isBanned {};
	} playerInfo {};

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

	/**
	 * Submits a new record to the API.
	 */
	template<typename CB>
	SubmitRecordResult SubmitRecord(u16 filterID, f64 time, u32 teleports, std::string_view modeMD5, void *styles, std::string_view metadata, CB &&cb)
	{
		if (!this->player->IsAuthenticated() && !this->player->hasPrime)
		{
			return SubmitRecordResult::PlayerNotAuthenticated;
		}

		{
			std::unique_lock currentMapLock(KZGlobalService::currentMap.mutex);
			if (!KZGlobalService::currentMap.data.has_value())
			{
				META_CONPRINTF("[KZ::Global] Cannot submit record on non-global map.\n");
				return SubmitRecordResult::MapNotGlobal;
			}
		}

		KZ::API::events::NewRecord data;
		data.playerID = this->player->GetSteamId64();
		data.filterID = filterID;
		data.modeChecksum = modeMD5;
		for (const auto &style : *(std::vector<RecordAnnounce::StyleInfo> *)styles)
		{
			data.styles.push_back({style.name, style.md5});
		}
		data.teleports = teleports;
		data.time = time;
		data.metadata = metadata;

		switch (KZGlobalService::state.load())
		{
			case KZGlobalService::State::HandshakeCompleted:
				KZGlobalService::SendMessage("new-record", data, cb);
				return SubmitRecordResult::Submitted;

			case KZGlobalService::State::Disconnected:
				return SubmitRecordResult::NotConnected;

			default:
				KZGlobalService::AddMainThreadCallback([=]() { KZGlobalService::SendMessage("new-record", data, cb); });
				return SubmitRecordResult::Queued;
		}
	}

	/**
	 * Query the personal best of a player on a certain map, course, mode, style.
	 */
	template<typename CB>
	static bool QueryPB(u64 steamid64, std::string_view targetPlayerName, std::string_view mapName, std::string_view courseNameOrNumber,
						KZ::API::Mode mode, const CUtlVector<CUtlString> &styleNames, CB &&cb)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		std::string_view event("want-personal-best");
		KZ::API::events::WantPersonalBest data = {steamid64, targetPlayerName, mapName, courseNameOrNumber, mode};
		FOR_EACH_VEC(styleNames, i)
		{
			data.styles.emplace_back(styleNames[i].Get());
		}
		return KZGlobalService::SendMessage(event, data, std::move(cb));
	}

	/**
	 * Query the course top on a certain map, mode with a certain limit and offset.
	 */
	template<typename CB>
	static bool QueryCourseTop(std::string_view mapName, std::string_view courseNameOrNumber, KZ::API::Mode mode, u32 limit, u32 offset, CB &&cb)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		std::string_view event("want-course-top");
		KZ::API::events::WantCourseTop data = {mapName, courseNameOrNumber, mode, limit, offset};
		return KZGlobalService::SendMessage(event, data, std::move(cb));
	}

	/**
	 * Query the world record of a course on a certain mode.
	 */
	template<typename CB>
	static bool QueryWorldRecords(std::string_view mapName, std::string_view courseNameOrNumber, KZ::API::Mode mode, CB &&cb)
	{
		if (!KZGlobalService::IsAvailable())
		{
			return false;
		}

		std::string_view event("want-world-records");
		KZ::API::events::WantWorldRecords data = {mapName, courseNameOrNumber, mode};
		return KZGlobalService::SendMessage(event, data, std::move(cb));
	}

private:
	enum class State
	{
		/**
		 * The default state
		 */
		Uninitialized,

		/**
		 * After `Init()` has returned
		 */
		Initialized,

		/**
		 * After we receive an "open" message on the WS thread
		 */
		Connected,

		/**
		 * After we sent the 'Hello' message
		 */
		HandshakeInitiated,

		/**
		 * After we received the 'HelloAck' message
		 */
		HandshakeCompleted,

		/**
		 * After the server closed the connection for a reason that won't change
		 */
		Disconnected,

		/**
		 * After the server closed the connection, but it's still worth to retry the connection
		 */
		DisconnectedButWorthRetrying,
	};

	static inline std::atomic<State> state = State::Uninitialized;

	static inline struct
	{
		std::mutex mutex;
		std::vector<std::function<void()>> queue;
		std::vector<std::function<void()>> whenConnectedQueue;
	} mainThreadCallbacks {};

	// invariant: should be `nullptr` if `state == Uninitialized` and otherwise a valid pointer
	static inline ix::WebSocket *socket = nullptr;
	static inline std::atomic<u32> nextMessageID = 1;

	static inline struct
	{
		std::mutex mutex;
		std::unordered_map<u32, std::function<void(u32, const Json &)>> queue;
	} messageCallbacks {};

	static inline struct
	{
		std::mutex mutex;
		std::optional<KZ::API::Map> data;
	} currentMap {};

	static inline struct
	{
		std::mutex mutex;
		std::vector<KZ::API::handshake::HelloAck::ModeInfo> data;
	} globalModes;

	static inline struct
	{
		std::mutex mutex;
		std::vector<KZ::API::handshake::HelloAck::StyleInfo> data;
	} globalStyles;

	static void EnforceConVars();
	static void RestoreConVars();

	static void OnWebSocketMessage(const ix::WebSocketMessagePtr &message);
	static void InitiateHandshake();
	static void CompleteHandshake(KZ::API::handshake::HelloAck &ack);

	template<typename CB>
	static void AddMainThreadCallback(CB &&callback)
	{
		std::unique_lock lock(KZGlobalService::mainThreadCallbacks.mutex);
		KZGlobalService::mainThreadCallbacks.queue.emplace_back(std::move(callback));
	}

	template<typename CB>
	static void AddWhenConnectedCallback(CB &&callback)
	{
		std::unique_lock lock(KZGlobalService::mainThreadCallbacks.mutex);
		KZGlobalService::mainThreadCallbacks.whenConnectedQueue.emplace_back(std::move(callback));
	}

	template<typename CB>
	static void AddMessageCallback(u32 messageID, CB &&callback)
	{
		std::unique_lock lock(KZGlobalService::messageCallbacks.mutex);
		KZGlobalService::messageCallbacks.queue[messageID] = std::move(callback);
	}

	static void ExecuteMessageCallback(u32 messageID, const Json &payload);

	static bool PrepareMessage(std::string_view event, u32 messageID, const KZ::API::handshake::Hello &data, Json &payload)
	{
		if (KZGlobalService::state.load() != State::Connected)
		{
			META_CONPRINTF("[KZ::Global] WARN: called `SendMessage()` before connection was established (state=%i)\n", KZGlobalService::state.load());
			return false;
		}

		return payload.Set("id", messageID) && data.ToJson(payload);
	}

	template<typename T>
	static bool PrepareMessage(std::string_view event, u32 messageID, const T &data, Json &payload)
	{
		if (KZGlobalService::state.load() != State::HandshakeCompleted)
		{
			META_CONPRINTF("[KZ::Global] WARN: called `SendMessage()` before handshake has completed (state=%i)\n", KZGlobalService::state.load());
			return false;
		}

		// clang-format off
		return payload.Set("id", messageID)
			&& payload.Set("event", event)
			&& payload.Set("data", data);
		// clang-format on
	}

	template<typename T>
	static bool SendMessage(std::string_view event, const T &data)
	{
		Json payload;

		if (!KZGlobalService::PrepareMessage(event, KZGlobalService::nextMessageID++, data, payload))
		{
			return false;
		}

		KZGlobalService::socket->send(payload.ToString());
		return true;
	}

	template<typename T, typename CB>
	static bool SendMessage(std::string_view event, const T &data, CB &&callback)
	{
		u32 messageID = KZGlobalService::nextMessageID++;
		Json payload;

		if (!KZGlobalService::PrepareMessage(event, messageID, data, payload))
		{
			return false;
		}

		// clang-format off
		KZGlobalService::AddMessageCallback(messageID, [callback = std::move(callback)](u32 messageID, const Json& payload)
		{
			if (!payload.IsValid())
			{
				META_CONPRINTF("[KZ::Global] WebSocket message is not valid JSON.\n");
				return;
			}

			std::remove_reference_t<typename decltype(std::function(callback))::argument_type> decoded;

			if (!payload.Get("data", decoded))
			{
				META_CONPRINTF("[KZ::Global] WebSocket message does not contain a valid `data` field.\n");
				return;
			}

			callback(decoded);
		});
		// clang-format on

		KZGlobalService::socket->send(payload.ToString());
		return true;
	}
};
