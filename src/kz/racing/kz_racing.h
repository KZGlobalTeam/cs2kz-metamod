#pragma once
#include "../kz.h"
#include "utils/json.h"
#include "public/steam/isteamugc.h"
#include <vendor/ixwebsocket/ixwebsocket/IXWebSocket.h>

namespace KZ::racing
{
	struct PlayerInfo
	{
		u64 id {};
		std::string name {};

		bool ToJson(Json &json) const;
		bool FromJson(const Json &json);
	};

	struct MapInfo
	{
		std::string mapName;
		u64 workshopID;

		bool ToJson(Json &json) const;
	};

	struct RaceInfo
	{
		std::string mapName;
		u64 workshopID;
		std::string courseName;
		std::string modeName;
		u32 maxTeleports;
		f32 duration;

		bool ToJson(Json &json) const;
	};

	namespace handshake
	{
		struct Hello
		{
			MapInfo mapInfo;

			bool ToJson(Json &json) const;
		};

		struct HelloAck
		{
			std::optional<RaceInfo> raceInfo {};
			// seconds
			f64 heartbeatInterval {};

			bool FromJson(const Json &json);
		};
	}; // namespace handshake

	namespace events
	{
		struct RaceInit
		{
			RaceInfo raceInfo;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct MapUpdated
		{
			MapInfo mapInfo;

			bool ToJson(Json &json) const;
		};

