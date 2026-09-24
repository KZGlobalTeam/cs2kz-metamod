#include <algorithm>
#include <iterator>

#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"
#include "kz/replays/kz_replay.h"
#include "kz/replays/data.h"
#include "kz/replays/bot.h"
#include "kz/replays/playback.h"
#include "utils/async_file_io.h"
#include "utils/http.h"
#include "utils/utils.h"
#include "filesystem.h"

using namespace KZ::replaysystem;

void KZGlobalService::ReplayManager::QueueUpload(PendingUpload &&upload)
{
	std::lock_guard _guard(this->mutex);
	this->pendingUploads.push_back(std::move(upload));
}

void KZGlobalService::ReplayManager::RetryUpload(std::shared_ptr<PendingUpload> upload, const char *reason)
{
	if (upload->attempts >= maxUploadAttempts)
	{
		KZ_LOG_WARN(LogChannel::Global, "Failed to upload replay %s (%s); giving up after %u attempts.\n",
					upload->recordID.ToString().c_str(), reason, upload->attempts);
		return;
	}

	KZ_LOG_WARN(LogChannel::Global, "Failed to upload replay %s (%s); retrying.\n", upload->recordID.ToString().c_str(), reason);
	upload->notBefore = std::chrono::steady_clock::now() + uploadRetryDelay;

	std::lock_guard _guard(KZGlobalService::replayManager.mutex);
	KZGlobalService::replayManager.pendingUploads.push_back(std::move(*upload));
}

void KZGlobalService::ReplayManager::ProcessUploads()
{
	std::vector<PendingUpload> uploadsToProcess;
	const auto now = std::chrono::steady_clock::now();

	{
		std::lock_guard _guard(this->mutex);

		if (this->pendingUploads.empty())
		{
			return;
		}

		// Uploads waiting out their retry delay stay queued.
		auto due = std::stable_partition(this->pendingUploads.begin(), this->pendingUploads.end(),
										 [now](const PendingUpload &upload) { return upload.notBefore > now; });
		std::move(due, this->pendingUploads.end(), std::back_inserter(uploadsToProcess));
		this->pendingUploads.erase(due, this->pendingUploads.end());
	}

	if (uploadsToProcess.empty())
	{
		return;
	}

	std::string apiUrl;

	if (!KZGlobalService::GetApiHttpUrl(apiUrl))
	{
		KZ_LOG_WARN(LogChannel::Global, "Dropping %zu replay upload(s) because `apiUrl` is invalid.\n", uploadsToProcess.size());
		return;
	}

	for (PendingUpload &pendingUpload : uploadsToProcess)
	{
		// Shared so the response and error callbacks can hand it back to the queue for a retry.
		auto upload = std::make_shared<PendingUpload>(std::move(pendingUpload));
		upload->attempts++;

		std::string recordID = upload->recordID.ToString();
		HTTP::Request request(HTTP::Method::POST, apiUrl + "records/" + recordID + "/replay");
		request.SetHeader("Authorization", "Bearer " + upload->uploadKey);
		request.SetBody(std::string(upload->replayData.begin(), upload->replayData.end()), "application/octet-stream");

		KZ_LOG_INFO(LogChannel::Global, "Uploading replay %s (%zu bytes, attempt %u)...\n", recordID.c_str(), upload->replayData.size(),
					upload->attempts);

		auto onResponse = [upload](HTTP::Response response)
		{
			std::string recordID = upload->recordID.ToString();

			if (response.status >= 200 && response.status < 300)
			{
				KZ_LOG_INFO(LogChannel::Global, "Uploaded replay %s.\n", recordID.c_str());
				return;
			}

			if (response.status >= 500)
			{
				std::string reason = "status " + std::to_string(response.status);
				RetryUpload(upload, reason.c_str());
				return;
			}

			// 4xx: the key is invalid, expired or already used, so a retry cannot succeed.
			KZ_LOG_WARN(LogChannel::Global, "API rejected replay %s with status %d.\n", recordID.c_str(), response.status);
		};

		request.Send(onResponse, [upload]() { RetryUpload(upload, "request failed"); });
	}
}

