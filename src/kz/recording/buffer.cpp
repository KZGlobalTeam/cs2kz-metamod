#include "kz_recording.h"
#include "kz/language/kz_language.h"
#include <unordered_set>
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
	u16 earliestWeaponIndex = 0;
	for (i32 i = 0; i < this->weaponChangeEvents->GetReadAvailable(); i++)
	{
		WeaponSwitchEvent data;
		if (!this->weaponChangeEvents->Peek(&data, 1, i))
		{
			break;
		}
		if (data.serverTick + 2 * 60 * 64 < currentTick)
		{
			earliestWeaponIndex = data.weaponIndex;
			numToRemove++;
			continue;
		}
		break;
	}
	this->weaponChangeEvents->Advance(numToRemove);

	// Rebuild weapon table from remaining events to avoid keeping unused weapons
	if (numToRemove > 0)
	{
		// Collect all unique weapon indices still in use
		std::unordered_set<u16> usedIndices;
		for (i32 i = 0; i < this->weaponChangeEvents->GetReadAvailable(); i++)
		{
			WeaponSwitchEvent *event = this->weaponChangeEvents->PeekSingle(i);
			if (event)
			{
				usedIndices.insert(event->weaponIndex);
			}
		}

		// Build new weapon table with only used weapons
		std::vector<EconInfo> newWeaponTable;
		std::unordered_map<u16, u16> oldToNewIndexMap;

		for (u16 oldIndex : usedIndices)
		{
			if (oldIndex < this->weaponTable.size())
			{
				u16 newIndex = static_cast<u16>(newWeaponTable.size());
				newWeaponTable.push_back(this->weaponTable[oldIndex]);
				oldToNewIndexMap[oldIndex] = newIndex;
			}
		}

		// Update weapon indices in the circular buffer
		for (i32 i = 0; i < this->weaponChangeEvents->GetReadAvailable(); i++)
		{
			WeaponSwitchEvent *event = this->weaponChangeEvents->PeekSingle(i);
			if (event)
			{
				auto it = oldToNewIndexMap.find(event->weaponIndex);
				if (it != oldToNewIndexMap.end())
				{
					event->weaponIndex = it->second;
				}
			}
		}

		// Rebuild the weapon index map
		this->weaponIndexMap.clear();
		for (size_t i = 0; i < newWeaponTable.size(); i++)
		{
			this->weaponIndexMap[newWeaponTable[i]] = static_cast<u16>(i);
		}

		// Replace the weapon table
		this->weaponTable = std::move(newWeaponTable);

		// Update earliestWeapon
		if (earliestWeaponIndex < this->weaponTable.size())
		{
			this->earliestWeapon = this->weaponTable[earliestWeaponIndex];
		}
	}
}

void CircularRecorder::TrimOldEvents(u32 currentTick)
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

	if (out_uuid != nullptr)
	{
		*out_uuid = recorder->uuid.ToString();
	}
	f32 replayDuration = recorder->tickData.size() * ENGINE_FIXED_TICK_INTERVAL;

	// Queue for async write
	if (s_fileWriter)
	{
		s_fileWriter->QueueWrite(std::move(recorder));
	}

	return replayDuration;
}

void KZRecordingService::WriteCircularBufferToFileAsync(f32 duration, const char *cheaterReason, KZPlayer *saver, WriteSuccessCallback onSuccess,
														WriteFailureCallback onFailure)
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

	// Queue for async write with callbacks
	if (s_fileWriter)
	{
		s_fileWriter->QueueWrite(std::move(recorder), onSuccess, onFailure);
	}
	else if (onFailure)
	{
		onFailure("File writer not initialized");
	}
}
