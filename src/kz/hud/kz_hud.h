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

// One tintable state of an element: the label the options menu shows, the preference behind it,
// and the color it falls back to.
struct MHUDColorPrefDef
{
	const char *phraseKey;
	const char *prefKey;
	// TODO: Use `Color` instead?
	u8 r, g, b;
};

// The color states of one element, in menu order.
const MHUDColorPrefDef *MHUDElementColorPrefs(MHUDElement element, i32 &count);

// Put one element's preferences back to their defaults.
void MHUDResetElementPrefs(KZPlayer *player, MHUDElement element);

extern const MHUDElementDef MHUD_ELEMENTS[(i32)MHUDElement::Count];

#define MHUD_SIZE_MIN 8
#define MHUD_SIZE_MAX 100

// Default element colors, shared by both the layout HUD (as pal-fg classes) and the legacy HTML HUD
// (as hex), so the two never disagree on what an unset preference looks like.
static_global const Color MHUD_DEF_BASE_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_PERF_COLOR(0x40, 0xFF, 0x40, 0xFF);
static_global const Color MHUD_DEF_JUMPBUG_COLOR(0xFF, 0xFF, 0x20, 0xFF);
static_global const Color MHUD_DEF_CJ_COLOR(0x71, 0xEE, 0xB8, 0xFF);
static_global const Color MHUD_DEF_TIMER_TP_COLOR(255, 255, 255, 255);
static_global const Color MHUD_DEF_TIMER_PRO_COLOR(0x5F, 0x99, 0xD9, 0xFF);
static_global const Color MHUD_DEF_TIMER_PAUSED_COLOR(0xFF, 0xFF, 0x00, 0xFF);
static_global const Color MHUD_DEF_TIMER_STOPPED_COLOR(0xFF, 0xA0, 0xA0, 0xFF);
static_global const Color MHUD_DEF_KEYS_OVERLAP_COLOR(0xFF, 0x40, 0x40, 0xFF);

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

	// Returns true when the layout based HUD can be used at all.
	// Either the MultiAddonManager is present and the MHUD addon is mounted,
	// or we are on `-tools -addon mhud`, or kz_force_mhud is set to 1.
	static bool IsLayoutHudAvailable();

	// The layout style, once the addon is there and the player has not asked for the legacy HUD.
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

	// Drives this player's MHUD layout, taking its values from source (the same player, or the
	// spectated one). When the panel is off or the legacy style is selected, every element collapses
	// rather than blanking, so the values survive a toggle.
	bool UpdateHudLayout(KZPlayer *source);

	// Per-element enable flags.
	bool IsMHUDElementEnabled(MHUDElement element);

	// While the options menu has a stepper page open for an element, that element is drawn even if
	// its enable flag is off: the player would otherwise be adjusting something invisible.
	void SetMHUDForcedElement(MHUDElement element, bool forced)
	{
		this->forcedElement = forced ? (i32)element : -1;
	}

	bool IsMHUDTimerDetailed();
	bool IsMHUDKeysOverlapEnabled();
	bool IsMHUDKeysHidingUnpressed();
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
		// Cache the resolved color class so the nearest-palette search (O(palette)) only runs when the
		// color actually changes, not every tick.
		const char *colorClassComputed {};
		u32 lastColorPacked {};
		bool colorComputed {};
	};

	// TODO (?): Update the container so it is always big enough for the text.
	struct LayoutKeysState
	{
		bool pressed[6] {};
		bool hideIdle {};
		i32 fontSize {INT_MIN};
		const char *fontClass {};
	};

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

	// Destroy this player's owned entity (disconnect / unload). Safe to call when there is none.
	void DestroyOwnedLayout();

	// Tear down the owned entity when the player leaves, matching the other services' hook. Reset()
	// keeps the entity (it survives respawns/map changes and is recreated lazily); only a disconnect
	// destroys it.
	void OnClientDisconnect()
	{
		this->DestroyOwnedLayout();
	}

	// Destroy every player's owned entity. Called on plugin unload.
	static void Cleanup();

	// Masks every player's owned mhud entity away from every client but its owner. Called from
	// Hook_CheckTransmit.
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);

private:
	i32 forcedElement {-1};
};
