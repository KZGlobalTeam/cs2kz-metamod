#ifndef _INCLUDE_ICS2KZ_H_
#define _INCLUDE_ICS2KZ_H_

#include <cstdint>

// Public API for CS2KZ.
// Main (game) thread only. Calling from a worker thread is undefined behaviour.
// Strings: every const char * returned by this interface aliases internal storage. Copy it,
// don't cache it. Never returns null; "" stands for "unavailable".
//
// Players are identified by slot (0 .. 63, i.e. entity index - 1). A slot that is not
// occupied by an in-game player makes every getter return false / "" / 0.
#define CS2KZ_INTERFACE "ICS2KZ001"

// Player button bits, matching the game's own input bitmask.
// The implementation static_asserts these against the SDK values, so a game update that
// moves a bit breaks the cs2kz build rather than silently lying to consumers.
enum class KZButton : uint64_t
{
	Attack = 0x1,
	Jump = 0x2,
	Duck = 0x4,
	Forward = 0x8,
	Back = 0x10,
	Use = 0x20,
	TurnLeft = 0x80,
	TurnRight = 0x100,
	MoveLeft = 0x200,
	MoveRight = 0x400,
	Attack2 = 0x800,
	Reload = 0x2000,
	Speed = 0x10000, // Shift (walk)
	Score = 0x200000000,
	Zoom = 0x400000000,
	LookAtWeapon = 0x800000000,
};

// Where a ban came from. A player is only marked banned once per session, so this is
// whichever source got there first.
enum class KZBanSource : int
{
	Detection = 0,  // the anticheat flagged them during this session
	GlobalDatabase, // global auth reported an existing ban
	LocalDatabase,  // this server's own ban table reported an existing ban
};

// Description of a map course. Handed out with the timer events and inside KZTimerStatus.
// `name` aliases internal storage and stays valid until the map changes; copy it.
struct KZCourseInfo
{
	const char *name; // "" when the player isn't on a course
	int32_t splitCount;
	int32_t checkpointCount;
	int32_t stageCount;
};

// Where a player is and how they are moving this tick, filled by GetMovementState.
// This is the movement snapshot only - the timer, the mode and the profile live in
// KZTimerStatus and the profile getters.
struct KZMovementState
{
	// ---- Position and orientation ----
	float origin[3];
	float eyeOrigin[3];
	float velocity[3];
	float eyeAngles[3]; // pitch, yaw, roll

	// ---- Input ----
	uint64_t buttons; // KZButton bitmask of the buttons currently held

	// ---- Movement mode ----
	bool alive;
	bool onGround;
	bool onLadder;
	bool noclipping;

	// ---- Ducking ----
	bool ducking;     // mid duck transition
	bool ducked;      // fully ducked
	float duckAmount; // 0.0 standing .. 1.0 fully ducked

	// ---- Last takeoff ----
	// These describe the player's MOST RECENT TAKEOFF, not the current tick. They latch
	// when the player leaves the ground and stay until the next takeoff. The three flags
	// are the same values the in-game speed HUD tints itself with.
	bool perf;          // perfect bhop: no speed loss on landing. Ladder takeoffs don't count.
	bool crouchJump;    // the player was ducking or ducked at takeoff
	bool jumpbug;       // the duckbug and the perf landed on the same frame. Implies perf.
	float takeoffSpeed; // horizontal speed at that takeoff
};

// Snapshot of a player's timer state, filled by GetTimerStatus.
struct KZTimerStatus
{
	bool running;
	bool paused;
	bool valid;

	double time; // seconds since the run started, 0 when not running

	uint32_t teleportsUsed;

	bool onCourse; // false when the player isn't on a course, `course` is then empty
	KZCourseInfo course;

	// Alias internal storage, copy them. "" when unavailable.
	const char *modeShortName; // e.g. "CKZ"
	const char *modeName;      // e.g. "Classic"
};

// Timer event notifications. Derive, override what you need, and register the instance
// with ICS2KZ::RegisterEventListener.
//
// These are notification-only: there is no way to veto a timer start, pause or resume from
// outside the plugin. All callbacks run on the game thread, after cs2kz has finished
// applying the event internally.
class ICS2KZEventListener
{
public:
	virtual ~ICS2KZEventListener() = default;

	// The player started a run on `course`.
	virtual void OnTimerStartPost(int slot, const KZCourseInfo &course) {}

	// The player finished a run. `time` is the final run time in seconds.
	virtual void OnTimerEndPost(int slot, const KZCourseInfo &course, float time, uint32_t teleportsUsed) {}

	// The run was stopped without finishing (left the course, died, /stop, ...).
	virtual void OnTimerStoppedPost(int slot, const KZCourseInfo &course) {}

	// The run is still going but can no longer be submitted (saveloc, noclip, ...).
	virtual void OnTimerInvalidatedPost(int slot) {}

	virtual void OnPausePost(int slot) {}

	virtual void OnResumePost(int slot) {}

