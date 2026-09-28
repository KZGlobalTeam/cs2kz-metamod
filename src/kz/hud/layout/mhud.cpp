#include "kz/hud/layout/layout.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/mode/kz_mode.h"
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

// The border never enters a format string, so the table stays inert data.
static_function void FormatBordered(char *out, i32 outLen, const MHUDPrefs::Element &element, bool precise, f32 value)
{
	char number[16];
	V_snprintf(number, sizeof(number), precise ? "%.2f" : "%.0f", value);
	const MHUDBorderDef &border = MHUD_BORDERS[(i32)element.border];
	V_snprintf(out, outLen, "%s%s%s", border.prefix, number, border.suffix);
}

void KZHUDService::UpdateSpeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	char text[24];
	FormatBordered(text, sizeof(text), prefs.elements[(i32)MHUDElement::Speed], prefs.speedPrecise, info.speed);
	const Color color = prefs.speed[(i32)info.GetState()];
	this->UpdateLayoutElement(layout, MHUDElement::Speed, this->IsMHUDElementEnabled(MHUDElement::Speed), text, color, force);
}

void KZHUDService::UpdatePrespeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	char text[24];
	FormatBordered(text, sizeof(text), prefs.elements[(i32)MHUDElement::Prespeed], prefs.prespeedPrecise, info.takeoffSpeed);
	const Color color = prefs.prespeed[(i32)info.GetState()];
	const bool hideWalkOff = prefs.prespeedShow == MHUDPrespeedShow::JumpOrLadder;
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Prespeed) && info.showTakeoff && !(hideWalkOff && info.walkedOff);
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
	const MHUDPrefs &prefs = this->GetPrefs();
	std::string text = source->hudService->GetCheckpointText(this->player->languageService->GetLanguage());
	const bool replay = KZ::replaysystem::IsReplayBot(source);
	const i32 teleports = replay ? KZ::replaysystem::GetTeleportCount() : source->checkpointService->GetTeleportCount();
	const Color color = teleports > 0 ? prefs.checkpointTp : prefs.checkpoint;
	const bool show = this->IsMHUDElementEnabled(MHUDElement::Checkpoint) && !text.empty();
	this->UpdateLayoutElement(layout, MHUDElement::Checkpoint, show, text.c_str(), color, force);
}

void KZHUDService::UpdateIndicatorElements(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force)
{
	const MHUDPrefs &prefs = this->GetPrefs();
	// A jumpbug is a crouchbug plus a perf, so it lights the perf indicator too. MHUDSpeedState has to
	// settle on one color for the speed number and keeps them exclusive; indicators are separate lamps.
	const bool active[MHUD_INDICATOR_COUNT] = {info.perf || info.jumpbug, info.crouchJump, info.jumpbug};

	for (i32 i = 0; i < MHUD_INDICATOR_COUNT; i++)
	{
		const MHUDIndicatorDef &indicator = MHUD_INDICATOR_DEFS[i];
		const bool show = this->IsMHUDElementEnabled(indicator.element) && info.recentTakeoff && active[i];
		if (!show)
		{
			// A hidden element keeps its last text, so there is nothing to resolve.
			this->UpdateLayoutElement(layout, indicator.element, false, NULL, prefs.indicator[i], force);
			continue;
		}
		const char *phrase = prefs.indicatorAcronym[i] ? indicator.shortPhrase : indicator.fullPhrase;
		const std::string text = this->player->languageService->PrepareMessage(phrase);
		this->UpdateLayoutElement(layout, indicator.element, true, text.c_str(), prefs.indicator[i], force);
	}
}

// indexed with js tiers
static_global const char *const JS_TIER_CLASSES[DISTANCETIER_COUNT] = {"js-tier-0", "js-tier-1", "js-tier-2", "js-tier-3",
																	   "js-tier-4", "js-tier-5", "js-tier-6"};
static_global const char *const JS_TIER_PHRASES[DISTANCETIER_COUNT] = {
	NULL, "Menu - Tier Meh", "Menu - Tier Impressive", "Menu - Tier Perfect", "Menu - Tier Godlike", "Menu - Tier Ownage", "Menu - Tier Wrecker"};
// indexed with history pill age.. no class meaning its gon
static_global const char *const JS_AGE_CLASSES[MHUD_JS_PILL_GONE_RANK] = {"js-age-0", "js-age-1", "js-age-2", "js-age-3"};

// per pill: pill, type, info, distance
static_global const char *const JS_PILL_PANELS[MHUD_JS_PILL_COUNT][4] = {{"mhud_js_h0", "mhud_js_h0_t", "mhud_js_h0_i", "mhud_js_h0_d"},
																		 {"mhud_js_h1", "mhud_js_h1_t", "mhud_js_h1_i", "mhud_js_h1_d"},
																		 {"mhud_js_h2", "mhud_js_h2_t", "mhud_js_h2_i", "mhud_js_h2_d"},
																		 {"mhud_js_h3", "mhud_js_h3_t", "mhud_js_h3_i", "mhud_js_h3_d"},
																		 {"mhud_js_h4", "mhud_js_h4_t", "mhud_js_h4_i", "mhud_js_h4_d"}};

