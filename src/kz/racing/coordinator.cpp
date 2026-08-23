#include "kz_racing.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"

static_global class KZTimerServiceEventListener_Racing : public KZTimerServiceEventListener
{
	virtual bool OnTimerStart(KZPlayer *player, u32 courseGUID) override
	{
		return player->racingService->OnTimerStart(courseGUID);
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override
	{
		player->racingService->OnTimerEndPost(courseGUID, time, teleportsUsed);
	}
} timerEventListener;

void KZRacingService::Init()
{
	if (KZRacingService::state.load() != KZRacingService::State::Uninitialized)
	{
		return;
	}

	KZ_LOG_INFO(LogChannel::Racing, "Initializing RacingService...");

	KeyValues *config = KZOptionService::GetOptionKV("GlobalKZ Race Coordinator");

	if (!config)
	{
		KZ_LOG_INFO(LogChannel::Racing, "no configuration found; RacingService will be disabled");
		return;
	}

	std::string authToken(config->GetString("token", ""));

	if (authToken.empty())
	{
		KZ_LOG_INFO(LogChannel::Racing, "auth token is empty; RacingService will be disabled");
		return;
	}

	std::string url(config->GetString("baseURL", ""));

	if (url.empty())
	{
		KZ_LOG_INFO(LogChannel::Global, "base URL is empty; RacingService will be disabled");
		return;
	}

	if (url.size() < 4 || url.substr(0, 4) != "http")
	{
		KZ_LOG_WARN(LogChannel::Global, "base URL is invalid; RacingService will be disabled\n");
		return;
	}

	url.replace(0, 4, "ws");

	if (url.substr(url.size() - 1) != "/")
	{
		url += "/";
	}

	url += "gss";

	std::shared_ptr<KZWebSocket> socket = std::make_shared<KZWebSocket>();

	// clang-format off
	socket->Configure(url, {{"Authorization", std::string("Bearer ") + authToken}}, [=](const ix::WebSocketMessagePtr& message) {
		return KZRacingService::OnWebSocketMessage(*socket, message);
	});
	// clang-format on

	KZRacingService::socket = std::make_unique<KZWebSocket::Handle>(KZWebSocket::SpawnDispatchThread(socket));
	KZRacingService::state.store(KZRacingService::State::Configured);
	KZ_LOG_INFO(LogChannel::Racing, "RacingService configured.\n");
	KZRacingService::itemDownloadHandler.m_CallbackDownloadItemResult.Register(&KZRacingService::itemDownloadHandler,
																			   &KZRacingService::ItemDownloadHandler::OnAddonDownloaded);

	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZRacingService::Cleanup()
{
	if (KZRacingService::socket)
	{
		KZRacingService::socket->Shutdown();
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		KZRacingService::socket.reset(nullptr);
	}

	KZTimerService::UnregisterEventListener(&timerEventListener);
	KZRacingService::itemDownloadHandler.m_CallbackDownloadItemResult.Unregister();
	KZRacingService::state.store(KZRacingService::State::Uninitialized);
	KZ_LOG_INFO(LogChannel::Racing, "RacingService cleaned up.\n");
}

void KZRacingService::ReloadConfig()
{
	KZRacingService::Cleanup();
	KZRacingService::Init();

	if (KZRacingService::state.load() != KZRacingService::State::Configured)
	{
		return;
	}

	KZ_LOG_INFO(LogChannel::Racing, "Starting WebSocket...\n");
	KZRacingService::state.store(KZRacingService::State::Connecting);
	KZRacingService::socket->Start();
}

void KZRacingService::OnActivateServer()
{
	if (KZRacingService::state.load() == KZRacingService::State::Uninitialized)
	{
		KZRacingService::Init();
	}

	if (KZRacingService::state.load() == KZRacingService::State::Configured)
	{
		KZ_LOG_INFO(LogChannel::Racing, "Starting WebSocket...\n");
		KZRacingService::socket->Start();
		KZRacingService::state.store(KZRacingService::State::Connecting);
	}

	if (KZRacingService::state.load() == KZRacingService::State::Connected && KZRacingService::currentRace.state == RaceInfo::State::Init)
	{
		if (g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.spec.workshopID)
		{
			KZRacingService::SendReady();
		}
		else
		{
			KZRacingService::SendUnready();
		}
	}
}

void KZRacingService::OnServerGamePostSimulate()
{
	KZRacingService::ProcessMainThreadCallbacks();
	if (KZRacingService::currentRace.spec.maxDurationSeconds > 0
		&& (KZRacingService::currentRace.earliestStartTick + KZRacingService::currentRace.spec.maxDurationSeconds * ENGINE_FIXED_TICK_RATE
				<= g_pKZUtils->GetServerGlobals()->tickcount
			&& KZRacingService::currentRace.state == RaceInfo::State::Ongoing))
	{
		KZ_LOG_INFO(LogChannel::Racing, "Race duration expired.\n");
		KZRacingService::SendRaceFinished();
		KZRacingService::currentRace.state = RaceInfo::State::None;
	}
}

void KZRacingService::ProcessMainThreadCallbacks()
{
	{
		std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
		if (!KZRacingService::mainThreadCallbacks.queue.empty())
		{
			KZ_LOG_INFO(LogChannel::Racing, "Running callbacks...\n");
			for (const std::function<void()> &callback : KZRacingService::mainThreadCallbacks.queue)
			{
				callback();
			}
			KZRacingService::mainThreadCallbacks.queue.clear();
		}
	}

	std::vector<KZWebSocket::Message> receivedMessages;
	KZRacingService::socket->ReceiveMessages(receivedMessages);

	for (const KZWebSocket::Message &message : receivedMessages)
	{
		KZ_LOG_DEBUG(LogChannel::Racing, "processing message %s (%s)", message.id.c_str(), message.tag.c_str());

		switch (KZRacingService::state.load())
		{
			case KZRacingService::State::Connected:
			{
				// Dispatch based on message tag
				if (message.tag == "chat_message")
				{
					KZ::racing::events::ChatMessage event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnChatMessage(event);
					}
				}
				else if (message.tag == "race_initialized")
				{
					KZ::racing::events::RaceInitialized event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnRaceInitialized(event);
					}
				}
				else if (message.tag == "server_join_race")
				{
					KZ::racing::events::ServerJoinRace event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnServerJoinRace(event);
					}
				}
				else if (message.tag == "server_leave_race")
				{
					KZ::racing::events::ServerLeaveRace event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnServerLeaveRace(event);
					}
				}
				else if (message.tag == "player_join_race")
				{
					KZ::racing::events::PlayerJoinRace event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnPlayerJoinRace(event);
					}
				}
				else if (message.tag == "player_leave_race")
				{
					KZ::racing::events::PlayerLeaveRace event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnPlayerLeaveRace(event);
					}
				}
				else if (message.tag == "start_race")
				{
					KZ::racing::events::StartRace event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnStartRace(event);
					}
				}
				else if (message.tag == "player_finish")
				{
					KZ::racing::events::PlayerFinish event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnPlayerFinish(event);
					}
				}
				else if (message.tag == "player_disconnect")
				{
					KZ::racing::events::PlayerDisconnect event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnPlayerDisconnect(event);
					}
				}
				else if (message.tag == "player_surrender")
				{
					KZ::racing::events::PlayerSurrender event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnPlayerSurrender(event);
					}
				}
				else if (message.tag == "race_finished")
				{
					KZ::racing::events::RaceFinished event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnRaceFinished(event);
					}
				}
				else if (message.tag == "race_cancelled")
				{
					KZ::racing::events::RaceCancelled event;
					if (event.FromJson(message.data))
					{
						KZRacingService::OnRaceCancelled(event);
					}
				}
				else
				{
					KZ_LOG_WARN(LogChannel::Racing, "incoming message contained an unknown tag: `%s`\n", message.tag.c_str());
				}
			}
			break;

			default:
				continue;
		}
	}
}