	virtual void OnSplitZoneTouchPost(int slot, uint32_t splitZone) {}

	virtual void OnCheckpointZoneTouchPost(int slot, uint32_t checkpointZone) {}

	virtual void OnStageZoneTouchPost(int slot, uint32_t stageZone) {}

	// The player was marked as a cheater. Fires once per session, on the first source that
	// marks them - a live detection, or an existing ban found at connect time.
	// `reason` is "" when the source didn't provide one (the local ban table never does).
	//
	// The player may be kicked immediately after this returns, depending on kz_ac_autokick,
	// so read whatever you need from their slot inside the callback, not later.
	virtual void OnPlayerBannedPost(int slot, KZBanSource source, const char *reason) {}
};

class ICS2KZ
{
public:
	// ============================== Events ==============================

	// Start receiving timer events. Returns false if this listener is already registered.
	// Unregister it in your Unload(), see the lifetime note at the top of this header.
	// Note: You must call UnregisterEventListener() before unloading your plugin!
	virtual bool RegisterEventListener(ICS2KZEventListener *listener) = 0;

	// Stop receiving timer events. Returns false if the listener was not registered.
	// Safe to call from inside a callback.
	virtual bool UnregisterEventListener(ICS2KZEventListener *listener) = 0;

	// ============================== Players =============================

	// Returns true if `slot` holds a connected, in-game player.
	virtual bool IsValidPlayer(int slot) = 0;

	// Returns the player's SteamID64, or 0 if the slot is invalid or the player is not authenticated.
	virtual uint64_t GetSteamID64(int slot) = 0;

	// Returns the slot of the player with this SteamID64, or -1 if they're not on the server.
	virtual int SlotFromSteamID64(uint64_t steamID64) = 0;

	// Returns the player's current in-game name, trimmed of surrounding whitespace. "" for an
	// invalid slot. Aliases internal storage that the next call overwrites, so copy it.
	virtual const char *GetPlayerName(int slot) = 0;

	// Returns true if cs2kz has marked this player as a cheater, whether by a live detection or by
	// an existing ban found at connect time. False for an invalid slot.
	// A banned player is not necessarily kicked, see kz_ac_autokick.
	virtual bool IsPlayerBanned(int slot) = 0;

	// Returns the slot of the player this player is spectating, or -1 if they are not
	// spectating anyone - which includes being alive, and spectating their own corpse.
	virtual int GetSpectatedSlot(int slot) = 0;

	// ========================= Profile / clan tag =======================
	//
	// The scoreboard clan tag and the chat prefix are independent

	// Returns the player's competitive rating for the mode they are currently playing
	// or -1 when CS2KZ does not have one.
	virtual double GetRating(int slot) = 0;

	// The tag currently shown on the scoreboard, brackets included (e.g. "[CKZ Pro]").
	// This is the override when one is set, otherwise what CS2KZ computed.
	virtual const char *GetClanTag(int slot) = 0;

	// Replace the scoreboard clan tag. nullptr or "" clears the override and restores the
	// CS2KZ-computed tag. Applies immediately, survives mode switches and style changes, and
	// applies even on a server running kz_profile_clantag_enabled 0.
	virtual void SetClanTagOverride(int slot, const char *tag) = 0;

	// Returns the override string, or "" if the tag is CS2KZ-computed.
	virtual const char *GetClanTagOverride(int slot) = 0;

	// Returns the prefix printed before the player's name in chat, exactly as CS2KZ holds it:
	// with the color tokens ("{lime}", "{default}", ...) still in place. An override is
	// returned verbatim.
	virtual const char *GetChatPrefix(int slot) = 0;

	// Replace the chat prefix. nullptr or "" clears the override.
	// Color tokens are allowed and render in chat, but the same string is also used where
	// CS2KZ prints uncolored (server console, log file), so they show up literally there.
	// Pass plain text if that matters.
	virtual void SetChatPrefixOverride(int slot, const char *prefix) = 0;

	// The override string, or "" if the prefix is CS2KZ-computed.
	virtual const char *GetChatPrefixOverride(int slot) = 0;

	// ============================== Movement ============================

	// Fill `out` with where the player is and how they are moving. Returns false (and `out`
	// untouched) for an invalid slot or a player not in game.
	virtual bool GetMovementState(int slot, KZMovementState *out) = 0;

	// True if the player holds `button`. With `onlyDown`, a button that was pressed and
	// released within the same tick does not count - which KZMovementState::buttons, being
	// a plain held-buttons mask, cannot express.
	virtual bool IsButtonPressed(int slot, KZButton button, bool onlyDown) = 0;

	// =============================== Timer ==============================

	// Fill `out` with the player's timer state. Returns false (and `out` untouched) for an invalid
	// slot or a player not in game. A player with no run has running == false and
	// onCourse == false, which is still a successful call.
	virtual bool GetTimerStatus(int slot, KZTimerStatus *out) = 0;
};

#endif // _INCLUDE_ICS2KZ_H_
