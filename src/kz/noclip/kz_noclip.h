#pragma once
#include "../kz.h"

#define KZ_JUST_NOCLIP_TIME 0.05f;

class KZNoclipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	f32 lastNoclipTime {};
	bool inNoclip {};

public:
	static void RegisterCommands();

	void DisableNoclip()
	{
		this->inNoclip = false;
	}

	void ToggleNoclip()
	{
		this->inNoclip = !this->inNoclip;
	}

	bool IsNoclipping()
	{
		return this->inNoclip;
	}

	bool JustNoclipped()
	{
		return g_pKZUtils->GetServerGlobals()->curtime - lastNoclipTime < KZ_JUST_NOCLIP_TIME;
	}

	virtual void Reset() override;
	void HandleMoveCollision();
	void HandleNoclip();

	void OnTeleport(const Vector *newPosition, const QAngle *newAngles, const Vector *newVelocity);
};
