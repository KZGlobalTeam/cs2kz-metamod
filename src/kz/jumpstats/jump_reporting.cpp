#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"

#include "kz_jumpstats.h"
#include "../mode/kz_mode.h"
#include "../style/kz_style.h"
#include "../option/kz_option.h"
#include "../language/kz_language.h"

static_global const char *columnKeys[] = {"#.",
										  "Sync",
										  "Gain",
										  "",
										  "Loss",
										  "",
										  "Max",
										  "Air Time",
										  "Bad Angles (Short)",
										  "Overlap (Short)",
										  "Dead Air (Short)",
										  "Average Gain (Short)",
										  "Gain Efficiency (Short)",
										  "Angle Ratio"};

std::string Jump::GetInvalidationReasonString(const char *reason, const char *language)
{
	if (!reason || reason[0] == '\0')
	{
		return "";
	}
	const char *lang = language ? language : this->GetJumpPlayer()->languageService->GetLanguage();
	std::string reasonText = KZLanguageService::PrepareMessageWithLang(lang, reason);
	return std::string(KZLanguageService::PrepareMessageWithLang(lang, "Jumpstats Report - Invalidation Reason", reasonText.c_str()));
}

void KZJumpstatsService::PrintJumpToChat(KZPlayer *target, Jump *jump)
{
	const char *language = target->languageService->GetLanguage();
	DistanceTier color = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	const char *jumpColor = distanceTierColors[color];
	if (jump->GetOffset() <= -JS_EPSILON || !jump->IsValid())
	{
		jumpColor = distanceTierColors[DistanceTier_Meh];
	}

	f32 flooredDist = floor(jump->GetDistance() * 10) / 10;

	// clang-format off
	target->languageService->PrintChat(true, false, "Jumpstats Report - Chat Summary", 
		jumpColor,
		jumpTypeShortStr[jump->GetJumpType()],
		jump->GetDistance(true, false, 1),
		jump->strafes.Count(), 
		KZLanguageService::PrepareMessageWithLang(language, jump->strafes.Count() > 1 ? "Strafes" : "Strafe").c_str(),
		jump->GetSync() * 100.0f,
		jump->GetJumpPlayer()->takeoffVelocity.Length2D(),
		jump->GetMaxSpeed(),
		jump->GetBadAngles() * 100,
		jump->GetOverlap() * 100,
		jump->GetDeadAir() * 100,
		jump->GetDeviation(),
		jump->GetWidth(),
		jump->GetMaxHeight());
	// clang-format on
}

