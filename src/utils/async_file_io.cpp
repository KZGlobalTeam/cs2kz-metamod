#include "async_file_io.h"
#include "utils.h"
#include "filesystem.h"
#include "common.h"

AsyncFileIO *g_asyncFileIO = nullptr;

void AsyncFileIO::Init()
{
	g_asyncFileIO = new AsyncFileIO();
	g_asyncFileIO->Start();
}

void AsyncFileIO::Cleanup()
{
	g_asyncFileIO->Stop();
	delete g_asyncFileIO;
	g_asyncFileIO = nullptr;
}

// C++17 overloaded visitor helper
template<class... Ts>
struct overloaded_async : Ts...
{
	using Ts::operator()...;
};
template<class... Ts>
overloaded_async(Ts...) -> overloaded_async<Ts...>;

AsyncFileIO::~AsyncFileIO()
{
	Stop();
}

void AsyncFileIO::Start()
{
	if (m_thread)
	{
		return;
	}
	m_terminate = false;
	m_thread = std::make_unique<std::thread>(&AsyncFileIO::ThreadRun, this);
}

void AsyncFileIO::Stop()
{
	if (!m_thread)
	{
		return;
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
}

void AsyncFileIO::QueueRename(std::string oldPath, std::string newPath, RenameCallback onDone)
{
	RenameTask task;
	task.oldPath = std::move(oldPath);
	task.newPath = std::move(newPath);
	task.onDone = std::move(onDone);
	EnqueueTask(std::move(task));
}

void AsyncFileIO::QueueRead(std::string path, ReadCallback onRead)
{
	ReadTask task;
	task.path = std::move(path);
	task.onRead = std::move(onRead);
	EnqueueTask(std::move(task));
}

void AsyncFileIO::QueueWriteBuffer(std::string path, std::vector<char> buffer)
{
	RawWriteTask task;
	task.path = std::move(path);
	task.buffer = std::move(buffer);
	EnqueueTask(std::move(task));
}

void AsyncFileIO::EnqueueTask(AsyncAnyTask &&task)
{
	{
		std::lock_guard<std::mutex> lock(m_queueLock);
		m_taskQueue.push(std::move(task));
	}
	m_queueCV.notify_one();
}

void AsyncFileIO::RunFrame()
{
	std::queue<AsyncAnyResult> completed;
	{
		std::lock_guard<std::mutex> lock(m_completedLock);
		completed.swap(m_completedTasks);
	}

	while (!completed.empty())
	{
		std::visit(
			overloaded_async {
				[](RenameResult &r)
				{
					if (r.onDone)
					{
						r.onDone(r.success);
					}
				},
				[](ReadResult &r)
				{
					if (r.onRead)
					{
						r.onRead(r.success, std::move(r.buffer));
					}
				},
			},
			completed.front());
		completed.pop();
	}
}

void AsyncFileIO::ThreadRun()
{
	auto processTask = [&](AsyncAnyTask &anyTask)
	{
		std::visit(
			overloaded_async {
				[&](RenameTask &task)
				{
					bool ok = g_pFullFileSystem->RenameFile(task.oldPath.c_str(), task.newPath.c_str(), "GAME");
					if (!ok)
					{
						META_CONPRINTF("[KZ] Failed to rename file from %s to %s\n", task.oldPath.c_str(), task.newPath.c_str());
					}
					if (task.onDone)
					{
						RenameResult result;
						result.success = ok;
						result.onDone = std::move(task.onDone);
						std::lock_guard<std::mutex> lock(m_completedLock);
						m_completedTasks.push(std::move(result));
					}
				},
				[&](ReadTask &task)
				{
					ReadResult result;
					result.success = utils::ReadBufferFromFile(task.path.c_str(), result.buffer);
					result.onRead = std::move(task.onRead);
					std::lock_guard<std::mutex> lock(m_completedLock);
					m_completedTasks.push(std::move(result));
				},
				[&](RawWriteTask &task)
				{
					// Fire-and-forget: write buffer directly to disk, no result marshalled back.
					utils::WriteBufferToFile(task.path.c_str(), task.buffer);
				},
			},
			anyTask);
	};

	while (true)
	{
		AsyncAnyTask task;
		{
			std::unique_lock<std::mutex> lock(m_queueLock);
			m_queueCV.wait(lock, [this] { return m_terminate || !m_taskQueue.empty(); });

			if (m_terminate && m_taskQueue.empty())
			{
				break;
			}

			if (!m_taskQueue.empty())
			{
				task = std::move(m_taskQueue.front());
				m_taskQueue.pop();
			}
		}

		processTask(task);
	}

	// Flush remaining tasks on shutdown — disk writes only, skip reads
	while (!m_taskQueue.empty())
	{
		auto &front = m_taskQueue.front();
		if (auto *rt = std::get_if<RenameTask>(&front))
		{
			g_pFullFileSystem->RenameFile(rt->oldPath.c_str(), rt->newPath.c_str(), "GAME");
		}
		else if (auto *rwt = std::get_if<RawWriteTask>(&front))
		{
			utils::WriteBufferToFile(rwt->path.c_str(), rwt->buffer);
		}
		// ReadTask: skip on shutdown
		m_taskQueue.pop();
	}
}
