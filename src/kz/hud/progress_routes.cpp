#include "progress_routes.h"
#include "kz/hud/kz_hud.h"
#include "progress/replay_route.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"
#include "kz/replays/watcher.h"
#include "filesystem.h"
#include "KeyValues.h"
#include <charconv>
#include <future>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

extern ReplayWatcher g_ReplayWatcher;
extern CConVar<bool> kz_progress_enable;

namespace KZ::progress
{
	using Key = std::pair<u32, std::string>;
	static std::map<Key, std::shared_ptr<const Route>> routes;
	static std::future<std::vector<std::shared_ptr<Route>>> loading;
	static std::atomic<bool> cancel {false};
	static std::set<std::string> attempted;
	static std::set<std::string> reread;
	static std::string mapName, mapMD5;
	static bool refreshNeeded = true;
	static u64 refreshRevision {}, workerRevision {}, replayRevision {};
	static u64 generation = 1;
	static std::map<Key, std::string> pinned, pending, inflightRequests;
	static constexpr const char *selectionFile = "addons/cs2kz/data/progress-references.cfg";

	static std::string SelectionPath()
	{
		// GAME's first writable search path may be addons/metamod. Pin data to csgo.
		return std::string(Plat_GetGameDirectory()) + "/csgo/" + selectionFile;
	}

	static std::string SelectionKey(i32 courseID, const std::string &mode)
	{
		return mapMD5 + "_" + std::to_string(courseID) + "_" + mode;
	}

	static bool SaveSelection(i32 courseID, const std::string &mode, const std::string &uuid)
	{
		std::unique_ptr<KeyValues> kv(new KeyValues("ProgressReferences"));
		const auto absolute = SelectionPath();
		if (g_pFullFileSystem->FileExists(absolute.c_str()) && !kv->LoadFromFile(g_pFullFileSystem, absolute.c_str()))
		{
			Warning("[Progress] Cannot read selection file; preserving it.\n");
			return false;
		}
		kv->SetString(SelectionKey(courseID, mode).c_str(), uuid.c_str());
		const auto directory = std::string(Plat_GetGameDirectory()) + "/csgo/addons/cs2kz/data";
		g_pFullFileSystem->CreateDirHierarchy(directory.c_str());
		const std::string temporary = std::string(selectionFile) + ".tmp";
		if (!kv->SaveToFile(g_pFullFileSystem, (absolute + ".tmp").c_str(), nullptr, true) || !utils::RenameFile(temporary.c_str(), selectionFile))
		{
			Warning("[Progress] Could not save reference selection; previous selection retained.\n");
			return false;
		}
		return true;
	}

	static std::string SavedSelection(i32 courseID, const std::string &mode)
	{
		std::unique_ptr<KeyValues> kv(new KeyValues("ProgressReferences"));
		kv->LoadFromFile(g_pFullFileSystem, SelectionPath().c_str());
		return kv->GetString(SelectionKey(courseID, mode).c_str(), "");
	}

	void Clear()
	{
		cancel = true;
		if (loading.valid())
		{
			loading.get();
		}
		if (g_pKZPlayerManager)
		{
			for (u32 i = 0; i <= MAXPLAYERS; ++i)
			{
				auto *p = g_pKZPlayerManager->ToPlayer(i);
				if (p && p->hudService)
				{
					p->hudService->ResetProgress();
				}
			}
		}
		routes.clear();
		pinned.clear();
		pending.clear();
		inflightRequests.clear();
		attempted.clear();
		reread.clear();
		mapName.clear();
		mapMD5.clear();
		InvalidateRoutes();
		++generation;
	}

