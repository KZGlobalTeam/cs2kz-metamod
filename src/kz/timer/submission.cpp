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
	: uid(RunSubmission::idCount++), timestamp(g_pKZUtils->GetServerGlobals()->realtime), userID(player->GetClient()->GetUserID()),
	  localUUID(player->recordingService->GetCurrentRunUUID()), finalUUID(player->recordingService->GetCurrentRunUUID()),
	  time(player->timerService->GetTime()), teleports(player->checkpointService->GetTeleportCount())
{
	this->local = KZDatabaseService::IsReady() && KZDatabaseService::IsMapSetUp();
	this->global = player->hasPrime && KZGlobalService::MayBecomeAvailable();

	if (kz_debug_announce_global.Get() && !this->global)
	{
		if (!player->hasPrime)
		{
			META_CONPRINTF("[KZ::Global - %u] Player %s does not have Prime, will not submit globally.\n", uid, player->GetName());
		}
		if (!KZGlobalService::IsAvailable())
		{
			META_CONPRINTF("[KZ::Global - %u] Global service is not available, will not submit globally.\n", uid);
		}
	}

	// Setup player snapshot
	this->player.name = player->GetName();
	this->player.steamid64 = player->GetSteamId64();

	// Setup mode
	auto mode = KZ::mode::GetModeInfo(player->modeService);
	this->mode.name = mode.shortModeName;
	KZ::api::Mode apiMode;
	this->global = KZ::api::DecodeModeString(this->mode.name, apiMode);
	if (!this->global)
	{
		if (kz_debug_announce_global.Get())
		{
			META_CONPRINTF("[KZ::Global - %u] Mode '%s' is not a valid global mode, will not submit globally.\n", uid, this->mode.name.c_str());
		}
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

	// clang-format off
	KZGlobalService::WithCurrentMap([&](const std::optional<KZ::api::Map> &currentMap)
	{
		this->global = currentMap.has_value();

		if (!currentMap)
		{
			return;
		}

		const KZ::api::Map::Course *course = nullptr;
		for (const KZ::api::Map::Course &c : currentMap->courses)
		{
			if (KZ_STREQ(c.name.c_str(), this->course.name.c_str()))
			{
				course = &c;
				break;
			}
		}

		if (course == nullptr)
		{
			if (kz_debug_announce_global.Get())
			{
				META_CONPRINTF("[KZ::Global - %u] Course '%s' not found on global map '%s', will not submit globally.\n", uid,
							   this->course.name.c_str(), currentMap->name.c_str());
				META_CONPRINTF("[KZ::Global - %u] Available courses:\n", uid);
				for (const KZ::api::Map::Course &c : currentMap->courses)
				{
					META_CONPRINTF(" - %s\n", c.name.c_str());
				}
			}
			global = false;
		}
		else
		{
			this->globalFilterID = (apiMode == KZ::api::Mode::Classic)
				? course->filters.classic.id
				: course->filters.vanilla.id;
		}
	});
	// clang-format on

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
	}

	// Snapshot previous global PBs for diff display
	if (global)
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
	if (global)
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
		META_CONPRINTF("[KZ::Global - %u] Global record submission result: %d\n", uid, static_cast<int>(submissionResult));
	}

	switch (submissionResult)
	{
		case KZGlobalService::SubmitRecordResult::PlayerNotAuthenticated:
		case KZGlobalService::SubmitRecordResult::MapNotGlobal:
		{
			this->global = false;
			// No API UUID will arrive; insert locally now with localUUID.
			if (this->local)
			{
				SubmitLocal(localUUID.ToString().c_str());
			}
			break;
		}
		case KZGlobalService::SubmitRecordResult::Queued:
		{
			// The message will be sent once the API reconnects.
			// Do NOT insert locally yet — we must wait for the API's UUID.
			// CheckAll() will keep this submission alive; Clear() on map change cleans it up.
			this->pendingQueuedSubmission = true;
			break;
		}
		case KZGlobalService::SubmitRecordResult::NotConnected:
		{
			this->global = false;
			if (this->local)
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
		KZPlayer *callbackPlayer = g_pKZPlayerManager->ToPlayer(userID);
		if (callbackPlayer)
		{
			callbackPlayer->languageService->PrintChat(true, false, "Replay - Run Replay Saved", finalUUID.ToString().c_str());
			callbackPlayer->languageService->PrintConsole(false, false, "Replay - Run Replay Saved (Console)", finalUUID.ToString().c_str());
		}
	}

	TryFinalize();
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

	// No submissions — nothing to finalize; just mark done so CheckAll() can announce and GC.
	if (!local && !global)
	{
		finalized = true;
		return;
	}

	if (!replayReady && global)
	{
		return;
	}

	const f64 now = g_pKZUtils->GetServerGlobals()->realtime;
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

	// 1. Local DB insert with the final UUID
	if (local)
	{
		SubmitLocal(finalUUID.ToString().c_str());
	}

	// 2. Queue replay upload to the global API
	if (global && apiResponseReceived && g_asyncFileIO)
	{
		// std::move is safe here; the copy into QueueWriteBuffer in OnReplayReady left replayBuffer intact.
		KZGlobalService::QueueReplayUpload(finalUUID, std::move(replayBuffer));
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

	// Queue replay upload
	if (!replayBuffer.empty())
	{
		// Buffer still in RAM — upload directly
		KZGlobalService::QueueReplayUpload(apiFinalUUID, std::move(replayBuffer));
	}
	else
	{
		// Buffer was already moved into the upload queue (or never set);
		// load from disk on the bg thread and then upload.
		// clang-format off
		g_asyncFileIO->QueueRead(newPath,
			[apiFinalUUID](bool success, std::vector<char> &&buf)
			{
				if (success)
				{
					KZGlobalService::QueueReplayUpload(apiFinalUUID, std::move(buf));
				}
				else
				{
					META_CONPRINTF("[KZ] DoLateAPIResponse: failed to read replay file for upload: %s\n", apiFinalUUID.ToString().c_str());
				}
			});
		// clang-format on
	}

	// Keep finalUUID consistent with the authoritative API-assigned UUID
	finalUUID = apiFinalUUID;
}

// ---------------------------------------------------------------------------
// Local DB submission
// ---------------------------------------------------------------------------

void RunSubmission::SubmitLocal(const char *uuid)
{
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
			if (this->time < this->oldGPB.pro.time || this->oldGPB.pro.time == 0)
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

// ---------------------------------------------------------------------------
// CheckAll — per-frame tick: drive announcements and garbage-collect
// ---------------------------------------------------------------------------

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
			bool globalReady = !sub->global || sub->globalResponse.received;

			if (sub->finalized && localReady && globalReady && !sub->runAnnounced)
			{
				sub->runAnnounced = true;
				sub->AnnounceRun();
				if (sub->local) sub->AnnounceLocal();
				if (sub->global) sub->AnnounceGlobal();
			}

			// Keep alive until the queued submission eventually gets an API response.
			// These entries are cleaned up by Clear() on map change.
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
				// Announcement timeout — announce what we have and GC
				if (!sub->runAnnounced)
				{
					sub->runAnnounced = true;
					sub->AnnounceRun();
					if (sub->local && sub->localResponse.received)
					{
						sub->AnnounceLocal();
					}
				}
				if (sub->global && sub->globalResponse.received)
				{
					sub->AnnounceGlobal();
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
	META_CONPRINTF("[KZ::Global - %u] Record submitted under ID %s\n", uid, ack.recordId.c_str());

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
		player->languageService->PrintChat(true, false, "Beat Course Info - Basic", this->player.name.c_str(), this->course.name.c_str(),
										   formattedTime, combinedModeStyleText.Get(), teleportText.c_str());
	}
}

void RunSubmission::AnnounceLocal()
{
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
				utils::PlaySoundToClient(p->GetPlayerSlot(), "kz.holyshit", p->optionService->GetPreferenceFloat("recordVolume", 1.0f));
			}
		}
	}
}
