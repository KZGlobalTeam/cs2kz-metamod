#include "kz/replay/kz_replay.h"
#include "kz_replay.h"

// ====================================

CRRIterator CircularReplayRecorder::Iterate() const
{
	CRRIterator iter;
	iter.ctx = this;
	iter.currentTickIndex = this->tickData[this->nextTickIndex].HasSubtickMoves() ? this->nextTickIndex : 0;
	iter.ticksLeft = this->maxTicks;
	iter.currentSubtickMoveIndex = (this->nextSubtickMoveIndex + this->emptySubtickMoveSlots) % this->maxSubtickMoves;
	iter.forward = true;
	return iter;
}

CRRIterator CircularReplayRecorder::IterateReverse() const
{
	CRRIterator iter;
	iter.ctx = this;
	iter.currentTickIndex = (this->nextTickIndex + this->maxTicks - 1) % this->maxTicks;
	iter.ticksLeft = this->maxTicks;
	iter.currentSubtickMoveIndex =
		(this->nextSubtickMoveIndex + this->maxSubtickMoves - this->tickData[iter.currentTickIndex].numSubtickMoves) % this->maxSubtickMoves;
	iter.forward = false;
	return iter;
}

void CircularReplayRecorder::CheckEvents()
{
	// Clear all the events from the head where the event tick is older than server tick count plus the buffer size.
	while (!this->events.empty() && this->events.front().serverTick < g_pKZUtils->GetServerGlobals()->tickcount - this->maxTicks)
	{
		this->events.pop_front();
	}
}

// ====================================

void CircularReplayRecorder::Init()
{
	this->nextTickIndex = 0;

	this->maxSubtickMoves = maxTicks * 2;
	this->subtickMoves = new SubtickMoveLite[this->maxSubtickMoves] {};
	this->nextSubtickMoveIndex = 0;
	this->emptySubtickMoveSlots = this->maxSubtickMoves;
}

void CircularReplayRecorder::Deinit()
{
	delete[] this->tickData;
	delete[] this->subtickMoves;
}

void CircularReplayRecorder::Push(const TickData &tickData, const SubtickMoveLite *moves)
{
	// Free the subtick moves that the tickData we're overriding used.
	u16 prevNumSubtickMoves = this->tickData[this->nextTickIndex].numSubtickMoves;
	this->emptySubtickMoveSlots += prevNumSubtickMoves;

	if (tickData.numSubtickMoves > this->emptySubtickMoveSlots)
	{
		// Increase the size of the subtick moves array to the next multiple of maxTicks plus the number of subtick moves of this tickData.
		u32 newMaxMoves = tickData.numSubtickMoves + (this->maxTicks * ((this->maxSubtickMoves / this->maxTicks) + 1));
		SubtickMoveLite *newSubtickMoves = new SubtickMoveLite[newMaxMoves] {};
		u32 newNextMoveIndex = 0;

		// Figure out where the index of the first move in the buffer actually is.
		u32 firstMoveIndex = (this->nextSubtickMoveIndex + this->emptySubtickMoveSlots) % this->maxSubtickMoves;
		// Initialize the number of move to copy with total amount of subtick moves we have in the buffer.
		u32 movesLeftToCopy = (this->maxSubtickMoves - this->emptySubtickMoveSlots);
		// Copy all the subtick moves to the right to the start of the new move buffer.
		if (movesLeftToCopy != 0)
		{
			u32 movesToTheRight = (this->maxSubtickMoves - firstMoveIndex);
			u32 movesToCopy = MIN(movesToTheRight, movesLeftToCopy);
			memcpy(newSubtickMoves, this->subtickMoves + firstMoveIndex, movesToCopy * sizeof(SubtickMove));
			movesLeftToCopy -= movesToCopy;
			newNextMoveIndex += movesToCopy;
		}
		// Copy the rest of the subtick moves to the new move buffer.
		if (movesLeftToCopy != 0)
		{
			u32 movesToCopy = firstMoveIndex;
			assert(movesToCopy == movesLeftToCopy);
			memcpy(newSubtickMoves + newNextMoveIndex, this->subtickMoves, movesToCopy * sizeof(SubtickMove));
			movesLeftToCopy -= movesToCopy;
			newNextMoveIndex += movesToCopy;
		}

		delete[] this->subtickMoves;

		this->maxSubtickMoves = newMaxMoves;
		this->subtickMoves = newSubtickMoves;
		this->nextSubtickMoveIndex = newNextMoveIndex;
		this->emptySubtickMoveSlots = this->maxSubtickMoves - newNextMoveIndex;
	}

	this->emptySubtickMoveSlots -= tickData.numSubtickMoves;
	for (u16 i = 0; i < tickData.numSubtickMoves; i++)
	{
		this->subtickMoves[this->nextSubtickMoveIndex] = moves[i];
		this->nextSubtickMoveIndex = (this->nextSubtickMoveIndex + 1) % this->maxSubtickMoves;
	}

	this->tickData[this->nextTickIndex] = tickData;
	this->nextTickIndex = (this->nextTickIndex + 1) % this->maxTicks;
}

