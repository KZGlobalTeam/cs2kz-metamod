/*
 * RunSubmission — unified run submission and announcement state machine.
 *
 * This class replaces RecordAnnounce. It owns the full lifecycle of a finished run:
 *   1. Global API submission (NewRecord → NewRecordAck)
 *   2. Waiting for the replay breather to finish (OnReplayReady)
 *   3. Writing the replay to disk and queueing it for API upload
 *   4. Local database insertion
 *   5. Announcing rank/points results to players
 *
 * All methods must be called from the main thread.
 */
#pragma once

#include "kz/timer/kz_timer.h"
#include "kz/global/messages.h"
#include "utils/uuid.h"

struct RunSubmission
{
	RunSubmission(KZPlayer *player);

	// -------------------------------------------------------------------------
	// Identity
	// -------------------------------------------------------------------------

	const u32 uid;       // unique ID for callbacks to look up this submission
	const f64 timestamp; // realtime at creation, used for timeouts

	const CPlayerUserId userID;

	// -------------------------------------------------------------------------
	// Submission state
	// -------------------------------------------------------------------------

	// UUID generated at timer end. Used immediately for local-only submissions.
	const UUID_t localUUID;

	// UUID that goes into the DB row and replay filename.
	// Equal to localUUID unless the API responded with its own recordId before finalization.
	UUID_t finalUUID;

	bool global {}; // will an API submission be attempted?
	bool local {};  // will a local DB insert be attempted?

	// Set to true when SubmitRecord returns Queued so CheckAll() keeps this submission
	// alive until the API reconnects and delivers the ack. Cleared on map change via Clear().
	bool pendingQueuedSubmission {};

	bool replayReady {};         // set when the file writer delivers the buffer
	bool apiResponseReceived {}; // set when NewRecordAck arrives
	bool finalized {};           // set once TryFinalize() actually commits
	bool runAnnounced {};        // AnnounceRun()/AnnounceLocal()/AnnounceGlobal() have been called

	// Replay buffer held in RAM from OnReplayReady() until QueueUpload() moves it away.
	// After the move it is empty; DoLateAPIResponse() will fall back to a disk read.
	std::vector<char> replayBuffer;

	// -------------------------------------------------------------------------
	// Player / run data (snapshots taken at construction from the live KZPlayer)
	// -------------------------------------------------------------------------

	struct
	{
		std::string name {};
		u64 steamid64 {};
	} player;

	const f64 time;
	const u32 teleports;

	struct
	{
		std::string name {};
		std::string md5 {};
		u32 localID {};
	} mode;

	struct
	{
		std::string name {};
		std::string md5 {};
	} map;

	struct
	{
		std::string name {};
		u32 localID {};
	} course;

	u16 globalFilterID {};

	struct StyleInfo
	{
		std::string name {};
		std::string md5 {};
	};

	std::vector<StyleInfo> styles;
	u64 styleIDs {};

	std::string metadata;

	// Previous global PBs captured at construction for diff display
	struct
	{
		struct
		{
			f64 time {};
			f64 points {};
		} overall, pro;
	} oldGPB;

	// -------------------------------------------------------------------------
	// Global API response
	// -------------------------------------------------------------------------

	struct
	{
		bool received {};
		std::string recordId {};

		struct RunData
		{
			u32 rank {};
			f64 points = -1;
			u64 maxRank {};
		};

		RunData overall {};
		RunData pro {};
	} globalResponse;

	// -------------------------------------------------------------------------
	// Local DB response
	// -------------------------------------------------------------------------

	struct
	{
		bool received {};

		struct
		{
			bool firstTime {};
			f32 pbDiff {};
			u32 rank {};
			u32 maxRank {};
		} overall, pro;
	} localResponse;

	// -------------------------------------------------------------------------
	// Lifecycle callbacks
	// -------------------------------------------------------------------------

	// Called when the file writer finishes serializing the replay. Main thread only.
	void OnReplayReady(std::vector<char> &&buffer);

	// Called when a NewRecordAck arrives from the global API. Main thread only.
	void OnAPIResponse(const KZ::api::messages::NewRecordAck &ack);

	// -------------------------------------------------------------------------
	// Static management
	// -------------------------------------------------------------------------

	static RunSubmission *Create(KZPlayer *player)
	{
		RunSubmission *sub = new RunSubmission(player);
		submissions.push_back(sub);
		return sub;
	}

	// Look up by uid (used in API/DB callbacks)
	static RunSubmission *Get(u32 uid)
	{
		auto it = std::find_if(submissions.begin(), submissions.end(), [uid](RunSubmission *s) { return s->uid == uid; });
		return (it != submissions.end()) ? *it : nullptr;
	}

	// Look up by the local UUID (used in file writer callback)
	static RunSubmission *GetByUUID(const UUID_t &uuid)
	{
		auto it = std::find_if(submissions.begin(), submissions.end(), [&uuid](RunSubmission *s) { return s->localUUID == uuid; });
		return (it != submissions.end()) ? *it : nullptr;
	}

	// Called once per frame from a timer tick to drive announcements and GC
	static void CheckAll();

	static void Clear()
	{
		for (auto *s : submissions)
		{
			delete s;
		}
		submissions.clear();
	}

private:
	// Core decision point — called after any state change.
	// Commits the run (disk write, DB insert, upload, announce) if all preconditions are met.
	void TryFinalize();

	// Called when the API responds AFTER finalization has already occurred with localUUID.
	// Renames the replay file, updates the DB row, and queues the upload.
	void DoLateAPIResponse(const std::string &apiUUID);

	// Submit to the global API; sets pendingQueuedSubmission if queued offline.
	void SubmitGlobal();

	// Insert the run into the local database using the provided UUID.
	void SubmitLocal(const char *uuid);

	// Update caches after receiving API / DB responses.
	void UpdateGlobalCache();
	void UpdateLocalCache();

	// Announce to all players.
	void AnnounceRun();
	void AnnounceLocal();
	void AnnounceGlobal();

	static void OnGlobalRecordSubmitted(const KZ::api::messages::NewRecordAck &ack, u32 uid);

	static inline std::vector<RunSubmission *> submissions;
	static inline u32 idCount = 0;
	// How long to wait for the global API to respond before finalizing with localUUID.
	static inline constexpr f64 timeout = 5.0f;
	// How long to wait before force-announcing with whatever responses have arrived.
	// Longer than timeout so the local DB still has time to respond after a global API timeout.
	static inline constexpr f64 announcementTimeout = 10.0f;
};