#define JS_ROW_COUNT 11
static_global const char *const JS_ROW_LABELS[JS_ROW_COUNT] = {"mhud_js_l0", "mhud_js_l1", "mhud_js_l2", "mhud_js_l3", "mhud_js_l4", "mhud_js_l5",
															   "mhud_js_l6", "mhud_js_l7", "mhud_js_l8", "mhud_js_l9", "mhud_js_l10"};
static_global const char *const JS_ROW_VALUES[JS_ROW_COUNT] = {"mhud_js_v0", "mhud_js_v1", "mhud_js_v2", "mhud_js_v3", "mhud_js_v4", "mhud_js_v5",
															   "mhud_js_v6", "mhud_js_v7", "mhud_js_v8", "mhud_js_v9", "mhud_js_v10"};

bool KZHUDService::ShowJumpstat(Jump *jump, i32 colorTier)
{
	if (!this->IsShowingPanel() || !this->IsUsingLayoutStyle())
	{
		return false;
	}
	bool created = false;
	CCSCustomHudLayout *layout = this->EnsureOwnedLayout(created);
	if (!layout)
	{
		return false;
	}
	LayoutJumpstatsState &js = this->layoutJumpstats;
	if (js.hasShown)
	{
		this->PushJumpstatHistory(layout);
	}

	KZLanguageService *lang = this->player->languageService;
	KZPlayer *jumper = jump->GetJumpPlayer();
	const JumpType type = jump->GetReportJumpType();
	const DistanceTier tier = jumper->modeService->GetDistanceTier(type, jump->GetDistance());
	const std::string typeName = lang->PrepareMessage(jumpTypeStr[type]);
	const std::string dist = lang->PrepareMessage("Jumpstats HUD - Distance", jump->GetDistance(true, false, 1));
	layout->SetDialogVariableString(
		"mhud_js_type", "v", jump->IsFailstat() ? lang->PrepareMessage("Jumpstats HUD - Failstat", typeName.c_str()).c_str() : typeName.c_str());
	layout->SetDialogVariableString("mhud_js_tier", "v", JS_TIER_PHRASES[tier] ? lang->PrepareMessage(JS_TIER_PHRASES[tier]).c_str() : "");
	layout->SetDialogVariableString("mhud_js_dist", "v", dist.c_str());
	this->SetLayoutClass(layout, "mhud_js_type", js.typeTierClass, JS_TIER_CLASSES[colorTier]);
	this->SetLayoutClass(layout, "mhud_js_dist", js.distTierClass, JS_TIER_CLASSES[colorTier]);

	const std::string labels[JS_ROW_COUNT] = {
		lang->PrepareMessage("Strafes"),
		lang->PrepareMessage("Sync"),
		lang->PrepareMessage("Pre") + " / " + lang->PrepareMessage("Max"),
		lang->PrepareMessage("Height"),
		lang->PrepareMessage("Air Time"),
		lang->PrepareMessage("Width"),
		lang->PrepareMessage("Gain Efficiency (Short)"),
		lang->PrepareMessage("Air Path"),
		lang->PrepareMessage("Bad Angles (Short)") + " / " + lang->PrepareMessage("Overlap (Short)") + " / "
			+ lang->PrepareMessage("Dead Air (Short)"),
		lang->PrepareMessage("Jumpstats HUD - Release"),
		lang->PrepareMessage("Offset"),
	};
	char values[JS_ROW_COUNT][32];
	V_snprintf(values[0], sizeof(values[0]), "%i", jump->GetStrafeCount());
	V_snprintf(values[1], sizeof(values[1]), "%.0f%%", jump->GetSync() * 100.0f);
	V_snprintf(values[2], sizeof(values[2]), "%.1f / %.1f", jump->GetTakeoffSpeed(), jump->GetMaxSpeed());
	V_snprintf(values[3], sizeof(values[3]), "%.1f", jump->GetMaxHeight());
	V_snprintf(values[4], sizeof(values[4]), "%.3fs", jumper->landingTimeActual - jumper->takeoffTime);
	V_snprintf(values[5], sizeof(values[5]), "%.1f°", jump->GetWidth());
	V_snprintf(values[6], sizeof(values[6]), "%.0f%%", jump->GetGainEfficiency() * 100.0f);
	V_snprintf(values[7], sizeof(values[7]), "%.2f", jump->GetAirPath());
	V_snprintf(values[8], sizeof(values[8]), "%.0f%% / %.0f%% / %.0f%%", jump->GetBadAngles() * 100.0f, jump->GetOverlap() * 100.0f,
			   jump->GetDeadAir() * 100.0f);
	// Jump::GetReleaseString
	const f32 release = jump->GetReleaseInTick();
	if ((type != JumpType_LongJump && type != JumpType_LadderJump && type != JumpType_WeirdJump) || release < -20)
	{
		V_strncpy(values[9], "-", sizeof(values[9]));
	}
	else if (release > 10 || release == 0)
	{
		V_strncpy(values[9], release > 10 ? "✗" : "✓", sizeof(values[9]));
	}
	else
	{
		V_snprintf(values[9], sizeof(values[9]), "%+.1f", release);
	}
	V_snprintf(values[10], sizeof(values[10]), "%+.2f", jump->GetOffset());
	for (i32 i = 0; i < JS_ROW_COUNT; i++)
	{
		layout->SetDialogVariableString(JS_ROW_LABELS[i], "v", labels[i].c_str());
		layout->SetDialogVariableString(JS_ROW_VALUES[i], "v", values[i]);
	}

	js.shownType = jumpTypeShortStr[type];
	if (jump->IsFailstat())
	{
		js.shownType += "-F";
	}
	js.shownInfo = lang->PrepareMessage("Jumpstats HUD - History Info", jump->GetStrafeCount(), jump->GetSync() * 100.0f, jump->GetTakeoffSpeed());
	js.shownDist = dist;
	js.shownTier = colorTier;
	js.hasShown = true;
	js.hideTime = g_pKZUtils->GetServerGlobals()->curtime + MHUD_JS_PANEL_TIME;
	this->SetLayoutClass(layout, "mhud_js_panel", js.panelClass, "js-show");
	return true;
}

