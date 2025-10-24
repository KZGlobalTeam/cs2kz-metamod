#include "base_request.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/global/events.h"

#include "utils/simplecmds.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

struct TopRecordRequest : public BaseRequest
{
	using BaseRequest::BaseRequest;
	static constexpr u64 trFeatures = RequestFeature::Course | RequestFeature::Map | RequestFeature::Mode;

	struct RecordData
	{
		bool hasRecord {};
		CUtlString holder;
		f32 runTime {};
		u32 teleportsUsed {};

		bool hasRecordPro {};
		CUtlString holderPro;
		f32 runTimePro {};
	} srData, wrData;

	virtual void PrintInstructions() override
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		player->languageService->PrintChat(true, false, "WR/SR Command Usage");
		player->languageService->PrintConsole(false, false, "WR/SR Command Usage - Console");
	}

	virtual void QueryGlobal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}
		if (this->globalStatus == ResponseStatus::ENABLED)
		{
			auto callback = [uid = this->uid](KZ::API::events::WorldRecords &wrs)
			{
				TopRecordRequest *req = (TopRecordRequest *)TopRecordRequest::Find(uid);
				if (!req)
				{
					return;
				}

				if (wrs.map.has_value() && wrs.course.has_value())
				{
					req->mapName = wrs.map->name.c_str();
					req->courseName = wrs.course->name.c_str();
					req->globalStatus = ResponseStatus::RECEIVED;
				}
				else
				{
					req->globalStatus = ResponseStatus::DISABLED;
					return;
				}

				req->wrData.hasRecord = wrs.overall.has_value();
				if (req->wrData.hasRecord)
				{
					req->wrData.holder = wrs.overall->player.name.c_str();
					req->wrData.runTime = wrs.overall->time;
					req->wrData.teleportsUsed = wrs.overall->teleports;
				}
				req->wrData.hasRecordPro = wrs.pro.has_value();
				if (req->wrData.hasRecordPro)
				{
					req->wrData.holderPro = wrs.pro->player.name.c_str();
					req->wrData.runTimePro = wrs.pro->time;
				}
			};
			this->globalStatus = ResponseStatus::PENDING;
			KZGlobalService::QueryWorldRecords(std::string_view(this->mapName.Get(), this->mapName.Length()),
											   std::string_view(this->courseName.Get(), this->courseName.Length()), this->apiMode, callback);
		}
	}

	virtual void QueryLocal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}
		if (this->localStatus == ResponseStatus::ENABLED)
		{
			this->localStatus = ResponseStatus::PENDING;

			KZPlayer *callingPlayer = g_pKZPlayerManager->ToPlayer(userID);
			if (!callingPlayer)
			{
				this->Invalidate();
				return;
			}

			u64 uid = this->uid;

			auto onQuerySuccess = [uid](std::vector<ISQLQuery *> queries)
			{
				TopRecordRequest *req = (TopRecordRequest *)TopRecordRequest::Find(uid);
				if (!req)
				{
					return;
				}
				req->localStatus = ResponseStatus::RECEIVED;
				ISQLResult *result = queries[0]->GetResultSet();
				if (result && result->GetRowCount() > 0)
				{
					req->srData.hasRecord = true;
					if (result->FetchRow())
					{
						req->srData.holder = result->GetString(2);
						req->srData.runTime = result->GetFloat(3);
						req->srData.teleportsUsed = result->GetInt(4);
					}
				}
				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					req->srData.hasRecordPro = true;
					if (result->FetchRow())
					{
						req->srData.holderPro = result->GetString(2);
						req->srData.runTimePro = result->GetFloat(3);
					}
				}
			};

			auto onQueryFailure = [uid](std::string, int)
			{
				TopRecordRequest *req = (TopRecordRequest *)TopRecordRequest::Find(uid);
				if (req)
				{
					req->localStatus = ResponseStatus::DISABLED;
				}
			};

			KZDatabaseService::QueryRecords(this->mapName, this->courseName, this->localModeID, 1, 0, onQuerySuccess, onQueryFailure);
		}
	}

	virtual void Reply()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		if (localStatus != ResponseStatus::RECEIVED && globalStatus != ResponseStatus::RECEIVED)
		{
			player->languageService->PrintChat(true, false, "Top Record Request - Failed (Generic)");
			return;
		}
		if (this->localStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyLocal();
		}
		if (this->globalStatus == ResponseStatus::RECEIVED)
		{
			this->ReplyGlobal();
		}
	}

	void ReplyGlobal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		// Local stuff
		std::string tpText;
		if (wrData.teleportsUsed > 0)
		{
			tpText = wrData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
											   : player->languageService->PrepareMessage("2+ Teleports Text", wrData.teleportsUsed);
		}

		char standardTime[32];
		utils::FormatTime(wrData.runTime, standardTime, sizeof(standardTime));
		char proTime[32];
		utils::FormatTime(wrData.runTimePro, proTime, sizeof(proTime));

		// Global Records on kz_map (Main) [VNL]
		player->languageService->PrintChat(true, false, "WR Header", mapName.Get(), courseName.Get(), modeName.Get());
		if (!wrData.hasRecord)
		{
			player->languageService->PrintChat(true, false, "No Times");
		}
		else if (!wrData.hasRecordPro)
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, wrData.holder.Get(), tpText);
		}
		// Their MAP PB has 0 teleports, and is therefore also their PRO PB
		else if (wrData.teleportsUsed == 0)
		{
			// KZ | Overall/PRO Record: 01:23.45 by Bill
			player->languageService->PrintChat(true, false, "Top Record - Combined", standardTime, wrData.holder.Get());
		}
		else
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, wrData.holder.Get(), tpText);
			// KZ | PRO Record: 23.45 by Player
			player->languageService->PrintChat(true, false, "Top Record - PRO", proTime, wrData.holderPro.Get());
		}
	}

	void ReplyLocal()
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		// Local stuff
		std::string localTPText;
		if (srData.teleportsUsed > 0)
		{
			localTPText = srData.teleportsUsed == 1 ? player->languageService->PrepareMessage("1 Teleport Text")
													: player->languageService->PrepareMessage("2+ Teleports Text", srData.teleportsUsed);
		}

		char standardTime[32];
		utils::FormatTime(srData.runTime, standardTime, sizeof(standardTime));
		char proTime[32];
		utils::FormatTime(srData.runTimePro, proTime, sizeof(proTime));

		// Server Records on kz_map (Main) [VNL]
		player->languageService->PrintChat(true, false, "SR Header", mapName.Get(), courseName.Get(), modeName.Get());
		if (!srData.hasRecord)
		{
			player->languageService->PrintChat(true, false, "No Times");
		}
		else if (!srData.hasRecordPro)
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, srData.holder.Get(), localTPText);
		}
		// Their MAP PB has 0 teleports, and is therefore also their PRO PB
		else if (srData.teleportsUsed == 0)
		{
			// KZ | Overall/PRO Record: 01:23.45 by Bill
			player->languageService->PrintChat(true, false, "Top Record - Combined", standardTime, srData.holder.Get());
		}
		else
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, srData.holder.Get(), localTPText);
			// KZ | PRO Record: 23.45 by Player
			player->languageService->PrintChat(true, false, "Top Record - PRO", proTime, srData.holderPro.Get());
		}
	}
};

SCMD(kz_wr, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	TopRecordRequest::Create<TopRecordRequest>(player, TopRecordRequest::trFeatures, true, true, args);
	return MRES_SUPERCEDE;
}

SCMD(kz_sr, SCFL_RECORD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	TopRecordRequest::Create<TopRecordRequest>(player, TopRecordRequest::trFeatures, true, false, args);
	return MRES_SUPERCEDE;
}
