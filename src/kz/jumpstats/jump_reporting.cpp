#include "kz_jumpstats.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"

#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"

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

void KZJumpstatsService::PrintJumpToChat(KZPlayer *target, Jump *jump, bool extended)
{
	if (!target || !target->IsInGame())
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();
	DistanceTier color = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	DistanceTier minTier = static_cast<DistanceTier>(
		target->optionService->GetPreferenceInt("jsMinTier", KZOptionService::GetOptionInt("defaultJSMinTier", DistanceTier_Impressive)));
	if (!target->optionService->GetPreferenceBool("jsAlways", false) && (minTier == DistanceTier_None || color < minTier))
	{
		return;
	}
	const char *jumpColor = distanceTierColors[color];
	if (jump->GetOffset() <= -JS_EPSILON || !jump->IsValid() || target->optionService->GetPreferenceBool("jsAlways", false))
	{
		jumpColor = distanceTierColors[DistanceTier_Meh];
	}

	f32 flooredDist = floor(jump->GetDistance() * 10) / 10;

	std::string releaseString = "";
	if (jump->GetJumpType() == JumpType_LongJump || jump->GetJumpType() == JumpType_LadderJump || jump->GetJumpType() == JumpType_WeirdJump)
	{
		releaseString = jump->GetReleaseString(true);
	}
	if (!extended)
	{
		// clang-format off
		target->languageService->PrintChat(true, false, "Jumpstats Report - Chat Summary (Simple)", 
			jumpColor,
			jumpTypeShortStr[jump->GetJumpType()],
			jump->GetDistance(true, false, 1),
			jump->strafes.Count(), 
			KZLanguageService::PrepareMessageWithLang(language, jump->strafes.Count() > 1 ? "Strafes" : "Strafe").c_str(),
			jump->GetSync() * 100.0f,
			jump->GetJumpPlayer()->takeoffVelocity.Length2D(),
			jump->GetMaxSpeed(),
			releaseString.c_str()
		);
		// clang-format on
		return;
	}
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
		jump->GetMaxHeight(),
		releaseString.c_str()
	);
	// clang-format on
}

void KZJumpstatsService::PrintJumpToConsole(KZPlayer *target, Jump *jump, bool broadcast)
{
	if (!target || !target->IsInGame())
	{
		return;
	}

	DistanceTier color = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	DistanceTier minTier = static_cast<DistanceTier>(
		broadcast ? target->optionService->GetPreferenceInt("jsBroadcastMinTierConsole",
															KZOptionService::GetOptionInt("defaultJSBroadcastMinTierConsole", DistanceTier_Ownage))
				  : target->optionService->GetPreferenceInt("jsMinTierConsole",
															KZOptionService::GetOptionInt("defaultJSMinTierConsole", DistanceTier_Impressive)));
	// We only print if the jump meets one of these requirements:
	// - The jump tier is equal or higher than the minimum tier set in preferences
	// - The "jsAlways" option is enabled and this is not a broadcasted jump
	// - The target is a CSTV (HLTV) client
	bool shouldPrint = minTier != DistanceTier_None && color >= minTier;
	shouldPrint |= !broadcast && target->optionService->GetPreferenceBool("jsAlways", false);
	shouldPrint |= target->IsCSTV();
	if (broadcast && !jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		shouldPrint = false;
	}
	if (!shouldPrint)
	{
		return;
	}
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
	std::string releaseString = "";
	if (jump->GetJumpType() == JumpType_LongJump || jump->GetJumpType() == JumpType_LadderJump || jump->GetJumpType() == JumpType_WeirdJump)
	{
		releaseString = jump->GetReleaseString(false);
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
		jump->GetMaxHeight(),
		releaseString.c_str()
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

void KZJumpstatsService::BroadcastJumpToChat(KZPlayer *target, Jump *jump)
{
	if (!target || !target->IsInGame())
	{
		return;
	}

	if (jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		return;
	}
	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	const char *jumpColor = distanceTierColors[tier];

	DistanceTier broadcastTier = static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
		"jsBroadcastMinTier", KZOptionService::GetOptionInt("defaultJSBroadcastMinTier", DistanceTier_Ownage)));
	bool broadcastEnabled = broadcastTier != DistanceTier_None;
	bool validBroadcastTier = tier >= broadcastTier;
	if (broadcastEnabled && validBroadcastTier)
	{
		// clang-format off
		target->languageService->PrintChat(true, false, "Broadcast Jumpstat Chat Report", 
			jump->GetJumpPlayer()->GetName(), 
			jumpColor,
			jump->GetDistance(),
			jumpTypeStr[jump->GetJumpType()],
			jump->GetJumpPlayer()->modeService->GetModeName()
		);
		KZJumpstatsService::PrintJumpToConsole(target, jump);
		KZJumpstatsService::PlayJumpstatSound(target, jump, true);
		// clang-format on
	}
}

