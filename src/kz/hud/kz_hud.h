#pragma once
#include "../kz.h"

class KZHUDService : public KZBaseService
{
	using KZBaseService::KZBaseService;
public:
	void DrawSpeedPanel();
};