#pragma once
#include "kz/kz.h"
#include "kz/timer/kz_timer.h"
#include "entityhandle.h"

#define KZ_HUD_TIMER_STOPPED_GRACE_TIME 3.0f
#define KZ_HUD_ON_GROUND_THRESHOLD      0.07f

// The m_iHideHUD bits the client's own HUD elements check, as of CS2 1.41.8.3. Same values as HIDEHUD_* in
// shareddefs.h, which clashes with the SDK definitions the plugin already has.
#define KZ_HIDEHUD_WEAPONSELECTION (1 << 0)
#define KZ_HIDEHUD_ALL             (1 << 2)
#define KZ_HIDEHUD_HEALTH          (1 << 3)
#define KZ_HIDEHUD_MISCSTATUS      (1 << 6)
#define KZ_HIDEHUD_CHAT            (1 << 7)
#define KZ_HIDEHUD_CROSSHAIR       (1 << 8)
#define GAME_HUD_PART_COUNT        6

// One toggle per m_iHideHUD bit on the Game HUD page.
struct GameHudPartDef
{
	const char *phraseKey;
	const char *prefKey; // bool preference
	u32 bit;
};

extern const GameHudPartDef GAME_HUD_PARTS[GAME_HUD_PART_COUNT];
class CCSCustomHudLayout;
class CCheckTransmitInfo;

enum class MHUDElement
{
	Timer,
	Speed,
	Prespeed,
	Keys,
	Checkpoint,
	// The indicators are contiguous and last, so an element index maps straight onto the indicator tables.
	Perf,
	CrouchJump,
	Jumpbug,
	Count
};

#define MHUD_INDICATOR_COUNT 3

static_assert((i32)MHUDElement::Count - (i32)MHUDElement::Perf == MHUD_INDICATOR_COUNT, "indicators must be the last three elements");

inline i32 MHUDIndicatorIndex(MHUDElement element)
{
	return (i32)element - (i32)MHUDElement::Perf;
}

inline bool IsMHUDIndicator(MHUDElement element)
{
	const i32 index = MHUDIndicatorIndex(element);
	return index >= 0 && index < MHUD_INDICATOR_COUNT;
}

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
	const char *opacityKey;
	const char *alignKey;
	const char *borderKey;
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
	// The legacy HTML HUD tints its speed line from these, so they are not layout-only like the rest.
	bool affectLegacy {};
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
static_global const Color MHUD_DEF_KEYS_PRESSED_COLOR(0x3B, 0xED, 0xA0, 0xFF);
static_global const Color MHUD_DEF_KEYS_OVERLAP_GLOW_COLOR(0xFF, 0x40, 0x40, 0xFF);

enum class MHUDSpeedState
{
	Base,
	CrouchJump,
	Perf,
	CrouchPerf,
	Jumpbug,
	Count,
};

enum class MHUDAlign
{
	Left,
	Center,
	Right,
};

// Symbols wrapped around a numeric element's value.
enum class MHUDBorder
{
	None,
	Parens,   // (1234)
	Dash,     // -1234-
	Dashes,   // --1234--
	Angles,   // <1234>
	Brackets, // [1234]
	Braces,   // {1234}
	Under,    // _1234_
	Unders,   // __1234__
	Pipes,    // |1234|
	Count,
};

struct MHUDBorderDef
{
	const char *prefix;
	const char *suffix;
	const char *label; // the symbols around a sample value, so the picker needs no phrase; NULL for None
};

extern const MHUDBorderDef MHUD_BORDERS[(i32)MHUDBorder::Count];

// The rows each indicator contributes to the flattened Indicators page.
enum class MHUDIndicatorRow
{
	Enabled,
	Position,
	Align,
	Size,
	Font,
	Outline,
	Opacity,
	Color,
	Acronym,
	Count,
};

struct MHUDIndicatorDef
{
	MHUDElement element;
	const char *acronymKey;  // bool preference: draw the short form instead of the full one
	const char *fullPhrase;  // drawn in Full mode
	const char *shortPhrase; // drawn in Acronym mode
	// Indicator-qualified: all three share one page, where a bare "Size" would repeat three times.
	const char *rowPhrase[(i32)MHUDIndicatorRow::Count];
};

extern const MHUDIndicatorDef MHUD_INDICATOR_DEFS[MHUD_INDICATOR_COUNT];

