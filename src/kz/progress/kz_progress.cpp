#include "kz_progress.h"
#include "route.h"
#include "cs2kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "kz/replays/kz_replaysystem.h"
#include <algorithm>
#include <thread>

#include "tier0/memdbgon.h"

// Only projections are throttled; the timer and HUD retain their update frequency.
// Record-cache and completed replay-file changes drive all reference loading.
static constexpr f64 PROGRESS_UPDATE_INTERVAL = 0.1;

struct ProgressReference
{
	KZ::progress::Route route;
	std::vector<std::string> uuids;
	u64 generation {};
	u64 revision = 1;
	bool dirty = true;
};

struct ReferenceFile
{
	std::string uuid, path;
};

struct ReferenceLoadRequest
{
	PBDataKey key;
	u64 revision;
	std::vector<ReferenceFile> files;
	std::string map, md5, course, mode;
	i32 courseID;
};

struct ReferenceLoadResult
{
	PBDataKey key;
	u64 revision;
	KZ::progress::Route route;
};

static_global std::unordered_map<PBDataKey, ProgressReference> references;
static_global std::thread loader;
static_global std::atomic<bool> cancelLoad {false}, loadReady {false};
static_global ReferenceLoadResult loadedReference;
static_global u64 nextGeneration = 1;

static_function void InvalidateReference(ProgressReference &reference)
{
	reference.dirty = true;
	reference.revision++;
	reference.generation = 0;
	reference.route = {};
}

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

	void OnRecordCacheUpdated(PBDataKey key) override
	{
		auto reference = references.find(key);
		if (reference != references.end())
		{
			InvalidateReference(reference->second);
		}
	}

	void OnRecordCacheCleared() override
	{
		for (auto &[key, reference] : references)
		{
			InvalidateReference(reference);
		}
	}
} timerEventListener;

static_global class ReplayEventListener_Progress : public KZ::replaysystem::ReplayEventListener
{
	void OnReplayFileChanged(const char *uuid) override
	{
		for (auto &[key, reference] : references)
		{
			if (std::find(reference.uuids.begin(), reference.uuids.end(), uuid) != reference.uuids.end())
			{
				InvalidateReference(reference);
			}
		}
	}
} replayEventListener;

void KZProgressService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
	KZ::replaysystem::RegisterReplayEventListener(&replayEventListener);
}

void KZProgressService::Cleanup()
{
	KZTimerService::UnregisterEventListener(&timerEventListener);
	KZ::replaysystem::UnregisterReplayEventListener(&replayEventListener);
	KZProgressService::OnMapChange();
}

void KZProgressService::OnMapChange()
{
	// Readers own snapshots, never live records or players. Join before clearing
	// a map so a completed old result cannot be installed in its replacement.
	cancelLoad = true;
	if (loader.joinable())
	{
		loader.join();
	}
	cancelLoad = false;
	loadReady = false;
	loadedReference = {};
	references.clear();
	if (!g_pKZPlayerManager)
	{
		return;
	}
	for (i32 i = 0; i <= MAXPLAYERS; i++)
	{
		auto *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->progressService)
		{
			player->progressService->Reset();
		}
	}
}

void KZProgressService::OnGameFrame()
{
	if (!loadReady)
	{
		return;
	}
	loader.join();
	loadReady = false;
	auto reference = references.find(loadedReference.key);
	if (reference != references.end() && reference->second.revision == loadedReference.revision)
	{
		reference->second.route = std::move(loadedReference.route);
		reference->second.generation = reference->second.route.length > 0 ? nextGeneration++ : 0;
	}
	loadedReference = {};
}

static_function std::vector<std::string> CollectReferenceUUIDs(PBDataKey key)
{
	const PBData *world = KZTimerService::GetCachedRecord(key, true);
	const PBData *server = KZTimerService::GetCachedRecord(key, false);
	const char *uuids[] = {world ? world->pro.replayUUID.Get() : "", world ? world->overall.replayUUID.Get() : "",
						   server ? server->pro.replayUUID.Get() : "", server ? server->overall.replayUUID.Get() : ""};
	std::vector<std::string> result;
	for (const char *uuid : uuids)
	{
		if (*uuid && std::find(result.begin(), result.end(), uuid) == result.end())
		{
			result.emplace_back(uuid);
		}
	}
	return result;
}

static_function ReferenceLoadResult ReadShortestReference(const ReferenceLoadRequest &request)
{
	ReferenceLoadResult result {request.key, request.revision, {}};
	for (const auto &file : request.files)
	{
		if (cancelLoad)
		{
			break;
		}
		KZ::replaysystem::data::ReplayMovement replay;
		if (!KZ::replaysystem::data::ReadReplayMovement(file.path.c_str(), replay, cancelLoad) || replay.header.type() != cs2kz::replay::RP_RUN
			|| !replay.header.has_run() || replay.header.map().name() != request.map || replay.header.map().md5() != request.md5
			|| replay.header.run().course_name() != request.course || replay.header.run().mode().name() != request.mode
			|| replay.header.run().styles_size() != 0)
		{
			continue;
		}
		KZ::progress::Route route;
		if (route.Build(replay, request.courseID, cancelLoad) && (result.route.length == 0 || route.length < result.route.length))
		{
			result.route = std::move(route);
		}
	}
	return result;
}

static_function void StartReferenceLoad(PBDataKey key, ProgressReference &reference, const KZCourseDescriptor *course, const char *modeName)
{
	if (!reference.dirty || loader.joinable())
	{
		return;
	}
	char md5[33] {};
	if (!g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5)))
	{
		return;
	}
	reference.dirty = false;
	reference.uuids = CollectReferenceUUIDs(key);
	ReferenceLoadRequest request {key, reference.revision, {}, g_pKZUtils->GetCurrentMapName().Get(), md5, course->name, modeName, course->id};
	for (const auto &uuid : reference.uuids)
	{
		std::string path;
		if (KZ::replaysystem::FindReplayPath(uuid.c_str(), path))
		{
			request.files.push_back({uuid, std::move(path)});
		}
	}
	if (request.files.empty())
	{
		return;
	}
	// Failed reads stay evaluated until a record or completed file write changes
	// the inputs. No periodic lookup, stat call or timed decode retry is needed.
	loader = std::thread(
		[request = std::move(request)]()
		{
			loadedReference = ReadShortestReference(request);
			loadReady = true;
		});
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
	this->distance = -1;
	this->nextUpdate = 0;
	this->visible = false;
}

void KZProgressService::OnTimerEnd(u32 courseGUID)
{
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
	f64 now = Plat_FloatTime();
	if (now < this->nextUpdate)
	{
		return;
	}
	f64 phase = this->player->GetPlayerSlot().Get() * PROGRESS_UPDATE_INTERVAL / MAXPLAYERS;
	this->nextUpdate = (floor((now - phase) / PROGRESS_UPDATE_INTERVAL) + 1) * PROGRESS_UPDATE_INTERVAL + phase;
	this->visible = false;
	const auto *course = this->player->timerService->GetCourse();
	if (!course)
	{
		course = KZ::course::GetFirstCourse();
	}
	if (!course || course->guid == this->completedCourse)
	{
		return;
	}
	PBDataKey key = ToPBDataKey(KZ::mode::GetModeInfo(this->player->modeService).id, course->guid);
	auto &reference = references[key];
	StartReferenceLoad(key, reference, course, this->player->modeService->GetModeName());
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
