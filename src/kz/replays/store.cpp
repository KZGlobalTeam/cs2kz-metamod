#include "store.h"
#include "kz_replaysystem.h"
#include "ledger.h"
#include "watcher.h"
#include "kz/kz.h"
#include "kz/option/kz_option.h"
#include "utils/async_file_io.h"
#include "utils/http.h"
#include "utils/json.h"
#include "filesystem/ifilesystem.h"

#include <memory>

extern ReplayWatcher g_ReplayWatcher;

namespace KZ::replaysystem::store
{
	struct Upload
	{
		u32 attempts {};
		f64 nextAttempt {};
		bool inFlight {};
		// Only the first attempt has the replay in memory.
		std::vector<char> data;
	};

	static_global constexpr u32 maxUploadsInFlight = 2;
	static_global constexpr f64 uploadRetryDelays[] = {2.0, 10.0, 60.0, 300.0};
	static_global constexpr f64 rejectedUploadRetryDelay = 3600.0;
	static_global std::unordered_map<UUID_t, Upload> uploads;
	static_global u32 uploadsInFlight;
	static_global f64 nextUploadCheck;

	static_function std::string EncodeURLValue(const std::string &text)
	{
		std::string out;
		for (unsigned char c : text)
		{
			if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			{
				out += (char)c;
			}
			else
			{
				out += tfm::format("%%%02X", (u32)c);
			}
		}
		return out;
	}

	static_function std::string FillURL(std::string url, const UUID_t &uuid, const std::string &map)
	{
		const std::pair<const char *, std::string> placeholders[] = {{"{id}", uuid.ToString()}, {"{map}", EncodeURLValue(map)}};
		for (const auto &[placeholder, value] : placeholders)
		{
			for (size_t pos = url.find(placeholder); pos != std::string::npos; pos = url.find(placeholder, pos + value.size()))
			{
				url.replace(pos, strlen(placeholder), value);
			}
		}
		return url;
	}

	static_function std::string GetCurrentMap()
	{
		return g_pKZUtils->GetCurrentMapName().c_str();
	}

	static_function void SetAuthorization(HTTP::Request &request)
	{
		const std::string auth = KZOptionService::GetOptionStr("replays/store/auth", "");
		if (!auth.empty())
		{
			request.SetHeader("Authorization", auth);
		}
	}

	// A replay file starts with the little-endian length of its header and the header itself.
	static_function bool ParseHeader(const char *data, size_t size, ReplayHeader &header)
	{
		u32 length = 0;
		if (size < sizeof(length))
		{
			return false;
		}
		memcpy(&length, data, sizeof(length));
		return length > 0 && length <= size - sizeof(length) && header.ParseFromArray(data + sizeof(length), (i32)length);
	}

	static_function bool DecodeBase64(const std::string &text, std::string &out)
	{
		auto decode = [](char c) -> i32
		{
			if (c >= 'A' && c <= 'Z')
			{
				return c - 'A';
			}
			if (c >= 'a' && c <= 'z')
			{
				return c - 'a' + 26;
			}
			if (c >= '0' && c <= '9')
			{
				return c - '0' + 52;
			}
			// The URL-safe alphabet uses - and _ for + and /.
			if (c == '+' || c == '-')
			{
				return 62;
			}
			if (c == '/' || c == '_')
			{
				return 63;
			}
			return -1;
		};
		out.clear();
		u32 buffer = 0;
		i32 bits = 0;
		for (char c : text)
		{
			if (c == '=')
			{
				break;
			}
			const i32 value = decode(c);
			if (value < 0)
			{
				return false;
			}
			buffer = (buffer << 6) | (u32)value;
			bits += 6;
			if (bits >= 8)
			{
				bits -= 8;
				out.push_back((char)((buffer >> bits) & 0xFF));
			}
		}
		return true;
	}

	static_function void FinishUpload(const UUID_t &uuid, bool stored)
	{
		uploads.erase(uuid);
		ledger::UpdateEntry(uuid,
							[stored](ledger::Entry &entry)
							{
								entry.uploadToStore = false;
								entry.offloaded = entry.offloaded || stored;
							});
	}

