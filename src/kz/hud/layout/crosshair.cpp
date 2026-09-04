// Panorama replica of the player's own crosshair.

// Geometry from client.dll's painter, classic static only (no recoil, friendly-fire warning or
// weapon-based gap):
//   length    = int(screenHeight / 480 * cl_crosshairsize)
//   thickness = max(1, int(screenHeight / 480 * cl_crosshairthickness))
//   gap       = int(cl_crosshairgap + 4)              raw pixels, not screen-scaled
//   arm       = [center + thickness/2 + gap, + length]
//   dot       = thickness-sized square; cl_crosshair_t drops the top arm
//   alpha     = cl_crosshairusealpha ? cl_crosshairalpha : 200
// Panorama's reference height is 1080, so screenHeight is 1080 and the scale is constant.
//
// Each arm is anchored to the inner edge of its own half of the screen: a width, a height and a
// margin class.

#include "kz/hud/layout/layout.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/cvarquery.h"
#include "utils/ctimer.h"

#include "tier0/memdbgon.h"

// Panorama's 1080px reference over the client's 480px crosshair scale.
#define MHUD_XH_SCALE 2.25f
// Largest suffix xh-w--/xh-h-- define.
#define MHUD_XH_MAX_PX 56
// xh-m--N is a margin of N - MHUD_XH_MARGIN_BIAS pixels, so arms can cross the centre.
#define MHUD_XH_MARGIN_BIAS 8
#define MHUD_XH_MAX_MARGIN  24
// The game caps cl_crosshair_outlinethickness at 3, in raw pixels.
#define MHUD_XH_MAX_OUTLINE 3
// Opacity classes are 5% steps.
#define MHUD_XH_OPACITY_STEPS 20
#define MHUD_XH_POLL_INTERVAL 2.5f

// Alpha goes on each painted panel, not the container: parent opacity does not reach children.
static_global const char *const XH_PAINTED[] = {"xh_left",    "xh_right",    "xh_top",    "xh_bottom",    "xh_dot",
												"xh_ol_left", "xh_ol_right", "xh_ol_top", "xh_ol_bottom", "xh_ol_dot"};
static_global const char *const XH_ARMS[] = {"xh_left", "xh_right", "xh_top", "xh_bottom"};
static_global const char *const XH_HORIZONTAL[] = {"xh_left", "xh_right"};
static_global const char *const XH_VERTICAL[] = {"xh_top", "xh_bottom"};
static_global const char *const XH_TINTED[] = {"xh_left", "xh_right", "xh_top", "xh_bottom", "xh_dot"};
static_global const char *const XH_DOT[] = {"xh_dot"};
// One black panel behind each arm, grown by the outline thickness on every side.
static_global const char *const XH_OUTLINES[] = {"xh_ol_left", "xh_ol_right", "xh_ol_top", "xh_ol_bottom", "xh_ol_dot"};
static_global const char *const XH_OUTLINE_ARMS[] = {"xh_ol_left", "xh_ol_right", "xh_ol_top", "xh_ol_bottom"};
static_global const char *const XH_OUTLINE_H[] = {"xh_ol_left", "xh_ol_right"};
static_global const char *const XH_OUTLINE_V[] = {"xh_ol_top", "xh_ol_bottom"};
static_global const char *const XH_OUTLINE_DOT[] = {"xh_ol_dot"};

// === Reading the client's convars ==================================================

struct MHUDCrosshairCvar
{
	const char *name;
	void (*apply)(MHUDCrosshairSettings &settings, const char *value);
};

// The bool convars come back as "true"/"false", not 0/1.
static_function bool ParseBool(const char *value)
{
	return V_stricmp(value, "true") == 0 || V_stricmp(value, "yes") == 0 || atof(value) != 0.0;
}

