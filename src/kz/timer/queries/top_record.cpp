#include "base_request.h"
#include "kz/timer/kz_timer.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/global/messages.h"

#include "utils/simplecmds.h"

#include "vendor/sql_mm/src/public/sql_mm.h"

struct TopRecordRequest : public BaseRequest
{
	using BaseRequest::BaseRequest;
	static constexpr u64 trFeatures = RequestFeature::Course | RequestFeature::Map | RequestFeature::Mode;

	struct RecordData
	{
		bool hasRecord {};
		std::string holder;
		f32 runTime {};
		u32 teleportsUsed {};

		bool hasRecordPro {};
		std::string holderPro;
		f32 runTimePro {};
	} srData, wrData;

	virtual void PrintInstructions() override
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
		if (!player)
		{
			return;
		}
		player->languageService->PrintChat(true, false, "Top Record Command Usage");
		player->languageService->PrintConsole(false, false, "Top Record Command Usage - Console");
	}

	virtual void QueryGlobal()
	{
		if (this->requestingFirstCourse)
		{
			return;
		}
		if (this->globalStatus == ResponseStatus::ENABLED)
		{
			this->globalStatus = ResponseStatus::PENDING;

			std::string_view map = this->mapName;
			std::string_view course = this->courseName;
			KZGlobalService::MessageCallback<KZ::api::messages::WorldRecords> callback(TopRecordRequest::OnWorldRecordsQueried, this->uid);
			KZGlobalService::QueryWorldRecords(map, course, this->apiMode, std::move(callback));
		}
	}

	static void OnWorldRecordsQueried(const KZ::api::messages::WorldRecords &records, u64 uid)
	{
		TopRecordRequest *req = (TopRecordRequest *)TopRecordRequest::Find(uid);
		if (!req)
		{
			return;
		}

		if (records.map.has_value() && records.course.has_value())
		{
			req->mapName = records.map->name.c_str();
			req->courseName = records.course->name.c_str();
			req->globalStatus = ResponseStatus::RECEIVED;
		}
		else
		{
			req->globalStatus = ResponseStatus::DISABLED;
			return;
		}

		req->wrData.hasRecord = records.overall.has_value();
		if (req->wrData.hasRecord)
		{
			req->wrData.holder = records.overall->player.name.c_str();
			req->wrData.runTime = records.overall->time;
			req->wrData.teleportsUsed = records.overall->teleports;
		}
		req->wrData.hasRecordPro = records.pro.has_value();
		if (req->wrData.hasRecordPro)
		{
			req->wrData.holderPro = records.pro->player.name.c_str();
			req->wrData.runTimePro = records.pro->time;
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
						req->srData.holder = utils::OrEmpty(result->GetString(2));
						req->srData.runTime = result->GetFloat(3);
						req->srData.teleportsUsed = result->GetInt(4);
					}
				}
				if ((result = queries[1]->GetResultSet()) && result->GetRowCount() > 0)
				{
					req->srData.hasRecordPro = true;
					if (result->FetchRow())
					{
						req->srData.holderPro = utils::OrEmpty(result->GetString(2));
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
		player->languageService->PrintChat(true, false, "WR Header", mapName.c_str(), courseName.c_str(), modeName.c_str());
		if (!wrData.hasRecord)
		{
			player->languageService->PrintChat(true, false, "No Times");
		}
		else if (!wrData.hasRecordPro)
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, wrData.holder.c_str(), tpText);
		}
		// Their MAP PB has 0 teleports, and is therefore also their PRO PB
		else if (wrData.teleportsUsed == 0)
		{
			// KZ | Overall/PRO Record: 01:23.45 by Bill
			player->languageService->PrintChat(true, false, "Top Record - Combined", standardTime, wrData.holder.c_str());
		}
		else
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, wrData.holder.c_str(), tpText);
			// KZ | PRO Record: 23.45 by Player
			player->languageService->PrintChat(true, false, "Top Record - PRO", proTime, wrData.holderPro.c_str());
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
		player->languageService->PrintChat(true, false, "SR Header", mapName.c_str(), courseName.c_str(), modeName.c_str());
		if (!srData.hasRecord)
		{
			player->languageService->PrintChat(true, false, "No Times");
		}
		else if (!srData.hasRecordPro)
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, srData.holder.c_str(), localTPText);
		}
		// Their MAP PB has 0 teleports, and is therefore also their PRO PB
		else if (srData.teleportsUsed == 0)
		{
			// KZ | Overall/PRO Record: 01:23.45 by Bill
			player->languageService->PrintChat(true, false, "Top Record - Combined", standardTime, srData.holder.c_str());
		}
		else
		{
			// KZ | Overall Record: 01:23.45 (5 TP) by Bill
			player->languageService->PrintChat(true, false, "Top Record - Overall", standardTime, srData.holder.c_str(), localTPText);
			// KZ | PRO Record: 23.45 by Player
			player->languageService->PrintChat(true, false, "Top Record - PRO", proTime, srData.holderPro.c_str());
		}
	}
};

SCMD(kz_wr, SCFL_RECORD | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	TopRecordRequest::Create<TopRecordRequest>(player, TopRecordRequest::trFeatures, true, true, args);
	return true;
}

SCMD(kz_sr, SCFL_RECORD)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	TopRecordRequest::Create<TopRecordRequest>(player, TopRecordRequest::trFeatures, true, false, args);
	return true;
}
