#pragma once

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