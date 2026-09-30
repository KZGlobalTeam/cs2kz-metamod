#include "kz_progress.h"
#include "route.h"
#include "cs2kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "kz/replays/kz_replaysystem.h"
#include "filesystem.h"
#include <thread>

#include "tier0/memdbgon.h"

struct ProgressReference
{
	KZ::progress::Route route;
	std::string uuid;
	std::unordered_map<std::string, f64> rejected;
	u64 generation {};
	f64 nextCheck {};
};

static_global std::unordered_map<PBDataKey, ProgressReference> references;
static_global std::thread loader;
static_global std::atomic<bool> cancelLoad {false}, loadReady {false};
static_global PBDataKey loadingKey;
static_global ProgressReference loadedReference;
static_global u64 nextGeneration = 1;

static_global class KZTimerServiceEventListener_Progress : public KZTimerServiceEventListener
{
	void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override
	{
		player->progressService->Reset();
	}

	void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override
	{
		player->progressService->OnTimerEnd(courseGUID);
	}
} timerEventListener;

void KZProgressService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

void KZProgressService::Cleanup()
{
	KZTimerService::UnregisterEventListener(&timerEventListener);
	KZProgressService::ClearRoutes();
}

void KZProgressService::OnGameFrame()
{
	if (!loadReady)
	{
		return;
	}
	loader.join();
	loadReady = false;
	auto &reference = references[loadingKey];
	if (loadedReference.route.length > 0)
	{
		reference.route = std::move(loadedReference.route);
		reference.uuid = loadedReference.uuid;
		reference.generation = nextGeneration++;
	}
	else
	{
		// A file may still be downloading or being written when first observed.
		reference.rejected[loadedReference.uuid] = g_pKZUtils->GetServerGlobals()->curtime + 30;
	}
	loadedReference = {};
}

void KZProgressService::ClearRoutes()
{
	// Map changes and plugin unload join the reader before destroying its result.
	// Players hold only a generation and a distance, never pointers into this cache.
	cancelLoad = true;
	if (loader.joinable())
	{
		loader.join();
	}
	cancelLoad = false;
	loadReady = false;
	loadedReference = {};
	references.clear();
	for (i32 i = 0; i <= MAXPLAYERS; i++)
	{
		auto *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->progressService)
		{
			player->progressService->Reset();
		}
	}
}

static_function ProgressReference &GetReference(const KZCourseDescriptor *course, PluginId modeID, const char *modeName)
{
	PBDataKey key = ToPBDataKey(modeID, course->guid);
	auto &reference = references[key];
	f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (loader.joinable() || now < reference.nextCheck)
	{
		return reference;
	}
	reference.nextCheck = now + 1;
	const PBData *world = KZTimerService::GetCachedRecord(key, true);
	const PBData *server = KZTimerService::GetCachedRecord(key, false);
	// Prefer a local pro WR, then overall WR, pro SR and overall SR. Overall
	// references allow hard courses with no completed no-TP record. No extra SQL,
	// directory scans, downloads or playback requests are made for progress.
	const char *uuids[] = {world ? world->pro.replayUUID.Get() : "", world ? world->overall.replayUUID.Get() : "",
						   server ? server->pro.replayUUID.Get() : "", server ? server->overall.replayUUID.Get() : ""};
	for (const char *uuid : uuids)
	{
		auto rejected = reference.rejected.find(uuid);
		if (!*uuid || (rejected != reference.rejected.end() && now < rejected->second))
		{
			continue;
		}
		if (reference.uuid == uuid)
		{
			return reference;
		}
		char path[512];
		V_snprintf(path, sizeof(path), KZ_REPLAY_PATH "/%s.replay", uuid);
		if (!g_pFullFileSystem->FileExists(path))
		{
			V_snprintf(path, sizeof(path), KZ_REPLAY_DOWNLOADS_PATH "/%s.replay", uuid);
			if (!g_pFullFileSystem->FileExists(path))
			{
				continue;
			}
		}
		char md5[33] {};
		if (!g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5)))
		{
			return reference;
		}
		loadingKey = key;
		// Snapshot all engine-owned strings on the main thread. The existing replay
		// reader produces a private value; it never installs it as the playing replay.
		loader = std::thread(
			[path = std::string(path), uuid = std::string(uuid), map = std::string(g_pKZUtils->GetCurrentMapName().Get()), md5 = std::string(md5),
			 courseName = std::string(course->name), courseID = course->id, modeName = std::string(modeName)]()
			{
				loadedReference.uuid = uuid;
				KZ::replaysystem::data::ReplayMovement replay;
				bool valid = KZ::replaysystem::data::ReadReplayMovement(path.c_str(), replay, cancelLoad);
				if (valid && !cancelLoad && replay.header.type() == cs2kz::replay::RP_RUN && replay.header.has_run()
					&& replay.header.map().name() == map && replay.header.map().md5() == md5 && replay.header.run().course_name() == courseName
					&& replay.header.run().mode().name() == modeName && replay.header.run().styles_size() == 0)
				{
					if (!loadedReference.route.Build(replay, courseID, cancelLoad))
					{
						loadedReference.route = {};
					}
				}
				// Atomic publication makes the worker's value visible before the main
				// thread joins and moves it into the cache. The worker never accesses players.
				loadReady = true;
			});
		break;
	}
	return reference;
}

