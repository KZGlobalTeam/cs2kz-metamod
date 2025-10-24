#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_replaybot.h"
#include "kz_replay.h"
#include "kz_replaydata.h"
#include "kz_replayitem.h"
#include "kz/spec/kz_spec.h"
#include "utils/ctimer.h"

static CHandle<CCSPlayerController> g_replayBot;
extern CConVar<bool> kz_replay_playback_skins_enable;

static_function f64 SetBotModel()
{
	auto bot = g_replayBot.Get();
	if (!bot)
	{
		return -1.0f;
	}
	GeneralReplayHeader &header = KZ::replaysystem::data::GetCurrentReplay()->header;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
	KZ::replaysystem::item::ApplyModelAttributesToPawn(player->GetPlayerPawn(), header.gloves, header.modelName);
	return -1.0f;
}

namespace KZ::replaysystem::bot
{
	void KickBot()
	{
		if (g_KZPlugin.unloading)
		{
			return;
		}
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return;
		}
		interfaces::pEngine->KickClient(bot->GetPlayerSlot(), "bye bot", NETWORK_DISCONNECT_KICKED);
		g_replayBot.Term();
		CConVarRef<int> bot_quota("bot_quota");
		bot_quota.Set(bot_quota.Get() - 1);
	}

	void CheckBots()
	{
		// Make sure to kick bots that aren't ours.
		CConVarRef<int> bot_quota("bot_quota");
		if (!g_replayBot.Get() && bot_quota.Get() > 0)
		{
			bot_quota.Set(0);
		}
	}

	void SpawnBot()
	{
		if (g_replayBot.Get())
		{
			return;
		}
		BotProfile *profile = BotProfile::PersistentProfile(CS_TEAM_T);
		if (!profile)
		{
			return;
		}
		CCSPlayerController *bot = g_pKZUtils->CreateBot(profile, CS_TEAM_T);
		if (!bot)
		{
			return;
		}

		g_replayBot = bot;
		CConVarRef<int> bot_quota("bot_quota");
		bot_quota.Set(bot_quota.Get() + 1);
	}

	void MakeBotAlive()
	{
		SpawnBot();
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
		KZ::misc::JoinTeam(player, CS_TEAM_CT, false);
	}

	void MoveBotToSpec()
	{
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
		KZ::misc::JoinTeam(player, CS_TEAM_SPECTATOR, false);
	}

	CCSPlayerController *GetBot()
	{
		return g_replayBot.Get();
	}

	bool IsValidBot(CCSPlayerController *controller)
	{
		return controller && g_replayBot == controller;
	}

	KZPlayer *GetBotPlayer()
	{
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return nullptr;
		}
		return g_pKZPlayerManager->ToPlayer(bot);
	}

	void InitializeBotForReplay(const GeneralReplayHeader &header)
	{
		MakeBotAlive();
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
		player->SetName(header.player.name);
		if (kz_replay_playback_skins_enable.Get())
		{
			StartTimer(SetBotModel, 0.05, false);
		}
	}

	void SpectateBot(KZPlayer *spectator)
	{
		KZPlayer *botPlayer = GetBotPlayer();
		if (botPlayer && spectator)
		{
			spectator->specService->SpectatePlayer(botPlayer);
		}
	}

} // namespace KZ::replaysystem::bot
