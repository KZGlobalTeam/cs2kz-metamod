#include "kz_pubapi.h"

#include "kz/kz.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/hud/kz_hud.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/mode/kz_mode.h"
#include "kz/profile/kz_profile.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/spec/kz_spec.h"
#include "kz/style/kz_style.h"
#include "kz/timer/kz_timer.h"

#include <algorithm>
#include <vector>

// The public button enum must keep matching the game's input bitmask. If a game update
// moves a bit, this breaks the build instead of silently handing consumers wrong data.
static_assert((uint64)KZButton::Attack == IN_ATTACK);
static_assert((uint64)KZButton::Jump == IN_JUMP);
static_assert((uint64)KZButton::Duck == IN_DUCK);
static_assert((uint64)KZButton::Forward == IN_FORWARD);
static_assert((uint64)KZButton::Back == IN_BACK);
static_assert((uint64)KZButton::Use == IN_USE);
static_assert((uint64)KZButton::TurnLeft == IN_TURNLEFT);
static_assert((uint64)KZButton::TurnRight == IN_TURNRIGHT);
static_assert((uint64)KZButton::MoveLeft == IN_MOVELEFT);
static_assert((uint64)KZButton::MoveRight == IN_MOVERIGHT);
static_assert((uint64)KZButton::Attack2 == IN_ATTACK2);
static_assert((uint64)KZButton::Reload == IN_RELOAD);
static_assert((uint64)KZButton::Speed == IN_SPEED);
static_assert((uint64)KZButton::Score == IN_SCORE);
static_assert((uint64)KZButton::Zoom == IN_ZOOM);
static_assert((uint64)KZButton::LookAtWeapon == IN_LOOK_AT_WEAPON);

// Same deal for the ban sources: the public enum is a copy, keep the two in step.
static_assert((int)KZBanSource::Detection == (int)KZAnticheatBanSource::Detection);
static_assert((int)KZBanSource::GlobalDatabase == (int)KZAnticheatBanSource::GlobalDatabase);
static_assert((int)KZBanSource::LocalDatabase == (int)KZAnticheatBanSource::LocalDatabase);

static_global const KZButton allButtons[] = {KZButton::Attack,   KZButton::Jump,      KZButton::Duck,     KZButton::Forward,
											 KZButton::Back,     KZButton::Use,       KZButton::TurnLeft, KZButton::TurnRight,
											 KZButton::MoveLeft, KZButton::MoveRight, KZButton::Attack2,  KZButton::Reload,
											 KZButton::Speed,    KZButton::Score,     KZButton::Zoom,     KZButton::LookAtWeapon};

static_global std::vector<ICS2KZEventListener *> externalListeners;

// GetChatPrefix hands out a pointer, but KZProfileService::GetPrefix builds a std::string.
// Main thread only, so one shared buffer is enough - documented as "copy it, don't cache".
static_global char prefixBuffer[256];

static_function KZPlayer *ResolveSlot(int slot)
{
	if (slot < 0 || slot >= MAXPLAYERS)
	{
		return nullptr;
	}
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(slot));
	if (!player || !player->IsInGame())
	{
		return nullptr;
	}
	return player;
}

static_function KZProfileService *ResolveProfile(int slot)
{
	KZPlayer *player = ResolveSlot(slot);
	return player ? player->profileService : nullptr;
}

// A listener may unregister itself from inside its own callback, so dispatch over a copy.
template<typename Fn>
static_function void ForEachListener(Fn &&fn)
{
	if (externalListeners.empty())
	{
		return;
	}
	std::vector<ICS2KZEventListener *> snapshot = externalListeners;
	for (ICS2KZEventListener *listener : snapshot)
	{
		fn(listener);
	}
}

static_function i32 SlotOf(KZPlayer *player)
{
	return player ? player->GetPlayerSlot().Get() : -1;
}

// Course GUIDs are an internal identity, consumers get the description instead.
// The descriptor is looked up against the current map, so it goes away on a map change.
static_function KZCourseInfo DescribeCourse(const KZCourseDescriptor *course)
{
	KZCourseInfo info {};
	info.name = "";
	if (course)
	{
		info.name = course->name;
		info.splitCount = course->splitCount;
		info.checkpointCount = course->checkpointCount;
		info.stageCount = course->stageCount;
	}
	return info;
}

