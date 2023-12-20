#pragma once
// Suppress HL2SDK related warnings, comment these out if something goes wrong
#ifdef _WIN32
#pragma warning (disable: 4099 4005 4267 5033)
#endif

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <sh_vector.h>
#include <stdint.h>

#include "entity2/entitysystem.h"
#define MAXPLAYERS 64

extern CGlobalVars *gpGlobals;
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
#define static_global static // static class functions

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
