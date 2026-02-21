#include "kz_anticheat.h"
#include "kz/global/kz_global.h"
#include "kz/db/kz_db.h"

KZAnticheatService::Infraction *KZAnticheatService::GetPendingInfraction(const UUID_t &infractionId)
{
	for (KZAnticheatService::Infraction &infraction : KZAnticheatService::pendingBans)
	{
		if (infraction.id == infractionId && !infraction.submitted)
		{
			return &infraction;
		}
	}
	return nullptr;
}

KZAnticheatService::Infraction *KZAnticheatService::GetPendingInfraction()
{
	for (KZAnticheatService::Infraction &infraction : KZAnticheatService::pendingBans)
	{
		if (infraction.steamID == this->player->GetSteamId64() && !infraction.submitted)
		{
			return &infraction;
		}
	}
	return nullptr;
}

void KZAnticheatService::CleanupInfractions()
{
	for (auto &infraction : KZAnticheatService::pendingBans)
	{
		if (infraction.submitted && infraction.replay == nullptr)
		{
			infraction.Finalize();
		}
	}
	auto &infractions = KZAnticheatService::pendingBans;
	// clang-format off
	infractions.erase(std::remove_if(infractions.begin(), infractions.end(),
		[](const KZAnticheatService::Infraction &infraction)
		{
			return infraction.submitted && infraction.replay == nullptr;
		}), infractions.end());
	// clang-format on
}

void KZAnticheatService::MarkInfraction(Infraction::Type type, const std::string &details)
{
	if (!this->ShouldRunDetections())
	{
		return;
	}
	if (!this->player->IsAuthenticated())
	{
		// Cannot mark infraction for unauthenticated players, so we just kick them.
		this->player->Kick("Invalid Steam Logon", NETWORK_DISCONNECT_STEAM_LOGON);
		return;
	}

	if (this->isBanned)
	{
		return;
	}
	this->isBanned = true;

	Infraction &infraction = KZAnticheatService::pendingBans.emplace_back();
	infraction.type = type;
	infraction.details = details;
	infraction.steamID = this->player->GetSteamId64();

	if (Infraction::shouldGenerateReplay[static_cast<u8>(type)])
	{
		infraction.replay = std::make_unique<CheaterRecorder>(this->player, details.c_str(), nullptr);
		this->player->recordingService->CopyWeaponsToRecorder(infraction.replay.get());
		infraction.replayUUID = infraction.replay->uuid;
	}
	infraction.SubmitGlobalInfraction();
}

void KZAnticheatService::Infraction::SubmitGlobalInfraction()
{
	// TODO Anticheat: Make API call to submit global infraction
	this->OnGlobalSubmitFailure();
}

void KZAnticheatService::Infraction::OnGlobalSubmitSuccess(const UUID_t &infractionId, const UUID_t &replayUUID, f32 banDuration)
{
	// Update infraction info with data from API.
	this->id = infractionId;
	this->banDuration = banDuration;
	this->replayUUID = replayUUID;
	if (this->replay)
	{
		this->replay.get()->uuid = replayUUID;
	}
	this->SubmitLocalInfraction();
	this->SaveReplay();
}

void KZAnticheatService::Infraction::OnGlobalSubmitFailure()
{
	this->banDuration = KZAnticheatService::Infraction::banDurations[(u8)this->type];
	this->SubmitLocalInfraction();
	this->SaveReplay();
}

void KZAnticheatService::Infraction::SubmitLocalInfraction()
{
	if (this->banDuration < 0.0f || !KZDatabaseService::IsReady())
	{
		this->Finalize();
		return;
	}
	auto &id = this->id;
	auto onSuccess = [id](std::vector<ISQLQuery *> queries)
	{
		META_CONPRINTF("[KZ::Anticheat] Saved local infraction %s\n", id.ToString().c_str());
		if (KZAnticheatService::Infraction *infraction = KZAnticheatService::GetPendingInfraction(id))
		{
			infraction->Finalize();
		}
	};
	auto onFailure = [id](std::string error, int failIndex)
	{
		META_CONPRINTF("[KZ::Anticheat] Failed to record local infraction %s: %s\n", id.ToString().c_str(), error.c_str());
		if (KZAnticheatService::Infraction *infraction = KZAnticheatService::GetPendingInfraction(id))
		{
			infraction->Finalize();
		}
	};
	KZDatabaseService::Ban(this->steamID, this->details.c_str(), this->banDuration, this->id, this->replayUUID, onSuccess, onFailure);
}

void KZAnticheatService::Infraction::SaveReplay()
{
	if (this->replay)
	{
		// Queue replay for saving
		std::string name = this->replay.get()->replayHeader.player().name();
		u64 steamID = this->steamID;
		// clang-format off
		KZRecordingService::fileWriter->QueueWrite(
			std::move(this->replay),
			// Success callback
			[name, steamID](const UUID_t &uuid, f32 replayDuration)
			{
				META_CONPRINTF("[KZ::Anticheat] Cheater replay saved for player %s (%llu)\n", name.c_str(), steamID);
				// TODO Anticheat: Add UUID to the global upload queue
			},
			// Failure callback
			[name, steamID](const char *error)
			{
				META_CONPRINTF("[KZ::Anticheat] Failed to save cheater replay for player %s (%llu) - Error: %s\n", name.c_str(), steamID, error);
			});
		// clang-format on
	}
}

void KZAnticheatService::Infraction::Finalize()
{
	this->submitted = true;
	// Kick the player if not already gone
	KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(this->steamID);
	if (!player)
	{
		return;
	}

	player->Kick(this->kickInternalReasons[static_cast<u8>(this->type)], Infraction::kickReasons[static_cast<u8>(this->type)]);
};
