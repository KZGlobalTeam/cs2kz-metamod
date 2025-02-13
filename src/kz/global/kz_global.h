#pragma once

#include <vendor/ixwebsocket/ixwebsocket/IXWebSocket.h>

#include "utils/json.h"

#include "kz/kz.h"
#include "kz/global/api/api.h"
#include "kz/global/api/events.h"
#include "kz/global/api/maps.h"

class KZGlobalService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	template<typename... Args>
	using Callback = std::function<void(Args...)>;

	static void Init();
	static void Cleanup();
	static void RegisterCommands();

	static const char *ApiURL()
	{
		return KZGlobalService::apiUrl.empty() ? nullptr : KZGlobalService::apiUrl.c_str();
	}

	/**
	 * Returns whether we are currently connected to the API.
	 */
	static bool IsConnected()
	{
		// clang-format off

		return (KZGlobalService::apiSocket != nullptr)
			&& (KZGlobalService::apiSocket->getReadyState() == ix::ReadyState::Open)
			&& (KZGlobalService::handshakeCompleted);

		// clang-format on
	}

	/**
	 * Returns whether we might connect to the API in the future.
	 */
	static bool MightConnect()
	{
		// clang-format off

		return (KZGlobalService::apiSocket != nullptr)
			&& (KZGlobalService::apiSocket->getReadyState() != ix::ReadyState::Open);

		// clang-format on
	}

	/**
	 * Executes the given function `f` with a pointer to the current map (if any).
	 *
	 * This function will lock access to the current map and therefore should complete quickly.
	 */
	template<typename F>
	static auto WithCurrentMap(F &&f)
	{
		std::unique_lock currentMapLock(KZGlobalService::currentMapMutex);
		return f(KZGlobalService::currentMap.has_value() ? &KZGlobalService::currentMap->info : nullptr);
	}

	/**
	 * Update the global records cache.
	 */
	static void UpdateGlobalCache();

	/**
	 * Enforce the convar values to be the default values.
	 */
	static void EnforceConVars();
	/**
	 * Restore the convar values to the original, unenforced state.
	 */
	static void RestoreConVars();

	static void OnActivateServer();
	static void OnServerGamePostSimulate();

	void OnPlayerAuthorized();
	void OnClientDisconnect();

	enum class SubmitRecordResult
	{
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
	 *
	 * Returns whether the record was submitted. This could be false if the map is not global.
	 */
	SubmitRecordResult SubmitRecord(u16 filterID, f64 time, u32 teleports, std::string_view modeMD5, void *styles, std::string_view metadata,
									Callback<KZ::API::events::NewRecordAck> cb);

	/**
	 * Query the personal best of a player on a certain map, course, mode, style.
	 *
	 * Always return true.
	 */
	static bool QueryPB(u64 steamid64, CUtlString targetPlayerName, CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode,
						CUtlVector<CUtlString> &styleNames, Callback<KZ::API::events::PersonalBest> cb);

	/**
	 * Query the course top on a certain map, mode with a certain limit and offset.
	 *
	 * Always return true.
	 */
	static bool QueryCourseTop(CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode, u32 limit, u32 offset,
							   Callback<KZ::API::events::CourseTop> cb);

	/**
	 * Query the world record of a course on a certain mode.
	 *
	 * Always return true.
	 */
	static bool QueryWorldRecords(CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode, Callback<KZ::API::events::WorldRecords> cb);

private:
	/**
	 * URL to make HTTP requests to.
	 *
	 * Read from the configuration file.
	 */
	static inline std::string apiUrl {};

	/**
	 * Access Key used to authenticate the WebSocket connection.
	 *
	 * Read from the configuration file.
	 */
	static inline std::string apiKey {};

	/**
	 * WebSocket connected to the API.
	 */
	static inline ix::WebSocket *apiSocket {};

	/**
	 * Protects `handshakeInitiated`.
	 */
	static inline std::mutex handshakeLock {};

	/**
	 * Whether we already initiated the handshake
	 */
	static inline bool handshakeInitiated {};

	/**
	 * Used to wait for `handshakeInitiated` to become true
	 */
	static inline std::condition_variable handshakeCondvar {};

	/**
	 * Interval at which we need to send ping messages over the WebSocket.
	 *
	 * Set by the API during the handshake.
	 */
	static inline f64 heartbeatInterval = -1;

	/**
	 * Whether we have completed the handshake already.
	 */
	static inline bool handshakeCompleted {};

	/**
	 * Whether we should reauthenticate existing players.
	 * This happens if the API initializes after the players have connected.
	 */
	static inline std::atomic_bool shouldAuthenticateExistingPlayers = false;

	/**
	 * The ID to use for the next WebSocket message.
	 */
	static inline u64 nextMessageId = 1;

	/**
	 * Protects `currentMap`.
	 */
	static inline std::mutex currentMapMutex {};

	struct CurrentMap
	{
		u64 messageId;
		KZ::API::Map info;

		CurrentMap(u64 messageId, KZ::API::Map info) : messageId(messageId), info(info) {}
	};

	/**
	 * The API's view of the current map.
	 */
	static inline std::optional<CurrentMap> currentMap {};

	/**
	 * Protects `callbacks`.
	 */
	static inline std::mutex callbacksMutex {};

	struct StoredCallback
	{
		Callback<u64, const Json &> callback;

		/**
		 * The payload received by the WebSocket.
		 *
		 * If this is `std::nullopt` then we haven't received a response yet and cannot execute the callback.
		 */
		std::optional<Json> payload;
	};

	/**
	 * Callbacks to execute when the API sends messages with specific IDs.
	 */
	static inline std::unordered_map<u64, StoredCallback> callbacks {};

	/**
	 * Protects `runQueue`.
	 */
	static inline std::mutex runQueueMutex {};

	struct QueuedRecord
	{
		KZ::API::events::NewRecord data;
		Callback<KZ::API::events::NewRecordAck> callback;

		QueuedRecord(KZ::API::events::NewRecord &&data, Callback<KZ::API::events::NewRecordAck> &&callback)
			: data(std::move(data)), callback(std::move(callback))
		{
		}
	};

	/**
	 * Runs that were submitted while we weren't connected, which we might be
	 * able to submit later when connected again.
	 */
	static inline std::vector<QueuedRecord> runQueue {};

	/**
	 * Called bx IXWebSocket whenever we receive a message.
	 */
	static void OnWebSocketMessage(const ix::WebSocketMessagePtr &message);

	/**
	 * Called right after establishing the WebSocket connection.
	 */
	static void InitiateWebSocketHandshake(const ix::WebSocketMessagePtr &message);

	/**
	 * Called right after establishing the WebSocket connection.
	 */
	static void CompleteWebSocketHandshake(const ix::WebSocketMessagePtr &message);

	/**
	 * Sends a ping message every `heartbeatInterval` seconds.
	 */
	static void HeartbeatThread();

	/**
	 * Sends a WebSocket message.
	 */
	template<typename T>
	static void SendMessage(const char *event, const T &data)
	{
		if (!KZGlobalService::apiSocket)
		{
			META_CONPRINTF("[KZ::Global] WARN: called `SendMessage()` without API connection\n");
			return;
		}

		u64 messageId = KZGlobalService::nextMessageId++;

		Json payload {};
		payload.Set("id", messageId);
		payload.Set("event", event);
		payload.Set("data", data);

		KZGlobalService::apiSocket->send(payload.ToString());
	}

	/**
	 * Sends a WebSocket message.
	 */
	template<typename T, typename CallbackFunc>
	static void SendMessage(const char *event, const T &data, CallbackFunc &&callback)
	{
		if (!KZGlobalService::apiSocket)
		{
			META_CONPRINTF("[KZ::Global] WARN: called `SendMessage()` without API connection\n");
			return;
		}

		u64 messageId = KZGlobalService::nextMessageId++;

		Json payload {};
		payload.Set("id", messageId);
		payload.Set("event", event);
		payload.Set("data", data);

		auto callbackToStore = [callback](u64 messageId, Json responseJson)
		{
			if (!responseJson.IsValid())
			{
				META_CONPRINTF("[KZ::Global] WebSocket message is not valid JSON.\n");
				return;
			}

			typename std::remove_const<typename std::remove_reference<typename decltype(std::function(callback))::second_argument_type>::type>::type
				responseData {};

			if (!responseJson.Get("data", responseData))
			{
				META_CONPRINTF("[KZ::Global] WebSocket message does not contain a valid `data` field.\n");
				return;
			}

			META_CONPRINTF("[KZ::Global] Calling callback (%d).\n", messageId);
			callback(messageId, responseData);
		};

		{
			std::unique_lock callbacksLock(KZGlobalService::callbacksMutex);
			KZGlobalService::callbacks[messageId] = {callbackToStore, std::nullopt};
		}

		KZGlobalService::apiSocket->send(payload.ToString());
	}
};
