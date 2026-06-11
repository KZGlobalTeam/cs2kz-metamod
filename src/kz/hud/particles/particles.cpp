#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "sdk/entity/cparticlesystem.h"
#include "entitykeyvalues.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "utils/gamesystem.h"

#include "tier0/memdbgon.h"

// === Particle asset paths ============================================================

#define PARTICLE_NUMBERS_PATTERN     "particles/velo/velo_overlay_large_%s_%s.vpcf"
#define PARTICLE_TIMER_DELIM_PATTERN "particles/timer_delimiter/timer_delimiter_%s_%s.vpcf"
#define PARTICLE_INPUTS_PATTERN      "particles/inputs/inputs_%s_%s.vpcf"

// === Layout constants ================================================================
#define MHUD_SPEED_X_LEFT       -0.625f
#define MHUD_SPEED_X_CENTER     0.0f
#define MHUD_SPEED_X_RIGHT_3DIG 0.3125f
#define MHUD_SPEED_X_RIGHT_4DIG 0.625f

#define MHUD_TIMER_PAIR_STEP   1.6f // full spacing between consecutive digit-pair particles
#define MHUD_TIMER_DELIM_COLON 0
#define MHUD_TIMER_DELIM_DOT   1

// === Default preferences =============================================================

static_global constexpr const char *AVAILABLE_FONTS[] = {"lato", "verdana"};
static_global constexpr const char *fontList = "Lato/Verdana";

#define MHUD_DEF_SPEED_OFFSET_X    0.0f
#define MHUD_DEF_SPEED_OFFSET_Y    -4.5f
#define MHUD_DEF_SPEED_SCALE       0.04f
#define MHUD_DEF_PRESPEED_OFFSET_X 0.0f
#define MHUD_DEF_PRESPEED_OFFSET_Y -7.2f
#define MHUD_DEF_PRESPEED_SCALE    0.03f
#define MHUD_DEF_TIMER_OFFSET_X    0.0f
#define MHUD_DEF_TIMER_OFFSET_Y    -20.0f
#define MHUD_DEF_TIMER_SCALE       0.03f
#define MHUD_DEF_KEYS_OFFSET_X     0.0f
#define MHUD_DEF_KEYS_OFFSET_Y     -6.0f
#define MHUD_DEF_KEYS_SCALE        0.075f

static_global const Color MHUD_DEF_BASE_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_PERF_COLOR(0x40, 0xFF, 0x40, 0xFF);
static_global const Color MHUD_DEF_JUMPBUG_COLOR(0xFF, 0xFF, 0x20, 0xFF);
static_global const Color MHUD_DEF_CJ_COLOR(0x71, 0xEE, 0xB8, 0xFF);
static_global const Color MHUD_DEF_TIMER_TP_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_TIMER_PRO_COLOR(0x5F, 0x99, 0xD9, 0xFF);
static_global const Color MHUD_DEF_TIMER_PAUSED_COLOR(0xFF, 0xFF, 0x00, 0xFF);
static_global const Color MHUD_DEF_TIMER_STOPPED_COLOR(0xFF, 0xA0, 0xA0, 0xFF);
static_global const Color MHUD_DEF_KEYS_OVERLAP_COLOR(0xFF, 0x40, 0x40, 0xFF);

// === Helpers ========================================================================

static_function i64 PackColor(const Color &c)
{
	return ((i64)c.r() << 24) | ((i64)c.g() << 16) | ((i64)c.b() << 8) | (i64)c.a();
}

static_function Color UnpackColor(i64 packed)
{
	return Color((u8)((packed >> 24) & 0xFF), (u8)((packed >> 16) & 0xFF), (u8)((packed >> 8) & 0xFF), (u8)(packed & 0xFF));
}

Color KZHUDService::GetMHUDColorPref(const char *name, const Color &defaultColor)
{
	i64 packed = this->player->optionService->GetPreferenceInt(name, PackColor(defaultColor));
	return UnpackColor(packed);
}

static_function void BuildParticlePath(char *buf, size_t bufSize, const char *pattern, const char *font, bool outline)
{
	V_snprintf(buf, bufSize, pattern, font, outline ? "outline" : "no_outline");
}

void KZHUDService::PrecacheParticles(IEntityResourceManifest *pResourceManifest)
{
	// Build path for all possible particle combinations.
	char particlePath[256];
	for (const char *font : AVAILABLE_FONTS)
	{
		for (bool outline : {false, true})
		{
			BuildParticlePath(particlePath, sizeof(particlePath), PARTICLE_NUMBERS_PATTERN, font, outline);
			pResourceManifest->AddResource(particlePath);
			BuildParticlePath(particlePath, sizeof(particlePath), PARTICLE_TIMER_DELIM_PATTERN, font, outline);
			pResourceManifest->AddResource(particlePath);
			BuildParticlePath(particlePath, sizeof(particlePath), PARTICLE_INPUTS_PATTERN, font, outline);
			pResourceManifest->AddResource(particlePath);
		}
	}
}

// === Particle creation ==============================================================

static_function CParticleSystem *CreateMHUDParticle(const char *particleName, const Color &color, const f32 sequence, const f32 size,
													const f32 offsetX, const f32 offsetY)
{
	CParticleSystem *particleSystem = utils::CreateEntityByName<CParticleSystem>("info_particle_system");
	if (!particleSystem)
	{
		Warning("[KZ] Failed to create particle system for MHUD!\n");
		return nullptr;
	}
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("effect_name", particleName);
	pKeyValues->SetBool("start_active", true);
	pKeyValues->SetInt("tint_cp", 16);
	pKeyValues->SetColor("tint_cp_color", color);
	pKeyValues->SetInt("data_cp", 17);
	pKeyValues->SetVector("data_cp_value", Vector(sequence, size, 1.0f));
	// Mark this as a custom plugin particle so kz_quiet's CheckTransmit
	// hook can apply per-owner visibility filtering.
	particleSystem->m_iTeamNum(CUSTOM_PARTICLE_SYSTEM_TEAM);
	particleSystem->DispatchSpawn(pKeyValues);
	particleSystem->SetControlPointValue(18, Vector(offsetX, offsetY, 1.0f));
	return particleSystem;
}

bool KZHUDService::OwnsParticle(const CEntityHandle &handle) const
{
	return handle == this->speedParticles[0] || handle == this->speedParticles[1] || handle == this->prespeedParticles[0]
		   || handle == this->prespeedParticles[1] || handle == this->timerTextParticles[0] || handle == this->timerTextParticles[1]
		   || handle == this->timerTextParticles[2] || handle == this->timerTextParticles[3] || handle == this->timerDelimiterParticles[0]
		   || handle == this->timerDelimiterParticles[1] || handle == this->timerDelimiterParticles[2] || handle == this->keysParticle;
}

