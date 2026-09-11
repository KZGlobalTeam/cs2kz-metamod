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
	const MHUDPrefs &prefs = this->GetPrefs();
	std::string text = source->hudService->GetTimerText(this->player->languageService->GetLanguage(), prefs.timerShowState);
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
	const Color color = prefs.speed[(i32)info.GetState()];
	this->UpdateLayoutElement(layout, MHUDElement::Speed, this->IsMHUDElementEnabled(MHUDElement::Speed), text, color, force);
}

void KZHUDService::UpdatePrespeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	char text[16];
	const char *format = prefs.prespeedBrackets ? (prefs.prespeedPrecise ? "(%.2f)" : "(%.0f)") : (prefs.prespeedPrecise ? "%.2f" : "%.0f");
	V_snprintf(text, sizeof(text), format, info.takeoffSpeed);
	const Color color = prefs.prespeed[(i32)info.GetState()];
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Prespeed) && info.showTakeoff && !(prefs.prespeedHideWalkOff && info.walkedOff);
	this->UpdateLayoutElement(layout, MHUDElement::Prespeed, show, text, color, force);
}

// Order matches the panels in mhud.xml: C W J on the top row, A S D on the bottom.
static_global const char *KEY_PANELS[] = {"mhud_key_c", "mhud_key_w", "mhud_key_j", "mhud_key_a", "mhud_key_s", "mhud_key_d"};

// The glyph labels inside those buttons. A child only restyles when it is touched itself, so the
// font class goes on these rather than on the buttons.
static_global const char *KEY_GLYPHS[] = {"mhud_kg_c_main",   "mhud_kg_c_idle", "mhud_kg_w_main",   "mhud_kg_w_letter",
										  "mhud_kg_w_idle",   "mhud_kg_j_main", "mhud_kg_j_idle",   "mhud_kg_a_main",
										  "mhud_kg_a_letter", "mhud_kg_a_idle", "mhud_kg_s_main",   "mhud_kg_s_letter",
										  "mhud_kg_s_idle",   "mhud_kg_d_main", "mhud_kg_d_letter", "mhud_kg_d_idle"};

// The movement axis each panel sits on, so an overlap can be shown on just the keys causing it.
enum KeyAxis
{
	KEY_AXIS_NONE = -1,
	KEY_AXIS_FORWARD_BACK,
	KEY_AXIS_LEFT_RIGHT,
};

static_global const i32 KEY_AXES[] = {KEY_AXIS_NONE,       KEY_AXIS_FORWARD_BACK, KEY_AXIS_NONE,
									  KEY_AXIS_LEFT_RIGHT, KEY_AXIS_FORWARD_BACK, KEY_AXIS_LEFT_RIGHT};

