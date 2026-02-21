// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include <string_view>

#include <ixwebsocket/IXNetSystem.h>

#include "kz/kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"
#include "utils/json.h"

#include "kz_global.h"
#include "messages.h"

static_function bool GetApiUrl(std::string &url)
{
	url = KZOptionService::GetOptionStr("apiUrl", KZOptionService::GetOptionStr("apiUrl", "https://api.cs2kz.org"));

	if (url.empty())
	{
		META_CONPRINTF("[KZ::Global] `apiUrl` is empty. GlobalService will be disabled.\n");
		return false;
	}

	if (url.size() < 4 || url.substr(0, 4) != "http")
	{
		META_CONPRINTF("[KZ::Global] `apiUrl` is invalid. GlobalService will be disabled.\n");
		return false;
	}

	url.replace(0, 4, "ws");

	if (url.substr(url.size() - 1) != "/")
	{
		url += "/";
	}

	url += "auth/cs2";

	return true;
}

static_function bool GetApiKey(std::string &url)
{
	key = KZOptionService::GetOptionStr("apiKey");

	if (key.empty())
	{
		META_CONPRINTF("[KZ::Global] `apiKey` is empty. GlobalService will be disabled.\n");
		return false;
	}

	return true;
}

void KZGlobalService::Init()
{
	if (KZGlobalService::state.load() != KZGlobalService::State::Uninitialized)
	{
		return;
	}

	META_CONPRINTF("[KZ::Global] Initializing GlobalService...\n");

	std::string apiUrl;
	if (!GetApiUrl(apiUrl))
	{
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}
	META_CONPRINTF("[KZ::Global] API URL is `%s`.\n", apiUrl.c_str());

	std::string apiKey;
	if (!GetApiKey(apiKey))
	{
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}

	KZGlobalService::EnforceConVars();

	ix::initNetSystem();

	KZGlobalService::WS::socket = std::make_unique<ix::WebSocket>();
	KZGlobalService::WS::socket->setUrl(apiUrl);
	KZGlobalService::WS::socket->setExtraHeaders({{"Authorization", std::string("Bearer ") + apiKey}});
	KZGlobalService::WS::socket->setOnMessageCallback(KZGlobalService::WS::OnMessage);
	KZGlobalService::WS::socket->setPingInterval(15);

	KZGlobalService::state.store(KZGlobalService::State::Configured);
}

void KZGlobalService::Cleanup()
{
	if (KZGlobalService::WS::socket)
	{
		KZGlobalService::WS::socket->stop();
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		KZGlobalService::WS::socket.reset(nullptr);

		ix::uninitNetSystem();
	}

	KZGlobalService::state.store(KZGlobalService::State::Uninitialized);
	KZGlobalService::RestoreConVars();
}

void KZGlobalService::UpdateRecordCache()
{
	// clang-format off
	u16 currentMapID = KZGlobalService::WithCurrentMap([](const std::optional<KZ::api::Map> &mapInfo) {
		return mapInfo ? mapInfo->id : 0;
	});
	// clang-format on

	if (currentMapID == 0)
	{
		META_CONPRINTF("[KZ::Global] Current map is not global; not updating record cache\n");
		return;
	}

	KZ::api::messages::WantWorldRecordsForCache message {currentMapID};
	KZGlobalService::MessageCallback<KZ::api::messages::WorldRecordsForCache> callback(KZGlobalService::OnWorldRecordsForCache);
	KZGlobalService::WS::SendMessage(message, std::move(callback));
}

void KZGlobalService::OnWorldRecordsForCache(const KZ::api::messages::WorldRecordsForCache &records)
{
	for (const KZ::api::Record &record : records.records)
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
}

void KZGlobalService::OnMapInfo(const std::optional<KZ::api::Map> &mapInfo)
{
	bool mapNameOk = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

	if (mapInfo.has_value())
	{
		if (mapNameOk)
		{
			META_CONPRINTF("[KZ::Global] %s is approved.\n", mapInfo->name.c_str());
			if (mapInfo->name == currentMapName.Get())
			{
				for (const auto &course : mapInfo->courses)
				{
					KZ::course::UpdateCourseGlobalID(course.name.c_str(), course.id);
					META_CONPRINTF("[KZ::Global] Registered course '%s' with ID %i!\n", course.name.c_str(), course.id);
				}
			}
		}
		else
		{
			META_CONPRINTF("[KZ::Global] Failed to get current map name.\n");
		}
	}
	else
	{
		META_CONPRINTF("[KZ::Global] %s is not approved.\n", currentMapName.Get());
	}

	{
		std::lock_guard _guard(KZGlobalService::currentMap.mutex);
		KZGlobalService::currentMap.info = std::move(mapInfo);
	}
}

