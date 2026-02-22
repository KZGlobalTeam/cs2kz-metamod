// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include <string_view>

#include <ixwebsocket/IXNetSystem.h>

#include "common.h"
#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_global.h"
#include "kz/global/handshake.h"
#include "kz/global/events.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"

#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

extern IClientCvarValue *g_pClientCvarValue;

bool KZGlobalService::IsAvailable()
{
	return KZGlobalService::state.load() == KZGlobalService::State::HandshakeCompleted;
}

bool KZGlobalService::MayBecomeAvailable()
{
	return KZGlobalService::state.load() != KZGlobalService::State::Disconnected;
}

void KZGlobalService::UpdateRecordCache()
{
	u16 currentMapID = 0;

	{
		std::unique_lock lock(KZGlobalService::currentMap.mutex);

		if (!KZGlobalService::currentMap.data.has_value())
		{
			return;
		}

		currentMapID = KZGlobalService::currentMap.data->id;
	}

	std::string_view event("want-world-records-for-cache");
	KZ::API::events::WantWorldRecordsForCache data {currentMapID};
	auto callback = [](KZ::API::events::WorldRecordsForCache &records)
	{
		for (const KZ::API::Record &record : records.records)
		{
			const KZCourseDescriptor *course = KZ::course::GetCourseByGlobalCourseID(record.course.id);

			if (course == nullptr)
			{
				META_CONPRINTF("[KZ::Global] Could not find current course?\n");
				continue;
			}

			PluginId modeID = KZ::mode::GetModeInfo(record.mode).id;

			KZTimerService::InsertRecordToCache(record.time, course, modeID, record.nubPoints != 0, true);
		}
	};

	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::HandshakeCompleted:
			KZGlobalService::SendMessage(event, data, callback);
			break;

		case KZGlobalService::State::Disconnected:
			break;

		default:
			KZGlobalService::AddWhenConnectedCallback([=]() { KZGlobalService::SendMessage(event, data, callback); });
	}
}

void KZGlobalService::Init()
{
	if (KZGlobalService::state.load() != KZGlobalService::State::Uninitialized)
	{
		return;
	}

	META_CONPRINTF("[KZ::Global] Initializing GlobalService...\n");

	std::string url = KZOptionService::GetOptionStr("apiUrl", KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));
	std::string_view key = KZOptionService::GetOptionStr("apiKey");

	if (url.empty())
	{
		META_CONPRINTF("[KZ::Global] `apiUrl` is empty. GlobalService will be disabled.\n");
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}

	if (url.size() < 4 || url.substr(0, 4) != "http")
	{
		META_CONPRINTF("[KZ::Global] `apiUrl` is invalid. GlobalService will be disabled.\n");
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}

	if (key.empty())
	{
		META_CONPRINTF("[KZ::Global] `apiKey` is empty. GlobalService will be disabled.\n");
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}

	url.replace(0, 4, "ws");

	if (url.size() < 9 || url.substr(url.size() - 9) != "/auth/cs2")
	{
		if (url.substr(url.size() - 1) != "/")
		{
			url += "/";
		}

		url += "auth/cs2";
	}

	KZGlobalService::socket = new ix::WebSocket();
	KZGlobalService::socket->setUrl(url);

	// ix::WebSocketHttpHeaders headers;
	// headers["Authorization"] = "Bearer ";
	// headers["Authorization"] += key;
	KZGlobalService::socket->setExtraHeaders({
		{"Authorization", std::string("Bearer ") + key.data()},
	});

	KZGlobalService::socket->setOnMessageCallback(KZGlobalService::OnWebSocketMessage);
	KZGlobalService::socket->start();

	KZGlobalService::EnforceConVars();

	KZGlobalService::state.store(KZGlobalService::State::Initialized);
}

void KZGlobalService::Cleanup()
{
	if (KZGlobalService::state.load() == KZGlobalService::State::Uninitialized)
	{
		return;
	}

	META_CONPRINTF("[KZ::Global] Cleaning up GlobalService...\n");

	KZGlobalService::RestoreConVars();

	if (KZGlobalService::socket != nullptr)
	{
		KZGlobalService::socket->stop();
		delete KZGlobalService::socket;
		KZGlobalService::socket = nullptr;
	}

	KZGlobalService::state.store(KZGlobalService::State::Uninitialized);
}