void KZHUDService::UpdateKeysElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force)
{
	CPlayer_MovementServices *ms = source->hudService->GetHudMoveServices();
	CInButtonState *buttons = ms ? &ms->m_nButtons() : nullptr;
	auto pressed = [buttons](InputBitMask_t button) { return buttons && buttons->IsButtonPressed(button, false); };
	const bool left = pressed(IN_MOVELEFT), forward = pressed(IN_FORWARD), back = pressed(IN_BACK), right = pressed(IN_MOVERIGHT);
	const bool keys[] = {pressed(IN_DUCK), forward, source->hudService->JumpedThisTick(), left, back, right};

	const MHUDPrefs &prefs = this->GetPrefs();
	const bool overlap = ((forward && back) || (left && right)) && prefs.keysOverlapEnabled;
	bool overlapped[KZ_ARRAYSIZE(KEY_PANELS)] {};
	for (i32 i = 0; overlap && i < KZ_ARRAYSIZE(KEY_PANELS); i++)
	{
		overlapped[i] = !prefs.keysOverlapAxis || (KEY_AXES[i] == KEY_AXIS_FORWARD_BACK && forward && back)
						|| (KEY_AXES[i] == KEY_AXIS_LEFT_RIGHT && left && right);
	}
	// Axis mode leaves the element on the base color and tints the offending keys one by one below.
	const Color color = overlap && !prefs.keysOverlapAxis ? prefs.keysOverlap : prefs.keys;
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
	const i32 boxOutline = prefs.keysBoxOutline ? 1 : 0;
	if (this->layoutKeys.boxOutline != boxOutline)
	{
		this->layoutKeys.boxOutline = boxOutline;
		layout->SetHasClass(keysPanel, "keys-boxoutline", boxOutline ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 noGlow = prefs.keysGlowEnabled ? 0 : 1;
	if (this->layoutKeys.noGlow != noGlow)
	{
		this->layoutKeys.noGlow = noGlow;
		layout->SetHasClass(keysPanel, "keys-noglow", noGlow ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	const i32 noFill = prefs.keysFillEnabled ? 0 : 1;
	if (this->layoutKeys.noFill != noFill)
	{
		this->layoutKeys.noFill = noFill;
		layout->SetHasClass(keysPanel, "keys-nofill", noFill ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
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
	const i32 glow = panorama::GetNearestSolidIndex(panorama::ResolveSolidColor(prefs.keysPressed, MHUD_DEF_KEYS_PRESSED_COLOR));
	const i32 glowOverlap =
		overlap ? panorama::GetNearestSolidIndex(panorama::ResolveSolidColor(prefs.keysOverlapGlow, MHUD_DEF_KEYS_OVERLAP_GLOW_COLOR)) : glow;
	const char *overlapClass = prefs.keysOverlapAxis ? panorama::ResolveColorClass(prefs.keysOverlap) : NULL;
	for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_PANELS); i++)
	{
		// The element's own color is inherited, so a class here overrides it for this key alone.
		this->SetLayoutClass(layout, KEY_PANELS[i], this->layoutKeys.overlapClass[i], overlapped[i] ? overlapClass : NULL);

		const i32 wanted = overlapped[i] ? glowOverlap : glow;
		if (this->layoutKeys.glow[i] == wanted)
		{
			continue;
		}
		char glowClass[32];
		if (this->layoutKeys.glow[i] >= 0)
		{
			V_snprintf(glowClass, sizeof(glowClass), "key-glow-%i", this->layoutKeys.glow[i]);
			layout->SetHasClass(KEY_PANELS[i], glowClass, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(glowClass, sizeof(glowClass), "key-glow-%i", wanted);
		layout->SetHasClass(KEY_PANELS[i], glowClass, k_eHudPanelClassStatus_HasClass);
		this->layoutKeys.glow[i] = wanted;
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

	// keys-size.css scales the boxes and their gaps with the glyph; one class on the keys panel.
	const i32 boxSize = Clamp((i32)prefs.elements[(i32)MHUDElement::Keys].size, MHUD_SIZE_MIN, MHUD_SIZE_MAX);
	if (this->layoutKeys.boxSize != boxSize)
	{
		char className[32];
		if (this->layoutKeys.boxSize != INT_MIN)
		{
			V_snprintf(className, sizeof(className), "key-size--%i", this->layoutKeys.boxSize);
			layout->SetHasClass(keysPanel, className, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(className, sizeof(className), "key-size--%i", boxSize);
		layout->SetHasClass(keysPanel, className, k_eHudPanelClassStatus_HasClass);
		this->layoutKeys.boxSize = boxSize;
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

	const i32 outline = this->IsMHUDOutlineEnabled(MHUDElement::Keys) ? 1 : 0;
	if (this->layoutKeys.outline != outline)
	{
		this->layoutKeys.outline = outline;
		const auto status = outline ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass;
		for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_GLYPHS); i++)
		{
			layout->SetHasClass(KEY_GLYPHS[i], "outline", status);
		}
	}

	const char *fontClass = KZHUDService::GetMHUDFontClass(this->player, MHUDElement::Keys);
	if (this->layoutKeys.fontClass != fontClass)
	{
		for (i32 i = 0; i < KZ_ARRAYSIZE(KEY_GLYPHS); i++)
		{
			if (this->layoutKeys.fontClass)
			{
				layout->SetHasClass(KEY_GLYPHS[i], this->layoutKeys.fontClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(KEY_GLYPHS[i], fontClass, k_eHudPanelClassStatus_HasClass);
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
