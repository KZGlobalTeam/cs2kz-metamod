#pragma once
#include "../kz.h"
#include "../timer/kz_timer.h"
#include "entityhandle.h"
#include "sdk/entity/cparticlesystem.h"

#define KZ_HUD_TIMER_STOPPED_GRACE_TIME 3.0f
#define KZ_HUD_ON_GROUND_THRESHOLD      0.07f

class KZHUDService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool jumpedThisTick {};
	bool fromDuckbug {};
	bool crouchJumping {};
	bool showPanel {};
	f64 timerStoppedTime {};
	f64 currentTimeWhenTimerStopped {};

public:
	virtual void Reset() override;
	static void Init();

	// Draw the panel from a player to a specific target.
	static void DrawPanels(KZPlayer *player, KZPlayer *target);

	void ResetShowPanel();
	void TogglePanel();

	void OnPhysicsSimulate()
	{
		jumpedThisTick = false;
	}

	void OnProcessMovementPost()
	{
		if (this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND)
		{
			fromDuckbug = false;
		}
		if (this->player->GetMoveType() == MOVETYPE_LADDER)
		{
			fromDuckbug = false;
			crouchJumping = false;
		}
	}

	void OnJump(bool modern = false)
	{
		jumpedThisTick = modern ? this->player->IsButtonPressed(IN_JUMP) : true;
	}

	void OnStopTouchGround()
	{
		if (jumpedThisTick)
		{
			fromDuckbug = player->duckBugged;
			crouchJumping = player->GetPlayerPawn()->m_fFlags() & FL_DUCKING || player->GetMoveServices()->m_bDucking();
		}
		else
		{
			fromDuckbug = false;
			crouchJumping = false;
		}
	}

	bool IsShowingPanel()
	{
		return this->showPanel;
	}

	void OnTimerStopped(f64 currentTimeWhenTimerStopped);

	bool ShouldShowTimerAfterStop()
	{
		return g_pKZUtils->GetServerGlobals()->curtime > KZ_HUD_TIMER_STOPPED_GRACE_TIME
			   && g_pKZUtils->GetServerGlobals()->curtime - timerStoppedTime < KZ_HUD_TIMER_STOPPED_GRACE_TIME;
	}

private:
	std::string GetSpeedText(const char *language = KZ_DEFAULT_LANGUAGE);
	std::string GetKeyText(const char *language = KZ_DEFAULT_LANGUAGE);
	std::string GetCheckpointText(const char *language = KZ_DEFAULT_LANGUAGE);
	std::string GetTimerText(const char *language = KZ_DEFAULT_LANGUAGE);

	// Control point mapping:
	// 16 = Color | 17X = Reserved | 17Y = Size | 18X = X offset | 18Y = Y offset

	// 0.03 scale
	// (0, -4.5) for speed 0-99,
	// (?/ -4.5) for 100-999 speed
	// (-0.625/0.625, -4.5) offset for 1000+ speed
	CHandle<CParticleSystem> speedParticles[2];
	// 0.0225 scale, (-0.625/0.625, -7.2) offset
	CHandle<CParticleSystem> prespeedParticles[2];

	// 17X = Input flags
	enum KeyParticleFlags : u8
	{
		Forward = 1 << 0,
		Left = 1 << 1,
		Back = 1 << 2,
		Right = 1 << 3,
		Jump = 1 << 4,
		Duck = 1 << 5
	};

	// 0.05 scale, (0, -7 offset)
	CHandle<CParticleSystem> keysParticle;

	// 0.03 scale, TBD offset
	CHandle<CParticleSystem> timerTextParticles[4];
	CHandle<CParticleSystem> timerDelimiterParticle;

	// Usage: !mhud prespeed/speed/keys/indicator/timer [reset/set <preference> <value>]
	void ParsePreferences(const char *prefs);
	void UpdateParticles();

	// (Pre)Speed args: offsetX1/offsetY1/offsetX2/offsetY2/scale/perfColor/jbColor
	void CheckMHUDSpeedParticles();
	void SetMHUDSpeedParticleVelocity(const Vector &speed, const Vector *prespeed = nullptr);

	// Timer preference args: offsetX/offsetY/tpColor/proColor/pausedColor/stoppedColor
	void CheckMHUDTimerParticles();
	void SetMHUDTimerValue(f64 time);

	// Crouch jump indicator args: offsetX/offsetY/scale/color/verbose
	void CheckMHUDCrouchJumpParticle();
	void SetMHUDCrouchJumpIndicator(bool enable);

	// Key args: offsetX/offsetY/scale/color/overlapColor
	void CheckMHUDKeyParticle();
	void SetMHUDKeys(u64 *keyMask = nullptr);
};
