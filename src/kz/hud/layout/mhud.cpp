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

	const MHUDPrefs &prefs = this->GetPrefs();
	Color color;
	if (paused)
	{
		color = prefs.timerPaused;
	}
	else if (!running)
	{
		color = prefs.timerStopped;
	}
	else
	{
		color = teleports > 0 ? prefs.timerTp : prefs.timerPro;
	}

	const bool show = this->IsMHUDElementEnabled(MHUDElement::Timer) && !text.empty();
	this->UpdateLayoutElement(layout, MHUDElement::Timer, show, text.c_str(), color, force);
}

void KZHUDService::UpdateSpeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	char text[16];
	V_snprintf(text, sizeof(text), prefs.speedPrecise ? "%.2f" : "%.0f", info.speed);
	const Color color = info.crouchJump ? prefs.speedCj : prefs.speed;
	this->UpdateLayoutElement(layout, MHUDElement::Speed, this->IsMHUDElementEnabled(MHUDElement::Speed), text, color, force);
}

void KZHUDService::UpdatePrespeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	char text[16];
	const char *format = prefs.prespeedBrackets ? (prefs.prespeedPrecise ? "(%.2f)" : "(%.0f)") : (prefs.prespeedPrecise ? "%.2f" : "%.0f");
	V_snprintf(text, sizeof(text), format, info.takeoffSpeed);
	Color color;
	if (info.jumpbug)
	{
		color = prefs.prespeedJumpbug;
	}
	else if (info.perf)
	{
		color = prefs.prespeedPerf;
	}
	else
	{
		color = prefs.prespeed;
	}
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Prespeed) && info.showTakeoff && !(prefs.prespeedHideWalkOff && info.walkedOff);
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

	const MHUDPrefs &prefs = this->GetPrefs();
	const bool overlap = (forward && back) || (left && right);
	const Color color = overlap && prefs.keysOverlapEnabled ? prefs.keysOverlap : prefs.keys;
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

	const char *const keysPanel = MHUD_ELEMENTS[(i32)MHUDElement::Keys].panelId;
	const i32 idle = (i32)prefs.keysIdle;
	if (this->layoutKeys.idle != idle)
	{
		this->layoutKeys.idle = idle;
		layout->SetHasClass(keysPanel, "hide-idle",
							idle == (i32)MHUDKeysIdle::Hide ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
		layout->SetHasClass(keysPanel, "keys-underscore",
							idle == (i32)MHUDKeysIdle::Underscore ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 noBorder = prefs.keysBorder ? 0 : 1;
	if (this->layoutKeys.noBorder != noBorder)
	{
		this->layoutKeys.noBorder = noBorder;
		layout->SetHasClass(keysPanel, "keys-noborder", noBorder ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 noGlow = prefs.keysGlowEnabled ? 0 : 1;
	if (this->layoutKeys.noGlow != noGlow)
	{
		this->layoutKeys.noGlow = noGlow;
		layout->SetHasClass(keysPanel, "keys-noglow", noGlow ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 letters = prefs.keysLetters ? 1 : 0;
	if (this->layoutKeys.letters != letters)
	{
		this->layoutKeys.letters = letters;
		layout->SetHasClass(keysPanel, "keys-letters", letters ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 square = prefs.keysSquare ? 1 : 0;
	if (this->layoutKeys.square != square)
	{
		this->layoutKeys.square = square;
		layout->SetHasClass(keysPanel, "keys-square", square ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 glow = panorama::GetNearestSolidIndex(panorama::ResolveSolidColor(prefs.keysGlow, MHUD_DEF_KEYS_GLOW_COLOR));
	if (this->layoutKeys.glow != glow)
	{
		char glowClass[32];
		for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_PANELS); i++)
		{
			if (this->layoutKeys.glow >= 0)
			{
				V_snprintf(glowClass, sizeof(glowClass), "key-glow-%i", this->layoutKeys.glow);
				layout->SetHasClass(KEY_PANELS[i], glowClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			V_snprintf(glowClass, sizeof(glowClass), "key-glow-%i", glow);
			layout->SetHasClass(KEY_PANELS[i], glowClass, k_eHudPanelClassStatus_HasClass);
		}
		this->layoutKeys.glow = glow;
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

	const i32 size = panorama::SnapToStep((i32)prefs.elements[(i32)MHUDElement::Keys].size, 0, 500);
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

	const char *fontClass = KZHUDService::GetMHUDFontClass(this->player, MHUDElement::Keys);
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
	const Color color = this->GetPrefs().checkpoint;
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
		// show=false applies the hidden class and returns, so the text and color here are ignored.
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
