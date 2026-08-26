#include "kz/option/menu/tables.h"

#include "tier0/memdbgon.h"

// clang-format off
// slug, className, displayName, family, variant
// TODO: Nuke this table when custom fonts are supported?
extern const PanoramaFontDef PANORAMA_FONTS[] =
{
	{"stratum2", "font-family--stratum2", "Stratum2", "Stratum2", "Regular"},
	{"stratum2-thin", "font-family--stratum2-thin", "Stratum2 Thin", "Stratum2", "Thin"},
	{"stratum2-thin-italic", "font-family--stratum2-thin-italic", "Stratum2 Thin Italic", "Stratum2", "Thin Italic"},
	{"stratum2-thin-condensed", "font-family--stratum2-thin-condensed", "Stratum2 Thin Condensed", "Stratum2", "Thin Condensed"},
	{"stratum2-thin-tf", "font-family--stratum2-thin-tf", "Stratum2 Thin TF", "Stratum2", "Thin TF"},
	{"stratum2-light", "font-family--stratum2-light", "Stratum2 Light", "Stratum2", "Light"},
	{"stratum2-light-italic", "font-family--stratum2-light-italic", "Stratum2 Light Italic", "Stratum2", "Light Italic"},
	{"stratum2-light-condensed", "font-family--stratum2-light-condensed", "Stratum2 Light Condensed", "Stratum2", "Light Condensed"},
	{"stratum2-light-tf", "font-family--stratum2-light-tf", "Stratum2 Light TF", "Stratum2", "Light TF"},
	{"stratum2-italic", "font-family--stratum2-italic", "Stratum2 Italic", "Stratum2", "Italic"},
	{"stratum2-condensed", "font-family--stratum2-condensed", "Stratum2 Condensed", "Stratum2", "Condensed"},
	{"stratum2-tf", "font-family--stratum2-tf", "Stratum2 TF", "Stratum2", "TF"},
	{"stratum2-medium", "font-family--stratum2-medium", "Stratum2 Medium", "Stratum2", "Medium"},
	{"stratum2-medium-italic", "font-family--stratum2-medium-italic", "Stratum2 Medium Italic", "Stratum2", "Medium Italic"},
	{"stratum2-medium-condensed", "font-family--stratum2-medium-condensed", "Stratum2 Medium Condensed", "Stratum2", "Medium Condensed"},
	{"stratum2-medium-tf", "font-family--stratum2-medium-tf", "Stratum2 Medium TF", "Stratum2", "Medium TF"},
	{"stratum2-bold", "font-family--stratum2-bold", "Stratum2 Bold", "Stratum2", "Bold"},
	{"stratum2-bold-italic", "font-family--stratum2-bold-italic", "Stratum2 Bold Italic", "Stratum2", "Bold Italic"},
	{"stratum2-bold-condensed", "font-family--stratum2-bold-condensed", "Stratum2 Bold Condensed", "Stratum2", "Bold Condensed"},
	{"stratum2-bold-tf", "font-family--stratum2-bold-tf", "Stratum2 Bold TF", "Stratum2", "Bold TF"},
	{"stratum2-black", "font-family--stratum2-black", "Stratum2 Black", "Stratum2", "Black"},
	{"stratum2-black-italic", "font-family--stratum2-black-italic", "Stratum2 Black Italic", "Stratum2", "Black Italic"},
	{"stratum2-black-condensed", "font-family--stratum2-black-condensed", "Stratum2 Black Condensed", "Stratum2", "Black Condensed"},
	{"stratum2-black-tf", "font-family--stratum2-black-tf", "Stratum2 Black TF", "Stratum2", "Black TF"},
	{"stratum2-mono", "font-family--stratum2-mono", "Stratum2 Mono", "Stratum2", "Mono"},
	{"stratum2-mono-light", "font-family--stratum2-mono-light", "Stratum2 Mono Light", "Stratum2", "Mono Light"},
	{"stratum2-mono-bold", "font-family--stratum2-mono-bold", "Stratum2 Mono Bold", "Stratum2", "Mono Bold"},
	{"stratum2-regular-monodigit", "font-family--stratum2-regular-monodigit", "Stratum2 Regular Monodigit", "Stratum2", "Regular Monodigit"},
	{"stratum2-bold-monodigit", "font-family--stratum2-bold-monodigit", "Stratum2 Bold Monodigit", "Stratum2", "Bold Monodigit"},
	{"forcestratum2", "font-family--forcestratum2", "ForceStratum2", "ForceStratum2", "Regular"},
	{"noto-sans", "font-family--noto-sans", "Noto Sans", "Noto Sans", "Regular"},
	{"noto-sans-italic", "font-family--noto-sans-italic", "Noto Sans Italic", "Noto Sans", "Italic"},
	{"noto-sans-bold", "font-family--noto-sans-bold", "Noto Sans Bold", "Noto Sans", "Bold"},
	{"noto-sans-bold-italic", "font-family--noto-sans-bold-italic", "Noto Sans Bold Italic", "Noto Sans", "Bold Italic"},
	{"noto-serif", "font-family--noto-serif", "Noto Serif", "Noto Serif", "Regular"},
	{"noto-serif-italic", "font-family--noto-serif-italic", "Noto Serif Italic", "Noto Serif", "Italic"},
	{"noto-serif-bold", "font-family--noto-serif-bold", "Noto Serif Bold", "Noto Serif", "Bold"},
	{"noto-serif-bold-italic", "font-family--noto-serif-bold-italic", "Noto Serif Bold Italic", "Noto Serif", "Bold Italic"},
	{"noto-mono", "font-family--noto-mono", "Noto Mono", "Noto Mono", "Regular"},
	{"noto-sans-symbols", "font-family--noto-sans-symbols", "Noto Sans Symbols", "Noto Sans Symbols", "Regular"},
	{"noto-sans-jp-light", "font-family--noto-sans-jp-light", "Noto Sans JP Light", "Noto Sans JP", "Light"},
	{"noto-sans-jp", "font-family--noto-sans-jp", "Noto Sans JP", "Noto Sans JP", "Regular"},
	{"noto-sans-jp-bold", "font-family--noto-sans-jp-bold", "Noto Sans JP Bold", "Noto Sans JP", "Bold"},
	{"noto-sans-kr-light", "font-family--noto-sans-kr-light", "Noto Sans KR Light", "Noto Sans KR", "Light"},
	{"noto-sans-kr", "font-family--noto-sans-kr", "Noto Sans KR", "Noto Sans KR", "Regular"},
	{"noto-sans-kr-bold", "font-family--noto-sans-kr-bold", "Noto Sans KR Bold", "Noto Sans KR", "Bold"},
	{"noto-sans-sc-light", "font-family--noto-sans-sc-light", "Noto Sans SC Light", "Noto Sans SC", "Light"},
	{"noto-sans-sc", "font-family--noto-sans-sc", "Noto Sans SC", "Noto Sans SC", "Regular"},
	{"noto-sans-sc-bold", "font-family--noto-sans-sc-bold", "Noto Sans SC Bold", "Noto Sans SC", "Bold"},
	{"noto-sans-tc-light", "font-family--noto-sans-tc-light", "Noto Sans TC Light", "Noto Sans TC", "Light"},
	{"noto-sans-tc", "font-family--noto-sans-tc", "Noto Sans TC", "Noto Sans TC", "Regular"},
	{"noto-sans-tc-bold", "font-family--noto-sans-tc-bold", "Noto Sans TC Bold", "Noto Sans TC", "Bold"},
	{"noto-sans-thai-light", "font-family--noto-sans-thai-light", "Noto Sans Thai Light", "Noto Sans Thai", "Light"},
	{"noto-sans-thai", "font-family--noto-sans-thai", "Noto Sans Thai", "Noto Sans Thai", "Regular"},
	{"noto-sans-thai-bold", "font-family--noto-sans-thai-bold", "Noto Sans Thai Bold", "Noto Sans Thai", "Bold"},
	{"arial", "font-family--arial", "Arial", "Arial", "Regular"},
	{"sans-serif", "font-family--sans-serif", "sans-serif", "sans-serif", "Regular"},
	{"serif", "font-family--serif", "serif", "serif", "Regular"},
	{"monospace", "font-family--monospace", "monospace", "monospace", "Regular"},
};
// clang-format on

