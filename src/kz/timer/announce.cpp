#include "announce.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/global/events.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/style/kz_style.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

CConVar<bool> kz_debug_announce_global("kz_debug_announce_global", FCVAR_NONE, "Print debug info for record announcements.", false);

RecordAnnounce::RecordAnnounce(KZPlayer *player)
	: uid(RecordAnnounce::idCount++), timestamp(g_pKZUtils->GetServerGlobals()->realtime), userID(player->GetClient()->GetUserID()),
	  time(player->timerService->GetTime()), teleports(player->checkpointService->GetTeleportCount())
{
	this->local = KZDatabaseService::IsReady() && KZDatabaseService::IsMapSetUp();
	this->global = player->hasPrime && KZGlobalService::IsAvailable();
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
	// Setup player
	this->player.name = player->GetName();
	this->player.steamid64 = player->GetSteamId64();

	// Setup mode
	auto mode = KZ::mode::GetModeInfo(player->modeService);
	this->mode.name = mode.shortModeName;
	KZ::API::Mode apiMode;
	this->global = KZ::API::DecodeModeString(this->mode.name, apiMode);
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

	KZGlobalService::WithCurrentMap([&](const KZ::API::Map *currentMap)
	{
		this->global = currentMap != nullptr;

		if (currentMap == nullptr)
		{
			return;
		}

		const KZ::API::Map::Course *course = nullptr;

		for (const KZ::API::Map::Course &c : currentMap->courses)
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
				META_CONPRINTF("[KZ::Global - %u] Course '%s' not found on global map '%s', will not submit globally.\n", uid, this->course.name.c_str(),
							   currentMap->name.c_str());
				META_CONPRINTF("[KZ::Global - %u] Available courses:\n", uid);
				for (const KZ::API::Map::Course &c : currentMap->courses)
				{
					META_CONPRINTF(" - %s\n", c.name.c_str());
				}
			}
			global = false;
		}
		else
		{
			this->globalFilterID = (apiMode == KZ::API::Mode::Classic)
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

	// Previous GPBs
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

	if (local)
	{
		this->SubmitLocal();
	}
	if (global)
	{
		this->SubmitGlobal();
	}
}

void RecordAnnounce::SubmitGlobal()
{
	auto callback = [uid = this->uid](KZ::API::events::NewRecordAck &ack)
	{
		META_CONPRINTF("[KZ::Global - %u] Record submitted under ID %d\n", uid, ack.recordId);

		RecordAnnounce *rec = RecordAnnounce::Get(uid);
		if (!rec)
		{
			return;
		}
		rec->globalResponse.received = true;
		rec->globalResponse.recordId = ack.recordId;
		// TODO: Remove this 0.1 when API sends the correct rating
		rec->globalResponse.playerRating = ack.playerRating * 0.1f;
		rec->globalResponse.overall.rank = ack.overallData.rank;
		rec->globalResponse.overall.points = ack.overallData.points;
		rec->globalResponse.overall.maxRank = ack.overallData.leaderboardSize;
		rec->globalResponse.pro.rank = ack.proData.rank;
		rec->globalResponse.pro.points = ack.proData.points;
		rec->globalResponse.pro.maxRank = ack.proData.leaderboardSize;

		// cache should not be overwritten by styled runs
		if (rec->styles.empty())
		{
			rec->UpdateGlobalCache();
		}
	};

	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);

	// Dirty hack since nested forward declaration isn't possible.
	KZGlobalService::SubmitRecordResult submissionResult = player->globalService->SubmitRecord(
		this->globalFilterID, this->time, this->teleports, this->mode.md5, (void *)(&this->styles), this->metadata.c_str(), callback);

	if (kz_debug_announce_global.Get())
	{
		META_CONPRINTF("[KZ::Global - %u] Global record submission result: %d\n", uid, static_cast<int>(submissionResult));
	}
	switch (submissionResult)
	{
		case KZGlobalService::SubmitRecordResult::PlayerNotAuthenticated: /* fallthrough */
		case KZGlobalService::SubmitRecordResult::MapNotGlobal:           /* fallthrough */
		case KZGlobalService::SubmitRecordResult::Queued:
		{
			this->global = false;
			break;
		};
		case KZGlobalService::SubmitRecordResult::Submitted:
		{
			this->global = true;
			break;
		};
	}
}