// === Cleanup ========================================================================

static_function void DestroyHandle(CHandle<CParticleSystem> &handle)
{
	CParticleSystem *p = handle.Get();
	if (p)
	{
		g_pKZUtils->RemoveEntity(p);
	}
	handle = {};
}

void KZHUDService::DestroyAllParticles()
{
	DestroyHandle(this->speedParticles[0]);
	DestroyHandle(this->speedParticles[1]);
	DestroyHandle(this->prespeedParticles[0]);
	DestroyHandle(this->prespeedParticles[1]);
	DestroyHandle(this->keysParticle);
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerTextParticles); i++)
	{
		DestroyHandle(this->timerTextParticles[i]);
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); i++)
	{
		DestroyHandle(this->timerDelimiterParticles[i]);
	}
}

void KZHUDService::OnClientDisconnect()
{
	this->DestroyAllParticles();
}

// === Preferences ====================================================================

bool KZHUDService::IsMHUDSpeedEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudSpeedEnabled", false);
}

bool KZHUDService::IsMHUDTimerEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudTimerEnabled", false);
}

bool KZHUDService::IsMHUDKeysEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysEnabled", false);
}

bool KZHUDService::IsMHUDTimerDetailed()
{
	return this->player->optionService->GetPreferenceBool("mhudTimerDetailed", true);
}

bool KZHUDService::IsMHUDKeysOverlapEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudKeysOverlap", true);
}

bool KZHUDService::IsMHUDPrespeedEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudPrespeedEnabled", false);
}

bool KZHUDService::IsMHUDOutlineEnabled()
{
	return this->player->optionService->GetPreferenceBool("mhudOutline", true);
}

// === Speed + prespeed ===============================================================

// Set both digit-pair particles for a value 0..9999.
// `transmitFlags` (out) receives whether each particle should be transmitted.
static_function void LayoutDigitPair(i32 value, f32 size, f32 baseOffsetX, f32 baseOffsetY, CParticleSystem *p0, CParticleSystem *p1,
									 bool transmit[2])
{
	transmit[0] = true;
	transmit[1] = true;
	if (value >= 1000)
	{
		i32 hi = value / 100;
		i32 lo = value % 100;
		if (lo < 10)
		{
			lo += 100; // Force the "0X" texture variant.
		}
		p0->SetControlPointValue(17, Vector((f32)hi, size, 0.0f));
		p0->SetControlPointValue(18, Vector(baseOffsetX + MHUD_SPEED_X_LEFT, baseOffsetY, 1.0f));
		p1->SetControlPointValue(17, Vector((f32)lo, size, 0.0f));
		p1->SetControlPointValue(18, Vector(baseOffsetX + MHUD_SPEED_X_RIGHT_4DIG, baseOffsetY, 1.0f));
	}
	else if (value >= 100)
	{
		i32 hi = value / 100;
		i32 lo = value % 100;
		if (lo < 10)
		{
			lo += 100;
		}
		p0->SetControlPointValue(17, Vector((f32)hi, size, 0.0f));
		p0->SetControlPointValue(18, Vector(baseOffsetX + MHUD_SPEED_X_LEFT, baseOffsetY, 1.0f));
		p1->SetControlPointValue(17, Vector((f32)lo, size, 0.0f));
		p1->SetControlPointValue(18, Vector(baseOffsetX + MHUD_SPEED_X_RIGHT_3DIG, baseOffsetY, 1.0f));
	}
	else
	{
		p0->SetControlPointValue(17, Vector((f32)value, size, 0.0f));
		p0->SetControlPointValue(18, Vector(baseOffsetX + MHUD_SPEED_X_CENTER, baseOffsetY, 1.0f));
		transmit[1] = false;
	}
}

static_function void SetParticleTint(CParticleSystem *particle, const Color &color)
{
	if (!particle)
	{
		return;
	}
	particle->SetControlPointValue(16, Vector((f32)color.r(), (f32)color.g(), (f32)color.b()));
}

