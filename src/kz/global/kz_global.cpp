// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include <chrono>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>

#include "common.h"
#include "cs2kz.h"
#include "kz/kz.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "kz/timer/announce.h"
#include "kz/option/kz_option.h"
#include "kz/global/kz_global.h"
#include "kz/global/api/handshake.h"
#include "kz/global/api/events.h"
#include "kz/global/api/maps.h"

#include <vendor/ClientCvarValue/public/iclientcvarvalue.h>

extern IClientCvarValue *g_pClientCvarValue;

static_global bool initialized = false;

void KZGlobalService::Init()
{
	if (initialized)
	{
		return;
	}

	META_CONPRINTF("[KZ::Global] Initializing GlobalService...\n");

	KZGlobalService::apiUrl = KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org");
	KZGlobalService::apiKey = KZOptionService::GetOptionStr("apiKey", "");

	if (KZGlobalService::apiUrl.size() < 4 || KZGlobalService::apiUrl.substr(0, 4) != "http")
	{
		META_CONPRINTF("[KZ::Global] Invalid API url. GlobalService will be disabled.\n");
		KZGlobalService::apiUrl = "";
		return;
	}

	if (!g_pClientCvarValue)
	{
		META_CONPRINT("[KZ::Global] ClientCvarValue not found. WebSocket connection will be disabled.\n");
		return;
	}

	if (KZGlobalService::apiKey.empty())
	{
		META_CONPRINTF("[KZ::Global] No API key found! WebSocket connection will be disabled.\n");
		return;
	}

	std::string websocketUrl = KZGlobalService::apiUrl.replace(0, 4, "ws") + "/auth/cs2";
	ix::WebSocketHttpHeaders websocketHeaders;
	websocketHeaders["Authorization"] = std::string("Bearer ") + KZGlobalService::apiKey;

#ifdef _WIN32
	ix::initNetSystem();
#endif

	KZGlobalService::apiSocket = new ix::WebSocket();
	KZGlobalService::apiSocket->setUrl(websocketUrl);
	KZGlobalService::apiSocket->setExtraHeaders(websocketHeaders);
	KZGlobalService::apiSocket->setOnMessageCallback(KZGlobalService::OnWebSocketMessage);

	META_CONPRINTF("[KZ::Global] Establishing WebSocket connection... (url = '%s')\n", websocketUrl.c_str());
	KZGlobalService::apiSocket->start();

	KZGlobalService::EnforceConVars();

	initialized = true;
}

void KZGlobalService::Cleanup()
{
	META_CONPRINTF("[KZ::Global] Cleaning up...\n");

	if (KZGlobalService::apiSocket)
	{
		META_CONPRINTF("[KZ::Global] Closing WebSocket...\n");

		KZGlobalService::apiSocket->stop();
		delete KZGlobalService::apiSocket;
		KZGlobalService::apiSocket = nullptr;
	}

	if (initialized)
	{
		KZGlobalService::RestoreConVars();
	}

#ifdef _WIN32
	ix::uninitNetSystem();
#endif
}

void KZGlobalService::UpdateGlobalCache()
{
	u16 currentMapID = 0;

	{
		std::unique_lock currentMapLock(KZGlobalService::currentMapMutex);

		if (!KZGlobalService::currentMap.has_value())
		{
			return;
		}

		currentMapID = KZGlobalService::currentMap->info.id;
	}

	KZ::API::events::WantWorldRecordsForCache data(currentMapID);

	auto callback = [](u64 messageId, const KZ::API::events::WorldRecordsForCache &records)
	{
		for (const KZ::API::Record &record : records.records)
		{
			const KZCourse *course = KZ::course::GetCourseByGlobalCourseID(record.course.id);

			if (course == nullptr)
			{
				META_CONPRINTF("[KZ::Global] Could not find current course?\n");
				continue;
			}

			PluginId modeID = KZ::mode::GetModeInfo(record.mode).id;

			KZTimerService::InsertRecordToCache(record.time, course, modeID, record.nubPoints != 0, true);
		}
	};

	KZGlobalService::SendMessage("want-world-records-for-cache", data, callback);
}

