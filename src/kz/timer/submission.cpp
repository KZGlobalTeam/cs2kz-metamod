#include "submission.h"
#include "kz/db/kz_db.h"
#include "kz/anticheat/kz_anticheat.h"
#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/recording/kz_recording.h"
#include "kz/style/kz_style.h"
#include "kz/option/kz_option.h"
#include "kz/replays/kz_replay.h"
#include "utils/async_file_io.h"
#include "utils/utils.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

CConVar<bool> kz_debug_announce_global("kz_debug_announce_global", FCVAR_NONE, "Print debug info for record announcements.", false);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void BuildReplayPath(char *buf, int bufLen, const UUID_t &uuid)
{
	V_snprintf(buf, bufLen, "%s/%s.replay", KZ_REPLAY_PATH, uuid.ToString().c_str());
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

RunSubmission::RunSubmission(KZPlayer *player)
	: uid(RunSubmission::idCount++), timestamp(g_pKZUtils->GetServerGlobals()->realtime), mapGeneration(RunSubmission::currentMapGeneration),
	  userID(player->GetClient()->GetUserID()), localUUID(player->recordingService->GetCurrentRunUUID()),
	  finalUUID(player->recordingService->GetCurrentRunUUID()), time(player->timerService->GetTime()),
	  teleports(player->checkpointService->GetTeleportCount())
{
	this->local = KZDatabaseService::IsReady() && KZDatabaseService::IsMapSetUp();

	if (kz_debug_announce_global.Get() && !(player->CheckPrime() && KZGlobalService::MayBecomeAvailable()))
	{
		if (!player->hasPrime)
		{
			KZ_LOG_INFO(LogChannel::Global, "[%u] Player %s does not have Prime, will not submit globally.\n", uid, player->GetName());
		}
		if (!KZGlobalService::IsAvailable())
		{
			KZ_LOG_INFO(LogChannel::Global, "[%u] Global service is not available, will not submit globally.\n", uid);
		}
	}

	// Setup player snapshot
	this->player.name = player->GetName();
	this->player.steamid64 = player->GetSteamId64();

	// Setup mode
	auto mode = KZ::mode::GetModeInfo(player->modeService);
	this->mode.name = mode.shortModeName;
	KZ::api::Mode decodedMode;
	if (KZ::api::DecodeModeString(this->mode.name, decodedMode))
	{
		this->apiMode = decodedMode;
	}
	else if (kz_debug_announce_global.Get())
	{
		KZ_LOG_INFO(LogChannel::Global, "[%u] Mode '%s' is not a valid global mode, will not submit globally.\n", uid, this->mode.name.c_str());
	}
	this->mode.md5 = mode.md5;
	if (mode.databaseID <= 0)
	{
		this->local = false;
	}
	else
	{
		this->mode.localID = mode.databaseID;
	}

	// Setup map
	this->map.name = g_pKZUtils->GetServerGlobals()->mapname.ToCStr();
	char md5[33];
	g_pKZUtils->GetCurrentMapMD5(md5, sizeof(md5));
	this->map.md5 = md5;

	// Setup course
	assert(player->timerService->GetCourse());
	this->course.name = player->timerService->GetCourse()->GetName().Get();
	this->course.localID = player->timerService->GetCourse()->localDatabaseID;

	if (this->apiMode.has_value())
	{
		// clang-format off
		KZGlobalService::WithCurrentMapState([&](const std::optional<KZ::api::Map> &currentMapInfo, bool confirmed)
		{
			if (!confirmed)
			{
				// map_change reply still pending, or the API connection itself is down/
				// reconnecting. Defer instead of assuming non-global; TryFinalize() polls for
				// this to be confirmed later (for this run's own map, never a different one).
				this->mapResolutionPending = true;
				return;
			}

			if (!currentMapInfo.has_value())
			{
				return; // confirmed not-global; this->global stays false
			}

			this->global = ResolveGlobalFilterID(*currentMapInfo);

			if (!this->global && kz_debug_announce_global.Get())
			{
				KZ_LOG_INFO(LogChannel::Global, "[%u] Course '%s' not found on global map '%s', will not submit globally.\n", uid,
							 this->course.name.c_str(), currentMapInfo->name.c_str());
				KZ_LOG_INFO(LogChannel::Global, "[%u] Available courses:\n", uid);
				for (const KZ::api::Map::Course &c : currentMapInfo->courses)
				{
					KZ_LOG_INFO(LogChannel::Global, " - %s\n", c.name.c_str());
				}
			}
		});
		// clang-format on
	}

	// Setup styles
	FOR_EACH_VEC(player->styleServices, i)
	{
		auto style = KZ::style::GetStyleInfo(player->styleServices[i]);
		this->styles.push_back({player->styleServices[i]->GetStyleShortName(), style.md5});
		if (style.databaseID < 0)
		{
			this->local = false;
		}
		this->styleIDs |= (1ull << style.databaseID);
	}

	// Metadata
	this->metadata = player->timerService->GetCurrentRunMetadata().Get();

	// Cheaters and unauthenticated players do not submit
	if (player->anticheatService->isBanned || !player->IsAuthenticated())
	{
		this->local = false;
		this->global = false;
		this->mapResolutionPending = false; // no point deferring for a banned/unauth player
	}

	// Snapshot previous global PBs for diff display
	{
		auto pbData = player->timerService->GetGlobalCachedPB(player->timerService->GetCourse(), mode.id);
		if (pbData)
		{
			this->oldGPB.overall.time = pbData->overall.pbTime;
			this->oldGPB.overall.points = pbData->overall.points;
			this->oldGPB.pro.time = pbData->pro.pbTime;
			this->oldGPB.pro.points = pbData->pro.points;
		}
	}

	// --- Kick off submissions ---
	// Global submission is always started immediately so the API gets the run data as soon as possible.
	// Local DB insert is deferred to TryFinalize() so the API has a chance to supply the canonical UUID
	// before we write to the DB.
	// Exception: non-global runs insert to DB immediately (no UUID synchronization needed).
	if (mapResolutionPending)
	{
		// We don't know how long the API will take to respond, so we don't want to block the local insert on it.
		if (local)
		{
			SubmitLocal(localUUID.ToString().c_str());
		}
	}
	else if (global)
	{
		SubmitGlobal();
	}
	else if (local)
	{
		// No global UUID to wait for; insert now with the locally-generated UUID.
		SubmitLocal(localUUID.ToString().c_str());
	}
}

// ---------------------------------------------------------------------------
// API response
// ---------------------------------------------------------------------------

bool RunSubmission::ResolveGlobalFilterID(const KZ::api::Map &mapInfo)
{
	if (!this->apiMode.has_value())
	{
		return false;
	}

	const KZ::api::Map::Course *course = nullptr;
	for (const KZ::api::Map::Course &c : mapInfo.courses)
	{
		if (KZ_STREQ(c.name.c_str(), this->course.name.c_str()))
		{
			course = &c;
			break;
		}
	}

	if (course == nullptr)
	{
		return false;
	}

	this->globalFilterID = (*this->apiMode == KZ::api::Mode::Classic) ? course->filters.classic.id : course->filters.vanilla.id;
	return true;
}

void RunSubmission::SubmitGlobal()
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
	if (!player || player->anticheatService->isBanned)
	{
		this->global = false;
		return;
	}

	KZGlobalService::RecordData data;
	data.filterID = this->globalFilterID;
	data.time = this->time;
	data.teleports = this->teleports;
	data.modeMD5 = this->mode.md5;
	data.styles = this->styles;
	data.metadata = this->metadata;

	KZGlobalService::MessageCallback<KZ::api::messages::NewRecordAck> callback(RunSubmission::OnGlobalRecordSubmitted, this->uid);
	KZGlobalService::SubmitRecordResult submissionResult = player->globalService->SubmitRecord(std::move(data), std::move(callback));

	if (kz_debug_announce_global.Get())
	{
		KZ_LOG_INFO(LogChannel::Global, "[%u] Global record submission result: %d\n", uid, static_cast<int>(submissionResult));
	}

	switch (submissionResult)
	{
		case KZGlobalService::SubmitRecordResult::PlayerNotAuthenticated:
		case KZGlobalService::SubmitRecordResult::MapNotGlobal:
		{
			this->global = false;
			// No API UUID will arrive; insert locally now with localUUID.
			if (this->local && !this->localSubmitted)
			{
				SubmitLocal(localUUID.ToString().c_str());
			}
			break;
		}
		case KZGlobalService::SubmitRecordResult::Queued:
		{
			// The message will be sent once the API reconnects. TryFinalize() commits the local
			// row with localUUID right away (it doesn't wait on pendingQueuedSubmission); once the
			// late ack arrives, DoLateAPIResponse() reconciles the UUID and uploads the replay.
			// CheckAll() keeps this submission alive until that ack arrives.
			this->pendingQueuedSubmission = true;
			break;
		}
		case KZGlobalService::SubmitRecordResult::NotConnected:
		{
			this->global = false;
			if (this->local && !this->localSubmitted)
			{
				SubmitLocal(localUUID.ToString().c_str());
			}
			break;
		}
		case KZGlobalService::SubmitRecordResult::Submitted:
		{
			this->global = true;
			break;
		}
	}
}

