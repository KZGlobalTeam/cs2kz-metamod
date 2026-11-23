#include "kz_jumpstats.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/db/kz_db.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/recording/kz_recording.h"

void KZJumpstatsService::SaveJump(Jump *jump)
{
	if (!KZDatabaseService::IsReady() || !this->player->databaseService->IsSetUp() || this->player->IsFakeClient())
	{
		return;
	}
	// Only valid jumps without styles from players who aren't banned count.
	if (jump->IsFailstat() || !jump->IsValid() || jump->GetOffset() < -JS_EPSILON || !this->player->styleServices.empty()
		|| this->player->anticheatService->isBanned)
	{
		return;
	}
	const i64 minTier = KZOptionService::GetOptionInt("jumpstatsMinSaveTier", DistanceTier_Godlike);
	if (this->player->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance()) < minTier)
	{
		return;
	}
	const i32 modeID = KZ::mode::GetModeInfo(this->player->modeService).databaseID;
	if (modeID < 0)
	{
		return;
	}
	KZDatabaseService::SaveJumpstat(this->player->GetSteamId64(), jump->GetJumpType(), modeID, jump->GetDistance(), jump->GetBlock(),
									(u32)jump->strafes.size(), jump->GetSync() * 100.0f, jump->GetTakeoffSpeed(), jump->GetMaxSpeed(), jump->airtime,
									this->player->recordingService->GetLastJumpUUID(), KZDatabaseService::OnGenericTxnSuccess);
}
