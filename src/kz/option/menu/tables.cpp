#include "kz/option/menu/tables.h"

#include "tier0/memdbgon.h"

// One curated palette swatch: the class that tints text (pal-fg-N), the class that fills a swatch
// panel (pal-bg-N), and the color both draw. Private to this file - callers use the functions below.
struct PanoramaColorDef
{
	const char *fgClass;
	const char *bgClass;
	u8 r, g, b;
};

// A gradient text/swatch pair. The color stops live only in palette.css; C++ needs the class names.
struct PanoramaGradientDef
{
	const char *fgClass; // color: gradient(...) applied to HUD/menu text
	const char *bgClass; // background-color: gradient(...) for the picker swatch
};

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
	// System fonts: Panorama falls back to whatever the player has installed, so these may not render.
	// The * carries through to the picker, where a footer explains it.
	{"trebuchet", "font-family--trebuchet", "Trebuchet MS*", "Trebuchet MS*", "Regular"},
	{"trebuchet-bold", "font-family--trebuchet-bold", "Trebuchet MS Bold*", "Trebuchet MS*", "Bold"},
	{"trebuchet-italic", "font-family--trebuchet-italic", "Trebuchet MS Italic*", "Trebuchet MS*", "Italic"},
	{"lato-light", "font-family--lato-light", "Lato Light*", "Lato*", "Light"},
	{"lato", "font-family--lato", "Lato*", "Lato*", "Regular"},
	{"lato-bold", "font-family--lato-bold", "Lato Bold*", "Lato*", "Bold"},
	{"lato-black", "font-family--lato-black", "Lato Black*", "Lato*", "Black"},
	{"arial", "font-family--arial", "Arial", "Arial", "Regular"},
	{"sans-serif", "font-family--sans-serif", "sans-serif", "sans-serif", "Regular"},
	{"serif", "font-family--serif", "serif", "serif", "Regular"},
	{"monospace", "font-family--monospace", "monospace", "monospace", "Regular"},
};
// clang-format on

extern const i32 PANORAMA_FONT_COUNT = KZ_ARRAYSIZE(PANORAMA_FONTS);

