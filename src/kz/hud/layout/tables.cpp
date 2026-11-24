#include "kz/hud/layout/layout.h"

#include "tier0/memdbgon.h"

// clang-format off
extern const MHUDElementDef mhudElements[(i32)MHUDElement::Count] =
{
	{"mhud_timer",      "mhud_timer_pos",      "mhud_timer_hit",      "timer",      "mhudTimerEnabled",      "mhudTimerX",      "mhudTimerY",      "mhudTimerSize",      "mhudTimerFont",      "mhudTimerOutline",      "mhudTimerOpacity",      "mhudTimerAlign",      NULL,                  MHUD_DEF_TIMER_X,      MHUD_DEF_TIMER_Y,      MHUD_DEF_TIMER_SIZE,      MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_speed",      "mhud_speed_pos",      "mhud_speed_hit",      "speed",      "mhudSpeedEnabled",      "mhudSpeedX",      "mhudSpeedY",      "mhudSpeedSize",      "mhudSpeedFont",      "mhudSpeedOutline",      "mhudSpeedOpacity",      "mhudSpeedAlign",      "mhudSpeedBorder",     MHUD_DEF_SPEED_X,      MHUD_DEF_SPEED_Y,      MHUD_DEF_SPEED_SIZE,      MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_prespeed",   "mhud_prespeed_pos",   "mhud_prespeed_hit",   "prespeed",   "mhudPrespeedEnabled",   "mhudPrespeedX",   "mhudPrespeedY",   "mhudPrespeedSize",   "mhudPrespeedFont",   "mhudPrespeedOutline",   "mhudPrespeedOpacity",   "mhudPrespeedAlign",   "mhudPrespeedBorder",  MHUD_DEF_PRESPEED_X,   MHUD_DEF_PRESPEED_Y,   MHUD_DEF_PRESPEED_SIZE,   MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_keys",       "mhud_keys_pos",       "mhud_keys_hit",       "keys",       "mhudKeysEnabled",       "mhudKeysX",       "mhudKeysY",       "mhudKeysSize",       "mhudKeysFont",       "mhudKeysOutline",       "mhudKeysOpacity",       NULL,                  NULL,                  MHUD_DEF_KEYS_X,       MHUD_DEF_KEYS_Y,       MHUD_DEF_KEYS_SIZE,       MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_checkpoint", "mhud_checkpoint_pos", "mhud_checkpoint_hit", "checkpoint", "mhudCheckpointEnabled", "mhudCheckpointX", "mhudCheckpointY", "mhudCheckpointSize", "mhudCheckpointFont", "mhudCheckpointOutline", "mhudCheckpointOpacity", "mhudCheckpointAlign", NULL,                  MHUD_DEF_CHECKPOINT_X, MHUD_DEF_CHECKPOINT_Y, MHUD_DEF_CHECKPOINT_SIZE, MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_js",         "mhud_js_pos",         "mhud_js_hit",         NULL,         NULL,                    "mhudJsX",         "mhudJsY",         "mhudJsSize",         "mhudJsFont",         NULL,                    "mhudJsOpacity",         NULL,                  NULL,                  MHUD_DEF_JS_X,         MHUD_DEF_JS_Y,         MHUD_DEF_JS_SIZE,         MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX},
	{"mhud_course",     "mhud_course_pos",     "mhud_course_hit",     NULL,         "mhudCourseEnabled",     "mhudCourseX",     "mhudCourseY",     "mhudCourseSize",     "mhudCourseFont",     NULL,                    "mhudCourseOpacity",     "mhudCourseAlign",     NULL,                  MHUD_DEF_COURSE_X,     MHUD_DEF_COURSE_Y,     MHUD_DEF_COURSE_SIZE,     MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX},
	{"mhud_release",    "mhud_release_pos",    "mhud_release_hit",    "release",    "mhudReleaseEnabled",    "mhudReleaseX",    "mhudReleaseY",    "mhudReleaseSize",    "mhudReleaseFont",    "mhudReleaseOutline",    "mhudReleaseOpacity",    "mhudReleaseAlign",    NULL,                  MHUD_DEF_RELEASE_X,    MHUD_DEF_RELEASE_Y,    MHUD_DEF_RELEASE_SIZE,    MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_race",       "mhud_race_pos",       "mhud_race_hit",       NULL,         "mhudRaceEnabled",       "mhudRaceX",       "mhudRaceY",       "mhudRaceSize",       "mhudRaceFont",       NULL,                    "mhudRaceOpacity",       "mhudRaceAlign",       NULL,                  MHUD_DEF_RACE_X,       MHUD_DEF_RACE_Y,       MHUD_DEF_RACE_SIZE,       MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX},
	{"mhud_perf",       "mhud_perf_pos",       "mhud_perf_hit",       "perf",       "mhudPerfEnabled",       "mhudPerfX",       "mhudPerfY",       "mhudPerfSize",       "mhudPerfFont",       "mhudPerfOutline",       "mhudPerfOpacity",       "mhudPerfAlign",       NULL,                  MHUD_DEF_PERF_X,       MHUD_DEF_PERF_Y,       MHUD_DEF_PERF_SIZE,       MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_cj",         "mhud_cj_pos",         "mhud_cj_hit",         "cj",         "mhudCjEnabled",         "mhudCjX",         "mhudCjY",         "mhudCjSize",         "mhudCjFont",         "mhudCjOutline",         "mhudCjOpacity",         "mhudCjAlign",         NULL,                  MHUD_DEF_CJ_X,         MHUD_DEF_CJ_Y,         MHUD_DEF_CJ_SIZE,         MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
	{"mhud_jumpbug",    "mhud_jumpbug_pos",    "mhud_jumpbug_hit",    "jumpbug",    "mhudJumpbugEnabled",    "mhudJumpbugX",    "mhudJumpbugY",    "mhudJumpbugSize",    "mhudJumpbugFont",    "mhudJumpbugOutline",    "mhudJumpbugOpacity",    "mhudJumpbugAlign",    NULL,                  MHUD_DEF_JUMPBUG_X,    MHUD_DEF_JUMPBUG_Y,    MHUD_DEF_JUMPBUG_SIZE,    MHUD_SIZE_MIN,    MHUD_SIZE_MAX},
};

// A sample value inside each pair so the picker shows the symbols themselves and needs no phrases.
extern const MHUDBorderDef mhudBorders[(i32)MHUDBorder::Count] =
{
	{"",   "",   NULL},
	{"(",  ")",  "(0)"},
	{"-",  "-",  "-0-"},
	{"--", "--", "--0--"},
	{"<",  ">",  "<0>"},
	{"[",  "]",  "[0]"},
	{"{",  "}",  "{0}"},
	{"_",  "_",  "_0_"},
	{"__", "__", "__0__"},
	{"|",  "|",  "|0|"},
};

// What each bit hides according to the client's HUD element constructors.
extern const GameHudPartDef gameHudParts[GAME_HUD_PART_COUNT] =
{
	// HIDEHUD_ALL also stops the chat input from opening and kz_hidehud from chat could never undo it.
	{"Menu - Game HUD All", "hideGameHudAll", KZ_HIDEHUD_ALL, nullptr},
	{"Menu - Game HUD Crosshair", "hideGameHudCrosshair", KZ_HIDEHUD_CROSSHAIR, "crosshair"},
	{"Menu - Game HUD Weapons", "hideGameHudWeapons", KZ_HIDEHUD_WEAPONSELECTION, "weapons"},
	{"Menu - Game HUD Notices", "hideGameHudNotices", KZ_HIDEHUD_MISCSTATUS, "notices"},
	{"Menu - Game HUD Voice", "hideGameHudVoice", KZ_HIDEHUD_CHAT, "voice"},
};

extern const MHUDIndicatorDef mhudIndicatorDefs[MHUD_INDICATOR_COUNT] =
{
	{MHUDElement::Perf, "mhudPerfAcronym", "HUD - Perf Full", "HUD - Perf Short",
	 {"Menu - Ind Perf", "Menu - Ind Perf Position", "Menu - Ind Perf Align", "Menu - Ind Perf Size", "Menu - Ind Perf Font",
	  "Menu - Ind Perf Outline", "Menu - Ind Perf Opacity", "Menu - Ind Perf Color", "Menu - Ind Perf Acronym"}},
	{MHUDElement::CrouchJump, "mhudCjAcronym", "HUD - Crouch Jump Full", "HUD - Crouch Jump Short",
	 {"Menu - Ind CJ", "Menu - Ind CJ Position", "Menu - Ind CJ Align", "Menu - Ind CJ Size", "Menu - Ind CJ Font",
	  "Menu - Ind CJ Outline", "Menu - Ind CJ Opacity", "Menu - Ind CJ Color", "Menu - Ind CJ Acronym"}},
	{MHUDElement::Jumpbug, "mhudJumpbugAcronym", "HUD - Jumpbug Full", "HUD - Jumpbug Short",
	 {"Menu - Ind JB", "Menu - Ind JB Position", "Menu - Ind JB Align", "Menu - Ind JB Size", "Menu - Ind JB Font",
	  "Menu - Ind JB Outline", "Menu - Ind JB Opacity", "Menu - Ind JB Color", "Menu - Ind JB Acronym"}},
};

static_global constexpr MHUDColorPrefDef timerColorPrefs[] =
{
	{"Menu - Color Pro",      "mhudTimerProColor",     0x5F, 0x99, 0xD9},
	{"Menu - Color TP",       "mhudTimerTpColor",      0xFF, 0xFF, 0xFF},
	{"Menu - Color Paused",   "mhudTimerPausedColor",  0xFF, 0xFF, 0x00},
	{"Menu - Color Stopped",  "mhudTimerStoppedColor", 0xFF, 0xA0, 0xA0},
};

static_global constexpr MHUDColorPrefDef speedColorPrefs[] =
{
	{"Menu - Color Base",       "mhudSpeedColor",           0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color CJ",         "mhudSpeedCjColor",         0x71, 0xEE, 0xB8, false, NULL, true},
	{"Menu - Color Perf",       "mhudSpeedPerfColor",       0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color Crouch Perf","mhudSpeedCrouchPerfColor", 0x71, 0xEE, 0xB8, false, NULL, true},
	{"Menu - Color Jumpbug",    "mhudSpeedJumpbugColor",    0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color Deadstrafe", "mhudSpeedDeadstrafeColor", 0xFF, 0x00, 0x00, false, "mhudSpeedDeadstrafe"},
};

static_global constexpr MHUDColorPrefDef prespeedColorPrefs[] =
{
	{"Menu - Color Base",       "mhudPrespeedColor",           0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color CJ",         "mhudPrespeedCjColor",         0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color Perf",       "mhudPrespeedPerfColor",       0x40, 0xFF, 0x40, false, NULL, true},
	{"Menu - Color Crouch Perf","mhudPrespeedCrouchPerfColor", 0x40, 0xFF, 0x40, false, NULL, true},
	{"Menu - Color Jumpbug",    "mhudPrespeedJumpbugColor",    0xFF, 0xFF, 0x20, false, NULL, true},
	{"Menu - Color Deadstrafe", "mhudPrespeedDeadstrafeColor", 0xFF, 0x00, 0x00, false, "mhudPrespeedDeadstrafe"},
};

static_global constexpr MHUDColorPrefDef keysColorPrefs[] =
{
	{"Menu - Color Base",     "mhudKeysColor",         0xFF, 0xFF, 0xFF},
	{"Menu - Color Overlap",  "mhudKeysOverlapColor",  0xFF, 0x40, 0x40, false, "mhudKeysOverlap"},
	{"Menu - Color Overlap Glow", "mhudKeysOverlapGlowColor", 0xFF, 0x40, 0x40, true, "mhudKeysOverlap"},
	{"Menu - Color Pressed",      "mhudKeysPressedColor",     0x3B, 0xED, 0xA0, true},
};

static_global constexpr MHUDColorPrefDef checkpointColorPrefs[] =
{
	{"Menu - Color Base",     "mhudCheckpointColor",   0xFF, 0xFF, 0xFF},
	{"Menu - Color TP",       "mhudCheckpointTpColor", 0xFF, 0xFF, 0xFF},
};

static_global constexpr MHUDColorPrefDef releaseColorPrefs[] =
{
	{"Menu - Color Early",    "mhudReleaseEarlyColor",   0x5E, 0x98, 0xD9},
	{"Menu - Color Perfect",  "mhudReleasePerfectColor", 0x40, 0xFF, 0x40},
	{"Menu - Color Late",     "mhudReleaseLateColor",    0xFF, 0x40, 0x40},
};

// One color each; the flattened Indicators page relabels the row with the indicator's own phrase.
static_global constexpr MHUDColorPrefDef perfColorPrefs[] =
{
	{"Menu - Color Base",     "mhudPerfColor",         0x40, 0xFF, 0x40},
};

static_global constexpr MHUDColorPrefDef cjColorPrefs[] =
{
	{"Menu - Color Base",     "mhudCjColor",           0x71, 0xEE, 0xB8},
};

static_global constexpr MHUDColorPrefDef jumpbugColorPrefs[] =
{
	{"Menu - Color Base",     "mhudJumpbugColor",      0xFF, 0xFF, 0x20},
};
// clang-format on

const MHUDColorPrefDef *KZHUDService::GetMHUDElementColorPrefs(MHUDElement element, i32 &count)
{
	switch (element)
	{
		case MHUDElement::Timer:
			count = KZ_ARRAYSIZE(timerColorPrefs);
			return timerColorPrefs;
		case MHUDElement::Speed:
			count = KZ_ARRAYSIZE(speedColorPrefs);
			return speedColorPrefs;
		case MHUDElement::Prespeed:
			count = KZ_ARRAYSIZE(prespeedColorPrefs);
			return prespeedColorPrefs;
		case MHUDElement::Keys:
			count = KZ_ARRAYSIZE(keysColorPrefs);
			return keysColorPrefs;
		case MHUDElement::Checkpoint:
			count = KZ_ARRAYSIZE(checkpointColorPrefs);
			return checkpointColorPrefs;
		case MHUDElement::Release:
			count = KZ_ARRAYSIZE(releaseColorPrefs);
			return releaseColorPrefs;
		case MHUDElement::Perf:
			count = KZ_ARRAYSIZE(perfColorPrefs);
			return perfColorPrefs;
		case MHUDElement::CrouchJump:
			count = KZ_ARRAYSIZE(cjColorPrefs);
			return cjColorPrefs;
		case MHUDElement::Jumpbug:
			count = KZ_ARRAYSIZE(jumpbugColorPrefs);
			return jumpbugColorPrefs;
		default:
			count = 0;
			return NULL;
	}
}