void RunSubmission::OnAPIResponse(const KZ::api::messages::NewRecordAck &ack)
{
	apiResponseReceived = true;
	globalResponse.received = true;
	globalResponse.recordId = ack.recordId;
	globalResponse.overall.rank = ack.overallData.rank;
	globalResponse.overall.points = ack.overallData.points;
	globalResponse.overall.maxRank = ack.overallData.leaderboardSize;
	globalResponse.pro.rank = ack.proData.rank;
	globalResponse.pro.points = ack.proData.points;
	globalResponse.pro.maxRank = ack.proData.leaderboardSize;

	pendingQueuedSubmission = false;

	if (finalized)
	{
		// We already committed with localUUID — patch things up retroactively.
		DoLateAPIResponse(ack.recordId);
	}
	else
	{
		TryFinalize();
	}
}

// ---------------------------------------------------------------------------
// Replay ready
// ---------------------------------------------------------------------------

void RunSubmission::OnReplayReady(std::vector<char> &&buffer)
{
	replayBuffer = std::move(buffer);
	replayReady = true;

	// Eagerly resolve finalUUID if the API already responded, so the disk write uses the
	// right path. If the API responds later, DoLateAPIResponse handles the rename.
	if (apiResponseReceived && !globalResponse.recordId.empty())
	{
		finalUUID = UUID_t(globalResponse.recordId.c_str());
	}

	// Write replay to disk and notify the player regardless of local/global state.
	//    QueueWriteBuffer takes the buffer by value, so replayBuffer is copy-constructed
	//    and the original remains available for QueueReplayUpload below.
	char replayPath[512];
	BuildReplayPath(replayPath, sizeof(replayPath), finalUUID);
	if (g_asyncFileIO)
	{
		g_asyncFileIO->QueueWriteBuffer(replayPath, replayBuffer);
		// Notify the player now (write is async but fire-and-forget, effectively always succeeds).
		KZPlayer *callbackPlayer = this->IsFromPreviousMap() ? nullptr : g_pKZPlayerManager->ToPlayer(userID);
		if (callbackPlayer)
		{
			callbackPlayer->languageService->PrintChat(true, false, "Replay - Run Replay Saved", finalUUID.ToString().c_str());
			callbackPlayer->languageService->PrintConsole(false, false, "Replay - Run Replay Saved (Console)", finalUUID.ToString().c_str());
		}
	}

	if (finalized)
	{
		if (global && apiResponseReceived && !globalResponse.recordId.empty() && !replayBuffer.empty())
		{
			KZGlobalService::QueueReplayUpload(finalUUID, std::vector<char>(replayBuffer));

			if (KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880) == 0)
			{
				char deletePath[512];
				BuildReplayPath(deletePath, sizeof(deletePath), finalUUID);
				utils::RemoveFile(deletePath);
			}
		}
	}
	else
	{
		TryFinalize();
	}

	// A queued submission parks here until its ack arrives, which can be minutes away or never.
	// Holding the whole replay in RAM for that entire time is a waste of memory.
	// DoLateAPIResponse() should take care of reading it back from disk later.
	if (pendingQueuedSubmission && g_asyncFileIO)
	{
		replayBuffer.clear();
		replayBuffer.shrink_to_fit();
	}
}