void KZGlobalService::OnServerGamePostSimulate()
{
	std::vector<std::function<void()>> callbacks;

	{
		std::unique_lock lock(KZGlobalService::mainThreadCallbacks.mutex, std::defer_lock);

		if (lock.try_lock())
		{
			KZGlobalService::mainThreadCallbacks.queue.swap(callbacks);

			if (KZGlobalService::state.load() == KZGlobalService::State::HandshakeCompleted)
			{
				callbacks.reserve(callbacks.size() + KZGlobalService::mainThreadCallbacks.whenConnectedQueue.size());
				callbacks.insert(callbacks.end(), KZGlobalService::mainThreadCallbacks.whenConnectedQueue.begin(),
								 KZGlobalService::mainThreadCallbacks.whenConnectedQueue.end());
				KZGlobalService::mainThreadCallbacks.whenConnectedQueue.clear();
			}
		}
	}

	for (const std::function<void()> &callback : callbacks)
	{
		callback();
	}
}

void KZGlobalService::OnActivateServer()
{
	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::Uninitialized:
			KZGlobalService::Init();
			break;

		case KZGlobalService::State::HandshakeCompleted:
		{
			bool mapNameOk = false;
			CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

			if (!mapNameOk)
			{
				META_CONPRINTF("[KZ::Global] Failed to get current map name. Cannot send `map-change` event.\n");
				return;
			}

			std::string_view event("map-change");
			KZ::API::events::MapChange data(currentMapName.Get());

			// clang-format off
			KZGlobalService::SendMessage(event, data, [currentMapName](KZ::API::events::MapInfo& mapInfo)
			{
				if (mapInfo.data.has_value())
				{
					META_CONPRINTF("[KZ::Global] %s is approved.\n", mapInfo.data->name.c_str());
					if (mapInfo.data->name == currentMapName.Get())
					{
						for (const auto &course : mapInfo.data->courses)
						{
							KZ::course::UpdateCourseGlobalID(course.name.c_str(), course.id);
							META_CONPRINTF("[KZ::Global] Registered course '%s' with ID %i!\n", course.name.c_str(), course.id);
						}
					}
				}
				else
				{
					META_CONPRINTF("[KZ::Global] %s is not approved.\n", currentMapName.Get());
				}

				{
					std::unique_lock lock(KZGlobalService::currentMap.mutex);
					KZGlobalService::currentMap.data = std::move(mapInfo.data);
				}
			});
			// clang-format on
		}
		break;
	}
}

