#include "common.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "kz/kz.h"
#include "utils/simplecmds.h"

#include "kz/mode/kz_mode.h"
#include "kz/hud/kz_hud.h"
#include "kz/jumpstats/kz_jumpstats.h"

#include "sdk/gamerules.h"

#define RESTART_CHECK_INTERVAL 1800.0f
static_global CTimer<> *mapRestartTimer;

static_function f64 CheckRestart()
{
	utils::ResetMapIfEmpty();
	return RESTART_CHECK_INTERVAL;
}

void KZ::misc::Init()
{
	//KZOptionService::RegisterEventListener(&optionEventListener);
	mapRestartTimer = StartTimer(CheckRestart, RESTART_CHECK_INTERVAL, true, false);
}

void KZ::misc::OnServerActivate()
{
	static_persist bool cvTweaked {};
	if (!cvTweaked)
	{
		cvTweaked = true;
		auto cvarHandle = g_pCVar->FindConVar("sv_infinite_ammo");
		if (cvarHandle.IsValid())
		{
			g_pCVar->GetConVar(cvarHandle)->flags &= ~FCVAR_CHEAT;
		}
		else
		{
			META_CONPRINTF("Warning: sv_infinite_ammo is not found!\n");
		}
		cvarHandle = g_pCVar->FindConVar("bot_stop");
		if (cvarHandle.IsValid())
		{
			g_pCVar->GetConVar(cvarHandle)->flags &= ~FCVAR_CHEAT;
		}
		else
		{
			META_CONPRINTF("Warning: bot_stop is not found!\n");
		}
	}
	g_pKZUtils->UpdateCurrentMapMD5();

	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
	KZ::misc::InitTimeLimit();

	// Restart round to ensure settings (e.g. mp_weapons_allow_map_placed) are applied
	interfaces::pEngine->ServerCommand("mp_restartgame 1");
}

// TODO: move command registration to the service class?
void KZ::misc::RegisterCommands()
{
	KZJumpstatsService::RegisterCommands();
	KZHUDService::RegisterCommands();
	KZ::mode::RegisterCommands();
}

static_function void SanitizeMsg(const char *input, char *output, u32 size)
{
	int x = 0;
	for (int i = 0; input[i] != '\0'; i++)
	{
		if (x + 1 == size)
		{
			break;
		}

		int character = input[i];

		if (character > 0x10)
		{
			if (character == '\\')
			{
				output[x++] = character;
			}
			output[x++] = character;
		}
	}

	output[x] = '\0';
}

void KZ::misc::ProcessConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args)
{
	if (!GameEntitySystem())
	{
		return;
	}
	CPlayerSlot slot = ctx.GetPlayerSlot();

	CCSPlayerController *controller = (CCSPlayerController *)utils::GetController(slot);

	KZPlayer *player = NULL;

	if (!cmd.IsValid() || !controller || !(player = g_pKZPlayerManager->ToPlayer(controller)))
	{
		return;
	}
	const char *commandName = g_pCVar->GetCommand(cmd)->GetName();

	// Is it a chat message?
	if (!V_stricmp(commandName, "say") || !V_stricmp(commandName, "say_team"))
	{
		if (args.ArgC() < 2)
		{
			// no argument somehow
			return;
		}

		i32 argLen = strlen(args[1]);
		if (argLen < 1 || args[1][0] == SCMD_CHAT_SILENT_TRIGGER)
		{
			// arg is too short!
			return;
		}

		CUtlString message;
		for (int i = 1; i < args.ArgC(); i++)
		{
			if (i > 1)
			{
				message += ' ';
			}
			message += args[i];
		}

		if (player->IsAlive())
		{
			utils::SayChat(player->GetController(), "{lime}%s{default}: %s", player->GetName(), message.Get());
			utils::PrintConsoleAll("%s: %s", player->GetName(), message.Get());
			META_CONPRINTF("%s: %s\n", player->GetName(), message.Get());
		}
		else
		{
			utils::SayChat(player->GetController(), "{grey}* {lime}%s{default}: %s", player->GetName(), message.Get());
			utils::PrintConsoleAll("* %s: %s", player->GetName(), message.Get());
			META_CONPRINTF("* %s: %s\n", player->GetName(), message.Get());
		}
		return;
	}

	return;
}

void KZ::misc::OnRoundStart()
{
	CCSGameRules *gameRules = g_pKZUtils->GetGameRules();
	if (gameRules)
	{
		gameRules->m_bGameRestart(true);
		gameRules->m_iRoundWinStatus(1);
		// Make sure that the round time is synchronized with the global time.
		gameRules->m_fRoundStartTime().SetTime(0.0f);
		gameRules->m_flGameStartTime().SetTime(0.0f);
	}
}
