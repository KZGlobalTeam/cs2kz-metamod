#include "common.h"
#include "utils/logging.h"
#include "kz/option/kz_option.h"

CConVar<bool> kz_log_to_file("kz_log_to_file", FCVAR_NONE, "Whether to mirror CS2KZ log output to a file in addons/cs2kz/logs.", true,
							 [](CConVar<bool> *, CSplitScreenSlot, const bool *, const bool *) { g_KZLoggingListener.CheckFile(); });

struct KZChannel_t
{
	LogChannel channel;
	const char *name;
	LoggingChannelID_t handle;
};

static void RegisterKZChannelTags(LoggingChannelID_t channelID)
{
	LoggingSystem_AddTagToChannel(channelID, KZ_LOG_TAG);
}

static KZChannel_t g_KZChannels[] = {
	// Handles are set in function below
	{LogChannel::General, "CS2KZ::General", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::AC, "CS2KZ::AC", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::DB, "CS2KZ::DB", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Global, "CS2KZ::Global", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Language, "CS2KZ::Language", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::MappingAPI, "CS2KZ::MappingAPI", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Misc, "CS2KZ::Misc", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Mode, "CS2KZ::Mode", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Movement, "CS2KZ::Movement", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Option, "CS2KZ::Option", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Player, "CS2KZ::Player", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Profile, "CS2KZ::Profile", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Racing, "CS2KZ::Racing", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Recording, "CS2KZ::Recording", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Replays, "CS2KZ::Replays", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Style, "CS2KZ::Style", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Timer, "CS2KZ::Timer", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Tip, "CS2KZ::Tip", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Trigger, "CS2KZ::Trigger", INVALID_LOGGING_CHANNEL_ID},
};

static void PurgeLogs()
{
	int retentionDays = (int)KZOptionService::GetOptionInt("logRetentionDays", 30);
	if (retentionDays <= 0)
	{
		return;
	}

	std::filesystem::path logDir = std::filesystem::path(g_SMAPI->GetBaseDir()) / "addons" / "cs2kz" / "logs";

	std::error_code ec;
	if (!std::filesystem::exists(logDir, ec) || ec)
	{
		return;
	}

	auto cutoff = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * retentionDays);

	for (const auto &entry : std::filesystem::directory_iterator(logDir, ec))
	{
		if (ec)
		{
			break;
		}
		if (!entry.is_regular_file())
		{
			continue;
		}
		const auto &path = entry.path();
		if (path.extension() != ".log" || path.stem().string().rfind("cs2kz", 0) != 0)
		{
			continue;
		}
		auto writeTime = entry.last_write_time(ec);
		if (ec)
		{
			ec.clear();
			continue;
		}
		if (writeTime < cutoff)
		{
			std::filesystem::remove(path, ec);
			ec.clear();
		}
	}
}

void InitKZLogging()
{
	LoggingSystem_RegisterLoggingListener(&g_KZLoggingListener);
	LoggingVerbosity_t verbosity = CommandLine()->FindParm("-debug") ? LV_MAX : LV_DEFAULT;
	for (auto &entry : g_KZChannels)
	{
		if (LoggingSystem_FindChannel(entry.name) == -1)
		{
			entry.handle = LoggingSystem_RegisterLoggingChannel(entry.name, RegisterKZChannelTags, 0, verbosity, Color(37, 162, 255, 255));
		}
	}
	PurgeLogs();
}

LoggingChannelID_t GetServiceChannel(LogChannel channel)
{
	for (const auto &entry : g_KZChannels)
	{
		if (entry.channel == channel)
		{
			return entry.handle;
		}
	}

	return g_KZChannels[0].handle;
}

const char *GetServiceChannelName(LoggingChannelID_t channelID)
{
	for (const auto &entry : g_KZChannels)
	{
		if (entry.handle == channelID)
		{
			return entry.name;
		}
	}

	return g_KZChannels[0].name;
}

void KZLoggingListener::Log(const LoggingContext_t *pContext, const tchar *pMessage)
{
	if (!LoggingSystem_HasTag(pContext->m_ChannelID, KZ_LOG_TAG))
	{
		return;
	}

	if (kz_log_to_file.Get())
	{
		CheckFile();
		if (!m_pFile)
		{
			return;
		}
		std::time_t t = std::time(nullptr);
		std::tm tm {};
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		size_t msgLen = V_strlen(pMessage);
		bool needsNewline = (msgLen == 0 || pMessage[msgLen - 1] != '\n');
		char ts[32];
		std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

		char logMessage[1024];
		V_snprintf(logMessage, sizeof(logMessage), "[%s] %s%s", ts, pMessage, needsNewline ? "\n" : "");
		fputs(logMessage, m_pFile);
		fflush(m_pFile);
	}
}

void KZLoggingListener::CheckFile()
{
	if (kz_log_to_file.Get())
	{
		OpenFile();
	}
	else
	{
		CloseFile();
	}
}

void KZLoggingListener::OpenFile()
{
	if (m_pFile)
	{
		return;
	}

	std::filesystem::path logDir = std::filesystem::path(g_SMAPI->GetBaseDir()) / "addons" / "cs2kz" / "logs";

	std::error_code ec;
	if (!std::filesystem::exists(logDir))
	{
		std::filesystem::create_directories(logDir, ec);
		if (ec)
		{
			return;
		}
	}

	char szFileName[256];
	if (KZOptionService::GetOptionInt("logNewFileOnStartup", true))
	{
		std::time_t now = std::time(nullptr);
		std::tm tm {};
#ifdef _WIN32
		localtime_s(&tm, &now);
#else
		localtime_r(&now, &tm);
#endif
		V_snprintf(szFileName, sizeof(szFileName), "cs2kz_%04d-%02d-%02d.log", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	}
	else
	{
		V_strncpy(szFileName, "cs2kz.log", sizeof(szFileName));
	}

	std::filesystem::path fullPath = logDir / szFileName;
	m_pFile = fopen(fullPath.string().c_str(), "a");
}

void KZLoggingListener::CloseFile()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = nullptr;
	}
}
