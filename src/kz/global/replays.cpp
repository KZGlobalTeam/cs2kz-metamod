#include "kz/global/kz_global.h"

bool KZGlobalService::ReplayManager::QueueUpload(const UUID_t &uploadID, std::vector<char> &&replayData)
{
	std::lock_guard _guard(this->mutex);
	this->pendingUploads.emplace_back(uploadID, std::move(replayData));
	return true;
}

void KZGlobalService::ReplayManager::ProcessUploads()
{
	std::vector<std::pair<UUID_t, std::vector<char>>> uploadsToProcess;

	{
		std::lock_guard _guard(this->mutex);
		uploadsToProcess = std::move(this->pendingUploads);
		this->pendingUploads.clear();
	}

	for (auto &[uploadID, replayData] : uploadsToProcess)
	{
		KZGlobalService::WS::SendMessageWithBinary(KZ::api::messages::NewReplay {uploadID.ToString()}, replayData);
	}
}
