
#include "common.h"
#include "utils/utils.h"
#include "simplecmds.h"
#include "../kz/kz.h"
#include "../kz/language/kz_language.h"

#include <algorithm>

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

// clang-format on

struct ScmdManager
{
	i32 cmdCount;
	Scmd cmds[SCMD_MAX_CMDS];
	u32 revision;
};

static_global ScmdManager g_cmdManager = {};

i32 scmd::GetCategoryCount()
{
	return KZ_ARRAYSIZE(cmdFlagNames);
}

const char *scmd::GetCategoryName(i32 category)
{
	return category >= 0 && category < scmd::GetCategoryCount() ? cmdFlagNames[category] : nullptr;
}

const std::vector<scmd::CommandInfo> &scmd::GetCategoryCommands(i32 category, bool chatNames)
{
	static_persist const std::vector<CommandInfo> empty;
	if (!scmd::GetCategoryName(category))
	{
		return empty;
	}

	struct CachedCommands
	{
		u32 revision {};
		std::vector<CommandInfo> commands;
	};

	static_persist CachedCommands cache[KZ_ARRAYSIZE(cmdFlagNames)][2];
	CachedCommands &cached = cache[category][chatNames ? 1 : 0];
	if (cached.revision == g_cmdManager.revision)
	{
		return cached.commands;
	}
	auto &result = cached.commands;
	result.clear();
	for (i32 i = 0; i < g_cmdManager.cmdCount; i++)
	{
		const Scmd &cmd = g_cmdManager.cmds[i];
		if (!(cmd.flags & (1ull << category)))
		{
			continue;
		}
		const std::string name =
			chatNames ? std::string(1, SCMD_CHAT_TRIGGER) + (cmd.hasConsolePrefix ? cmd.name + strlen(SCMD_CONSOLE_PREFIX) : cmd.name) : cmd.name;
		auto entry = std::find_if(result.begin(), result.end(), [&](const CommandInfo &row) { return row.descriptionKey == cmd.descKey; });
		if (entry == result.end())
		{
			result.push_back({name, cmd.descKey});
		}
		else
		{
			entry->names += (chatNames ? " / " : "/") + name;
		}
	}
	cached.revision = g_cmdManager.revision;
	return result;
}

#define SCMD_COOLDOWN 0.2f

// Blocks commands from clients who aren't fully in-game yet, and enforces a cooldown between commands.
// Silent when blocking for not-in-game (client can't receive chat yet); prints the remaining wait when on cooldown.
static_global bool CanRunCommand(KZPlayer *player, u64 flags)
{
	if (!player->IsInGame())
	{
		return false;
	}

	// Certain commands are exempt from the cooldown, since they're expected to be spammed.
	if (flags & (SCFL_CHECKPOINT | SCFL_MEASURE | SCFL_PREFERENCE))
	{
		return true;
	}

	f32 curtime = g_pKZUtils->GetServerGlobals()->curtime;
	f32 remaining = SCMD_COOLDOWN - (curtime - player->lastCommandTime);
	if (remaining > 0.0f)
	{
		player->languageService->PrintChat(true, false, "Command Cooldown (Time Remaining)", remaining);
		return false;
	}

	player->lastCommandTime = curtime;
	return true;
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
	g_cmdManager.revision++;

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
		for (i32 i = indexToDelete; i < g_cmdManager.cmdCount - 1; i++)
		{
			g_cmdManager.cmds[i] = g_cmdManager.cmds[i + 1];
		}
		g_cmdManager.cmdCount--;
		g_cmdManager.revision++;
		return true;
	}
	return false;
}

bool scmd::OnClientCommand(CPlayerSlot &slot, const CCommand &args)
{
	bool result = false;
	if (!GameEntitySystem())
	{
		return result;
	}

	CCSPlayerController *controller = (CCSPlayerController *)GameEntitySystem()->GetEntityInstance(CEntityIndex((i32)slot.Get() + 1));

	KZPlayer *player = controller ? g_pKZPlayerManager->ToPlayer(controller) : nullptr;
	if (!controller || !player)
	{
		return false;
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
			if (!CanRunCommand(player, g_cmdManager.cmds[i].flags))
			{
				return true;
			}
			result = g_cmdManager.cmds[i].callback(controller, &args);
			if (result)
			{
				return result;
			}
		}
	}
	return result;
}

bool scmd::OnDispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	bool result = false;
	if (!GameEntitySystem())
	{
		return result;
	}
	CPlayerSlot slot = ctx.GetPlayerSlot();

	CCSPlayerController *controller = (CCSPlayerController *)utils::GetController(slot);

	KZPlayer *player = controller ? g_pKZPlayerManager->ToPlayer(controller) : nullptr;
	if (!cmd.IsValidRef() || !controller || !player)
	{
		return false;
	}
	const char *commandName = cmd.GetName();

	if (!V_stricmp(commandName, "say") || !V_stricmp(commandName, "say_team"))
	{
		// A client that isn't fully in-game yet can't legitimately chat at all, command or not.
		if (!player->IsInGame())
		{
			return true;
		}

		if (args.ArgC() < 2)
		{
			// no argument somehow
			return false;
		}

		if (args[1][0] != SCMD_CHAT_TRIGGER && args[1][0] != SCMD_CHAT_SILENT_TRIGGER)
		{
			// no chat command trigger
			return false;
		}

		i32 argLen = strlen(args[1]);
		if (argLen < 1)
		{
			// arg is too short!
			return false;
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
				if (!CanRunCommand(player, cmds[i].flags))
				{
					return true;
				}
				bool result = cmds[i].callback(controller, &cmdArgs);
				if (args[1][0] == SCMD_CHAT_SILENT_TRIGGER || result)
				{
					// don't send chat message
					return true;
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
				if (!CanRunCommand(player, cmds[i].flags))
				{
					return true;
				}
				bool result = g_cmdManager.cmds[i].callback(controller, &args);
				if (result)
				{
					return result;
				}
			}
		}
	}

	return false;
}
