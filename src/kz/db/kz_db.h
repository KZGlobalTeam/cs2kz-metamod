#pragma once
#include "../kz.h"

class KZDBService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	enum DatabaseType
	{
		DatabaseType_None = -1,
		DatabaseType_SQLite,
		DatabaseType_MySQL
	};

	static_global void Init();

	static_global bool IsReady();
	static_global void RegisterCommands();
	static_global void SetupDatabase();
	static_global void CreateTables();
	static_global void SetupMap();
	static_global void SetupCourses();

private:
	static_global DatabaseType GetDatabaseType();
};
