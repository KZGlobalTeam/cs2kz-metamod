#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"

struct KZServerConfig
{
	const char *defaultMode = "CKZ";
	const char *defaultStyle = "NRM";
	const char *defaultLanguage = "en";
};

extern KZServerConfig *g_pKZServerConfig;

class KZOptionService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	virtual void Reset() override;
	static_global void InitOptions();

private:
	static void LoadDefaultOptions();
};
