#include "../kz.h"
#include "cs2kz.h"
#include "kz_hud.h"
#include "sdk/datatypes.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"

#include "kz/language/kz_language.h"

#include "tier0/memdbgon.h"

#define HUD_ON_GROUND_THRESHOLD 0.07f

void KZHUDService::Init()
{

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
		return KZLanguageService::PrepareMessageWithLang(language, "HUD - Speed Text", velocity.Length2D());
	}
	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Speed Text (Takeoff)", velocity.Length2D(),
													 this->player->takeoffVelocity.Length2D());
}

std::string KZHUDService::GetKeyText(const char *language)
{
	// clang-format off

	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Key Text",
		this->player->IsButtonPressed(IN_MOVELEFT) ? 'A' : '_',
		this->player->IsButtonPressed(IN_FORWARD) ? 'W' : '_',
		this->player->IsButtonPressed(IN_BACK) ? 'S' : '_',
		this->player->IsButtonPressed(IN_MOVERIGHT) ? 'D' : '_',
		this->player->IsButtonPressed(IN_DUCK) ? 'C' : '_',
		this->player->IsButtonPressed(IN_JUMP) ? 'J' : '_'
	);

	// clang-format on
}

void KZHUDService::DrawPanels(KZPlayer *player, KZPlayer *target)
{
	if (!target->hudService->IsShowingPanel())
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();

	std::string keyText = player->hudService->GetKeyText(language);
	std::string checkpointText = "";
	std::string timerText = "";
	std::string speedText = player->hudService->GetSpeedText(language);

	// clang-format off
	std::string centerText = KZLanguageService::PrepareMessageWithLang(language, "HUD - Center Text", 
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());
	std::string alertText = KZLanguageService::PrepareMessageWithLang(language, "HUD - Alert Text", 
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());
	std::string htmlText = KZLanguageService::PrepareMessageWithLang(language, "HUD - Html Center Text",
		keyText.c_str(), checkpointText.c_str(), timerText.c_str(), speedText.c_str());

	// clang-format on

	centerText = centerText.substr(0, centerText.find_last_not_of('\n') + 1);
	alertText = alertText.substr(0, alertText.find_last_not_of('\n') + 1);
	htmlText = htmlText.substr(0, htmlText.find_last_not_of('\n') + 1);

	// Remove trailing newlines just in case a line is empty.
	if (!centerText.empty())
	{
		target->PrintCentre(false, false, centerText.c_str());
	}
	if (!alertText.empty())
	{
		target->PrintAlert(false, false, alertText.c_str());
	}
	if (!htmlText.empty())
	{
		target->PrintHTMLCentre(false, false, htmlText.c_str());
	}
}

void KZHUDService::ResetShowPanel()
{
	this->showPanel = true;
}

void KZHUDService::TogglePanel()
{
	this->showPanel = !this->showPanel;
	//this->player->optionService->SetPreferenceBool("showPanel", this->showPanel);
	if (!this->showPanel)
	{
		utils::PrintAlert(this->player->GetController(), "#SFUI_EmptyString");
		utils::PrintCentre(this->player->GetController(), "#SFUI_EmptyString");
		this->player->languageService->PrintHTMLCentre(false, false, "HUD - HTML Panel Disabled");
	}
}

static_function SCMD_CALLBACK(Command_KzPanel)
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