void KZGlobalService::OnPlayerAuthorized()
{
	if (!this->player->IsConnected())
	{
		return;
	}

	u64 steamID = this->player->GetSteamId64();

	std::string_view event("player-join");
	KZ::API::events::PlayerJoin data;
	data.steamID = steamID;
	data.name = this->player->GetName();
	data.ipAddress = this->player->GetIpAddress();

	auto callback = [steamID](KZ::API::events::PlayerJoinAck &ack)
	{
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);

		if (player == nullptr)
		{
			return;
		}

		player->globalService->playerInfo.isBanned = ack.isBanned;
		player->optionService->InitializeGlobalPrefs(ack.preferences.ToString());

		// clang-format off
		u16 currentMapID = KZGlobalService::WithCurrentMap([](const KZ::API::Map* currentMap)
		{
			return (currentMap == nullptr) ? 0 : currentMap->id;
		});
		// clang-format on

		if (currentMapID != 0)
		{
			std::string_view event("want-player-records");
			KZ::API::events::WantPlayerRecords data;
			data.mapID = currentMapID;
			data.playerID = steamID;

			auto callback = [steamID](KZ::API::events::PlayerRecords &records)
			{
				KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);

				if (player == nullptr)
				{
					return;
				}

				for (const KZ::API::Record &record : records.records)
				{
					const KZCourseDescriptor *course = KZ::course::GetCourseByGlobalCourseID(record.course.id);

					if (course == nullptr)
					{
						continue;
					}

					PluginId modeID = KZ::mode::GetModeInfo(record.mode).id;

					if (record.nubPoints != 0)
					{
						player->timerService->InsertPBToCache(record.time, course, modeID, true, true, "", record.nubPoints);
					}

					if (record.proPoints != 0)
					{
						player->timerService->InsertPBToCache(record.time, course, modeID, false, true, "", record.proPoints);
					}
				}
			};

			switch (KZGlobalService::state.load())
			{
				case KZGlobalService::State::HandshakeInitiated:
					KZGlobalService::AddWhenConnectedCallback([=]() { KZGlobalService::SendMessage(event, data, callback); });
					break;

				case KZGlobalService::State::HandshakeCompleted:
					KZGlobalService::SendMessage(event, data, callback);
					break;

				case KZGlobalService::State::Disconnected:
					META_CONPRINTF("[KZ::Global] Cannot fetch player PBs if we are disconnected from the API. (state=%i)\n");
					break;

				default:
					// handshake hasn't been initiated yet, so by the time that
					// happens, player will be sent as part of the handshake
					break;
			}
		}
	};

	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::HandshakeInitiated:
			KZGlobalService::AddWhenConnectedCallback([=]() { KZGlobalService::SendMessage(event, data, callback); });
			break;

		case KZGlobalService::State::HandshakeCompleted:
			KZGlobalService::SendMessage(event, data, callback);
			break;

		case KZGlobalService::State::Disconnected:
			break;

		default:
			// handshake hasn't been initiated yet, so by the time that
			// happens, player will be sent as part of the handshake
			break;
	}
}

void KZGlobalService::OnClientDisconnect()
{
	if (!this->player->IsConnected() || !this->player->IsAuthenticated())
	{
		return;
	}

	if (KZGlobalService::state.load() != KZGlobalService::State::HandshakeCompleted)
	{
		return;
	}

	CUtlString getPrefsError;
	CUtlString getPrefsResult;
	this->player->optionService->GetPreferencesAsJSON(&getPrefsError, &getPrefsResult);

	if (!getPrefsError.IsEmpty())
	{
		META_CONPRINTF("[KZ::Global] Failed to get preferences: %s\n", getPrefsError.Get());
		META_CONPRINTF("[KZ::Global] Not sending `player-leave` event.\n");
		return;
	}

	std::string_view event("player-leave");
	KZ::API::events::PlayerLeave data;
	data.steamID = this->player->GetSteamId64();
	data.name = this->player->GetName();
	data.preferences = Json(getPrefsResult.Get());

	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::HandshakeCompleted:
			KZGlobalService::SendMessage(event, data);
			break;

		default:
			break;
	}
}

void KZGlobalService::SubmitBan(u64 steamID, std::string reason, std::string details)
{
	// TODO Anticheat: Implement this!
}

