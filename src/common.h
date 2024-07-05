#pragma once
// Suppress HL2SDK related warnings, comment these out if something goes wrong
#ifdef _WIN32
#pragma warning(disable: 4099 4005 4267 5033)
#endif

#include <ISmmPlugin.h>
#include <stdint.h>
// This is quite wild, this library must be included here so memory corruption doesn't happen on windows...
#define TINYFORMAT_USE_VARIADIC_TEMPLATES
#include "vendor/tinyformat.h"

#include "entity2/entitysystem.h"
#define MAXPLAYERS 64

#define ENGINE_FIXED_TICK_INTERVAL 0.015625f
#define ENGINE_FIXED_TICK_RATE     (1.0f / ENGINE_FIXED_TICK_INTERVAL)
#define EPSILON                    0.000001f

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

PLUGIN_GLOBALVARS();

// These are for searchability, because static behaves differently in different scopes.
// Regular static is for static class functions.
#define static_global   static // static global variables
#define static_persist  static // static local variables
#define static_function static // static functions

typedef int8 i8;
typedef int16 i16;
typedef int32 i32;
typedef int64 i64;

typedef uint8 u8;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef float f32;
typedef double f64;

#define CS_TEAM_NONE      0
#define CS_TEAM_SPECTATOR 1
#define CS_TEAM_T         2
#define CS_TEAM_CT        3

// Enable water fix.
#define WATER_FIX
