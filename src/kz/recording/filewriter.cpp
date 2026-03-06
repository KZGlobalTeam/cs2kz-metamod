#include "kz_recording.h"
#include "filesystem.h"
#include "cs2kz.h"
#include "utils/utils.h"

extern CConVar<bool> kz_replay_recording_debug;

ReplayFileWriter::ReplayFileWriter() {}

ReplayFileWriter::~ReplayFileWriter()
{
	Stop();
}

void ReplayFileWriter::Stop()
{
	std::unique_lock<std::mutex> lock(m_shutdownMutex);
	m_shutdownCV.wait(lock, [this] { return m_activeThreads == 0; });
}

template<typename F>
void ReplayFileWriter::SpawnThread(F &&work)
{
	m_activeThreads++;
	std::thread(
		[this, work = std::forward<F>(work)]() mutable
		{
			work();
			if (--m_activeThreads == 0)
			{
				m_shutdownCV.notify_all();
			}
		})
		.detach();
}

void ReplayFileWriter::QueueWrite(std::unique_ptr<Recorder> recorder, BufferSuccessCallback onSuccess, WriteFailureCallback onFailure)
{
	SpawnThread(
		[this, rec = std::move(recorder), onSuccess = std::move(onSuccess), onFailure = std::move(onFailure)]()
		{
			if (!rec)
			{
				return;
			}
			UUID_t uuid = rec->uuid;
			f32 duration = rec->tickData.size() * ENGINE_FIXED_TICK_INTERVAL;
			std::vector<char> buffer;
			bool ok = rec->WriteToMemory(buffer);
			std::lock_guard<std::mutex> lock(m_completedLock);
			if (ok)
			{
				m_completedCallbacks.push(
					[uuid, duration, buf = std::move(buffer), onSuccess]() mutable
					{
						if (onSuccess)
						{
							onSuccess(uuid, duration, std::move(buf));
						}
					});
			}
			else
			{
				m_completedCallbacks.push(
					[onFailure]()
					{
						if (onFailure)
						{
							onFailure("Failed to serialize replay to memory");
						}
					});
			}
		});
}

void ReplayFileWriter::QueueWriteToFile(std::unique_ptr<Recorder> recorder, DiskWriteSuccessCallback onSuccess, WriteFailureCallback onFailure)
{
	SpawnThread(
		[this, rec = std::move(recorder), onSuccess = std::move(onSuccess), onFailure = std::move(onFailure)]()
		{
			if (!rec)
			{
				return;
			}
			UUID_t uuid = rec->uuid;
			f32 duration = rec->tickData.size() * ENGINE_FIXED_TICK_INTERVAL;
			bool ok = rec->WriteToFile();
			if (!onSuccess && !onFailure)
			{
				return;
			}
			std::lock_guard<std::mutex> lock(m_completedLock);
			if (ok)
			{
				m_completedCallbacks.push(
					[uuid, duration, onSuccess]()
					{
						if (onSuccess)
						{
							onSuccess(uuid, duration);
						}
					});
			}
			else
			{
				m_completedCallbacks.push(
					[onFailure]()
					{
						if (onFailure)
						{
							onFailure("Failed to write replay to disk");
						}
					});
			}
		});
}

void ReplayFileWriter::RunFrame()
{
	std::queue<std::function<void()>> pending;
	{
		std::lock_guard<std::mutex> lock(m_completedLock);
		pending.swap(m_completedCallbacks);
	}
	while (!pending.empty())
	{
		pending.front()();
		pending.pop();
	}
}
