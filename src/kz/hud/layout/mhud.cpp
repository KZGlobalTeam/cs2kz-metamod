#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/replays/kz_replaysystem.h"
#include "sdk/entity/ccscustomhudlayout.h"

#include "tier0/memdbgon.h"

void KZHUDService::UpdateTimerElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force)
{
	std::string text = source->hudService->GetTimerText(this->player->languageService->GetLanguage());
	if (!this->IsMHUDTimerDetailed())
	{
		// Drop the fraction, keeping any (STOPPED)/(PAUSED) suffix.
		const size_t dot = text.find('.');
		if (dot != std::string::npos)
		{
			size_t end = dot + 1;
			while (end < text.size() && V_isdigit(text[end]))
			{
				end++;
			}
			text.erase(dot, end - dot);
		}
	}

	const bool replay = KZ::replaysystem::IsReplayBot(source);
	const bool paused = replay ? KZ::replaysystem::GetPaused() : source->timerService->GetPaused();
	const bool running = replay ? KZ::replaysystem::GetEndTime() == 0.0f : source->timerService->GetTimerRunning();
	const i32 teleports = replay ? KZ::replaysystem::GetTeleportCount() : source->checkpointService->GetTeleportCount();

	auto *opts = this->player->optionService;
	Color color;
	if (paused)
	{
		color = opts->GetPreferenceColor("mhudTimerPausedColor", MHUD_DEF_TIMER_PAUSED_COLOR);
	}
	else if (!running)
	{
		color = opts->GetPreferenceColor("mhudTimerStoppedColor", MHUD_DEF_TIMER_STOPPED_COLOR);
	}
	else
	{
		color = teleports > 0 ? opts->GetPreferenceColor("mhudTimerTpColor", MHUD_DEF_TIMER_TP_COLOR)
							  : opts->GetPreferenceColor("mhudTimerProColor", MHUD_DEF_TIMER_PRO_COLOR);
	}

	const bool show = this->IsMHUDElementEnabled(MHUDElement::Timer) && !text.empty();
	this->UpdateLayoutElement(layout, MHUDElement::Timer, show, text.c_str(), color, force);
}

// TODO: add preference for decimal places.
void KZHUDService::UpdateSpeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	char text[16];
	V_snprintf(text, sizeof(text), "%.0f", info.speed);
	auto *opts = this->player->optionService;
	const Color color = info.crouchJump ? opts->GetPreferenceColor("mhudSpeedCjColor", MHUD_DEF_CJ_COLOR)
										: opts->GetPreferenceColor("mhudSpeedColor", MHUD_DEF_BASE_COLOR);
	this->UpdateLayoutElement(layout, MHUDElement::Speed, this->IsMHUDElementEnabled(MHUDElement::Speed), text, color, force);
}

// TODO: add preference for decimal places.
void KZHUDService::UpdatePrespeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	char text[16];
	V_snprintf(text, sizeof(text), "%.0f", info.takeoffSpeed);
	auto *opts = this->player->optionService;
	Color color;
	if (info.jumpbug)
	{
		color = opts->GetPreferenceColor("mhudPrespeedJumpbugColor", MHUD_DEF_JUMPBUG_COLOR);
	}
	else if (info.perf)
	{
		color = opts->GetPreferenceColor("mhudPrespeedPerfColor", MHUD_DEF_PERF_COLOR);
	}
	else
	{
		color = opts->GetPreferenceColor("mhudPrespeedColor", MHUD_DEF_BASE_COLOR);
	}
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Prespeed) && info.showTakeoff;
	this->UpdateLayoutElement(layout, MHUDElement::Prespeed, show, text, color, force);
}

// Order matches the panels in mhud.xml: C W J on the top row, A S D on the bottom.
static_global const char *KEY_PANELS[] = {"mhud_key_c", "mhud_key_w", "mhud_key_j", "mhud_key_a", "mhud_key_s", "mhud_key_d"};

