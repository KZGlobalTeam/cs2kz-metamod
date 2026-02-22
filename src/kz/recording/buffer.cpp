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
	for (const auto &jump : this->jumps)
	{
		if (jump.overall.serverTick + 2 * 60 * 64 < currentTick)
		{
			numToRemove++;
		}
		else
		{
			break;
		}
	}

	// Remove old jumps from front of deque
	if (numToRemove > 0)
	{
		this->jumps.erase(this->jumps.begin(), this->jumps.begin() + numToRemove);
	}
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

	// Copy weapons before queuing to another thread
	this->CopyWeaponsToRecorder(recorder.get());

	// Queue for async write with callbacks
	if (fileWriter)
	{
		fileWriter->QueueWrite(std::move(recorder), onSuccess, onFailure);
	}
	else if (onFailure)
	{
		onFailure("File writer not initialized");
	}
}
