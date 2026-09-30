#include "kz_progress.h"
#include "route.h"
#include "cs2kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "kz/replays/kz_replaysystem.h"
#include "filesystem.h"
#include <thread>

#include "tier0/memdbgon.h"

struct ReferenceFile
{
	std::string uuid, path;
	u32 size;
	long modified;

	bool operator==(const ReferenceFile &other) const
	{
		return uuid == other.uuid && path == other.path && size == other.size && modified == other.modified;
	}
};

struct ProgressReference
{
	KZ::progress::Route route;
	std::vector<ReferenceFile> files;
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
	// Publish only a stable snapshot. A download or recording that changes during
	// decoding will be picked up on the next check instead of caching a partial file.
	bool changed = false;
	for (const auto &file : loadedReference.files)
	{
		if (!g_pFullFileSystem->FileExists(file.path.c_str()) || g_pFullFileSystem->Size(file.path.c_str()) != file.size
			|| g_pFullFileSystem->GetFileTime(file.path.c_str()) != file.modified)
		{
			changed = true;
			break;
		}
	}
	if (changed)
	{
		reference.nextCheck = 0;
	}
	else
	{
		// Invalid, unchanged files stay evaluated, too. Retry only when the cached
		// record UUID, file path, size or modification time changes, never on a timer.
		reference.files = std::move(loadedReference.files);
		reference.route = std::move(loadedReference.route);
		reference.generation = reference.route.length > 0 ? nextGeneration++ : 0;
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
	f64 now = Plat_FloatTime();
	if (loader.joinable() || now < reference.nextCheck)
	{
		return reference;
	}
	reference.nextCheck = now + 1;
	const PBData *world = KZTimerService::GetCachedRecord(key, true);
	const PBData *server = KZTimerService::GetCachedRecord(key, false);
	// Compare every available record by its simplified route length. A pro WR
	// can contain more failed attempts than an overall record on a hard course.
	// These UUIDs come from the existing cache; progress does not query or download.
	const char *uuids[] = {world ? world->pro.replayUUID.Get() : "", world ? world->overall.replayUUID.Get() : "",
						   server ? server->pro.replayUUID.Get() : "", server ? server->overall.replayUUID.Get() : ""};
	std::vector<ReferenceFile> files;
	for (const char *uuid : uuids)
	{
		if (!*uuid)
		{
			continue;
		}
		bool duplicate = false;
		for (const auto &file : files)
		{
			duplicate |= file.uuid == uuid;
		}
		if (duplicate)
		{
			continue;
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
		files.push_back({uuid, path, g_pFullFileSystem->Size(path), g_pFullFileSystem->GetFileTime(path)});
	}
	if (files == reference.files)
	{
		return reference;
	}
	if (files.empty())
	{
		reference.files.clear();
		reference.route = {};
		reference.generation = 0;
		return reference;
	}
	char md5[33] {};
	if (!g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5)))
	{
		return reference;
	}
	loadingKey = key;
	// Snapshot engine-owned data on the main thread. Only one candidate's compact
	// movement data and the shortest route are retained while loading off-thread.
	loader = std::thread(
		[files = std::move(files), map = std::string(g_pKZUtils->GetCurrentMapName().Get()), md5 = std::string(md5),
		 courseName = std::string(course->name), courseID = course->id, modeName = std::string(modeName)]()
		{
			loadedReference.files = files;
			for (const auto &file : files)
			{
				if (cancelLoad)
				{
					break;
				}
				KZ::replaysystem::data::ReplayMovement replay;
				bool valid = KZ::replaysystem::data::ReadReplayMovement(file.path.c_str(), replay, cancelLoad);
				if (!valid || cancelLoad || replay.header.type() != cs2kz::replay::RP_RUN || !replay.header.has_run()
					|| replay.header.map().name() != map || replay.header.map().md5() != md5 || replay.header.run().course_name() != courseName
					|| replay.header.run().mode().name() != modeName || replay.header.run().styles_size() != 0)
				{
					continue;
				}
				KZ::progress::Route route;
				if (route.Build(replay, courseID, cancelLoad) && (loadedReference.route.length == 0 || route.length < loadedReference.route.length))
				{
					loadedReference.route = std::move(route);
				}
			}
			// Atomic publication precedes joining and moving the result on the main
			// thread. The worker never accesses players or changes the record cache.
			loadReady = true;
		});
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
	f64 now = Plat_FloatTime();
	if (now < this->nextUpdate)
	{
		return;
	}
	// Give each slot a fixed phase so projections are spread across server frames.
	constexpr f64 interval = 0.1;
	f64 phase = this->player->GetPlayerSlot().Get() * interval / MAXPLAYERS;
	this->nextUpdate = (floor((now - phase) / interval) + 1) * interval + phase;
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
