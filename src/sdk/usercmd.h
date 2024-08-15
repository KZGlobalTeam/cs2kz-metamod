#pragma once
#include "common.h"

class CSGOUserCmdPB;

class CUserCmdBase
{
private:
	uint8 unknown[0x8];
	virtual ~CUserCmdBase() = 0;
	virtual void unk0() = 0;
	virtual void unk1() = 0;
	virtual void unk2() = 0;
	virtual void unk3() = 0;
	virtual void unk4() = 0;
	virtual void unk5() = 0;
	virtual void unk6() = 0;
};

template<typename T>
class CUserCmdBaseHost : public CUserCmdBase, public T
{
	virtual ~CUserCmdBaseHost() = 0;
};

class CUserCmd : public CUserCmdBaseHost<CSGOUserCmdPB>
{
public:
	virtual ~CUserCmd() = 0;
};
