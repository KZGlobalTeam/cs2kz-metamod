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
	uint32_t unknown[2];
	u64 flags;
	PlayerCommand *unknowncmd;
	PlayerCommand *parentcmd;
};