void KZHUDService::UpdateMHUDSpeed()
{
	bool speedEnabled = this->IsMHUDSpeedEnabled();
	bool prespeedEnabled = this->IsMHUDPrespeedEnabled();

	// Tear down disabled elements.
	if (!speedEnabled)
	{
		if (this->speedParticles[0])
		{
			DestroyHandle(this->speedParticles[0]);
		}
		if (this->speedParticles[1])
		{
			DestroyHandle(this->speedParticles[1]);
		}
	}
	if (!prespeedEnabled)
	{
		if (this->prespeedParticles[0])
		{
			DestroyHandle(this->prespeedParticles[0]);
		}
		if (this->prespeedParticles[1])
		{
			DestroyHandle(this->prespeedParticles[1]);
		}
	}
	if (!speedEnabled && !prespeedEnabled)
	{
		return;
	}

	const Color baseColor = this->GetMHUDColorPref("mhudSpeedColor", MHUD_DEF_BASE_COLOR);
	const Color prespeedBaseColor = this->GetMHUDColorPref("mhudPrespeedColor", MHUD_DEF_BASE_COLOR);
	const f32 speedOffsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedOffsetX", MHUD_DEF_SPEED_OFFSET_X);
	const f32 speedOffsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedOffsetY", MHUD_DEF_SPEED_OFFSET_Y);
	const f32 speedScale = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedScale", MHUD_DEF_SPEED_SCALE);
	const f32 prespeedOffsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedOffsetX", MHUD_DEF_PRESPEED_OFFSET_X);
	const f32 prespeedOffsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedOffsetY", MHUD_DEF_PRESPEED_OFFSET_Y);
	const f32 prespeedScale = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedScale", MHUD_DEF_PRESPEED_SCALE);

	// Lazy-create.
	const char *font = this->player->optionService->GetPreferenceStr("mhudFont", AVAILABLE_FONTS[0]);
	char fontLower[64];
	V_strncpy(fontLower, font, sizeof(fontLower));
	V_strlower(fontLower);
	bool outline = this->IsMHUDOutlineEnabled();
	char numbersPath[256];
	BuildParticlePath(numbersPath, sizeof(numbersPath), PARTICLE_NUMBERS_PATTERN, fontLower, outline);
	if (speedEnabled)
	{
		if (!this->speedParticles[0])
		{
			this->speedParticles[0] = CreateMHUDParticle(numbersPath, baseColor, 0.0f, speedScale, speedOffsetX, speedOffsetY);
		}
		if (!this->speedParticles[1])
		{
			this->speedParticles[1] = CreateMHUDParticle(numbersPath, baseColor, 0.0f, speedScale, speedOffsetX, speedOffsetY);
		}
	}
	if (prespeedEnabled)
	{
		if (!this->prespeedParticles[0])
		{
			this->prespeedParticles[0] = CreateMHUDParticle(numbersPath, prespeedBaseColor, 0.0f, prespeedScale, prespeedOffsetX, prespeedOffsetY);
		}
		if (!this->prespeedParticles[1])
		{
			this->prespeedParticles[1] = CreateMHUDParticle(numbersPath, prespeedBaseColor, 0.0f, prespeedScale, prespeedOffsetX, prespeedOffsetY);
		}
	}

	Vector velocity, baseVelocity;
	this->player->GetVelocity(&velocity);
	this->player->GetBaseVelocity(&baseVelocity);
	velocity += baseVelocity;

	bool useTakeoff = !((this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND
						 && g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTime > KZ_HUD_ON_GROUND_THRESHOLD)
						|| (this->player->GetPlayerPawn()->m_MoveType() == MOVETYPE_LADDER && !player->IsButtonPressed(IN_JUMP)));

	this->SetMHUDSpeedParticleVelocity(velocity, useTakeoff ? &this->player->takeoffVelocity : nullptr);

	// === Color selection ==========================================================
	// Current speed: CJ tint only (no perf indicator — prespeed handles that).
	// Prespeed: perf/jumpbug tint only (no CJ indicator — speed handles that).
	const Color perfColor = this->GetMHUDColorPref("mhudPrespeedPerfColor", MHUD_DEF_PERF_COLOR);
	const Color jumpbugColor = this->GetMHUDColorPref("mhudPrespeedJumpbugColor", MHUD_DEF_JUMPBUG_COLOR);
	const Color cjColor = this->GetMHUDColorPref("mhudSpeedCjColor", MHUD_DEF_CJ_COLOR);

	bool perfing = this->player->IsPerfing() && !this->player->possibleLadderHop && !this->player->takeoffFromLadder;

	const Color speedColor = useTakeoff && this->crouchJumping ? cjColor : baseColor;
	SetParticleTint(this->speedParticles[0].Get(), speedColor);
	SetParticleTint(this->speedParticles[1].Get(), speedColor);

	const Color prespeedColor = perfing ? (this->fromDuckbug ? jumpbugColor : perfColor) : prespeedBaseColor;
	SetParticleTint(this->prespeedParticles[0].Get(), prespeedColor);
	SetParticleTint(this->prespeedParticles[1].Get(), prespeedColor);
}

void KZHUDService::SetMHUDSpeedParticleVelocity(const Vector &speed, const Vector *prespeed)
{
	const f32 speedOffsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedOffsetX", MHUD_DEF_SPEED_OFFSET_X);
	const f32 speedOffsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedOffsetY", MHUD_DEF_SPEED_OFFSET_Y);
	const f32 speedScale = (f32)this->player->optionService->GetPreferenceFloat("mhudSpeedScale", MHUD_DEF_SPEED_SCALE);
	const f32 prespeedOffsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedOffsetX", MHUD_DEF_PRESPEED_OFFSET_X);
	const f32 prespeedOffsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedOffsetY", MHUD_DEF_PRESPEED_OFFSET_Y);
	const f32 prespeedScale = (f32)this->player->optionService->GetPreferenceFloat("mhudPrespeedScale", MHUD_DEF_PRESPEED_SCALE);

	if (this->speedParticles[0] && this->speedParticles[1])
	{
		i32 speedRounded = RoundFloatToInt(speed.Length2D());
		if (speedRounded < 0)
		{
			speedRounded = 0;
		}
		if (speedRounded > 9999)
		{
			speedRounded = 9999;
		}
		bool transmit[2];
		LayoutDigitPair(speedRounded, speedScale, speedOffsetX, speedOffsetY, this->speedParticles[0].Get(), this->speedParticles[1].Get(), transmit);
		this->speedParticles[0].Get()->Start();
		if (transmit[1])
		{
			this->speedParticles[1].Get()->Start();
		}
		else
		{
			this->speedParticles[1].Get()->Destroy();
		}
	}

	if (this->prespeedParticles[0] && this->prespeedParticles[1])
	{
		if (prespeed)
		{
			i32 prespeedRounded = RoundFloatToInt(prespeed->Length2D());
			if (prespeedRounded < 0)
			{
				prespeedRounded = 0;
			}
			if (prespeedRounded > 9999)
			{
				prespeedRounded = 9999;
			}
			bool transmit[2];
			LayoutDigitPair(prespeedRounded, prespeedScale, prespeedOffsetX, prespeedOffsetY, this->prespeedParticles[0].Get(),
							this->prespeedParticles[1].Get(), transmit);
			this->prespeedParticles[0].Get()->Start();
			if (transmit[1])
			{
				this->prespeedParticles[1].Get()->Start();
			}
			else
			{
				this->prespeedParticles[1].Get()->Destroy();
			}
		}
		else
		{
			this->prespeedParticles[0].Get()->Destroy();
			this->prespeedParticles[1].Get()->Destroy();
		}
	}
}

// === Timer ==========================================================================

void KZHUDService::CheckMHUDTimerParticles()
{
	bool enabled = this->IsMHUDTimerEnabled();

	if (!enabled)
	{
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerTextParticles); i++)
		{
			if (this->timerTextParticles[i])
			{
				DestroyHandle(this->timerTextParticles[i]);
			}
		}
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); i++)
		{
			if (this->timerDelimiterParticles[i])
			{
				DestroyHandle(this->timerDelimiterParticles[i]);
			}
		}
		return;
	}

	const Color tpColor = this->GetMHUDColorPref("mhudTimerTpColor", MHUD_DEF_TIMER_TP_COLOR);
	const f32 offsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudTimerOffsetY", MHUD_DEF_TIMER_OFFSET_Y);
	const f32 scale = (f32)this->player->optionService->GetPreferenceFloat("mhudTimerScale", MHUD_DEF_TIMER_SCALE);
	char numbersPath[256], delimPath[256], fontLower[64];
	const char *font = this->player->optionService->GetPreferenceStr("mhudFont", AVAILABLE_FONTS[0]);
	V_strncpy(fontLower, font, sizeof(fontLower));
	V_strlower(fontLower);
	bool outline = this->IsMHUDOutlineEnabled();
	BuildParticlePath(numbersPath, sizeof(numbersPath), PARTICLE_NUMBERS_PATTERN, fontLower, outline);
	BuildParticlePath(delimPath, sizeof(delimPath), PARTICLE_TIMER_DELIM_PATTERN, fontLower, outline);

	if (!this->timerTextParticles[0])
	{
		this->timerTextParticles[0] = CreateMHUDParticle(numbersPath, tpColor, 0.0f, scale, 0.0f, offsetY);
	}
	if (!this->timerTextParticles[1])
	{
		this->timerTextParticles[1] = CreateMHUDParticle(numbersPath, tpColor, 0.0f, scale, 0.0f, offsetY);
	}
	if (!this->timerTextParticles[2])
	{
		this->timerTextParticles[2] = CreateMHUDParticle(numbersPath, tpColor, 0.0f, scale, 0.0f, offsetY);
	}
	if (!this->timerTextParticles[3])
	{
		this->timerTextParticles[3] = CreateMHUDParticle(numbersPath, tpColor, 0.0f, scale, 0.0f, offsetY);
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); i++)
	{
		if (!this->timerDelimiterParticles[i])
		{
			this->timerDelimiterParticles[i] = CreateMHUDParticle(delimPath, tpColor, 0.0f, scale, 0.0f, offsetY);
		}
	}
}

