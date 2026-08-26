#pragma once
#include "kz/kz.h"

// The generated Panorama utility sheets, as tables. Nothing here is HUD specific: the menu chrome
// picks a font and a color out of the same sheets the movement HUD does.

struct PanoramaFontDef
{
	const char *slug; // stored preference value, and the class suffix
	const char *className;
	const char *displayName;
	const char *family;
	const char *variant;
};

struct PanoramaColorDef
{
	const char *className;
	u8 r, g, b;
};

#define PANORAMA_COLOR_NAME_OFFSET 7 // strlen("color--")

extern const PanoramaFontDef PANORAMA_FONTS[];
extern const i32 PANORAMA_FONT_COUNT;
extern const PanoramaColorDef PANORAMA_COLORS[];
extern const i32 PANORAMA_COLOR_COUNT;

// The generated color class closest to an arbitrary RGB.
// Class-only styling + limit on available classes means we cannot do better.
const char *PanoramaNearestColorClass(const Color &color);

const char *PanoramaResolveFontSlug(const char *name, const char *fallback);
const char *PanoramaFontClass(const char *name, const char *fallback);
const char *PanoramaFontDisplayName(const char *name, const char *fallback);

// Clamp to a value the generated sheets actually define.
i32 PanoramaSnapToStep(i32 value, i32 lo, i32 hi);