// How long prespeed stays visible after landing.
enum class MHUDPrespeedShow
{
	Brief,        // default: shows briefly after any landing
	JumpOrLadder, // shows briefly, but only after a jump or ladder takeoff, not a walk-off
	Always,       // stays visible while grounded, using the last known takeoff speed
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
		i32 opacity {100};
		MHUDAlign align {MHUDAlign::Center};
		MHUDBorder border {MHUDBorder::None};
	};

	Element elements[(i32)MHUDElement::Count] {};

	Color timerPaused, timerStopped, timerTp, timerPro;
	Color speed[(i32)MHUDSpeedState::Count];
	Color prespeed[(i32)MHUDSpeedState::Count];
	Color keys, keysOverlap, keysPressed, keysOverlapGlow;
	Color checkpoint, checkpointTp;
	Color indicator[MHUD_INDICATOR_COUNT];
	bool indicatorAcronym[MHUD_INDICATOR_COUNT] {};

	bool legacyStyle {};
	bool compactPanel {};
	bool timerDetailed {true};
	bool timerShowState {true};
	bool speedPrecise {};
	bool prespeedPrecise {};
	MHUDPrespeedShow prespeedShow {MHUDPrespeedShow::Brief};
	bool keysOverlapEnabled {true};
	bool keysOverlapAxis {}; // tint only the two keys causing the overlap, not the whole element
	bool keysLetters {};
	bool keysSquare {};
	bool keysBorder {true};
	bool keysGlowEnabled {true};
	bool keysFillEnabled {true};
	MHUDKeysIdle keysIdle {MHUDKeysIdle::Show};
	bool mimicSpec {};    // read from the viewer's own set only, never from the player being mimicked
	u32 hiddenGameHud {}; // m_iHideHUD bits, also only ever read from the viewer's own set
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

	// The player's own preferences or the spectated player's, depending on the mimicSpec setting.
	// If the player is not spectating, this is always their own.
	const MHUDPrefs &GetPrefs();

	const MHUDPrefs &GetOwnPrefs();

	// Any preference change refills the whole set rather than working out which key it was.
	void InvalidatePrefs()
	{
		this->prefsDirty = true;
	}

	static void RegisterMenu();

	static bool IsLayoutHudAvailable();
	// Caches whether the layout asset is mounted here. Called on load and on every map change,
	// since that is when mounts settle.
	static void RefreshLayoutAvailability();

	bool IsUsingLayoutStyle();
	void ToggleStyle();

	// Draw the panel from a player to a specific target.
	static void DrawPanels(KZPlayer *player, KZPlayer *target);

	// Only the bits this service added are ever cleared, since the server sets some of them itself.
	void UpdateGameHud();
	void RestoreGameHud();

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
		bool walkedOff {};     // left the ground without jumping and not off a ladder
		bool recentTakeoff {}; // airborne or inside the landing grace, even when showTakeoff is held on

		MHUDSpeedState GetState() const
		{
			if (this->jumpbug)
			{
				return MHUDSpeedState::Jumpbug;
			}
			if (this->perf)
			{
				return this->crouchJump ? MHUDSpeedState::CrouchPerf : MHUDSpeedState::Perf;
			}
			return this->crouchJump ? MHUDSpeedState::CrouchJump : MHUDSpeedState::Base;
		}
	};

	SpeedInfo GetSpeedInfo(const MHUDPrefs &prefs);

	// Player pawn while alive, observer pawn otherwise.
	CPlayer_MovementServices *GetHudMoveServices();

	// Shared by both HUDs
	std::string GetTimerText(const char *language = KZ_DEFAULT_LANGUAGE, bool showState = true);
	std::string GetCheckpointText(const char *language = KZ_DEFAULT_LANGUAGE);

	static void DrawLegacyPanels(KZPlayer *player, KZPlayer *target);
	// Legacy panels only.
	std::string GetSpeedText(const MHUDPrefs &prefs, const char *language = KZ_DEFAULT_LANGUAGE);
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
		i32 opacity {INT_MIN};
		const char *alignClass {};
		// Cached so the nearest-palette search only runs when the color changes, not every tick.
		const char *colorClassComputed {};
		u32 lastColorPacked {};
		bool colorComputed {};
	};

	struct LayoutKeysState
	{
		bool pressed[6] {};
		i32 glow[6] {-1, -1, -1, -1, -1, -1};
		const char *overlapClass[6] {}; // per-key overlap tint, used by the axis-only mode
		i32 idle {-1};
		i32 letters {-1};
		i32 square {-1};
		i32 noBorder {-1};
		i32 noGlow {-1};
		i32 noFill {-1};
		i32 outline {-1};
		i32 fontSize {INT_MIN};
		i32 boxSize {INT_MIN};
		const char *fontClass {};
	};

	CHandle<CBaseEntity> ownedLayout {};
	LayoutElementState layoutElements[(i32)MHUDElement::Count] {};
	LayoutKeysState layoutKeys {};
	f64 nextTimerLayoutUpdate {};
	CHandle<CBaseEntity> timerLayoutSource {};

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
	void UpdateIndicatorElements(CCSCustomHudLayout *layout, const SpeedInfo &info, bool force);

public:
	static CCSCustomHudLayout *GetLayoutEntity(const char *layoutPath, CHandle<CBaseEntity> &cache);

	void DestroyOwnedLayout();

	void OnClientDisconnect()
	{
		this->DestroyOwnedLayout();
	}

	static void Cleanup();

	// Masks every player's owned entity away from every client but its owner.
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

private:
	MHUDPrefs prefs {};
	bool prefsDirty {true};
	void RefreshPrefs();

	CHandle<CBasePlayerPawn> gameHudPawn {};
	u32 gameHudBits {};
};