extern const i32 PANORAMA_FONT_COUNT = KZ_ARRAYSIZE(PANORAMA_FONTS);

// clang-format off
extern const PanoramaColorDef PANORAMA_COLORS[] =
{
	{"color--aliceblue", 0xF0, 0xF8, 0xFF},
	{"color--antiquewhite", 0xFA, 0xEB, 0xD7},
	{"color--aqua", 0x00, 0xFF, 0xFF},
	{"color--aquamarine", 0x7F, 0xFF, 0xD4},
	{"color--azure", 0xF0, 0xFF, 0xFF},
	{"color--beige", 0xF5, 0xF5, 0xDC},
	{"color--bisque", 0xFF, 0xE4, 0xC4},
	{"color--black", 0x00, 0x00, 0x00},
	{"color--blanchedalmond", 0xFF, 0xEB, 0xCD},
	{"color--blue", 0x00, 0x00, 0xFF},
	{"color--blueviolet", 0x8A, 0x2B, 0xE2},
	{"color--brown", 0xA5, 0x2A, 0x2A},
	{"color--burlywood", 0xDE, 0xB8, 0x87},
	{"color--cadetblue", 0x5F, 0x9E, 0xA0},
	{"color--chartreuse", 0x7F, 0xFF, 0x00},
	{"color--chocolate", 0xD2, 0x69, 0x1E},
	{"color--coral", 0xFF, 0x7F, 0x50},
	{"color--cornflowerblue", 0x64, 0x95, 0xED},
	{"color--cornsilk", 0xFF, 0xF8, 0xDC},
	{"color--crimson", 0xDC, 0x14, 0x3C},
	{"color--cyan", 0x00, 0xFF, 0xFF},
	{"color--darkblue", 0x00, 0x00, 0x8B},
	{"color--darkcyan", 0x00, 0x8B, 0x8B},
	{"color--darkgoldenrod", 0xB8, 0x86, 0x0B},
	{"color--darkgray", 0xA9, 0xA9, 0xA9},
	{"color--darkgreen", 0x00, 0x64, 0x00},
	{"color--darkgrey", 0xA9, 0xA9, 0xA9},
	{"color--darkkhaki", 0xBD, 0xB7, 0x6B},
	{"color--darkmagenta", 0x8B, 0x00, 0x8B},
	{"color--darkolivegreen", 0x55, 0x6B, 0x2F},
	{"color--darkorange", 0xFF, 0x8C, 0x00},
	{"color--darkorchid", 0x99, 0x32, 0xCC},
	{"color--darkred", 0x8B, 0x00, 0x00},
	{"color--darksalmon", 0xE9, 0x96, 0x7A},
	{"color--darkseagreen", 0x8F, 0xBC, 0x8F},
	{"color--darkslateblue", 0x48, 0x3D, 0x8B},
	{"color--darkslategray", 0x2F, 0x4F, 0x4F},
	{"color--darkslategrey", 0x2F, 0x4F, 0x4F},
	{"color--darkturquoise", 0x00, 0xCE, 0xD1},
	{"color--darkviolet", 0x94, 0x00, 0xD3},
	{"color--deeppink", 0xFF, 0x14, 0x93},
	{"color--deepskyblue", 0x00, 0xBF, 0xFF},
	{"color--dimgray", 0x69, 0x69, 0x69},
	{"color--dimgrey", 0x69, 0x69, 0x69},
	{"color--dodgerblue", 0x1E, 0x90, 0xFF},
	{"color--firebrick", 0xB2, 0x22, 0x22},
	{"color--floralwhite", 0xFF, 0xFA, 0xF0},
	{"color--forestgreen", 0x22, 0x8B, 0x22},
	{"color--fuchsia", 0xFF, 0x00, 0xFF},
	{"color--gainsboro", 0xDC, 0xDC, 0xDC},
	{"color--ghostwhite", 0xF8, 0xF8, 0xFF},
	{"color--gold", 0xFF, 0xD7, 0x00},
	{"color--goldenrod", 0xDA, 0xA5, 0x20},
	{"color--gray", 0x80, 0x80, 0x80},
	{"color--green", 0x00, 0x80, 0x00},
	{"color--greenyellow", 0xAD, 0xFF, 0x2F},
	{"color--grey", 0x80, 0x80, 0x80},
	{"color--honeydew", 0xF0, 0xFF, 0xF0},
	{"color--hotpink", 0xFF, 0x69, 0xB4},
	{"color--indianred", 0xCD, 0x5C, 0x5C},
	{"color--indigo", 0x4B, 0x00, 0x82},
	{"color--ivory", 0xFF, 0xFF, 0xF0},
	{"color--khaki", 0xF0, 0xE6, 0x8C},
	{"color--lavender", 0xE6, 0xE6, 0xFA},
	{"color--lavenderblush", 0xFF, 0xF0, 0xF5},
	{"color--lawngreen", 0x7C, 0xFC, 0x00},
	{"color--lemonchiffon", 0xFF, 0xFA, 0xCD},
	{"color--lightblue", 0xAD, 0xD8, 0xE6},
	{"color--lightcoral", 0xF0, 0x80, 0x80},
	{"color--lightcyan", 0xE0, 0xFF, 0xFF},
	{"color--lightgoldenrodyellow", 0xFA, 0xFA, 0xD2},
	{"color--lightgray", 0xD3, 0xD3, 0xD3},
	{"color--lightgreen", 0x90, 0xEE, 0x90},
	{"color--lightgrey", 0xD3, 0xD3, 0xD3},
	{"color--lightpink", 0xFF, 0xB6, 0xC1},
	{"color--lightsalmon", 0xFF, 0xA0, 0x7A},
	{"color--lightseagreen", 0x20, 0xB2, 0xAA},
	{"color--lightskyblue", 0x87, 0xCE, 0xFA},
	{"color--lightslategray", 0x77, 0x88, 0x99},
	{"color--lightslategrey", 0x77, 0x88, 0x99},
	{"color--lightsteelblue", 0xB0, 0xC4, 0xDE},
	{"color--lightyellow", 0xFF, 0xFF, 0xE0},
	{"color--lime", 0x00, 0xFF, 0x00},
	{"color--limegreen", 0x32, 0xCD, 0x32},
	{"color--linen", 0xFA, 0xF0, 0xE6},
	{"color--magenta", 0xFF, 0x00, 0xFF},
	{"color--maroon", 0x80, 0x00, 0x00},
	{"color--mediumaquamarine", 0x66, 0xCD, 0xAA},
	{"color--mediumblue", 0x00, 0x00, 0xCD},
	{"color--mediumorchid", 0xBA, 0x55, 0xD3},
	{"color--mediumpurple", 0x93, 0x70, 0xDB},
	{"color--mediumseagreen", 0x3C, 0xB3, 0x71},
	{"color--mediumslateblue", 0x7B, 0x68, 0xEE},
	{"color--mediumspringgreen", 0x00, 0xFA, 0x9A},
	{"color--mediumturquoise", 0x48, 0xD1, 0xCC},
	{"color--mediumvioletred", 0xC7, 0x15, 0x85},
	{"color--midnightblue", 0x19, 0x19, 0x70},
	{"color--mintcream", 0xF5, 0xFF, 0xFA},
	{"color--mistyrose", 0xFF, 0xE4, 0xE1},
	{"color--moccasin", 0xFF, 0xE4, 0xB5},
	{"color--navajowhite", 0xFF, 0xDE, 0xAD},
	{"color--navy", 0x00, 0x00, 0x80},
	{"color--oldlace", 0xFD, 0xF5, 0xE6},
	{"color--olive", 0x80, 0x80, 0x00},
	{"color--olivedrab", 0x6B, 0x8E, 0x23},
	{"color--orange", 0xFF, 0xA5, 0x00},
	{"color--orangered", 0xFF, 0x45, 0x00},
	{"color--orchid", 0xDA, 0x70, 0xD6},
	{"color--palegoldenrod", 0xEE, 0xE8, 0xAA},
	{"color--palegreen", 0x98, 0xFB, 0x98},
	{"color--paleturquoise", 0xAF, 0xEE, 0xEE},
	{"color--palevioletred", 0xDB, 0x70, 0x93},
	{"color--papayawhip", 0xFF, 0xEF, 0xD5},
	{"color--peachpuff", 0xFF, 0xDA, 0xB9},
	{"color--peru", 0xCD, 0x85, 0x3F},
	{"color--pink", 0xFF, 0xC0, 0xCB},
	{"color--plum", 0xDD, 0xA0, 0xDD},
	{"color--powderblue", 0xB0, 0xE0, 0xE6},
	{"color--purple", 0x80, 0x00, 0x80},
	{"color--rebeccapurple", 0x66, 0x33, 0x99},
	{"color--red", 0xFF, 0x00, 0x00},
	{"color--rosybrown", 0xBC, 0x8F, 0x8F},
	{"color--royalblue", 0x41, 0x69, 0xE1},
	{"color--saddlebrown", 0x8B, 0x45, 0x13},
	{"color--salmon", 0xFA, 0x80, 0x72},
	{"color--sandybrown", 0xF4, 0xA4, 0x60},
	{"color--seagreen", 0x2E, 0x8B, 0x57},
	{"color--seashell", 0xFF, 0xF5, 0xEE},
	{"color--sienna", 0xA0, 0x52, 0x2D},
	{"color--silver", 0xC0, 0xC0, 0xC0},
	{"color--skyblue", 0x87, 0xCE, 0xEB},
	{"color--slateblue", 0x6A, 0x5A, 0xCD},
	{"color--slategray", 0x70, 0x80, 0x90},
	{"color--slategrey", 0x70, 0x80, 0x90},
	{"color--snow", 0xFF, 0xFA, 0xFA},
	{"color--springgreen", 0x00, 0xFF, 0x7F},
	{"color--steelblue", 0x46, 0x82, 0xB4},
	{"color--tan", 0xD2, 0xB4, 0x8C},
	{"color--teal", 0x00, 0x80, 0x80},
	{"color--thistle", 0xD8, 0xBF, 0xD8},
	{"color--tomato", 0xFF, 0x63, 0x47},
	{"color--turquoise", 0x40, 0xE0, 0xD0},
	{"color--violet", 0xEE, 0x82, 0xEE},
	{"color--wheat", 0xF5, 0xDE, 0xB3},
	{"color--white", 0xFF, 0xFF, 0xFF},
	{"color--whitesmoke", 0xF5, 0xF5, 0xF5},
	{"color--yellow", 0xFF, 0xFF, 0x00},
	{"color--yellowgreen", 0x9A, 0xCD, 0x32},
};
// clang-format on

