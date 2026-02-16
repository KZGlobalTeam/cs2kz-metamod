#include "../kz.h"
#include "kz_anticheat.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "utils/ctimer.h"
#include "sdk/usercmd.h"

CConVar<bool> kz_ac_autokick("kz_ac_autokick", FCVAR_NONE, "Whether to kick players that are already banned", false);

CConVar<bool> kz_ac_debug("kz_ac_debug", FCVAR_NONE, "Enable anticheat debug messages", false);

CON_COMMAND_F(kz_unban, "Unban a player by their SteamID. Does not globally unban players. Only works if the server isn't globally connected.",
			  FCVAR_NONE)
{
	// TODO Anticheat: API doesn't fully support bans yet.
	// if (KZGlobalService::MayBecomeAvailable())
	// {
	// 	META_CONPRINTF("[KZ::Anticheat] Cannot unban players while connected to the global service.\n");
	// 	return;
	// }

	if (args.ArgC() != 2)
	{
		META_CONPRINTF("[KZ::Anticheat] Usage: kz_unban <SteamID64>\n");
		return;
	}
	u64 steamID = atoll(args.Arg(1));
	KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);
	if (player)
	{
		player->anticheatService->isBanned = false;
	}
	KZDatabaseService::Unban(steamID);
}

CON_COMMAND_F(kz_testban, "Test ban a player by their SteamID.", FCVAR_NONE)
{
	if (args.ArgC() != 2)
	{
		META_CONPRINTF("[KZ::Anticheat] Usage: kz_bantest <SteamID64>\n");
		return;
	}
	u64 steamID = atoll(args.Arg(1));
	KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID);
	if (player)
	{
		player->anticheatService->MarkInfraction(KZAnticheatService::Infraction::Type::Other, "Test ban via kz_testban command");
	}
}

static_global class : public KZDatabaseServiceEventListener
{
public:
	virtual void OnClientSetup(Player *player, u64 steamID64, bool isBanned)
	{
		KZPlayer *kzPlayer = g_pKZPlayerManager->ToKZPlayer(player);
		if (kzPlayer->IsFakeClient() || kzPlayer->IsCSTV())
		{
			return;
		}
		kzPlayer->anticheatService->OnClientSetup(isBanned);
	};
} databaseEventListener;

void KZAnticheatService::Init()
{
	KZDatabaseService::RegisterEventListener(&databaseEventListener);
	KZAnticheatService::InitSvCheatsWatcher();
}

void KZAnticheatService::OnPlayerFullyConnect()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	this->InitCvarMonitor();
}

void KZAnticheatService::OnSetupMove(PlayerCommand *cmd)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}

	if (kz_ac_debug.Get())
	{
		META_CONPRINTF("%s\n", cmd->DebugString().c_str());
	}
	this->currentCmdNum = cmd->cmdNum;
	this->CheckSubtickAbuse(cmd);
	// this->RecordNumJumpForCommand(cmd);
	this->CreateInputEvents(cmd);
	this->ParseCommandForJump(cmd);
}

void KZAnticheatService::OnPhysicsSimulatePost()
{
	this->CheckNulls();
	this->CheckSuspiciousSubtickCommands();
	this->CleanupOldInputEvents();
	this->CheckLandingEvents();
}

void KZAnticheatService::OnGlobalAuthFinished(BanInfo *banInfo)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	this->isBanned = banInfo != nullptr;
	// Already banned? Just add the player to the local ban database and ignore any current infraction.
	if (banInfo)
	{
		KZDatabaseService::AddOrUpdateBan(this->player->GetSteamId64(), banInfo->reason.c_str(), banInfo->expirationDate.c_str(), banInfo->banId);
		if (this->GetPendingInfraction())
		{
			this->GetPendingInfraction()->replay = nullptr; // Wipe replay to avoid saving it
			this->GetPendingInfraction()->submitted = true;
		}
		if (kz_ac_autokick.Get())
		{
			this->player->Kick("Marked as cheater in global database", NETWORK_DISCONNECT_KICKED_UNTRUSTEDACCOUNT);
		}
		else
		{
			this->isBanned = true; // Don't kick, but still mark as banned
		}
	}
	else if (this->GetPendingInfraction())
	{
		this->GetPendingInfraction()->SubmitGlobalInfraction();
	}
}

void KZAnticheatService::OnClientSetup(bool isBanned)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	if (KZGlobalService::MayBecomeAvailable())
	{
		// Wait for global auth instead.
		return;
	}
	this->isBanned = isBanned;
	// Already banned? Kick the player and ignore any current infraction.
	if (isBanned)
	{
		if (this->GetPendingInfraction())
		{
			this->GetPendingInfraction()->replay = nullptr; // Wipe replay to avoid saving it
			this->GetPendingInfraction()->submitted = true;
		}
		if (kz_ac_autokick.Get())
		{
			this->player->Kick("Marked as cheater in local database", NETWORK_DISCONNECT_KICKED_UNTRUSTEDACCOUNT);
		}
		else
		{
			this->isBanned = true; // Don't kick, but still mark as banned
		}
	}
	else if (this->GetPendingInfraction())
	{
		// Note: This will skip straight to local submission anyway.
		this->GetPendingInfraction()->SubmitGlobalInfraction();
	}
}
