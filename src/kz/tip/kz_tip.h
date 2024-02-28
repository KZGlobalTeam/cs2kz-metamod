#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "utils/ctimer.h"

class KZTipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	i32 tipCounter{};
	bool showTips{};

private:
	bool ShouldPrintTip();
	void PrintTip();
	static void LoadTips();

public:
	virtual void Reset() override;
	void ToggleTips();
	static_global void InitTips();
	static_global void PrintTips();
};