extern const i32 PANORAMA_COLOR_COUNT = KZ_ARRAYSIZE(PANORAMA_COLORS);

// Nearest by squared RGB distance. Alpha is dropped: no generated class carries one.
const char *PanoramaNearestColorClass(const Color &c)
{
	const char *best = PANORAMA_COLORS[0].className;
	i32 bestDistance = INT_MAX;
	for (const auto &entry : PANORAMA_COLORS)
	{
		i32 dr = (i32)c.r() - (i32)entry.r;
		i32 dg = (i32)c.g() - (i32)entry.g;
		i32 db = (i32)c.b() - (i32)entry.b;
		i32 distance = dr * dr + dg * dg + db * db;
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = entry.className;
		}
	}
	return best;
}

const char *PanoramaResolveFontSlug(const char *name, const char *fallback)
{
	if (name && name[0])
	{
		for (i32 i = 0; i < PANORAMA_FONT_COUNT; i++)
		{
			if (KZ_STREQI(name, PANORAMA_FONTS[i].slug))
			{
				return PANORAMA_FONTS[i].slug;
			}
		}
	}
	return fallback;
}

// Static storage, so callers may cache it by pointer.
const char *PanoramaFontClass(const char *name, const char *fallback)
{
	const char *slug = PanoramaResolveFontSlug(name, fallback);
	for (i32 i = 0; i < PANORAMA_FONT_COUNT; i++)
	{
		if (V_strcmp(slug, PANORAMA_FONTS[i].slug) == 0)
		{
			return PANORAMA_FONTS[i].className;
		}
	}
	return PANORAMA_FONTS[0].className;
}

const char *PanoramaFontDisplayName(const char *name, const char *fallback)
{
	const char *slug = PanoramaResolveFontSlug(name, fallback);
	for (i32 i = 0; i < PANORAMA_FONT_COUNT; i++)
	{
		if (V_strcmp(slug, PANORAMA_FONTS[i].slug) == 0)
		{
			return PANORAMA_FONTS[i].displayName;
		}
	}
	return PANORAMA_FONTS[0].displayName;
}

// The sheets do not define every value: positions step by 1 inside +/-100 and by 5 out to +/-500,
// sizes by 1 up to 100 and by 5 above. Snapping keeps a preference from naming a missing class.
// TODO: Nuke this function when dyanmic font sizes are supported
i32 PanoramaSnapToStep(i32 value, i32 lo, i32 hi)
{
	value = clamp(value, lo, hi);
	if (value > 100 || value < -100)
	{
		value = (value / 5) * 5;
	}
	return value;
}
