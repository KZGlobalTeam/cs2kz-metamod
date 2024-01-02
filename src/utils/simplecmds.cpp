
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
	const char *description;
	scmd::Callback_t *callback;
};

struct ScmdManager
{
	i32 cmdCount;
	Scmd cmds[SCMD_MAX_CMDS];
};

internal ScmdManager g_cmdManager = {};
internal bool g_coreCmdsRegistered = false;

internal SCMD_CALLBACK(Command_KzHelp)
{
	utils::PrintChat(controller, "Look in your console for a list of commands!");
	utils::PrintConsole(controller, "To use these commands, you can type \"bind <key> %s<command name>\" in your console, or type !<command name> or /<command name> in the chat.\nFor example: \"bind 1 kz_cp\" or \"!cp\" or \"/cp\"", SCMD_CONSOLE_PREFIX);
	Scmd *cmds = g_cmdManager.cmds;
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		utils::PrintConsole(controller, "%s: %s",
							cmds[i].name,
							cmds[i].description ? cmds[i].description : "");
	}
	return MRES_SUPERCEDE;
}

internal void RegisterCoreCmds()
{
	g_coreCmdsRegistered = true;
	
	scmd::RegisterCmd("kz_help", Command_KzHelp);
}

bool scmd::RegisterCmd(const char *name, scmd::Callback_t *callback, const char *description/* = nullptr*/)
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
		description,
		callback
	};
	g_cmdManager.cmds[g_cmdManager.cmdCount++] = cmd;
	return true;
}

META_RES scmd::OnClientCommand(CPlayerSlot &slot, const CCommand &args)
{
	if (!g_coreCmdsRegistered)
	{
		RegisterCoreCmds();
	}
	
	META_RES result = MRES_IGNORED;
	if (!g_pEntitySystem)
	{
		return result;
	}
	
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

META_RES scmd::OnDispatchConCommand(ConCommandHandle cmd, const CCommandContext &ctx, const CCommand &args)
{
	if (!g_coreCmdsRegistered)
	{
		RegisterCoreCmds();
	}
	
	META_RES result = MRES_IGNORED;
	if (!g_pEntitySystem)
	{
		return result;
	}
	CPlayerSlot slot = ctx.GetPlayerSlot();

	CCSPlayerController *controller = (CCSPlayerController *)utils::GetController(slot);

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

	CCommand cmdArgs;
	cmdArgs.Tokenize(args[1]);

	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!cmds[i].callback)
		{
			// TODO: error?
			Assert(cmds[i].callback);
			continue;
		}
		
		const char *arg = cmdArgs[0] + 1; // skip chat trigger
		const char *cmdName = cmds[i].hasConsolePrefix ? cmds[i].name + strlen(SCMD_CONSOLE_PREFIX) : cmds[i].name;
		if (stricmp(arg, cmdName) == 0)
		{
			cmds[i].callback(controller, &cmdArgs);
			if (args[1][0] == SCMD_CHAT_SILENT_TRIGGER)
			{
				// don't send chat message
				return MRES_SUPERCEDE;
			}
		}
	}
	return MRES_IGNORED;
}