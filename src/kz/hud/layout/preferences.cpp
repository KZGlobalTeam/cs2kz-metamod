// The per-player MHUD preferences and the class names they turn into.
#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"

#include "tier0/memdbgon.h"

const char *MHUDFontClass(KZPlayer *player, MHUDElement element)
{
	return panorama::ResolveFontClass(player->optionService->GetPreferenceStr(MHUD_ELEMENTS[(i32)element].fontKey, MHUD_DEFAULT_FONT), MHUD_DEFAULT_FONT);
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

void MHUDResetElementPrefs(KZPlayer *p, MHUDElement element)
{
	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)element];
	auto *opts = p->optionService;
	opts->SetPreferenceBool(def.enabledKey, true);
	opts->SetPreferenceFloat(def.xKey, def.xDefault);
	opts->SetPreferenceFloat(def.yKey, def.yDefault);
	opts->SetPreferenceFloat(def.sizeKey, def.sizeDefault);
	opts->SetPreferenceStr(def.fontKey, MHUD_DEFAULT_FONT);
	opts->SetPreferenceBool(def.outlineKey, true);

	i32 count = 0;
	const MHUDColorPrefDef *colors = MHUDElementColorPrefs(element, count);
	for (i32 i = 0; i < count; i++)
	{
		opts->SetPreferenceColor(colors[i].prefKey, Color(colors[i].r, colors[i].g, colors[i].b, 255));
	}

	switch (element)
	{
		case MHUDElement::Timer:
			opts->SetPreferenceBool("mhudTimerDetailed", true);
			break;
		case MHUDElement::Keys:
			opts->SetPreferenceBool("mhudKeysOverlap", true);
			opts->SetPreferenceBool("mhudKeysHideUnpressed", false);
			break;
		default:
			break;
	}
}
