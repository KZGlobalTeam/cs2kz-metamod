#include "kz/global/kz_global.h"
#include "kz/language/kz_language.h"
#include "kz/replays/kz_replay.h"
#include "kz/replays/data.h"
#include "kz/replays/bot.h"
#include "kz/replays/playback.h"
#include "utils/async_file_io.h"
#include "utils/utils.h"
#include "filesystem.h"

using namespace KZ::replaysystem;

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

void KZGlobalService::ReplayManager::OnReplayRequestSuccess(const KZ::api::messages::ReplayData &ack, const std::vector<char> &binaryData,
															CPlayerUserId userID)
{
	UUID_t replayID;
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
		g_pFullFileSystem->CreateDirHierarchy(KZ_REPLAY_DOWNLOADS_PATH, "GAME");
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

	requester->languageService->PrintChat(true, false, "Replay Request - Sending", replayID.ToString().c_str());
	KZGlobalService::MessageCallback<KZ::api::messages::ReplayData, true> callback(&KZGlobalService::ReplayManager::OnReplayRequestSuccess, userID);
	callback.OnError(
		[userID](const KZ::api::messages::Error &error)
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

			requester->languageService->PrintChat(true, false, "Replay Request - Error", error.message.c_str());
		});
	callback.OnCancelled(
		[userID](KZGlobalService::MessageCallbackCancelReason)
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

			requester->languageService->PrintChat(true, false, "Replay Request - Cancelled");
		});
	// A replay should not take over two minutes to download... hopefully.
	callback.Timeout(std::chrono::seconds(120));

	{
		std::lock_guard _guard(this->mutex);
		this->pendingDownload = replayID;
	}
	KZGlobalService::WS::SendMessage(KZ::api::messages::WantReplay {replayID.ToString()});
}
