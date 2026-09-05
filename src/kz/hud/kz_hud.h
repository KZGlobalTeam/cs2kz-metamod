#pragma once
#include "kz/kz.h"
#include "kz/timer/kz_timer.h"
#include "entityhandle.h"

#define KZ_HUD_TIMER_STOPPED_GRACE_TIME 3.0f
#define KZ_HUD_ON_GROUND_THRESHOLD      0.07f
class CCSCustomHudLayout;
class CCheckTransmitInfo;

enum class MHUDElement
{
	Timer,
	Speed,
	Prespeed,
	Keys,
	Checkpoint,
	Count
};

struct MHUDElementDef
{
	const char *panelId; // See the panel ids in mhud.xml
	const char *varName;
	const char *enabledKey;
	const char *xKey;
	const char *yKey;
	const char *sizeKey;
	const char *fontKey;
	const char *outlineKey;
	i32 xDefault;
	i32 yDefault;
	i32 sizeDefault;
};

struct MHUDColorPrefDef
{
	const char *phraseKey;
	const char *prefKey;
	// TODO: Use `Color` instead?
	u8 r, g, b;
	bool solidOnly {};
	const char *enabledBy {};
};

extern const MHUDElementDef MHUD_ELEMENTS[(i32)MHUDElement::Count];

#define MHUD_SIZE_MIN 8
#define MHUD_SIZE_MAX 100

// Shared by the layout HUD (as pal-fg classes) and the legacy HTML HUD (as hex).
static_global const Color MHUD_DEF_BASE_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_PERF_COLOR(0x40, 0xFF, 0x40, 0xFF);
static_global const Color MHUD_DEF_JUMPBUG_COLOR(0xFF, 0xFF, 0x20, 0xFF);
static_global const Color MHUD_DEF_CJ_COLOR(0x71, 0xEE, 0xB8, 0xFF);
static_global const Color MHUD_DEF_TIMER_TP_COLOR(0xFF, 0xFF, 0x00, 0xFF);
static_global const Color MHUD_DEF_TIMER_PRO_COLOR(0x5F, 0x99, 0xD9, 0xFF);
static_global const Color MHUD_DEF_TIMER_PAUSED_COLOR(0xFF, 0x80, 0x00, 0xFF);
static_global const Color MHUD_DEF_TIMER_STOPPED_COLOR(0xFF, 0xA0, 0xA0, 0xFF);
static_global const Color MHUD_DEF_KEYS_OVERLAP_COLOR(0xFF, 0x40, 0x40, 0xFF);
static_global const Color MHUD_DEF_KEYS_GLOW_COLOR(0x3B, 0xED, 0xA0, 0xFF);

// The player's own cl_crosshair* values. The game's defaults stand until a query answers.
struct MHUDCrosshairSettings
{
	f32 size {5.0f};
	f32 thickness {0.5f};
	f32 gap {-2.0f};
	f32 outlineThickness {1.0f};
	i32 color {1};
	i32 r {50}, g {250}, b {50};
	i32 alpha {200};
	bool useAlpha {true};
	bool drawOutline {true};
	bool dot {false};
	bool tStyle {false};
};

// What an unpressed key looks like.
enum class MHUDKeysIdle
{
	Show,
	Hide,
	Underscore,
};

struct MHUDPrefs
{
	struct Element
	{
		bool enabled {true};
		f32 x {}, y {}, size {};
		const char *fontClass {};
		bool outline {true};
	};

	Element elements[(i32)MHUDElement::Count] {};

	Color timerPaused, timerStopped, timerTp, timerPro;
	Color speed, speedCj;
	Color prespeed, prespeedPerf, prespeedJumpbug;
	Color keys, keysOverlap, keysGlow;
	Color checkpoint;

	bool legacyStyle {};
	bool compactPanel {};
	bool crosshair {};
	bool timerDetailed {true};
	bool keysOverlapEnabled {true};
	bool keysLetters {};
	bool keysSquare {};
	bool keysBorder {true};
	bool keysGlowEnabled {true};
	MHUDKeysIdle keysIdle {MHUDKeysIdle::Show};
};

class KZHUDService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool jumpedThisTick {};
	bool fromDuckbug {};
	bool crouchJumping {};
	bool showPanel {};
	f64 timerStoppedTime {};
	f64 currentTimeWhenTimerStopped {};

