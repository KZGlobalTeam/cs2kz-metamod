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

	struct RaceSpec
	{
		u32 workshopID;
		std::string courseName;
		std::string modeName;
		f64 maxDurationSeconds;
		u32 maxTeleports;

		bool ToJson(Json &json) const;
		bool FromJson(const Json &json);
	};

	namespace events
	{
		struct ChatMessage
		{
			PlayerInfo player;
			std::string content;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct RaceInitialized
		{
			RaceSpec spec;

			bool FromJson(const Json &json);
		};

		struct Ready
		{
			bool ToJson(Json &json) const
			{
				return true;
			}
		};

		struct ServerJoinRace
		{
			std::string name;

			bool FromJson(const Json &json);
		};

		struct Unready
		{
			bool ToJson(Json &json) const
			{
				return true;
			}
		};

		struct ServerLeaveRace
		{
			std::string name;

			bool FromJson(const Json &json);
		};

		struct PlayerJoinRace
		{
			PlayerInfo player;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerLeaveRace
		{
			PlayerInfo player;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct StartRace
		{
			f64 countdownSeconds;

			bool FromJson(const Json &json);
		};

		struct PlayerFinish
		{
			PlayerInfo player;
			u32 teleports;
			f64 timeSeconds;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerDisconnect
		{
			PlayerInfo player;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerSurrender
		{
			PlayerInfo player;

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct RaceResults
		{
			struct Participant
			{
				enum class State
				{
					Disconnected,
					Surrendered,
					DidNotFinish,
					Finished,
				};

				u64 id;
				std::string name;
				State state;

				// These are only initialized when `state == State::Finished`.
				u32 teleports;
				f64 timeSeconds;

				bool FromJson(const Json &json);
			};

			std::vector<Participant> participants;

			bool FromJson(const Json &json);
		};

		struct RaceFinished
		{
			RaceResults results;

			bool ToJson(Json &json) const
			{
				return true;
			}

			bool FromJson(const Json &json);
		};

		struct CancelRace
		{
			bool ToJson(Json &json) const
			{
				return true;
			}
		};

		struct RaceCancelled
		{
			bool FromJson(const Json &json)
			{
				return true;
			}
		};

	}; // namespace events
}; // namespace KZ::racing

struct RaceInfo
{
	enum class State
	{
		None,
		Init,
		Ongoing
	};

	State state = State::None;
	KZ::racing::RaceSpec spec;
	std::vector<KZ::racing::PlayerInfo> localParticipants;
	// This also includes people who surrendered.
	std::vector<KZ::racing::PlayerInfo> localFinishers;
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
		Disconnected,
	};

	/**
	 * The current connection state
	 */
	static inline std::atomic<State> state = State::Uninitialized;

	// INVARIANT: should be `nullptr` when `state == Uninitialized`, and a valid pointer otherwise
	static inline std::unique_ptr<ix::WebSocket> socket = nullptr;

	static void Init();
	static void Cleanup();
	static void OnActivateServer();
	static void OnServerGamePostSimulate();

	/* ===== Sending events ===== */

	// Note that unlike the global service, these functions do not have callbacks from the coordinator.
	// The server only acts upon receiving broadcasted messages from the coordinator.

	static void SendInitRace(u32 workshopID, std::string courseName, std::string modeName, f64 maxDurationSeconds, u32 maxTeleports);
	static void SendCancelRace();
	static void SendReady();
	static void SendUnready();
	void SendJoinRace();
	void SendLeaveRace();
	void SendDisconnect();
	void SendSurrenderRace();
	void SendFinishRace(f64 timeSeconds, u32 teleports);
	static void SendRaceFinished();

	void SendChatMessage(const std::string &message);

	/* ===== Receiving events =====*/
	// Called on the WS thread and is therefore async.
	static void OnWebSocketMessage(const ix::WebSocketMessagePtr &message);
	// Called on the main thread.
	static void OnChatMessage(const KZ::racing::events::ChatMessage &message);
	static void OnRaceInitialized(const KZ::racing::events::RaceInitialized &message);
	static void OnStartRace(const KZ::racing::events::StartRace &message);
	static void OnRaceCancelled(const KZ::racing::events::RaceCancelled &message);
	static void OnRaceFinished(const KZ::racing::events::RaceFinished &message);
	static void OnServerJoinRace(const KZ::racing::events::ServerJoinRace &message);
	static void OnServerLeaveRace(const KZ::racing::events::ServerLeaveRace &message);
	static void OnPlayerJoinRace(const KZ::racing::events::PlayerJoinRace &message);
	static void OnPlayerLeaveRace(const KZ::racing::events::PlayerLeaveRace &message);
	static void OnPlayerFinish(const KZ::racing::events::PlayerFinish &message);
	static void OnPlayerDisconnect(const KZ::racing::events::PlayerDisconnect &message);
	static void OnPlayerSurrender(const KZ::racing::events::PlayerSurrender &message);

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
	void SurrenderRace();
	bool IsRaceParticipant();
	static void RemoveLocalRaceParticipant(u64 steamID);

	bool CanTeleport();
	// Return false if a race is active, the player is one of the participants and the start time hasn't arrived yet.
	// Also returns false if the mode or the course is invalid, or if the player has any active style.
	bool OnTimerStart(u32 courseGUID);

	i32 timerStartTickServer {};
	// Notify the coordinator about the end of the run.
	void OnTimerEndPost(u32 courseGUID, f32 time, u32 teleportsUsed);

	void OnClientDisconnect();

private:
	/**
	 * Prepares a message to be sent to the API.
	 */
	template<typename T>
	static bool PrepareMessage(std::string_view event, const T &data, Json &payload)
	{
		bool success = payload.Set("event", event) && payload.Set("data", data);

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
		if (KZRacingService::state.load() != KZRacingService::State::Connected)
		{
			return false;
		}

		Json payload;

		if (!KZRacingService::PrepareMessage(event, data, payload))
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
