#include "kz/hud/kz_hud.h"
#include "sdk/entity/cparticlesystem.h"
#include "entitykeyvalues.h"
#include "utils/simplecmds.h"

#define PARTICLE_SPEED           "particles/velo/velo_overlay.vpcf"
#define PARTICLE_SPEED_LARGE     "particles/velo/velo_overlay_large.vpcf"
#define PARTICLE_CROUCH_JUMP     "particles/cj/cj.vpcf"
#define PARTICLE_TIMER           "particles/velo/velo_overlay_large.vpcf"
#define PARTICLE_TIMER_DELIMITER "particles/timer/timer_delimiter.vpcf"
#define PARTICLE_KEYS            "particles/keys/keys.vpcf"

CParticleSystem *CreateMHUDParticle(const char *particleName, const Color &color, const f32 reserved, const f32 size, const f32 offsetX,
									const f32 offsetY)
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
	pKeyValues->SetVector("data_cp_value", Vector(reserved, size, 1.0f /* unused */));
	particleSystem->DispatchSpawn(pKeyValues);
	particleSystem->SetControlPointValue(18, Vector(offsetX, offsetY, 1.0f /* unused */));
	return particleSystem;
}

SCMD(kz_test_particle, SCFL_MISC)
{
	CreateMHUDParticle(PARTICLE_SPEED_LARGE, Color(255, 255, 255, 255), 255.0f, 0.03f, 0, 0);
	return MRES_SUPERCEDE;
}

void KZHUDService::CheckMHUDSpeedParticles()
{
	if (!this->speedParticles[0])
	{
		this->speedParticles[0] = CreateMHUDParticle(PARTICLE_SPEED_LARGE, Color(255, 255, 255, 255), 1.0f, 0.03f, -0.625f, -4.5f);
	}
	if (!this->speedParticles[1])
	{
		this->speedParticles[1] = CreateMHUDParticle(PARTICLE_SPEED_LARGE, Color(255, 255, 255, 255), 1.0f, 0.03f, 0.625f, -4.5f);
	}
	Vector velocity, baseVelocity;
	this->player->GetVelocity(&velocity);
	this->player->GetBaseVelocity(&baseVelocity);
	velocity += baseVelocity;
	this->SetMHUDSpeedParticleVelocity(velocity);
}

CConVar<float> offset_left("offset_left", FCVAR_NONE, "", -0.35f);
CConVar<float> offset_right("offset_right", FCVAR_NONE, "", 0.35f);

void KZHUDService::SetMHUDSpeedParticleVelocity(const Vector &speed, const Vector *prespeed)
{
	i32 speedRounded = RoundFloatToInt(speed.Length2D());
	if (speedRounded > 1000)
	{
		i32 firstTwoDigits = speedRounded / 100;
		i32 lastTwoDigits = speedRounded % 100;
		// Leading 0 corresponds to sequences 100-109
		lastTwoDigits += 100;
		this->speedParticles[0].Get()->SetControlPointValue(17, Vector(static_cast<f32>(firstTwoDigits), 0.03f, 0.0f));
		this->speedParticles[0].Get()->SetControlPointValue(18, Vector(-0.625f, -4.5f, 1.0f));
		this->speedParticles[1].Get()->SetControlPointValue(17, Vector(static_cast<f32>(lastTwoDigits), 0.03f, 0.0f));
		this->speedParticles[1].Get()->SetControlPointValue(18, Vector(0.625f, -4.5f, 1.0f));
	}
	else if (speedRounded > 100)
	{
		// First particle display the first 2 digits, second particle display the last two.
		i32 firstTwoDigits = speedRounded / 100;
		i32 lastTwoDigits = speedRounded % 100;
		this->speedParticles[0].Get()->SetControlPointValue(17, Vector(static_cast<f32>(firstTwoDigits), 0.03f, 0.0f));
		this->speedParticles[0].Get()->SetControlPointValue(18, Vector(offset_left.GetFloat(), -4.5f, 1.0f));
		this->speedParticles[1].Get()->SetControlPointValue(17, Vector(static_cast<f32>(lastTwoDigits), 0.03f, 0.0f));
		this->speedParticles[1].Get()->SetControlPointValue(18, Vector(offset_right.GetFloat(), -4.5f, 1.0f));
	}
	else
	{
		this->speedParticles[0].Get()->SetControlPointValue(17, Vector(static_cast<f32>(speedRounded), 0.03f, 0.0f));
		this->speedParticles[0].Get()->SetControlPointValue(18, Vector(0, -4.5f, 1.0f));
		// hide second particle by setting reserved to 100 which is out of range
		// todo: actually checktransmit this out
		this->speedParticles[1].Get()->SetControlPointValue(18, Vector(100.0f, 0.03f, 0.0f));
	}
}
