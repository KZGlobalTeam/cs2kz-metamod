#include "kz/hud/layout/layout.h"

#include "tier0/memdbgon.h"

// clang-format off
extern const MHUDElementDef MHUD_ELEMENTS[(i32)MHUDElement::Count] =
{
	{"mhud_timer",      "timer",      "mhudTimerEnabled",      "mhudTimerX",      "mhudTimerY",      "mhudTimerSize",      "mhudTimerFont",      "mhudTimerOutline",      "mhudTimerOpacity",      "mhudTimerAlign",      NULL,                  MHUD_DEF_TIMER_X,      MHUD_DEF_TIMER_Y,      MHUD_DEF_TIMER_SIZE},
	{"mhud_speed",      "speed",      "mhudSpeedEnabled",      "mhudSpeedX",      "mhudSpeedY",      "mhudSpeedSize",      "mhudSpeedFont",      "mhudSpeedOutline",      "mhudSpeedOpacity",      "mhudSpeedAlign",      "mhudSpeedBorder",     MHUD_DEF_SPEED_X,      MHUD_DEF_SPEED_Y,      MHUD_DEF_SPEED_SIZE},
	{"mhud_prespeed",   "prespeed",   "mhudPrespeedEnabled",   "mhudPrespeedX",   "mhudPrespeedY",   "mhudPrespeedSize",   "mhudPrespeedFont",   "mhudPrespeedOutline",   "mhudPrespeedOpacity",   "mhudPrespeedAlign",   "mhudPrespeedBorder",  MHUD_DEF_PRESPEED_X,   MHUD_DEF_PRESPEED_Y,   MHUD_DEF_PRESPEED_SIZE},
	{"mhud_keys",       "keys",       "mhudKeysEnabled",       "mhudKeysX",       "mhudKeysY",       "mhudKeysSize",       "mhudKeysFont",       "mhudKeysOutline",       "mhudKeysOpacity",       NULL,                  NULL,                  MHUD_DEF_KEYS_X,       MHUD_DEF_KEYS_Y,       MHUD_DEF_KEYS_SIZE},
	{"mhud_checkpoint", "checkpoint", "mhudCheckpointEnabled", "mhudCheckpointX", "mhudCheckpointY", "mhudCheckpointSize", "mhudCheckpointFont", "mhudCheckpointOutline", "mhudCheckpointOpacity", "mhudCheckpointAlign", NULL,                  MHUD_DEF_CHECKPOINT_X, MHUD_DEF_CHECKPOINT_Y, MHUD_DEF_CHECKPOINT_SIZE},
	{"mhud_perf",       "perf",       "mhudPerfEnabled",       "mhudPerfX",       "mhudPerfY",       "mhudPerfSize",       "mhudPerfFont",       "mhudPerfOutline",       "mhudPerfOpacity",       "mhudPerfAlign",       NULL,                  MHUD_DEF_PERF_X,       MHUD_DEF_PERF_Y,       MHUD_DEF_PERF_SIZE},
	{"mhud_cj",         "cj",         "mhudCjEnabled",         "mhudCjX",         "mhudCjY",         "mhudCjSize",         "mhudCjFont",         "mhudCjOutline",         "mhudCjOpacity",         "mhudCjAlign",         NULL,                  MHUD_DEF_CJ_X,         MHUD_DEF_CJ_Y,         MHUD_DEF_CJ_SIZE},
	{"mhud_jumpbug",    "jumpbug",    "mhudJumpbugEnabled",    "mhudJumpbugX",    "mhudJumpbugY",    "mhudJumpbugSize",    "mhudJumpbugFont",    "mhudJumpbugOutline",    "mhudJumpbugOpacity",    "mhudJumpbugAlign",    NULL,                  MHUD_DEF_JUMPBUG_X,    MHUD_DEF_JUMPBUG_Y,    MHUD_DEF_JUMPBUG_SIZE},
};

// A sample value inside each pair, so the picker shows the symbols themselves and needs no phrases.
extern const MHUDBorderDef MHUD_BORDERS[(i32)MHUDBorder::Count] =
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

// What each bit hides, from the client's HUD element constructors.
extern const GameHudPartDef GAME_HUD_PARTS[GAME_HUD_PART_COUNT] =
{
	{"Menu - Game HUD All", "hideGameHudAll", KZ_HIDEHUD_ALL},
	{"Menu - Game HUD Crosshair", "hideGameHudCrosshair", KZ_HIDEHUD_CROSSHAIR},
	{"Menu - Game HUD Weapons", "hideGameHudWeapons", KZ_HIDEHUD_WEAPONSELECTION},
	{"Menu - Game HUD Damage", "hideGameHudDamage", KZ_HIDEHUD_HEALTH},
	{"Menu - Game HUD Notices", "hideGameHudNotices", KZ_HIDEHUD_MISCSTATUS},
	{"Menu - Game HUD Voice", "hideGameHudVoice", KZ_HIDEHUD_CHAT},
};