void RunSubmission::OnReplayFailed()
{
	if (replayReady)
	{
		return;
	}

	// There is no buffer and there never will be one, but the rest of the submission still has to
	// finish. Mark the replay side done so CheckAll() stops holding this submission alive forever.
	replayReady = true;

	if (!finalized)
	{
		TryFinalize();
	}
}

// ---------------------------------------------------------------------------
// TryFinalize
// ---------------------------------------------------------------------------

void RunSubmission::TryFinalize()
{
	if (finalized)
	{
		return;
	}

	const f64 now = g_pKZUtils->GetServerGlobals()->realtime;

	if (mapResolutionPending)
	{
		bool giveUp = !KZGlobalService::MayBecomeAvailable() || now >= (timestamp + RunSubmission::timeout);
		bool resolved = false;
		bool isGlobal = false;

		// clang-format off
		KZGlobalService::WithCurrentMapState([&](const std::optional<KZ::api::Map> &currentMapInfo, bool confirmed)
		{
			if (!confirmed)
			{
				return;
			}
			resolved = true;

			// Confirmed not-global, or confirmed for a DIFFERENT map than this run was played on
			// (the map changed after the run and the API confirmed the new one); never submit
			// globally in that case. 
			if (!currentMapInfo.has_value() || currentMapInfo->name != this->map.name)
			{
				return;
			}

			isGlobal = ResolveGlobalFilterID(*currentMapInfo);
		});
		// clang-format on

		if (!resolved && !giveUp)
		{
			return; // keep polling next frame
		}

		mapResolutionPending = false;
		this->global = resolved && isGlobal;

		if (kz_debug_announce_global.Get())
		{
			KZ_LOG_INFO(LogChannel::Global, "[%u] Deferred map resolution: global=%s (resolved=%s, gaveUp=%s)\n", uid,
						this->global ? "true" : "false", resolved ? "true" : "false", giveUp ? "true" : "false");
		}

		// Nothing to do for the local side here: deferring always commits the local row in the
		// constructor, so it is already in flight regardless of how this resolves.
		if (this->global)
		{
			SubmitGlobal();
		}
	}

	// No submissions — nothing to finalize; just mark done so CheckAll() can announce and GC.
	if (!local && !global)
	{
		finalized = true;
		return;
	}

	const bool timeoutReached = now >= (timestamp + RunSubmission::timeout);

	// For global runs that haven't heard back from the API yet:
	// keep waiting unless we've timed out or the run was only queued (offline).
	if (global && !apiResponseReceived && !timeoutReached && !pendingQueuedSubmission)
	{
		return;
	}

	finalized = true;

	// Determine the authoritative UUID for this run (if not already set in OnReplayReady).
	if (apiResponseReceived && !globalResponse.recordId.empty())
	{
		finalUUID = UUID_t(globalResponse.recordId.c_str());
	}
	// else: finalUUID stays equal to localUUID (set in constructor)

	// 1. Local DB insert with the final UUID. This is the only place a global run's local row is
	// written: the constructor deliberately defers it so the API has a chance to supply the
	// canonical UUID first. Runs that already inserted with localUUID in the constructor
	// (non-global, or deferred map resolution) are skipped by the localSubmitted guard.
	if (local && !localSubmitted)
	{
		SubmitLocal(finalUUID.ToString().c_str());
	}

	// 2. Upload replay to the global API if the run was accepted.
	if (global && apiResponseReceived && !globalResponse.recordId.empty() && !replayBuffer.empty())
	{
		KZGlobalService::QueueReplayUpload(finalUUID, std::vector<char>(replayBuffer));

		// Delete local replay file after uploading if archiveRetentionMinutes is 0.
		if (KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880) == 0)
		{
			char deletePath[512];
			BuildReplayPath(deletePath, sizeof(deletePath), finalUUID);
			utils::RemoveFile(deletePath);
		}
	}
}