void KZGlobalService::PrintAnnouncements()
{
	{
		std::lock_guard _guard(KZGlobalService::globalAnnouncements.mutex);
		for (const KZ::api::messages::handshake::HelloAck::Announcement &announcement : KZGlobalService::globalAnnouncements.announcements)
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
}

void KZGlobalService::OnActivateServer()
{
	if (KZGlobalService::state.load() == KZGlobalService::State::Uninitialized)
	{
		KZGlobalService::Init();
	}

	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::Configured:
		{
			META_CONPRINTF("[KZ::Global] Starting WebSocket...\n");
			KZGlobalService::WS::socket->start();
			KZGlobalService::state.store(KZGlobalService::State::Connecting);
		}
		break;

		case KZGlobalService::State::HandshakeCompleted:
		{
			KZ::api::messages::MapChange message;

			bool mapNameOk = false;
			CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

			if (!mapNameOk)
			{
				META_CONPRINTF("[KZ::Global] Failed to get current map name. Cannot send `map_change` event.\n");
				break;
			}

			message.name = currentMapName.Get();

			KZGlobalService::MessageCallback<std::optional<KZ::api::Map>> callback(KZGlobalService::OnMapInfo);
			KZGlobalService::WS::SendMessage(message, std::move(callback));
		}
		break;
	}
}

void KZGlobalService::OnServerGamePostSimulate()
{
	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::Connected:
		{
			KZGlobalService::WS::InitiateHandshake();
			return;
		}
		break;

		case KZGlobalService::State::HandshakeInitiated:
		{
			std::optional<KZ::api::messages::handshake::HelloAck> helloAck;

			{
				std::lock_guard _guard(KZGlobalService::callbacks.mutex);
				if (KZGlobalService::callbacks.helloAck)
				{
					KZGlobalService::callbacks.helloAck.swap(helloAck);
				}
			}

			if (helloAck)
			{
				KZGlobalService::WS::CompleteHandshake(std::move(helloAck.value()));
			}
		}
		break;

		case KZGlobalService::State::HandshakeCompleted:
		{
			{
				std::lock_guard _guard(KZGlobalService::callbacks.mutex);
				if (!KZGlobalService::callbacks.queue.empty())
				{
					META_CONPRINTF("[KZ::Global] Running callbacks...\n");
					for (const std::function<void()> &callback : KZGlobalService::callbacks.queue)
					{
						callback();
					}
					KZGlobalService::callbacks.queue.clear();
				}
			}

			{
				std::unordered_map<u32, std::unique_ptr<MessageCallbackInternal>> messageCallbacks;

				{
					std::lock_guard _messageCallbacksQueueGuard(KZGlobalService::ws.messageCallbacks.mutex);
					KZGlobalService::ws.messageCallbacks.callbacks.swap(messageCallbacks);
				}

				std::lock_guard _messagesQueueGuard(KZGlobalService::ws.receivedMessages.mutex);
				// for (WS::ReceivedMessage &message : KZGlobalService::ws.receivedMessages.queue)
				for (auto it = KZGlobalService::ws.receivedMessages.queue.begin(); it != KZGlobalService::ws.receivedMessages.queue.end();)
				{
					auto callbackHandle = messageCallbacks.extract(it->id);
					if (!callbackHandle)
					{
						++it;
						continue;
					}

					META_CONPRINTF("[KZ::Global] Running callback for message %i...\n", it->id);

					if (it->isError)
					{
						KZ::api::messages::Error error;

						if (error.FromJson(it->payload))
						{
							callbackHandle.mapped()->OnError(it->id, error);
						}
						else
						{
							META_CONPRINTF("[KZ::Global] Failed to decode `error` message (messageID=%i)\n", it->id);
						}
					}
					else if ((std::chrono::system_clock::now() - callbackHandle.mapped()->sentAt) > callbackHandle.mapped()->expiresAfter)
					{
						callbackHandle.mapped()->OnCancelled(it->id, KZGlobalService::MessageCallbackCancelReason::Timeout);
					}
					else
					{
						callbackHandle.mapped()->OnResponse(it->id, it->payload);
					}

					it = KZGlobalService::ws.receivedMessages.queue.erase(it);
				}

				KZGlobalService::ws.receivedMessages.queue.clear();

				{
					std::lock_guard _messageCallbacksQueueGuard(KZGlobalService::ws.messageCallbacks.mutex);
					KZGlobalService::ws.messageCallbacks.callbacks.merge(messageCallbacks);
				}
			}
		}
		break;

		default:
			break;
	}
}

