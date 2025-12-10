// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include "kz_racing.h"
#include "kz/option/kz_option.h"
#include "kz/timer/kz_timer.h"
#include <ixwebsocket/IXNetSystem.h>

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

	META_CONPRINTF("[KZ::Racing] Initializing RacingService...\n");

	std::string url = KZOptionService::GetOptionStr("racingCoordinatorUrl", "");

	if (url.empty())
	{
		META_CONPRINTF("[KZ::Racing] `racingCoordinatorUrl` is empty. RacingService will be disabled.\n");
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		return;
	}

	if (url.size() < 4 || url.substr(0, 4) != "http")
	{
		META_CONPRINTF("[KZ::Racing] `racingCoordinatorUrl` is invalid. RacingService will be disabled.\n");
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		return;
	}

	url.replace(0, 4, "ws");

	std::string key = KZOptionService::GetOptionStr("racingSecret");

	if (key.empty())
	{
		META_CONPRINTF("[KZ::Racing] `racingSecret` is empty. RacingService will be disabled.\n");
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		return;
	}

	KZRacingService::socket = std::make_unique<ix::WebSocket>();
	KZRacingService::socket->setUrl(url);
	KZRacingService::socket->setExtraHeaders({{"Authorization", std::string("Bearer ") + key}});
	KZRacingService::socket->setOnMessageCallback(KZRacingService::OnWebSocketMessage);

	KZRacingService::state.store(KZRacingService::State::Configured);
	META_CONPRINTF("[KZ::Racing] RacingService configured.\n");

	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZRacingService::Cleanup()
{
	if (KZRacingService::socket)
	{
		KZRacingService::socket->stop();
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		KZRacingService::socket.reset(nullptr);
	}

	KZRacingService::state.store(KZRacingService::State::Uninitialized);
	META_CONPRINTF("[KZ::Racing] RacingService cleaned up.\n");
}

void KZRacingService::OnActivateServer()
{
	if (KZRacingService::state.load() == KZRacingService::State::Uninitialized)
	{
		KZRacingService::Init();
	}

	if (KZRacingService::state.load() == KZRacingService::State::Configured)
	{
		META_CONPRINTF("[KZ::Racing] Starting WebSocket...\n");
		KZRacingService::socket->setPingInterval(10);
		KZRacingService::socket->start();
		KZRacingService::state.store(KZRacingService::State::Connecting);
	}

	if (KZRacingService::state.load() == KZRacingService::State::Connected && KZRacingService::currentRace.state == RaceInfo::RACE_INIT)
	{
		if (g_pKZUtils->GetCurrentMapWorkshopID() == KZRacingService::currentRace.data.raceInfo.workshopID)
		{
			KZRacingService::SendRaceJoin();
		}
		else
		{
			KZRacingService::SendRaceLeave();
		}
	}
}

void KZRacingService::OnServerGamePostSimulate()
{
	KZRacingService::ProcessMainThreadCallbacks();
	if (KZRacingService::currentRace.data.raceInfo.duration > 0
		&& (KZRacingService::currentRace.earliestStartTick + KZRacingService::currentRace.data.raceInfo.duration * ENGINE_FIXED_TICK_RATE
				<= g_pKZUtils->GetServerGlobals()->tickcount
			&& KZRacingService::currentRace.state == RaceInfo::RACE_ONGOING))
	{
		META_CONPRINTF("[KZ::Racing] Race duration expired.\n");
		KZRacingService::SendRaceEnd(false);
		KZRacingService::currentRace.state = RaceInfo::RACE_NONE;
	}
}

void KZRacingService::ProcessMainThreadCallbacks()
{
	{
		std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
		if (!KZRacingService::mainThreadCallbacks.queue.empty())
		{
			META_CONPRINTF("[KZ::Racing] Running callbacks...\n");
			for (const std::function<void()> &callback : KZRacingService::mainThreadCallbacks.queue)
			{
				callback();
			}
			KZRacingService::mainThreadCallbacks.queue.clear();
		}
	}
}