void KZJumpstatsService::PlayJumpstatSound(KZPlayer *target, Jump *jump, bool broadcast)
{
	DistanceTier tier = jump->GetJumpPlayer()->modeService->GetDistanceTier(jump->GetJumpType(), jump->GetDistance());
	DistanceTier soundMinTier =
		broadcast ? static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
						"jsBroadcastSoundMinTier", KZOptionService::GetOptionInt("defaultJSBroadcastSoundMinTier", DistanceTier_Ownage)))
				  : static_cast<DistanceTier>(target->optionService->GetPreferenceInt(
						"jsSoundMinTier", KZOptionService::GetOptionInt("defaultJSSoundMinTier", DistanceTier_Impressive)));
	// We only print if the jump meets one of these requirements:
	// - The jump tier is equal or higher than the minimum tier set in preferences
	// - The "jsAlways" option is not enabled
	// - The target is a CSTV (HLTV) client
	bool shouldPlay = soundMinTier != DistanceTier_None && tier >= soundMinTier && tier > DistanceTier_Meh;
	shouldPlay |= target->IsCSTV();

	if (broadcast && !jump->GetJumpPlayer()->anticheatService->isBanned)
	{
		shouldPlay = false;
	}
	if (!shouldPlay || target->optionService->GetPreferenceBool("jsAlways", false))
	{
		return;
	}
	utils::PlaySoundToClient(target->GetPlayerSlot(), distanceTierSounds[tier], target->optionService->GetPreferenceFloat("jsVolume", 0.75f));
}

void KZJumpstatsService::AnnounceJump(Jump *jump)
{
	if (jump->GetJumpType() == JumpType_FullInvalid)
	{
		return;
	}
	for (i32 i = 1; i <= MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || !player->IsInGame())
		{
			continue;
		}
		// If the player is the one who did the jump or is spectating the jumper, we show more details in chat.
		if (player == jump->GetJumpPlayer() || player->specService->GetSpectatedPlayer() == jump->GetJumpPlayer())
		{
			if ((jump->GetOffset() <= -JS_EPSILON || !jump->IsValid()) && !player->optionService->GetPreferenceBool("jsAlways", false))
			{
				continue;
			}
			if (!player->optionService->GetPreferenceBool("jsReporting", true))
			{
				continue;
			}
			KZJumpstatsService::PrintJumpToChat(player, jump, player->optionService->GetPreferenceBool("jsExtendedChatStats", false));
			KZJumpstatsService::PrintJumpToConsole(player, jump);
			KZJumpstatsService::PlayJumpstatSound(player, jump);
		}
		// If the player is a CSTV bot, we only display console output.
		else if (player->IsCSTV())
		{
			// CSTV bots see all jumps.
			KZJumpstatsService::PrintJumpToConsole(player, jump);
		}
		// If the player has broadcasting enabled, we show a brief summary in chat and play a sound.
		else
		{
			if ((jump->GetOffset() <= -JS_EPSILON || !jump->IsValid()))
			{
				continue;
			}
			if (!player->optionService->GetPreferenceBool("jsReporting", true))
			{
				continue;
			}
			KZJumpstatsService::BroadcastJumpToChat(player, jump);
			KZJumpstatsService::PrintJumpToConsole(player, jump, true);
			KZJumpstatsService::PlayJumpstatSound(player, jump, true);
		}
	}
}
