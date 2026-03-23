#include "kz_paint.h"
#include "../language/kz_language.h"
#include "../option/kz_option.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"
#include "sdk/tracefilter.h"
#include "sdk/cglobalsymbol.h"
#include "sdk/recipientfilters.h"

void KZPaintService::Reset()
{
	// Reset to default: red color and default size
	player->optionService->SetPreferenceInt("paintColor", 0xFF0000FF); // Red (RGBA format)
	player->optionService->SetPreferenceFloat("paintSize", DEFAULT_PAINT_SIZE);
	player->optionService->SetPreferenceBool("showAllPaint", false);

	this->autoPaintEnabled = false;
	this->hasLastAutoPaintPosition = false;
	this->nextAutoPaintTime = 0.0;
}

Color KZPaintService::GetColor() const
{
	u32 colorData = player->optionService->GetPreferenceInt("paintColor", 0xFF0000FF); // Default to red
	u8 r = (colorData >> 24) & 0xFF;
	u8 g = (colorData >> 16) & 0xFF;
	u8 b = (colorData >> 8) & 0xFF;
	u8 a = colorData & 0xFF;
	return Color(r, g, b, a);
}

f32 KZPaintService::GetSize() const
{
	return (f32)player->optionService->GetPreferenceFloat("paintSize", DEFAULT_PAINT_SIZE);
}

bool KZPaintService::SetColor(const char *colorName)
{
	if (!colorName)
	{
		return false;
	}

	Color color;
	if (KZ_STREQI(colorName, "red"))
	{
		color = Color(255, 0, 0, 255);
	}
	else if (KZ_STREQI(colorName, "white"))
	{
		color = Color(255, 255, 255, 255);
	}
	else if (KZ_STREQI(colorName, "black"))
	{
		color = Color(0, 0, 0, 255);
	}
	else if (KZ_STREQI(colorName, "blue"))
	{
		color = Color(0, 0, 255, 255);
	}
	else if (KZ_STREQI(colorName, "brown"))
	{
		color = Color(165, 42, 42, 255);
	}
	else if (KZ_STREQI(colorName, "green"))
	{
		color = Color(0, 128, 0, 255);
	}
	else if (KZ_STREQI(colorName, "yellow"))
	{
		color = Color(255, 255, 0, 255);
	}
	else if (KZ_STREQI(colorName, "purple"))
	{
		color = Color(128, 0, 128, 255);
	}
	else
	{
		return false;
	}

	u32 colorData = ((u32)color.r() << 24) | ((u32)color.g() << 16) | ((u32)color.b() << 8) | (u32)color.a();
	player->optionService->SetPreferenceInt("paintColor", colorData);
	return true;
}

bool KZPaintService::SetColorRGB(u8 r, u8 g, u8 b, u8 a)
{
	u32 colorData = ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
	player->optionService->SetPreferenceInt("paintColor", colorData);
	return true;
}

bool KZPaintService::SetSize(f32 value)
{
	if (value <= 0.0f)
	{
		return false;
	}

	player->optionService->SetPreferenceFloat("paintSize", value);
	return true;
}

const char *KZPaintService::GetColorName() const
{
	Color color = GetColor();

	if (color.r() == 255 && color.g() == 0 && color.b() == 0)
	{
		return "Red";
	}
	if (color.r() == 255 && color.g() == 255 && color.b() == 255)
	{
		return "White";
	}
	if (color.r() == 0 && color.g() == 0 && color.b() == 0)
	{
		return "Black";
	}
	if (color.r() == 0 && color.g() == 0 && color.b() == 255)
	{
		return "Blue";
	}
	if (color.r() == 165 && color.g() == 42 && color.b() == 42)
	{
		return "Brown";
	}
	if (color.r() == 0 && color.g() == 128 && color.b() == 0)
	{
		return "Green";
	}
	if (color.r() == 255 && color.g() == 255 && color.b() == 0)
	{
		return "Yellow";
	}
	if (color.r() == 128 && color.g() == 0 && color.b() == 128)
	{
		return "Purple";
	}

	return "Custom";
}

bool KZPaintService::ShouldShowAllPaint() const
{
	return this->player->optionService->GetPreferenceBool("showAllPaint", false);
}

bool KZPaintService::IsAutoPaintEnabled() const
{
	return this->autoPaintEnabled;
}

void KZPaintService::OnGameFrame()
{
	for (i32 i = 1; i <= MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || !player->paintService)
		{
			continue;
		}

		player->paintService->TryAutoPaint();
	}
}

void KZPaintService::ToggleAutoPaint()
{
	this->autoPaintEnabled = !this->autoPaintEnabled;
	if (this->autoPaintEnabled)
	{
		this->hasLastAutoPaintPosition = false;
		this->nextAutoPaintTime = 0.0;
	}
}

void KZPaintService::ToggleShowAllPaint()
{
	bool newValue = !this->player->optionService->GetPreferenceBool("showAllPaint", false);
	this->player->optionService->SetPreferenceBool("showAllPaint", newValue);
}

// Commands

#define KZ_PAINT_TRACE_DISTANCE 32768.0f

bool KZPaintService::TracePaint(trace_t &tr) const
{
	CCSPlayerPawnBase *pawn = static_cast<CCSPlayerPawnBase *>(this->player->GetCurrentPawn());
	if (!pawn)
	{
		return false;
	}

	Vector origin;
	this->player->GetEyeOrigin(&origin);
	QAngle angles = pawn->v_angle();

	Vector forward;
	AngleVectors(angles, &forward);
	Vector endPos = origin + forward * KZ_PAINT_TRACE_DISTANCE;

	bbox_t bounds({vec3_origin, vec3_origin});
	CTraceFilterPlayerMovementCS filter(pawn);
	filter.EnableInteractsExcludeLayer(LAYER_INDEX_CONTENTS_PLAYER_CLIP);
	filter.EnableInteractsExcludeLayer(LAYER_INDEX_CONTENTS_PLAYER);
	g_pKZUtils->TracePlayerBBox(origin, endPos, bounds, &filter, tr);

	return tr.DidHit();
}