void RecordAnnounce::UpdateGlobalCache()
{
	KZGlobalService::UpdateRecordCache();
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
	if (player && this->globalResponse.received)
	{
		const KZCourseDescriptor *course = KZ::course::GetCourse(this->course.name.c_str());
		auto mode = KZ::mode::GetModeInfo(this->mode.name.c_str());
		if (mode.id > -2)
		{
			META_CONPRINTF("this->time=%f this->oldGPB.overall.time=%f, this->oldGPB.pro.time=%f", this->time, this->oldGPB.overall.time,
						   this->oldGPB.pro.time);

			if (this->time < this->oldGPB.overall.time)
			{
				player->timerService->InsertPBToCache(this->time, course, mode.id, true, true, this->metadata.c_str(),
													  this->globalResponse.overall.points);
			}
			if (this->time < this->oldGPB.pro.time)
			{
				player->timerService->InsertPBToCache(this->time, course, mode.id, false, true, this->metadata.c_str(),
													  this->globalResponse.pro.points);
			}
		}
	}
}

void RecordAnnounce::SubmitLocal()
{
	auto onFailure = [uid = this->uid](std::string, int)
	{
		RecordAnnounce *rec = RecordAnnounce::Get(uid);
		if (!rec)
		{
			return;
		}
		rec->local = false;
	};
	auto onSuccess = [uid = this->uid](std::vector<ISQLQuery *> queries)
	{
		RecordAnnounce *rec = RecordAnnounce::Get(uid);
		if (!rec)
		{
			return;
		}
		rec->localResponse.received = true;

		ISQLResult *result = queries[1]->GetResultSet();
		rec->localResponse.overall.firstTime = result->GetRowCount() == 1;
		if (!rec->localResponse.overall.firstTime)
		{
			result->FetchRow();
			f32 pb = result->GetFloat(0);
			// Close enough. New time is new PB.
			if (fabs(pb - rec->time) < EPSILON)
			{
				result->FetchRow();
				f32 oldPB = result->GetFloat(0);
				rec->localResponse.overall.pbDiff = rec->time - oldPB;
			}
			else // Didn't beat PB
			{
				rec->localResponse.overall.pbDiff = rec->time - pb;
			}
		}
		// Get NUB Rank
		result = queries[2]->GetResultSet();
		result->FetchRow();
		rec->localResponse.overall.rank = result->GetInt(0);
		result = queries[3]->GetResultSet();
		result->FetchRow();
		rec->localResponse.overall.maxRank = result->GetInt(0);

		if (rec->teleports == 0)
		{
			ISQLResult *result = queries[4]->GetResultSet();
			rec->localResponse.pro.firstTime = result->GetRowCount() == 1;
			if (!rec->localResponse.pro.firstTime)
			{
				result->FetchRow();
				f32 pb = result->GetFloat(0);
				// Close enough. New time is new PB.
				if (fabs(pb - rec->time) < EPSILON)
				{
					result->FetchRow();
					f32 oldPB = result->GetFloat(0);
					rec->localResponse.pro.pbDiff = rec->time - oldPB;
				}
				else // Didn't beat PB
				{
					rec->localResponse.pro.pbDiff = rec->time - pb;
				}
			}
			// Get PRO rank
			result = queries[5]->GetResultSet();
			result->FetchRow();
			rec->localResponse.pro.rank = result->GetInt(0);
			result = queries[6]->GetResultSet();
			result->FetchRow();
			rec->localResponse.pro.maxRank = result->GetInt(0);
		}
		rec->UpdateLocalCache();
	};
	KZDatabaseService::SaveTime(this->player.steamid64, this->course.localID, this->mode.localID, this->time, this->teleports, this->styleIDs,
								this->metadata, onSuccess, onFailure);
}

void RecordAnnounce::UpdateLocalCache()
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
	if (player)
	{
		player->timerService->UpdateLocalPBCache();
	}
	KZTimerService::UpdateLocalRecordCache();
}

