#pragma once

#include "common.h"

class KZPlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);
	void AddonInit();
	bool IsAddonMounted();
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

	virtual void *OnMetamodQuery(const char *iface, int *ret) override;

	bool simulatingPhysics = false;
	CGlobalVars serverGlobals;
	bool unloading = false;

private:
	void UpdateSelfMD5();
	char md5[33];

public:
	const char *GetMD5()
	{
		return md5;
	}
};

extern KZPlugin g_KZPlugin;
