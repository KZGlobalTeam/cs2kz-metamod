
#include "common.h"
#include "utils/utils.h"
#include "simplecmds.h"
#include "../kz/kz.h"
#include "../kz/language/kz_language.h"
#include "../kz/option/kz_option.h"
#include "utils/tables.h"

#include "tier0/memdbgon.h"
// private structs
#define SCMD_MAX_NAME_LEN 128

struct Scmd
{
	bool hasConsolePrefix;
	i32 nameLength;
	char name[SCMD_MAX_NAME_LEN];
	scmd::Callback_t *callback;
	char descKey[SCMD_MAX_NAME_LEN];
	u64 flags;
};

// clang-format off
const char* cmdFlagNames[] = {
	"Checkpoint",
	"Record",
	"Jumpstats",
	"Measure",
	"ModeStyle",
	"Preference",
	"Racing",
	"Replay",
	"Saveloc",
	"Spec",
	"Status",
	"Timer",
	"Player",
	"Global",
	"Misc",
	"Map",
	"HUD"
};

static_global const char *columnKeys[] = {
	"Command List Header - Name",
	"Command List Header - Description"
};

// clang-format on

struct ScmdManager
{
	i32 cmdCount;
	Scmd cmds[SCMD_MAX_CMDS];
};

static_global ScmdManager g_cmdManager = {};

static_global void PrintCategoryCommands(KZPlayer *player, i32 category, bool printEmpty)
{
	char tableName[64];
	V_snprintf(tableName, sizeof(tableName), "Command List - %s", cmdFlagNames[category]);
	CUtlString headers[KZ_ARRAYSIZE(columnKeys)];
	for (u32 i = 0; i < KZ_ARRAYSIZE(columnKeys); i++)
	{
		headers[i] = player->languageService->PrepareMessage(columnKeys[i]).c_str();
	}
	Scmd *cmds = g_cmdManager.cmds;
	utils::Table<KZ_ARRAYSIZE(columnKeys)> table(player->languageService->PrepareMessage(tableName).c_str(), headers);

	u32 cmdCount = 0;
	CUtlVector<CUtlString> uniqueCallbacks;
	CUtlVector<i32> callbackRowIndices;
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (cmds[i].flags & (1ull << category))
		{
			i32 existingIndex = uniqueCallbacks.Find(cmds[i].descKey);
			if (existingIndex == -1)
			{
				uniqueCallbacks.AddToTail(cmds[i].descKey);
				callbackRowIndices.AddToTail(cmdCount);
				table.SetRow(cmdCount, cmds[i].name, player->languageService->PrepareMessage(cmds[i].descKey).c_str());
				cmdCount++;
			}
			else
			{
				i32 rowIndex = callbackRowIndices[existingIndex];
				CUtlString newEntry = table.GetEntry(rowIndex).data[0];
				// Remove the trailing space that exists in each column
				newEntry.SetLength(newEntry.Length() - strlen("ᅟ"));
				newEntry.Append("/");
				newEntry.Append(cmds[i].name);
				table.Set(rowIndex, 0, newEntry);
			}
		}
	}
	if (!printEmpty && cmdCount == 0)
	{
		return;
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());

	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

SCMD(kz_help, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->languageService->PrintChat(true, false, "Command Help Response (Chat)");
	player->languageService->PrintConsole(false, false, "Command Help Response (Console)");
	u64 category = 0;
	bool foundCategory {};
	if (args->ArgC() >= 2)
	{
		for (i32 i = 1; i < args->ArgC(); i++)
		{
			for (i32 j = 0; j < KZ_ARRAYSIZE(cmdFlagNames); j++)
			{
				if (!V_stricmp(args->Arg(i), cmdFlagNames[j]))
				{
					PrintCategoryCommands(player, j, true);
					foundCategory = true;
				}
			}
		}
	}

	if (!foundCategory)
	{
		player->languageService->PrintConsole(false, false, "Command Help Response Category Hint (Console)");
		for (i32 i = 0; i < KZ_ARRAYSIZE(cmdFlagNames); i++)
		{
			PrintCategoryCommands(player, i, false);
		}
	}
	return MRES_SUPERCEDE;
}

