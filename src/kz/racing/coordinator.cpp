// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include "kz_racing.h"
#include "kz/option/kz_option.h"
#include <ixwebsocket/IXNetSystem.h>

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

	if (url.size() < 4 || url.substr(0, 4) != "ws")
	{
		META_CONPRINTF("[KZ::Racing] `racingCoordinatorUrl` is invalid. RacingService will be disabled.\n");
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		return;
	}

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
}

void KZRacingService::Cleanup()
{
	if (KZRacingService::socket)
	{
		KZRacingService::socket->stop();
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		KZRacingService::socket.reset(nullptr);
	}

	{
		std::lock_guard _guard(KZRacingService::heartbeatThread.mutex);
		if (KZRacingService::heartbeatThread.handle.joinable())
		{
			KZRacingService::heartbeatThread.shutdownNotification.cv.notify_all();
			KZRacingService::heartbeatThread.handle.join();
		}
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
		KZRacingService::socket->start();
		KZRacingService::state.store(KZRacingService::State::Connecting);
	}

	if (KZRacingService::state.load() == KZRacingService::State::HandshakeCompleted)
	{
		KZRacingService::SendMapUpdate(g_pKZUtils->GetCurrentMapName().Get(), g_pKZUtils->GetCurrentMapWorkshopID());
	}
}

void KZRacingService::OnServerGamePostSimulate()
{
	KZRacingService::ProcessMainThreadCallbacks();
	if (KZRacingService::currentRace.earliestStartTick + KZRacingService::currentRace.data.raceInfo.duration * ENGINE_FIXED_TICK_RATE
			<= g_pKZUtils->GetServerGlobals()->tickcount
		&& KZRacingService::currentRace.state == RaceInfo::RACE_ONGOING)
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
		case KZRacingService::State::HandshakeInitiated:
		case KZRacingService::State::Reconnecting:
		{
			KZ::racing::handshake::HelloAck ack;

			if (!ack.FromJson(payload))
			{
				KZRacingService::state.store(KZRacingService::State::Disconnected);
				META_CONPRINTF("[KZ::Racing] Failed to decode `hello_ack` message.\n");
				break;
			}

			{
				std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);

				// clang-format off
				KZRacingService::mainThreadCallbacks.queue.emplace_back([ack = std::move(ack)]() mutable {
					return KZRacingService::WS_CompleteHandshake(std::move(ack));
				});
				// clang-format on
			}
		}
		break;

		case KZRacingService::State::HandshakeCompleted:
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
			if (event == "race-init")
			{
				KZ::racing::events::RaceInit raceInit;
				if (raceInit.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceInit]() { KZRacingService::OnRaceInit(raceInit); });
				}
			}
			else if (event == "race-start")
			{
				KZ::racing::events::RaceStart raceStart;
				if (raceStart.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceStart]() { KZRacingService::OnRaceStart(raceStart); });
				}
			}
			else if (event == "player-forfeit")
			{
				KZ::racing::events::PlayerForfeit playerForfeit;
				if (playerForfeit.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerForfeit]() { KZRacingService::OnPlayerForfeit(playerForfeit); });
				}
			}
			else if (event == "player-finish")
			{
				KZ::racing::events::PlayerFinish playerFinish;
				if (playerFinish.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([playerFinish]() { KZRacingService::OnPlayerFinish(playerFinish); });
				}
			}
			else if (event == "race-end")
			{
				KZ::racing::events::RaceEnd raceEnd;
				if (raceEnd.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceEnd]() { KZRacingService::OnRaceEnd(raceEnd); });
				}
			}
			else if (event == "race-result")
			{
				KZ::racing::events::RaceResult raceResult;
				if (raceResult.FromJson(data))
				{
					std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
					KZRacingService::mainThreadCallbacks.queue.emplace_back([raceResult]() { KZRacingService::OnRaceResult(raceResult); });
				}
			}
			else if (event == "chat-message")
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
	{
		std::lock_guard _guard(KZRacingService::mainThreadCallbacks.mutex);
		KZRacingService::mainThreadCallbacks.queue.emplace_back([]() { return KZRacingService::WS_InitiateHandshake(); });
	}
}

