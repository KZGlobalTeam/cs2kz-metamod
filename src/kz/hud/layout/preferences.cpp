// The per-player MHUD preferences and the class names they turn into.
#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"

#include "tier0/memdbgon.h"

void KZHUDService::RefreshPrefs()
{
	auto *opts = this->player->optionService;
	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		const MHUDElementDef &def = MHUD_ELEMENTS[e];
		MHUDPrefs::Element &element = this->prefs.elements[e];
		element.enabled = opts->GetPreferenceBool(def.enabledKey, true);
		element.x = (f32)opts->GetPreferenceFloat(def.xKey, def.xDefault);
		element.y = (f32)opts->GetPreferenceFloat(def.yKey, def.yDefault);
		element.size = (f32)opts->GetPreferenceFloat(def.sizeKey, def.sizeDefault);
		element.fontClass = panorama::ResolveFontClass(opts->GetPreferenceStr(def.fontKey, MHUD_DEFAULT_FONT), MHUD_DEFAULT_FONT);
		element.outline = opts->GetPreferenceBool(def.outlineKey, true);
	}

	this->prefs.timerPaused = opts->GetPreferenceColor("mhudTimerPausedColor", MHUD_DEF_TIMER_PAUSED_COLOR);
	this->prefs.timerStopped = opts->GetPreferenceColor("mhudTimerStoppedColor", MHUD_DEF_TIMER_STOPPED_COLOR);
	this->prefs.timerTp = opts->GetPreferenceColor("mhudTimerTpColor", MHUD_DEF_TIMER_TP_COLOR);
	this->prefs.timerPro = opts->GetPreferenceColor("mhudTimerProColor", MHUD_DEF_TIMER_PRO_COLOR);
	this->prefs.speed = opts->GetPreferenceColor("mhudSpeedColor", MHUD_DEF_BASE_COLOR);
	this->prefs.speedCj = opts->GetPreferenceColor("mhudSpeedCjColor", MHUD_DEF_CJ_COLOR);
	this->prefs.prespeed = opts->GetPreferenceColor("mhudPrespeedColor", MHUD_DEF_BASE_COLOR);
	this->prefs.prespeedPerf = opts->GetPreferenceColor("mhudPrespeedPerfColor", MHUD_DEF_PERF_COLOR);
	this->prefs.prespeedJumpbug = opts->GetPreferenceColor("mhudPrespeedJumpbugColor", MHUD_DEF_JUMPBUG_COLOR);
	this->prefs.keys = opts->GetPreferenceColor("mhudKeysColor", MHUD_DEF_BASE_COLOR);
	this->prefs.keysOverlap = opts->GetPreferenceColor("mhudKeysOverlapColor", MHUD_DEF_KEYS_OVERLAP_COLOR);
	this->prefs.keysGlow = opts->GetPreferenceColor("mhudKeysGlowColor", MHUD_DEF_KEYS_GLOW_COLOR);
	this->prefs.checkpoint = opts->GetPreferenceColor("mhudCheckpointColor", MHUD_DEF_BASE_COLOR);

	this->prefs.legacyStyle = opts->GetPreferenceBool("hudLegacyStyle", false);
	this->prefs.compactPanel = opts->GetPreferenceBool("compactPanel", false);
	this->prefs.crosshair = opts->GetPreferenceBool("mhudCrosshair", false);
	this->prefs.timerDetailed = opts->GetPreferenceBool("mhudTimerDetailed", true);
	this->prefs.speedPrecise = opts->GetPreferenceBool("mhudSpeedPrecise", false);
	this->prefs.prespeedPrecise = opts->GetPreferenceBool("mhudPrespeedPrecise", false);
	this->prefs.prespeedBrackets = opts->GetPreferenceBool("mhudPrespeedBrackets", false);
	this->prefs.prespeedHideWalkOff = opts->GetPreferenceBool("mhudPrespeedHideWalkOff", false);
	this->prefs.keysOverlapEnabled = opts->GetPreferenceBool("mhudKeysOverlap", true);
	this->prefs.keysLetters = opts->GetPreferenceBool("mhudKeysLetters", false);
	this->prefs.keysSquare = opts->GetPreferenceBool("mhudKeysSquare", false);
	this->prefs.keysBorder = opts->GetPreferenceBool("mhudKeysBorder", true);
	this->prefs.keysGlowEnabled = opts->GetPreferenceBool("mhudKeysGlow", true);
	// mhudKeysIdle replaced the mhudKeysHideUnpressed toggle; carry the old setting over once.
	const i32 idle = opts->HasPreference("mhudKeysIdle")
						 ? (i32)opts->GetPreferenceInt("mhudKeysIdle", (i64)MHUDKeysIdle::Show)
						 : (opts->GetPreferenceBool("mhudKeysHideUnpressed", false) ? (i32)MHUDKeysIdle::Hide : (i32)MHUDKeysIdle::Show);
	this->prefs.keysIdle = (MHUDKeysIdle)Clamp(idle, (i32)MHUDKeysIdle::Show, (i32)MHUDKeysIdle::Underscore);

	this->prefsDirty = false;
}

const MHUDPrefs &KZHUDService::GetPrefs()
{
	if (this->prefsDirty)
	{
		this->RefreshPrefs();
	}
	return this->prefs;
}

const char *KZHUDService::GetMHUDFontClass(KZPlayer *player, MHUDElement element)
{
	return player->hudService->GetPrefs().elements[(i32)element].fontClass;
}

bool KZHUDService::IsMHUDElementEnabled(MHUDElement element)
{
	return this->GetPrefs().elements[(i32)element].enabled;
}

bool KZHUDService::IsMHUDTimerDetailed()
{
	return this->GetPrefs().timerDetailed;
}

bool KZHUDService::IsMHUDOutlineEnabled(MHUDElement element)
{
	return this->GetPrefs().elements[(i32)element].outline;
}

bool KZHUDService::IsUsingLayoutStyle()
{
	return KZHUDService::IsLayoutHudAvailable() && !this->GetPrefs().legacyStyle;
}

void KZHUDService::ToggleStyle()
{
	auto *opts = this->player->optionService;
	opts->SetPreferenceBool("hudLegacyStyle", !opts->GetPreferenceBool("hudLegacyStyle", false));
}