void KZHUDService::UpdateMHUDTimer()
{
	this->CheckMHUDTimerParticles();
	if (!this->timerTextParticles[0])
	{
		return; // Disabled or creation failed.
	}

	// Determine whether to display anything at all.
	bool timerRunning = this->player->timerService->GetTimerRunning();
	bool showAfterStop = this->ShouldShowTimerAfterStop();
	if (!timerRunning && !showAfterStop)
	{
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerTextParticles); i++)
		{
			CParticleSystem *p = this->timerTextParticles[i].Get();
			if (p)
			{
				p->Destroy();
			}
		}
		for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); i++)
		{
			if (this->timerDelimiterParticles[i])
			{
				this->timerDelimiterParticles[i].Get()->Destroy();
			}
		}
		return;
	}

	f64 time = timerRunning ? this->player->timerService->GetTime() : this->currentTimeWhenTimerStopped;
	if (time < 0.0)
	{
		time = 0.0;
	}

	bool detailed = this->IsMHUDTimerDetailed();
	bool paused = this->player->timerService->GetPaused();

	// Choose layout.
	i32 totalSeconds = (i32)time;
	i32 hours = totalSeconds / 3600;
	i32 minutes = (totalSeconds / 60) % 60;
	i32 seconds = totalSeconds % 60;
	i32 centis = (i32)(time * 100.0) % 100;
	if (centis < 0)
	{
		centis = 0;
	}

	// Four layouts (all digit pairs are double-digit, i.e. +100 when < 10):
	//   hours>0 && detailed : "HH:MM:SS.CC"  (2 colons + 1 dot)
	//   hours>0             : "HH:MM:SS"     (2 colons)
	//   detailed && <1h     : "MM:SS.CC"     (1 colon + 1 dot)
	//   else                : "MM:SS"        (1 colon)
	i32 digit[4] = {0, 0, 0, 0};
	bool digitVisible[4] = {false, false, false, false};
	// delimTypes[d] is the type of delimiter d (COLON or DOT); numDelimiters <= 3.
	i32 delimTypes[3] = {-1, -1, -1};
	i32 numDelimiters = 0;

	if (hours > 0 && detailed)
	{
		// HH:MM:SS.CC
		digit[0] = hours > 9 ? (hours > 99 ? 99 : hours) : hours + 100;
		digit[1] = (minutes < 10) ? minutes + 100 : minutes;
		digit[2] = (seconds < 10) ? seconds + 100 : seconds;
		digit[3] = (centis < 10) ? centis + 100 : centis;
		digitVisible[0] = digitVisible[1] = digitVisible[2] = digitVisible[3] = true;
		delimTypes[0] = MHUD_TIMER_DELIM_COLON;
		delimTypes[1] = MHUD_TIMER_DELIM_COLON;
		delimTypes[2] = MHUD_TIMER_DELIM_DOT;
		numDelimiters = 3;
	}
	else if (hours > 0)
	{
		// HH:MM:SS
		digit[0] = hours > 9 ? (hours > 99 ? 99 : hours) : hours + 100;
		digit[1] = (minutes < 10) ? minutes + 100 : minutes;
		digit[2] = (seconds < 10) ? seconds + 100 : seconds;
		digit[3] = 0;
		digitVisible[0] = digitVisible[1] = digitVisible[2] = true;
		digitVisible[3] = false;
		delimTypes[0] = MHUD_TIMER_DELIM_COLON;
		delimTypes[1] = MHUD_TIMER_DELIM_COLON;
		numDelimiters = 2;
	}
	else if (detailed)
	{
		// MM:SS.CC
		digit[0] = (minutes < 10) ? minutes + 100 : minutes;
		digit[1] = (seconds < 10) ? seconds + 100 : seconds;
		digit[2] = (centis < 10) ? centis + 100 : centis;
		digit[3] = 0;
		digitVisible[0] = digitVisible[1] = digitVisible[2] = true;
		digitVisible[3] = false;
		delimTypes[0] = MHUD_TIMER_DELIM_COLON;
		delimTypes[1] = MHUD_TIMER_DELIM_DOT;
		numDelimiters = 2;
	}
	else
	{
		// MM:SS
		digit[0] = (minutes < 10) ? minutes + 100 : minutes;
		digit[1] = (seconds < 10) ? seconds + 100 : seconds;
		digit[2] = 0;
		digit[3] = 0;
		digitVisible[0] = digitVisible[1] = true;
		digitVisible[2] = digitVisible[3] = false;
		delimTypes[0] = MHUD_TIMER_DELIM_COLON;
		numDelimiters = 1;
	}

	const f32 offsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudTimerOffsetX", MHUD_DEF_TIMER_OFFSET_X);
	const f32 offsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudTimerOffsetY", MHUD_DEF_TIMER_OFFSET_Y);
	const f32 scale = (f32)this->player->optionService->GetPreferenceFloat("mhudTimerScale", MHUD_DEF_TIMER_SCALE);

	// Center the active pairs around offsetX.
	// Formula: pair i of N visible pairs → (2i − (N−1)) × half_step
	i32 numVisible = 0;
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(digitVisible); i++)
	{
		numVisible += digitVisible[i] ? 1 : 0;
	}
	f32 layoutX[4] = {};
	for (i32 i = 0; i < numVisible; i++)
	{
		layoutX[i] = (2 * i - (numVisible - 1)) * (MHUD_TIMER_PAIR_STEP / 2.0f);
	}

	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerTextParticles); i++)
	{
		CParticleSystem *p = this->timerTextParticles[i].Get();
		if (!p)
		{
			continue;
		}
		p->SetControlPointValue(17, Vector((f32)digit[i], scale, 0.0f));
		p->SetControlPointValue(18, Vector(offsetX + layoutX[i], offsetY, 1.0f));
		if (digitVisible[i])
		{
			p->Start();
		}
		else
		{
			p->Destroy();
		}
	}
	for (i32 d = 0; d < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); d++)
	{
		CParticleSystem *p = this->timerDelimiterParticles[d].Get();
		if (!p)
		{
			continue;
		}
		if (d < numDelimiters)
		{
			f32 delimX = offsetX + layoutX[d] + (MHUD_TIMER_PAIR_STEP / 2.0f);
			p->SetControlPointValue(17, Vector((f32)delimTypes[d], scale, 0.0f));
			p->SetControlPointValue(18, Vector(delimX, offsetY, 1.0f));
			p->Start();
		}
		else
		{
			p->Destroy();
		}
	}

	// Tint based on state.
	Color color;
	if (!timerRunning)
	{
		color = this->GetMHUDColorPref("mhudTimerStoppedColor", MHUD_DEF_TIMER_STOPPED_COLOR);
	}
	else if (paused)
	{
		color = this->GetMHUDColorPref("mhudTimerPausedColor", MHUD_DEF_TIMER_PAUSED_COLOR);
	}
	else if (this->player->checkpointService->GetTeleportCount() > 0)
	{
		color = this->GetMHUDColorPref("mhudTimerTpColor", MHUD_DEF_TIMER_TP_COLOR);
	}
	else
	{
		color = this->GetMHUDColorPref("mhudTimerProColor", MHUD_DEF_TIMER_PRO_COLOR);
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerTextParticles); i++)
	{
		SetParticleTint(this->timerTextParticles[i].Get(), color);
	}
	for (i32 i = 0; i < (i32)KZ_ARRAYSIZE(this->timerDelimiterParticles); i++)
	{
		SetParticleTint(this->timerDelimiterParticles[i].Get(), color);
	}
}

