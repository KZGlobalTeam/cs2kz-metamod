#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <variant>
#include <functional>
#include <string>
#include <vector>
#include <memory>

// ---------------------------------------------------------------------------
// Callback types
// ---------------------------------------------------------------------------

using RenameCallback = std::function<void(bool success)>;
using ReadCallback = std::function<void(bool success, std::vector<char> &&buffer)>;

// ---------------------------------------------------------------------------
// Task types
// ---------------------------------------------------------------------------

struct RenameTask
{
	std::string oldPath;
	std::string newPath;
	RenameCallback onDone; // may be nullptr
};

struct ReadTask
{
	std::string path;
	ReadCallback onRead;
};

// Fire-and-forget: write a raw buffer to a path on the background thread.
struct RawWriteTask
{
	std::string path;
	std::vector<char> buffer;
};

using AsyncAnyTask = std::variant<RenameTask, ReadTask, RawWriteTask>;

// ---------------------------------------------------------------------------
// Result types — marshalled back to the main thread via RunFrame()
// ---------------------------------------------------------------------------

struct RenameResult
{
	bool success;
	RenameCallback onDone;
};

struct ReadResult
{
	bool success;
	std::vector<char> buffer;
	ReadCallback onRead;
};

using AsyncAnyResult = std::variant<RenameResult, ReadResult>;

// ---------------------------------------------------------------------------
// AsyncFileIO — single background thread for generic file operations.
// Lifecycle (Start/Stop) is owned by cs2kz.cpp.
// RunFrame() must be called once per game frame from the main thread.
// ---------------------------------------------------------------------------

class AsyncFileIO
{
public:
	AsyncFileIO() = default;
	~AsyncFileIO();

	static void Init();
	static void Cleanup();

	void Start();
	void Stop();

	// Queue a file rename on the bg thread; onDone (optional) is called on the main thread
	void QueueRename(std::string oldPath, std::string newPath, RenameCallback onDone = nullptr);

	// Queue a file read on the bg thread; onRead is called on the main thread with the loaded buffer
	void QueueRead(std::string path, ReadCallback onRead);

	// Queue a raw buffer write on the bg thread (fire-and-forget, no callback)
	void QueueWriteBuffer(std::string path, std::vector<char> buffer);

	// Process completed tasks and invoke their callbacks — call once per game frame from the main thread
	void RunFrame();

private:
	void EnqueueTask(AsyncAnyTask &&task);
	void ThreadRun();

	std::unique_ptr<std::thread> m_thread;
	std::queue<AsyncAnyTask> m_taskQueue;
	std::queue<AsyncAnyResult> m_completedTasks;
	std::mutex m_queueLock;
	std::mutex m_completedLock;
	std::condition_variable m_queueCV;
	bool m_terminate = false;
};

extern AsyncFileIO *g_asyncFileIO;
