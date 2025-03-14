#pragma once
#include "common.h"
#include "cs_usercmd.pb.h"

struct SubtickMoveNode;
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

struct CmdData
{
	u32 serverTick {};
	u32 cmdNum {};
	SubtickMoveNode *subtickMoves;
	f64 renderTimes[4] {};
	i32 mousedx {};
	i32 mousedy {};
	i32 weapon {};
	QAngle angles;
	u64 buttons[3] {};

	f32 GetFps() const
	{
		i32 numSamples = 0;
		while (numSamples < 4 && renderTimes[numSamples] != 0.0)
		{
			numSamples++;
		}

		return numSamples < 2 ? -1.0f : (renderTimes[numSamples - 1] - renderTimes[0]) / (numSamples - 1);
	}
};

struct CmdDataNode
{
	CmdDataNode *prev;
	CmdDataNode *next;
	CmdData data;
	void Free();
};

class CUserCmd : public CUserCmdBaseHost<CSGOUserCmdPB>
{
public:
	struct
	{
		void **vtable;
		uint64 buttons[3];
	} buttonstates;

	// Not part of the player message
	uint32 unknown[2];

	enum Flag : uint32
	{
		None = 0,
		ServerGeneratedCommand = 1 << 0,
		Attack1StartHistoryIndex = 1 << 1,
		Attack2StartHistoryIndex = 1 << 2,
		Attack3StartHistoryIndex = 1 << 3,
		ViewAngles = 1 << 4,
		ForwardMove = 1 << 5,
		LeftMove = 1 << 6,
		UpMove = 1 << 7,
		WeaponSelect = 1 << 8,
		InputHistory = 1 << 9,
		// PlayerTickCount = 1 << 10,
		PlayerTickFraction = 1 << 11,
		// RenderTickCount = 1 << 12,
		RenderTickFraction = 1 << 13,
		ShootPosition = 1 << 14,
		TargetHeadPosCheck = 1 << 15,
		TargetAbsPosCheck = 1 << 16,
		TargetAbsAngCheck = 1 << 17,
		// ClInterp = 1 << 18,
		SvInterp0 = 1 << 19,
		SvInterp1 = 1 << 20,
		PlayerInterp = 1 << 21,
		SubtickMoveWhen = 1 << 22,
		SubtickAnalogForwardDelta = 1 << 23,
		SubtickAnalogLeftDelta = 1 << 24,
	};

	Flag flags;
	CUserCmd *unknowncmd;
	CUserCmd *parentcmd;

	static void InitPools();
	static void CleanupPools();
	CmdDataNode *MakeDataNode(u32 serverTick);
};