// clang-format off
static_global const PanoramaColorDef PANORAMA_COLORS[] =
{
	{"pal-fg-0", "pal-bg-0", 0x00, 0x00, 0x00}, // #000000
	{"pal-fg-1", "pal-bg-1", 0x5E, 0x5E, 0x5E}, // #5e5e5e
	{"pal-fg-2", "pal-bg-2", 0x82, 0x82, 0x82}, // #828282
	{"pal-fg-3", "pal-bg-3", 0x9C, 0x9C, 0x9C}, // #9c9c9c
	{"pal-fg-4", "pal-bg-4", 0xB2, 0xB2, 0xB2}, // #b2b2b2
	{"pal-fg-5", "pal-bg-5", 0xC5, 0xC5, 0xC5}, // #c5c5c5
	{"pal-fg-6", "pal-bg-6", 0xD5, 0xD5, 0xD5}, // #d5d5d5
	{"pal-fg-7", "pal-bg-7", 0xE4, 0xE4, 0xE4}, // #e4e4e4
	{"pal-fg-8", "pal-bg-8", 0xF2, 0xF2, 0xF2}, // #f2f2f2
	{"pal-fg-9", "pal-bg-9", 0xFF, 0xFF, 0xFF}, // #ffffff
	{"pal-fg-10", "pal-bg-10", 0xFF, 0x00, 0x00}, // #ff0000
	{"pal-fg-11", "pal-bg-11", 0xFF, 0x80, 0x00}, // #ff8000
	{"pal-fg-12", "pal-bg-12", 0xFF, 0xFF, 0x00}, // #ffff00
	{"pal-fg-13", "pal-bg-13", 0x80, 0xFF, 0x00}, // #80ff00
	{"pal-fg-14", "pal-bg-14", 0x00, 0xFF, 0x55}, // #00ff55
	{"pal-fg-15", "pal-bg-15", 0x00, 0xFF, 0xEA}, // #00ffea
	{"pal-fg-16", "pal-bg-16", 0x00, 0xAA, 0xFF}, // #00aaff
	{"pal-fg-17", "pal-bg-17", 0x00, 0x00, 0xFF}, // #0000ff
	{"pal-fg-18", "pal-bg-18", 0xAA, 0x00, 0xFF}, // #aa00ff
	{"pal-fg-19", "pal-bg-19", 0xFF, 0x00, 0xAA}, // #ff00aa
	{"pal-fg-20", "pal-bg-20", 0x8C, 0x00, 0x00}, // #8c0000
	{"pal-fg-21", "pal-bg-21", 0x8C, 0x46, 0x00}, // #8c4600
	{"pal-fg-22", "pal-bg-22", 0x8C, 0x8C, 0x00}, // #8c8c00
	{"pal-fg-23", "pal-bg-23", 0x46, 0x8C, 0x00}, // #468c00
	{"pal-fg-24", "pal-bg-24", 0x00, 0x8C, 0x2F}, // #008c2f
	{"pal-fg-25", "pal-bg-25", 0x00, 0x8C, 0x81}, // #008c81
	{"pal-fg-26", "pal-bg-26", 0x00, 0x5E, 0x8C}, // #005e8c
	{"pal-fg-27", "pal-bg-27", 0x00, 0x00, 0x8C}, // #00008c
	{"pal-fg-28", "pal-bg-28", 0x5E, 0x00, 0x8C}, // #5e008c
	{"pal-fg-29", "pal-bg-29", 0x8C, 0x00, 0x5E}, // #8c005e
	{"pal-fg-30", "pal-bg-30", 0xFF, 0xA6, 0xA6}, // #ffa6a6
	{"pal-fg-31", "pal-bg-31", 0xFF, 0xD2, 0xA6}, // #ffd2a6
	{"pal-fg-32", "pal-bg-32", 0xFF, 0xFF, 0xA6}, // #ffffa6
	{"pal-fg-33", "pal-bg-33", 0xD2, 0xFF, 0xA6}, // #d2ffa6
	{"pal-fg-34", "pal-bg-34", 0xA6, 0xFF, 0xC4}, // #a6ffc4
	{"pal-fg-35", "pal-bg-35", 0xA6, 0xFF, 0xF8}, // #a6fff8
	{"pal-fg-36", "pal-bg-36", 0xA6, 0xE1, 0xFF}, // #a6e1ff
	{"pal-fg-37", "pal-bg-37", 0xA6, 0xA6, 0xFF}, // #a6a6ff
	{"pal-fg-38", "pal-bg-38", 0xE1, 0xA6, 0xFF}, // #e1a6ff
	{"pal-fg-39", "pal-bg-39", 0xFF, 0xA6, 0xE1}, // #ffa6e1
	{"pal-fg-40", "pal-bg-40", 0xFE, 0xE4, 0xE2}, // #fee4e2
	{"pal-fg-41", "pal-bg-41", 0xFD, 0xE6, 0xD7}, // #fde6d7
	{"pal-fg-42", "pal-bg-42", 0xF5, 0xEB, 0xCE}, // #f5ebce
	{"pal-fg-43", "pal-bg-43", 0xE5, 0xF0, 0xD4}, // #e5f0d4
	{"pal-fg-44", "pal-bg-44", 0xD9, 0xF3, 0xDD}, // #d9f3dd
	{"pal-fg-45", "pal-bg-45", 0xD0, 0xF5, 0xEB}, // #d0f5eb
	{"pal-fg-46", "pal-bg-46", 0xCE, 0xF3, 0xFC}, // #cef3fc
	{"pal-fg-47", "pal-bg-47", 0xE0, 0xEC, 0xFE}, // #e0ecfe
	{"pal-fg-48", "pal-bg-48", 0xEE, 0xE7, 0xFE}, // #eee7fe
	{"pal-fg-49", "pal-bg-49", 0xFD, 0xE2, 0xF6}, // #fde2f6
	{"pal-fg-50", "pal-bg-50", 0xFF, 0xC8, 0xC3}, // #ffc8c3
	{"pal-fg-51", "pal-bg-51", 0xFD, 0xCC, 0xAB}, // #fdccab
	{"pal-fg-52", "pal-bg-52", 0xE9, 0xD7, 0x9F}, // #e9d79f
	{"pal-fg-53", "pal-bg-53", 0xCC, 0xE1, 0xAB}, // #cce1ab
	{"pal-fg-54", "pal-bg-54", 0xB5, 0xE6, 0xBE}, // #b5e6be
	{"pal-fg-55", "pal-bg-55", 0xA1, 0xE8, 0xD7}, // #a1e8d7
	{"pal-fg-56", "pal-bg-56", 0x9D, 0xE5, 0xF7}, // #9de5f7
	{"pal-fg-57", "pal-bg-57", 0xC0, 0xD9, 0xFF}, // #c0d9ff
	{"pal-fg-58", "pal-bg-58", 0xDE, 0xCE, 0xFE}, // #decefe
	{"pal-fg-59", "pal-bg-59", 0xF8, 0xC5, 0xEB}, // #f8c5eb
	{"pal-fg-60", "pal-bg-60", 0xFE, 0xA6, 0x9E}, // #fea69e
	{"pal-fg-61", "pal-bg-61", 0xF8, 0xAE, 0x7B}, // #f8ae7b
	{"pal-fg-62", "pal-bg-62", 0xDA, 0xBF, 0x69}, // #dabf69
	{"pal-fg-63", "pal-bg-63", 0xB1, 0xCE, 0x7D}, // #b1ce7d
	{"pal-fg-64", "pal-bg-64", 0x8C, 0xD5, 0x9B}, // #8cd59b
	{"pal-fg-65", "pal-bg-65", 0x66, 0xD8, 0xC1}, // #66d8c1
	{"pal-fg-66", "pal-bg-66", 0x5F, 0xD3, 0xED}, // #5fd3ed
	{"pal-fg-67", "pal-bg-67", 0x9B, 0xC3, 0xFE}, // #9bc3fe
	{"pal-fg-68", "pal-bg-68", 0xCC, 0xB1, 0xFE}, // #ccb1fe
	{"pal-fg-69", "pal-bg-69", 0xED, 0xA6, 0xDD}, // #eda6dd
	{"pal-fg-70", "pal-bg-70", 0xF5, 0x80, 0x79}, // #f58079
	{"pal-fg-71", "pal-bg-71", 0xEC, 0x8C, 0x44}, // #ec8c44
	{"pal-fg-72", "pal-bg-72", 0xC8, 0xA4, 0x16}, // #c8a416
	{"pal-fg-73", "pal-bg-73", 0x93, 0xB7, 0x47}, // #93b747
	{"pal-fg-74", "pal-bg-74", 0x5B, 0xC1, 0x75}, // #5bc175
	{"pal-fg-75", "pal-bg-75", 0x0C, 0xC2, 0xA8}, // #0cc2a8
	{"pal-fg-76", "pal-bg-76", 0x1B, 0xBB, 0xD8}, // #1bbbd8
	{"pal-fg-77", "pal-bg-77", 0x70, 0xA9, 0xFD}, // #70a9fd
	{"pal-fg-78", "pal-bg-78", 0xB6, 0x91, 0xF5}, // #b691f5
	{"pal-fg-79", "pal-bg-79", 0xDF, 0x82, 0xCC}, // #df82cc
	{"pal-fg-80", "pal-bg-80", 0xC5, 0x4D, 0x49}, // #c54d49
	{"pal-fg-81", "pal-bg-81", 0xB8, 0x5F, 0x02}, // #b85f02
	{"pal-fg-82", "pal-bg-82", 0x93, 0x77, 0x0E}, // #93770e
	{"pal-fg-83", "pal-bg-83", 0x67, 0x88, 0x09}, // #678809
	{"pal-fg-84", "pal-bg-84", 0x11, 0x92, 0x45}, // #119245
	{"pal-fg-85", "pal-bg-85", 0x13, 0x8E, 0x7A}, // #138e7a
	{"pal-fg-86", "pal-bg-86", 0x12, 0x89, 0x9F}, // #12899f
	{"pal-fg-87", "pal-bg-87", 0x39, 0x79, 0xD4}, // #3979d4
	{"pal-fg-88", "pal-bg-88", 0x89, 0x61, 0xC7}, // #8961c7
	{"pal-fg-89", "pal-bg-89", 0xB0, 0x51, 0x9E}, // #b0519e
	{"pal-fg-90", "pal-bg-90", 0x9F, 0x32, 0x31}, // #9f3231
	{"pal-fg-91", "pal-bg-91", 0x8E, 0x48, 0x04}, // #8e4804
	{"pal-fg-92", "pal-bg-92", 0x71, 0x5B, 0x03}, // #715b03
	{"pal-fg-93", "pal-bg-93", 0x4F, 0x68, 0x08}, // #4f6808
	{"pal-fg-94", "pal-bg-94", 0x08, 0x71, 0x33}, // #087133
	{"pal-fg-95", "pal-bg-95", 0x0D, 0x6D, 0x5E}, // #0d6d5e
	{"pal-fg-96", "pal-bg-96", 0x0E, 0x69, 0x7A}, // #0e697a
	{"pal-fg-97", "pal-bg-97", 0x21, 0x5B, 0xAD}, // #215bad
	{"pal-fg-98", "pal-bg-98", 0x6B, 0x45, 0xA2}, // #6b45a2
	{"pal-fg-99", "pal-bg-99", 0x8C, 0x37, 0x7D}, // #8c377d
	{"pal-fg-100", "pal-bg-100", 0x72, 0x24, 0x23}, // #722423
	{"pal-fg-101", "pal-bg-101", 0x67, 0x32, 0x00}, // #673200
	{"pal-fg-102", "pal-bg-102", 0x51, 0x41, 0x06}, // #514106
	{"pal-fg-103", "pal-bg-103", 0x37, 0x4A, 0x02}, // #374a02
	{"pal-fg-104", "pal-bg-104", 0x04, 0x51, 0x22}, // #045122
	{"pal-fg-105", "pal-bg-105", 0x00, 0x4E, 0x42}, // #004e42
	{"pal-fg-106", "pal-bg-106", 0x03, 0x4B, 0x58}, // #034b58
	{"pal-fg-107", "pal-bg-107", 0x18, 0x41, 0x7C}, // #18417c
	{"pal-fg-108", "pal-bg-108", 0x4C, 0x31, 0x73}, // #4c3173
	{"pal-fg-109", "pal-bg-109", 0x64, 0x27, 0x59}, // #642759
	{"pal-fg-110", "pal-bg-110", 0x4C, 0x17, 0x16}, // #4c1716
	{"pal-fg-111", "pal-bg-111", 0x46, 0x1F, 0x00}, // #461f00
	{"pal-fg-112", "pal-bg-112", 0x35, 0x2A, 0x03}, // #352a03
	{"pal-fg-113", "pal-bg-113", 0x23, 0x31, 0x01}, // #233101
	{"pal-fg-114", "pal-bg-114", 0x00, 0x36, 0x14}, // #003614
	{"pal-fg-115", "pal-bg-115", 0x02, 0x34, 0x2B}, // #02342b
	{"pal-fg-116", "pal-bg-116", 0x05, 0x31, 0x3A}, // #05313a
	{"pal-fg-117", "pal-bg-117", 0x0F, 0x2A, 0x52}, // #0f2a52
	{"pal-fg-118", "pal-bg-118", 0x32, 0x20, 0x4D}, // #32204d
	{"pal-fg-119", "pal-bg-119", 0x42, 0x19, 0x3B}, // #42193b
	{"pal-fg-120", "pal-bg-120", 0x2A, 0x6B, 0x8F}, // #2a6b8f
	{"pal-fg-121", "pal-bg-121", 0x32, 0x81, 0xAC}, // #3281ac
	{"pal-fg-122", "pal-bg-122", 0x1B, 0x60, 0x85}, // #1b6085
	{"pal-fg-123", "pal-bg-123", 0x32, 0x63, 0x7E}, // #32637e
	{"pal-fg-124", "pal-bg-124", 0x4A, 0xA8, 0xFF}, // #4aa8ff
	{"pal-fg-125", "pal-bg-125", 0x5D, 0xC6, 0xFF}, // #5dc6ff
	{"pal-fg-126", "pal-bg-126", 0xB5, 0xD4, 0xEE}, // #b5d4ee
	{"pal-fg-127", "pal-bg-127", 0xB1, 0xC8, 0xEC}, // #b1c8ec
	{"pal-fg-128", "pal-bg-128", 0x44, 0x44, 0x77}, // #444477
	{"pal-fg-129", "pal-bg-129", 0x1F, 0x12, 0x31}, // #1f1231
	{"pal-fg-130", "pal-bg-130", 0xFF, 0xD7, 0x00}, // #ffd700
	{"pal-fg-131", "pal-bg-131", 0xEA, 0xD1, 0x8A}, // #ead18a
	{"pal-fg-132", "pal-bg-132", 0xEA, 0xBE, 0x54}, // #eabe54
	{"pal-fg-133", "pal-bg-133", 0xEA, 0xB5, 0x41}, // #eab541
	{"pal-fg-134", "pal-bg-134", 0xD8, 0x76, 0x0E}, // #d8760e
	{"pal-fg-135", "pal-bg-135", 0xFF, 0x99, 0x00}, // #ff9900
	{"pal-fg-136", "pal-bg-136", 0xB1, 0xAF, 0x2D}, // #b1af2d
	{"pal-fg-137", "pal-bg-137", 0xB1, 0x99, 0x2E}, // #b1992e
	{"pal-fg-138", "pal-bg-138", 0x5E, 0x68, 0x69}, // #5e6869
	{"pal-fg-139", "pal-bg-139", 0x44, 0x4F, 0x52}, // #444f52
	{"pal-fg-140", "pal-bg-140", 0xFF, 0x00, 0x00}, // #ff0000
	{"pal-fg-141", "pal-bg-141", 0xDD, 0x00, 0x00}, // #dd0000
	{"pal-fg-142", "pal-bg-142", 0xE1, 0x00, 0x00}, // #e10000
	{"pal-fg-143", "pal-bg-143", 0xEB, 0x4B, 0x4B}, // #eb4b4b
	{"pal-fg-144", "pal-bg-144", 0x00, 0xFF, 0x00}, // #00ff00
	{"pal-fg-145", "pal-bg-145", 0x4C, 0xAF, 0x50}, // #4caf50
	{"pal-fg-146", "pal-bg-146", 0x5F, 0xFF, 0x6C}, // #5fff6c
	{"pal-fg-147", "pal-bg-147", 0x25, 0xD9, 0x6D}, // #25d96d
	{"pal-fg-148", "pal-bg-148", 0x2D, 0xDD, 0x33}, // #2ddd33
	{"pal-fg-149", "pal-bg-149", 0x22, 0xA5, 0x27}, // #22a527
	{"pal-fg-150", "pal-bg-150", 0x40, 0xFF, 0x40}, // #40ff40
	{"pal-fg-151", "pal-bg-151", 0x71, 0xEE, 0xB8}, // #71eeb8
	{"pal-fg-152", "pal-bg-152", 0xFF, 0xFF, 0x20}, // #ffff20
	{"pal-fg-153", "pal-bg-153", 0x5F, 0x99, 0xD9}, // #5f99d9
	{"pal-fg-154", "pal-bg-154", 0xFF, 0xA0, 0xA0}, // #ffa0a0
	{"pal-fg-155", "pal-bg-155", 0xFF, 0x40, 0x40}, // #ff4040
	{"pal-fg-156", "pal-bg-156", 0x24, 0xF0, 0x97}, // #24f097
	{"pal-fg-157", "pal-bg-157", 0xFF, 0xFF, 0x00}, // #ffff00
	{"pal-fg-158", "pal-bg-158", 0x3D, 0x44, 0x48}, // #3d4448
	{"pal-fg-159", "pal-bg-159", 0x3E, 0x3E, 0x3E}, // #3e3e3e
};
// clang-format on