// ---------------------------------------------------------------------------
// Late API response (API replied after we already finalized with localUUID)
// ---------------------------------------------------------------------------

void RunSubmission::DoLateAPIResponse(const std::string &apiUUID)
{
	if (!g_asyncFileIO)
	{
		return;
	}

	UUID_t apiFinalUUID(apiUUID.c_str());

	char oldPath[512], newPath[512];
	BuildReplayPath(oldPath, sizeof(oldPath), localUUID);
	BuildReplayPath(newPath, sizeof(newPath), apiFinalUUID);

	// Rename replay file on the bg thread
	g_asyncFileIO->QueueRename(oldPath, newPath);

	// Update DB row
	KZDatabaseService::UpdateRunUUID(localUUID.ToString().c_str(), apiUUID.c_str(), nullptr, nullptr);

	// Keep finalUUID consistent with the authoritative API-assigned UUID
	finalUUID = apiFinalUUID;

	// Upload replay now that we have the correct API-assigned UUID.
	if (!replayBuffer.empty())
	{
		KZGlobalService::QueueReplayUpload(finalUUID, std::vector<char>(replayBuffer));

		// Delete local replay file after uploading if archiveRetentionMinutes is 0.
		if (KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880) == 0)
		{
			char deletePath[512];
			BuildReplayPath(deletePath, sizeof(deletePath), finalUUID);
			utils::RemoveFile(deletePath);
		}
	}
	else
	{
		// The buffer was released while this submission was parked waiting for the ack (see
		// OnReplayReady), so read it back from disk.
		std::string uploadPath(newPath);
		// clang-format off
		g_asyncFileIO->QueueRead(uploadPath,
			[uploadPath, uploadUUID = apiFinalUUID](bool success, std::vector<char> &&buffer)
			{
				if (!success || buffer.empty())
				{
					KZ_LOG_WARN(LogChannel::Global, "Could not read replay '%s' back from disk; skipping upload.\n",
								uploadPath.c_str());
					return;
				}

				KZGlobalService::QueueReplayUpload(uploadUUID, std::move(buffer));

				if (KZOptionService::GetOptionInt("archiveRetentionMinutes", 2880) == 0)
				{
					utils::RemoveFile(uploadPath.c_str());
				}
			});
		// clang-format on
	}
}

