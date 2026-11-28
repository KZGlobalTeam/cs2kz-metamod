// The per-player MHUD preferences and the class names they turn into.
#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "kz/spec/kz_spec.h"
#include "kz/jumpstats/kz_jumpstats.h"

#include "tier0/memdbgon.h"

void KZHUDService::RefreshPrefs()
{
	auto *opts = this->player->optionService;
	for (i32 e = 0; e < (i32)MHUDElement::Count; e++)
	{
		const MHUDElementDef &def = mhudElements[e];
		MHUDPrefs::Element &element = this->prefs.elements[e];
		// The jumpstats panel follows where jumpstats are reported.
		if (def.enabledKey)
		{
			element.enabled = opts->GetPreferenceBool(def.enabledKey, IsMHUDElementOnByDefault((MHUDElement)e));
		}
		else
		{
			element.enabled = opts->GetPreferenceInt("jsReportType", JSReportType_Hud) != JSReportType_Chat;
		}
		// Older saves allowed +-100 and that puts the anchor off screen.
		element.x = Clamp((f32)opts->GetPreferenceFloat(def.xKey, def.xDefault), -50.0f, 50.0f);
		element.y = Clamp((f32)opts->GetPreferenceFloat(def.yKey, def.yDefault), -50.0f, 50.0f);
		element.size = (f32)opts->GetPreferenceFloat(def.sizeKey, def.sizeDefault);
		if (IsMHUDElementScaled((MHUDElement)e) || (MHUDElement)e == MHUDElement::Potential)
		{
			// Only the scales js-scale.css defines.
			element.size = Clamp(element.size, (f32)def.sizeMin, (f32)def.sizeMax);
		}
		element.fontClass = panorama::ResolveFontClass(opts->GetPreferenceStr(def.fontKey, MHUD_DEFAULT_FONT), MHUD_DEFAULT_FONT);
		element.outline = def.outlineKey ? opts->GetPreferenceBool(def.outlineKey, true) : false;
		element.opacity = (i32)opts->GetPreferenceInt(def.opacityKey, 100);
		const char *bgOpacityKey = GetMHUDBackgroundOpacityKey((MHUDElement)e);
		if ((MHUDElement)e == MHUDElement::Potential)
		{
			bgOpacityKey = "mhudPotentialBgOpacity";
		}
		element.bgOpacity = bgOpacityKey ? (i32)opts->GetPreferenceInt(bgOpacityKey, MHUD_DEF_BG_OPACITY) : MHUD_DEF_BG_OPACITY;
		const i64 alignDefault = (i64)GetMHUDDefaultAlign((MHUDElement)e);
		const i64 align = def.alignKey ? opts->GetPreferenceInt(def.alignKey, alignDefault) : alignDefault;
		element.align = (MHUDAlign)Clamp(align, (i64)MHUDAlign::Left, (i64)MHUDAlign::Right);
		const i64 border = def.borderKey ? opts->GetPreferenceInt(def.borderKey, (i64)MHUDBorder::None) : (i64)MHUDBorder::None;
		element.border = (MHUDBorder)Clamp(border, (i64)MHUDBorder::None, (i64)MHUDBorder::Count - 1);
	}

	this->prefs.timerPaused = opts->GetPreferenceColor("mhudTimerPausedColor", mhudDefTimerPausedColor);
	this->prefs.timerStopped = opts->GetPreferenceColor("mhudTimerStoppedColor", mhudDefTimerStoppedColor);
	this->prefs.timerTp = opts->GetPreferenceColor("mhudTimerTpColor", mhudDefTimerTpColor);
	this->prefs.timerPro = opts->GetPreferenceColor("mhudTimerProColor", mhudDefTimerProColor);
	// Indexed by MHUDSpeedState.
	this->prefs.speed[(i32)MHUDSpeedState::Base] = opts->GetPreferenceColor("mhudSpeedColor", mhudDefBaseColor);
	this->prefs.speed[(i32)MHUDSpeedState::CrouchJump] = opts->GetPreferenceColor("mhudSpeedCjColor", mhudDefCjColor);
	this->prefs.speed[(i32)MHUDSpeedState::Perf] = opts->GetPreferenceColor("mhudSpeedPerfColor", mhudDefBaseColor);
	this->prefs.speed[(i32)MHUDSpeedState::CrouchPerf] = opts->GetPreferenceColor("mhudSpeedCrouchPerfColor", mhudDefCjColor);
	this->prefs.speed[(i32)MHUDSpeedState::Jumpbug] = opts->GetPreferenceColor("mhudSpeedJumpbugColor", mhudDefBaseColor);

	this->prefs.prespeed[(i32)MHUDSpeedState::Base] = opts->GetPreferenceColor("mhudPrespeedColor", mhudDefBaseColor);
	this->prefs.prespeed[(i32)MHUDSpeedState::CrouchJump] = opts->GetPreferenceColor("mhudPrespeedCjColor", mhudDefBaseColor);
	this->prefs.prespeed[(i32)MHUDSpeedState::Perf] = opts->GetPreferenceColor("mhudPrespeedPerfColor", mhudDefPerfColor);
	this->prefs.prespeed[(i32)MHUDSpeedState::CrouchPerf] = opts->GetPreferenceColor("mhudPrespeedCrouchPerfColor", mhudDefPerfColor);
	this->prefs.prespeed[(i32)MHUDSpeedState::Jumpbug] = opts->GetPreferenceColor("mhudPrespeedJumpbugColor", mhudDefJumpbugColor);
	this->prefs.speedDeadstrafe = opts->GetPreferenceColor("mhudSpeedDeadstrafeColor", mhudDefDeadstrafeColor);
	this->prefs.prespeedDeadstrafe = opts->GetPreferenceColor("mhudPrespeedDeadstrafeColor", mhudDefDeadstrafeColor);
	this->prefs.keys = opts->GetPreferenceColor("mhudKeysColor", mhudDefBaseColor);
	this->prefs.keysOverlap = opts->GetPreferenceColor("mhudKeysOverlapColor", mhudDefKeysOverlapColor);
	this->prefs.keysPressed = opts->GetPreferenceColor("mhudKeysPressedColor", mhudDefKeysPressedColor);
	this->prefs.keysOverlapGlow = opts->GetPreferenceColor("mhudKeysOverlapGlowColor", mhudDefKeysOverlapGlowColor);
	this->prefs.checkpoint = opts->GetPreferenceColor("mhudCheckpointColor", mhudDefBaseColor);
	this->prefs.checkpointTp = opts->GetPreferenceColor("mhudCheckpointTpColor", mhudDefBaseColor);

	for (i32 i = 0; i < MHUD_INDICATOR_COUNT; i++)
	{
		const MHUDIndicatorDef &indicator = mhudIndicatorDefs[i];
		i32 count = 0;
		const MHUDColorPrefDef *colors = KZHUDService::GetMHUDElementColorPrefs(indicator.element, count);
		this->prefs.indicator[i] = opts->GetPreferenceColor(colors[0].prefKey, Color(colors[0].r, colors[0].g, colors[0].b, 255));
		this->prefs.indicatorAcronym[i] = opts->GetPreferenceBool(indicator.acronymKey, false);
	}

	KZJumpstatsService::GetFieldLayout(this->player, this->prefs.jsFields);
	this->prefs.jsHistory = opts->GetPreferenceBool("mhudJsShowHistory", true);
	this->prefs.jsHistoryBelow = opts->GetPreferenceBool("mhudJsHistoryBelow", false);
	this->prefs.jsCompact = opts->GetPreferenceBool("mhudJsCompact", false);
	this->prefs.jsDuration = Clamp((i32)opts->GetPreferenceInt("mhudJsDuration", MHUD_JS_PANEL_TIME), MHUD_JS_PANEL_TIME_MIN, MHUD_JS_PANEL_TIME_MAX);
	this->prefs.releaseEarly = opts->GetPreferenceColor("mhudReleaseEarlyColor", mhudDefReleaseEarlyColor);
	this->prefs.releasePerfect = opts->GetPreferenceColor("mhudReleasePerfectColor", mhudDefReleasePerfectColor);
	this->prefs.releaseLate = opts->GetPreferenceColor("mhudReleaseLateColor", mhudDefReleaseLateColor);
	this->prefs.courseMap = opts->GetPreferenceBool("mhudCourseShowMap", true);
	this->prefs.courseProgress = opts->GetPreferenceBool("mhudCourseShowProgress", true);
	this->prefs.courseRecords = opts->GetPreferenceBool("mhudCourseShowRecords", true);
	this->prefs.coursePro = opts->GetPreferenceBool("mhudCourseShowPro", true);
	this->prefs.courseSplits = opts->GetPreferenceBool("mhudCourseShowSplits", true);
	this->prefs.courseShowProgress = opts->GetPreferenceBool("mhudCourseShowRoute", true);
	this->prefs.courseShowProgressLabel = opts->GetPreferenceBool("mhudCourseShowRouteLabel", true);

	this->prefs.legacyStyle = opts->GetPreferenceBool("hudLegacyStyle", false);
	this->prefs.compactPanel = opts->GetPreferenceBool("compactPanel", false);
	this->prefs.timerDetailed = opts->GetPreferenceBool("mhudTimerDetailed", true);
	this->prefs.timerShowState = opts->GetPreferenceBool("mhudTimerShowState", true);
	this->prefs.speedPrecise = opts->GetPreferenceBool("mhudSpeedPrecise", false);
	this->prefs.prespeedPrecise = opts->GetPreferenceBool("mhudPrespeedPrecise", false);
	this->prefs.speedDeadstrafeEnabled = opts->GetPreferenceBool("mhudSpeedDeadstrafe", false);
	this->prefs.prespeedDeadstrafeEnabled = opts->GetPreferenceBool("mhudPrespeedDeadstrafe", false);
	const i32 prespeedShow = (i32)opts->GetPreferenceInt("mhudPrespeedShow", (i64)MHUDPrespeedShow::Brief);
	this->prefs.prespeedShow = (MHUDPrespeedShow)Clamp(prespeedShow, (i32)MHUDPrespeedShow::Brief, (i32)MHUDPrespeedShow::Always);
	this->prefs.keysOverlapEnabled = opts->GetPreferenceBool("mhudKeysOverlap", true);
	this->prefs.keysOverlapAxis = opts->GetPreferenceBool("mhudKeysOverlapAxis", false);
	this->prefs.keysLetters = opts->GetPreferenceBool("mhudKeysLetters", false);
	this->prefs.keysJumpCount = opts->GetPreferenceBool("mhudKeysJumpCount", false);
	this->prefs.keysSquare = opts->GetPreferenceBool("mhudKeysSquare", false);
	this->prefs.keysBorder = opts->GetPreferenceBool("mhudKeysBorder", true);
	this->prefs.keysGlowEnabled = opts->GetPreferenceBool("mhudKeysGlow", true);
	this->prefs.keysFillEnabled = opts->GetPreferenceBool("mhudKeysFill", true);
	// mhudKeysIdle replaced the mhudKeysHideUnpressed toggle; carry the old setting over once.
	const i32 idle = opts->HasPreference("mhudKeysIdle")
						 ? (i32)opts->GetPreferenceInt("mhudKeysIdle", (i64)MHUDKeysIdle::Show)
						 : (opts->GetPreferenceBool("mhudKeysHideUnpressed", false) ? (i32)MHUDKeysIdle::Hide : (i32)MHUDKeysIdle::Show);
	this->prefs.keysIdle = (MHUDKeysIdle)Clamp(idle, (i32)MHUDKeysIdle::Show, (i32)MHUDKeysIdle::Underscore);
	this->prefs.mimicSpec = opts->GetPreferenceBool("mhudMimicSpec", false);
	this->prefs.raceTimeLeft = opts->GetPreferenceBool("raceHudTimeLeft", true);
	this->prefs.racePB = opts->GetPreferenceBool("raceHudPB", true);
	this->prefs.screenWidth = (i32)Clamp(opts->GetPreferenceInt("mhudScreenWidth", MHUD_DEF_SCREEN_WIDTH), (i64)1000, (i64)4000);
	this->prefs.hiddenGameHud = 0;
	for (const GameHudPartDef &part : gameHudParts)
	{
		if (opts->GetPreferenceBool(part.prefKey, false))
		{
			this->prefs.hiddenGameHud |= part.bit;
		}
	}

	this->prefsDirty = false;
}

const MHUDPrefs &KZHUDService::GetOwnPrefs()
{
	if (this->prefsDirty)
	{
		this->RefreshPrefs();
	}
	return this->prefs;
}

const MHUDPrefs &KZHUDService::GetPrefs()
{
	const MHUDPrefs &own = this->GetOwnPrefs();
	if (!own.mimicSpec)
	{
		return own;
	}
	// Bots have no preferences of their own and mimicking a replay bot would just wipe the HUD.
	KZPlayer *target = this->player->specService->GetSpectatedPlayer();
	if (!target || target == this->player || target->IsFakeClient())
	{
		return own;
	}
	return target->hudService->GetOwnPrefs();
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
	// The player's own set on purpose because which HUD is drawn stays their choice even while mimicking.
	return KZHUDService::IsLayoutHudAvailable() && !this->GetOwnPrefs().legacyStyle;
}

void KZHUDService::ToggleStyle()
{
	auto *opts = this->player->optionService;
	opts->SetPreferenceBool("hudLegacyStyle", !opts->GetPreferenceBool("hudLegacyStyle", false));
}