static_function KZCourseInfo DescribeCourse(u32 courseGUID)
{
	return DescribeCourse(KZ::course::GetCourse(courseGUID));
}

// Translates the internal, KZPlayer-based timer events into the slot-based public ones.
static_global class : public KZTimerServiceEventListener
{
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		KZCourseInfo course = DescribeCourse(courseGUID);
		ForEachListener([slot, &course](ICS2KZEventListener *listener) { listener->OnTimerStartPost(slot, course); });
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		KZCourseInfo course = DescribeCourse(courseGUID);
		ForEachListener([slot, &course, time, teleportsUsed](ICS2KZEventListener *listener)
						{ listener->OnTimerEndPost(slot, course, time, teleportsUsed); });
	}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		KZCourseInfo course = DescribeCourse(courseGUID);
		ForEachListener([slot, &course](ICS2KZEventListener *listener) { listener->OnTimerStoppedPost(slot, course); });
	}

	virtual void OnTimerInvalidated(KZPlayer *player) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot](ICS2KZEventListener *listener) { listener->OnTimerInvalidatedPost(slot); });
	}

	virtual void OnPausePost(KZPlayer *player) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot](ICS2KZEventListener *listener) { listener->OnPausePost(slot); });
	}

	virtual void OnResumePost(KZPlayer *player) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot](ICS2KZEventListener *listener) { listener->OnResumePost(slot); });
	}

	virtual void OnSplitZoneTouchPost(KZPlayer *player, u32 splitZone) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot, splitZone](ICS2KZEventListener *listener) { listener->OnSplitZoneTouchPost(slot, splitZone); });
	}

	virtual void OnCheckpointZoneTouchPost(KZPlayer *player, u32 checkpointZone) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot, checkpointZone](ICS2KZEventListener *listener) { listener->OnCheckpointZoneTouchPost(slot, checkpointZone); });
	}

	virtual void OnStageZoneTouchPost(KZPlayer *player, u32 stageZone) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		ForEachListener([slot, stageZone](ICS2KZEventListener *listener) { listener->OnStageZoneTouchPost(slot, stageZone); });
	}
} timerEventForwarder;

static_global class : public KZAnticheatServiceEventListener
{
	virtual void OnPlayerBannedPost(KZPlayer *player, KZAnticheatBanSource source, const char *reason) override
	{
		i32 slot = SlotOf(player);
		if (slot < 0)
		{
			return;
		}
		KZBanSource publicSource = (KZBanSource)source;
		ForEachListener([slot, publicSource, reason](ICS2KZEventListener *listener) { listener->OnPlayerBannedPost(slot, publicSource, reason); });
	}
} anticheatEventForwarder;

class CS2KZAPI : public ICS2KZ
{
public:
	virtual bool RegisterEventListener(ICS2KZEventListener *listener) override
	{
		if (!listener || std::find(externalListeners.begin(), externalListeners.end(), listener) != externalListeners.end())
		{
			return false;
		}
		externalListeners.push_back(listener);
		return true;
	}

	virtual bool UnregisterEventListener(ICS2KZEventListener *listener) override
	{
		auto it = std::find(externalListeners.begin(), externalListeners.end(), listener);
		if (it == externalListeners.end())
		{
			return false;
		}
		externalListeners.erase(it);
		return true;
	}

	virtual bool IsValidPlayer(int slot) override
	{
		return ResolveSlot(slot) != nullptr;
	}

	virtual uint64_t GetSteamID64(int slot) override
	{
		KZPlayer *player = ResolveSlot(slot);
		return player ? player->GetSteamId64() : 0;
	}

	virtual int SlotFromSteamID64(uint64_t steamID64) override
	{
		KZPlayer *player = g_pKZPlayerManager->SteamIdToPlayer(steamID64);
		return player ? player->GetPlayerSlot().Get() : -1;
	}

	virtual const char *GetPlayerName(int slot) override
	{
		KZPlayer *player = ResolveSlot(slot);
		// GetName() rewrites the player's own sanitizedName buffer, hence the "copy it"
		// note on the interface.
		return player ? player->GetName() : "";
	}