	static_function void RetryUpload(const UUID_t &uuid, u16 status)
	{
		auto it = uploads.find(uuid);
		if (it == uploads.end())
		{
			return;
		}
		Upload &upload = it->second;
		upload.inFlight = false;
		// Other client errors usually mean a setup problem such as wrong credentials or a size limit.
		const bool transient = status == 0 || status >= 500 || status == 408 || status == 429;
		const f64 delay = transient ? uploadRetryDelays[MIN((size_t)upload.attempts, KZ_ARRAYSIZE(uploadRetryDelays)) - 1] : rejectedUploadRetryDelay;
		upload.nextAttempt = Plat_FloatTime() + delay;
		if (!transient)
		{
			KZ_LOG_WARN(LogChannel::Replays, "The replay store rejected replay %s with status %u and it is retried in an hour.\n",
						uuid.ToString().c_str(), status);
		}
		else if (upload.attempts == 1)
		{
			KZ_LOG_WARN(LogChannel::Replays, "Failed to upload replay %s to the replay store (status %u) and it is retried later.\n",
						uuid.ToString().c_str(), status);
		}
	}

	static_function void SendUpload(const UUID_t &uuid, std::vector<char> &&data)
	{
		ReplayHeader header;
		if (!ParseHeader(data.data(), data.size(), header))
		{
			KZ_LOG_WARN(LogChannel::Replays, "Replay %s has an unreadable header and is not uploaded to the replay store.\n",
						uuid.ToString().c_str());
			uploadsInFlight--;
			FinishUpload(uuid, false);
			return;
		}
		HTTP::Request request(HTTP::Method::PUT, FillURL(KZOptionService::GetOptionStr("replays/store/uploadUrl", ""), uuid, header.map().name()));
		SetAuthorization(request);
		request.SetBody(std::string(data.begin(), data.end()), "application/octet-stream");
		auto onResponse = [uuid](HTTP::Response response)
		{
			uploadsInFlight--;
			// A store that refuses to overwrite already has the replay because replay UUIDs are unique.
			if ((response.status >= 200 && response.status < 300) || response.status == 409 || response.status == 412)
			{
				FinishUpload(uuid, true);
				return;
			}
			RetryUpload(uuid, response.status);
		};
		auto onError = [uuid]()
		{
			uploadsInFlight--;
			RetryUpload(uuid, 0);
		};
		request.Send(onResponse, onError);
	}

	static_function void StartUpload(const UUID_t &uuid)
	{
		Upload &upload = uploads[uuid];
		upload.inFlight = true;
		upload.attempts++;
		uploadsInFlight++;
		if (!upload.data.empty())
		{
			std::vector<char> data = std::move(upload.data);
			upload.data.clear();
			SendUpload(uuid, std::move(data));
			return;
		}
		const std::string path = std::string(KZ_REPLAY_PATH) + "/" + uuid.ToString() + ".replay";
		auto onRead = [uuid, path](bool success, std::vector<char> &&buffer)
		{
			if (!uploads.count(uuid))
			{
				uploadsInFlight--;
				return;
			}
			if (success && !buffer.empty())
			{
				SendUpload(uuid, std::move(buffer));
				return;
			}
			uploadsInFlight--;
			if (g_pFullFileSystem->FileExists(path.c_str()))
			{
				RetryUpload(uuid, 0);
				return;
			}
			KZ_LOG_WARN(LogChannel::Replays, "Replay %s is no longer on disk and is not uploaded to the replay store.\n", uuid.ToString().c_str());
			FinishUpload(uuid, false);
		};
		if (!g_asyncFileIO)
		{
			uploadsInFlight--;
			RetryUpload(uuid, 0);
			return;
		}
		g_asyncFileIO->QueueRead(path, onRead);
	}

	bool IsEnabled()
	{
		return KZOptionService::GetOptionStr("replays/store/uploadUrl", "")[0] != '\0';
	}

	void Init()
	{
		const bool enabled = IsEnabled();
		f64 nextAttempt = Plat_FloatTime();
		for (const auto &[uuid, entry] : ledger::GetEntries())
		{
			if (!entry.IsPending())
			{
				continue;
			}
			// An upload to the API can't resume because its key only lived in memory.
			// A run still waiting for the API keeps the UUID it has.
			ledger::UpdateEntry(uuid,
								[enabled](ledger::Entry &entry)
								{
									entry.waitingForUUID = false;
									entry.uploadToAPI = false;
									entry.uploadToStore = enabled;
								});
			if (enabled)
			{
				uploads[uuid].nextAttempt = nextAttempt;
				nextAttempt += 0.5;
			}
		}
	}