// ---------------------------------------------------------------------------
// Local DB submission
// ---------------------------------------------------------------------------

void RunSubmission::SubmitLocal(const char *uuid)
{
	this->localSubmitted = true;

	// Styled runs use a fire-and-forget insert (save_time.cpp skips rank queries);
	// mark local false now so CheckAll() doesn't wait for a response that will never arrive.
	if (this->styleIDs != 0)
	{
		this->local = false;
	}

	auto onFailure = [uid = this->uid](std::string, int)
	{
		RunSubmission *sub = RunSubmission::Get(uid);
		if (!sub)
		{
			return;
		}
		sub->local = false;
	};

	auto onSuccess = [uid = this->uid](std::vector<ISQLQuery *> queries)
	{
		RunSubmission *sub = RunSubmission::Get(uid);
		if (!sub)
		{
			return;
		}
		sub->localResponse.received = true;

		ISQLResult *result = queries[1]->GetResultSet();
		sub->localResponse.overall.firstTime = result->GetRowCount() == 1;
		if (!sub->localResponse.overall.firstTime)
		{
			result->FetchRow();
			f32 pb = result->GetFloat(0);
			if (fabs(pb - sub->time) < EPSILON)
			{
				result->FetchRow();
				f32 oldPB = result->GetFloat(0);
				sub->localResponse.overall.pbDiff = sub->time - oldPB;
			}
			else
			{
				sub->localResponse.overall.pbDiff = sub->time - pb;
			}
		}

		result = queries[2]->GetResultSet();
		result->FetchRow();
		sub->localResponse.overall.rank = result->GetInt(0);

		result = queries[3]->GetResultSet();
		result->FetchRow();
		sub->localResponse.overall.maxRank = result->GetInt(0);

		if (sub->teleports == 0)
		{
			result = queries[4]->GetResultSet();
			sub->localResponse.pro.firstTime = result->GetRowCount() == 1;
			if (!sub->localResponse.pro.firstTime)
			{
				result->FetchRow();
				f32 pb = result->GetFloat(0);
				if (fabs(pb - sub->time) < EPSILON)
				{
					result->FetchRow();
					f32 oldPB = result->GetFloat(0);
					sub->localResponse.pro.pbDiff = sub->time - oldPB;
				}
				else
				{
					sub->localResponse.pro.pbDiff = sub->time - pb;
				}
			}

			result = queries[5]->GetResultSet();
			result->FetchRow();
			sub->localResponse.pro.rank = result->GetInt(0);

			result = queries[6]->GetResultSet();
			result->FetchRow();
			sub->localResponse.pro.maxRank = result->GetInt(0);
		}

		sub->UpdateLocalCache();
	};

	KZDatabaseService::SaveTime(uuid, this->player.steamid64, this->course.localID, this->mode.localID, this->time, this->teleports, this->styleIDs,
								this->metadata, onSuccess, onFailure);
}

