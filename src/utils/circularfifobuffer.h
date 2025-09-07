#include "circularbuffer.h"

template<typename T, size_t SIZE>
struct CFIFOCircularBuffer
{
	CFIFOCircularBuffer()
	{
		this->buffer = new CCircularBuffer(SIZE * sizeof(T));
	}

	~CFIFOCircularBuffer()
	{
		delete this->buffer;
	}

	void Resize(size_t newSize)
	{
		this->buffer->Resize(newSize * sizeof(T));
	}

	void Write(const T &data)
	{
		this->buffer->Write(&data, sizeof(T));
	}

	int Peek(T *out, size_t count = 1, int offset = 0)
	{
		if (offset < 0 || offset >= this->GetReadAvailable() || !out)
		{
			return 0;
		}
		T *buffer = new T[offset + count];
		i32 numElements = this->buffer->Peek(buffer, (offset + count) * sizeof(T)) / sizeof(T);
		if (numElements)
		{
			V_memcpy(out, buffer + offset, (numElements - offset) * sizeof(T));
			delete[] buffer;
			return numElements - offset;
		}
		delete[] buffer;
		return 0;
	}

	bool Read(T *out)
	{
		if (!Peek(out))
		{
			return false;
		}

		Advance(1);
		return true;
	}

	size_t Advance(size_t count)
	{
		return this->buffer->Advance(count * sizeof(T)) / sizeof(T);
	}

	size_t GetReadAvailable() const
	{
		return this->buffer->GetReadAvailable() / sizeof(T);
	}

	size_t GetWriteAvailable() const
	{
		return this->buffer->GetWriteAvailable() / sizeof(T);
	}

	CCircularBuffer *buffer;
};
