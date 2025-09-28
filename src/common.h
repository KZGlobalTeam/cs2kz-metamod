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
#include "tier0/dbg.h"

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

// Common macro for service event listeners in different parts of the project.
#define CALL_FORWARD(listeners, func, ...)                FOR_EACH_VEC(listeners, i) listeners[i]->func(__VA_ARGS__);
#define CALL_FORWARD_BOOL(retValue, listeners, func, ...) FOR_EACH_VEC(listeners, i) retValue &= listeners[i]->func(__VA_ARGS__)

// str*cmp considered harmful.
//  Macros to make sure you don't mess up checking if strings are equal.
//  The I means case insensitive.
#define KZ_STREQ(a, b)             (V_strcmp(a, b) == 0)
#define KZ_STREQI(a, b)            (V_stricmp(a, b) == 0)
#define KZ_STREQLEN(a, b, maxlen)  (V_strncmp(a, b, maxlen) == 0)
#define KZ_STREQILEN(a, b, maxlen) (V_strnicmp(a, b, maxlen) == 0)
// ARRAYSIZE gets undef'd if metamod_oslink is included after commonmacros.h. We can use our own implementation instead.
#define KZ_ARRAYSIZE(a)       ((sizeof(a) / sizeof(*(a))) / static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#define KZ_FOURCC(a, b, c, d) ((u32)(((d) << 24) | ((c) << 16) | ((b) << 8) | (a)))

#define VPROF_LEVEL 1
#define VPROF_ENTER_SCOPE_KZ(name) \
	g_VProfCurrentProfile.EnterScope(name, false, VProfBudgetGroupCallSite {"CS2KZ", 0}, {__FILE__, __LINE__, __func__})
