#pragma once
#include "common.h"
#include "cs_usercmd.pb.h"
class CSGOUserCmdPB;

class CUserCmdBase
{
public:
	int cmdNum;
	uint8_t unk[4];

	virtual ~CUserCmdBase();

private:
	virtual void unk0();
	virtual void unk1();
	virtual void unk2();
	virtual void unk3();
	virtual void unk4();
	virtual void unk5();
	virtual void unk6();
};

template<typename T>
class CUserCmdBaseHost : public CUserCmdBase, public T
{
};

class CUserCmd : public CUserCmdBaseHost<CSGOUserCmdPB>
{
};

class PlayerCommand : public CUserCmd
{
public:
	struct
	{
		void **vtable;
		uint64 buttons[3];
	} buttonstates;

	// Not part of the player message
	uint32 unknown[2];
	enum class Flag : uint32
	{
		None = 0,
		ServerGeneratedCommand = 1ULL << 0,
		Attack1StartHistoryIndex = 1ULL << 1,
		Attack2StartHistoryIndex = 1ULL << 2,
		Attack3StartHistoryIndex = 1ULL << 3,
		ViewAngles = 1ULL << 4,
		ForwardMove = 1ULL << 5,
		LeftMove = 1ULL << 6,
		UpMove = 1ULL << 7,
		WeaponSelect = 1ULL << 8,
		InputHistory = 1ULL << 9,
		// PlayerTickCount = 1ULL << 10,
		PlayerTickFraction = 1ULL << 11,
		// RenderTickCount = 1ULL << 12,
		RenderTickFraction = 1ULL << 13,
		ShootPosition = 1ULL << 14,
		TargetHeadPosCheck = 1ULL << 15,
		TargetAbsPosCheck = 1ULL << 16,
		TargetAbsAngCheck = 1ULL << 17,
		// ClInterp = 1ULL << 18,
		SvInterp0 = 1ULL << 19,
		SvInterp1 = 1ULL << 20,
		PlayerInterp = 1ULL << 21,
		SubtickMoveWhen = 1ULL << 22,
		SubtickAnalogForwardDelta = 1ULL << 23,
		SubtickAnalogLeftDelta = 1ULL << 24,
	};
	Flag flags;
	PlayerCommand *unknowncmd;
	PlayerCommand *parentcmd;
};