// === Keys ===========================================================================

void KZHUDService::CheckMHUDKeyParticle()
{
	bool enabled = this->IsMHUDKeysEnabled();
	if (!enabled)
	{
		if (this->keysParticle)
		{
			DestroyHandle(this->keysParticle);
		}
		return;
	}

	const Color color = this->GetMHUDColorPref("mhudKeysColor", MHUD_DEF_BASE_COLOR);
	const f32 offsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysOffsetX", MHUD_DEF_KEYS_OFFSET_X);
	const f32 offsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysOffsetY", MHUD_DEF_KEYS_OFFSET_Y);
	const f32 scale = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysScale", MHUD_DEF_KEYS_SCALE);
	char inputsPath[256];
	const char *font = this->player->optionService->GetPreferenceStr("mhudFont", AVAILABLE_FONTS[0]);
	char fontLower[64];
	V_strncpy(fontLower, font, sizeof(fontLower));
	V_strlower(fontLower);
	BuildParticlePath(inputsPath, sizeof(inputsPath), PARTICLE_INPUTS_PATTERN, fontLower, this->IsMHUDOutlineEnabled());
	if (!this->keysParticle)
	{
		this->keysParticle = CreateMHUDParticle(inputsPath, color, 0.0f, scale, offsetX, offsetY);
	}
}

void KZHUDService::UpdateMHUDKeys()
{
	this->CheckMHUDKeyParticle();
	if (!this->keysParticle)
	{
		return;
	}

	u8 mask = 0;
	if (this->player->IsButtonPressed(IN_FORWARD))
	{
		mask |= Forward;
	}
	if (this->player->IsButtonPressed(IN_MOVELEFT))
	{
		mask |= Left;
	}
	if (this->player->IsButtonPressed(IN_BACK))
	{
		mask |= Back;
	}
	if (this->player->IsButtonPressed(IN_MOVERIGHT))
	{
		mask |= Right;
	}
	// Use jumpedThisTick so the J flashes only when the player actually jumped
	// this tick, matching the existing panel HUD semantics.
	if (this->jumpedThisTick || this->player->IsButtonPressed(IN_JUMP))
	{
		mask |= Jump;
	}
	if (this->player->IsButtonPressed(IN_DUCK))
	{
		mask |= Duck;
	}

	const f32 offsetX = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysOffsetX", MHUD_DEF_KEYS_OFFSET_X);
	const f32 offsetY = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysOffsetY", MHUD_DEF_KEYS_OFFSET_Y);
	const f32 scale = (f32)this->player->optionService->GetPreferenceFloat("mhudKeysScale", MHUD_DEF_KEYS_SCALE);

	CParticleSystem *p = this->keysParticle.Get();
	p->SetControlPointValue(17, Vector((f32)mask, scale, 0.0f));
	p->SetControlPointValue(18, Vector(offsetX, offsetY, 1.0f));

	bool hasOverlap = ((mask & Forward) && (mask & Back)) || ((mask & Left) && (mask & Right));
	Color tint = this->GetMHUDColorPref("mhudKeysColor", MHUD_DEF_BASE_COLOR);
	if (hasOverlap && this->IsMHUDKeysOverlapEnabled())
	{
		tint = this->GetMHUDColorPref("mhudKeysOverlapColor", MHUD_DEF_KEYS_OVERLAP_COLOR);
	}
	SetParticleTint(p, tint);
}

void KZHUDService::UpdateParticles()
{
	this->UpdateMHUDSpeed();
	this->UpdateMHUDTimer();
	this->UpdateMHUDKeys();
}

// === Command helpers ================================================================

// Toggle a MHUD bool preference and print the result.
static_function void MHUDToggle(KZPlayer *p, const char *prefKey, bool defaultValue, const char *enabledKey, const char *disabledKey)
{
	bool next = !p->optionService->GetPreferenceBool(prefKey, defaultValue);
	p->optionService->SetPreferenceBool(prefKey, next);
	p->languageService->PrintChat(true, false, next ? enabledKey : disabledKey);
}

