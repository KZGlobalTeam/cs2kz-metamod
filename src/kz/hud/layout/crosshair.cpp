// Panorama replica of the player's own crosshair, from their cl_crosshair* values. Draws the static
// styles (3 circle, 4 cross, 6 dot); every other style falls back to the cross.

#include "kz/hud/layout/layout.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/option/menu/tables.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/cvarquery.h"
#include "utils/ctimer.h"

#include "tier0/memdbgon.h"

#define MHUD_XH_REFERENCE_HEIGHT  1080.0f
#define MHUD_XH_MIN_SCREEN_HEIGHT 240
// Every crosshair class is generated in quarter layout pixels: a whole unit is coarser than a device
// pixel above 1080p.
#define MHUD_XH_STEP 4
// Largest xh-w--/xh-h-- in pixels.
#define MHUD_XH_MAX_SIZE 96
// xh-m*--N covers -MHUD_XH_MARGIN_BIAS to MHUD_XH_MAX_MARGIN pixels, biased so it can go negative.
#define MHUD_XH_MARGIN_BIAS 48
#define MHUD_XH_MAX_MARGIN  48
#define MHUD_XH_MAX_BORDER  16
// Opacity classes are 5% steps.
#define MHUD_XH_OPACITY_STEPS 20
#define MHUD_XH_POLL_INTERVAL 2.5f

// Each panel sits in the quadrant whose corner is the screen centre and is pushed off that corner by
// the margins on its two centre-facing sides.
static_global const struct
{
	const char *id;
	const char *marginX;
	const char *marginY;
	bool tinted;
} XH_PANELS[] = {
	{"xh_left", "xh-mr--", "xh-mb--", true},   {"xh_right", "xh-ml--", "xh-mb--", true}, {"xh_top", "xh-mr--", "xh-mb--", true},
	{"xh_bottom", "xh-mr--", "xh-mt--", true}, {"xh_dot", "xh-mr--", "xh-mb--", true},   {"xh_ring_outline", "xh-mr--", "xh-mb--", false},
	{"xh_ring", "xh-mr--", "xh-mb--", true},
};

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
	{"cl_crosshair_length",        [](MHUDCrosshairSettings &s, const char *v) { s.length = atoi(v); }},
	{"cl_crosshair_thickness",     [](MHUDCrosshairSettings &s, const char *v) { s.thickness = atoi(v); }},
	{"cl_crosshair_gap",           [](MHUDCrosshairSettings &s, const char *v) { s.gap = atoi(v); }},
	{"cl_crosshair_drawoutline",   [](MHUDCrosshairSettings &s, const char *v) { s.drawOutline = ParseBool(v); }},
	{"cl_crosshairdot",            [](MHUDCrosshairSettings &s, const char *v) { s.dot = ParseBool(v); }},
	{"cl_crosshair_t",             [](MHUDCrosshairSettings &s, const char *v) { s.tStyle = ParseBool(v); }},
	{"cl_crosshairstyle",          [](MHUDCrosshairSettings &s, const char *v) { s.style = atoi(v); }},
	{"cl_crosshaircolor_r",        [](MHUDCrosshairSettings &s, const char *v) { s.r = atoi(v); }},
	{"cl_crosshaircolor_g",        [](MHUDCrosshairSettings &s, const char *v) { s.g = atoi(v); }},
	{"cl_crosshaircolor_b",        [](MHUDCrosshairSettings &s, const char *v) { s.b = atoi(v); }},
	{"cl_crosshaircolor_a",        [](MHUDCrosshairSettings &s, const char *v) { s.a = atoi(v); }},
	{"cl_crosshair_screen_height", [](MHUDCrosshairSettings &s, const char *v) { s.screenHeight = atoi(v); }},
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

