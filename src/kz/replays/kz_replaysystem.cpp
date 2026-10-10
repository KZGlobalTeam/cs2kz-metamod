#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_replaysystem.h"
#include "bot.h"
#include "data.h"
#include "playback.h"
#include "events.h"
#include "commands.h"
#include "kz_replay.h"
#include "utils/uuid.h"
#include "filesystem.h"

#include <algorithm>

namespace KZ::replaysystem
{
	static_global std::vector<ReplayEventListener *> replayEventListeners;

	bool RegisterReplayEventListener(ReplayEventListener *listener)
	{
		if (!listener || std::find(replayEventListeners.begin(), replayEventListeners.end(), listener) != replayEventListeners.end())
		{
			return false;
		}
		replayEventListeners.push_back(listener);
		return true;
	}

	bool UnregisterReplayEventListener(ReplayEventListener *listener)
	{
		const auto found = std::find(replayEventListeners.begin(), replayEventListeners.end(), listener);
		if (found == replayEventListeners.end())
		{
			return false;
		}
		replayEventListeners.erase(found);
		return true;
	}

	void NotifyReplayFileChanged(const char *uuid)
	{
		if (!uuid || !UUID_t::FromString(uuid))
		{
			return;
		}
		// A listener may unsubscribe while handling the notification.
		const auto listeners = replayEventListeners;
		for (ReplayEventListener *listener : listeners)
		{
			if (std::find(replayEventListeners.begin(), replayEventListeners.end(), listener) != replayEventListeners.end())
			{
				listener->OnReplayFileChanged(uuid);
			}
		}
	}

	bool FindReplayPath(const char *uuid, std::string &path)
	{
		path.clear();
		UUID_t parsed(false);
		if (!uuid || !g_pFullFileSystem || !UUID_t::FromString(uuid, &parsed))
		{
			return false;
		}
		const std::string filename = parsed.ToString() + ".replay";
		for (const char *directory : {KZ_REPLAY_PATH, KZ_REPLAY_DOWNLOADS_PATH})
		{
			const std::string candidate = std::string(directory) + "/" + filename;
			if (g_pFullFileSystem->FileExists(candidate.c_str()))
			{
				path = candidate;
				return true;
			}
		}
		return false;
	}

	void Init()
	{
		KZ::replaysystem::item::InitItemAttributes();
	}

	void Cleanup()
	{
		bot::KickBot();
		CleanupWatcher();
		data::Shutdown();
	}

	void OnRoundStart()
	{
		bot::KickBot();
	}

	void OnGameFrame()
	{
		data::ProcessAsyncLoadCompletion();
	}

	void OnPhysicsSimulate(KZPlayer *player)
	{
		playback::OnPhysicsSimulate(player);
	}

	void OnProcessMovement(KZPlayer *player)
	{
		playback::OnProcessMovement(player);
	}

	void OnProcessMovementPost(KZPlayer *player)
	{
		playback::OnProcessMovementPost(player);
	}

	void OnFinishMovePre(KZPlayer *player, CMoveData *pMoveData)
	{
		playback::OnFinishMovePre(player, pMoveData);
	}

	void OnPhysicsSimulatePost(KZPlayer *player)
	{
		playback::OnPhysicsSimulatePost(player);
	}

	void OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command)
	{
		playback::OnPlayerRunCommandPre(player, command);
	}

	bool IsReplayBot(KZPlayer *player)
	{
		return bot::IsValidBot(player ? player->GetController() : nullptr);
	}

	bool CanTouchTrigger(KZPlayer *player, CBaseTrigger *trigger)
	{
		// Don't care about non-bot players.
		if (!bot::IsValidBot(player->GetController()))
		{
			return true;
		}

		// Don't care about non timer triggers.
		const KzTrigger *kzTrigger = KZ::mapapi::GetKzTrigger(trigger);
		if (!kzTrigger)
		{
			return true;
		}

		if (KZ::mapapi::IsTimerTrigger(kzTrigger->type) || KZ::mapapi::IsTeleportTrigger(kzTrigger->type))
		{
			return false;
		}

		return true;
	}

	i32 GetCurrentCpIndex()
	{
		return data::GetCurrentCpIndex();
	}

	i32 GetCheckpointCount()
	{
		return data::GetCheckpointCount();
	}

	i32 GetTeleportCount()
	{
		return data::GetTeleportCount();
	}

	f32 GetTime()
	{
		return data::GetReplayTime();
	}

	f32 GetEndTime()
	{
		return data::GetEndTime();
	}

	bool GetPaused()
	{
		return data::GetPaused();
	}

	const char *GetCourseName()
	{
		return data::GetCourseName();
	}

} // namespace KZ::replaysystem
