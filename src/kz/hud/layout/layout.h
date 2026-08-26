#pragma once
#include "kz/hud/kz_hud.h"

#define MHUD_DEF_TIMER_X    0
#define MHUD_DEF_TIMER_Y    4
#define MHUD_DEF_TIMER_SIZE 24

#define MHUD_DEF_SPEED_X    0
#define MHUD_DEF_SPEED_Y    8
#define MHUD_DEF_SPEED_SIZE 34

#define MHUD_DEF_PRESPEED_X    0
#define MHUD_DEF_PRESPEED_Y    12
#define MHUD_DEF_PRESPEED_SIZE 22

#define MHUD_DEF_KEYS_X    0
#define MHUD_DEF_KEYS_Y    16
#define MHUD_DEF_KEYS_SIZE 20

#define MHUD_DEF_CHECKPOINT_X    0
#define MHUD_DEF_CHECKPOINT_Y    24
#define MHUD_DEF_CHECKPOINT_SIZE 20

static_global const Color MHUD_DEF_BASE_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_PERF_COLOR(0x40, 0xFF, 0x40, 0xFF);
static_global const Color MHUD_DEF_JUMPBUG_COLOR(0xFF, 0xFF, 0x20, 0xFF);
static_global const Color MHUD_DEF_CJ_COLOR(0x71, 0xEE, 0xB8, 0xFF);
static_global const Color MHUD_DEF_TIMER_TP_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_TIMER_PRO_COLOR(0x5F, 0x99, 0xD9, 0xFF);
static_global const Color MHUD_DEF_TIMER_PAUSED_COLOR(0xFF, 0xFF, 0x00, 0xFF);
static_global const Color MHUD_DEF_TIMER_STOPPED_COLOR(0xFF, 0xA0, 0xA0, 0xFF);
static_global const Color MHUD_DEF_KEYS_OVERLAP_COLOR(0xFF, 0x40, 0x40, 0xFF);

#define MHUD_DEFAULT_FONT "stratum2-bold-monodigit"
#define KZ_MHUD_LAYOUT    "panorama/layout/custom_game/mhud.xml"

// Returns the class name for the font preference of the given element.
// Static storage, we can keep caching it by pointer.
const char *MHUDFontClass(KZPlayer *player, MHUDElement element);

extern CHandle<CBaseEntity> g_hMHUDLayout;

// The HUD's pages in the options menu.
void MHUDRegisterMenu();
