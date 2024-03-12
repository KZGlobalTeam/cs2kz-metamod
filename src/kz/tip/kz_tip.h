#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "utils/ctimer.h"

#define KZ_DEFAULT_LANGUAGE "en"

class KZTipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool showTips;
	//TODO: add other languages
	const char *language = KZ_DEFAULT_LANGUAGE;

public:
	virtual void Reset() override;
	void ToggleTips();
	static_global void InitTips();
	static_global f64 PrintTips();

private:
	bool ShouldPrintTip();
	void PrintTip();
	static void ShuffleTips();
	static void LoadTips();
};
