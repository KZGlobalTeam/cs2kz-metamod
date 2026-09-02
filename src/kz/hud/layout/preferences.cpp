// The per-player MHUD preferences and the class names they turn into.
#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"

#include "tier0/memdbgon.h"

const char *MHUDFontClass(KZPlayer *player, MHUDElement element)
{
	return panorama::ResolveFontClass(player->optionService->GetPreferenceStr(MHUD_ELEMENTS[(i32)element].fontKey, MHUD_DEFAULT_FONT),
									  MHUD_DEFAULT_FONT);
}

bool KZHUDService::IsMHUDElementEnabled(MHUDElement element)
{
	// An element the options menu is currently positioning is drawn regardless.
	if (this->forcedElement == (i32)element)
	{
		return true;
	}
	return this->player->optionService->GetPreferenceBool(MHUD_ELEMENTS[(i32)element].enabledKey, true);
}

bool KZHUDService::IsMHUDTimerDetailed()
{
	return this->player->optionService->GetPreferenceBool("mhudTimerDetailed", true);
}

bool KZHUDService::IsMHUDKeysOverlapEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysOverlap", true);
}

bool KZHUDService::IsMHUDKeysHidingUnpressed()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysHideUnpressed", false);
}

bool KZHUDService::IsMHUDKeysUsingLetters()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysLetters", false);
}

bool KZHUDService::IsMHUDKeysSquare()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysSquare", false);
}

bool KZHUDService::IsMHUDOutlineEnabled(MHUDElement element)
{
	return this->player->optionService->GetPreferenceBool(MHUD_ELEMENTS[(i32)element].outlineKey, true);
}

bool KZHUDService::IsUsingLayoutStyle()
{
	// The legacy fallback is structural: an unmounted addon can never select the layout.
	return KZHUDService::IsLayoutHudAvailable() && !this->player->optionService->GetPreferenceBool("hudLegacyStyle", false);
}

void KZHUDService::ToggleStyle()
{
	auto *opts = this->player->optionService;
	opts->SetPreferenceBool("hudLegacyStyle", !opts->GetPreferenceBool("hudLegacyStyle", false));
}

