#include <algorithm>
#include <iterator>

#include "kz/global/kz_global.h"
#include "kz/replays/kz_replaysystem.h"
#include "kz/replays/ledger.h"
#include "utils/http.h"
#include "utils/utils.h"

using namespace KZ::replaysystem;

void KZGlobalService::ReplayManager::QueueUpload(PendingUpload &&upload)
{
	ledger::UpdateEntry(upload.recordID, [](ledger::Entry &entry) { entry.uploadToAPI = true; });
	std::lock_guard _guard(this->mutex);
	this->pendingUploads.push_back(std::move(upload));
}

void KZGlobalService::ReplayManager::RetryUpload(std::shared_ptr<PendingUpload> upload, const char *reason)
{
	if (upload->attempts >= maxUploadAttempts)
	{
		KZ_LOG_WARN(LogChannel::Global, "Failed to upload replay %s (%s); giving up after %u attempts.\n", upload->recordID.ToString().c_str(),
					reason, upload->attempts);
		OnAPIUploadFinished(upload->recordID, false);
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
		for (const PendingUpload &upload : uploadsToProcess)
		{
			OnAPIUploadFinished(upload.recordID, false);
		}
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
				OnAPIUploadFinished(upload->recordID, true);
				return;
			}

			if (response.status >= 500)
			{
				std::string reason = "status " + std::to_string(response.status);
				RetryUpload(upload, reason.c_str());
				return;
			}

			// A 4xx means the key is invalid, expired or already used and a retry cannot succeed.
			KZ_LOG_WARN(LogChannel::Global, "API rejected replay %s with status %d.\n", recordID.c_str(), response.status);
			OnAPIUploadFinished(upload->recordID, false);
		};

		request.Send(onResponse, [upload]() { RetryUpload(upload, "request failed"); });
	}
}