void KZRacingService::WS_OnCloseMessage(const ix::WebSocketCloseInfo &closeInfo)
{
	META_CONPRINTF("[KZ::Racing] WebSocket connection closed (%i): %s\n", closeInfo.code, closeInfo.reason.c_str());

	switch (closeInfo.code)
	{
		case 1000 /* NORMAL */:
		case 1001 /* GOING AWAY */:
		case 1006 /* ABNORMAL */:
		{
			KZRacingService::socket->enableAutomaticReconnection();
			KZRacingService::socket->setMinWaitBetweenReconnectionRetries(10'000 /* ms */);
			KZRacingService::state.store(KZRacingService::State::Reconnecting);
		}
		break;

		case 1008 /* POLICY VIOLATION */:
		{
			if (closeInfo.reason.find("heartbeat timeout") != -1)
			{
				KZRacingService::socket->enableAutomaticReconnection();
				KZRacingService::state.store(KZRacingService::State::Reconnecting);
			}
			else
			{
				KZRacingService::socket->disableAutomaticReconnection();
				KZRacingService::state.store(KZRacingService::State::Disconnected);
			}
		}
		break;

		default:
		{
			KZRacingService::socket->enableAutomaticReconnection();
			KZRacingService::socket->setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
			KZRacingService::state.store(KZRacingService::State::Reconnecting);
		}
	}

	{
		std::lock_guard _guard(KZRacingService::heartbeatThread.mutex);
		if (KZRacingService::heartbeatThread.handle.joinable())
		{
			KZRacingService::heartbeatThread.shutdownNotification.cv.notify_all();
			KZRacingService::heartbeatThread.handle.join();
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
			KZRacingService::state.store(KZRacingService::State::Reconnecting);
		}
	}
}

void KZRacingService::WS_InitiateHandshake()
{
	KZRacingService::State currentState = KZRacingService::state.load();

	if (currentState != KZRacingService::State::Connected)
	{
		return;
	}

	if (!KZRacingService::state.compare_exchange_strong(currentState, KZRacingService::State::HandshakeInitiated))
	{
		return;
	}

	META_CONPRINTF("[KZ::Racing] Initiating WebSocket handshake...\n");

	KZ::racing::handshake::Hello message;

	// TODO: Get current map info properly
	bool mapNameOk = false;
	CUtlString currentMapName = g_pKZUtils->GetCurrentMapName(&mapNameOk);

	if (mapNameOk)
	{
		message.mapInfo.mapName = currentMapName.Get();
	}

	// TODO: Get workshop ID
	message.mapInfo.workshopID = 0;

	Json payload;
	if (!message.ToJson(payload))
	{
		META_CONPRINTF("[KZ::Racing] Failed to serialize 'Hello' message\n");
		KZRacingService::state.store(KZRacingService::State::Disconnected);
		return;
	}

	KZRacingService::socket->send(payload.ToString());
}

void KZRacingService::WS_CompleteHandshake(KZ::racing::handshake::HelloAck &&ack)
{
	KZRacingService::State currentState = KZRacingService::state.load();

	if ((currentState != KZRacingService::State::HandshakeInitiated) && (currentState != KZRacingService::State::Reconnecting))
	{
		META_CONPRINTF("[KZ::Racing] Unexpected state when calling `WS_CompleteHandshake()`.\n");
	}

	META_CONPRINTF("[KZ::Racing] Completing WebSocket handshake...\n");

	{
		std::lock_guard _guard(KZRacingService::heartbeatThread.mutex);
		if (!KZRacingService::heartbeatThread.handle.joinable())
		{
			META_CONPRINTF("[KZ::Racing] Spawning WebSocket heartbeat thread...\n");
			KZRacingService::heartbeatThread.handle = std::thread(KZRacingService::WS_Heartbeat, static_cast<u64>(ack.heartbeatInterval));
		}
	}

	// If there's an active race, update the local state
	if (ack.raceInfo.has_value())
	{
		META_CONPRINTF("[KZ::Racing] Active race detected: %s\n", ack.raceInfo->mapName.c_str());
		// TODO: Handle active race
	}

	if (!KZRacingService::state.compare_exchange_strong(currentState, KZRacingService::State::HandshakeCompleted))
	{
		META_CONPRINTF("[KZ::Racing] State changed unexpectedly during `WS_CompleteHandshake()`.\n");
	}
}

void KZRacingService::WS_Heartbeat(u64 intervalInSeconds)
{
	assert(intervalInSeconds > 0);

	// `* 800` here is effectively `* 0.8` (in milliseconds), which we do to ensure we send heartbeats frequently
	// enough even under sub-optimal network conditions.
	std::chrono::milliseconds interval(intervalInSeconds * 800);

	std::unique_lock shutdownLock(KZRacingService::heartbeatThread.shutdownNotification.mutex);
	KZRacingService::State lastKnownState = KZRacingService::state.load();

	while (lastKnownState != KZRacingService::State::Disconnected)
	{
		// clang-format off
		bool shutdownRequested = KZRacingService::heartbeatThread.shutdownNotification.cv.wait_for(shutdownLock, interval, [&]() {
			lastKnownState = KZRacingService::state.load();
			return ((lastKnownState == KZRacingService::State::Reconnecting)
			     || (lastKnownState == KZRacingService::State::Disconnected));
		});
		// clang-format on

		if (shutdownRequested)
		{
			META_CONPRINTF("[KZ::Racing] WebSocket heartbeat thread shutting down...\n");
			break;
		}

		KZRacingService::socket->ping("");
		META_CONPRINTF("[KZ::Racing] Sent heartbeat WebSocket message. (interval=%is)\n",
					   std::chrono::duration_cast<std::chrono::seconds>(interval).count());
	}
}