// ---------------------------------------------------------------------------
// Cache updates
// ---------------------------------------------------------------------------

void RunSubmission::UpdateGlobalCache()
{
	KZGlobalService::UpdateRecordCache();
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
	if (player && this->globalResponse.received)
	{
		const KZCourseDescriptor *course = KZ::course::GetCourse(this->course.name.c_str());
		auto mode = KZ::mode::GetModeInfo(this->mode.name.c_str());
		if (mode.id > -2)
		{
			if (this->time < this->oldGPB.overall.time || this->oldGPB.overall.time == 0)
			{
				player->timerService->InsertPBToCache(this->time, course, mode.id, true, true, this->metadata.c_str(),
													  this->globalResponse.overall.points);
			}
			if (this->teleports == 0 && (this->time < this->oldGPB.pro.time || this->oldGPB.pro.time == 0))
			{
				player->timerService->InsertPBToCache(this->time, course, mode.id, false, true, this->metadata.c_str(),
													  this->globalResponse.pro.points);
			}
		}
	}
}

void RunSubmission::UpdateLocalCache()
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
	if (player)
	{
		player->timerService->UpdateLocalPBCache();
	}
	KZTimerService::UpdateLocalRecordCache();
}

void RunSubmission::OnMapChange()
{
	++RunSubmission::currentMapGeneration;
}

