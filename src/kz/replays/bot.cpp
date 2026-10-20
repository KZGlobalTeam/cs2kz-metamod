#include "cs2kz.h"
#include "kz/kz.h"
#include "bot.h"
#include "kz_replay.h"
#include "data.h"
#include "item.h"
#include "kz/spec/kz_spec.h"
#include "kz/timer/kz_timer.h"
#include "kz/language/kz_language.h"
#include "utils/ctimer.h"

// How long a player with a running timer gets to become able to pause before the replay gives up on moving them to spectator.
#define KZ_REPLAY_SPECTATE_TIMEOUT 2.0f

static_global CHandle<CCSPlayerController> g_replayBot;
extern CConVar<bool> kz_replay_playback_skins_enable;

static_function f64 RetrySpectateBot(CPlayerUserId userID, UUID_t replayID, f64 deadline)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	KZPlayer *botPlayer = KZ::replaysystem::bot::GetBotPlayer();
	// The player left, or the replay ended or was replaced.
	if (!player || !player->IsConnected() || !botPlayer || !(KZ::replaysystem::data::GetCurrentReplay()->uuid == replayID))
	{
		return 0.0f;
	}
	if (!player->timerService->GetTimerRunning() || player->specService->CanSpectate())
	{
		player->specService->SpectatePlayer(botPlayer, true);
		return 0.0f;
	}
	if (g_pKZUtils->GetServerGlobals()->curtime < deadline)
	{
		return ENGINE_FIXED_TICK_INTERVAL;
	}
	// CanSpectate failed on an alive, unpaused player, so CanPause prints why.
	player->timerService->CanPause(true);
	player->languageService->PrintChat(true, false, "Replay - Spectate Failed", botPlayer->GetName());
	return 0.0f;
}

static_function f64 SetBotModel()
{
	auto bot = g_replayBot.Get();
	if (!bot)
	{
		return -1.0f;
	}
	ReplayHeader &header = KZ::replaysystem::data::GetCurrentReplay()->header;
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
	EconInfo glovesInfo = {};
	if (header.has_gloves())
	{
		if (header.gloves().has_main_info())
		{
			const auto &mainInfo = header.gloves().main_info();
			glovesInfo.mainInfo.itemDef = mainInfo.item_def();
			glovesInfo.mainInfo.quality = mainInfo.quality();
			glovesInfo.mainInfo.level = mainInfo.level();
			glovesInfo.mainInfo.accountID = mainInfo.account_id();
			glovesInfo.mainInfo.itemID = mainInfo.item_id();
			glovesInfo.mainInfo.inventoryPosition = mainInfo.inventory_position();
			V_strncpy(glovesInfo.mainInfo.customName, mainInfo.custom_name().c_str(), sizeof(glovesInfo.mainInfo.customName));
			V_strncpy(glovesInfo.mainInfo.customNameOverride, mainInfo.custom_name_override().c_str(),
					  sizeof(glovesInfo.mainInfo.customNameOverride));
		}

		glovesInfo.mainInfo.numAttributes = MIN(header.gloves().attributes_size(), 32);
		for (int i = 0; i < glovesInfo.mainInfo.numAttributes; i++)
		{
			const auto &attr = header.gloves().attributes(i);
			glovesInfo.attributes[i].defIndex = attr.def_index();
			glovesInfo.attributes[i].value = attr.value();
		}
	}
	KZ::replaysystem::item::ApplyModelAttributesToPawn(player->GetPlayerPawn(), glovesInfo, header.model_name().c_str());
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
		// CS2 will kick bots that don't have spectator pending team when another player joins.
		player->GetController()->m_iPendingTeamNum(1);
		ReplayHeader &header = KZ::replaysystem::data::GetCurrentReplay()->header;

		bot->GetPlayerPawn()->m_flViewmodelOffsetX() = header.viewmodel_offset_x();
		bot->GetPlayerPawn()->m_flViewmodelOffsetY() = header.viewmodel_offset_y();
		bot->GetPlayerPawn()->m_flViewmodelOffsetZ() = header.viewmodel_offset_z();
		bot->GetPlayerPawn()->m_flViewmodelFOV() = header.viewmodel_fov();
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

	void InitializeBotForReplay(const ReplayHeader &header)
	{
		MakeBotAlive();
		auto bot = g_replayBot.Get();
		if (!bot)
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(bot);
		if (header.has_player())
		{
			player->SetName(header.player().name().c_str());
		}
		if (kz_replay_playback_skins_enable.Get())
		{
			StartTimer(SetBotModel, 0.05, false);
		}
	}

	void SpectateBot(KZPlayer *spectator)
	{
		KZPlayer *botPlayer = GetBotPlayer();
		if (!botPlayer || !spectator)
		{
			return;
		}
		// Without a running timer there is no run to protect, so the pause checks do not apply.
		if (!spectator->timerService->GetTimerRunning() || spectator->specService->CanSpectate())
		{
			spectator->specService->SpectatePlayer(botPlayer, true);
			return;
		}
		f64 deadline = g_pKZUtils->GetServerGlobals()->curtime + KZ_REPLAY_SPECTATE_TIMEOUT;
		StartTimer<CPlayerUserId, UUID_t, f64>(RetrySpectateBot, spectator->GetClient()->GetUserID(), data::GetCurrentReplay()->uuid, deadline,
											   ENGINE_FIXED_TICK_INTERVAL, false);
	}

} // namespace KZ::replaysystem::bot
