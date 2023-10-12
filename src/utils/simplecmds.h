/* date = October 12th 2023 8:02 pm */

#ifndef SIMPLECMDS_H
#define SIMPLECMDS_H

#define SCMD_CALLBACK(name) META_RES name(CCSPlayerController *controller, const CCommand *args)
typedef SCMD_CALLBACK(ScmdCallback_t);

#define SCMD_CONSOLE_PREFIX "kz_"
#define SCMD_CHAT_SILENT_TRIGGER '/'
#define SCMD_CHAT_TRIGGER '!'
#define SCMD_MAX_CMDS 512

bool ScmdRegisterCmd(const char *name, ScmdCallback_t *callback);



META_RES Scmd_OnClientCommand(CPlayerSlot &slot, const CCommand &args);
META_RES Scmd_OnHost_Say(CCSPlayerController *controller, const CCommand &args);

#endif //SIMPLECMDS_H