void RunSubmission::CheckAll()
{
	// clang-format off
	submissions.erase(std::remove_if(submissions.begin(), submissions.end(),
		[](RunSubmission *sub)
		{
			const f64 now = g_pKZUtils->GetServerGlobals()->realtime;

			// Try to finalize on each frame until it commits
			sub->TryFinalize();

			// Wait for all pending responses before announcing rank info, so local and
			// global messages arrive together in a single burst.
			bool localReady = !sub->local || sub->localResponse.received;
			bool globalReady =
				!sub->mapResolutionPending && (!sub->global || sub->globalResponse.received || sub->pendingQueuedSubmission);

			if (localReady && globalReady && !sub->runAnnounced)
			{
				sub->runAnnounced = true;
				sub->AnnounceRun();
				if (sub->local)
				{
					sub->AnnounceLocal();
				}
				if (sub->globalResponse.received)
				{
					sub->AnnounceGlobal();
				}
			}

			// Keep alive until the queued submission gets its API response. 
			// TODO: WS::OnCloseMessage() clears callbacks.queue without notifying anyone, so
			// a queued message destroyed before it flushes means the ack never arrives and this
			// submission parks forever. Should fix that in the next global rewrite.
			if (sub->pendingQueuedSubmission)
			{
				return false;
			}

			if (!sub->finalized)
			{
				return false;
			}

			// Wait for local and global responses (with regular timeout)
			bool waitingForLocal = sub->local && !sub->localResponse.received;
			bool waitingForGlobal = sub->global && !sub->globalResponse.received;
			const bool announcementTimeoutReached = now >= (sub->timestamp + RunSubmission::announcementTimeout);

			if (waitingForLocal || waitingForGlobal)
			{
				if (!announcementTimeoutReached)
				{
					return false;
				}

				if (!sub->runAnnounced)
				{
					sub->runAnnounced = true;
					sub->AnnounceRun();
					if (sub->localResponse.received)
					{
						sub->AnnounceLocal();
					}
					if (sub->globalResponse.received)
					{
						sub->AnnounceGlobal();
					}
				}
			}

			// Keep alive until the replay breather ends and OnReplayReady fires,
			// so the disk write and player notification are not lost.
			if (!sub->replayReady)
			{
				return false;
			}

			delete sub;
			return true;
		}),
		submissions.end());
	// clang-format on
}

// ---------------------------------------------------------------------------
// Global API callback
// ---------------------------------------------------------------------------

void RunSubmission::OnGlobalRecordSubmitted(const KZ::api::messages::NewRecordAck &ack, u32 uid)
{
	KZ_LOG_INFO(LogChannel::Global, "[%u] Record submitted under ID %s\n", uid, ack.recordId.c_str());

	RunSubmission *sub = RunSubmission::Get(uid);
	if (!sub)
	{
		return;
	}

	sub->OnAPIResponse(ack);

	// Global cache update — skip for styled runs
	if (sub->styles.empty())
	{
		sub->UpdateGlobalCache();
	}
}

// ---------------------------------------------------------------------------
// Announce methods
// ---------------------------------------------------------------------------

void RunSubmission::AnnounceRun()
{
	if (IsFromPreviousMap())
	{
		return;
	}

	char formattedTime[32];
	utils::FormatTime(time, formattedTime, sizeof(formattedTime));

	CUtlString combinedModeStyleText;
	combinedModeStyleText.Format("{purple}%s{grey}", this->mode.name.c_str());
	for (auto &style : this->styles)
	{
		combinedModeStyleText += " +{grey2}";
		combinedModeStyleText.Append(style.name.c_str());
		combinedModeStyleText += "{grey}";
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player->IsInGame())
		{
			continue;
		}
		std::string teleportText = "{blue}PRO{grey}";
		if (this->teleports > 0)
		{
			teleportText = this->teleports == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
												: player->languageService->PrepareMessage("2+ Teleports Text", this->teleports);
		}
		player->languageService->PrintChat(true, false, "Beat Course Info - Basic", this->player.name.c_str(), this->map.name.c_str(),
										   this->course.name.c_str(), formattedTime, combinedModeStyleText.Get(), teleportText.c_str());
	}
}

void RunSubmission::AnnounceLocal()
{
	if (IsFromPreviousMap())
	{
		return;
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player->IsInGame())
		{
			continue;
		}

		char formattedDiffTime[32];
		KZTimerService::FormatDiffTime(this->localResponse.overall.pbDiff, formattedDiffTime, sizeof(formattedDiffTime));

		char formattedDiffTimePro[32];
		KZTimerService::FormatDiffTime(this->localResponse.pro.pbDiff, formattedDiffTimePro, sizeof(formattedDiffTimePro));

		// clang-format off
		std::string diffText = this->localResponse.overall.firstTime
			? ""
			: player->languageService->PrepareMessage("Personal Best Difference",
				this->localResponse.overall.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTime);
		std::string diffTextPro = this->localResponse.pro.firstTime
			? ""
			: player->languageService->PrepareMessage("Personal Best Difference",
				this->localResponse.pro.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTimePro);
		// clang-format on

		if (this->teleports > 0)
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - Local (TP)", this->localResponse.overall.rank,
											   this->localResponse.overall.maxRank, diffText.c_str());
		}
		else
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - Local (PRO)", this->localResponse.overall.rank,
											   this->localResponse.overall.maxRank, diffText.c_str(), this->localResponse.pro.rank,
											   this->localResponse.pro.maxRank, diffTextPro.c_str());
		}
	}
}

