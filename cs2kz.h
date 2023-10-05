#pragma once

#include <ISmmPlugin.h>
#include <igameevents.h>
#include <iplayerinfo.h>
#include <sh_vector.h>


class KZPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AllPluginsLoaded();
public:
	const char *GetAuthor();
	const char *GetName();
	const char *GetDescription();
	const char *GetURL();
	const char *GetLicense();
	const char *GetVersion();
	const char *GetDate();
	const char *GetLogTag();
};

extern KZPlugin g_KZPlugin;

void Hook_ClientCommand(CPlayerSlot slot, const CCommand& args);
void Hook_GameFrame(bool simulating, bool bFirstTick, bool bLastTick);
float Hook_ProcessUsercmds(CPlayerSlot slot, bf_read *buf, int numcmds, bool ignore, bool paused);

PLUGIN_GLOBALVARS();