#include "../kz.h"
#include "kz_anticheat.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/global/kz_global.h"
#include "kz/db/kz_db.h"
#include "utils/ctimer.h"

CConVar<bool> kz_ac_autoban("kz_ac_autoban", FCVAR_NONE, "Whether to autoban players when they are suspected of cheating.", true);

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
		player->anticheatService->MarkDetection("Test Ban", true, 0.0f, true);
	}
}

static_global class : public KZDatabaseServiceEventListener
{
public:
	virtual void OnClientSetup(Player *player, u64 steamID64, bool isBanned)
	{
		KZPlayer *kzPlayer = g_pKZPlayerManager->ToKZPlayer(player);
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
	this->InitCvarMonitor();
}

void KZAnticheatService::OnAuthorized()
{
	if (this->isBanned)
	{
		UUID_t cheaterReplayUUID = this->cheaterRecorder ? this->cheaterRecorder->uuid : UUID_t(false);
		if (this->cheaterRecorder.get())
		{
			std::string reason = this->cheatReason;
			std::string name = this->player->GetName();
			u64 steamID = this->player->GetSteamId64();
			this->cheaterRecorder->replayHeader.mutable_player()->set_steamid64(this->player->GetSteamId64());
			// clang-format off
			KZRecordingService::fileWriter->QueueWrite(
				std::move(this->cheaterRecorder),
				// Success callback
				[reason, name, steamID](const UUID_t &uuid, f32 replayDuration)
				{
					META_CONPRINTF("[KZ::Anticheat] Cheater replay saved for player %s (%llu) - Reason: %s\n", name.c_str(), steamID, reason.c_str());
				},
				// Failure callback
				[name, steamID](const char *error)
				{
					META_CONPRINTF("[KZ::Anticheat] Failed to save cheater replay for player %s (%llu) - Error: %s\n", name.c_str(), steamID, error);
				});
			// clang-format on
		}
		// We need to pass the uuid to the global service because the replay will be gone by the time the global service tries to access it.
		this->player->globalService->SubmitBan(this->cheatReason, cheaterReplayUUID);
		this->player->databaseService->Ban(this->cheatReason.c_str(), 0.0f, UUID_t(false), cheaterReplayUUID);
		if (kz_ac_autoban.Get())
		{
			this->player->Kick("Marked as cheater", NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT);
		}
	}
}

void KZAnticheatService::OnGlobalAuthFinished(bool isBanned, const UUID_t *banId, std::string reason)
{
	this->isBanned = isBanned;
	if (isBanned)
	{
		this->player->databaseService->Ban(reason.c_str(), "'9999-12-31 23:59:59'");
		if (kz_ac_autoban.Get())
		{
			this->player->Kick("Marked as cheater in global database", NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT);
		}
	}
}

void KZAnticheatService::OnClientSetup(bool isBanned)
{
	if (KZGlobalService::MayBecomeAvailable())
	{
		return;
	}
	this->isBanned = isBanned;
	if (isBanned)
	{
		if (kz_ac_autoban.Get())
		{
			this->player->Kick("Marked as cheater in local database", NETWORK_DISCONNECT_KICKED_UNTRUSTEDACCOUNT);
		}
	}
}

void KZAnticheatService::MarkDetection(std::string reason, bool generateReplay, f32 banDuration, bool kick)
{
	if (this->isBanned)
	{
		return;
	}
	this->isBanned = true;
	this->cheatReason = reason;
	if (generateReplay)
	{
		if (!this->cheaterRecorder)
		{
			this->cheaterRecorder = std::make_unique<CheaterRecorder>(this->player, reason.c_str(), nullptr);
		}
	}
	if (this->player->IsAuthenticated())
	{
		std::string name = this->player->GetName();
		u64 steamID = this->player->GetSteamId64();
		UUID_t cheaterReplayUUID = this->cheaterRecorder ? this->cheaterRecorder->uuid : UUID_t(false);
		// clang-format off
		KZRecordingService::fileWriter->QueueWrite(
			std::move(this->cheaterRecorder),
			// Success callback
			[reason, name, steamID](const UUID_t &uuid, f32 replayDuration)
			{
				META_CONPRINTF("[KZ::Anticheat] Cheater replay %s saved for player %s (%llu) - Reason: %s\n", uuid.ToString().c_str(), name.c_str(), steamID, reason.c_str());
			},
			// Failure callback
			[name, steamID](const char *error)
			{
				META_CONPRINTF("[KZ::Anticheat] Failed to save cheater replay for player %s (%llu) - Error: %s\n", name.c_str(), steamID, error);
			});
		// clang-format on
		this->player->globalService->SubmitBan(this->cheatReason, cheaterReplayUUID);
		this->player->databaseService->Ban(this->cheatReason.c_str(), banDuration, UUID_t(false), cheaterReplayUUID);
	}
	if (kz_ac_autoban.Get())
	{
		this->player->Kick("Marked as cheater in local database", NETWORK_DISCONNECT_KICKED_CONVICTEDACCOUNT);
	}
}