void KZGlobalService::OnActivateServer()
{
	if (!initialized)
	{
		KZGlobalService::Init();
	}

	{
		std::unique_lock handshakeLock(KZGlobalService::handshakeLock);

		if (!KZGlobalService::handshakeInitiated)
		{
			KZGlobalService::handshakeInitiated = true;
			KZGlobalService::handshakeCondvar.notify_one();
			return;
		}
	}

	bool ok = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&ok);

	if (!ok)
	{
		META_CONPRINTF("[KZ::Global] Failed to get current map name. Cannot send `map-change` event.\n");
		return;
	}

	KZ::API::events::MapChange data(currentMapName.Get());

	auto callback = [currentMapName](u64 messageID, const KZ::API::events::MapInfo &mapInfo)
	{
		{
			std::unique_lock handshakeLock(KZGlobalService::handshakeLock);

			if (mapInfo.map.has_value())
			{
				KZGlobalService::currentMap = KZGlobalService::CurrentMap(messageID, *mapInfo.map);
			}
		}

		if (!mapInfo.map.has_value())
		{
			META_CONPRINTF("[KZ::Global] %s is not global.\n", currentMapName.Get());
			return;
		}

		for (const KZ::API::Map::Course &course : mapInfo.map->courses)
		{
			KZ::course::UpdateCourseGlobalID(course.name.c_str(), course.id);
		}

		KZGlobalService::UpdateGlobalCache();
	};

	KZGlobalService::SendMessage("map-change", data, callback);
}

void KZGlobalService::OnServerGamePostSimulate()
{
	std::unordered_map<u64, StoredCallback> readyCallbacks;

	{
		std::unique_lock callbacksLock(KZGlobalService::callbacksMutex);

		for (auto it = KZGlobalService::callbacks.cbegin(); it != KZGlobalService::callbacks.cend();)
		{
			auto curr = it++;

			if (curr->second.payload.has_value())
			{
				readyCallbacks.insert(KZGlobalService::callbacks.extract(curr));
			}
		}
	}

	for (auto &[messageID, callback] : readyCallbacks)
	{
		callback.callback(messageID, *callback.payload);
	}
}