void RecordAnnounce::AnnounceRun()
{
	char formattedTime[32];
	utils::FormatTime(time, formattedTime, sizeof(formattedTime));

	CUtlString combinedModeStyleText;
	combinedModeStyleText.Format("{purple}%s{grey}", this->mode.name.c_str());
	for (auto style : this->styles)
	{
		combinedModeStyleText += " +{grey2}";
		combinedModeStyleText.Append(style.name.c_str());
		combinedModeStyleText += "{grey}";
	}

	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		// Print basic information
		// KZ | GameChaos finished "blocks2006" in 10:06.84 [VNL | PRO]
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

void RecordAnnounce::AnnounceLocal()
{
	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);

		if (!player->IsInGame())
		{
			continue;
		}
		// Print server ranking information if available.

		char formattedDiffTime[32];
		KZTimerService::FormatDiffTime(this->localResponse.overall.pbDiff, formattedDiffTime, sizeof(formattedDiffTime));

		char formattedDiffTimePro[32];
		KZTimerService::FormatDiffTime(this->localResponse.pro.pbDiff, formattedDiffTimePro, sizeof(formattedDiffTimePro));

		// clang-format off
        std::string diffText = this->localResponse.overall.firstTime ? 
            "" : player->languageService->PrepareMessage("Personal Best Difference", this->localResponse.overall.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTime);
        std::string diffTextPro = this->localResponse.pro.firstTime ? 
            "" : player->languageService->PrepareMessage("Personal Best Difference", this->localResponse.pro.pbDiff < 0 ? "{green}" : "{red}", formattedDiffTimePro);

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

void RecordAnnounce::AnnounceGlobal()
{
	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);

		if (!player->IsInGame())
		{
			continue;
		}
		bool hasOldPB = this->oldGPB.overall.time;
		bool hasOldPBPro = this->oldGPB.pro.time;

		f64 nubPbDiff = this->time - this->oldGPB.overall.time;
		// Players can't lose points for being slower than PB.
		f64 nubPointsDiff = MAX(this->globalResponse.overall.points - this->oldGPB.overall.points, 0.0f);

		char formattedDiffTime[32];
		KZTimerService::FormatDiffTime(nubPbDiff, formattedDiffTime, sizeof(formattedDiffTime));

		// clang-format off
        std::string diffText = hasOldPB
            ? player->languageService->PrepareMessage("Personal Best Difference", nubPbDiff < 0 ? "{green}" : "{red}", formattedDiffTime)
            : "";
		// clang-format on
		bool beatWR = false;
		bool beatWRPro = false;
		if (hasOldPB)
		{
			beatWR = this->time < this->oldGPB.overall.time && this->globalResponse.overall.rank == 1;
		}
		else if (this->globalResponse.overall.rank == 1)
		{
			beatWR = true;
		}
		if (hasOldPBPro)
		{
			beatWRPro = this->time < this->oldGPB.pro.time && this->globalResponse.pro.rank == 1;
		}
		else if (this->globalResponse.pro.rank == 1)
		{
			beatWRPro = true;
		}
		if (this->teleports)
		{
			player->languageService->PrintChat(true, false, "Beat Course Info - Global (TP)", this->globalResponse.overall.rank,
											   this->globalResponse.overall.maxRank, diffText.c_str());

			player->languageService->PrintChat(true, false, "Beat Course Info - Global Points (TP)", this->globalResponse.overall.points,
											   nubPointsDiff, this->globalResponse.playerRating);
		}

		if (this->globalResponse.pro.rank != 0)
		{
			f64 proPbDiff = this->time - this->oldGPB.pro.time;
			f64 proPointsDiff = MAX(this->globalResponse.pro.points - this->oldGPB.pro.points, 0.0f);

			char formattedDiffTimePro[32];
			KZTimerService::FormatDiffTime(proPbDiff, formattedDiffTimePro, sizeof(formattedDiffTimePro));

			// clang-format off
            std::string diffTextPro = hasOldPBPro
                ? player->languageService->PrepareMessage("Personal Best Difference", proPbDiff < 0 ? "{green}" : "{red}", formattedDiffTimePro)
                : "";
			// clang-format on
			player->languageService->PrintChat(true, false, "Beat Course Info - Global (PRO)", this->globalResponse.overall.rank,
											   this->globalResponse.overall.maxRank, diffText.c_str(), this->globalResponse.pro.rank,
											   this->globalResponse.pro.maxRank, diffTextPro.c_str());

			player->languageService->PrintChat(true, false, "Beat Course Info - Global Points (PRO)", this->globalResponse.overall.points,
											   nubPointsDiff, this->globalResponse.pro.points, proPointsDiff, this->globalResponse.playerRating);
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
			for (i32 i = 0; i < MAXPLAYERS + 1; i++)
			{
				utils::PlaySoundToClient(CPlayerSlot(i), "kz.holyshit");
			}
		}
	}
}
