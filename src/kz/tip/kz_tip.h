#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "utils/ctimer.h"

class KZTipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool showTips{};

public:
	virtual void Reset() override;
	void ToggleTips();
	static_global void InitTips();
	static_global float PrintTips();

private:
	bool ShouldPrintTip();
	void PrintTip();
	static void LoadTips();
};