static_function void OnPlayerRecordsReceived(const KZ::api::messages::PlayerRecords &records, u64 steamID)
{
	KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);

	if (player == nullptr)
	{
		return;
	}

	for (const KZ::api::Record &record : records.records)
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
}

void KZGlobalService::OnPlayerJoinAck(const KZ::api::messages::PlayerJoinAck &ack, u64 steamID)
{
	META_CONPRINTF("[KZ::Global] Received `player_join_ack` response. (player.id=%llu, player.is_banned=%s)\n", steamID,
				   ack.isBanned ? "true" : "false");

	KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);
	if (player == nullptr)
	{
		return;
	}

	player->globalService->playerInfo.isBanned = ack.isBanned;
	player->optionService->InitializeGlobalPrefs(ack.preferences.ToString());

	u16 currentMapID = KZGlobalService::WithCurrentMap([](const std::optional<KZ::api::Map> &map) { return map ? map->id : 0; });

	if (currentMapID != 0)
	{
		std::string_view event("want-player-records");
		KZ::api::messages::WantPlayerRecords message;
		message.mapID = currentMapID;
		message.playerID = steamID;

		KZGlobalService::MessageCallback<KZ::api::messages::PlayerRecords> callback(OnPlayerRecordsReceived, steamID);
		KZGlobalService::WS::SendMessage(message, std::move(callback));
	}
}

void KZGlobalService::OnPlayerAuthorized()
{
	if (KZGlobalService::state.load() != KZGlobalService::State::HandshakeCompleted)
	{
		return;
	}

	if (!this->player->IsConnected())
	{
		return;
	}
	assert(this->player->IsAuthenticated());

	u64 steamID = this->player->GetSteamId64();
	std::string stringifiedSteamID = std::to_string(steamID);

	KZ::api::messages::PlayerJoin message;
	message.id = stringifiedSteamID;
	message.name = this->player->GetName();
	message.ipAddress = this->player->GetIpAddress();

	KZGlobalService::MessageCallback<KZ::api::messages::PlayerJoinAck> callback(KZGlobalService::OnPlayerJoinAck, steamID);
	KZGlobalService::WS::SendMessage(message, std::move(callback));
}

void KZGlobalService::OnClientDisconnect()
{
	if (KZGlobalService::state.load() != KZGlobalService::State::HandshakeCompleted)
	{
		return;
	}

	if (!this->player->IsConnected() || !this->player->IsAuthenticated())
	{
		return;
	}

	u64 steamID = this->player->GetSteamId64();
	std::string stringifiedSteamID = std::to_string(steamID);

	KZ::api::messages::PlayerLeave message;
	message.id = stringifiedSteamID;
	message.name = this->player->GetName();

	CUtlString getPrefsError;
	CUtlString getPrefsResult;
	this->player->optionService->GetPreferencesAsJSON(&getPrefsError, &getPrefsResult);
	if (getPrefsError.IsEmpty())
	{
		message.preferences = Json(getPrefsResult.Get());
	}

	KZGlobalService::WS::SendMessage(message);
}

