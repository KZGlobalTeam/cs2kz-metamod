#pragma once
#include "../kz.h"

class KZHUDService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	void Reset();

	void DrawSpeedPanel();

	void TogglePanel();
	bool IsShowingPanel() { return this->showPanel; }

private:
	bool showPanel{};
};
