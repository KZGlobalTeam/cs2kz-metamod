/* date = November 19th 2023 11:33 pm */

#ifndef USERCMD_H
#define USERCMD_H

#include "protobuf/generated/cs_usercmd.pb.h"
// NOTE(GameChaos): protobuff stuff wants to be included before common.h
#include "common.h"

// size: 0x90
// name is just a guess based on IDA (in ISource2GameClients::ProcessUserCmds):
// usercmd::CUserCmdBaseHost<usercmd::CProtoUserCmdBase<CSGOUserCmdPB>>
class CUserCmdBaseHost
{
	public:
	CSGOUserCmdPB cmd;
	u8 padding[0x48];
};
static_assert(sizeof(CUserCmdBaseHost) == 0x90, "");

#endif //USERCMD_H
