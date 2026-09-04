#include "kz/hud/layout/layout.h"

#include "tier0/memdbgon.h"

// clang-format off
extern const MHUDElementDef MHUD_ELEMENTS[(i32)MHUDElement::Count] =
{
	{"mhud_timer",      "timer",      "mhudTimerEnabled",      "mhudTimerX",      "mhudTimerY",      "mhudTimerSize",      "mhudTimerFont",      "mhudTimerOutline",      MHUD_DEF_TIMER_X,      MHUD_DEF_TIMER_Y,      MHUD_DEF_TIMER_SIZE},
	{"mhud_speed",      "speed",      "mhudSpeedEnabled",      "mhudSpeedX",      "mhudSpeedY",      "mhudSpeedSize",      "mhudSpeedFont",      "mhudSpeedOutline",      MHUD_DEF_SPEED_X,      MHUD_DEF_SPEED_Y,      MHUD_DEF_SPEED_SIZE},
	{"mhud_prespeed",   "prespeed",   "mhudPrespeedEnabled",   "mhudPrespeedX",   "mhudPrespeedY",   "mhudPrespeedSize",   "mhudPrespeedFont",   "mhudPrespeedOutline",   MHUD_DEF_PRESPEED_X,   MHUD_DEF_PRESPEED_Y,   MHUD_DEF_PRESPEED_SIZE},
	{"mhud_keys",       "keys",       "mhudKeysEnabled",       "mhudKeysX",       "mhudKeysY",       "mhudKeysSize",       "mhudKeysFont",       "mhudKeysOutline",       MHUD_DEF_KEYS_X,       MHUD_DEF_KEYS_Y,       MHUD_DEF_KEYS_SIZE},
	{"mhud_checkpoint", "checkpoint", "mhudCheckpointEnabled", "mhudCheckpointX", "mhudCheckpointY", "mhudCheckpointSize", "mhudCheckpointFont", "mhudCheckpointOutline", MHUD_DEF_CHECKPOINT_X, MHUD_DEF_CHECKPOINT_Y, MHUD_DEF_CHECKPOINT_SIZE},
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
	{"Menu - Color Base",     "mhudSpeedColor",        0xFF, 0xFF, 0xFF},
	{"Menu - Color CJ",       "mhudSpeedCjColor",      0x71, 0xEE, 0xB8},
};

static_global constexpr MHUDColorPrefDef PRESPEED_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudPrespeedColor",         0xFF, 0xFF, 0xFF},
	{"Menu - Color Perf",     "mhudPrespeedPerfColor",     0x40, 0xFF, 0x40},
	{"Menu - Color Jumpbug",  "mhudPrespeedJumpbugColor",  0xFF, 0xFF, 0x20},
};

static_global constexpr MHUDColorPrefDef KEYS_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudKeysColor",         0xFF, 0xFF, 0xFF},
	{"Menu - Color Overlap",  "mhudKeysOverlapColor",  0xFF, 0x40, 0x40},
};

static_global constexpr MHUDColorPrefDef CHECKPOINT_COLOR_PREFS[] =
{
	{"Menu - Color Base",     "mhudCheckpointColor",   0xFF, 0xFF, 0xFF},
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
		default:
			count = 0;
			return NULL;
	}
}