	void ProcessUploads()
	{
		const f64 now = Plat_FloatTime();
		if (uploads.empty() || now < nextUploadCheck)
		{
			return;
		}
		nextUploadCheck = now + 1.0;
		if (!IsEnabled())
		{
			return;
		}
		std::vector<UUID_t> due;
		for (const auto &[uuid, upload] : uploads)
		{
			if (!upload.inFlight && upload.nextAttempt <= now)
			{
				due.push_back(uuid);
			}
		}
		for (const UUID_t &uuid : due)
		{
			if (uploadsInFlight >= maxUploadsInFlight)
			{
				break;
			}
			StartUpload(uuid);
		}
	}

	void QueueUpload(const UUID_t &uuid, std::vector<char> &&data)
	{
		if (!IsEnabled())
		{
			return;
		}
		ledger::UpdateEntry(uuid, [](ledger::Entry &entry) { entry.uploadToStore = true; });
		Upload &upload = uploads[uuid];
		if (!upload.inFlight && upload.data.empty())
		{
			upload.data = std::move(data);
		}
	}

	bool HasQueryEndpoint()
	{
		return IsEnabled() && KZOptionService::GetOptionStr("replays/store/queryUrl", "")[0] != '\0';
	}

	static_function void SendQuery(const std::vector<std::pair<std::string, std::string>> &parameters, QueryCallback callback)
	{
		HTTP::Request request(HTTP::Method::GET, KZOptionService::GetOptionStr("replays/store/queryUrl", ""));
		for (const auto &[name, value] : parameters)
		{
			request.SetQuery(name, EncodeURLValue(value));
		}
		SetAuthorization(request);
		auto onResponse = [callback](HTTP::Response response)
		{
			nlohmann::json json = nlohmann::json::parse(response.Body().value_or(""), nullptr, false);
			if (response.status != 200 || !json.is_object() || !json.contains("total") || !json["total"].is_number_integer()
				|| !json.contains("replays") || !json["replays"].is_array())
			{
				KZ_LOG_WARN(LogChannel::Replays, "The replay store's query endpoint gave an invalid answer (status %u).\n", response.status);
				callback(false, 0, {});
				return;
			}
			std::vector<std::pair<UUID_t, ReplayHeader>> replays;
			for (const auto &item : json["replays"])
			{
				UUID_t uuid(false);
				std::string bytes;
				ReplayHeader header;
				if (item.is_object() && item.contains("id") && item["id"].is_string() && item.contains("header") && item["header"].is_string()
					&& UUID_t::FromString(item["id"].get<std::string>().c_str(), &uuid) && DecodeBase64(item["header"].get<std::string>(), bytes)
					&& ParseHeader(bytes.data(), bytes.size(), header))
				{
					replays.push_back({uuid, std::move(header)});
				}
			}
			callback(true, json["total"].get<u64>(), std::move(replays));
		};
		request.Send(onResponse, [callback]() { callback(false, 0, {}); });
	}

