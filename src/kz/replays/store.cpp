#include "kz_replaysystem.h"
#include "ledger.h"
#include "watcher.h"
#include "kz/kz.h"
#include "kz/option/kz_option.h"
#include "utils/async_file_io.h"
#include "utils/http.h"

extern ReplayWatcher g_ReplayWatcher;

namespace KZ::replaysystem
{
	// Callbacks of the replays being downloaded.
	static_global std::unordered_map<UUID_t, std::vector<std::function<void(const std::string &path)>>> fetches;

	void LocateReplays(const std::vector<std::pair<UUID_t, std::string>> &replays, AvailabilityCallback callback)
	{
		std::unordered_map<UUID_t, ReplayAvailability> results;
		for (const auto &[uuid, map] : replays)
		{
			std::string path;
			if (FindReplayPath(uuid.ToString().c_str(), path))
			{
				const auto header = g_ReplayWatcher.GetReplayHeader(uuid);
				results[uuid] = {true, header ? header->map().name() : std::string()};
				continue;
			}
			results[uuid] = {};
		}
		callback(std::move(results));
	}

	static_function void FinishFetch(const UUID_t &uuid, const std::string &path)
	{
		auto it = fetches.find(uuid);
		if (it == fetches.end())
		{
			return;
		}
		auto callbacks = std::move(it->second);
		fetches.erase(it);
		for (auto &callback : callbacks)
		{
			callback(path);
		}
	}

	static_function void SaveDownload(const UUID_t &uuid, std::vector<char> &&data)
	{
		const std::string path = std::string(KZ_REPLAY_DOWNLOADS_PATH) + "/" + uuid.ToString() + ".replay";
		if (!g_asyncFileIO)
		{
			FinishFetch(uuid, "");
			return;
		}
		auto onWritten = [uuid, path](bool success)
		{
			if (success)
			{
				NotifyReplayFileChanged(uuid.ToString().c_str());
			}
			FinishFetch(uuid, success ? path : "");
		};
		g_asyncFileIO->QueueWriteBuffer(path, std::move(data), onWritten);
	}

	static_function void DownloadReplay(const UUID_t &uuid, const std::string &url, std::function<void()> onMissing)
	{
		HTTP::Request request(HTTP::Method::GET, url);
		auto onResponse = [uuid, onMissing](HTTP::Response response)
		{
			std::optional<std::vector<char>> body = response.status == 200 ? response.RawBody() : std::nullopt;
			if (body.has_value() && !body->empty())
			{
				SaveDownload(uuid, std::move(*body));
				return;
			}
			onMissing();
		};
		request.Send(onResponse, onMissing);
	}

	static_function void DownloadReplayFromAPI(const UUID_t &uuid)
	{
		std::string url = KZOptionService::GetOptionStr("replayDownloadUrl", "https://replays.cs2kz.org/{id}");
		const size_t idPos = url.find("{id}");
		if (idPos == std::string::npos)
		{
			KZ_LOG_WARN(LogChannel::Global, "`replayDownloadUrl` (%s) has no {id} placeholder; cannot download replays.\n", url.c_str());
			FinishFetch(uuid, "");
			return;
		}
		url.replace(idPos, strlen("{id}"), uuid.ToString());
		DownloadReplay(uuid, url, [uuid]() { FinishFetch(uuid, ""); });
	}

	void FetchReplay(const UUID_t &uuid, std::function<void(const std::string &path)> callback)
	{
		std::string path;
		if (FindReplayPath(uuid.ToString().c_str(), path))
		{
			callback(path);
			return;
		}
		auto &waiting = fetches[uuid];
		waiting.push_back(std::move(callback));
		if (waiting.size() > 1)
		{
			return;
		}
		DownloadReplayFromAPI(uuid);
	}

	void OnAPIUploadFinished(const UUID_t &uuid, bool uploaded)
	{
		ledger::UpdateEntry(uuid,
							[uploaded](ledger::Entry &entry)
							{
								if (!entry.uploadToAPI)
								{
									return;
								}
								entry.uploadToAPI = false;
								if (uploaded)
								{
									entry.offloaded =
										entry.offloaded || KZOptionService::GetOptionInt("replays/disk/deleteGlobalRunsAfterUpload", false);
								}
							});
	}
} // namespace KZ::replaysystem
