/* date = October 12th 2023 8:02 pm */

#ifndef SIMPLECMDS_H
#define SIMPLECMDS_H

#define SCMD_CALLBACK(name) META_RES name(CCSPlayerController *controller, const CCommand *args)

#define SCMD_CONSOLE_PREFIX "kz_"
#define SCMD_CHAT_SILENT_TRIGGER '/'
#define SCMD_CHAT_TRIGGER '!'
#define SCMD_MAX_CMDS 512

namespace scmd
{
	typedef SCMD_CALLBACK(Callback_t);
	bool RegisterCmd(const char *name, Callback_t *callback);
	
	META_RES OnClientCommand(CPlayerSlot &slot, const CCommand &args);
	META_RES OnHost_Say(CCSPlayerController *controller, const CCommand &args);
}


#endif //SIMPLECMDS_H