		struct RaceStart
		{
			u32 countdownSeconds;
			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerForfeit
		{
			PlayerInfo player;
			bool manual;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerFinish
		{
			PlayerInfo player;
			f32 time;
			u32 teleportsUsed;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct RaceEnd
		{
			// Time expired if not true.
			// If manual is true, coordinator should force all other servers to terminate the race.
			bool manual;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct RaceResult
		{
			struct Finisher
			{
				PlayerInfo player;
				f32 time;
				u32 teleportsUsed;
				bool completed;

				bool FromJson(const Json &json);
			};

			std::vector<Finisher> finishers;
			bool FromJson(const Json &json);
		};

		struct ChatMessage
		{
			PlayerInfo player;
			std::string message;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};
	}; // namespace events
}; // namespace KZ::racing

struct RaceInfo
{
	enum State
	{
		RACE_NONE,
		RACE_INIT,
		RACE_ONGOING
	};

	State state;
	KZ::racing::events::RaceInit data;
	std::vector<KZ::racing::PlayerInfo> localParticipants;
	// Server-side only
	i32 earliestStartTick;
};

class KZRacingService : public KZBaseService
{
public:
	using KZBaseService::KZBaseService;

	static inline RaceInfo currentRace {};

	virtual void Reset()
	{
		this->timerStartTickServer = {};
	}

	/* ===== Coordinator ===== */
	enum class State
	{
		Uninitialized,
		Configured,
		Connecting,
		Connected,
		HandshakeInitiated,
		HandshakeCompleted,
		Reconnecting,
		Disconnected,
	};

	/**
	 * The current connection state
	 */
	static inline std::atomic<State> state = State::Uninitialized;

	// INVARIANT: should be `nullptr` when `state == Uninitialized`, and a valid pointer otherwise
	static inline std::unique_ptr<ix::WebSocket> socket = nullptr;

	/**
	 * The ID we'll use for the next message we send to the coordinator.
	 */
	static inline std::atomic<u32> nextMessageID = 1;

	/**
	 * A handle to the thread that continuously sends 'ping' messages to the coordinator, so we don't get disconnected for inactivity.
	 */
	static inline struct
	{
		// protects `handle`
		std::mutex mutex;
		std::thread handle;

		// used for waiting for heartbeat timeout and shutdown at the same time
		struct
		{
			std::mutex mutex;
			std::condition_variable cv;
		} shutdownNotification;
	} heartbeatThread;

	static void Init();
	static void Cleanup();
	static void OnActivateServer();
	static void OnServerGamePostSimulate();

	/* ===== Sending events ===== */

	// Note that unlike the global service, these functions do not have callbacks from the coordinator.
	// The server only acts upon receiving broadcasted messages from the coordinator.

	static void SendRaceInit(std::string mapName, u64 workshopID, std::string courseName, std::string modeName, u32 maxTeleports, f32 duration);
	static void SendMapUpdate(std::string mapName, u64 workshopID);
	static void SendRaceStart(u32 countdownSeconds);
	void SendPlayerRegistration();
	void SendPlayerUnregistration();
	void SendAcceptRace();
	void SendForfeitRace(bool manual);
	void SendRaceFinish(f32 time, u32 teleportsUsed);
	static void SendRaceEnd(bool manual);

	void SendChatMessage(const std::string &message);

	/* ===== Receiving events =====*/
	// Called on the WS thread and is therefore async.
	static void OnWebSocketMessage(const ix::WebSocketMessagePtr &message);
	// Called on the main thread.
	static void OnRaceInit(const KZ::racing::events::RaceInit &raceInit);
	static void OnRaceStart(const KZ::racing::events::RaceStart &raceStart);
	static void OnPlayerForfeit(const KZ::racing::events::PlayerForfeit &playerForfeit);
	static void OnPlayerFinish(const KZ::racing::events::PlayerFinish &playerFinish);
	static void OnRaceEnd(const KZ::racing::events::RaceEnd &raceEnd);
	static void OnRaceResult(const KZ::racing::events::RaceResult &raceResult);
	static void OnChatMessage(const KZ::racing::events::ChatMessage &chatMessage);

	/**
	 * Process queued callbacks on the main thread.
	 * Should be called every frame from the game loop.
	 */
	static void ProcessMainThreadCallbacks();

private:
	/**
	 * Helper function called by `OnWebSocketMessage()` if we get an `Open` message.
	 */
	static void WS_OnOpenMessage();

	/**
	 * Helper function called by `OnWebSocketMessage()` if we get a `Close` message.
	 */
	static void WS_OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo);

	/**
	 * Helper function called by `OnWebSocketMessage()` if we get an `Error` message.
	 */
	static void WS_OnErrorMessage(const ix::WebSocketErrorInfo &errorInfo);

	/**
	 * Initiates the WebSocket handshake with the coordinator.
	 */
	static void WS_InitiateHandshake();

	/**
	 * Completes the WebSocket handshake with the coordinator.
	 */
	static void WS_CompleteHandshake(KZ::racing::handshake::HelloAck &&ack);

	/**
	 * The function given to `heartbeatThread` to execute.
	 */
	static void WS_Heartbeat(u64 intervalInSeconds);

public:
	/* ===== Map management ===== */

	// Check if a race is active and update the map to the current race's map.
	static void CheckMap();
	static bool IsMapCorrectForRace();
	static bool IsMapReadyForChange(u64 workshopID);
	static bool IsMapQueuedForDownload(u64 workshopID);
	static void TriggerWorkshopDownload(u64 workshopID);

	static inline struct ItemDownloadHandler
	{
		STEAM_GAMESERVER_CALLBACK_MANUAL(KZRacingService::ItemDownloadHandler, OnAddonDownloaded, DownloadItemResult_t, m_CallbackDownloadItemResult);
	} itemDownloadHandler;

	/* ===== Race management ===== */

	static void BroadcastRaceInfo();

	// Race participation
	void AcceptRace();
	bool IsRaceParticipant();
	static void RemoveLocalRaceParticipant(u64 steamID);
	// Return false if a race is active, the player is one of the participants and the start time hasn't arrived yet.
	// Also returns false if the mode or the course is invalid, or if the player has any active style.
	bool OnTimerStart(u32 courseGUID);

	i32 timerStartTickServer {};
	// Notify the coordinator about the end of the run.
	void OnTimerEndPost(u32 courseGUID, f32 time, u32 teleportsUsed);

	void OnPlayerDisconnect();

private:
	/**
	 * Prepares a message to be sent to the API.
	 */
	template<typename T>
	static bool PrepareMessage(std::string_view event, u32 messageID, const T &data, Json &payload)
	{
		if (KZRacingService::state.load() != State::HandshakeCompleted)
		{
			META_CONPRINTF("[KZ::Racing] WARN: called `SendMessage()` before handshake has completed (state=%i)\n", KZRacingService::state.load());
			return false;
		}

		// clang-format off
		bool success = payload.Set("id", messageID)
			&& payload.Set("event", event)
			&& payload.Set("data", data);
		// clang-format on

		if (!success)
		{
			META_CONPRINTF("[KZ::Global] Failed to serialize message for event `%s`.\n", event);
		}

		return success;
	}

	/**
	 * Sends a message to the API.
	 */
	template<typename T>
	static bool SendMessage(std::string_view event, const T &data)
	{
		Json payload;

		if (!KZRacingService::PrepareMessage(event, KZRacingService::nextMessageID++, data, payload))
		{
			return false;
		}

		KZRacingService::socket->send(payload.ToString());
		return true;
	}

	/**
	 * Callbacks to execute on the main thread as soon as possible.
	 */
	static inline struct
	{
		std::mutex mutex;
		std::vector<std::function<void()>> queue;
	} mainThreadCallbacks {};

	/**
	 * Queues a callback to be executed on the main thread as soon as possible.
	 */
	template<typename CB>
	static void AddMainThreadCallback(CB &&callback)
	{
		std::unique_lock lock(KZRacingService::mainThreadCallbacks.mutex);
		KZRacingService::mainThreadCallbacks.queue.emplace_back(std::forward<CB>(callback));
	}
};