// Moves one numeric class family from the cached value to newValue on one panel.
static_function void ApplyValueClass(CCSCustomHudLayout *layout, const char *panelId, const char *prefix, i32 &cache, i32 newValue)
{
	if (cache == newValue)
	{
		return;
	}
	char className[32];
	if (cache >= 0)
	{
		V_snprintf(className, sizeof(className), "%s%i", prefix, cache);
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
	}
	V_snprintf(className, sizeof(className), "%s%i", prefix, newValue);
	layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
	cache = newValue;
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

// Device pixels to the quarter-pixel class index, before clamping.
static_function i32 ToQuarters(f32 devicePixels, f32 unitsPerPixel)
{
	return (i32)roundf(devicePixels * unitsPerPixel * MHUD_XH_STEP);
}

// Panorama rounds a box's anchored edge to the nearest device pixel but its size up, so a size a hair
// over the target gains a whole pixel. Sizes quantize down instead, just under the exact value.
static_function i32 ToSizeClass(f32 devicePixels, f32 unitsPerPixel)
{
	const i32 quarters = (i32)floorf(devicePixels * unitsPerPixel * MHUD_XH_STEP - 0.01f);
	return Clamp(quarters, 0, MHUD_XH_MAX_SIZE * MHUD_XH_STEP);
}

static_function i32 ToMarginClass(f32 devicePixels, f32 unitsPerPixel)
{
	const i32 bias = MHUD_XH_MARGIN_BIAS * MHUD_XH_STEP;
	return Clamp(ToQuarters(devicePixels, unitsPerPixel), -bias, MHUD_XH_MAX_MARGIN * MHUD_XH_STEP) + bias;
}

void KZHUDService::ApplyCrosshair(CCSCustomHudLayout *layout, bool show, bool force)
{
	static_assert((i32)KZ_ARRAYSIZE(XH_PANELS) == LayoutCrosshairState::PANELS);
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
	// The client scales its sizes by screenHeight / cl_crosshair_screen_height and Panorama scales by
	// screenHeight / 1080, so the actual screen height cancels out.
	const f32 unitsPerPixel = MHUD_XH_REFERENCE_HEIGHT / MAX(settings.screenHeight, MHUD_XH_MIN_SCREEN_HEIGHT);

	const i32 thickness = MAX(settings.thickness, 0);
	const i32 length = MAX(settings.length, 0);
	const i32 gap = MAX(settings.gap, 1);
	const i32 outline = settings.drawOutline ? 1 : 0;
	const bool circle = settings.style == 3;
	const bool dotOnly = settings.style == 6;

	// Rects are pixel edges from the screen centre, spanning [-ceil(t/2), floor(t/2)) across the bar. The
	// border draws inside the box, so each box is the rect grown by the outline on every side.
	const i32 lo = MAX(thickness, 1) / 2;
	const i32 longSide = length + 2 * outline;
	const i32 shortSide = thickness + 2 * outline;
	const i32 nearEdge = gap - outline;
	const i32 farEdge = gap - (thickness % 2) - outline;
	const i32 crossEdge = -(lo + outline);
	const bool bars = !circle && !dotOnly && thickness > 0 && length > 0;

	// The ring runs from radius - width to radius, centred half a pixel up and left for odd thicknesses
	// like the rects. Its outline is a separate ring behind it, one pixel wider on each side.
	const f32 ringCentre = (thickness % 2) ? -0.5f : 0.0f;
	const i32 ringRadius = gap + thickness;
	const i32 ringWidth = MAX(thickness - 1, 1);
	const bool ring = circle && thickness > 0;

	const struct
	{
		f32 width, height, marginX, marginY;
		i32 border;
		bool visible;
	} boxes[] = {
		{(f32)longSide, (f32)shortSide, (f32)nearEdge, (f32)crossEdge, outline, bars},
		{(f32)longSide, (f32)shortSide, (f32)farEdge, (f32)crossEdge, outline, bars},
		{(f32)shortSide, (f32)longSide, (f32)crossEdge, (f32)nearEdge, outline, bars && !settings.tStyle},
		{(f32)shortSide, (f32)longSide, (f32)crossEdge, (f32)farEdge, outline, bars},
		{(f32)shortSide, (f32)shortSide, (f32)crossEdge, (f32)crossEdge, outline, thickness > 0 && (settings.dot || dotOnly)},
		{2.0f * (ringRadius + 1), 2.0f * (ringRadius + 1), -(ringCentre + ringRadius + 1), -(ringCentre + ringRadius + 1), ringWidth + 2,
		 ring && outline},
		{2.0f * ringRadius, 2.0f * ringRadius, -(ringCentre + ringRadius), -(ringCentre + ringRadius), ringWidth, ring},
	};

	// Alpha goes on each painted panel, not the container: parent opacity does not reach children. A
	// bar's outline is its own border, so it fades with the bar as the game does.
	const i32 opacity = Clamp(settings.a, 0, 255) * MHUD_XH_OPACITY_STEPS / 255;
	const char *colorClass =
		panorama::ResolveSwatchClass(Color(Clamp(settings.r, 0, 255), Clamp(settings.g, 0, 255), Clamp(settings.b, 0, 255), 255));
	for (i32 i = 0; i < LayoutCrosshairState::PANELS; i++)
	{
		const char *id = XH_PANELS[i].id;
		ApplyFlagClass(layout, id, "hidden", state.hidden[i], !boxes[i].visible);
		ApplyValueClass(layout, id, "xh-w--", state.width[i], ToSizeClass(boxes[i].width, unitsPerPixel));
		ApplyValueClass(layout, id, "xh-h--", state.height[i], ToSizeClass(boxes[i].height, unitsPerPixel));
		ApplyValueClass(layout, id, XH_PANELS[i].marginX, state.marginX[i], ToMarginClass(boxes[i].marginX, unitsPerPixel));
		ApplyValueClass(layout, id, XH_PANELS[i].marginY, state.marginY[i], ToMarginClass(boxes[i].marginY, unitsPerPixel));
		ApplyValueClass(layout, id, "xh-b--", state.border[i],
						Clamp(ToQuarters((f32)boxes[i].border, unitsPerPixel), 0, MHUD_XH_MAX_BORDER * MHUD_XH_STEP));

		i32 opacityCache = state.opacity;
		ApplyValueClass(layout, id, "xh-op--", opacityCache, opacity);
		if (XH_PANELS[i].tinted && state.colorClass != colorClass)
		{
			if (state.colorClass)
			{
				layout->SetHasClass(id, state.colorClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(id, colorClass, k_eHudPanelClassStatus_HasClass);
		}
	}
	state.opacity = opacity;
	state.colorClass = colorClass;
}