void KZRacingService::OnWebSocketMessage(const ix::WebSocketMessagePtr &message)
{
	switch (message->type)
	{
		case ix::WebSocketMessageType::Open:
			return KZRacingService::WS_OnOpenMessage();

		case ix::WebSocketMessageType::Close:
			return KZRacingService::WS_OnCloseMessage(message->closeInfo);

		case ix::WebSocketMessageType::Error:
			return KZRacingService::WS_OnErrorMessage(message->errorInfo);

		case ix::WebSocketMessageType::Ping:
			return META_CONPRINTF("[KZ::Racing] Received ping WebSocket message.\n");

		case ix::WebSocketMessageType::Pong:
			return META_CONPRINTF("[KZ::Racing] Received pong WebSocket message.\n");
	}

	META_CONPRINTF("[KZ::Racing] Received WebSocket message.\n"
				   "----------------------------------------\n"
				   "%s"
				   "\n----------------------------------------\n",
				   message->str.c_str());

	Json payload(message->str);

	if (!payload.IsValid())
	{
		META_CONPRINTF("[KZ::Racing] Incoming WebSocket message is not valid JSON.\n");
		return;
	}

	switch (KZRacingService::state.load())
	{
		case KZRacingService::State::Connected:
		{
			std::string event;
			if (!payload.Get("event", event))
			{
				META_CONPRINTF("[KZ::Racing] Incoming WebSocket message did not contain a valid `event` field.\n");
				break;
			}

			Json data;
			if (!payload.Get("data", data))
			{
				META_CONPRINTF("[KZ::Racing] Incoming WebSocket message did not contain a valid `data` field.\n");
				break;
			}

			// Dispatch based on event type
			if (event == "init_race")
			{
				KZ::racing::events::RaceInit raceInit;
				if (raceInit.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceInit]() { KZRacingService::OnRaceInit(raceInit); });
				}
			}
			else if (event == "cancel_race")
			{
				KZ::racing::events::RaceCancel raceCancel;
				if (raceCancel.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceCancel]() { KZRacingService::OnRaceCancel(raceCancel); });
				}
			}
			else if (event == "start_race")
			{
				KZ::racing::events::RaceStart raceStart;
				if (raceStart.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceStart]() { KZRacingService::OnRaceStart(raceStart); });
				}
			}
			else if (event == "add_race_participant")
			{
				KZ::racing::events::PlayerAccept playerAccept;
				if (playerAccept.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerAccept]() { KZRacingService::OnPlayerAccept(playerAccept); });
				}
			}
			else if (event == "remove_race_participant")
			{
				KZ::racing::events::PlayerUnregister playerUnreg;
				if (playerUnreg.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerUnreg]() { KZRacingService::OnPlayerUnregister(playerUnreg); });
				}
			}
			else if (event == "player_forfeit")
			{
				KZ::racing::events::PlayerForfeit playerForfeit;
				if (playerForfeit.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerForfeit]() { KZRacingService::OnPlayerForfeit(playerForfeit); });
				}
			}
			else if (event == "player_finished")
			{
				KZ::racing::events::PlayerFinish playerFinish;
				if (playerFinish.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerFinish]() { KZRacingService::OnPlayerFinish(playerFinish); });
				}
			}
			else if (event == "end_race")
			{
				KZ::racing::events::RaceEnd raceEnd;
				if (raceEnd.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceEnd]() { KZRacingService::OnRaceEnd(raceEnd); });
				}
			}
			else if (event == "race_finished")
			{
				KZ::racing::events::RaceResult raceResult;
				if (raceResult.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceResult]() { KZRacingService::OnRaceResult(raceResult); });
				}
			}
			else if (event == "chat_message")
			{
				KZ::racing::events::ChatMessage chatMessage;
				if (chatMessage.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([chatMessage]() { KZRacingService::OnChatMessage(chatMessage); });
				}
			}
			else
			{
				META_CONPRINTF("[KZ::Racing] Incoming WebSocket message contained an unknown `event` field: `%s`\n", event.c_str());
			}
		}
		break;

		default:
			return;
	}
}

void KZRacingService::WS_OnOpenMessage()
{
	KZRacingService::state.store(KZRacingService::State::Connected);
	META_CONPRINTF("[KZ::Racing] WebSocket connection established.\n");
}

void KZRacingService::WS_OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo)
{
	META_CONPRINTF("[KZ::Racing] WebSocket connection closed (%i): %s\n", closeInfo.code, closeInfo.reason.c_str());

	// TODO: cleanup race state

	switch (closeInfo.code)
	{
		case 1000 /* NORMAL */:
		case 1001 /* GOING AWAY */:
		case 1006 /* ABNORMAL */:
		{
			KZRacingService::socket->enableAutomaticReconnection();
			KZRacingService::socket->setMinWaitBetweenReconnectionRetries(10'000 /* ms */);
			KZRacingService::state.store(KZRacingService::State::Connecting);
		}
		break;

		case 1008 /* POLICY VIOLATION */:
		{
			KZRacingService::socket->disableAutomaticReconnection();
			KZRacingService::state.store(KZRacingService::State::Disconnected);
		}
		break;

		default:
		{
			KZRacingService::socket->enableAutomaticReconnection();
			KZRacingService::socket->setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
			KZRacingService::state.store(KZRacingService::State::Connecting);
		}
	}
}

void KZRacingService::WS_OnErrorMessage(const ix::WebSocketErrorInfo &errorInfo)
{
	META_CONPRINTF("[KZ::Racing] WebSocket error (status %i, retries=%i, wait_time=%f): %s\n", errorInfo.http_status, errorInfo.retries,
				   errorInfo.wait_time, errorInfo.reason.c_str());

	switch (errorInfo.http_status)
	{
		case 401:
		case 403:
		{
			KZRacingService::socket->disableAutomaticReconnection();
			KZRacingService::state.store(KZRacingService::State::Disconnected);
		}
		break;

		default:
		{
			KZRacingService::socket->enableAutomaticReconnection();
			KZRacingService::socket->setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
			KZRacingService::state.store(KZRacingService::State::Connecting);
		}
	}
}
