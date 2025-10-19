#include "cs2kz.h"
#include "kz/kz.h"
#include "kz_replaysystem.h"
#include "kz_replaybot.h"
#include "kz_replaydata.h"
#include "kz_replayplayback.h"
#include "kz_replayevents.h"
#include "kz_replaycommands.h"

namespace KZ::replaysystem
{

	void Init()
	{
		KZ::replaysystem::item::InitItemAttributes();
	}

	void Cleanup()
	{
		bot::KickBot();
	}

	void OnRoundStart()
	{
		bot::KickBot();
	}

	void OnGameFrame()
	{
		bot::CheckBots();
		// Process any completed async loads on the main thread
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

		if (KZ::mapapi::IsTimerTrigger(kzTrigger->type))
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

} // namespace KZ::replaysystem