// Set offset x+y for a MHUD element.
// argOffset is the index in args where x begins (e.g. 3 for "kz_mhud speed offset <x> <y>").
// If values are missing/invalid, prints current value alongside the usage hint.
static_function void MHUDSetOffset(KZPlayer *p, const CCommand *args, int argOffset, const char *prefKeyX, const char *prefKeyY, f32 defaultX,
								   f32 defaultY, const char *usageKey, const char *setKey)
{
	if (args->ArgC() < argOffset + 2 || !utils::IsNumeric(args->Arg(argOffset)) || !utils::IsNumeric(args->Arg(argOffset + 1)))
	{
		f32 cx = (f32)p->optionService->GetPreferenceFloat(prefKeyX, defaultX);
		f32 cy = (f32)p->optionService->GetPreferenceFloat(prefKeyY, defaultY);
		char cxs[16], cys[16];
		V_snprintf(cxs, sizeof(cxs), "%.4f", cx);
		V_snprintf(cys, sizeof(cys), "%.4f", cy);
		p->languageService->PrintChat(true, false, usageKey, cxs, cys);
		return;
	}
	f32 x = (f32)atof(args->Arg(argOffset));
	f32 y = (f32)atof(args->Arg(argOffset + 1));
	p->optionService->SetPreferenceFloat(prefKeyX, x);
	p->optionService->SetPreferenceFloat(prefKeyY, y);
	char xs[16], ys[16];
	V_snprintf(xs, sizeof(xs), "%.4f", x);
	V_snprintf(ys, sizeof(ys), "%.4f", y);
	p->languageService->PrintChat(true, false, setKey, xs, ys);
}

// Set scale for a MHUD element.
// argOffset is the index in args where the value begins.
// If the value is missing/invalid, prints the current value alongside the usage hint.
static_function void MHUDSetScale(KZPlayer *p, const CCommand *args, int argOffset, const char *prefKey, f32 defaultValue, const char *usageKey,
								  const char *setKey)
{
	if (args->ArgC() < argOffset + 1 || !utils::IsNumeric(args->Arg(argOffset)))
	{
		f32 cv = (f32)p->optionService->GetPreferenceFloat(prefKey, defaultValue);
		char cvs[16];
		V_snprintf(cvs, sizeof(cvs), "%.4f", cv);
		p->languageService->PrintChat(true, false, usageKey, cvs);
		return;
	}
	f32 v = (f32)atof(args->Arg(argOffset));
	p->optionService->SetPreferenceFloat(prefKey, v);
	char vs[16];
	V_snprintf(vs, sizeof(vs), "%.4f", v);
	p->languageService->PrintChat(true, false, setKey, vs);
}

// Set a color preference (packed as int).
// argOffset is the index in args where R or color name begins.
// Accepts either a predefined color name or R G B [A] values.
// If args are missing/invalid, prints the usage hint.
static_function void SetColorPref(KZPlayer *p, const CCommand *args, int argOffset, const char *prefKey, const char *usageKey, const char *setKey)
{
	Color c;
	bool named = args->ArgC() >= argOffset + 1 && utils::ParseColorName(args->Arg(argOffset), c);
	if (!named && !utils::ParseColorArgs(args, argOffset, c))
	{
		p->languageService->PrintChat(true, false, usageKey);
		return;
	}
	p->optionService->SetPreferenceInt(prefKey, PackColor(c));
	char colorStr[32];
	V_snprintf(colorStr, sizeof(colorStr), "(%d, %d, %d, %d)", c.r(), c.g(), c.b(), c.a());
	p->languageService->PrintChat(true, false, setKey, colorStr);
}

// Reset all preference keys for a single element to defaults.
enum class MHUDElement
{
	Speed,
	Prespeed,
	Timer,
	Keys
};

static_function void ResetElementPrefs(KZPlayer *p, MHUDElement element)
{
	switch (element)
	{
		case MHUDElement::Speed:
			p->optionService->SetPreferenceBool("mhudSpeedEnabled", false);
			p->optionService->SetPreferenceFloat("mhudSpeedOffsetX", MHUD_DEF_SPEED_OFFSET_X);
			p->optionService->SetPreferenceFloat("mhudSpeedOffsetY", MHUD_DEF_SPEED_OFFSET_Y);
			p->optionService->SetPreferenceFloat("mhudSpeedScale", MHUD_DEF_SPEED_SCALE);
			p->optionService->SetPreferenceInt("mhudSpeedColor", PackColor(MHUD_DEF_BASE_COLOR));
			p->optionService->SetPreferenceInt("mhudSpeedCjColor", PackColor(MHUD_DEF_CJ_COLOR));
			break;
		case MHUDElement::Prespeed:
			p->optionService->SetPreferenceBool("mhudPrespeedEnabled", false);
			p->optionService->SetPreferenceFloat("mhudPrespeedOffsetX", MHUD_DEF_PRESPEED_OFFSET_X);
			p->optionService->SetPreferenceFloat("mhudPrespeedOffsetY", MHUD_DEF_PRESPEED_OFFSET_Y);
			p->optionService->SetPreferenceFloat("mhudPrespeedScale", MHUD_DEF_PRESPEED_SCALE);
			p->optionService->SetPreferenceInt("mhudPrespeedColor", PackColor(MHUD_DEF_BASE_COLOR));
			p->optionService->SetPreferenceInt("mhudPrespeedPerfColor", PackColor(MHUD_DEF_PERF_COLOR));
			p->optionService->SetPreferenceInt("mhudPrespeedJumpbugColor", PackColor(MHUD_DEF_JUMPBUG_COLOR));
			break;
		case MHUDElement::Timer:
			p->optionService->SetPreferenceBool("mhudTimerEnabled", false);
			p->optionService->SetPreferenceBool("mhudTimerDetailed", true);
			p->optionService->SetPreferenceFloat("mhudTimerOffsetX", MHUD_DEF_TIMER_OFFSET_X);
			p->optionService->SetPreferenceFloat("mhudTimerOffsetY", MHUD_DEF_TIMER_OFFSET_Y);
			p->optionService->SetPreferenceFloat("mhudTimerScale", MHUD_DEF_TIMER_SCALE);
			p->optionService->SetPreferenceInt("mhudTimerTpColor", PackColor(MHUD_DEF_TIMER_TP_COLOR));
			p->optionService->SetPreferenceInt("mhudTimerProColor", PackColor(MHUD_DEF_TIMER_PRO_COLOR));
			p->optionService->SetPreferenceInt("mhudTimerPausedColor", PackColor(MHUD_DEF_TIMER_PAUSED_COLOR));
			p->optionService->SetPreferenceInt("mhudTimerStoppedColor", PackColor(MHUD_DEF_TIMER_STOPPED_COLOR));
			break;
		case MHUDElement::Keys:
			p->optionService->SetPreferenceBool("mhudKeysEnabled", false);
			p->optionService->SetPreferenceBool("mhudKeysOverlap", true);
			p->optionService->SetPreferenceFloat("mhudKeysOffsetX", MHUD_DEF_KEYS_OFFSET_X);
			p->optionService->SetPreferenceFloat("mhudKeysOffsetY", MHUD_DEF_KEYS_OFFSET_Y);
			p->optionService->SetPreferenceFloat("mhudKeysScale", MHUD_DEF_KEYS_SCALE);
			p->optionService->SetPreferenceInt("mhudKeysColor", PackColor(MHUD_DEF_BASE_COLOR));
			p->optionService->SetPreferenceInt("mhudKeysOverlapColor", PackColor(MHUD_DEF_KEYS_OVERLAP_COLOR));
			break;
	}
}

