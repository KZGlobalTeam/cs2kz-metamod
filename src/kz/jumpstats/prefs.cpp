#include "kz_jumpstats.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"

#include "utils/simplecmds.h"
#include "utils/utils.h"

bool KZJumpstatsService::GetDistTierFromString(const char *tierString, DistanceTier &outTier)
{
	if (!tierString || !V_stricmp("", tierString))
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Tier Hint");
		return false;
	}
	if (utils::IsNumeric(tierString))
	{
		DistanceTier tierValue = static_cast<DistanceTier>(V_StringToInt32(tierString, -1));
		if (tierValue < DistanceTier_None || tierValue >= DISTANCETIER_COUNT)
		{
			this->player->languageService->PrintChat(true, false, "Jumpstats Option - Tier Hint");
			return false;
		}
		outTier = tierValue;
		return true;
	}
	if (KZ_STREQI("None", tierString))
	{
		outTier = DistanceTier_None;
		return true;
	}
	if (KZ_STREQI("Meh", tierString))
	{
		outTier = DistanceTier_Meh;
		return true;
	}
	if (KZ_STREQI("Impressive", tierString))
	{
		outTier = DistanceTier_Impressive;
		return true;
	}
	if (KZ_STREQI("Perfect", tierString))
	{
		outTier = DistanceTier_Perfect;
		return true;
	}
	if (KZ_STREQI("Godlike", tierString))
	{
		outTier = DistanceTier_Godlike;
		return true;
	}
	if (KZ_STREQI("Ownage", tierString))
	{
		outTier = DistanceTier_Ownage;
		return true;
	}
	if (KZ_STREQI("Wrecker", tierString))
	{
		outTier = DistanceTier_Wrecker;
		return true;
	}
	return false;
}

void KZJumpstatsService::SetBroadcastMinTier(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->player->optionService->GetPreferenceInt("jsBroadcastMinTier", DistanceTier_Godlike))
	{
		return;
	}

	this->player->optionService->SetPreferenceInt("jsBroadcastMinTier", tier);
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Broadcast Tier - Response", tierString);
	}
}

void KZJumpstatsService::SetBroadcastSoundMinTier(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->player->optionService->GetPreferenceInt("jsBroadcastSoundMinTier", DistanceTier_Godlike))
	{
		return;
	}

	this->player->optionService->SetPreferenceInt("jsBroadcastSoundMinTier", tier);
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Broadcast Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Broadcast Tier - Response", tierString);
	}
}

void KZJumpstatsService::SetMinTier(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->player->optionService->GetPreferenceInt("jsMinTier", DistanceTier_Impressive))
	{
		return;
	}

	this->player->optionService->SetPreferenceInt("jsMinTier", tier);
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Tier - Response", tierString);
	}
}

void KZJumpstatsService::SetMinTierConsole(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->player->optionService->GetPreferenceInt("jsMinTierConsole", DistanceTier_Impressive))
	{
		return;
	}

	this->player->optionService->SetPreferenceInt("jsMinTierConsole", tier);
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Console Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Console Tier - Response", tierString);
	}
}

void KZJumpstatsService::ToggleExtendedChatStats()
{
	this->player->optionService->SetPreferenceBool("jsExtendedChatStats",
												   !this->player->optionService->GetPreferenceBool("jsExtendedChatStats", false));
	if (this->player->optionService->GetPreferenceBool("jsExtendedChatStats", false))
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Extended Chat Stats - Enable");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Extended Chat Stats - Disable");
	}
}

void KZJumpstatsService::SetJumpstatsVolume(f32 volume)
{
	this->player->optionService->SetPreferenceFloat("jsVolume", volume);
	this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Volume - Response", volume);
}

void KZJumpstatsService::SetSoundMinTier(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->player->optionService->GetPreferenceInt("jsSoundMinTier", DistanceTier_Impressive))
	{
		return;
	}

	this->player->optionService->SetPreferenceInt("jsSoundMinTier", tier);
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Sound Tier - Response", tierString);
	}
}

void KZJumpstatsService::ToggleJSAlways()
{
	this->player->optionService->SetPreferenceBool("jsAlways", !this->player->optionService->GetPreferenceBool("jsAlways", false));
	if (this->player->optionService->GetPreferenceBool("jsAlways", false))
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Always - Enable");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Always - Disable");
	}
}

SCMD(kz_jstier, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_jstierconsole, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetMinTierConsole(args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_jssound, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetSoundMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_jsbroadcast, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetBroadcastMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_jsbroadcastsound, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->SetBroadcastSoundMinTier(args->Arg(1));
	return MRES_SUPERCEDE;
}

SCMD(kz_jsalways, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleJSAlways();
	return MRES_SUPERCEDE;
}

SCMD(kz_jsextend, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->jumpstatsService->ToggleExtendedChatStats();
	return MRES_SUPERCEDE;
}

SCMD(kz_jsvolume, SCFL_JUMPSTATS | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	f32 volume = Clamp(static_cast<f32>(V_atof(args->Arg(1))), 0.0f, 1.0f);

	player->jumpstatsService->SetJumpstatsVolume(volume);
	return MRES_SUPERCEDE;
}