void KZGlobalService::WS::OnMessage(const ix::WebSocketMessagePtr &message)
{
	switch (message->type)
	{
		case ix::WebSocketMessageType::Open:
			return KZGlobalService::WS::OnOpenMessage();

		case ix::WebSocketMessageType::Close:
			return KZGlobalService::WS::OnCloseMessage(message->closeInfo);

		case ix::WebSocketMessageType::Error:
			return KZGlobalService::WS::OnErrorMessage(message->errorInfo);

		case ix::WebSocketMessageType::Ping:
			return META_CONPRINTF("[KZ::Global] Received ping WebSocket message.\n");

		case ix::WebSocketMessageType::Pong:
			return META_CONPRINTF("[KZ::Global] Received pong WebSocket message.\n");
	}

	META_CONPRINTF("[KZ::Global] Received WebSocket message.\n"
				   "----------------------------------------\n"
				   "%s"
				   "\n----------------------------------------\n",
				   message->str.c_str());

	Json payload(message->str);

	if (!payload.IsValid())
	{
		META_CONPRINTF("[KZ::Global] Incoming WebSocket message is not valid JSON.\n");
		return;
	}

	switch (KZGlobalService::state.load())
	{
		case KZGlobalService::State::HandshakeInitiated:
		{
			KZ::api::messages::handshake::HelloAck ack;

			if (!ack.FromJson(payload))
			{
				// maybe?
				// KZGlobalService::socket->stop();
				KZGlobalService::state.store(KZGlobalService::State::Disconnected);
				META_CONPRINTF("[KZ::Global] Failed to decode `hello_ack` message.\n");
				break;
			}

			{
				std::lock_guard _guard(KZGlobalService::callbacks.mutex);
				KZGlobalService::callbacks.helloAck = std::make_optional(std::move(ack));
			}
		}
		break;

		case KZGlobalService::State::HandshakeCompleted:
		{
			u32 messageID;
			if (!payload.Get("id", messageID))
			{
				META_CONPRINTF("[KZ::Global] Incoming WebSocket message did not contain a valid `id` field.\n");
				break;
			}

			std::string event;
			if (!payload.Get("event", event))
			{
				META_CONPRINTF("[KZ::Global] Incoming WebSocket message did not contain a valid `event` field.\n");
				break;
			}

			ReceivedMessage message;
			message.id = messageID;
			message.isError = (event == "error");

			if (!payload.Get("data", message.payload))
			{
				META_CONPRINTF("[KZ::Global] Incoming WebSocket message contained an invalid `data` field.\n");
				break;
			}

			{
				std::lock_guard _guard(KZGlobalService::ws.receivedMessages.mutex);
				KZGlobalService::ws.receivedMessages.queue.emplace_back(std::move(message));
			}
		}
		break;

		default:
			return;
	}
}

void KZGlobalService::WS::OnOpenMessage()
{
	KZGlobalService::state.store(KZGlobalService::State::Connected);
	META_CONPRINTF("[KZ::Global] WebSocket connection established.\n");
}