void KZHUDService::PrintMHUDSummary()
{
	auto *p = this->player;
	auto *opts = p->optionService;
	auto *lang = p->languageService;
	// clang-format off
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudSpeedEnabled", false) ? "MHUD - Speed Enabled" : "MHUD - Speed Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudPrespeedEnabled", false) ? "MHUD - Prespeed Enabled" : "MHUD - Prespeed Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudTimerEnabled", false) ? "MHUD - Timer Enabled" : "MHUD - Timer Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudTimerDetailed", true) ? "MHUD - Timer Detail Enabled" : "MHUD - Timer Detail Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudKeysEnabled", false) ? "MHUD - Keys Enabled" : "MHUD - Keys Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudKeysOverlap", true) ? "MHUD - Keys Overlap Enabled" : "MHUD - Keys Overlap Disabled");
	lang->PrintChat(true, false, opts->GetPreferenceBool("mhudOutline", true) ? "MHUD - Outline Enabled" : "MHUD - Outline Disabled");
	lang->PrintChat(true, false, "MHUD - Font", opts->GetPreferenceStr("mhudFont", AVAILABLE_FONTS[0]));
	// clang-format on
}

// Hierarchy:
//   kz_mhud                                  → full summary
//   kz_mhud speed                            → toggle speed
//   kz_mhud speed offset [x y]               → show/set X,Y offset
//   kz_mhud speed scale [v]                  → show/set scale
//   kz_mhud speed color|crouchjumpcolor/cjcolor [r g b [a]]
//   kz_mhud speed reset                      → reset all speed prefs
//   kz_mhud prespeed [offset|scale|color|perfcolor|jumpbugcolor/jbcolor|reset ...]
//   kz_mhud timer [detail|offset|scale|tpcolor|procolor|pausedcolor|stoppedcolor|reset ...]
//   kz_mhud keys [overlap|offset|scale|color|overlapcolor|reset ...]
//   kz_mhud font [lato|verdana]              → set font for all elements
//   kz_mhud outline                          → toggle outline for all elements