void KZHUDService::UpdateKeysElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force)
{
	CPlayer_MovementServices *ms = source->hudService->GetHudMoveServices();
	CInButtonState *buttons = ms ? &ms->m_nButtons() : nullptr;
	auto pressed = [buttons](InputBitMask_t button) { return buttons && buttons->IsButtonPressed(button, false); };
	const bool left = pressed(IN_MOVELEFT), forward = pressed(IN_FORWARD), back = pressed(IN_BACK), right = pressed(IN_MOVERIGHT);
	const bool keys[] = {pressed(IN_DUCK), forward, source->hudService->JumpedThisTick(), left, back, right};

	auto *opts = this->player->optionService;
	const bool overlap = (forward && back) || (left && right);
	const Color color = overlap && this->IsMHUDKeysOverlapEnabled() ? opts->GetPreferenceColor("mhudKeysOverlapColor", MHUD_DEF_KEYS_OVERLAP_COLOR)
																	: opts->GetPreferenceColor("mhudKeysColor", MHUD_DEF_BASE_COLOR);
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Keys);
	this->UpdateLayoutElement(layout, MHUDElement::Keys, show, NULL, color, force);
	if (force)
	{
		this->layoutKeys = LayoutKeysState();
	}
	if (!show)
	{
		return;
	}

	const bool hideIdle = this->IsMHUDKeysHidingUnpressed();
	if (this->layoutKeys.hideIdle != hideIdle)
	{
		this->layoutKeys.hideIdle = hideIdle;
		layout->SetHasClass(MHUD_ELEMENTS[(i32)MHUDElement::Keys].panelId, "hide-idle",
							hideIdle ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 letters = this->IsMHUDKeysUsingLetters() ? 1 : 0;
	if (this->layoutKeys.letters != letters)
	{
		this->layoutKeys.letters = letters;
		layout->SetHasClass(MHUD_ELEMENTS[(i32)MHUDElement::Keys].panelId, "keys-letters",
							letters ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 square = this->IsMHUDKeysSquare() ? 1 : 0;
	if (this->layoutKeys.square != square)
	{
		this->layoutKeys.square = square;
		layout->SetHasClass(MHUD_ELEMENTS[(i32)MHUDElement::Keys].panelId, "keys-square",
							square ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_PANELS); i++)
	{
		if (this->layoutKeys.pressed[i] == keys[i])
		{
			continue;
		}
		this->layoutKeys.pressed[i] = keys[i];
		layout->SetHasClass(KEY_PANELS[i], "pressed", keys[i] ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}

	const MHUDElementDef &def = MHUD_ELEMENTS[(i32)MHUDElement::Keys];
	const i32 size = panorama::SnapToStep((i32)opts->GetPreferenceFloat(def.sizeKey, def.sizeDefault), 0, 500);
	if (this->layoutKeys.fontSize != size)
	{
		char className[64];
		for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_PANELS); i++)
		{
			if (this->layoutKeys.fontSize != INT_MIN)
			{
				V_snprintf(className, sizeof(className), "font-size--%ipx", this->layoutKeys.fontSize);
				layout->SetHasClass(KEY_PANELS[i], className, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			V_snprintf(className, sizeof(className), "font-size--%ipx", size);
			layout->SetHasClass(KEY_PANELS[i], className, k_eHudPanelClassStatus_HasClass);
		}
		this->layoutKeys.fontSize = size;
	}

	const char *fontClass = MHUDFontClass(this->player, MHUDElement::Keys);
	if (this->layoutKeys.fontClass != fontClass)
	{
		for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_PANELS); i++)
		{
			if (this->layoutKeys.fontClass)
			{
				layout->SetHasClass(KEY_PANELS[i], this->layoutKeys.fontClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(KEY_PANELS[i], fontClass, k_eHudPanelClassStatus_HasClass);
		}
		this->layoutKeys.fontClass = fontClass;
	}
}

void KZHUDService::UpdateCheckpointElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force)
{
	std::string text = source->hudService->GetCheckpointText(this->player->languageService->GetLanguage());
	const Color color = this->player->optionService->GetPreferenceColor("mhudCheckpointColor", MHUD_DEF_BASE_COLOR);
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Checkpoint) && !text.empty();
	this->UpdateLayoutElement(layout, MHUDElement::Checkpoint, show, text.c_str(), color, force);
}

bool KZHUDService::UpdateHudLayout(KZPlayer *source)
{
	bool created = false;
	CCSCustomHudLayout *layout = this->EnsureOwnedLayout(created);
	if (!layout)
	{
		return false;
	}
	const bool force = created;
	const bool show = this->IsShowingPanel() && this->IsUsingLayoutStyle();

	// The crosshair is independent of the elements below, so it is applied before the collapse path.
	this->ApplyCrosshair(layout, show, force);

	if (!show)
	{
		// Panel off or the legacy style is selected: collapse every element. UpdateLayoutElement with
		// show=false only applies the hidden class and returns before reading any value, so the text and
		// color passed here are ignored (MHUD_DEF_BASE_COLOR is just a placeholder).
		for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
		{
			this->UpdateLayoutElement(layout, (MHUDElement)i, false, NULL, MHUD_DEF_BASE_COLOR, force);
		}
		return true;
	}

	const SpeedInfo info = source->hudService->GetSpeedInfo();
	this->UpdateTimerElement(layout, source, force);
	this->UpdateSpeedElement(layout, info, force);
	this->UpdatePrespeedElement(layout, info, force);
	this->UpdateKeysElement(layout, source, force);
	this->UpdateCheckpointElement(layout, source, force);
	return true;
}
