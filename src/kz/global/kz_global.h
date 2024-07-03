#pragma once

#include <atomic>
#include <functional>
#include <optional>
#include <string>

#include "kz/kz.h"
#include "common.h"
#include "error.h"
#include "players.h"
#include "maps.h"
#include "utils/http.h"

class KZGlobalService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	template<typename T>
	using Callback = std::function<void(T)>;

	static std::optional<KZ::API::Map> currentMap;

	/// Initializes a global instance of this service.
	static void Init();

	/// Registers commands.
	static void RegisterCommands();

	/// Checks whether the API is healthy.
	static bool IsHealthy()
	{
		return isHealthy;
	}

	/// Checks whether we are authenticated with the API.
	static bool IsAuthenticated()
	{
		return apiKey.has_value() && apiToken.has_value();
	}

	/// Fetches a player from the API using their name.
	///
	/// Returns whether an HTTP request was made.
	static bool FetchPlayer(const char *name, Callback<std::optional<KZ::API::Player>> onSuccess, Callback<KZ::API::Error> onError);

	/// Fetches a player from the API using their SteamID.
	///
	/// Returns whether an HTTP request was made.
	static bool FetchPlayer(u64 steamID, Callback<std::optional<KZ::API::Player>> onSuccess, Callback<KZ::API::Error> onError);

	/// Registers a player with the API.
	static bool RegisterPlayer(KZPlayer *player, Callback<std::optional<KZ::API::Error>> onError);

	/// Sends a player update to the API.
	static bool UpdatePlayer(KZPlayer *player, Callback<std::optional<KZ::API::Error>> onError);

	/// Fetches a map from the API using its name.
	static bool FetchMap(const char *name, Callback<std::optional<KZ::API::Map>> onSuccess, Callback<KZ::API::Error> onError);

	/// Fetches a map from the API using its ID.
	static bool FetchMap(u16 id, Callback<std::optional<KZ::API::Map>> onSuccess, Callback<KZ::API::Error> onError);

private:
	/// The API URL used for making requests.
	///
	/// This is read from the server configuration on startup, or falls back to the official URL.
	static const char *apiURL;

	/// Whether the API responded since the last heartbeat.
	///
	/// This is private with a public getter so other classes can check the API status but not tamper with it.
	static std::atomic<bool> isHealthy;

	/// Whether the timer for the authentication flow has already been started.
	///
	/// This is initialized as `false` and later set to `true` after the first successful heartbeat.
	static std::atomic<bool> authTimerInitialized;

	/// The server's API key used for generating access tokens.
	///
	/// This is read from the server configuration on startup.
	static std::optional<std::string> apiKey;

	/// JSON payload for generating a new access token.
	///
	/// Neither the API key nor the plugin version will change at runtime, so we can cache this.
	static std::optional<std::string> authPayload;

	/// The server's current access token.
	///
	/// This gets updated every ~10 minutes.
	static std::optional<std::string> apiToken;

	/// Interval in seconds for making heartbeat requests.
	static constexpr f64 heartbeatInterval = 30.0;

	/// Interval in seconds for fetching new access tokens.
	static constexpr f64 authInterval = 60.0 * 10;

	/// Timer callback for pinging the API regularly.
	static f64 Heartbeat();

	/// Timer callback for fetching access tokens every `authInterval` seconds.
	static f64 Auth();
};