SCMD(kz_mhud, SCFL_HUD | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	bool mhudAvail = KZHUDService::IsMHUDAvailable();

	// kz_mhud → full summary
	if (args->ArgC() < 2)
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		player->hudService->PrintMHUDSummary();
		return MRES_SUPERCEDE;
	}

	const char *element = args->Arg(1);
	const char *prop = args->ArgC() >= 3 ? args->Arg(2) : nullptr;

	if (KZ_STREQI(element, "speed"))
	{
		if (!prop)
		{
			// kz_mhud speed → toggle (particle-only, requires MHUD)
			if (!mhudAvail)
			{
				player->languageService->PrintChat(true, false, "MHUD - Unavailable");
				return MRES_SUPERCEDE;
			}
			MHUDToggle(player, "mhudSpeedEnabled", false, "MHUD - Speed Enabled", "MHUD - Speed Disabled");
		}
		else if (KZ_STREQI(prop, "offset"))
		{
			if (!mhudAvail)
			{
				player->languageService->PrintChat(true, false, "MHUD - Unavailable");
				return MRES_SUPERCEDE;
			}
			MHUDSetOffset(player, args, 3, "mhudSpeedOffsetX", "mhudSpeedOffsetY", MHUD_DEF_SPEED_OFFSET_X, MHUD_DEF_SPEED_OFFSET_Y,
						  "MHUD - Speed Offset Usage", "MHUD - Speed Offset Set");
		}
		else if (KZ_STREQI(prop, "scale"))
		{
			if (!mhudAvail)
			{
				player->languageService->PrintChat(true, false, "MHUD - Unavailable");
				return MRES_SUPERCEDE;
			}
			MHUDSetScale(player, args, 3, "mhudSpeedScale", MHUD_DEF_SPEED_SCALE, "MHUD - Speed Scale Usage", "MHUD - Speed Scale Set");
		}
		// Color commands are always available — they also affect the HTML panel.
		else if (KZ_STREQI(prop, "color"))
		{
			SetColorPref(player, args, 3, "mhudSpeedColor", "MHUD - Speed Color Usage", "MHUD - Speed Color Set");
		}
		else if (KZ_STREQI(prop, "crouchjumpcolor") || KZ_STREQI(prop, "cjcolor"))
		{
			SetColorPref(player, args, 3, "mhudSpeedCjColor", "MHUD - Speed CJ Color Usage", "MHUD - Speed CJ Color Set");
		}
		else if (KZ_STREQI(prop, "reset"))
		{
			ResetElementPrefs(player, MHUDElement::Speed);
			player->languageService->PrintChat(true, false, "MHUD - Speed Reset");
		}
		else
		{
			player->languageService->PrintChat(true, false, "MHUD - Speed Usage");
		}
	}
	else if (KZ_STREQI(element, "prespeed"))
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		if (!prop)
		{
			MHUDToggle(player, "mhudPrespeedEnabled", false, "MHUD - Prespeed Enabled", "MHUD - Prespeed Disabled");
		}
		else if (KZ_STREQI(prop, "offset"))
		{
			MHUDSetOffset(player, args, 3, "mhudPrespeedOffsetX", "mhudPrespeedOffsetY", MHUD_DEF_PRESPEED_OFFSET_X, MHUD_DEF_PRESPEED_OFFSET_Y,
						  "MHUD - Prespeed Offset Usage", "MHUD - Prespeed Offset Set");
		}
		else if (KZ_STREQI(prop, "scale"))
		{
			MHUDSetScale(player, args, 3, "mhudPrespeedScale", MHUD_DEF_PRESPEED_SCALE, "MHUD - Prespeed Scale Usage", "MHUD - Prespeed Scale Set");
		}
		else if (KZ_STREQI(prop, "color"))
		{
			SetColorPref(player, args, 3, "mhudPrespeedColor", "MHUD - Prespeed Color Usage", "MHUD - Prespeed Color Set");
		}
		else if (KZ_STREQI(prop, "perfcolor"))
		{
			SetColorPref(player, args, 3, "mhudPrespeedPerfColor", "MHUD - Prespeed Perf Color Usage", "MHUD - Prespeed Perf Color Set");
		}
		else if (KZ_STREQI(prop, "jumpbugcolor") || KZ_STREQI(prop, "jbcolor"))
		{
			SetColorPref(player, args, 3, "mhudPrespeedJumpbugColor", "MHUD - Prespeed Jumpbug Color Usage", "MHUD - Prespeed Jumpbug Color Set");
		}
		else if (KZ_STREQI(prop, "reset"))
		{
			ResetElementPrefs(player, MHUDElement::Prespeed);
			player->languageService->PrintChat(true, false, "MHUD - Prespeed Reset");
		}
		else
		{
			player->languageService->PrintChat(true, false, "MHUD - Prespeed Usage");
		}
	}
	else if (KZ_STREQI(element, "timer"))
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		if (!prop)
		{
			MHUDToggle(player, "mhudTimerEnabled", false, "MHUD - Timer Enabled", "MHUD - Timer Disabled");
		}
		else if (KZ_STREQI(prop, "detail"))
		{
			MHUDToggle(player, "mhudTimerDetailed", true, "MHUD - Timer Detail Enabled", "MHUD - Timer Detail Disabled");
		}
		else if (KZ_STREQI(prop, "offset"))
		{
			MHUDSetOffset(player, args, 3, "mhudTimerOffsetX", "mhudTimerOffsetY", MHUD_DEF_TIMER_OFFSET_X, MHUD_DEF_TIMER_OFFSET_Y,
						  "MHUD - Timer Offset Usage", "MHUD - Timer Offset Set");
		}
		else if (KZ_STREQI(prop, "scale"))
		{
			MHUDSetScale(player, args, 3, "mhudTimerScale", MHUD_DEF_TIMER_SCALE, "MHUD - Timer Scale Usage", "MHUD - Timer Scale Set");
		}
		else if (KZ_STREQI(prop, "tpcolor"))
		{
			SetColorPref(player, args, 3, "mhudTimerTpColor", "MHUD - Timer TP Color Usage", "MHUD - Timer TP Color Set");
		}
		else if (KZ_STREQI(prop, "procolor"))
		{
			SetColorPref(player, args, 3, "mhudTimerProColor", "MHUD - Timer Pro Color Usage", "MHUD - Timer Pro Color Set");
		}
		else if (KZ_STREQI(prop, "pausedcolor"))
		{
			SetColorPref(player, args, 3, "mhudTimerPausedColor", "MHUD - Timer Paused Color Usage", "MHUD - Timer Paused Color Set");
		}
		else if (KZ_STREQI(prop, "stoppedcolor"))
		{
			SetColorPref(player, args, 3, "mhudTimerStoppedColor", "MHUD - Timer Stopped Color Usage", "MHUD - Timer Stopped Color Set");
		}
		else if (KZ_STREQI(prop, "reset"))
		{
			ResetElementPrefs(player, MHUDElement::Timer);
			player->languageService->PrintChat(true, false, "MHUD - Timer Reset");
		}
		else
		{
			player->languageService->PrintChat(true, false, "MHUD - Timer Usage");
		}
	}
	else if (KZ_STREQI(element, "keys"))
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		if (!prop)
		{
			MHUDToggle(player, "mhudKeysEnabled", false, "MHUD - Keys Enabled", "MHUD - Keys Disabled");
		}
		else if (KZ_STREQI(prop, "overlap"))
		{
			MHUDToggle(player, "mhudKeysOverlap", true, "MHUD - Keys Overlap Enabled", "MHUD - Keys Overlap Disabled");
		}
		else if (KZ_STREQI(prop, "offset"))
		{
			MHUDSetOffset(player, args, 3, "mhudKeysOffsetX", "mhudKeysOffsetY", MHUD_DEF_KEYS_OFFSET_X, MHUD_DEF_KEYS_OFFSET_Y,
						  "MHUD - Keys Offset Usage", "MHUD - Keys Offset Set");
		}
		else if (KZ_STREQI(prop, "scale"))
		{
			MHUDSetScale(player, args, 3, "mhudKeysScale", MHUD_DEF_KEYS_SCALE, "MHUD - Keys Scale Usage", "MHUD - Keys Scale Set");
		}
		else if (KZ_STREQI(prop, "color"))
		{
			SetColorPref(player, args, 3, "mhudKeysColor", "MHUD - Keys Color Usage", "MHUD - Keys Color Set");
		}
		else if (KZ_STREQI(prop, "overlapcolor"))
		{
			SetColorPref(player, args, 3, "mhudKeysOverlapColor", "MHUD - Keys Overlap Color Usage", "MHUD - Keys Overlap Color Set");
		}
		else if (KZ_STREQI(prop, "reset"))
		{
			ResetElementPrefs(player, MHUDElement::Keys);
			player->languageService->PrintChat(true, false, "MHUD - Keys Reset");
		}
		else
		{
			player->languageService->PrintChat(true, false, "MHUD - Keys Usage");
		}
	}
	else if (KZ_STREQI(element, "font"))
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		if (!prop)
		{
			player->languageService->PrintChat(true, false, "MHUD - Font Usage", fontList);
		}
		else
		{
			bool found = false;
			for (const auto *font : AVAILABLE_FONTS)
			{
				if (KZ_STREQI(prop, font))
				{
					player->optionService->SetPreferenceStr("mhudFont", font);
					player->hudService->DestroyAllParticles();
					player->languageService->PrintChat(true, false, "MHUD - Set Font", font);
					found = true;
					break;
				}
			}
			if (!found)
			{
				player->languageService->PrintChat(true, false, "MHUD - Font Usage", fontList);
			}
		}
	}
	else if (KZ_STREQI(element, "outline"))
	{
		if (!mhudAvail)
		{
			player->languageService->PrintChat(true, false, "MHUD - Unavailable");
			return MRES_SUPERCEDE;
		}
		bool next = !player->optionService->GetPreferenceBool("mhudOutline", true);
		player->optionService->SetPreferenceBool("mhudOutline", next);
		player->hudService->DestroyAllParticles();
		player->languageService->PrintChat(true, false, next ? "MHUD - Outline Enabled" : "MHUD - Outline Disabled");
	}
	else
	{
		player->languageService->PrintChat(true, false, "MHUD - Usage");
	}

	return MRES_SUPERCEDE;
}