void KZGlobalService::WS::OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo)
{
	META_CONPRINTF("[KZ::Global] WebSocket connection closed (%i): %s\n", closeInfo.code, closeInfo.reason.c_str());

	switch (closeInfo.code)
	{
		case 1000 /* NORMAL */:
		case 1001 /* GOING AWAY */:
		case 1006 /* ABNORMAL */:
		{
			KZGlobalService::WS::socket->enableAutomaticReconnection();
			KZGlobalService::WS::socket->setMinWaitBetweenReconnectionRetries(10'000 /* ms */);
			KZGlobalService::state.store(KZGlobalService::State::Reconnecting);
		}
		break;

		case 1008 /* POLICY VIOLATION */:
		{
			if (closeInfo.reason.find("heartbeat timeout") != -1)
			{
				KZGlobalService::WS::socket->enableAutomaticReconnection();
				KZGlobalService::state.store(KZGlobalService::State::Reconnecting);
			}
			else
			{
				KZGlobalService::WS::socket->disableAutomaticReconnection();
				KZGlobalService::state.store(KZGlobalService::State::Disconnected);
			}
		}
		break;

		default:
		{
			KZGlobalService::WS::socket->enableAutomaticReconnection();
			KZGlobalService::WS::socket->setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
			KZGlobalService::state.store(KZGlobalService::State::Reconnecting);
		}
	}

	{
		std::lock_guard _guard(KZGlobalService::callbacks.mutex);
		KZGlobalService::callbacks.queue.clear();
	}

	{
		std::lock_guard _messagesQueueGuard(KZGlobalService::ws.receivedMessages.mutex);
		KZGlobalService::ws.receivedMessages.queue.clear();
	}

	{
		std::lock_guard _messageCallbacksQueueGuard(KZGlobalService::ws.messageCallbacks.mutex);
		for (const auto &[messageID, callback] : KZGlobalService::ws.messageCallbacks.callbacks)
		{
			callback->OnCancelled(messageID, KZGlobalService::MessageCallbackCancelReason::Disconnect);
		}
		KZGlobalService::ws.messageCallbacks.callbacks.clear();
	}
}

void KZGlobalService::WS::OnErrorMessage(const ix::WebSocketErrorInfo &errorInfo)
{
	META_CONPRINTF("[KZ::Global] WebSocket error (status %i, retries=%i, wait_time=%f): %s\n", errorInfo.http_status, errorInfo.retries,
				   errorInfo.wait_time, errorInfo.reason.c_str());

	switch (errorInfo.http_status)
	{
		case 401:
		case 403:
		{
			KZGlobalService::WS::socket->disableAutomaticReconnection();
			KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		}
		break;

		default:
		{
			KZGlobalService::WS::socket->enableAutomaticReconnection();
			KZGlobalService::WS::socket->setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
			KZGlobalService::state.store(KZGlobalService::State::Reconnecting);
		}
	}
}

void KZGlobalService::WS::InitiateHandshake()
{
	KZGlobalService::State currentState = KZGlobalService::state.load();

	if (currentState != KZGlobalService::State::Connected)
	{
		return;
	}

	if (!KZGlobalService::state.compare_exchange_strong(currentState, KZGlobalService::State::HandshakeInitiated))
	{
		return;
	}

	META_CONPRINTF("[KZ::Global] Initiating WebSocket handshake...\n");

	bool mapNameOk = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

	if (!mapNameOk)
	{
		META_CONPRINTF("[KZ::Global] Failed to get current map name.\n");
		KZGlobalService::state.store(KZGlobalService::State::Disconnected);
		return;
	}

	KZ::api::messages::handshake::Hello hello(g_KZPlugin.GetMD5(), currentMapName.Get());

	for (Player *player : g_pPlayerManager->players)
	{
		if (player && player->IsAuthenticated())
		{
			hello.AddPlayer(player->GetSteamId64(), player->GetName());
		}
	}

	KZGlobalService::WS::socket->send(Json(hello).ToString());
}

void KZGlobalService::WS::CompleteHandshake(KZ::api::messages::handshake::HelloAck &&ack)
{
	KZGlobalService::State currentState = KZGlobalService::state.load();

	if ((currentState != KZGlobalService::State::HandshakeInitiated) && (currentState != KZGlobalService::State::Reconnecting))
	{
		META_CONPRINTF("[KZ::Global] Unexpected state when calling `CompleteHandshake()`.\n");
	}

	META_CONPRINTF("[KZ::Global] Completing WebSocket handshake...\n");
	// META_CONPRINTF("[KZ::Global] WebSocket session ID: `%s`\n", ack.sessionID.c_str());

	{
		std::lock_guard _guard(KZGlobalService::currentMap.mutex);
		KZGlobalService::currentMap.info = std::move(ack.mapInfo);
	}

	{
		std::lock_guard _guard(KZGlobalService::globalModes.mutex);

		for (const KZ::api::messages::handshake::HelloAck::ModeInfo &modeInfo : ack.modes)
		{
			switch (modeInfo.mode)
			{
				case KZ::api::Mode::Vanilla:
				{
#ifdef _WIN32
					KZGlobalService::globalModes.checksums.vanilla = modeInfo.windowsChecksum;
#else
					KZGlobalService::globalModes.checksums.vanilla = modeInfo.linuxChecksum;
#endif
				}
				break;

				case KZ::api::Mode::Classic:
				{
#ifdef _WIN32
					KZGlobalService::globalModes.checksums.classic = modeInfo.windowsChecksum;
#else
					KZGlobalService::globalModes.checksums.classic = modeInfo.linuxChecksum;
#endif
				}
				break;
			}
		}
	}

	{
		std::lock_guard _guard(KZGlobalService::globalStyles.mutex);

		for (const KZ::api::messages::handshake::HelloAck::StyleInfo &styleInfo : ack.styles)
		{
#ifdef _WIN32
			const std::string &checksum = styleInfo.windowsChecksum;
#else
			const std::string &checksum = styleInfo.linuxChecksum;
#endif

			KZGlobalService::globalStyles.checksums.emplace_back(KZGlobalService::GlobalStyleChecksum {styleInfo.style, checksum});
		}
	}

	{
		std::lock_guard _guard(KZGlobalService::globalAnnouncements.mutex);
		KZGlobalService::globalAnnouncements.announcements.swap(ack.announcements);
	}

	if (!KZGlobalService::state.compare_exchange_strong(currentState, KZGlobalService::State::HandshakeCompleted))
	{
		META_CONPRINTF("[KZ::Global] State changed unexpectedly during `CompleteHandshake()`.\n");
	}

	for (Player *player : g_pKZPlayerManager->players)
	{
		if (player && player->IsConnected() && player->IsAuthenticated())
		{
			g_pKZPlayerManager->ToKZPlayer(player)->globalService->OnPlayerAuthorized();
		}
	}
}