	virtual bool IsPlayerBanned(int slot) override
	{
		KZPlayer *player = ResolveSlot(slot);
		return player && player->anticheatService ? player->anticheatService->isBanned : false;
	}

	virtual int GetSpectatedSlot(int slot) override
	{
		KZPlayer *player = ResolveSlot(slot);
		if (!player || !player->specService)
		{
			return -1;
		}
		// GetSpectatedPlayer already returns null for an alive player and for someone
		// watching their own corpse, which is what "not spectating anyone" should mean.
		KZPlayer *target = player->specService->GetSpectatedPlayer();
		return target ? SlotOf(target) : -1;
	}

	virtual double GetRating(int slot) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		// currentRating is already -1 until the API answers, so an invalid slot returning
		// the same value keeps a single "no rating" case for consumers.
		return profile ? profile->currentRating : -1.0;
	}

	virtual const char *GetClanTag(int slot) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		return profile ? profile->clanTag : "";
	}

	virtual void SetClanTagOverride(int slot, const char *tag) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		if (profile)
		{
			profile->SetClanTagOverride(tag);
		}
	}

	virtual const char *GetClanTagOverride(int slot) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		return profile ? profile->GetClanTagOverride() : "";
	}

	virtual const char *GetChatPrefix(int slot) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		if (!profile)
		{
			return "";
		}
		// Handed over with the color tokens intact. Stripping them here would have to
		// re-implement the token grammar in utils_print.cpp, and a consumer that wants
		// them gone knows its own output format better than this does.
		V_strncpy(prefixBuffer, profile->GetPrefix().c_str(), sizeof(prefixBuffer));
		return prefixBuffer;
	}

	virtual void SetChatPrefixOverride(int slot, const char *prefix) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		if (profile)
		{
			profile->SetChatPrefixOverride(prefix);
		}
	}

	virtual const char *GetChatPrefixOverride(int slot) override
	{
		KZProfileService *profile = ResolveProfile(slot);
		return profile ? profile->GetChatPrefixOverride() : "";
	}

	virtual bool GetMovementState(int slot, KZMovementState *out) override
	{
		KZPlayer *player = ResolveSlot(slot);
		if (!player || !out)
		{
			return false;
		}

		KZMovementState state {};

		Vector origin;
		Vector eyeOrigin;
		Vector velocity;
		QAngle angles;
		player->GetOrigin(&origin);
		player->GetEyeOrigin(&eyeOrigin);
		player->GetVelocity(&velocity);
		player->GetAngles(&angles);
		CopyVector(origin, state.origin);
		CopyVector(eyeOrigin, state.eyeOrigin);
		CopyVector(velocity, state.velocity);
		state.eyeAngles[0] = angles.x;
		state.eyeAngles[1] = angles.y;
		state.eyeAngles[2] = angles.z;

		state.buttons = ButtonMaskOf(player);
		state.jumpedThisTick = player->hudService && player->hudService->JumpedThisTick();

		state.alive = player->IsAlive();
		MoveType_t moveType = player->GetMoveType();
		state.onLadder = moveType == MOVETYPE_LADDER;
		state.noclipping = moveType == MOVETYPE_NOCLIP;

		CCSPlayerPawn *pawn = player->GetPlayerPawn();
		state.onGround = pawn ? !!(pawn->m_fFlags() & FL_ONGROUND) : false;

		CCSPlayer_MovementServices *moveServices = player->GetMoveServices();
		if (moveServices)
		{
			state.ducking = moveServices->m_bDucking();
			state.ducked = moveServices->m_bDucked();
			state.duckAmount = moveServices->m_flDuckAmount();
		}

		// Mirrors what the speed HUD tints with (KZHUDService::GetSpeedText): a ladder
		// takeoff is not a perf, and a jumpbug is a perf whose duckbug landed on the same
		// frame - the duckbug flag alone would also fire on non-perf duckbugs.
		state.perf = player->IsPerfing() && !player->possibleLadderHop && !player->takeoffFromLadder;
		if (player->hudService)
		{
			state.crouchJump = player->hudService->IsCrouchJumping();
			state.jumpbug = state.perf && player->hudService->IsFromDuckbug();
		}
		state.takeoffSpeed = player->takeoffVelocity.Length2D();

		*out = state;
		return true;
	}

	virtual bool GetTimerStatus(int slot, KZTimerStatus *out) override
	{
		KZPlayer *player = ResolveSlot(slot);
		if (!player || !out)
		{
			return false;
		}

		if (!player->timerService || !player->checkpointService)
		{
			return false;
		}

		KZTimerStatus status {};
		status.modeShortName = "";
		status.modeName = "";

		// Mirrors KZHUDService::GetTimerText/GetCheckpointText
		if (KZ::replaysystem::IsReplayBot(player))
		{
			bool running = KZ::replaysystem::GetEndTime() == 0.0f;
			status.running = running;
			status.paused = KZ::replaysystem::GetPaused();
			status.valid = true; // a replay is always of a previously-validated run
			status.time = running ? KZ::replaysystem::GetTime() : KZ::replaysystem::GetEndTime();
			status.teleportsUsed = static_cast<uint32_t>(KZ::replaysystem::GetTeleportCount());

			const char *courseName = KZ::replaysystem::GetCourseName();
			status.onCourse = courseName && courseName[0];
			status.course = DescribeCourse(status.onCourse ? KZ::course::GetCourse(courseName, false) : nullptr);
		}
		else
		{
			KZTimerService *timerService = player->timerService;
			status.running = timerService->GetTimerRunning();
			status.paused = timerService->GetPaused();
			status.valid = timerService->GetValidTimer();
			status.time = timerService->GetTime();
			status.teleportsUsed = player->checkpointService->GetTeleportCount();

			// GetCourse resolves the GUID against the current map's course list, so it comes
			// back null once the map changes out from under a stale run.
			const KZCourseDescriptor *course = timerService->GetCourse();
			status.onCourse = course != nullptr;
			status.course = DescribeCourse(course);
		}

		if (player->modeService)
		{
			status.modeShortName = player->modeService->GetModeShortName();
			status.modeName = player->modeService->GetModeName();
		}

		*out = status;
		return true;
	}

	virtual bool IsButtonPressed(int slot, KZButton button, bool onlyDown) override
	{
		KZPlayer *player = ResolveSlot(slot);
		return player ? player->IsButtonPressed((InputBitMask_t)button, onlyDown) : false;
	}

	virtual int GetStyleCount(int slot) override
	{
		KZPlayer *player = ResolveSlot(slot);
		return player ? player->styleServices.Count() : 0;
	}

	virtual bool GetStyle(int slot, int index, KZStyleInfo *out) override
	{
		KZPlayer *player = ResolveSlot(slot);
		if (!player || !out || index < 0 || index >= player->styleServices.Count())
		{
			return false;
		}

		// Both names are the style plugin's own string literals, so they stay valid as long as
		// the style is loaded - which is exactly as long as the player can have it enabled.
		KZStyleService *style = player->styleServices[index];
		KZStyleInfo info {};
		info.shortName = style->GetStyleShortName();
		info.name = style->GetStyleName();

		*out = info;
		return true;
	}

private:
	static uint64_t ButtonMaskOf(KZPlayer *player)
	{
		uint64_t mask = 0;
		for (KZButton button : allButtons)
		{
			if (player->IsButtonPressed((InputBitMask_t)button))
			{
				mask |= (uint64_t)button;
			}
		}
		return mask;
	}

	static void CopyVector(const Vector &source, float out[3])
	{
		out[0] = source.x;
		out[1] = source.y;
		out[2] = source.z;
	}
};

static_global CS2KZAPI g_CS2KZAPI;

void KZ::pubapi::Init()
{
	KZTimerService::RegisterEventListener(&timerEventForwarder);
	KZAnticheatService::RegisterEventListener(&anticheatEventForwarder);
}

void KZ::pubapi::Shutdown()
{
	KZTimerService::UnregisterEventListener(&timerEventForwarder);
	KZAnticheatService::UnregisterEventListener(&anticheatEventForwarder);
	externalListeners.clear();
}

ICS2KZ *KZ::pubapi::GetAPI()
{
	return static_cast<ICS2KZ *>(&g_CS2KZAPI);
}
