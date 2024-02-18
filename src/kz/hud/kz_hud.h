#pragma once
#include "../kz.h"

class KZHUDService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool showPanel{};

public:
	virtual void Reset() override;

	void DrawSpeedPanel();

	void TogglePanel();
	bool IsShowingPanel() { return this->showPanel; }

};
