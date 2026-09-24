// Panorama replica of the player's own crosshair, from their cl_crosshair* values. Draws the static
// styles (3 circle, 4 cross, 6 dot, 8 square); every other style falls back to the cross.

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
// the margins on its two centre-facing sides. The four bars draw the square's sides too.
static_global const struct
{
	const char *id;
	bool leftQuadrant;
	bool topQuadrant;
	bool tinted;
} XH_PANELS[] = {
	{"xh_left", true, true, true}, {"xh_right", false, true, true},        {"xh_top", true, true, true},  {"xh_bottom", true, false, true},
	{"xh_dot", true, true, true},  {"xh_ring_outline", true, true, false}, {"xh_ring", true, true, true},
};

enum XHPanel
{
	XH_LEFT,
	XH_RIGHT,
	XH_TOP,
	XH_BOTTOM,
	XH_DOT,
	XH_RING_OUTLINE,
	XH_RING,
	XH_PANEL_COUNT,
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
	{"cl_crosshair_drawoutline",   [](MHUDCrosshairSettings &s, const char *v) { s.outline = atoi(v); }},
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

// Pixel edges from the screen centre, [x0, x1) by [y0, y1), and the border widths on each side. The
// border draws inside the box, so the box already includes it.
struct XHBox
{
	f32 x0, y0, x1, y1;
	i32 borderTopLeft, borderBottomRight;
	bool visible;
};

// cl_crosshair_drawoutline: a full outline is a pixel on every side, a half one only on the top and left.
struct XHOutline
{
	i32 topLeft, bottomRight;
};

// A filled rect with the game's outline around it.
static_function XHBox MakeOutlinedBox(f32 x0, f32 y0, f32 x1, f32 y1, XHOutline outline, bool visible)
{
	return {x0 - outline.topLeft,
			y0 - outline.topLeft,
			x1 + outline.bottomRight,
			y1 + outline.bottomRight,
			outline.topLeft,
			outline.bottomRight,
			visible};
}

// Bars span [-ceil(t/2), floor(t/2)) across; the far bars start a pixel closer for odd t.
static_function void BuildCrossBoxes(const MHUDCrosshairSettings &settings, XHOutline outline, XHBox *boxes)
{
	const i32 t = settings.thickness;
	const i32 length = settings.length;
	const f32 hi = (f32)((t + 1) / 2);
	const f32 lo = (f32)(t / 2);
	const f32 nearEdge = (f32)-settings.gap;
	const f32 farEdge = (f32)(settings.gap - t % 2);
	const bool visible = t > 0 && length > 0;
	boxes[XH_LEFT] = MakeOutlinedBox(nearEdge - length, -hi, nearEdge, lo, outline, visible);
	boxes[XH_RIGHT] = MakeOutlinedBox(farEdge, -hi, farEdge + length, lo, outline, visible);
	boxes[XH_TOP] = MakeOutlinedBox(-hi, nearEdge - length, lo, nearEdge, outline, visible && !settings.tStyle);
	boxes[XH_BOTTOM] = MakeOutlinedBox(-hi, farEdge, lo, farEdge + length, outline, visible);
}

// Sides t thick around a 2 * gap hole; a dot with odd t pulls the top and left out a pixel. Top and
// bottom stop where the right side, which spans the full height, begins.
static_function void BuildSquareBoxes(const MHUDCrosshairSettings &settings, XHOutline outline, XHBox *boxes)
{
	const i32 t = settings.thickness;
	const f32 outer = (f32)(-settings.gap - t - (settings.dot && t % 2 == 1));
	const f32 inner = (f32)settings.gap;
	boxes[XH_LEFT] = MakeOutlinedBox(outer, outer, outer + t, inner + t, outline, t > 0);
	boxes[XH_RIGHT] = MakeOutlinedBox(inner, outer, inner + t, inner + t, outline, t > 0);
	boxes[XH_TOP] = MakeOutlinedBox(outer, outer, inner, outer + t, outline, t > 0);
	boxes[XH_BOTTOM] = MakeOutlinedBox(outer, inner, inner, inner + t, outline, t > 0);
}

// The ring runs from radius - width to radius, centred half a pixel up and left for odd t like the
// bars. Its outline is a second ring behind it, grown on the sides the outline covers. The game fades a
// half outline around the ring, so that one is only approximate.
static_function void BuildRingBoxes(const MHUDCrosshairSettings &settings, XHOutline outline, XHBox *boxes)
{
	const i32 t = settings.thickness;
	const f32 centre = t % 2 == 1 ? -0.5f : 0.0f;
	const f32 radius = (f32)(MAX(settings.gap, 1) + t);
	const i32 width = MAX(t - 1, 1);
	const i32 outlineWidth = width + outline.topLeft + outline.bottomRight;
	const f32 outlineStart = centre - radius - outline.topLeft;
	const f32 outlineEnd = centre + radius + outline.bottomRight;
	boxes[XH_RING_OUTLINE] = {outlineStart, outlineStart, outlineEnd, outlineEnd, outlineWidth, outlineWidth, t > 0 && outline.topLeft > 0};
	boxes[XH_RING] = {centre - radius, centre - radius, centre + radius, centre + radius, width, width, t > 0};
}

void KZHUDService::ApplyCrosshair(CCSCustomHudLayout *layout, bool show, bool force)
{
	static_assert(XH_PANEL_COUNT == LayoutCrosshairState::PANELS && (i32)KZ_ARRAYSIZE(XH_PANELS) == XH_PANEL_COUNT);
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

	MHUDCrosshairSettings settings = this->crosshair;
	settings.thickness = MAX(settings.thickness, 0);
	settings.length = MAX(settings.length, 0);
	settings.gap = MAX(settings.gap, 0);
	const XHOutline outline = {settings.outline > 0 ? 1 : 0, settings.outline == 1 ? 1 : 0};

	// Everything not built below stays hidden.
	XHBox boxes[XH_PANEL_COUNT] {};
	switch (settings.style)
	{
		case 3:
			BuildRingBoxes(settings, outline, boxes);
			break;
		case 6:
			break;
		case 8:
			BuildSquareBoxes(settings, outline, boxes);
			break;
		default:
			BuildCrossBoxes(settings, outline, boxes);
			break;
	}
	const f32 hi = (f32)((settings.thickness + 1) / 2);
	const f32 lo = (f32)(settings.thickness / 2);
	boxes[XH_DOT] = MakeOutlinedBox(-hi, -hi, lo, lo, outline, settings.thickness > 0 && (settings.dot || settings.style == 6));

	// The client scales its sizes by screenHeight / cl_crosshair_screen_height and Panorama scales by
	// screenHeight / 1080, so the actual screen height cancels out.
	const f32 unitsPerPixel = MHUD_XH_REFERENCE_HEIGHT / MAX(settings.screenHeight, MHUD_XH_MIN_SCREEN_HEIGHT);
	const i32 maxBorder = MHUD_XH_MAX_BORDER * MHUD_XH_STEP;
	// Alpha goes on each painted panel, not the container: parent opacity does not reach children. A
	// bar's outline is its own border, so it fades with the bar as the game does.
	const i32 opacity = Clamp(settings.a, 0, 255) * MHUD_XH_OPACITY_STEPS / 255;
	const char *colorClass =
		panorama::ResolveSwatchClass(Color(Clamp(settings.r, 0, 255), Clamp(settings.g, 0, 255), Clamp(settings.b, 0, 255), 255));
	for (i32 i = 0; i < XH_PANEL_COUNT; i++)
	{
		const char *id = XH_PANELS[i].id;
		const XHBox &box = boxes[i];
		const f32 marginX = XH_PANELS[i].leftQuadrant ? -box.x1 : box.x0;
		const f32 marginY = XH_PANELS[i].topQuadrant ? -box.y1 : box.y0;

		ApplyFlagClass(layout, id, "hidden", state.hidden[i], !box.visible);
		ApplyValueClass(layout, id, "xh-w--", state.width[i], ToSizeClass(box.x1 - box.x0, unitsPerPixel));
		ApplyValueClass(layout, id, "xh-h--", state.height[i], ToSizeClass(box.y1 - box.y0, unitsPerPixel));
		ApplyValueClass(layout, id, XH_PANELS[i].leftQuadrant ? "xh-mr--" : "xh-ml--", state.marginX[i], ToMarginClass(marginX, unitsPerPixel));
		ApplyValueClass(layout, id, XH_PANELS[i].topQuadrant ? "xh-mb--" : "xh-mt--", state.marginY[i], ToMarginClass(marginY, unitsPerPixel));
		ApplyValueClass(layout, id, "xh-btl--", state.borderTopLeft[i], Clamp(ToQuarters((f32)box.borderTopLeft, unitsPerPixel), 0, maxBorder));
		ApplyValueClass(layout, id, "xh-bbr--", state.borderBottomRight[i],
						Clamp(ToQuarters((f32)box.borderBottomRight, unitsPerPixel), 0, maxBorder));

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