	void QueryReplays(const ReplayFilterCriteria &criteria, u64 offset, u64 limit, QueryCallback callback)
	{
		std::vector<std::pair<std::string, std::string>> parameters;
		switch (criteria.type)
		{
			case RP_RUN:
			{
				parameters.push_back({"type", "run"});
				if (!criteria.modeNameSubString.empty())
				{
					parameters.push_back({"mode", criteria.modeNameSubString});
				}
				if (criteria.courseName.has_value())
				{
					parameters.push_back({"course", criteria.courseName.value()});
				}
				if (criteria.maxTime >= 0.0f)
				{
					parameters.push_back({"maxtime", tfm::format("%g", criteria.maxTime)});
				}
				if (criteria.maxTeleports >= 0)
				{
					parameters.push_back({"maxteleports", std::to_string(criteria.maxTeleports)});
				}
				if (criteria.numStyles >= 0)
				{
					parameters.push_back({"numstyles", std::to_string(criteria.numStyles)});
				}
				break;
			}
			case RP_JUMPSTATS:
			{
				parameters.push_back({"type", "jump"});
				if (!criteria.modeNameSubString.empty())
				{
					parameters.push_back({"mode", criteria.modeNameSubString});
				}
				if (criteria.jumpType != static_cast<u8>(-1))
				{
					parameters.push_back({"jumptype", std::to_string(criteria.jumpType)});
				}
				if (criteria.minDistance > 0.0f)
				{
					parameters.push_back({"mindistance", tfm::format("%g", criteria.minDistance)});
				}
				break;
			}
			case RP_MANUAL:
			{
				parameters.push_back({"type", "manual"});
				if (criteria.savedBy.nameSubString.has_value())
				{
					parameters.push_back({"saver", criteria.savedBy.nameSubString.value()});
				}
				if (criteria.savedBy.steamID.has_value())
				{
					parameters.push_back({"saversteamid", std::to_string(criteria.savedBy.steamID.value())});
				}
				break;
			}
			case RP_CHEATER:
			{
				parameters.push_back({"type", "cheater"});
				if (criteria.reasonSubString.has_value())
				{
					parameters.push_back({"reason", criteria.reasonSubString.value()});
				}
				break;
			}
			default:
			{
				callback(false, 0, {});
				return;
			}
		}
		if (!criteria.mapName.empty() && criteria.mapName != "*")
		{
			parameters.push_back({"map", criteria.mapName});
		}
		if (criteria.player.nameSubString.has_value())
		{
			parameters.push_back({"player", criteria.player.nameSubString.value()});
		}
		if (criteria.player.steamID.has_value())
		{
			parameters.push_back({"steamid", std::to_string(criteria.player.steamID.value())});
		}
		parameters.push_back({"offset", std::to_string(offset)});
		parameters.push_back({"limit", std::to_string(limit)});
		SendQuery(parameters, callback);
	}

	void QueryReplaysByUUIDSubstring(const std::string &text, QueryCallback callback)
	{
		// Two results are enough to tell that the text isn't unique.
		SendQuery({{"idsubstring", text}, {"limit", "2"}}, callback);
	}

	void QueryReplaysByUUID(const std::vector<UUID_t> &uuids, QueryCallback callback)
	{
		std::vector<std::pair<std::string, std::string>> parameters;
		for (const UUID_t &uuid : uuids)
		{
			parameters.push_back({"id", uuid.ToString()});
		}
		parameters.push_back({"limit", std::to_string(uuids.size())});
		SendQuery(parameters, callback);
	}
} // namespace KZ::replaysystem::store

namespace KZ::replaysystem
{
	struct CachedAvailability
	{
		ReplayAvailability availability;
		f64 expires {};
	};

	static_global constexpr f64 availabilityCacheSeconds = 600.0;
	static_global constexpr u32 maxChecksInFlight = 4;
	static_global constexpr size_t maxQueryUUIDs = 50;
	static_global std::unordered_map<UUID_t, CachedAvailability> availabilityCache;
	// Callbacks of the replays being downloaded.
	static_global std::unordered_map<UUID_t, std::vector<std::function<void(const std::string &path)>>> fetches;

	// Misses aren't remembered because a replay can reach the store at any time.
	static_function void RememberReplay(const UUID_t &uuid, const std::string &map)
	{
		availabilityCache[uuid] = {{true, map}, Plat_FloatTime() + availabilityCacheSeconds};
	}

	struct AvailabilitySearch
	{
		std::unordered_map<UUID_t, ReplayAvailability> results;
		std::vector<std::pair<UUID_t, std::string>> queue;
		u32 inFlight {};
		bool done {};
		AvailabilityCallback callback;
	};

	// Checks the queued replays with HEAD requests a few at a time.
	static_function void CheckNextReplays(std::shared_ptr<AvailabilitySearch> search)
	{
		while (search->inFlight < maxChecksInFlight && !search->queue.empty())
		{
			const auto [uuid, map] = search->queue.back();
			search->queue.pop_back();
			search->inFlight++;
			HTTP::Request request(HTTP::Method::HEAD, store::FillURL(KZOptionService::GetOptionStr("replays/store/downloadUrl", ""), uuid,
																	 map.empty() ? store::GetCurrentMap() : map));
			store::SetAuthorization(request);
			auto onResponse = [search, uuid](HTTP::Response response)
			{
				search->inFlight--;
				if (response.status == 200)
				{
					search->results[uuid].found = true;
					RememberReplay(uuid, search->results[uuid].map);
				}
				CheckNextReplays(search);
			};
			auto onError = [search]()
			{
				search->inFlight--;
				CheckNextReplays(search);
			};
			request.Send(onResponse, onError);
		}
		if (search->queue.empty() && search->inFlight == 0 && !search->done)
		{
			search->done = true;
			search->callback(std::move(search->results));
		}
	}

