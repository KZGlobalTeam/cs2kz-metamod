#include "../kz.h"
#include "kz_hud.h"
#include "sdk/datatypes.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include "../timer/kz_timer.h"
#include "../language/kz_language.h"
#include "../checkpoint/kz_checkpoint.h"

#include "tier0/memdbgon.h"

#define HUD_ON_GROUND_THRESHOLD 0.07f
internal KZHUDServiceTimerEventListener timerEventListener;

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

std::string KZHUDService::GetSpeedText(const char *language)
{
	Vector velocity;
	this->player->GetVelocity(&velocity);
	// Keep the takeoff velocity on for a while after landing so the speed values flicker less.
	if ((this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND
		 && g_pKZUtils->GetServerGlobals()->curtime - this->player->landingTime > HUD_ON_GROUND_THRESHOLD)
		|| (this->player->GetPlayerPawn()->m_MoveType == MOVETYPE_LADDER && !player->IsButtonPressed(IN_JUMP)))
	{
		return KZLanguageService::PrepareMessage(language, "HUD - Speed Text", velocity.Length2D());
	}
	return KZLanguageService::PrepareMessage(language, "HUD - Speed Text (Takeoff)", velocity.Length2D(), this->player->takeoffVelocity.Length2D());
}

std::string KZHUDService::GetKeyText(const char *language)
{
	// clang-format off

	return KZLanguageService::PrepareMessage(language, "HUD - Key Text",
		this->player->IsButtonPressed(IN_MOVELEFT) ? 'A' : '_',
		this->player->IsButtonPressed(IN_FORWARD) ? 'W' : '_',
		this->player->IsButtonPressed(IN_BACK) ? 'S' : '_',
		this->player->IsButtonPressed(IN_MOVERIGHT) ? 'D' : '_',
		this->player->IsButtonPressed(IN_DUCK) ? 'C' : '_',
		this->player->IsButtonPressed(IN_JUMP) ? 'J' : '_'
	);

	// clang-format on
}

std::string KZHUDService::GetCheckpointText(const char *language)
{
	// clang-format off

	return KZLanguageService::PrepareMessage(language, "HUD - Checkpoint Text",
		this->player->checkpointService->GetCurrentCpIndex(),
		this->player->checkpointService->GetCheckpointCount(),
		this->player->checkpointService->GetTeleportCount()
	);

	// clang-format on
}

std::string KZHUDService::GetTimerText(const char *language)
{
	if (this->player->timerService->GetTimerRunning() || this->ShouldShowTimerAfterStop())
	{
		char timeText[128];

		// clang-format off

		f64 time = this->player->timerService->GetTimerRunning()
			? player->timerService->GetTime()
			: this->currentTimeWhenTimerStopped;


		KZTimerService::FormatTime(time, timeText, sizeof(timeText));
		return KZLanguageService::PrepareMessage(language, "HUD - Timer Text",
			timeText,
			player->timerService->GetTimerRunning() ? "" : KZLanguageService::PrepareMessage(language, "HUD - Stopped Text").c_str(),
			player->timerService->GetPaused() ? KZLanguageService::PrepareMessage(language, "HUD - Paused Text").c_str() : ""
		);
		// clang-format on
	}
	return std::string("");
}

void KZHUDService::DrawPanels(KZPlayer *target)
{
	if (!this->IsShowingPanel())
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();
	char buffer[1024];
	buffer[0] = 0;
	std::string keyText = this->GetKeyText(language);
	std::string checkpointText = this->GetCheckpointText(language);
	std::string timerText = this->GetTimerText(language);
	std::string speedText = this->GetSpeedText(language);

	// clang-format off
	std::string centerText = KZLanguageService::PrepareMessage(language, "HUD - Center Text", 
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());
	std::string alertText = KZLanguageService::PrepareMessage(language, "HUD - Alert Text", 
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());
	std::string htmlText = KZLanguageService::PrepareMessage(language, "HUD - Html Center Text",
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());

	// clang-format on

	centerText = centerText.substr(0, centerText.find_last_not_of('\n') + 1);
	alertText = alertText.substr(0, alertText.find_last_not_of('\n') + 1);
	htmlText = htmlText.substr(0, htmlText.find_last_not_of('\n') + 1);

	// Remove trailing newlines just in case a line is empty.
	if (!centerText.empty())
	{
		target->PrintCentre(false, true, centerText.c_str());
	}
	if (!alertText.empty())
	{
		target->PrintAlert(false, true, alertText.c_str());
	}
	if (!htmlText.empty())
	{
		target->PrintHTMLCentre(false, true, htmlText.c_str());
	}
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

internal SCMD_CALLBACK(Command_KzPanel)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->hudService->TogglePanel();
	if (player->hudService->IsShowingPanel())
	{
		player->languageService->PrintChat(true, false, "HUD Option - Info Panel - Enable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "HUD Option - Info Panel - Disable");
	}
	return MRES_SUPERCEDE;
}

void KZHUDService::RegisterCommands()
{
	scmd::RegisterCmd("kz_panel", Command_KzPanel);
}
