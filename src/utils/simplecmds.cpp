
#include "common.h"
#include "utils/utils.h"
#include "simplecmds.h"

#include "tier0/memdbgon.h"

// private structs
struct Scmd
{
	b32 hasConsolePrefix;
	i32 nameLength;
	const char *name;
	scmd::Callback_t *callback;
};

struct ScmdManager
{
	i32 cmdCount;
	Scmd cmds[SCMD_MAX_CMDS];
};

internal ScmdManager g_cmdManager = {};

bool scmd::RegisterCmd(const char *name, scmd::Callback_t *callback)
{
	Assert(name);
	Assert(callback);
	if (!name || !callback || g_cmdManager.cmdCount >= SCMD_MAX_CMDS)
	{
		// TODO: print warning? error? segfault?
		Assert(0);
		return false;
	}
	
	i32 nameLength = strlen(name);
	
	if (nameLength == 0)
	{
		// TODO: print warning? error? segfault?
		Assert(0);
		return false;
	}
	
	i32 conPrefixLen = strlen(SCMD_CONSOLE_PREFIX);
	bool hasConPrefix = false;
	if (nameLength >= conPrefixLen && strnicmp(name, SCMD_CONSOLE_PREFIX, conPrefixLen) == 0)
	{
		if (nameLength == conPrefixLen)
		{
			// name is just the console prefix
			// TODO: print warning? error? segfault?
			Assert(0);
			return false;
		}
		hasConPrefix = true;
	}
	
	// Check if command with this name already exists
	Scmd *cmds = g_cmdManager.cmds;
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (nameLength != g_cmdManager.cmds[i].nameLength)
		{
			continue;
		}
		
		if (V_memcmp(name, g_cmdManager.cmds[i].name, nameLength) == 0)
		{
			// TODO: print warning? error? segfault?
			// Command already exists
			//Assert(0);
			return false;
		}
	}
	
	// Command name is unique!
	Scmd cmd = {
		hasConPrefix,
		nameLength,
		name,
		callback
	};
	g_cmdManager.cmds[g_cmdManager.cmdCount++] = cmd;
	return true;
}

META_RES scmd::OnClientCommand(CPlayerSlot &slot, const CCommand &args)
{
	META_RES result = MRES_IGNORED;
	CCSPlayerController *controller = (CCSPlayerController *)g_pEntitySystem->GetBaseEntity(CEntityIndex((i32)slot.Get() + 1));
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!g_cmdManager.cmds[i].callback)
		{
			// TODO: error?
			Assert(g_cmdManager.cmds[i].callback);
			continue;
		}
		
		if (stricmp(g_cmdManager.cmds[i].name, args[0]) == 0)
		{
			result = g_cmdManager.cmds[i].callback(controller, &args);
		}
	}
	return result;
}

META_RES scmd::OnHost_Say(CCSPlayerController *controller, const CCommand &args)
{
	if (args.ArgC() < 2)
	{
		// no argument somehow
		return MRES_IGNORED;
	}
	
	if (args[1][0] != SCMD_CHAT_TRIGGER && args[1][0] != SCMD_CHAT_SILENT_TRIGGER)
	{
		// no chat command trigger
		return MRES_IGNORED;
	}
	
	i32 argLen = strlen(args[1]);
	if (argLen < 1)
	{
		// arg is too short!
		return MRES_IGNORED;
	}
	
	Scmd *cmds = g_cmdManager.cmds;
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!cmds[i].callback)
		{
			// TODO: error?
			Assert(cmds[i].callback);
			continue;
		}
		
		const char *arg = args[1] + 1; // skip chat trigger
		const char *cmdName = cmds[i].hasConsolePrefix ? cmds[i].name + strlen(SCMD_CONSOLE_PREFIX) : cmds[i].name;
		if (stricmp(arg, cmdName) == 0)
		{
			cmds[i].callback(controller, &args);
			if (args[1][0] == SCMD_CHAT_SILENT_TRIGGER)
			{
				// don't send chat message
				return MRES_SUPERCEDE;
			}
		}
	}
	return MRES_IGNORED;
}