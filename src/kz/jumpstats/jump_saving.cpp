#include "kz_jumpstats.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/option/kz_option.h"
#include "kz/recording/kz_recording.h"
#include "vendor/sql_mm/src/public/sql_mm.h"

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
	const DistanceTier tier = this->player->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	if (tier < minTier)
	{
		return;
	}
	const i32 modeID = KZ::mode::GetModeInfo(this->player->modeService).databaseID;
	if (modeID < 0)
	{
		return;
	}
	// Compared as the stored integers so a jump equal to the saved best doesn't count.
	const i64 distance = (i64)(jump->GetDistance() * KZ_JUMPSTATS_DISTANCE_SCALE + 0.5);
	const i64 block = (i64)(jump->GetBlock() + 0.5f);
	const i32 jumpType = jump->GetJumpType();
	const UUID_t uuid = this->player->recordingService->GetLastJumpUUID();
	const CPlayerUserId userID = this->player->GetClient()->GetUserID();
	const char *color = distanceTierColors[tier];
	const std::string mode = KZ::mode::GetModeInfo(this->player->modeService).shortModeName;
	auto onSuccess = [userID, jumpType, distance, block, uuid, color, mode](std::vector<ISQLQuery *> queries)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		// The first saved jump of a type isn't announced.
		ISQLResult *result = queries[0]->GetResultSet();
		const bool best = result && result->FetchRow() && result->GetInt64(0) < distance;
		result = queries[1]->GetResultSet();
		const bool blockBest = block > 0 && result && result->FetchRow()
							   && (result->GetInt64(0) < block || (result->GetInt64(0) == block && result->GetInt64(1) < distance));
		const char *type = jumpTypeStr[jumpType];
		const f64 units = distance / KZ_JUMPSTATS_DISTANCE_SCALE;
		if (best)
		{
			player->languageService->PrintChat(true, false, "Jumpstats - Personal Best", type, color, units, mode.c_str());
		}
		if (blockBest)
		{
			player->languageService->PrintChat(true, false, "Jumpstats - Personal Best Block", type, color, units, (i32)block, mode.c_str());
		}
		if (best || blockBest)
		{
			player->recordingService->AnnounceJumpReplay(uuid);
		}
	};
	KZDatabaseService::SaveJumpstat(this->player->GetSteamId64(), jumpType, modeID, jump->GetDistance(), jump->GetBlock(), (u32)jump->strafes.size(),
									jump->GetSync() * 100.0f, jump->GetTakeoffSpeed(), jump->GetMaxSpeed(), jump->airtime, uuid, onSuccess);
}