void KZRacingService::OnWebSocketMessage(KZWebSocket &socket, const ix::WebSocketMessagePtr &message)
{
	// Runs on the ixwebsocket thread. Every event handler below parses JSON and allocates a
	// std::function onto the main-thread queue, so one scope here covers all of them.

	socket.OnMessage(message);

	switch (message->type)
	{
		case ix::WebSocketMessageType::Open:
			return KZRacingService::WS_OnOpenMessage();

		case ix::WebSocketMessageType::Close:
			return KZRacingService::WS_OnCloseMessage(message->closeInfo);

		case ix::WebSocketMessageType::Error:
			return KZRacingService::WS_OnErrorMessage(message->errorInfo);

		default:
			return;
	}
}

void KZRacingService::WS_OnOpenMessage()
{
	KZRacingService::state.store(KZRacingService::State::Connected);
}

void KZRacingService::WS_OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo)
{
	KZRacingService::currentRace = {};
	KZLanguageService::PrintChatAll(true, "Racing - Race Cancelled");

	if (KZRacingService::socket->MayReconnect())
	{
		KZRacingService::state.store(KZRacingService::State::Connecting);
	}
	else
	{
		KZRacingService::state.store(KZRacingService::State::Disconnected);
	}
}

void KZRacingService::WS_OnErrorMessage(const ix::WebSocketErrorInfo &errorInfo)
{
	if (KZRacingService::socket->MayReconnect())
	{
		KZRacingService::state.store(KZRacingService::State::Connecting);
	}
	else
	{
		KZRacingService::state.store(KZRacingService::State::Disconnected);
	}
}