void KZJumpstatsService::PrintJumpToConsole(KZPlayer *target, Jump *jump)
{
	const char *language = target->languageService->GetLanguage();
	// clang-format off
	target->languageService->PrintConsole(false, false, "Jumpstats Report - Console Summary",
		jump->GetJumpPlayer()->GetName(),
		jump->GetDistance(),
		jumpTypeStr[jump->GetJumpType()],
		jump->GetInvalidationReasonString(jump->invalidateReason)
	);
	std::string modeStyleNames = jump->GetJumpPlayer()->modeService->GetModeShortName();
	FOR_EACH_VEC(jump->GetJumpPlayer()->styleServices, i)
	{
		modeStyleNames += " +";
		modeStyleNames += jump->GetJumpPlayer()->styleServices[i]->GetStyleShortName();
	}
	target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Details 1",
		modeStyleNames.c_str(),
		jump->strafes.Count(),
		KZLanguageService::PrepareMessageWithLang(language, jump->strafes.Count() > 1 ? "Strafes" : "Strafe").c_str(),
		jump->GetSync() * 100.0f,
		jump->GetTakeoffSpeed(),
		jump->GetMaxSpeed(),
		jump->GetBadAngles() * 100.0f,
		jump->GetOverlap() * 100.0f,
		jump->GetDeadAir() * 100.0f,
		jump->GetMaxHeight()
	);

	target->languageService->PrintConsole(false, false, "Jumpstat Report - Console Details 2",
		jump->GetGainEfficiency() * 100.0f,
		jump->GetAirPath(),
		jump->GetDeviation(),
		jump->GetWidth(),
		jump->GetJumpPlayer()->landingTimeActual - jump->GetJumpPlayer()->takeoffTime,
		jump->GetOffset(),
		jump->GetDuckTime(true),
		jump->GetDuckTime(false)
	);
	
	CUtlString headers[KZ_ARRAYSIZE(columnKeys)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeys); i++)
	{
		headers[i] = target->languageService->PrepareMessage(columnKeys[i]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(columnKeys)> table("", headers);
	
	FOR_EACH_VEC(jump->strafes, i)
	{
		char strafeNumberString[5];
		char syncString[16], gainString[16], lossString[16], externalGainString[16], externalLossString[16], maxString[16], durationString[16];
		char badAngleString[16], overlapString[16], deadAirString[16], avgGainString[16], gainEffString[16];
		char angRatioString[32];
		V_snprintf(strafeNumberString, sizeof(strafeNumberString), "%i.", i+1);
		V_snprintf(syncString, sizeof(syncString), "%.0f%%%%", jump->strafes[i].GetSync() * 100.0f);
		V_snprintf(gainString, sizeof(gainString), "%.2f", jump->strafes[i].GetGain());
		V_snprintf(externalGainString, sizeof(externalGainString), "(+%.2f)", fabs(jump->strafes[i].GetGain(true)));
		V_snprintf(lossString, sizeof(lossString), "-%.2f", fabs(jump->strafes[i].GetLoss()));
		V_snprintf(externalLossString, sizeof(externalLossString), "(-%.2f)", fabs(jump->strafes[i].GetLoss(true)));
		V_snprintf(maxString, sizeof(maxString), "%.2f", jump->strafes[i].GetStrafeMaxSpeed());
		V_snprintf(durationString, sizeof(durationString), "%.3f", jump->strafes[i].GetStrafeDuration());
		V_snprintf(badAngleString, sizeof(badAngleString), "%.1f", jump->strafes[i].GetBadAngleDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(overlapString, sizeof(overlapString), "%.1f", jump->strafes[i].GetOverlapDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(deadAirString, sizeof(deadAirString), "%.1f", jump->strafes[i].GetDeadAirDuration() * ENGINE_FIXED_TICK_RATE);
		V_snprintf(avgGainString, sizeof(avgGainString), "%.2f", jump->strafes[i].GetGain() / jump->strafes[i].GetStrafeDuration() * ENGINE_FIXED_TICK_INTERVAL);
		V_snprintf(gainEffString, sizeof(gainEffString), "%.0f%%%%", jump->strafes[i].GetGain() / jump->strafes[i].GetMaxGain() * 100.0f);

		if (jump->strafes[i].arStats.available)
		{
			V_snprintf(angRatioString, sizeof(angRatioString),
				"%.2f/%.2f/%.2f",
				jump->strafes[i].arStats.average,
				jump->strafes[i].arStats.median,
				jump->strafes[i].arStats.max
			);
		}
		else
		{
			V_snprintf(angRatioString, sizeof(angRatioString), "N/A");
		}

		table.SetRow(i,
			strafeNumberString,
			syncString,
			gainString,
			externalGainString,
			lossString,
			externalLossString,
			maxString,
			durationString,
			badAngleString,
			overlapString,
			deadAirString,
			avgGainString,
			gainEffString,
			angRatioString
		);
	}
	// clang-format on

	if (jump->strafes.Count() > 0)
	{
		target->PrintConsole(false, false, table.GetHeader());
		for (u32 i = 0; i < table.GetNumEntries(); i++)
		{
			target->PrintConsole(false, false, table.GetLine(i));
		}
	}
}

void KZJumpstatsService::BroadcastJumpToChat(Jump *jump)
{
	if (jump->GetOffset() <= -JS_EPSILON || !jump->IsValid())
	{
		return;
	}

	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	const char *jumpColor = distanceTierColors[tier];

	for (i32 i = 0; i <= MAXPLAYERS; i++)
	{
		CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(i)));
		if (ent)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
			if (player == jump->GetJumpPlayer())
			{
				// Do not broadcast to self.
				continue;
			}
			bool broadcastEnabled = player->jumpstatsService->GetBroadcastMinTier() != DistanceTier_None;
			bool validBroadcastTier = tier >= player->jumpstatsService->GetBroadcastMinTier();
			if (broadcastEnabled && validBroadcastTier)
			{
				player->languageService->PrintChat(true, false, "Broadcast Jumpstat Chat Report", jump->GetJumpPlayer()->GetName(), jumpColor,
												   jump->GetDistance(), jumpTypeStr[jump->GetJumpType()],
												   jump->GetJumpPlayer()->modeService->GetModeName());
			}
		}
	}
}

void KZJumpstatsService::PlayJumpstatSound(KZPlayer *target, Jump *jump)
{
	if (jump->GetOffset() <= -JS_EPSILON || !jump->IsValid())
	{
		return;
	}

	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	if (target->jumpstatsService->GetSoundMinTier() > tier || tier <= DistanceTier_Meh
		|| target->jumpstatsService->GetSoundMinTier() == DistanceTier_None)
	{
		return;
	}

	utils::PlaySoundToClient(target->GetPlayerSlot(), distanceTierSounds[tier], 0.5f);
}
