/*
 * Run submission stuff.
 */

#pragma once

#include "kz/timer/kz_timer.h"
#include "kz/global/api.h"

struct RecordAnnounce
{
	RecordAnnounce(KZPlayer *player);

	const u32 uid;
	const f64 timestamp;

	const CPlayerUserId userID;

	struct
	{
		std::string name {};
		u64 steamid64 {};
	} player;

	const f64 time;

	std::string runUUID;

	// We need to store the previous global PBs because the player might be gone before the announcement is made.
	struct
	{
		struct
		{
			f64 time {};
			f64 points {};
		} overall, pro;
	} oldGPB;

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

	bool global {};

private:
	static inline std::vector<RecordAnnounce *> records;
	static inline u64 idCount = 0;
	static inline f64 timeout = 5.0f;

public:
	static RecordAnnounce *Create(KZPlayer *player)
	{
		RecordAnnounce *rec = new RecordAnnounce(player);
		records.push_back(rec);
		return rec;
	}

	static RecordAnnounce *Get(u64 uid)
	{
		auto it = std::find_if(records.begin(), records.end(), [uid](RecordAnnounce *req) { return req->uid == uid; });
		return (it != records.end()) ? *it : nullptr;
	}

	static void Check()
	{
		// clang-format off
		records.erase(std::remove_if(records.begin(), records.end(),
			[](RecordAnnounce *rec)
			{
				if (!rec->styles.empty())
				{
					rec->Announce();
					delete rec;
					return true;
				}

				bool waitingForLocal = rec->local && !rec->localResponse.received;
				bool waitingForGlobal = rec->global && !rec->globalResponse.received;
				bool timeoutReached = g_pKZUtils->GetServerGlobals()->realtime >= (rec->timestamp + RecordAnnounce::timeout);

                if ((waitingForLocal || waitingForGlobal) && !timeoutReached)
				{
					return false;
				}

				rec->Announce();
				delete rec;
				return true;
			}),
			records.end());
		// clang-format on
	}

	static void Clear()
	{
		for (auto req : records)
		{
			delete req;
		}
		records.clear();
	}

	// Submit the run globally, update the global cache if needed.
	void SubmitGlobal();
	void UpdateGlobalCache();

	struct
	{
		bool received {};

		struct RunData
		{
			u32 rank {};
			f64 points = -1;
			u64 maxRank {};
		};

		u32 recordId {};
		f64 playerRating {};
		RunData overall {};
		RunData pro {};
	} globalResponse;

	bool local;

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

	// Submit the run locally, update the cache if needed.
	void SubmitLocal();
	void UpdateLocalCache();

	// GameChaos finished "blocks2006" in 10:06.84 | VNL | PRO
	// Server: #1/24 Overall (-1:00.00) | #1/10 PRO (-2:00.00)
	// Global: #1/24 Overall (-1:00.00) | #1/10 PRO (-2:00.00)
	// Map Points: 2345 (+512) | Player Rating: 34475
	void Announce()
	{
		AnnounceRun();
		if (!this->styles.empty())
		{
			return;
		}
		if (local && localResponse.received)
		{
			AnnounceLocal();
		}
		if (global && globalResponse.received)
		{
			AnnounceGlobal();
		}
	}

	// Print to all players that someone has finished a run.
	void AnnounceRun();
	// Print the run's server rankings and PB difference (if available).
	void AnnounceLocal();
	// Print the run's global rankings, and PB difference, course points and total rating (if available).
	void AnnounceGlobal();
};
