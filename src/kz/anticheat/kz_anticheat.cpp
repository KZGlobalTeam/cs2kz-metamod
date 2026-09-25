#include "kz/kz.h"
#include "kz_anticheat.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "utils/ctimer.h"
#include "sdk/usercmd.h"

IMPLEMENT_CLASS_EVENT_LISTENER(KZAnticheatService, KZAnticheatServiceEventListener);

CConVar<i32> kz_ac_autokick("kz_ac_autokick", FCVAR_NONE,
							"How to handle players that are already banned. 0 = never kick, 1 = kick on join, 2 = let them play, but kick "
							"and renew the ban if they get detected again",
							static_cast<i32>(KZAnticheatService::AutokickMode::OnJoin), true,
							static_cast<i32>(KZAnticheatService::AutokickMode::Never), true,
							static_cast<i32>(KZAnticheatService::AutokickMode::OnReoffense));

KZAnticheatService::AutokickMode KZAnticheatService::GetAutokickMode()
{
	return static_cast<AutokickMode>(kz_ac_autokick.Get());
}

void KZAnticheatService::MarkBanned(KZAnticheatBanSource source, const char *reason)
{
	if (this->isBanned)
	{
		return;
	}
	this->isBanned = true;
	this->banSource = source;
	CALL_FORWARD(eventListeners, OnPlayerBannedPost, this->player, source, reason ? reason : "");
}

bool KZAnticheatService::CanReceiveInfraction() const
{
	if (!this->isBanned)
	{
		return true;
	}
	return this->banSource != KZAnticheatBanSource::Detection && GetAutokickMode() == AutokickMode::OnReoffense;
}

CON_COMMAND_F(kz_unban, "Unban a player by their SteamID. Does not globally unban players. Only works if the server isn't globally connected.",
			  FCVAR_NONE)
{
	// TODO Anticheat: API doesn't fully support bans yet.
	// if (KZGlobalService::MayBecomeAvailable())
	// {
	// 	KZ_LOG_INFO(LogChannel::AC, "Cannot unban players while connected to the global service.\n");
	// 	return;
	// }

	if (args.ArgC() != 2)
	{
		KZ_LOG_INFO(LogChannel::AC, "Usage: kz_unban <SteamID64>\n");
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

static_global class : public KZDatabaseServiceEventListener
{
public:
	virtual void OnClientSetup(Player *player, u64 steamID64, bool isBanned, const char *banReason)
	{
		KZPlayer *kzPlayer = g_pKZPlayerManager->ToKZPlayer(player);
		if (kzPlayer->IsFakeClient() || kzPlayer->IsCSTV())
		{
			return;
		}
		kzPlayer->anticheatService->OnClientSetup(isBanned, banReason);
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
	// Running detections on players that can't receive another infraction only spams the logs.
	if (!this->ShouldRunDetections() || !this->CanReceiveInfraction())
	{
		this->ClearDetectionBuffers();
		return;
	}

	this->currentCmdNum = cmd->cmdNum;
	this->CheckSubtickAbuse(cmd);
	this->CreateInputEvents(cmd);
	this->ParseCommandForJump(cmd);
	this->DetectOptimization(cmd);
}

void KZAnticheatService::OnPhysicsSimulatePost()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	if (!this->ShouldRunDetections() || !this->CanReceiveInfraction())
	{
		this->ClearDetectionBuffers();
		return;
	}
	this->CheckNulls();
	this->CheckSuspiciousSubtickCommands();
	this->CleanupOldInputEvents();
	this->CheckLandingEvents();
}

void KZAnticheatService::ClearDetectionBuffers()
{
	this->recentForwardBackwardEvents.clear();
	this->recentLeftRightEvents.clear();
	this->suspiciousSubtickMoveTimes.clear();
	this->invalidCommandTimes.clear();
	this->zeroWhenCommandTimes.clear();
	this->numCommandsWithSubtickInputs.clear();
	this->recentJumpStatuses.clear();
	this->recentJumps.clear();
	this->recentLandingEvents.clear();
	this->currentAirTime = 0.0f;
	this->airMovedThisFrame = false;
	this->lastValidMoveTypeTime = -1.0f;
	this->angleFrameHistory.clear();
	this->yawAccelPercent = 0.0f;
}

void KZAnticheatService::OnGlobalAuthFinished(BanInfo *banInfo)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// Already banned? Just add the player to the local ban database and ignore any current infraction.
	if (banInfo)
	{
		this->MarkBanned(KZAnticheatBanSource::GlobalDatabase, banInfo->reason.c_str());
		KZDatabaseService::AddOrUpdateBan(this->player->GetSteamId64(), banInfo->reason.c_str(), banInfo->expirationDate.c_str(), banInfo->banId);
		if (this->GetPendingInfraction())
		{
			this->GetPendingInfraction()->replay = nullptr; // Wipe replay to avoid saving it
			this->GetPendingInfraction()->submitted = true;
		}
		if (GetAutokickMode() == AutokickMode::OnJoin)
		{
			this->player->Kick("Marked as cheater in global database", NETWORK_DISCONNECT_KICKED_UNTRUSTEDACCOUNT);
		}
		else
		{
			// Don't kick, but still mark as banned.
			this->PrintCheaterMessage();
		}
		return;
	}
	this->isBanned = false;
	if (this->GetPendingInfraction())
	{
		KZDatabaseService::Unban(this->player->GetSteamId64());
		this->GetPendingInfraction()->SubmitGlobalInfraction();
	}
	else
	{
		KZDatabaseService::Unban(this->player->GetSteamId64());
	}
}

void KZAnticheatService::OnClientSetup(bool isBanned, const char *banReason)
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	// if (KZGlobalService::MayBecomeAvailable())
	// {
	// 	// Wait for global auth instead.
	// 	return;
	// }
	// Already banned? Kick the player and ignore any current infraction.
	if (isBanned)
	{
		this->MarkBanned(KZAnticheatBanSource::LocalDatabase, "");
		// A detection that beat the ban lookup renews the ban and kicks the player.
		if (this->GetPendingInfraction() && GetAutokickMode() != AutokickMode::OnReoffense)
		{
			this->GetPendingInfraction()->replay = nullptr; // Wipe replay to avoid saving it
			this->GetPendingInfraction()->submitted = true;
		}
		if (GetAutokickMode() == AutokickMode::OnJoin)
		{
			this->player->Kick("Marked as cheater in local database", Infraction::GetRejoinKickReason(banReason));
		}
		else
		{
			// Don't kick, but still mark as banned.
			this->PrintCheaterMessage();
		}
		return;
	}
	this->isBanned = false;
	if (this->GetPendingInfraction())
	{
		// Note: This will skip straight to local submission anyway.
		this->GetPendingInfraction()->SubmitGlobalInfraction();
	}
}

f64 KZAnticheatService::PrintWarning(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (player && !player->anticheatService->printedCheaterMessage)
	{
		player->anticheatService->canPrintCheaterMessage = true;
		if (!player->anticheatService->isBanned)
		{
			player->languageService->PrintChat(false, false, "Anti-Cheat Warning");
		}
		else
		{
			player->anticheatService->PrintCheaterMessage();
		}
	}
	return 0.0f;
}

void KZAnticheatService::PrintCheaterMessage()
{
	if (this->isBanned && this->canPrintCheaterMessage && !this->printedCheaterMessage)
	{
		this->player->languageService->PrintChat(true, false, "Cheater Warning");
		this->printedCheaterMessage = true;
	}
}