// ====================================

bool CRRSubtickMoveIterator::Next()
{
	if (this->subtickMovesLeft == 0)
	{
		return false;
	}

	this->move = this->ctx->subtickMoves + this->currentSubtickMoveIndex;

	this->currentSubtickMoveIndex = (this->currentSubtickMoveIndex + 1) % this->ctx->maxSubtickMoves;
	this->subtickMovesLeft--;

	return true;
}

// ====================================

bool CRRIterator::Next()
{
	if (this->ticksLeft == 0)
	{
		return false;
	}

	this->tickData = this->ctx->tickData + this->currentTickIndex;
	if (this->tickData->HasSubtickMoves())
	{
		CRRSubtickMoveIterator subtickMovesIter;
		subtickMovesIter.ctx = ctx;
		subtickMovesIter.currentSubtickMoveIndex = this->currentSubtickMoveIndex;
		subtickMovesIter.subtickMovesLeft = this->tickData->numSubtickMoves;
		this->subtickMovesIter = subtickMovesIter;
	}

	if (this->forward)
	{
		this->currentTickIndex = (this->currentTickIndex + 1) % this->ctx->maxSubtickMoves;
		this->currentSubtickMoveIndex = (this->currentSubtickMoveIndex + this->tickData->numSubtickMoves) % this->ctx->maxSubtickMoves;
	}
	else
	{
		this->currentTickIndex = (this->currentTickIndex + this->ctx->maxSubtickMoves - 1) % this->ctx->maxSubtickMoves;
		this->currentSubtickMoveIndex =
			(this->currentSubtickMoveIndex + this->ctx->maxSubtickMoves - (this->ctx->tickData[this->currentTickIndex].numSubtickMoves))
			% this->ctx->maxSubtickMoves;
	}

	this->ticksLeft--;

	return true;
}

// ====================================

void ReplayRecorder::Cleanup()
{
	if (!this->keepData)
	{
		delete this->rpData;
	}
}

void ReplayRecorder::Push(const TickData &tick, const SubtickMoveLite *subtickMoves)
{
	this->rpData->tickData.push_back(tick);
	this->rpData->subtickMoves.insert(this->rpData->subtickMoves.end(), subtickMoves, subtickMoves + tick.numSubtickMoves);
}

u64 ReplayRecorder::PushBreather(const CircularReplayRecorder &crr, u64 wishNumTicks)
{
	CRRIterator backIter = crr.IterateReverse();
	u64 numTicks = 0;
	for (u64 i = 0; i < wishNumTicks; i++)
	{
		if (!backIter.Next())
		{
			break;
		}
		numTicks++;
	}

	if (numTicks == 0)
	{
		return 0;
	}

	// This is stupid, the iterators should probably support this better...
	CRRIterator iter = backIter;
	iter.currentSubtickMoveIndex = (iter.currentSubtickMoveIndex + crr.tickData[iter.currentTickIndex].numSubtickMoves) % crr.maxSubtickMoves;
	iter.currentTickIndex = (iter.currentTickIndex + crr.maxTicks + 1) % crr.maxTicks;
	iter.ticksLeft = numTicks;
	iter.forward = true;

	while (iter.Next())
	{
		this->rpData->tickData.push_back(*iter.tickData);
		if (iter.tickData->HasSubtickMoves())
		{
			CRRSubtickMoveIterator subtickMovesIter = iter.subtickMovesIter;
			while (subtickMovesIter.Next())
			{
				this->rpData->subtickMoves.push_back(*subtickMovesIter.move);
			}
		}
	}

	// TODO: Push events as well
	return numTicks;
}

bool ReplayRecorder::SaveToFile()
{
	// TODO: Implement saving to file

	return true;
}