// clang-format off
static_global const MHUDCrosshairCvar CROSSHAIR_CVARS[] = {
	{"cl_crosshairsize",             [](MHUDCrosshairSettings &s, const char *v) { s.size = (f32)atof(v); }},
	{"cl_crosshairthickness",        [](MHUDCrosshairSettings &s, const char *v) { s.thickness = (f32)atof(v); }},
	{"cl_crosshairgap",              [](MHUDCrosshairSettings &s, const char *v) { s.gap = (f32)atof(v); }},
	{"cl_crosshair_outlinethickness",[](MHUDCrosshairSettings &s, const char *v) { s.outlineThickness = (f32)atof(v); }},
	{"cl_crosshair_drawoutline",     [](MHUDCrosshairSettings &s, const char *v) { s.drawOutline = ParseBool(v); }},
	{"cl_crosshairdot",              [](MHUDCrosshairSettings &s, const char *v) { s.dot = ParseBool(v); }},
	{"cl_crosshair_t",               [](MHUDCrosshairSettings &s, const char *v) { s.tStyle = ParseBool(v); }},
	{"cl_crosshaircolor",            [](MHUDCrosshairSettings &s, const char *v) { s.color = atoi(v); }},
	{"cl_crosshaircolor_r",          [](MHUDCrosshairSettings &s, const char *v) { s.r = atoi(v); }},
	{"cl_crosshaircolor_g",          [](MHUDCrosshairSettings &s, const char *v) { s.g = atoi(v); }},
	{"cl_crosshaircolor_b",          [](MHUDCrosshairSettings &s, const char *v) { s.b = atoi(v); }},
	{"cl_crosshairalpha",            [](MHUDCrosshairSettings &s, const char *v) { s.alpha = atoi(v); }},
	{"cl_crosshairusealpha",         [](MHUDCrosshairSettings &s, const char *v) { s.useAlpha = ParseBool(v); }},
};
// clang-format on

static_function void OnCrosshairCvarQueried(CPlayerSlot slot, cvarquery::Status status, const char *name, const char *value)
{
	if (status != cvarquery::Status::ValueIntact)
	{
		return;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (player && player->IsInGame())
	{
		player->hudService->OnCrosshairCvarValue(name, value);
	}
}

void KZHUDService::QueryCrosshairCvars()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV())
	{
		return;
	}
	for (const MHUDCrosshairCvar &cvar : CROSSHAIR_CVARS)
	{
		cvarquery::Query(this->player->GetPlayerSlot(), cvar.name, OnCrosshairCvarQueried);
	}
}

static_function f64 PollCrosshairCvars(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (!player)
	{
		return 0.0f;
	}
	// Nothing reads the settings while the crosshair is off; the timer stays armed regardless.
	if (player->IsInGame() && player->optionService->GetPreferenceBool("mhudCrosshair", false))
	{
		player->hudService->QueryCrosshairCvars();
	}
	return MHUD_XH_POLL_INTERVAL;
}

void KZHUDService::StartCrosshairPolling()
{
	if (this->player->IsFakeClient() || this->player->IsCSTV() || !this->player->GetClient())
	{
		return;
	}
	this->crosshair = MHUDCrosshairSettings();
	this->QueryCrosshairCvars();
	StartTimer<CPlayerUserId>(PollCrosshairCvars, this->player->GetClient()->GetUserID(), MHUD_XH_POLL_INTERVAL, true, true);
}

void KZHUDService::OnCrosshairCvarValue(const char *name, const char *value)
{
	for (const MHUDCrosshairCvar &cvar : CROSSHAIR_CVARS)
	{
		if (V_strcmp(cvar.name, name) == 0)
		{
			cvar.apply(this->crosshair, value);
			return;
		}
	}
}

// === Rendering =====================================================================