	void OnMapReady()
	{
		mapName = g_pKZUtils->GetCurrentMapName().Get();
		char md5[33] {};
		if (g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5)))
		{
			mapMD5 = md5;
		}
		InvalidateRoutes();
	}

	void InvalidateRoutes()
	{
		refreshNeeded = true;
		++refreshRevision;
		attempted.clear();
	}

	void Reload()
	{
		for (const auto &entry : routes)
		{
			reread.insert(entry.second->source);
		}
		InvalidateRoutes();
		Msg("[Progress] Cached record references will be re-read; the current HUD route remains until validation succeeds.\n");
	}

	std::shared_ptr<const Route> GetRoute(u32 guid, const char *mode)
	{
		auto it = routes.find({guid, mode});
		return it == routes.end() ? nullptr : it->second;
	}

	void OnGameFrame()
	{
		const u64 indexRevision = g_ReplayWatcher.GetRevision();
		if (indexRevision != replayRevision)
		{
			replayRevision = indexRevision;
			InvalidateRoutes();
		}
		if (loading.valid())
		{
			if (loading.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			{
				return;
			}
			auto loaded = loading.get();
			// A record refresh or newer admin selection supersedes this batch.
			if (workerRevision == refreshRevision)
			{
				for (const auto &[key, uuid] : inflightRequests)
				{
					auto request = pending.find(key);
					if (request == pending.end() || request->second != uuid)
					{
						continue;
					}
					auto accepted = std::find_if(loaded.begin(), loaded.end(), [&](const auto &r)
												 { return r->courseGUID == key.first && r->mode == key.second && r->source == uuid; });
					if (accepted == loaded.end())
					{
						Warning("[Progress] Reference %s failed validation; previous route retained.\n", uuid.c_str());
					}
					else if (SaveSelection((*accepted)->courseID, key.second, uuid))
					{
						pinned[key] = uuid;
					}
					else
					{
						loaded.erase(accepted);
					}
					pending.erase(request);
				}
				for (auto &r : loaded)
				{
					const auto *course = KZ::course::GetCourse(r->courseGUID);
					if (!course || course->id != r->courseID || r->courseName != course->name)
					{
						continue;
					}
					r->generation = ++generation;
					routes[{r->courseGUID, r->mode}] = r;
					reread.erase(r->source);
					KZ_LOG_INFO(LogChannel::General, "[Progress] Published course %s (%d), mode %s: %zu points, replay %s\n", r->courseName.c_str(),
								r->courseID, r->mode.c_str(), r->points.size(), r->source.c_str());
				}
			}
			inflightRequests.clear();
			refreshNeeded = true;
		}
		if (!kz_progress_enable.Get() || mapName.empty() || !refreshNeeded)
		{
			return;
		}
		if (mapMD5.empty())
		{
			char digest[33] {};
			if (!g_pKZUtils->GetCurrentMapMD5(digest, sizeof(digest)))
			{
				return;
			}
			mapMD5 = digest;
		}
		refreshNeeded = false;
		// Read existing indexes/caches only. No database query, directory scan, download or playback request.
		auto headers = g_ReplayWatcher.GetProgressCandidates(mapName.c_str(), mapMD5.c_str());
		std::set<std::string> available;
		for (const auto &entry : headers)
		{
			available.insert(entry.first.ToString());
		}
		std::vector<ReplayCandidate> candidates;
		for (const auto &entry : headers)
		{
			const auto &hdr = entry.second;
			const auto *course = KZ::course::GetCourse(hdr.run().course_name().c_str());
			if (!course)
			{
				continue;
			}
			Key key {course->guid, hdr.run().mode().name()};
			const std::string uuid = entry.first.ToString();
			if (!pinned.count(key))
			{
				pinned[key] = SavedSelection(course->id, key.second);
			}
			auto request = pending.find(key);
			std::string wanted = request != pending.end() ? request->second : pinned[key];
			if (wanted.empty())
			{
				auto mode = KZ::mode::GetModeInfo(CUtlString(key.second.c_str()));
				const auto *wr = KZTimerService::GetCachedRecord(course, mode.id, true);
				const auto *sr = KZTimerService::GetCachedRecord(course, mode.id, false);
				// Prefer the cached WR pro reference if present locally, otherwise cached SR pro.
				const auto active = routes.find(key);
				if (wr && available.count(wr->pro.replayUUID.Get())
					&& (!attempted.count(wr->pro.replayUUID.Get()) || (active != routes.end() && active->second->source == wr->pro.replayUUID.Get())))
				{
					wanted = wr->pro.replayUUID.Get();
				}
				else if (sr)
				{
					wanted = sr->pro.replayUUID.Get();
				}
			}
			if (wanted.empty() || uuid != wanted || attempted.count(uuid))
			{
				continue;
			}
			auto current = routes.find(key);
			if (current != routes.end() && current->second->source == uuid && request == pending.end() && !reread.count(uuid))
			{
				continue;
			}
			if (candidates.size() == 32)
			{
				refreshNeeded = true;
				break;
			}
			char relative[256];
			CBufferString path;
			V_snprintf(relative, sizeof(relative), KZ_REPLAY_PATH "/%s.replay", uuid.c_str());
			if (!g_pFullFileSystem->FileExists(relative, "GAME"))
			{
				V_snprintf(relative, sizeof(relative), KZ_REPLAY_DOWNLOADS_PATH "/%s.replay", uuid.c_str());
			}
			if (!g_pFullFileSystem->RelativePathToFullPath(relative, "GAME", path))
			{
				continue;
			}
			candidates.push_back({path.Get(), uuid, mapName, mapMD5, course->name, key.second, course->id, course->guid, hdr.run().time()});
			attempted.insert(uuid);
			if (request != pending.end())
			{
				inflightRequests[key] = uuid;
			}
		}
		if (candidates.empty())
		{
			return;
		}
		cancel = false;
		workerRevision = refreshRevision;
		// Value snapshots and read-only streams only. Publish on the main thread.
		loading = std::async(std::launch::async,
							 [batch = std::move(candidates)]()
							 {
								 std::vector<std::shared_ptr<Route>> result;
								 for (const auto &candidate : batch)
								 {
									 if (cancel)
									 {
										 break;
									 }
									 if (auto route = ReadReferenceReplay(candidate, cancel))
									 {
										 result.push_back(std::move(route));
									 }
								 }
								 return result;
							 });
	}

	static auto CourseByID(i32 id)
	{
		for (u32 i = 0; i < KZ::course::GetCourseCount(); ++i)
		{
			auto *course = KZ::course::GetCourseByIndex(i);
			if (course->id == id)
			{
				return course;
			}
		}
		return decltype(KZ::course::GetCourseByIndex(0))(nullptr);
	}

	static bool CourseID(const char *text, i32 &id)
	{
		auto end = text + std::strlen(text);
		auto parsed = std::from_chars(text, end, id);
		return parsed.ec == std::errc() && parsed.ptr == end && id > 0;
	}

	void ListReferences()
	{
		Msg("[Progress] Map %s, MD5 %s. Validated reference updates are applied live by HUDService.\n", mapName.c_str(), mapMD5.c_str());
		for (u32 i = 0; i < KZ::course::GetCourseCount(); ++i)
		{
			auto *course = KZ::course::GetCourseByIndex(i);
			Msg("[Progress] Course %d: %s\n", course->id, course->name);
		}
		for (const auto &entry : routes)
		{
			const auto &r = entry.second;
			Msg("[Progress] course=%d (%s) mode=%s active=%s selection=%s\n", r->courseID, r->courseName.c_str(), r->mode.c_str(), r->source.c_str(),
				pinned[entry.first].empty() ? "auto" : pinned[entry.first].c_str());
		}
		for (const auto &entry : pending)
		{
			Msg("[Progress] pending courseGUID=%u mode=%s UUID=%s\n", entry.first.first, entry.first.second.c_str(), entry.second.c_str());
		}
	}

	void ListReplays(const CCommand &args)
	{
		i32 id = 0;
		if (args.ArgC() > 3 || (args.ArgC() >= 2 && !CourseID(args[1], id)))
		{
			Msg("Usage: kz_progress_replays [courseID] [mode]\n");
			return;
		}
		auto headers = g_ReplayWatcher.GetProgressCandidates(mapName.c_str(), mapMD5.c_str());
		std::sort(headers.begin(), headers.end(), [](const auto &a, const auto &b) { return a.second.run().time() < b.second.run().time(); });
		u32 count = 0;
		for (const auto &entry : headers)
		{
			const auto &run = entry.second.run();
			const auto *course = KZ::course::GetCourse(run.course_name().c_str());
			if (!course || (id && id != course->id) || (args.ArgC() == 3 && run.mode().name() != args[2]))
			{
				continue;
			}
			Msg("[Progress] %s course=%d (%s) mode=%s time=%.3f player=%s\n", entry.first.ToString().c_str(), course->id, course->name,
				run.mode().name().c_str(), run.time(), entry.second.player().name().c_str());
			if (++count == 100)
			{
				break;
			}
		}
		Msg("[Progress] %u eligible replay headers shown (maximum 100). Selection performs full validation.\n", count);
	}

	void SelectReference(const CCommand &args)
	{
		i32 id;
		if (args.ArgC() != 4 || !CourseID(args[1], id))
		{
			Msg("Usage: kz_progress_reference <courseID> <mode> <UUID|auto>\n");
			return;
		}
		const auto *course = CourseByID(id);
		if (!course || mapMD5.empty())
		{
			Warning("[Progress] Unknown course or map not ready.\n");
			return;
		}
		std::string mode = args[2], uuid = args[3];
		if (mode.empty() || mode.size() > 64
			|| mode.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
		{
			Warning("[Progress] Invalid mode; copy its exact name from kz_progress_replays.\n");
			return;
		}
		Key key {course->guid, mode};
		if (uuid == "auto")
		{
			// Do not let an old worker re-pin a superseded selection.
			cancel = true;
			if (loading.valid())
			{
				loading.get();
			}
			inflightRequests.clear();
			if (!SaveSelection(id, mode, ""))
			{
				return;
			}
			pinned[key].clear();
			pending.erase(key);
			attempted.clear();
			InvalidateRoutes();
			Msg("[Progress] Automatic reference selection restored for course %d, mode %s. HUD updates after validation.\n", id, mode.c_str());
			return;
		}
		for (const auto &entry : g_ReplayWatcher.GetProgressCandidates(mapName.c_str(), mapMD5.c_str()))
		{
			if (entry.first.ToString() != uuid || entry.second.run().course_name() != course->name || entry.second.run().mode().name() != mode)
			{
				continue;
			}
			pending[key] = uuid;
			attempted.erase(uuid);
			InvalidateRoutes();
			Msg("[Progress] Validation queued: %s. Current route stays active until the replacement is accepted.\n", uuid.c_str());
			return;
		}
		Warning("[Progress] UUID not eligible for this map/MD5/course/mode. Use kz_progress_replays; requires a completed no-TP, no-style replay.\n");
	}
} // namespace KZ::progress

CON_COMMAND_F(kz_progress_routes, "List course IDs and active progress references (server/RCON).", FCVAR_NONE)
{
	if (context.GetPlayerSlot().Get() >= 0)
	{
		return;
	}
	KZ::progress::ListReferences();
}

CON_COMMAND_F(kz_progress_replays, "List eligible progress reference replays (server/RCON).", FCVAR_NONE)
{
	if (context.GetPlayerSlot().Get() >= 0)
	{
		return;
	}
	KZ::progress::ListReplays(args);
}

CON_COMMAND_F(kz_progress_reference, "Select persistent progress reference: <courseID> <mode> <UUID|auto> (server/RCON).", FCVAR_NONE)
{
	if (context.GetPlayerSlot().Get() >= 0)
	{
		return;
	}
	KZ::progress::SelectReference(args);
}
