#include "kz_recording.h"
#include "kz/language/kz_language.h"
extern CConVar<bool> kz_replay_recording_debug;

// CircularRecorder method implementations
void CircularRecorder::TrimOldCommands(u32 currentTick)
{
	i32 numToRemove = 0;
	for (i32 i = 0; i < this->cmdData->GetReadAvailable(); i++)
	{
		CmdData data;
		if (!this->cmdData->Peek(&data, 1, i))
		{
			break;
		}
		if (data.serverTick + 2 * 60 * 64 < currentTick)
		{
			numToRemove++;
		}
		else
		{
			break;
		}
	}
	this->cmdData->Advance(numToRemove);
	this->cmdSubtickData->Advance(numToRemove);
}

void CircularRecorder::TrimOldWeaponEvents(u32 currentTick)
{
	i32 numToRemove = 0;
	for (i32 i = 0; i < this->weaponChangeEvents->GetReadAvailable(); i++)
	{
		WeaponSwitchEvent data;
		if (!this->weaponChangeEvents->Peek(&data, 1, i))
		{
			break;
		}
		if (data.serverTick + 2 * 60 * 64 < currentTick)
		{
			this->earliestWeapon = data.econInfo;
			numToRemove++;
			continue;
		}
		break;
	}
	this->weaponChangeEvents->Advance(numToRemove);
}

void CircularRecorder::TrimOldRpEvents(u32 currentTick)
{
	i32 numToRemove = 0;
	for (i32 i = 0; i < this->rpEvents->GetReadAvailable(); i++)
	{
		RpEvent event;
		if (!this->rpEvents->Peek(&event, 1, i))
		{
			break;
		}
		if (event.serverTick + 2 * 60 * 64 < currentTick)
		{
			switch (event.type)
			{
				case RPEVENT_MODE_CHANGE:
				{
					V_strncpy(this->earliestMode.value().name, event.data.modeChange.name, sizeof(this->earliestMode.value().name));
					V_strncpy(this->earliestMode.value().shortName, event.data.modeChange.shortName, sizeof(this->earliestMode.value().shortName));
					V_strncpy(this->earliestMode.value().md5, event.data.modeChange.md5, sizeof(this->earliestMode.value().md5));
					break;
				}
				case RPEVENT_STYLE_CHANGE:
				{
					if (event.data.styleChange.clearStyles)
					{
						this->earliestStyles.value().clear();
					}
					RpModeStyleInfo style = {};
					V_strncpy(style.name, event.data.styleChange.name, sizeof(style.name));
					V_strncpy(style.shortName, event.data.styleChange.shortName, sizeof(style.shortName));
					V_strncpy(style.md5, event.data.styleChange.md5, sizeof(style.md5));
					this->earliestStyles.value().push_back(style);
					break;
				}
				case RPEVENT_CHECKPOINT:
				{
					this->earliestCheckpointIndex = event.data.checkpoint.index;
					this->earliestCheckpointCount = event.data.checkpoint.checkpointCount;
					this->earliestTeleportCount = event.data.checkpoint.teleportCount;
					break;
				}
			}
			numToRemove++;
			continue;
		}
		break;
	}
	this->rpEvents->Advance(numToRemove);
}

void CircularRecorder::TrimOldJumps(u32 currentTick)
{
	i32 numToRemove = 0;
	for (i32 i = 0; i < this->jumps->GetReadAvailable(); i++)
	{
		RpJumpStats *jump = this->jumps->PeekSingle(i);
		if (!jump)
		{
			break;
		}
		if (jump->overall.serverTick + 2 * 60 * 64 < currentTick)
		{
			numToRemove++;
			continue;
		}
		break;
	}
	this->jumps->Advance(numToRemove);
}

f32 KZRecordingService::WriteCircularBufferToFile(f32 duration, const char *cheaterReason, std::string *out_uuid, KZPlayer *saver)
{
	std::unique_ptr<Recorder> recorder;
	if (strlen(cheaterReason) > 0)
	{
		recorder = std::make_unique<CheaterRecorder>(this->player, cheaterReason, saver);
	}
	else
	{
		recorder = std::make_unique<ManualRecorder>(this->player, duration, saver);
	}

	if (recorder->WriteToFile())
	{
		if (out_uuid != nullptr)
		{
			*out_uuid = recorder->uuid.ToString();
		}
		return recorder->tickData.size() * ENGINE_FIXED_TICK_INTERVAL;
	}
	return 0.0f;
}
