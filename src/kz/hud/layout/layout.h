#pragma once
#include "kz/hud/kz_hud.h"

#define MHUD_DEF_TIMER_X    0
#define MHUD_DEF_TIMER_Y    35
#define MHUD_DEF_TIMER_SIZE 24

#define MHUD_DEF_SPEED_X    0
#define MHUD_DEF_SPEED_Y    8
#define MHUD_DEF_SPEED_SIZE 34

#define MHUD_DEF_PRESPEED_X    0
#define MHUD_DEF_PRESPEED_Y    12
#define MHUD_DEF_PRESPEED_SIZE 22

#define MHUD_DEF_KEYS_X    0
#define MHUD_DEF_KEYS_Y    20
#define MHUD_DEF_KEYS_SIZE 20

#define MHUD_DEF_CHECKPOINT_X    0
#define MHUD_DEF_CHECKPOINT_Y    30
#define MHUD_DEF_CHECKPOINT_SIZE 20

// The indicators stack to the right of the speed readout, in the order movementhud draws them.
#define MHUD_DEF_JUMPBUG_X    12
#define MHUD_DEF_JUMPBUG_Y    5
#define MHUD_DEF_JUMPBUG_SIZE 18

#define MHUD_DEF_CJ_X    12
#define MHUD_DEF_CJ_Y    8
#define MHUD_DEF_CJ_SIZE 18

#define MHUD_DEF_PERF_X    12
#define MHUD_DEF_PERF_Y    11
#define MHUD_DEF_PERF_SIZE 18

// Default element colors (MHUD_DEF_*_COLOR) live in kz_hud.h: both HUD styles share them.

#define MHUD_DEFAULT_FONT "stratum2-bold-monodigit"
#define KZ_MHUD_LAYOUT    "panorama/layout/custom_game/cs2kz/mhud.vxml_c"