// cl_crosshaircolor 0..4 are presets; 5 means the cl_crosshaircolor_* values.
static_function Color GetCrosshairColor(const MHUDCrosshairSettings &settings)
{
	switch (settings.color)
	{
		case 0:
			return Color(250, 50, 50, 255);
		case 2:
			return Color(250, 250, 50, 255);
		case 3:
			return Color(50, 50, 250, 255);
		case 4:
			return Color(50, 250, 250, 255);
		case 5:
			return Color(Clamp(settings.r, 0, 255), Clamp(settings.g, 0, 255), Clamp(settings.b, 0, 255), 255);
		default:
			return Color(50, 250, 50, 255);
	}
}

// Moves one numeric class family from oldValue to newValue on every listed panel. The caller owns
// the cache: one value can drive two families.
static_function void ApplyValueClass(CCSCustomHudLayout *layout, const char *const *panels, i32 count, const char *prefix, i32 oldValue, i32 newValue)
{
	if (oldValue == newValue)
	{
		return;
	}
	char className[32];
	for (i32 i = 0; i < count; i++)
	{
		if (oldValue >= 0)
		{
			V_snprintf(className, sizeof(className), "%s%i", prefix, oldValue);
			layout->SetHasClass(panels[i], className, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(className, sizeof(className), "%s%i", prefix, newValue);
		layout->SetHasClass(panels[i], className, k_eHudPanelClassStatus_HasClass);
	}
}

static_function void ApplyFlagClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, i32 &cache, bool set)
{
	if (cache == (i32)set)
	{
		return;
	}
	cache = (i32)set;
	layout->SetHasClass(panelId, className, set ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
}

void KZHUDService::ApplyCrosshair(CCSCustomHudLayout *layout, bool show, bool force)
{
	LayoutCrosshairState &state = this->layoutCrosshair;
	if (force)
	{
		state = LayoutCrosshairState();
	}

	const bool enabled = show && this->GetPrefs().crosshair;
	ApplyFlagClass(layout, "mhud_crosshair", "hidden", state.shown, !enabled);
	if (!enabled)
	{
		// A hidden crosshair keeps its classes, so switching it back on costs nothing.
		return;
	}

	const MHUDCrosshairSettings &settings = this->crosshair;
	const i32 armLength = Clamp((i32)(MHUD_XH_SCALE * settings.size), 0, MHUD_XH_MAX_PX);
	const i32 thickness = Clamp(MAX(1, (i32)(MHUD_XH_SCALE * settings.thickness)), 1, MHUD_XH_MAX_PX);
	const i32 gap = (i32)(settings.gap + 4.0f);
	const i32 outline = settings.drawOutline ? Clamp((i32)(settings.outlineThickness + 0.5f), 0, MHUD_XH_MAX_OUTLINE) : 0;
	// Inner edge distance; the outline starts one thickness closer and is that much bigger.
	const i32 inner = thickness / 2 + gap;
	const i32 margin = Clamp(inner, -MHUD_XH_MARGIN_BIAS, MHUD_XH_MAX_MARGIN) + MHUD_XH_MARGIN_BIAS;
	const i32 outlineMargin = Clamp(inner - outline, -MHUD_XH_MARGIN_BIAS, MHUD_XH_MAX_MARGIN) + MHUD_XH_MARGIN_BIAS;
	const i32 outlineLength = Clamp(armLength + 2 * outline, 0, MHUD_XH_MAX_PX);
	const i32 outlineThickness = Clamp(thickness + 2 * outline, 0, MHUD_XH_MAX_PX);
	// The game paints the outline with the bars' alpha.
	const i32 alpha = Clamp(settings.useAlpha ? settings.alpha : 200, 0, 255);
	const i32 opacity = alpha * MHUD_XH_OPACITY_STEPS / 255;
	const char *colorClass = panorama::ResolveSwatchClass(GetCrosshairColor(settings));

	ApplyValueClass(layout, XH_HORIZONTAL, KZ_ARRAYSIZE(XH_HORIZONTAL), "xh-w--", state.armLength, armLength);
	ApplyValueClass(layout, XH_VERTICAL, KZ_ARRAYSIZE(XH_VERTICAL), "xh-h--", state.armLength, armLength);
	state.armLength = armLength;

	ApplyValueClass(layout, XH_HORIZONTAL, KZ_ARRAYSIZE(XH_HORIZONTAL), "xh-h--", state.thickness, thickness);
	ApplyValueClass(layout, XH_VERTICAL, KZ_ARRAYSIZE(XH_VERTICAL), "xh-w--", state.thickness, thickness);
	ApplyValueClass(layout, XH_DOT, KZ_ARRAYSIZE(XH_DOT), "xh-w--", state.thickness, thickness);
	ApplyValueClass(layout, XH_DOT, KZ_ARRAYSIZE(XH_DOT), "xh-h--", state.thickness, thickness);
	state.thickness = thickness;

	ApplyValueClass(layout, XH_ARMS, KZ_ARRAYSIZE(XH_ARMS), "xh-m--", state.margin, margin);
	state.margin = margin;

	ApplyValueClass(layout, XH_OUTLINE_H, KZ_ARRAYSIZE(XH_OUTLINE_H), "xh-w--", state.outlineLength, outlineLength);
	ApplyValueClass(layout, XH_OUTLINE_V, KZ_ARRAYSIZE(XH_OUTLINE_V), "xh-h--", state.outlineLength, outlineLength);
	state.outlineLength = outlineLength;

	ApplyValueClass(layout, XH_OUTLINE_H, KZ_ARRAYSIZE(XH_OUTLINE_H), "xh-h--", state.outlineThickness, outlineThickness);
	ApplyValueClass(layout, XH_OUTLINE_V, KZ_ARRAYSIZE(XH_OUTLINE_V), "xh-w--", state.outlineThickness, outlineThickness);
	ApplyValueClass(layout, XH_OUTLINE_DOT, KZ_ARRAYSIZE(XH_OUTLINE_DOT), "xh-w--", state.outlineThickness, outlineThickness);
	ApplyValueClass(layout, XH_OUTLINE_DOT, KZ_ARRAYSIZE(XH_OUTLINE_DOT), "xh-h--", state.outlineThickness, outlineThickness);
	state.outlineThickness = outlineThickness;

	ApplyValueClass(layout, XH_OUTLINE_ARMS, KZ_ARRAYSIZE(XH_OUTLINE_ARMS), "xh-m--", state.outlineMargin, outlineMargin);
	state.outlineMargin = outlineMargin;

	ApplyValueClass(layout, XH_PAINTED, KZ_ARRAYSIZE(XH_PAINTED), "xh-op--", state.opacity, opacity);
	state.opacity = opacity;

	if (state.colorClass != colorClass)
	{
		for (const char *panelId : XH_TINTED)
		{
			if (state.colorClass)
			{
				layout->SetHasClass(panelId, state.colorClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(panelId, colorClass, k_eHudPanelClassStatus_HasClass);
		}
		state.colorClass = colorClass;
	}

	// With no outline the black panel matches the arm and would show through a translucent one.
	if (state.outline != outline)
	{
		state.outline = outline;
		for (const char *panelId : XH_OUTLINES)
		{
			layout->SetHasClass(panelId, "xh-noline", outline > 0 ? k_eHudPanelClassStatus_DoesNotHaveClass : k_eHudPanelClassStatus_HasClass);
		}
	}

	ApplyFlagClass(layout, "xh_dot", "hidden", state.dot, !settings.dot);
	ApplyFlagClass(layout, "xh_ol_dot", "hidden", state.outlineDot, !settings.dot);
	if (state.noTopArm != (i32)settings.tStyle)
	{
		state.noTopArm = (i32)settings.tStyle;
		const auto status = settings.tStyle ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass;
		layout->SetHasClass("xh_top", "hidden", status);
		layout->SetHasClass("xh_ol_top", "hidden", status);
	}
}
