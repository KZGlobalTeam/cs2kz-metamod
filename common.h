#pragma once
// Suppress HL2SDK related warnings, comment these out if something goes wrong
#pragma warning (disable: 4099)
#pragma warning (disable: 4005)
#pragma warning (disable: 4267)

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <sh_vector.h>

#include "entity2/entitysystem.h"
#define MAXPLAYERS 64

extern CEntitySystem *g_pEntitySystem;

#ifdef _WIN32
#define ROOTBIN "/bin/win64/"
#define GAMEBIN "/csgo/bin/win64/"
#else
#define ROOTBIN "/bin/linuxsteamrt64/"
#define GAMEBIN "/csgo/bin/linuxsteamrt64/"
#endif

PLUGIN_GLOBALVARS();