extern const MHUDIndicatorDef MHUD_INDICATOR_DEFS[MHUD_INDICATOR_COUNT] =
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

static_global constexpr MHUDColorPrefDef TIMER_COLOR_PREFS[] =
{
	{"Menu - Color Pro",      "mhudTimerProColor",     0x5F, 0x99, 0xD9},
	{"Menu - Color TP",       "mhudTimerTpColor",      0xFF, 0xFF, 0xFF},
	{"Menu - Color Paused",   "mhudTimerPausedColor",  0xFF, 0xFF, 0x00},
	{"Menu - Color Stopped",  "mhudTimerStoppedColor", 0xFF, 0xA0, 0xA0},
};

static_global constexpr MHUDColorPrefDef SPEED_COLOR_PREFS[] =
{
	{"Menu - Color Base",       "mhudSpeedColor",           0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color CJ",         "mhudSpeedCjColor",         0x71, 0xEE, 0xB8, false, NULL, true},
	{"Menu - Color Perf",       "mhudSpeedPerfColor",       0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color Crouch Perf","mhudSpeedCrouchPerfColor", 0x71, 0xEE, 0xB8, false, NULL, true},
	{"Menu - Color Jumpbug",    "mhudSpeedJumpbugColor",    0xFF, 0xFF, 0xFF, false, NULL, true},
};

static_global constexpr MHUDColorPrefDef PRESPEED_COLOR_PREFS[] =
{
	{"Menu - Color Base",       "mhudPrespeedColor",           0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color CJ",         "mhudPrespeedCjColor",         0xFF, 0xFF, 0xFF, false, NULL, true},
	{"Menu - Color Perf",       "mhudPrespeedPerfColor",       0x40, 0xFF, 0x40, false, NULL, true},
	{"Menu - Color Crouch Perf","mhudPrespeedCrouchPerfColor", 0x40, 0xFF, 0x40, false, NULL, true},
	{"Menu - Color Jumpbug",    "mhudPrespeedJumpbugColor",    0xFF, 0xFF, 0x20, false, NULL, true},
};

static_global constexpr MHUDColorPrefDef KEYS_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudKeysColor",         0xFF, 0xFF, 0xFF},
	{"Menu - Color Overlap",  "mhudKeysOverlapColor",  0xFF, 0x40, 0x40, false, "mhudKeysOverlap"},
	{"Menu - Color Overlap Glow", "mhudKeysOverlapGlowColor", 0xFF, 0x40, 0x40, true, "mhudKeysOverlap"},
	{"Menu - Color Pressed",      "mhudKeysPressedColor",     0x3B, 0xED, 0xA0, true},
};

static_global constexpr MHUDColorPrefDef CHECKPOINT_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudCheckpointColor",   0xFF, 0xFF, 0xFF},
	{"Menu - Color TP",       "mhudCheckpointTpColor", 0xFF, 0xFF, 0xFF},
};

// One color each; the flattened Indicators page relabels the row with the indicator's own phrase.
static_global constexpr MHUDColorPrefDef PERF_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudPerfColor",         0x40, 0xFF, 0x40},
};

static_global constexpr MHUDColorPrefDef CJ_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudCjColor",           0x71, 0xEE, 0xB8},
};

static_global constexpr MHUDColorPrefDef JUMPBUG_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudJumpbugColor",      0xFF, 0xFF, 0x20},
};
// clang-format on

const MHUDColorPrefDef *KZHUDService::GetMHUDElementColorPrefs(MHUDElement element, i32 &count)
{
	switch (element)
	{
		case MHUDElement::Timer:
			count = KZ_ARRAYSIZE(TIMER_COLOR_PREFS);
			return TIMER_COLOR_PREFS;
		case MHUDElement::Speed:
			count = KZ_ARRAYSIZE(SPEED_COLOR_PREFS);
			return SPEED_COLOR_PREFS;
		case MHUDElement::Prespeed:
			count = KZ_ARRAYSIZE(PRESPEED_COLOR_PREFS);
			return PRESPEED_COLOR_PREFS;
		case MHUDElement::Keys:
			count = KZ_ARRAYSIZE(KEYS_COLOR_PREFS);
			return KEYS_COLOR_PREFS;
		case MHUDElement::Checkpoint:
			count = KZ_ARRAYSIZE(CHECKPOINT_COLOR_PREFS);
			return CHECKPOINT_COLOR_PREFS;
		case MHUDElement::Perf:
			count = KZ_ARRAYSIZE(PERF_COLOR_PREFS);
			return PERF_COLOR_PREFS;
		case MHUDElement::CrouchJump:
			count = KZ_ARRAYSIZE(CJ_COLOR_PREFS);
			return CJ_COLOR_PREFS;
		case MHUDElement::Jumpbug:
			count = KZ_ARRAYSIZE(JUMPBUG_COLOR_PREFS);
			return JUMPBUG_COLOR_PREFS;
		default:
			count = 0;
			return NULL;
	}
}
