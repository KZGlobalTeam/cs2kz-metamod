#pragma once
#include "kz/kz.h"
#include "kz/option/kz_option.h"

class KZFOVService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static u32 GetMinFOV()
	{
		return KZOptionService::GetOptionInt("minFOV", 80);
	}

	static u32 GetMaxFOV()
	{
		return KZOptionService::GetOptionInt("maxFOV", 130);
	}

	static u32 GetDefaultFOV()
	{
		return KZOptionService::GetOptionInt("defaultFOV", 90);
	}

	void SetFOV(u32 newFOV)
	{
		this->player->optionService->SetPreferenceInt("fov", newFOV);
	}

	u32 GetFOV()
	{
		return this->player->optionService->GetPreferenceInt("fov", this->GetMinFOV());
	}

	void OnPhysicsSimulate();
};