void KZProgressService::Reset()
{
	this->routeGeneration = 0;
	this->completedCourse = 0;
	this->distance = -1;
	this->nextUpdate = 0;
	this->visible = false;
}

void KZProgressService::OnTeleport()
{
	// A teleport (including CP/undo) invalidates continuity. Acquire the destination
	// geometrically instead of using a segment index from before the teleport.
	this->distance = -1;
	this->nextUpdate = 0;
	this->visible = false;
}

void KZProgressService::OnTimerEnd(u32 courseGUID)
{
	// Completion hides the result until another run starts or course is selected.
	// Merely stopping a timer does not hide positional progress.
	this->completedCourse = courseGUID;
	this->OnTeleport();
}

void KZProgressService::OnPhysicsSimulatePost()
{
	if (!this->player->IsAlive() || KZ::replaysystem::IsReplayBot(this->player))
	{
		this->OnTeleport();
		return;
	}
	f64 now = g_pKZUtils->GetServerGlobals()->curtime;
	if (now < this->nextUpdate)
	{
		return;
	}
	this->nextUpdate = now + 0.1;
	this->visible = false;
	// The timer remembers the selected course even when stopped. Its running,
	// paused and valid flags do not gate positional progress. Before selecting a
	// course, use the map's first course, matching the other course-based features.
	const auto *course = this->player->timerService->GetCourse();
	if (!course)
	{
		course = KZ::course::GetFirstCourse();
	}
	if (!course || course->guid == this->completedCourse)
	{
		return;
	}
	auto &reference = GetReference(course, KZ::mode::GetModeInfo(this->player->modeService).id, this->player->modeService->GetModeName());
	if (!reference.generation)
	{
		return;
	}
	if (this->routeGeneration != reference.generation)
	{
		this->routeGeneration = reference.generation;
		this->distance = -1;
	}
	Vector position;
	this->player->GetOrigin(&position);
	f32 travel = (position - this->lastPosition).Length();
	f64 matchedDistance;
	this->visible = reference.route.Match(position, this->distance, travel, matchedDistance, this->approximate);
	this->lastPosition = position;
	if (this->visible)
	{
		this->distance = matchedDistance;
		this->percentage = Clamp((f32)(100 * matchedDistance / reference.route.length), 0.0f, 100.0f);
	}
	else
	{
		this->distance = -1;
	}
}

bool KZProgressService::GetProgress(f32 &value, bool &estimate) const
{
	value = this->percentage;
	estimate = this->approximate;
	return this->visible && this->player->IsAlive();
}
