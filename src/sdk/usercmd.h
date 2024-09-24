#pragma once
#include "common.h"
#include "cs_usercmd.pb.h"
#include "cinbuttonstate.h"
class CSGOUserCmdPB;

class CUserCmdBase
{
public:
	uint64_t unk;

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

class PlayerCommand : public CUserCmd, public CInButtonState
{
public:
	uint8_t unknown[8];
	uint64_t flags;
	PlayerCommand *prevCmd;
	PlayerCommand *nextCmd;
};
