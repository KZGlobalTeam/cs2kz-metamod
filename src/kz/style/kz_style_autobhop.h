#include "kz_style.h"

#define STYLE_NAME       "AutoBhop"
#define STYLE_NAME_SHORT "ABH"

class KZAutoBhopStylePlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);

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

class KZAutoBhopStyleService : public KZStyleService
{
	using KZStyleService::KZStyleService;

public:
	virtual const char *GetStyleName() override
	{
		return "AutoBhop";
	}

	virtual const char *GetStyleShortName() override
	{
		return "ABH";
	}

	virtual const char *GetTweakedConvarValue(const char *name) override;
	virtual void Init() override;
	virtual void Cleanup() override;
	virtual void OnProcessMovement() override;
};