public:
	virtual void Reset() override;
	static void Init();

	static const MHUDColorPrefDef *GetMHUDElementColorPrefs(MHUDElement element, i32 &count);

	// Static storage, so callers can keep caching the result by pointer.
	static const char *GetMHUDFontClass(KZPlayer *player, MHUDElement element);

	// The cached preference set, refilled on first use after a preference changed.
	const MHUDPrefs &GetPrefs();

	// Any preference change refills the whole set rather than working out which key it was.
	void InvalidatePrefs()
	{
		this->prefsDirty = true;
	}

	static void RegisterMenu();

	// Either the MultiAddonManager is present and the MHUD addon is mounted,
	// or we are on `-tools -addon mhud`, or kz_force_mhud is set to 1.
	static bool IsLayoutHudAvailable();

	bool IsUsingLayoutStyle();
	void ToggleStyle();

	// Draw the panel from a player to a specific target.
	static void DrawPanels(KZPlayer *player, KZPlayer *target);

	void ResetShowPanel();
	void TogglePanel();
	void ToggleCompactPanel();

	void OnPhysicsSimulate()
	{
		jumpedThisTick = false;
	}

	void OnProcessMovementPost();

	void OnJump(bool modern = false)
	{
		jumpedThisTick = modern ? this->player->IsButtonPressed(IN_JUMP) : true;
	}

	void OnStopTouchGround()
	{
		if (jumpedThisTick)
		{
			fromDuckbug = player->duckBugged;
			crouchJumping = player->GetPlayerPawn()->m_fFlags() & FL_DUCKING || player->GetMoveServices()->m_bDucking();
		}
		else
		{
			fromDuckbug = false;
			crouchJumping = false;
		}
	}

	bool IsShowingPanel()
	{
		return this->showPanel;
	}

	// These describe the player's most recent takeoff, not the current tick.
	bool IsCrouchJumping() const
	{
		return this->crouchJumping;
	}

	bool IsFromDuckbug() const
	{
		return this->fromDuckbug;
	}

	// True if the jump button registered a press this tick, even if it was already
	// released again by the time something polls buttons later in the same tick (e.g. a
	// scroll-wheel jump bind). Reset every tick in OnPhysicsSimulate, latched by OnJump.
	bool JumpedThisTick() const
	{
		return this->jumpedThisTick;
	}

	bool IsCompactPanel();

	void OnTimerStopped(f64 currentTimeWhenTimerStopped);

	bool ShouldShowTimerAfterStop()
	{
		return g_pKZUtils->GetServerGlobals()->curtime > KZ_HUD_TIMER_STOPPED_GRACE_TIME
			   && g_pKZUtils->GetServerGlobals()->curtime - timerStoppedTime < KZ_HUD_TIMER_STOPPED_GRACE_TIME;
	}

	// source is the same player, or the spectated one.
	bool UpdateHudLayout(KZPlayer *source);

	bool IsMHUDElementEnabled(MHUDElement element);

	bool IsMHUDTimerDetailed();
	bool IsMHUDOutlineEnabled(MHUDElement element);

private:
	struct SpeedInfo
	{
		f32 speed {};
		f32 takeoffSpeed {};
		bool showTakeoff {};
		bool perf {};
		bool jumpbug {};
		bool crouchJump {};
	};

	// Shared by the HTML panel and the MHUD layout so the two never disagree about a takeoff.
	SpeedInfo GetSpeedInfo();

	// Player pawn while alive, observer pawn otherwise.
	CPlayer_MovementServices *GetHudMoveServices();

	// Shared by both HUDs
	std::string GetTimerText(const char *language = KZ_DEFAULT_LANGUAGE);
	std::string GetCheckpointText(const char *language = KZ_DEFAULT_LANGUAGE);

	static void DrawLegacyPanels(KZPlayer *player, KZPlayer *target);
	// Legacy panels only.
	std::string GetSpeedText(const char *language = KZ_DEFAULT_LANGUAGE);
	std::string GetKeyText(const char *language = KZ_DEFAULT_LANGUAGE);

	struct LayoutElementState
	{
		std::string text {};
		const char *colorClass {};
		const char *fontClass {};
		i32 fontSize {-1};
		i32 x {INT_MIN};
		i32 y {INT_MIN};
		bool hidden {true};
		bool outline {false};
		// Cached so the nearest-palette search only runs when the color changes, not every tick.
		const char *colorClassComputed {};
		u32 lastColorPacked {};
		bool colorComputed {};
	};

	struct LayoutKeysState
	{
		bool pressed[6] {};
		i32 idle {-1};
		i32 letters {-1};
		i32 square {-1};
		i32 noBorder {-1};
		i32 noGlow {-1};
		i32 glow {-1};
		i32 fontSize {INT_MIN};
		const char *fontClass {};
	};

	// Numeric suffix of each class family last applied, -1 for nothing yet.
	struct LayoutCrosshairState
	{
		i32 shown {-1};
		i32 armLength {-1};
		i32 thickness {-1};
		i32 margin {-1};
		i32 outline {-1};
		i32 outlineLength {-1};
		i32 outlineThickness {-1};
		i32 outlineMargin {-1};
		i32 outlineDot {-1};
		i32 opacity {-1};
		i32 dot {-1};
		i32 noTopArm {-1};
		const char *colorClass {};
	};

	MHUDCrosshairSettings crosshair {};
	LayoutCrosshairState layoutCrosshair {};

	// Not an MHUDElement: the crosshair has no text, so no font/size/color machinery applies.
	void ApplyCrosshair(CCSCustomHudLayout *layout, bool show, bool force);

	CHandle<CBaseEntity> ownedLayout {};
	LayoutElementState layoutElements[(i32)MHUDElement::Count] {};
	LayoutKeysState layoutKeys {};

	CCSCustomHudLayout *EnsureOwnedLayout(bool &created);

	void UpdateLayoutElement(CCSCustomHudLayout *layout, MHUDElement element, bool show, const char *text, const Color &color, bool force);
	void SetLayoutClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *className);
	void SetLayoutValueClass(CCSCustomHudLayout *layout, const char *panelId, i32 &cache, i32 value, const char *prefix, bool percent);

	// One per element, all called from UpdateHudLayout.
	void UpdateTimerElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force);
	void UpdateSpeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force);
	void UpdatePrespeedElement(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force);
	void UpdateKeysElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force);
	void UpdateCheckpointElement(CCSCustomHudLayout *layout, KZPlayer *source, bool force);

public:
	static CCSCustomHudLayout *GetLayoutEntity(const char *layoutPath, CHandle<CBaseEntity> &cache);

	void DestroyOwnedLayout();

	void OnClientDisconnect()
	{
		this->DestroyOwnedLayout();
	}

	// Also called when the crosshair is switched on, so it updates before the next poll.
	void QueryCrosshairCvars();

	void OnCrosshairCvarValue(const char *name, const char *value);

	// Query once now, then keep re-querying so settings changed mid-session are picked up.
	void StartCrosshairPolling();

	static void Cleanup();

	// Masks every player's owned entity away from every client but its owner.
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

private:
	MHUDPrefs prefs {};
	bool prefsDirty {true};
	void RefreshPrefs();
};
