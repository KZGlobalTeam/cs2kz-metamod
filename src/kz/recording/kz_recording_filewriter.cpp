#include "kz_recording.h"
#include "filesystem.h"
#include "cs2kz.h"

extern CConVar<bool> kz_replay_recording_debug;

ReplayFileWriter::ReplayFileWriter() {}

ReplayFileWriter::~ReplayFileWriter()
{
	Stop();
}

void ReplayFileWriter::Start()
{
	if (m_thread)
	{
		return; // Already started
	}
	m_terminate = false;
	m_thread = std::make_unique<std::thread>(&ReplayFileWriter::ThreadRun, this);
	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: File writer thread started\n");
	}
}

void ReplayFileWriter::Stop()
{
	if (!m_thread)
	{
		return; // Not running
	}

	{
		std::lock_guard<std::mutex> lock(m_queueLock);
		m_terminate = true;
	}
	m_queueCV.notify_one();

	if (m_thread->joinable())
	{
		m_thread->join();
	}
	m_thread.reset();

	if (kz_replay_recording_debug.Get())
	{
		META_CONPRINTF("kz_replay_recording_debug: File writer thread stopped\n");
	}
}

void ReplayFileWriter::QueueWrite(std::unique_ptr<Recorder> recorder)
{
	WriteTask task;
	task.recorder = std::move(recorder);
	task.onSuccess = nullptr;
	task.onFailure = nullptr;

	{
		std::lock_guard<std::mutex> lock(m_queueLock);
		m_writeQueue.push(std::move(task));
	}
	m_queueCV.notify_one();
}

void ReplayFileWriter::QueueWrite(std::unique_ptr<Recorder> recorder, WriteSuccessCallback onSuccess, WriteFailureCallback onFailure)
{
	WriteTask task;
	task.recorder = std::move(recorder);
	task.onSuccess = onSuccess;
	task.onFailure = onFailure;

	{
		std::lock_guard<std::mutex> lock(m_queueLock);
		m_writeQueue.push(std::move(task));
	}
	m_queueCV.notify_one();
}

void ReplayFileWriter::RunFrame()
{
	// Process completed writes and invoke callbacks on main thread
	std::queue<WriteResult> completedWrites;
	{
		std::lock_guard<std::mutex> lock(m_completedLock);
		completedWrites.swap(m_completedWrites);
	}

	while (!completedWrites.empty())
	{
		WriteResult &result = completedWrites.front();

		if (result.success && result.onSuccess)
		{
			result.onSuccess(result.uuid, result.duration);
		}
		else if (!result.success && result.onFailure)
		{
			result.onFailure(result.errorMessage.c_str());
		}

		completedWrites.pop();
	}
}

void ReplayFileWriter::ThreadRun()
{
	while (true)
	{
		WriteTask task;

		{
			std::unique_lock<std::mutex> lock(m_queueLock);
			m_queueCV.wait(lock, [this] { return m_terminate || !m_writeQueue.empty(); });

			if (m_terminate && m_writeQueue.empty())
			{
				break;
			}

			if (!m_writeQueue.empty())
			{
				task = std::move(m_writeQueue.front());
				m_writeQueue.pop();
			}
		}

		if (task.recorder)
		{
			WriteResult result;
			result.uuid = task.recorder->uuid;
			result.duration = task.recorder->tickData.size() * ENGINE_FIXED_TICK_INTERVAL;
			result.onSuccess = task.onSuccess;
			result.onFailure = task.onFailure;

			bool writeSuccess = task.recorder->WriteToFile();
			result.success = writeSuccess;

			if (!writeSuccess)
			{
				result.errorMessage = "Failed to write replay file";
			}

			// Only queue result if there are callbacks
			if (result.onSuccess || result.onFailure)
			{
				std::lock_guard<std::mutex> lock(m_completedLock);
				m_completedWrites.push(std::move(result));
			}
		}
	}

	// Process any remaining items in the queue before exiting
	while (!m_writeQueue.empty())
	{
		auto task = std::move(m_writeQueue.front());
		m_writeQueue.pop();
		if (task.recorder)
		{
			task.recorder->WriteToFile();
		}
	}
}