static_global const i32 PANORAMA_COLOR_COUNT = KZ_ARRAYSIZE(PANORAMA_COLORS);

// Nearest by squared RGB distance. Alpha is dropped: no palette class carries one.
static_function i32 GetNearestColorIndex(const Color &c)
{
	i32 best = 0;
	i32 bestDistance = INT_MAX;
	for (i32 i = 0; i < PANORAMA_COLOR_COUNT; i++)
	{
		i32 dr = (i32)c.r() - (i32)PANORAMA_COLORS[i].r;
		i32 dg = (i32)c.g() - (i32)PANORAMA_COLORS[i].g;
		i32 db = (i32)c.b() - (i32)PANORAMA_COLORS[i].b;
		i32 distance = dr * dr + dg * dg + db * db;
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}

static_function const char *ResolveNearestColorClass(const Color &c)
{
	return PANORAMA_COLORS[GetNearestColorIndex(c)].fgClass;
}

static_function Color GetPaletteColor(i32 index)
{
	index = Clamp(index, 0, PANORAMA_COLOR_COUNT - 1);
	return Color(PANORAMA_COLORS[index].r, PANORAMA_COLORS[index].g, PANORAMA_COLORS[index].b, 255);
}

// clang-format off
// Gradient stops live in palette.css (.grad-N / .gbg-N). Keep this count in step with that file.
static_global const PanoramaGradientDef PANORAMA_GRADIENTS[] =
{
	{"grad-0", "gbg-0"},
	{"grad-1", "gbg-1"},
	{"grad-2", "gbg-2"},
	{"grad-3", "gbg-3"},
	{"grad-4", "gbg-4"},
	{"grad-5", "gbg-5"},
	{"grad-6", "gbg-6"},
	{"grad-7", "gbg-7"},
	{"grad-8", "gbg-8"},
	{"grad-9", "gbg-9"},
	{"grad-10", "gbg-10"},
	{"grad-11", "gbg-11"},
	{"grad-12", "gbg-12"},
	{"grad-13", "gbg-13"},
	{"grad-14", "gbg-14"},
	{"grad-15", "gbg-15"},
	{"grad-16", "gbg-16"},
	{"grad-17", "gbg-17"},
	{"grad-18", "gbg-18"},
	{"grad-19", "gbg-19"},
	{"grad-20", "gbg-20"},
	{"grad-21", "gbg-21"},
	{"grad-22", "gbg-22"},
	{"grad-23", "gbg-23"},
	{"grad-24", "gbg-24"},
	{"grad-25", "gbg-25"},
	{"grad-26", "gbg-26"},
	{"grad-27", "gbg-27"},
	{"grad-28", "gbg-28"},
	{"grad-29", "gbg-29"},
	{"grad-30", "gbg-30"},
	{"grad-31", "gbg-31"},
	{"grad-32", "gbg-32"},
	{"grad-33", "gbg-33"},
	{"grad-34", "gbg-34"},
	{"grad-35", "gbg-35"},
	{"grad-36", "gbg-36"},
	{"grad-37", "gbg-37"},
	{"grad-38", "gbg-38"},
	{"grad-39", "gbg-39"},
};
// clang-format on

static_global const i32 PANORAMA_GRADIENT_COUNT = KZ_ARRAYSIZE(PANORAMA_GRADIENTS);

const char *panorama::ResolveColorClass(const Color &c)
{
	if (IsGradient(c))
	{
		return PANORAMA_GRADIENTS[Clamp(GetGradientIndex(c), 0, PANORAMA_GRADIENT_COUNT - 1)].fgClass;
	}
	return ResolveNearestColorClass(c);
}

const char *panorama::ResolveSwatchClass(const Color &c)
{
	if (IsGradient(c))
	{
		return PANORAMA_GRADIENTS[Clamp(GetGradientIndex(c), 0, PANORAMA_GRADIENT_COUNT - 1)].bgClass;
	}
	return PANORAMA_COLORS[GetNearestColorIndex(c)].bgClass;
}

i32 panorama::GetColorEntryCount()
{
	return PANORAMA_COLOR_COUNT + PANORAMA_GRADIENT_COUNT;
}

const char *panorama::GetColorEntryBgClass(i32 entry)
{
	if (entry < PANORAMA_COLOR_COUNT)
	{
		return PANORAMA_COLORS[Clamp(entry, 0, PANORAMA_COLOR_COUNT - 1)].bgClass;
	}
	return PANORAMA_GRADIENTS[Clamp(entry - PANORAMA_COLOR_COUNT, 0, PANORAMA_GRADIENT_COUNT - 1)].bgClass;
}

Color panorama::GetColorEntryValue(i32 entry)
{
	if (entry < PANORAMA_COLOR_COUNT)
	{
		return GetPaletteColor(entry);
	}
	return MakeGradient(entry - PANORAMA_COLOR_COUNT);
}

i32 panorama::FindColorEntry(const Color &c)
{
	if (IsGradient(c))
	{
		const i32 g = GetGradientIndex(c);
		return (g >= 0 && g < PANORAMA_GRADIENT_COUNT) ? PANORAMA_COLOR_COUNT + g : -1;
	}
	return GetNearestColorIndex(c);
}

const char *panorama::ResolveFontSlug(const char *name, const char *fallback)
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
const char *panorama::ResolveFontClass(const char *name, const char *fallback)
{
	const char *slug = ResolveFontSlug(name, fallback);
	for (i32 i = 0; i < PANORAMA_FONT_COUNT; i++)
	{
		if (V_strcmp(slug, PANORAMA_FONTS[i].slug) == 0)
		{
			return PANORAMA_FONTS[i].className;
		}
	}
	return PANORAMA_FONTS[0].className;
}

const char *panorama::GetFontDisplayName(const char *name, const char *fallback)
{
	const char *slug = ResolveFontSlug(name, fallback);
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
i32 panorama::SnapToStep(i32 value, i32 lo, i32 hi)
{
	value = Clamp(value, lo, hi);
	if (value > 100 || value < -100)
	{
		value = (value / 5) * 5;
	}
	return value;
}