void KZGlobalService::ReplayManager::OnReplayRequestSuccess(const std::vector<char> &binaryData, CPlayerUserId userID)
{
	UUID_t replayID(false);
	{
		std::lock_guard _guard(KZGlobalService::replayManager.mutex);
		if (KZGlobalService::replayManager.pendingDownload.has_value())
		{
			replayID = KZGlobalService::replayManager.pendingDownload.value();
		}
		KZGlobalService::replayManager.pendingDownload.reset();
	}

	// Save the downloaded replay to disk asynchronously.
	if (replayID.IsV7())
	{
		char replayPath[512];
		V_snprintf(replayPath, sizeof(replayPath), "%s/%s.replay", KZ_REPLAY_DOWNLOADS_PATH, replayID.ToString().c_str());
		if (g_asyncFileIO)
		{
			g_asyncFileIO->QueueWriteBuffer(replayPath, binaryData);
		}
		else
		{
			utils::WriteBufferToFile(replayPath, binaryData);
		}
	}

	KZPlayer *requester = g_pKZPlayerManager->ToPlayer(userID);
	if (!requester || !requester->IsConnected())
	{
		return;
	}

	// clang-format off
	data::LoadReplayMemoryAsync(
		binaryData, replayID,
		data::LoadSuccessCallback([userID]()
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			if (!player || !player->IsConnected())
			{
				return;
			}
			if (!KZ_STREQI(data::GetCurrentReplay()->header.map().name().c_str(), g_pKZUtils->GetCurrentMapName().Get()))
			{
				player->languageService->PrintChat(true, false, "Replay - Wrong Map",
					data::GetCurrentReplay()->header.map().name().c_str(), g_pKZUtils->GetCurrentMapName().Get());
				return;
			}
			player->languageService->PrintChat(true, false, "Replay - Loaded Successfully");
			auto replay = data::GetCurrentReplay();
			bot::InitializeBotForReplay(replay->header);
			playback::StartReplay();
			playback::InitializeWeapons();
			bot::SpectateBot(player);
		}),
		data::LoadFailureCallback([userID](const char *error)
		{
			KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
			if (player)
			{
				player->languageService->PrintChat(true, false, error);
			}
		})
	);
	// clang-format on
}

void KZGlobalService::ReplayManager::RequestReplay(KZPlayer *requester, UUID_t replayID)
{
	if (!requester || !requester->IsConnected())
	{
		return;
	}
	if (pendingDownload.has_value())
	{
		requester->languageService->PrintChat(true, false, "Replay Request - Already Requested", pendingDownload->ToString().c_str());
		return;
	}
	auto userID = requester->GetClient()->GetUserID();

	std::string url = KZOptionService::GetOptionStr("replayDownloadUrl", "https://replays.cs2kz.org/{id}");
	const size_t idPos = url.find("{id}");
	if (idPos == std::string::npos)
	{
		KZ_LOG_WARN(LogChannel::Global, "`replayDownloadUrl` (%s) has no {id} placeholder; cannot download replays.\n", url.c_str());
		requester->languageService->PrintChat(true, false, "Replay Request - Error");
		return;
	}
	url.replace(idPos, sizeof("{id}") - 1, replayID.ToString());

	requester->languageService->PrintChat(true, false, "Replay Request - Sending", replayID.ToString().c_str());

	HTTP::Request request(HTTP::Method::GET, url);

	KZ_LOG_DEBUG(LogChannel::Global, "Requesting replay from %s.\n", url.c_str());

	auto doOnErrorCleanup = [userID]()
	{
		{
			std::lock_guard _guard(KZGlobalService::replayManager.mutex);
			KZGlobalService::replayManager.pendingDownload.reset();
		}
		KZPlayer *requester = g_pKZPlayerManager->ToPlayer(userID);
		if (!requester || !requester->IsConnected())
		{
			return;
		}

		requester->languageService->PrintChat(true, false, "Replay Request - Error");
	};

	auto onResponse = [userID, url, doOnErrorCleanup](HTTP::Response response)
	{
		KZ_LOG_DEBUG(LogChannel::Global, "Received response for replay %s: status %d.\n", url.c_str(), response.status);

		if (response.status != 200)
		{
			KZ_LOG_DEBUG(LogChannel::Global, "Non-200 response for replay %s: status %d.\n", url.c_str(), response.status);

			doOnErrorCleanup();

			return;
		}

		std::optional<std::vector<char>> body = response.RawBody();

		if (body.has_value())
		{
			KZGlobalService::ReplayManager::OnReplayRequestSuccess(*body, userID);
		}
	};

	request.Send(onResponse, doOnErrorCleanup);

	{
		std::lock_guard _guard(this->mutex);
		this->pendingDownload = replayID;
	}
}
