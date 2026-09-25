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
		// Preferences can also arrive through an import, so hold them to the server's bounds here too.
		i64 fov = this->player->optionService->GetPreferenceInt("fov", this->GetDefaultFOV());
		return (u32)Clamp(fov, (i64)GetMinFOV(), (i64)GetMaxFOV());
	}

	void OnPhysicsSimulate();
};