bool scmd::RegisterCmd(const char *name, scmd::Callback_t *callback, const char *descKey, u64 flags)
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
	if (nameLength >= conPrefixLen && V_strnicmp(name, SCMD_CONSOLE_PREFIX, conPrefixLen) == 0)
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

		if (!V_stricmp(g_cmdManager.cmds[i].name, name))
		{
			// TODO: print warning? error? segfault?
			// Command already exists
			// Assert(0);
			return false;
		}
	}

	// Command name is unique!
	Scmd cmd = {hasConPrefix, nameLength, "", callback, "", flags};
	V_snprintf(cmd.name, SCMD_MAX_NAME_LEN, "%s", name);

	// Check if we want to override the command description.
	if (!descKey)
	{
		V_snprintf(cmd.descKey, SCMD_MAX_NAME_LEN, "Command Description - %s", cmd.name);
	}
	else
	{
		V_snprintf(cmd.descKey, SCMD_MAX_NAME_LEN, "%s", descKey);
	}

	g_cmdManager.cmds[g_cmdManager.cmdCount++] = cmd;

	return true;
}

bool scmd::LinkCmd(const char *name, const char *linkedName)
{
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!V_stricmp(g_cmdManager.cmds[i].name, linkedName))
		{
			return scmd::RegisterCmd(name, g_cmdManager.cmds[i].callback, g_cmdManager.cmds[i].descKey, g_cmdManager.cmds[i].flags);
		}
	}
	return false;
}

bool scmd::UnregisterCmd(const char *name)
{
	i32 indexToDelete = -1;
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!V_stricmp(g_cmdManager.cmds[i].name, name))
		{
			indexToDelete = i;
			break;
		}
	}
	if (indexToDelete != -1)
	{
		for (i32 i = indexToDelete; i < g_cmdManager.cmdCount; i++)
		{
			g_cmdManager.cmds[i] = g_cmdManager.cmds[i + 1];
		}
		g_cmdManager.cmdCount--;
		return true;
	}
	return false;
}

META_RES scmd::OnClientCommand(CPlayerSlot &slot, const CCommand &args)
{
	META_RES result = MRES_IGNORED;
	if (!GameEntitySystem())
	{
		return result;
	}

	CCSPlayerController *controller = (CCSPlayerController *)GameEntitySystem()->GetEntityInstance(CEntityIndex((i32)slot.Get() + 1));

	if (!controller || !g_pKZPlayerManager->ToPlayer(controller))
	{
		return MRES_IGNORED;
	}

	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		if (!g_cmdManager.cmds[i].callback)
		{
			// TODO: error?
			Assert(g_cmdManager.cmds[i].callback);
			continue;
		}

		if (!V_stricmp(g_cmdManager.cmds[i].name, args[0]))
		{
			result = g_cmdManager.cmds[i].callback(controller, &args);
			if (result == MRES_SUPERCEDE)
			{
				return result;
			}
		}
	}
	return result;
}

META_RES scmd::OnDispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	META_RES result = MRES_IGNORED;
	if (!GameEntitySystem())
	{
		return result;
	}
	CPlayerSlot slot = ctx.GetPlayerSlot();

	CCSPlayerController *controller = (CCSPlayerController *)utils::GetController(slot);

	if (!cmd.IsValidRef() || !controller || !g_pKZPlayerManager->ToPlayer(controller))
	{
		return MRES_IGNORED;
	}
	const char *commandName = cmd.GetName();

	if (!V_stricmp(commandName, "say") || !V_stricmp(commandName, "say_team"))
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
			if (!V_stricmp(arg, cmdName))
			{
				META_RES result = cmds[i].callback(controller, &cmdArgs);
				if (args[1][0] == SCMD_CHAT_SILENT_TRIGGER || result == MRES_SUPERCEDE)
				{
					// don't send chat message
					return MRES_SUPERCEDE;
				}
			}
		}
	}
	else // Are we overriding a console command?
	{
		for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
		{
			Scmd *cmds = g_cmdManager.cmds;
			if (!cmds[i].callback)
			{
				// TODO: error?
				Assert(cmds[i].callback);
				continue;
			}

			const char *cmdName = cmds[i].hasConsolePrefix ? cmds[i].name + strlen(SCMD_CONSOLE_PREFIX) : cmds[i].name;
			if (!V_stricmp(commandName, cmdName))
			{
				META_RES result = g_cmdManager.cmds[i].callback(controller, &args);
				if (result == MRES_SUPERCEDE)
				{
					return result;
				}
			}
		}
	}

	return MRES_IGNORED;
}
