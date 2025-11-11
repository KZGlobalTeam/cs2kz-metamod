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

	if (tier == this->broadcastMinTier)
	{
		return;
	}

	this->broadcastMinTier = tier;
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

	if (tier == this->broadcastSoundMinTier)
	{
		return;
	}

	this->broadcastSoundMinTier = tier;
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

	if (tier == this->minTier)
	{
		return;
	}

	this->minTier = tier;
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

	if (tier == this->minTierConsole)
	{
		return;
	}

	this->minTierConsole = tier;
	if (tier == 0)
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Console Tier - Disabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Jumpstats Option - Jumpstats Minimum Console Tier - Response", tierString);
	}
}

void KZJumpstatsService::SetSoundMinTier(const char *tierString)
{
	DistanceTier tier;
	bool success = GetDistTierFromString(tierString, tier);
	if (!success)
	{
		return;
	}

	if (tier == this->soundMinTier)
	{
		return;
	}

	this->soundMinTier = tier;
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
	this->jsAlways = !this->jsAlways;
	if (this->jsAlways)
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
