#pragma once

#include "../kz.h"

class KZPaintService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	enum PaintColor : u8
	{
		PAINT_COLOR_RED = 0,
		PAINT_COLOR_WHITE = 1,
		PAINT_COLOR_BLACK = 2,
		PAINT_COLOR_BLUE = 3,
		PAINT_COLOR_BROWN = 4,
		PAINT_COLOR_GREEN = 5,
		PAINT_COLOR_YELLOW = 6,
		PAINT_COLOR_PURPLE = 7,
		PAINT_COLOR_CUSTOM = 8,
		PAINT_COLOR_COUNT = 9
	};

	static constexpr u32 DECAL_GROUP_NAME = 3420975428u; // MurmurHash2LowerCase("paint", 0x31415926)

	virtual void Reset() override;

	// Set paint color from predefined colors or custom RGB
	// Returns true if color was set successfully
	bool SetColor(const char *colorName);
	bool SetColorRGB(u8 r, u8 g, u8 b, u8 a = 255);

	static constexpr f32 DEFAULT_PAINT_SIZE = 8.0f;

	// Set paint size
	bool SetSize(f32 value);

	// Get color/size info from preferences
	Color GetColor() const;
	f32 GetSize() const;
	const char *GetColorName() const;

	void PlacePaint();

	bool pendingPaint = false;
};
