#include "kz_racing.h"
extern CSteamGameServerAPIContext g_steamAPI;

void KZRacingService::CheckMap()
{
	if (!KZRacingService::currentRace.state)
	{
		return;
	}

	if (KZRacingService::IsMapCorrectForRace())
	{
		return;
	}
	else if (KZRacingService::IsMapReadyForChange(KZRacingService::currentRace.data.raceInfo.workshopID))
	{
		// host_workshop_map <workshop ID>
		std::string command = "host_workshop_map " + std::to_string(KZRacingService::currentRace.data.raceInfo.workshopID);
		interfaces::pEngine->ServerCommand(command.c_str());
	}
	else if (!KZRacingService::IsMapQueuedForDownload(KZRacingService::currentRace.data.raceInfo.workshopID))
	{
		KZRacingService::TriggerWorkshopDownload(KZRacingService::currentRace.data.raceInfo.workshopID);
	}
}

bool KZRacingService::IsMapCorrectForRace()
{
	u32 currentWorkshopID = g_pKZUtils->GetCurrentMapWorkshopID();
	return currentWorkshopID == KZRacingService::currentRace.data.raceInfo.workshopID;
}

bool KZRacingService::IsMapReadyForChange(u64 workshopID)
{
	// Make sure it is installed, doesn't need update, and isn't in downloading state.
	auto state = g_steamAPI.SteamUGC()->GetItemState(workshopID);
	return (state & (k_EItemStateInstalled | k_EItemStateNeedsUpdate | k_EItemStateDownloading | k_EItemStateDownloadPending))
		   == k_EItemStateInstalled;
}

bool KZRacingService::IsMapQueuedForDownload(u64 workshopID)
{
	return g_steamAPI.SteamUGC()->GetItemState(workshopID) & (k_EItemStateDownloading | k_EItemStateDownloadPending);
}

void KZRacingService::TriggerWorkshopDownload(u64 workshopID)
{
	g_steamAPI.SteamUGC()->DownloadItem(workshopID, true);
}

void KZRacingService::ItemDownloadHandler::OnAddonDownloaded(DownloadItemResult_t *pParam)
{
	if (pParam->m_eResult != k_EResultOK)
	{
		META_CONPRINTF("[KZ::Racing] Failed to download workshop item %llu, error code: %d\n", pParam->m_nPublishedFileId, pParam->m_eResult);
		return;
	}

	META_CONPRINTF("[KZ::Racing] Successfully downloaded workshop item %llu\n", pParam->m_nPublishedFileId);
	KZRacingService::CheckMap();
}
