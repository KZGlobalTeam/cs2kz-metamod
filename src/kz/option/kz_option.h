#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"

class KZOptionService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static_global void InitOptions();
	static_global const char *GetDefaultOptionStr(const char *optionName, const char *defaultValue = "");
	static_global f64 GetDefaultOptionFloat(const char *optionName, f64 defaultValue = 0.0);
	static_global i64 GetDefaultOptionInt(const char *optionName, i64 defaultValue = 0);

private:
	static void LoadDefaultOptions();
};