void RunSubmission::AnnounceGlobal()
{
	if (IsFromPreviousMap())
	{
		return;
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player->IsInGame())
		{
			continue;
		}

		bool hasOldPB = this->oldGPB.overall.time != 0;
		bool hasOldPBPro = this->oldGPB.pro.time != 0;

		f64 nubPbDiff = this->time - this->oldGPB.overall.time;
		f64 nubPointsDiff = MAX(this->globalResponse.overall.points - this->oldGPB.overall.points, 0.0);

		char formattedDiffTime[32];
		KZTimerService::FormatDiffTime(nubPbDiff, formattedDiffTime, sizeof(formattedDiffTime));

		// clang-format off
		std::string diffText = hasOldPB
			? player->languageService->PrepareMessage("Personal Best Difference",
				nubPbDiff < 0 ? "{green}" : "{red}", formattedDiffTime)
			: "";
		// clang-format on

		bool beatWR =
			hasOldPB ? (this->time < this->oldGPB.overall.time && this->globalResponse.overall.rank == 1) : (this->globalResponse.overall.rank == 1);
		bool beatWRPro =
			hasOldPBPro ? (this->time < this->oldGPB.pro.time && this->globalResponse.pro.rank == 1) : (this->globalResponse.pro.rank == 1);

		if (this->teleports > 0)
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - Global (TP)", this->globalResponse.overall.rank,
											   this->globalResponse.overall.maxRank, diffText.c_str());
			player->languageService->PrintChat(true, false, "Beat Course Info - Global Points (TP)", this->globalResponse.overall.points,
											   nubPointsDiff);
		}
		else if (this->globalResponse.pro.rank != 0)
		{
			f64 proPbDiff = this->time - this->oldGPB.pro.time;
			f64 proPointsDiff = MAX(this->globalResponse.pro.points - this->oldGPB.pro.points, 0.0);

			char formattedDiffTimePro[32];
			KZTimerService::FormatDiffTime(proPbDiff, formattedDiffTimePro, sizeof(formattedDiffTimePro));

			// clang-format off
			std::string diffTextPro = hasOldPBPro
				? player->languageService->PrepareMessage("Personal Best Difference",
					proPbDiff < 0 ? "{green}" : "{red}", formattedDiffTimePro)
				: "";
			// clang-format on

			player->languageService->PrintChat(true, false, "Beat Course Info - Global (PRO)", this->globalResponse.overall.rank,
											   this->globalResponse.overall.maxRank, diffText.c_str(), this->globalResponse.pro.rank,
											   this->globalResponse.pro.maxRank, diffTextPro.c_str());
			player->languageService->PrintChat(true, false, "Beat Course Info - Global Points (PRO)", this->globalResponse.overall.points,
											   nubPointsDiff, this->globalResponse.pro.points, proPointsDiff);
		}

		if (beatWR)
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - New World Record", this->player.name.c_str(),
											   this->mode.name.c_str());
		}
		if (beatWRPro)
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - New World Record (PRO)", this->player.name.c_str(),
											   this->mode.name.c_str());
		}
		if (beatWR || beatWRPro)
		{
			for (i32 j = 0; j < MAXPLAYERS + 1; j++)
			{
				KZPlayer *p = g_pKZPlayerManager->ToPlayer(j);
				if (!p->IsInGame())
				{
					continue;
				}
				utils::PlaySoundToClient(p->GetPlayerSlot(), "kz.holyshit", p->optionService->GetPreferenceFloat("recordVolume", 1.0f));
			}
		}
	}
}
