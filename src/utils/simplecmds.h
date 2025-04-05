/* date = October 12th 2023 8:02 pm */
// This only creates client commands. If you want to create server-only commands, use CON_COMMAND_* macros instead.
// Note: Commands register through this system will have higher priority over console commands.
#ifndef SIMPLECMDS_H
#define SIMPLECMDS_H

class CCSPlayerController;

enum
{
	SCFL_HIDDEN = 0,
	SCFL_CHECKPOINT = 1 << 0,
	SCFL_RECORD = 1 << 1,
	SCFL_JUMPSTATS = 1 << 2,
	SCFL_MEASURE = 1 << 3,
	SCFL_MODESTYLE = 1 << 4,
	SCFL_PREFERENCE = 1 << 5,
	SCFL_RACING = 1 << 6,
	SCFL_REPLAY = 1 << 7,
	SCFL_SAVELOC = 1 << 8,
	SCFL_SPEC = 1 << 9,
	SCFL_STATUS = 1 << 10,
	SCFL_TIMER = 1 << 11,
	SCFL_PLAYER = 1 << 12,
	SCFL_GLOBAL = 1 << 13,
	SCFL_MISC = 1 << 14,
	SCFL_MAP = 1 << 15,
	SCFL_HUD = 1 << 16
};

#define SCMD_CALLBACK(name) META_RES name(CCSPlayerController *controller, const CCommand *args)

#define SCMD_CONSOLE_PREFIX      "kz_"
#define SCMD_CHAT_SILENT_TRIGGER '/'
#define SCMD_CHAT_TRIGGER        '!'
#define SCMD_MAX_CMDS            512

namespace scmd
{
	typedef SCMD_CALLBACK(Callback_t);
	bool RegisterCmd(const char *name, Callback_t *callback, const char *descKey = nullptr, u64 flags = 0);
	bool LinkCmd(const char *name, const char *linkedName);
	bool UnregisterCmd(const char *name);

	META_RES OnClientCommand(CPlayerSlot &slot, const CCommand &args);
	META_RES OnDispatchConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args);
} // namespace scmd

class SCmdRegister
{
public:
	typedef SCMD_CALLBACK(Callback_t);

	SCmdRegister(const char *name, Callback_t *callback, u64 flags = 0, bool hidden = false, const char *descKey = nullptr)
	{
		scmd::RegisterCmd(name, callback, descKey, flags);
	}
};

class SCmdLink
{
public:
	SCmdLink(const char *name, const char *linkedName)
	{
		scmd::LinkCmd(name, linkedName);
	}
};

#define SCMD(name, ...) \
	static_function SCMD_CALLBACK(name##_callback); \
	static_global SCmdRegister name##_reg(#name, name##_callback, ##__VA_ARGS__); \
	static_function SCMD_CALLBACK(name##_callback)

#define SCMD_LINK(name, linkedName) static_global SCmdLink name##_reg(#name, #linkedName);

#endif // SIMPLECMDS_H
