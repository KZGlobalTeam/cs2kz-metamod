#pragma once

#include "convar.h"
#include "tier0/logging.h"

extern CConVar<bool> kz_log_to_file;

// Common tag applied to every CS2KZ-owned logging channel.
// KZLoggingListener uses this to filter which channels it processes.
#define KZ_LOG_TAG "CS2KZ"

// Enum for service selection - maps to individual channels
enum class LogChannel
{
	General,
	AC,
	DB,
	Global,
	Language,
	MappingAPI,
	Misc,
	Mode,
	Movement,
	Option,
	Player,
	Profile,
	Racing,
	Recording,
	Replays,
	Style,
	Timer,
	Tip,
	Trigger,
};

// Register CS2KZ logging listener and all CS2KZ channels from the channel table.
void RegisterKZLogging();

// Get the logging channel for a given service.
LoggingChannelID_t GetServiceChannel(LogChannel service);

// Get the name of a logging channel
const char *GetServiceChannelName(LoggingChannelID_t channelID);

#define KZ_LOG_INFO(service, fmt, ...)  Log_Msg(GetServiceChannel(service), fmt, ##__VA_ARGS__)
#define KZ_LOG_DEBUG(service, fmt, ...) InternalMsg(GetServiceChannel(service), LS_DETAILED, fmt, ##__VA_ARGS__)
#define KZ_LOG_WARN(service, fmt, ...)  Log_Warning(GetServiceChannel(service), fmt, ##__VA_ARGS__)
#define KZ_LOG_ERROR(service, fmt, ...) Log_Error(GetServiceChannel(service), fmt, ##__VA_ARGS__)

#include "filesystem.h"

// Logging listener that claims every channel tagged with KZ_LOG_TAG, prints
// it to the console with a [CS2KZ.<Service>] [<LEVEL>] prefix and optionally
// mirrors output to a log file.
class KZLoggingListener : public ILoggingListener
{
public:
	void Log(const LoggingContext_t *pContext, const tchar *pMessage) override;
	void CheckFile();

private:
	void OpenFile();
	void CloseFile();
	FileHandle_t m_pFile = nullptr;
};

inline KZLoggingListener g_KZLoggingListener {};
