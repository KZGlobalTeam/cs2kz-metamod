#pragma once
#include "../kz.h"
#include "utils/json.h"
#include "utils/ws.h"
#include "public/steam/isteamugc.h"

namespace KZ::racing
{
	struct RaceConfig
	{
		std::string mapName;
		std::string courseName;
		u64 workshopID;
		std::string modeName;
		std::optional<u32> maxDurationSeconds;
		std::optional<u32> maxTeleports;

		bool FromJson(const Json &json);
	};

	struct RaceResult
	{
		enum class Status
		{
			Finished,
			Surrendered,
			Disconnected,
			DidNotFinish,
		};

		Status status;
		std::string playerName;

		// these two are only present if `status == Finished`
		std::optional<f64> timeSeconds;
		std::optional<u32> teleports;

		bool FromJson(const Json &json);
	};

	namespace events
	{
		struct ChatMessage
		{
			inline static constexpr const char *tag = "chat_message";

			std::string content;

			// SteamID or name depending on whether we are the sender or recipient
			std::string player;

			ChatMessage() = default;

			ChatMessage(const std::string &content, u64 steamID) : content(content), player(std::to_string(steamID)) {}

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct RaceConfigured
		{
			inline static constexpr const char *tag = "race_configured";

			RaceConfig conf;

			bool FromJson(const Json &json);
		};

		struct RaceStarting
		{
			inline static constexpr const char *tag = "race_starting";

			u64 countdownSeconds;

			bool FromJson(const Json &json);
		};

		struct RaceCancelled
		{
			inline static constexpr const char *tag = "race_cancelled";

			bool FromJson(const Json &json);
		};

		struct RaceCompleted
		{
			inline static constexpr const char *tag = "race_completed";

			std::vector<RaceResult> results;

			bool FromJson(const Json &json);
		};

		struct PlayerReady
		{
			inline static constexpr const char *tag = "player_ready";

			// SteamID or name depending on whether we are the sender or recipient
			std::string player;

			PlayerReady() = default;

			PlayerReady(u64 steamID) : player(std::to_string(steamID)) {}

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerFinished
		{
			inline static constexpr const char *tag = "player_finished";

			// SteamID or name depending on whether we are the sender or recipient
			std::string player;
			f64 timeSeconds;
			u32 teleports;

			PlayerFinished() = default;

			PlayerFinished(u64 steamID, f64 timeSeconds, u32 teleports)
				: player(std::to_string(steamID)), timeSeconds(timeSeconds), teleports(teleports)
			{
			}

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerSurrendered
		{
			inline static constexpr const char *tag = "player_surrendered";

			// SteamID or name depending on whether we are the sender or recipient
			std::string player;

			PlayerSurrendered() = default;

			PlayerSurrendered(u64 steamID) : player(std::to_string(steamID)) {}

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
		};

		struct PlayerDisconnected
		{
			inline static constexpr const char *tag = "player_disconnected";

			// SteamID or name depending on whether we are the sender or recipient
			std::string player;

			PlayerDisconnected() = default;

			PlayerDisconnected(u64 steamID) : player(std::to_string(steamID)) {}

			bool ToJson(Json &json) const;
			bool FromJson(const Json &json);
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
	KZ::racing::RaceConfig conf;
	std::vector<u64> localParticipants;
	// This also includes people who surrendered.
	std::vector<u64> localFinishers;
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
	static inline std::unique_ptr<KZWebSocket::Handle> socket = nullptr;

	static void Init();
	static void Cleanup();

	/**
	 * Applies a reloaded server configuration by restarting the WebSocket connection.
	 *
	 * Must be called on the main thread. Blocks until the WebSocket thread has been joined.
	 */
	static void ReloadConfig();
	static void OnActivateServer();
	static void OnServerGamePostSimulate();

	/* ===== Sending events ===== */

	// Note that unlike the global service, these functions do not have callbacks from the coordinator.
	// The server only acts upon receiving broadcasted messages from the coordinator.

	void SendReady();
	void SendDisconnected();
	void SendSurrenderRace();
	void SendFinishRace(f64 timeSeconds, u32 teleports);

	void SendChatMessage(const std::string &message);

	/* ===== Receiving events =====*/
	// Called on the WS thread and is therefore async.
	static void OnWebSocketMessage(KZWebSocket &socket, const ix::WebSocketMessagePtr &message);
	// Called on the main thread.
	static void OnChatMessage(const KZ::racing::events::ChatMessage &message);
	static void OnRaceConfigured(const KZ::racing::events::RaceConfigured &message);
	static void OnRaceStarting(const KZ::racing::events::RaceStarting &message);
	static void OnRaceCancelled(const KZ::racing::events::RaceCancelled &message);
	static void OnRaceCompleted(const KZ::racing::events::RaceCompleted &message);
	static void OnPlayerReady(const KZ::racing::events::PlayerReady &message);
	static void OnPlayerFinished(const KZ::racing::events::PlayerFinished &message);
	static void OnPlayerSurrendered(const KZ::racing::events::PlayerSurrendered &message);
	static void OnPlayerDisconnected(const KZ::racing::events::PlayerDisconnected &message);

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
	 * Sends a message to the API.
	 */
	template<typename T>
	static bool SendMessage(const T &data)
	{
		if (KZRacingService::state.load() != KZRacingService::State::Connected)
		{
			return false;
		}

		return KZRacingService::socket->SendMessage(data);
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