void KZGlobalService::OnWebSocketMessage(const ix::WebSocketMessagePtr &message)
{
	switch (message->type)
	{
		case ix::WebSocketMessageType::Open:
		{
			META_CONPRINTF("[KZ::Global] Connection established!\n");
			KZGlobalService::state.store(KZGlobalService::State::Connected);
			KZGlobalService::AddMainThreadCallback(KZGlobalService::InitiateHandshake);
		}
		break;

		case ix::WebSocketMessageType::Close:
		{
			META_CONPRINTF("[KZ::Global] Connection closed (code %i): %s\n", message->closeInfo.code, message->closeInfo.reason.c_str());

			switch (message->closeInfo.code)
			{
				case 1000 /* NORMAL */:     /* fall-through */
				case 1001 /* GOING AWAY */: /* fall-through */
				case 1006 /* ABNORMAL */:
				{
					KZGlobalService::socket->enableAutomaticReconnection();
					KZGlobalService::state.store(KZGlobalService::State::DisconnectedButWorthRetrying);
				}
				break;

				case 1008 /* POLICY VIOLATION */:
				{
					if (KZGlobalService::state.load() == KZGlobalService::State::HandshakeCompleted
						&& message->closeInfo.reason.find("heartbeat") != message->closeInfo.reason.size())
					{
						KZGlobalService::socket->enableAutomaticReconnection();
						KZGlobalService::state.store(KZGlobalService::State::DisconnectedButWorthRetrying);
					}
					else
					{
						KZGlobalService::socket->disableAutomaticReconnection();
						KZGlobalService::state.store(KZGlobalService::State::Disconnected);
					}
				}
				break;

				default:
				{
					KZGlobalService::socket->disableAutomaticReconnection();
					KZGlobalService::state.store(KZGlobalService::State::Disconnected);
				}
			}
		}
		break;

		case ix::WebSocketMessageType::Error:
		{
			META_CONPRINTF("[KZ::Global] WebSocket error (code %i): %s\n", message->errorInfo.http_status, message->errorInfo.reason.c_str());

			switch (message->errorInfo.http_status)
			{
				case 401:
				{
					KZGlobalService::socket->disableAutomaticReconnection();
					KZGlobalService::state.store(KZGlobalService::State::Disconnected);
				}
				break;

				case 429:
				{
					META_CONPRINTF("[KZ::Global] API rate limit reached; increasing down reconnection delay...\n");
					KZGlobalService::socket->enableAutomaticReconnection();
					KZGlobalService::socket->setMinWaitBetweenReconnectionRetries(10'000 /* ms */);
					KZGlobalService::socket->setMaxWaitBetweenReconnectionRetries(30'000 /* ms */);
					KZGlobalService::state.store(KZGlobalService::State::DisconnectedButWorthRetrying);
				}
				break;

				case 500: /* fall-through */
				case 502:
				{
					META_CONPRINTF("[KZ::Global] API encountered an internal error\n");
					KZGlobalService::socket->enableAutomaticReconnection();
					KZGlobalService::socket->setMinWaitBetweenReconnectionRetries(10 * 60'000 /* ms */);
					KZGlobalService::socket->setMaxWaitBetweenReconnectionRetries(30 * 60'000 /* ms */);
					KZGlobalService::state.store(KZGlobalService::State::DisconnectedButWorthRetrying);
				}
				break;

				default:
				{
					KZGlobalService::socket->disableAutomaticReconnection();
					KZGlobalService::state.store(KZGlobalService::State::Disconnected);
				}
			}
		}
		break;

		case ix::WebSocketMessageType::Message:
		{
			META_CONPRINTF("[KZ::Global] Received WebSocket message:\n-----\n%s\n------\n", message->str.c_str());

			Json payload(message->str);

			switch (KZGlobalService::state.load())
			{
				case KZGlobalService::State::HandshakeInitiated:
				{
					KZ::API::handshake::HelloAck helloAck;

					if (!helloAck.FromJson(payload))
					{
						META_CONPRINTF("[KZ::Global] Failed to decode 'HelloAck'\n");
						break;
					}

					KZGlobalService::AddMainThreadCallback([ack = std::move(helloAck)]() mutable { KZGlobalService::CompleteHandshake(ack); });
				}
				break;

				case KZGlobalService::State::HandshakeCompleted:
				{
					if (!payload.IsValid())
					{
						META_CONPRINTF("[KZ::Global] WebSocket message is not valid JSON.\n");
						break;
					}

					u32 messageID = 0;

					if (!payload.Get("id", messageID))
					{
						META_CONPRINTF("[KZ::Global] Ignoring message without valid ID\n");
						break;
					}

					KZGlobalService::AddMainThreadCallback([=]() { KZGlobalService::ExecuteMessageCallback(messageID, payload); });
				}
				break;
			}
		}
		break;

		case ix::WebSocketMessageType::Ping:
		{
			META_CONPRINTF("[KZ::Global] Ping!\n");
		}
		break;

		case ix::WebSocketMessageType::Pong:
		{
			META_CONPRINTF("[KZ::Global] Pong!\n");
		}
		break;
	}
}

void KZGlobalService::InitiateHandshake()
{
	bool mapNameOk = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

	if (!mapNameOk)
	{
		META_CONPRINTF("[KZ::Global] Failed to get current map name. Cannot initiate handshake.\n");
		return;
	}

	std::string_view event("hello");
	KZ::API::handshake::Hello data(g_KZPlugin.GetMD5(), currentMapName.Get());

	for (Player *player : g_pPlayerManager->players)
	{
		if (player && player->IsAuthenticated())
		{
			data.AddPlayer(player->GetSteamId64(), player->GetName());
		}
	}

	KZGlobalService::SendMessage(event, data);
	KZGlobalService::state.store(State::HandshakeInitiated);
}

void KZGlobalService::CompleteHandshake(KZ::API::handshake::HelloAck &ack)
{
	KZGlobalService::state.store(State::HandshakeCompleted);

	// clang-format off
	std::thread([heartbeatInterval = std::chrono::milliseconds(static_cast<i64>(ack.heartbeatInterval * 800))]()
	{
		while (KZGlobalService::state.load() == State::HandshakeCompleted) {
			KZGlobalService::socket->ping("");
			META_CONPRINTF("[KZ::Global] Sent heartbeat. (interval=%is)\n", std::chrono::duration_cast<std::chrono::seconds>(heartbeatInterval).count());
			std::this_thread::sleep_for(heartbeatInterval);
		}
	}).detach();
	// clang-format on

	if (ack.mapInfo.has_value() && ack.mapInfo->name == g_pKZUtils->GetCurrentMapName().Get())
	{
		for (const auto &course : ack.mapInfo->courses)
		{
			KZ::course::UpdateCourseGlobalID(course.name.c_str(), course.id);
			META_CONPRINTF("[KZ::Global] Registered course '%s' with ID %i!\n", course.name.c_str(), course.id);
		}
	}
	{
		std::unique_lock lock(KZGlobalService::currentMap.mutex);
		KZGlobalService::currentMap.data = std::move(ack.mapInfo);
	}

	{
		std::unique_lock lock(KZGlobalService::globalModes.mutex);
		KZGlobalService::globalModes.data = std::move(ack.modes);
	}

	{
		std::unique_lock lock(KZGlobalService::globalStyles.mutex);
		KZGlobalService::globalStyles.data = std::move(ack.styles);
	}

	{
		std::unique_lock lock(KZGlobalService::announcements.mutex);
		KZGlobalService::announcements.data = std::move(ack.announcements);
	}

	META_CONPRINTF("[KZ::Global] Completed handshake, activating server...\n");
	KZGlobalService::OnActivateServer();
}

void KZGlobalService::ExecuteMessageCallback(u32 messageID, const Json &payload)
{
	std::function<void(u32, const Json &)> callback;

	{
		std::unique_lock lock(KZGlobalService::messageCallbacks.mutex);
		std::unordered_map<u32, std::function<void(u32, const Json &)>> &callbacks = KZGlobalService::messageCallbacks.queue;

		if (auto found = callbacks.extract(messageID); !found.empty())
		{
			callback = found.mapped();
		}
	}

	if (callback)
	{
		META_CONPRINTF("[KZ::Global] Executing callback #%i\n", messageID);
		callback(messageID, payload);
	}
}

void KZGlobalService::PrintAnnouncements()
{
	std::unique_lock lock(KZGlobalService::announcements.mutex);
	for (const KZ::API::handshake::HelloAck::Announcement &announcement : KZGlobalService::announcements.data)
	{
		time_t now = std::time(nullptr);
		if (announcement.startsAt > (u64)now || announcement.expiresAt < (u64)now)
		{
			continue;
		}
		if (!announcement.title.empty())
		{
			this->player->PrintChat(false, false, "{yellow}GLOBAL {grey}| %s", announcement.title.c_str());
		}
		if (!announcement.body.empty())
		{
			this->player->PrintChat(false, false, "%s", announcement.body.c_str());
		}
	}
}
