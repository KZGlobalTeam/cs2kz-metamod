// Backup for when layout based HUD is not available, or the player has asked for the legacy style.
#include "../kz.h"
#include "cs2kz.h"
#include "kz_hud.h"
#include "utils/utils.h"

#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "kz/replays/kz_replaysystem.h"

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

std::string KZHUDService::GetSpeedText(const char *language)
{
	const SpeedInfo info = this->GetSpeedInfo();
	if (!info.showTakeoff)
	{
		return KZLanguageService::PrepareMessageWithLang(language, "HUD - Speed Text", info.speed);
	}
	const MHUDPrefs &prefs = this->GetPrefs();
	// The legacy HTML HUD prints a hex color, which a gradient has no equivalent for, so a gradient
	// preference falls back to that element's default color (the shared MHUD_DEF_* the layout HUD uses).
	const Color baseCol = panorama::ResolveSolidColor(prefs.speed, MHUD_DEF_BASE_COLOR);
	const Color perfCol = panorama::ResolveSolidColor(prefs.prespeedPerf, MHUD_DEF_PERF_COLOR);
	const Color jumpbugCol = panorama::ResolveSolidColor(prefs.prespeedJumpbug, MHUD_DEF_JUMPBUG_COLOR);
	const Color cjCol = panorama::ResolveSolidColor(prefs.speedCj, MHUD_DEF_CJ_COLOR);
	Color tintCol = info.jumpbug ? jumpbugCol : (info.perf ? perfCol : baseCol);
	char colorBuf[24];
	V_snprintf(colorBuf, sizeof(colorBuf), "<font color='#%02x%02x%02x'>", tintCol.r(), tintCol.g(), tintCol.b());
	char cjBuf[24];
	V_snprintf(cjBuf, sizeof(cjBuf), "<font color='#%02x%02x%02x'>", cjCol.r(), cjCol.g(), cjCol.b());
	std::string crouchJumpingText = info.crouchJump ? std::string(" ") + cjBuf + "C</font>" : "";
	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Speed Text (Takeoff)", info.speed, colorBuf, info.takeoffSpeed,
													 crouchJumpingText.c_str());
}

std::string KZHUDService::GetKeyText(const char *language)
{
	CPlayer_MovementServices *ms = this->GetHudMoveServices();
	CInButtonState *buttons = ms ? &ms->m_nButtons() : nullptr;
	auto pressed = [buttons](InputBitMask_t button) { return buttons && buttons->IsButtonPressed(button, false); };

	// clang-format off
	return KZLanguageService::PrepareMessageWithLang(language, "HUD - Key Text",
		pressed(IN_MOVELEFT) ? 'A' : '_',
		pressed(IN_FORWARD) ? 'W' : '_',
		pressed(IN_BACK) ? 'S' : '_',
		pressed(IN_MOVERIGHT) ? 'D' : '_',
		pressed(IN_DUCK) ? 'C' : '_',
		this->jumpedThisTick ? 'J' : '_'
	);

	// clang-format on
}

void KZHUDService::DrawLegacyPanels(KZPlayer *player, KZPlayer *target)
{
	// Yield the center channel while a cs2menus HTML menu is open.
	bool menuOpen = g_pMenus && g_pMenus->GetActiveMenuType(target->GetPlayerSlot().Get()) == MenuType::Html;
	// The html/centre channels are per viewer, so these still follow the spectated player, and
	// every getter below reads its pawn: they need a live one to draw for.
	bool sourceLive = player->IsAlive() && player->GetPlayerPawn();
	bool showPanel = target->hudService->IsShowingPanel() && !menuOpen && sourceLive;
	bool compact = target->hudService->IsCompactPanel();

	// The client renders the layout state of whoever it is watching, so the subject's style is
	// what decides whether these channels would double up on it.
	bool layoutLive = player->hudService->IsUsingLayoutStyle();

	if (!showPanel || layoutLive)
	{
		return;
	}
	const char *language = target->languageService->GetLanguage();

	std::string keyText = player->hudService->GetKeyText(language);
	std::string checkpointText = player->hudService->GetCheckpointText(language);
	std::string timerText = player->hudService->GetTimerText(language);
	std::string speedText = player->hudService->GetSpeedText(language);

	std::string centerText = "";
	std::string htmlText = "";
	// The alert template is empty in every shipped translation, so skip formatting it in that case.
	std::string alertText = KZLanguageService::IsMessageEmpty(language, "HUD - Alert Text")
								? std::string("")
								: KZLanguageService::PrepareMessageWithLang(language, "HUD - Alert Text", keyText.c_str(), checkpointText.c_str(),
																			timerText.c_str(), speedText.c_str());

	if (compact)
	{
		htmlText = !timerText.empty() && !speedText.empty() ? timerText + "<br>" + speedText : timerText + speedText;
	}
	else
	{
		centerText = KZLanguageService::PrepareMessageWithLang(language, "HUD - Center Text", keyText.c_str(), checkpointText.c_str(),
															   timerText.c_str(), speedText.c_str());
		htmlText = KZLanguageService::PrepareMessageWithLang(language, "HUD - Html Center Text", keyText.c_str(), checkpointText.c_str(),
															 timerText.c_str(), speedText.c_str());
	}

	centerText = centerText.substr(0, centerText.find_last_not_of('\n') + 1);
	alertText = alertText.substr(0, alertText.find_last_not_of('\n') + 1);
	htmlText = htmlText.substr(0, htmlText.find_last_not_of('\n') + 1);

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
