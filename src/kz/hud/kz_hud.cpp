#include "kz_hud.h"
#include "sdk/datatypes.h"
#include "utils/utils.h"

#include "../timer/kz_timer.h"
#include "tier0/memdbgon.h"

#include "../checkpoint/kz_checkpoint.h"

KZHUDServiceTimerEventListener timerEventListener;

void KZHUDService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZHUDService::Reset()
{
	this->showPanel = true;
	this->timerStoppedTime = {};
	this->currentTimeWhenTimerStopped = {};
}

void KZHUDService::AddSpeedText(char *buffer, int size)
{
	char speed[128];
	speed[0] = 0;
	Vector velocity;
	this->player->GetVelocity(&velocity);
	// Keep the takeoff velocity on for a while after landing so the speed values flicker less.
	if ((this->player->GetPawn()->m_fFlags & FL_ONGROUND && g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTime > 0.07)
		|| (this->player->GetPawn()->m_MoveType == MOVETYPE_LADDER && !player->IsButtonPressed(IN_JUMP)))
	{
		snprintf(speed, sizeof(speed), "Speed: %.0f", velocity.Length2D());
	}
	else
	{
		snprintf(speed, sizeof(speed), "Speed: %.0f (%.0f)", velocity.Length2D(), this->player->takeoffVelocity.Length2D());
	}
	V_strncat(buffer, speed, size);
}

void KZHUDService::AddKeyText(char *buffer, int size)
{
	char keys[128];
	keys[0] = 0;
	snprintf(keys, sizeof(keys), "Keys: %s %s %s %s %s %s",
		this->player->IsButtonPressed(IN_MOVELEFT) ? "A" : "_",
		this->player->IsButtonPressed(IN_FORWARD) ? "W" : "_",
		this->player->IsButtonPressed(IN_BACK) ? "S" : "_",
		this->player->IsButtonPressed(IN_MOVERIGHT) ? "D" : "_",
		this->player->IsButtonPressed(IN_DUCK) ? "C" : "_",
		this->player->IsButtonPressed(IN_JUMP) ? "J" : "_");
	V_strncat(buffer, keys, size);
}

void KZHUDService::AddTeleText(char *buffer, int size)
{
	char tele[128];
	tele[0] = 0;
	snprintf(tele, sizeof(tele), "CP: %i/%i TPs: %i",
		this->player->checkpointService->GetCurrentCpIndex(),
		this->player->checkpointService->GetCheckpointCount(),
		this->player->checkpointService->GetTeleportCount());
	V_strncat(buffer, tele, size);
}

void KZHUDService::AddTimerText(char *buffer, int size)
{
	char timer[128];
	if (this->player->timerService->GetTimerRunning() || this->ShouldShowTimerAfterStop())
	{
		V_strncat(buffer, "\n", size);
		f64 time = this->player->timerService->GetTimerRunning() ? player->timerService->GetTime() : this->currentTimeWhenTimerStopped;
		KZTimerService::FormatTime(time, timer, sizeof(timer));
		V_strncat(buffer, timer, size);
		if (!player->timerService->GetTimerRunning())
		{
			V_strncat(buffer, " (STOPPED)", size);
		}
		if (player->timerService->GetPaused())
		{
			V_strncat(buffer, " (PAUSED)", size);
		}
	}
}

void KZHUDService::DrawSpeedPanel()
{
	if(!this->IsShowingPanel())
		return;

	char buffer[1024];
	buffer[0] = 0;

	this->AddTeleText(buffer, sizeof(buffer));
	this->AddTimerText(buffer, sizeof(buffer));
	utils::PrintCentre(this->player->GetController(), buffer);
	buffer[0] = 0;

	this->AddSpeedText(buffer, sizeof(buffer));
	V_strncat(buffer, "\n", sizeof(buffer));
	this->AddKeyText(buffer, sizeof(buffer));

	utils::PrintAlert(this->player->GetController(), buffer);
}

void KZHUDService::TogglePanel()
{
	this->showPanel = !this->showPanel;
	if (!this->showPanel)
	{
		utils::PrintAlert(this->player->GetController(), "");
		utils::PrintCentre(this->player->GetController(), "");
	}
}

void KZHUDService::OnTimerStopped(f64 currentTimeWhenTimerStopped)
{
	this->timerStoppedTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->currentTimeWhenTimerStopped = currentTimeWhenTimerStopped;
}

void KZHUDServiceTimerEventListener::OnTimerStopped(KZPlayer *player)
{
	player->hudService->OnTimerStopped(player->timerService->GetTime());
}

void KZHUDServiceTimerEventListener::OnTimerEndPost(KZPlayer *player, const char *courseName, f32 time, u32 teleportsUsed)
{
	player->hudService->OnTimerStopped(time);
}
