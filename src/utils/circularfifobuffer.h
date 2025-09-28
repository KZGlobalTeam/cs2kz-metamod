#include "common.h"
#include <memory>
#include <algorithm>
#include <cstring>
#include "filesystem.h"
#include "vprof.h"

template<typename T, size_t SIZE>
struct CFIFOCircularBuffer
{
private:
	std::unique_ptr<T[]> buffer;
	size_t capacity;
	size_t readPos;
	size_t writePos;
	size_t count; // Number of elements currently in buffer

public:
	CFIFOCircularBuffer() : capacity(SIZE), readPos(0), writePos(0), count(0)
	{
		buffer = std::make_unique<T[]>(capacity);
	}

	~CFIFOCircularBuffer()
	{
		// Destructor for elements with non-trivial destructors
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			Clear();
		}
	}

	// Resize the buffer to hold newSize elements of type T
	void Resize(size_t newSize)
	{
		if (newSize == capacity)
		{
			return; // No change needed
		}

		auto newBuffer = std::make_unique<T[]>(newSize);
		size_t elementsToMove = std::min(count, newSize);

		// Copy existing elements to new buffer
		for (size_t i = 0; i < elementsToMove; ++i)
		{
			size_t srcIndex = (readPos + i) % capacity;
			if constexpr (std::is_move_constructible_v<T>)
			{
				new (&newBuffer[i]) T(std::move(buffer[srcIndex]));
			}
			else
			{
				new (&newBuffer[i]) T(buffer[srcIndex]);
			}
		}

		// Clean up old buffer if necessary
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			for (size_t i = 0; i < count; ++i)
			{
				size_t index = (readPos + i) % capacity;
				buffer[index].~T();
			}
		}

		buffer = std::move(newBuffer);
		capacity = newSize;
		readPos = 0;
		writePos = elementsToMove % capacity;
		count = elementsToMove;
	}

	// Write a single element of type T to the buffer
	void Write(const T &data)
	{
		buffer[writePos] = data;
		writePos = (writePos + 1) % capacity;

		if (count < capacity)
		{
			++count;
		}
		else
		{
			// Buffer is full, advance read position (overwrite oldest)
			readPos = (readPos + 1) % capacity;
		}
	}

	// Peek at up to 'count' elements of type T from the buffer, starting at 'offset'
	// Returns the number of elements actually read
	int Peek(T *out, size_t count = 1, int offset = 0)
	{
		if (!out || offset < 0 || static_cast<size_t>(offset) >= this->count)
		{
			return 0;
		}

		size_t availableFromOffset = this->count - static_cast<size_t>(offset);
		size_t elementsToPeek = min(count, availableFromOffset);

		for (size_t i = 0; i < elementsToPeek; ++i)
		{
			size_t index = (readPos + offset + i) % capacity;
			out[i] = buffer[index];
		}

		return static_cast<int>(elementsToPeek);
	}

	// Read and consume a single element of type T from the buffer
	// Returns true if an element was read, false otherwise
	bool Read(T *out)
	{
		if (count == 0 || !out)
		{
			return false;
		}

		*out = buffer[readPos];
		readPos = (readPos + 1) % capacity;
		--count;

		return true;
	}

	// Advance the read position by 'count' elements of type T
	// Returns the number of elements actually advanced
	size_t Advance(size_t count)
	{
		size_t elementsToAdvance = min(count, this->count);
		readPos = (readPos + elementsToAdvance) % capacity;
		this->count -= elementsToAdvance;
		return elementsToAdvance;
	}

	// Gets the number of elements of type T currently available for reading
	size_t GetReadAvailable() const
	{
		return count;
	}

	// Gets the number of elements of type T that can be written without overwriting unread data
	size_t GetWriteAvailable() const
	{
		return capacity - count;
	}

	// Write the n latest elements from the buffer to a file
	// Returns the number of elements actually written
	size_t WriteToFile(IFileSystem *fs, FileHandle_t file, size_t n) const
	{
		if (count == 0 || n == 0)
		{
			return 0;
		}

		size_t elementsToWrite = min(n, count);
		size_t elementsWritten = 0;

		// Calculate starting position for the n latest elements
		// Latest elements are those closest to writePos
		size_t startOffset = count - elementsToWrite;

		for (size_t i = 0; i < elementsToWrite; ++i)
		{
			size_t index = (readPos + startOffset + i) % capacity;

			// Use the external Write function
			if (fs->Write(&buffer[index], sizeof(T), file))
			{
				++elementsWritten;
			}
			else
			{
				break; // Stop on write error
			}
		}

		return elementsWritten;
	}

private:
	// Helper method to clear all elements
	void Clear()
	{
		if constexpr (!std::is_trivially_destructible_v<T>)
		{
			for (size_t i = 0; i < count; ++i)
			{
				size_t index = (readPos + i) % capacity;
				buffer[index].~T();
			}
		}
		count = 0;
		readPos = 0;
		writePos = 0;
	}
};