void KZPaintService::PlacePaint()
{
	trace_t tr;
	if (!this->TracePaint(tr))
	{
		return;
	}

	CGlobalSymbol paintDecal = MakeGlobalSymbol("paint");
	this->pendingPaint = true;
	g_pKZUtils->DecalTrace(&tr, &paintDecal, 0.0f);
	this->pendingPaint = false;
}

void KZPaintService::TryAutoPaint()
{
	if (!this->autoPaintEnabled)
	{
		return;
	}

	f64 curTime = g_pKZUtils->GetGlobals()->curtime;
	if (curTime < this->nextAutoPaintTime)
	{
		return;
	}
	this->nextAutoPaintTime = curTime + 0.05;

	trace_t tr;
	if (!this->TracePaint(tr))
	{
		return;
	}

	if (this->hasLastAutoPaintPosition && (tr.m_vEndPos - this->lastAutoPaintPosition).LengthSqr() < 1.0f)
	{
		return;
	}

	CGlobalSymbol paintDecal = MakeGlobalSymbol("paint");
	this->pendingPaint = true;
	g_pKZUtils->DecalTrace(&tr, &paintDecal, 0.0f);
	this->pendingPaint = false;

	this->lastAutoPaintPosition = tr.m_vEndPos;
	this->hasLastAutoPaintPosition = true;
}

SCMD(kz_paint, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->paintService->PlacePaint();
	return MRES_SUPERCEDE;
}

SCMD(kz_paintcolor, SCFL_PREFERENCE | SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	if (args->ArgC() < 2)
	{
		player->languageService->PrintChat(true, false, "Paint Color Command Usage");
		player->languageService->PrintChat(true, false, "Current Paint Color", player->paintService->GetColorName(),
										   player->paintService->GetColor().r(), player->paintService->GetColor().g(),
										   player->paintService->GetColor().b());
		return MRES_SUPERCEDE;
	}

	// Check for predefined colors
	if (player->paintService->SetColor(args->Arg(1)))
	{
		player->languageService->PrintChat(true, false, "Paint Color Set", player->paintService->GetColorName());
		return MRES_SUPERCEDE;
	}

	// Try to parse as RGB values
	if (args->ArgC() >= 4 && utils::IsNumeric(args->Arg(1)) && utils::IsNumeric(args->Arg(2)) && utils::IsNumeric(args->Arg(3)))
	{
		u8 r = (u8)Clamp(atoi(args->Arg(1)), 0, 255);
		u8 g = (u8)Clamp(atoi(args->Arg(2)), 0, 255);
		u8 b = (u8)Clamp(atoi(args->Arg(3)), 0, 255);
		u8 a = 255;

		if (args->ArgC() >= 5 && utils::IsNumeric(args->Arg(4)))
		{
			a = (u8)Clamp(atoi(args->Arg(4)), 0, 255);
		}

		player->paintService->SetColorRGB(r, g, b, a);
		player->languageService->PrintChat(true, false, "Paint Color RGB Set", r, g, b, a);
		return MRES_SUPERCEDE;
	}

	player->languageService->PrintChat(true, false, "Paint Color Command Usage");
	return MRES_SUPERCEDE;
}

SCMD(kz_paintsize, SCFL_PREFERENCE | SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	if (args->ArgC() < 2 || !utils::IsNumeric(args->Arg(1)))
	{
		player->languageService->PrintChat(true, false, "Paint Size Command Usage");
		player->languageService->PrintChat(true, false, "Current Paint Size", player->paintService->GetSize());
		return MRES_SUPERCEDE;
	}

	f32 value = (f32)atof(args->Arg(1));
	if (player->paintService->SetSize(value))
	{
		player->languageService->PrintChat(true, false, "Paint Size Set", player->paintService->GetSize());
		return MRES_SUPERCEDE;
	}

	player->languageService->PrintChat(true, false, "Paint Size Command Usage");
	return MRES_SUPERCEDE;
}

SCMD(kz_togglepaint, SCFL_MISC | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->paintService->ToggleAutoPaint();

	player->languageService->PrintChat(true, false, player->paintService->IsAutoPaintEnabled() ? "Paint Toggle Enabled" : "Paint Toggle Disabled");
	return MRES_SUPERCEDE;
}

SCMD(kz_showpaint, SCFL_MISC | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->paintService->ToggleShowAllPaint();

	player->languageService->PrintChat(true, false, player->paintService->ShouldShowAllPaint() ? "Show Paint Enabled" : "Show Paint Disabled");
	return MRES_SUPERCEDE;
}

SCMD(kz_cleardecals, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	CSingleRecipientFilter filter(player->GetPlayerSlot());
	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("round_start");
	if (!event)
	{
		return MRES_SUPERCEDE;
	}
	event->SetInt("timelimit", 0);
	event->SetInt("fraglimit", 0);
	event->SetString("objective", "");
	IGameEventListener2 *listener = g_pKZUtils->GetLegacyGameEventListener(player->GetPlayerSlot());
	listener->FireGameEvent(event);
	interfaces::pGameEventManager->FreeEvent(event);
	return MRES_SUPERCEDE;
}
