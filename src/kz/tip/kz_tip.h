#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "utils/ctimer.h"
#include "kz/option/kz_option.h"

class KZTipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool showTips;

public:
	virtual void Reset() override;
	void ToggleTips();
	static void InitTips();
	static f64 PrintTips();

private:
	bool ShouldPrintTip();
	void PrintTip();
	static void LoadTips();
	static void ShuffleTips();
};
