#pragma once
// Suppress HL2SDK related warnings, comment these out if something goes wrong
#ifdef _WIN32
#pragma warning (disable: 4099)
#pragma warning (disable: 4005)
#pragma warning (disable: 4267)
#endif

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <sh_vector.h>
#include <stdint.h>

#include "entity2/entitysystem.h"
#define MAXPLAYERS 64

#define C_WHITE            "\x1"
#define C_RED              "\x2"
#define C_LIGHT_PURPLE     "\x3"
#define C_GREEN            "\x4"
#define C_LIGHTER_GREEN    "\x5"
#define C_LIGHT_GREEN      "\x6"
#define C_LIGHT_RED        "\x7"
#define C_GREY             "\x8"
#define C_YELLOW           "\x9"

#define C_LIGHTER_BLUE     "\xa"
#define C_LIGHT_BLUE       "\xb"
#define C_BLUE             "\xc"
#define C_LIGHTER_BLUE2    "\xd"
#define C_PURPLE           "\xe"
#define C_LIGHTER_RED      "\xf"
#define C_GOLD             "\x10"

extern CEntitySystem *g_pEntitySystem;

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

PLUGIN_GLOBALVARS();

// these are for searchability, because static behaves differently in different scopes.
#define internal static // static functions & static global variables
#define local_persist static // static local variables

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32 b32; // 32 bit boolean

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;
