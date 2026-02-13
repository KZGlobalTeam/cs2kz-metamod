#pragma once
#include <memory>
#include <vector>
#include <functional>

#include "common.h"
#include "utils/utils.h"
#include "utils/argparse.h"
#include "utils/tables.h"
#include "kz/global/api.h"
#include "kz/language/kz_language.h"

struct BaseRequest
{
protected:
	BaseRequest(u64 uid, KZPlayer *player);

public:
	virtual ~BaseRequest();
	virtual void Init(u64 features, const CCommand *args, bool queryLocal, bool queryGlobal);

protected:
	static inline u64 idCount = 0;
	static inline std::vector<BaseRequest *> instances;
	static inline constexpr const char *paramKeys[] = {"c", "course", "mode", "map", "o", "offset", "l", "limit", "s", "style"};
	f64 timeout = 5.0f;

public:
	template<typename T, typename... Args>
	static T *Create(KZPlayer *player, u64 features, bool queryLocal, bool queryGlobal, const CCommand *args)
	{
		auto obj = new T(idCount++, player);
		obj->Init(features, args, queryLocal, queryGlobal);
		instances.push_back(obj);
		return obj;
	}

	static BaseRequest *Find(u64 uid);
	static void Remove(u64 uid);
	static void CheckRequests();

public:
	const u64 uid;
	// UserID for callback.
	const CPlayerUserId userID;
	const f64 timestamp;

	bool isValid = true;

	std::string invalidateReasonLocalized {};

	template<typename... Args>
	void Invalidate(CUtlString reason = "", Args &&...args)
	{
		if (!isValid)
		{
			return;
		}
		isValid = false;
		if (reason.IsEmpty())
		{
			return;
		}
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(this->userID);
		if (player && player->IsInGame())
		{
			invalidateReasonLocalized = player->languageService->PrepareMessage(reason, std::forward<Args>(args)...);
		}
	}

	// This defines which parameter should be enabled.
	enum RequestFeature
	{
		None = 0,
		Course = 1 << 0,
		Map = 1 << 1,
		Limit = 1 << 2,
		Offset = 1 << 3,
		Mode = 1 << 4,
		Style = 1 << 5,
		Player = 1 << 6,
	};

	u64 features;

	constexpr bool HasFeature(RequestFeature feature)
	{
		return (features & feature) == feature;
	}

	enum struct ResponseStatus
	{
		DISABLED = 0, // Query is disabled
		ENABLED,      // Query is enabled, but not sent
		PENDING,      // Query is sent, waiting for response
		RECEIVED      // Response received
	} localStatus, globalStatus;

	void CheckReply()
	{
		if (!isValid)
		{
			return;
		}
		if ((localStatus == ResponseStatus::RECEIVED || localStatus == ResponseStatus::DISABLED)
			&& (globalStatus == ResponseStatus::RECEIVED || globalStatus == ResponseStatus::DISABLED))
		{
			Reply();
			Invalidate();
		}
	};

protected:
	CUtlString mapName;

	CUtlString courseName;
	bool requestingFirstCourse = false;

	u64 limit = 10;
	u64 offset = 0;

	u64 targetSteamID64 {};
	CUtlString targetPlayerName {};
	// If this is true, the queries should be delayed until it's true.
	bool requestingLocalPlayer = false;
	// If this is true, the *local* query should be delayed.
	bool requestingGlobalPlayer = false;

	u64 localModeID;
	CUtlString modeName;
	KZ::api::Mode apiMode;

	u64 localStyleIDs;
	CUtlVector<CUtlString> styleList;

public:
	void SetupCourse(CUtlString courseName);
	void SetupPlayer(CUtlString playerName);

	void SetupMode(CUtlString modeName);

	void SetupStyles(CUtlString styleNames);

	virtual void PrintInstructions() {}

	virtual void QueryLocal() = 0;
	virtual void QueryGlobal() = 0;

	virtual void Reply() = 0;
};