void KZGlobalService::OnPlayerAuthorized()
{
	KZ::API::events::PlayerJoin data;
	data.steamId = this->player->GetSteamId64();
	data.name = this->player->GetName();
	data.ipAddress = this->player->GetIpAddress();

	auto callback = [player = this->player](u64 messageId, const KZ::API::events::PlayerJoinAck &ack)
	{
		const std::string preferences = ack.preferences.ToString();

		META_CONPRINTF("[KZ::Global] %s is %sbanned. preferences:\n```\n%s\n```\n", player->GetName(), ack.isBanned ? "" : "not ",
					   preferences.c_str());

		player->optionService->InitializeGlobalPrefs(preferences);
	};

	KZGlobalService::SendMessage("player-join", data, callback);

	u16 currentMapID = 0;

	{
		std::unique_lock currentMapLock(KZGlobalService::currentMapMutex);

		if (KZGlobalService::currentMap.has_value())
		{
			currentMapID = KZGlobalService::currentMap->info.id;
		}
	}

	if (currentMapID != 0)
	{
		KZ::API::events::WantPlayerRecords data;
		data.mapId = currentMapID;
		data.playerId = this->player->GetSteamId64();

		auto callback = [player = this->player](u64 messageId, const KZ::API::events::PlayerRecords &pbs)
		{
			for (const KZ::API::Record &record : pbs.records)
			{
				const KZCourse *course = KZ::course::GetCourseByGlobalCourseID(record.course.id);

				if (course == nullptr)
				{
					META_CONPRINTF("[KZ::Global] Could not find current course?\n");
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

		KZGlobalService::SendMessage("want-player-records", data, callback);
	}
}

void KZGlobalService::OnClientDisconnect()
{
	if (!this->player->IsConnected() || !this->player->IsAuthenticated())
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

	KZ::API::events::PlayerLeave data;
	data.steamId = this->player->GetSteamId64();
	data.name = this->player->GetName();
	data.preferences = Json(getPrefsResult.Get());

	KZGlobalService::SendMessage("player-leave", data);
}

bool KZGlobalService::SubmitRecord(u16 filterID, f64 time, u32 teleports, std::string_view modeMD5, void *styles, std::string_view metadata,
								   Callback<KZ::API::events::NewRecordAck> cb)
{
	if (!this->player->IsAuthenticated() && !this->player->hasPrime)
	{
		return false;
	}

	{
		std::unique_lock currentMapLock(KZGlobalService::currentMapMutex);
		if (!KZGlobalService::currentMap.has_value())
		{
			META_CONPRINTF("[KZ::Global] Cannot submit record on non-global map.\n");
			return false;
		}
	}

	KZ::API::events::NewRecord data;
	data.playerId = this->player->GetSteamId64();
	data.filterId = filterID;
	data.modeMD5 = modeMD5;
	for (auto style : *(std::vector<RecordAnnounce::StyleInfo> *)styles)
	{
		data.styles.push_back({style.name, style.md5});
	}
	data.teleports = teleports;
	data.time = time;
	data.metadata = metadata;

	KZGlobalService::SendMessage("new-record", data, [cb](u64 messageId, KZ::API::events::NewRecordAck ack) { return cb(ack); });

	return true;
}

bool KZGlobalService::QueryPB(u64 steamid64, CUtlString targetPlayerName, CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode,
							  CUtlVector<CUtlString> &styleNames, Callback<KZ::API::events::PersonalBest> cb)
{
	KZ::API::events::WantPersonalBest pbRequest = {steamid64, targetPlayerName.Get(), mapName.Get(), courseNameOrNumber.Get(), mode};
	FOR_EACH_VEC(styleNames, i)
	{
		pbRequest.styles.emplace_back(styleNames[i].Get());
	}

	KZGlobalService::SendMessage("want-personal-best", pbRequest, [=](u64 messageId, KZ::API::events::PersonalBest pb) { return cb(pb); });
	return true;
}

bool KZGlobalService::QueryCourseTop(CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode, u32 limit, u32 offset,
									 Callback<KZ::API::events::CourseTop> cb)
{
	KZ::API::events::WantCourseTop ctopRequest = {mapName.Get(), courseNameOrNumber.Get(), mode, limit, offset};

	KZGlobalService::SendMessage("want-course-top", ctopRequest, [=](u64 messageId, KZ::API::events::CourseTop courseTop) { return cb(courseTop); });
	return true;
}

bool KZGlobalService::QueryWorldRecords(CUtlString mapName, CUtlString courseNameOrNumber, KZ::API::Mode mode,
										Callback<KZ::API::events::WorldRecords> cb)
{
	KZ::API::events::WantWorldRecords ctopRequest = {mapName.Get(), courseNameOrNumber.Get(), mode};

	KZGlobalService::SendMessage("want-world-records", ctopRequest, [=](u64 messageId, KZ::API::events::WorldRecords wrs) { return cb(wrs); });
	return true;
}

void KZGlobalService::OnWebSocketMessage(const ix::WebSocketMessagePtr &message)
{
	switch (message->type)
	{
		case ix::WebSocketMessageType::Open:
		{
			META_CONPRINTF("[KZ::Global] WebSocket connection established.\n");
			InitiateWebSocketHandshake(message);
			break;
		}

		case ix::WebSocketMessageType::Close:
		{
			META_CONPRINTF("[KZ::Global] WebSocket connection closed.\n");

			switch (message->closeInfo.code)
			{
				case 1000 /* NORMAL */:
				{
					META_CONPRINTF("[KZ::Global] Server closed connection: %s\n", message->closeInfo.reason.c_str());
					break;
				};
				case 1008 /* POLICY */:
				{
					META_CONPRINTF("[KZ::Global] Violated server policy: %s\n", message->closeInfo.reason.c_str());
					KZGlobalService::apiSocket->disableAutomaticReconnection();
					break;
				};
				default:
				{
					META_CONPRINTF("[KZ::Global] Server closed connection unexpectedly: %s\n", message->closeInfo.reason.c_str());
					KZGlobalService::apiSocket->disableAutomaticReconnection();
				};
			}

			break;
		}

		case ix::WebSocketMessageType::Error:
		{
			META_CONPRINTF("[KZ::Global] WebSocket error: `%s`\n", message->errorInfo.reason.c_str());
			if (message->errorInfo.http_status == 401)
			{
				META_CONPRINTF("[KZ::Global] Unauthorized. Check your API key.\n");
				KZGlobalService::apiSocket->disableAutomaticReconnection();
			}
			break;
		}

		case ix::WebSocketMessageType::Ping:
		{
			META_CONPRINTF("[KZ::Global] Ping!\n");
			break;
		}

		case ix::WebSocketMessageType::Pong:
		{
			META_CONPRINTF("[KZ::Global] Pong!\n");
			break;
		}

		case ix::WebSocketMessageType::Message:
		{
			META_CONPRINTF("[KZ::Global] Received WebSocket message:\n```\n%s\n```\n", message->str.c_str());

			if (!KZGlobalService::handshakeCompleted)
			{
				CompleteWebSocketHandshake(message);
				break;
			}

			Json payload(message->str);

			if (!payload.IsValid())
			{
				META_CONPRINTF("[KZ::Global] WebSocket message is not valid JSON.\n");
				return;
			}

			u64 messageId = 0;

			if (!payload.Get("id", messageId))
			{
				META_CONPRINTF("[KZ::Global] WebSocket message does not contain a valid ID.\n");
				return;
			}

			{
				std::unique_lock callbacksLock(KZGlobalService::callbacksMutex);
				auto callback = KZGlobalService::callbacks.find(messageId);

				if (callback != KZGlobalService::callbacks.end())
				{
					callback->second.payload = std::make_optional(payload);
				}
			}

			break;
		}

		case ix::WebSocketMessageType::Fragment:
		{
			// unused
			break;
		}
	}
}

void KZGlobalService::InitiateWebSocketHandshake(const ix::WebSocketMessagePtr &message)
{
	META_CONPRINTF("[KZ::Global] Waiting for map to load...\n");

	{
		std::unique_lock handshakeLock(KZGlobalService::handshakeLock);
		KZGlobalService::handshakeCondvar.wait(handshakeLock, []() { return KZGlobalService::handshakeInitiated; });
	}

	META_CONPRINTF("[KZ::Global] Performing handshake...\n");
	assert(message->type == ix::WebSocketMessageType::Open);

	bool ok = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&ok);

	if (!ok)
	{
		META_CONPRINTF("[KZ::Global] Failed to get current map name. Cannot initiate handshake.\n");
		return;
	}

	KZ::API::handshake::Hello hello(currentMapName.Get(), g_KZPlugin.GetMD5());

	for (Player *player : g_pPlayerManager->players)
	{
		if (player->IsAuthenticated())
		{
			hello.AddPlayer(player->GetSteamId64(), player->GetName());
		}
	}

	Json payload {};

	if (!hello.ToJson(payload))
	{
		META_CONPRINTF("[KZ::Global] Failed to encode 'Hello' message.\n");
		return;
	}

	KZGlobalService::apiSocket->send(payload.ToString());

	META_CONPRINTF("[KZ::Global] Sent 'Hello' message.\n");
}

void KZGlobalService::CompleteWebSocketHandshake(const ix::WebSocketMessagePtr &message)
{
	META_CONPRINTF("[KZ::Global] Completing handshake...\n");
	assert(message->type == ix::WebSocketMessageType::Message);

	Json payload(message->str);
	KZ::API::handshake::HelloAck ack;

	if (!ack.FromJson(payload))
	{
		META_CONPRINTF("[KZ::Global] Failed to decode 'HelloAck' message.\n");
		return;
	}

	// x0.85 to reduce risk of timing out if there's high network latency
	KZGlobalService::heartbeatInterval = ack.heartbeatInterval * 0.85;
	KZGlobalService::handshakeCompleted = true;

	std::thread(HeartbeatThread).detach();

	if (ack.map.has_value())
	{
		std::unique_lock currentMapLock(KZGlobalService::currentMapMutex);
		KZGlobalService::currentMap = KZGlobalService::CurrentMap(0, *ack.map);
	}

	META_CONPRINTF("[KZ::Global] Completed handshake!\n");

	KZGlobalService::OnActivateServer();
}

void KZGlobalService::HeartbeatThread()
{
	if (!KZGlobalService::IsConnected())
	{
		return;
	}

	while (KZGlobalService::heartbeatInterval > 0)
	{
		if (KZGlobalService::IsConnected())
		{
			KZGlobalService::apiSocket->ping("");
			META_CONPRINTF("[KZ::Global] Sent heartbeat. (interval=%.2fs)\n", KZGlobalService::heartbeatInterval);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds((u64)(KZGlobalService::heartbeatInterval * 1000)));
	}
}
