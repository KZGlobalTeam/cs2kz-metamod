#pragma once
#include "kz/kz.h"

// Curated Panorama utility sheets, as tables. Only the font table and its element type are shared
// (the menu indexes it directly); the colour and gradient tables, their element types, and the
// palette/nearest helpers all live in tables.cpp and are reached through the functions declared here.
struct PanoramaFontDef
{
	const char *slug; // stored preference value, and the class suffix
	const char *className;
	const char *displayName;
	const char *family;
	const char *variant;
};

extern const PanoramaFontDef PANORAMA_FONTS[];
extern const i32 PANORAMA_FONT_COUNT;

namespace panorama
{
	// A gradient color is stored as a marker Color: alpha == 1, red == gradient index. Solid colors
	// are always stored with alpha 255, so alpha 1 is an unambiguous gradient flag.
	inline bool IsGradient(const Color &c)
	{
		return c.a() == 1;
	}

	inline Color MakeGradient(i32 index)
	{
		return Color((u8)index, 0, 0, 1);
	}

	inline i32 GetGradientIndex(const Color &c)
	{
		return c.r();
	}

	// A gradient has no single RGB, so anything that needs a real color (the legacy HTML HUD) resolves
	// a gradient marker to the given fallback instead.
	inline Color ResolveSolidColor(const Color &c, const Color &fallback)
	{
		return IsGradient(c) ? fallback : c;
	}

	// Gradient-aware resolvers: return the grad-* class for a stored gradient, else the nearest solid.
	const char *ResolveColorClass(const Color &color);  // text (color)
	const char *ResolveSwatchClass(const Color &color); // swatch (background-color)

	// The color picker's combined entry space: solids first, then gradients.
	i32 GetColorEntryCount();
	const char *GetColorEntryBgClass(i32 entry);
	Color GetColorEntryValue(i32 entry);      // the color to store when this entry is picked
	i32 FindColorEntry(const Color &color);   // which entry a stored color matches, -1 if none

	const char *ResolveFontSlug(const char *name, const char *fallback);
	const char *ResolveFontClass(const char *name, const char *fallback);
	const char *GetFontDisplayName(const char *name, const char *fallback);

	// Clamp to a value the generated sheets actually define.
	i32 SnapToStep(i32 value, i32 lo, i32 hi);
} // namespace panorama
