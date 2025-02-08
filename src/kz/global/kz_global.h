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
	 * Returns the current global map information.
	 */
	static std::optional<KZ::API::Map> GetCurrentMap()
	{
		return currentMap;
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

	void OnPlayerAuthorized();
	void OnClientDisconnect();

	/**
	 * Submits a new record to the API.
	 *
	 * Returns whether the record was submitted. This could be false if the map is not global.
	 */
	bool SubmitRecord(u16 filterID, f64 time, u32 teleports, std::string_view modeMD5, void *styles, std::string_view metadata,
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
	 * The ID to use for the next WebSocket message.
	 */
	static inline u64 nextMessageId = 1;

	/**
	 * The API's view of the current map.
	 */
	static inline std::optional<KZ::API::Map> currentMap {};

	/**
	 * Callbacks to execute when the API sends messages with specific IDs.
	 */
	static inline std::unordered_map<u64, Callback<const Json &>> callbacks {};

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
	static void SendMessage(const char *event, const T &data);

	/**
	 * Sends a WebSocket message.
	 */
	template<typename T, typename CallbackFunc>
	static void SendMessage(const char *event, const T &data, CallbackFunc callback);
};