void KZHUDService::PushJumpstatHistory(CCSCustomHudLayout *layout)
{
	LayoutJumpstatsState &js = this->layoutJumpstats;
	js.hasShown = false;
	// empty pill or oldest one
	i32 index = 0;
	for (i32 i = 0; i < MHUD_JS_PILL_COUNT; i++)
	{
		if (!js.pills[i].live)
		{
			index = i;
			break;
		}
		if (js.pills[i].rank > js.pills[index].rank)
		{
			index = i;
		}
	}
	// everything moves up in the hist pill stack, oldest one gon
	for (i32 i = 0; i < MHUD_JS_PILL_COUNT; i++)
	{
		LayoutJumpstatsState::Pill &pill = js.pills[i];
		if (pill.live && ++pill.rank >= MHUD_JS_PILL_GONE_RANK)
		{
			pill.live = false;
		}
	}

	LayoutJumpstatsState::Pill &pill = js.pills[index];
	pill.born = g_pKZUtils->GetServerGlobals()->curtime;
	pill.rank = 0;
	pill.live = true;
	pill.expired = false;
	pill.alt = !pill.alt;
	const char *const *ids = JS_PILL_PANELS[index];
	layout->SetDialogVariableString(ids[1], "v", js.shownType.c_str());
	layout->SetDialogVariableString(ids[2], "v", js.shownInfo.c_str());
	layout->SetDialogVariableString(ids[3], "v", js.shownDist.c_str());
	this->SetLayoutClass(layout, ids[1], pill.typeTierClass, JS_TIER_CLASSES[js.shownTier]);
	this->SetLayoutClass(layout, ids[3], pill.distTierClass, JS_TIER_CLASSES[js.shownTier]);
}

void KZHUDService::UpdateJumpstatsElement(CCSCustomHudLayout *layout, bool show, bool force)
{
	LayoutJumpstatsState &js = this->layoutJumpstats;
	if (force)
	{
		js = LayoutJumpstatsState();
	}
	this->SetLayoutClass(layout, "mhud_js", js.stackClass, show ? NULL : "hidden");

	const f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (js.hasShown && now >= js.hideTime)
	{
		this->PushJumpstatHistory(layout);
		this->SetLayoutClass(layout, "mhud_js_panel", js.panelClass, "js-hide");
	}
	for (i32 i = 0; i < MHUD_JS_PILL_COUNT; i++)
	{
		LayoutJumpstatsState::Pill &pill = js.pills[i];
		if (pill.live && now - pill.born >= MHUD_JS_HISTORY_TIME)
		{
			pill.live = false;
			pill.expired = true;
		}
		const char *age = NULL;
		if (pill.live || pill.expired)
		{
			age = pill.rank == 0 && pill.alt ? "js-age-0b" : JS_AGE_CLASSES[pill.rank];
		}
		this->SetLayoutClass(layout, JS_PILL_PANELS[i][0], pill.ageClass, age);
		this->SetLayoutClass(layout, JS_PILL_PANELS[i][0], pill.expireClass, pill.expired ? "js-expire" : NULL);
	}
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
	this->UpdateJumpstatsElement(layout, show, force);

	if (!show)
	{
		// show=false applies the hidden class and returns, so the text and color here are ignored.
		for (i32 i = 0; i < (i32)MHUDElement::Count; i++)
		{
			this->UpdateLayoutElement(layout, (MHUDElement)i, false, NULL, MHUD_DEF_BASE_COLOR, force);
		}
		return true;
	}

	const SpeedInfo info = source->hudService->GetSpeedInfo(this->GetPrefs());
	this->UpdateTimerElement(layout, source, force);
	this->UpdateSpeedElement(layout, info, force);
	this->UpdatePrespeedElement(layout, info, force);
	this->UpdateKeysElement(layout, source, force);
	this->UpdateCheckpointElement(layout, source, force);
	this->UpdateIndicatorElements(layout, info, force);
	return true;
}