	void LocateReplays(const std::vector<std::pair<UUID_t, std::string>> &replays, AvailabilityCallback callback)
	{
		auto search = std::make_shared<AvailabilitySearch>();
		search->callback = std::move(callback);
		const bool storeReadable = store::IsEnabled() && KZOptionService::GetOptionStr("replays/store/downloadUrl", "")[0] != '\0';
		const f64 now = Plat_FloatTime();
		for (const auto &[uuid, map] : replays)
		{
			std::string path;
			if (FindReplayPath(uuid.ToString().c_str(), path))
			{
				const auto header = g_ReplayWatcher.GetReplayHeader(uuid);
				search->results[uuid] = {true, header ? header->map().name() : std::string()};
				continue;
			}
			auto cached = availabilityCache.find(uuid);
			if (cached != availabilityCache.end() && cached->second.expires > now)
			{
				search->results[uuid] = cached->second.availability;
				continue;
			}
			search->results[uuid] = {};
			if (storeReadable)
			{
				search->queue.push_back({uuid, map});
			}
		}
		if (search->queue.empty() || !store::HasQueryEndpoint())
		{
			CheckNextReplays(search);
			return;
		}
		// The query endpoint answers for many replays at once and also returns their headers.
		while (!search->queue.empty())
		{
			std::vector<UUID_t> batch;
			while (!search->queue.empty() && batch.size() < maxQueryUUIDs)
			{
				batch.push_back(search->queue.back().first);
				search->queue.pop_back();
			}
			search->inFlight++;
			auto onResult = [search](bool, u64, std::vector<std::pair<UUID_t, ReplayHeader>> &&found)
			{
				search->inFlight--;
				for (const auto &[uuid, header] : found)
				{
					if (search->results.count(uuid))
					{
						search->results[uuid] = {true, header.map().name()};
						RememberReplay(uuid, header.map().name());
					}
				}
				CheckNextReplays(search);
			};
			store::QueryReplaysByUUID(batch, onResult);
		}
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
		if (url.find("{id}") == std::string::npos)
		{
			KZ_LOG_WARN(LogChannel::Global, "`replayDownloadUrl` (%s) has no {id} placeholder; cannot download replays.\n", url.c_str());
			FinishFetch(uuid, "");
			return;
		}
		DownloadReplay(uuid, store::FillURL(url, uuid, store::GetCurrentMap()), [uuid]() { FinishFetch(uuid, ""); });
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
		const std::string storeUrl = store::IsEnabled() ? std::string(KZOptionService::GetOptionStr("replays/store/downloadUrl", "")) : std::string();
		if (storeUrl.empty())
		{
			DownloadReplayFromAPI(uuid);
			return;
		}
		HTTP::Request request(HTTP::Method::GET, store::FillURL(storeUrl, uuid, store::GetCurrentMap()));
		store::SetAuthorization(request);
		auto onResponse = [uuid](HTTP::Response response)
		{
			std::optional<std::vector<char>> body = response.status == 200 ? response.RawBody() : std::nullopt;
			if (body.has_value() && !body->empty())
			{
				SaveDownload(uuid, std::move(*body));
				return;
			}
			DownloadReplayFromAPI(uuid);
		};
		request.Send(onResponse, [uuid]() { DownloadReplayFromAPI(uuid); });
	}

	void OnAPIUploadFinished(const UUID_t &uuid, bool uploaded)
	{
		bool uploadToStore = false;
		ledger::UpdateEntry(uuid,
							[&](ledger::Entry &entry)
							{
								if (!entry.uploadToAPI)
								{
									return;
								}
								entry.uploadToAPI = false;
								if (uploaded)
								{
									entry.offloaded = entry.offloaded || store::IsEnabled()
													  || KZOptionService::GetOptionInt("replays/disk/deleteGlobalRunsAfterUpload", false);
								}
								else
								{
									uploadToStore = !entry.uploadToStore && !KZOptionService::GetOptionInt("replays/store/includeGlobal", true);
								}
							});
		// A store that skips global runs still gets one the API doesn't have.
		if (uploadToStore)
		{
			store::QueueUpload(uuid);
		}
	}
} // namespace KZ::replaysystem
