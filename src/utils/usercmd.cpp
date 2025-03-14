#include "common.h"
#include "sdk/usercmd.h"
#include "sdk/datatypes.h"
#include "utils/pool.h"

static_global Pool<SubtickMoveNode> subtickMovePool;
static_global Pool<CmdDataNode> cmdDataPool;

void CUserCmd::InitPools()
{
	subtickMovePool.arena = new Arena(1024 * 1024 * 1024);
	cmdDataPool.arena = new Arena(1024 * 1024 * 1024);
}

void CUserCmd::CleanupPools()
{
	subtickMovePool.arena->~Arena();
	cmdDataPool.arena->~Arena();
}

CmdDataNode *CUserCmd::MakeDataNode(u32 serverTick)
{
	CmdDataNode *cmdData = cmdDataPool.Alloc();
	if (!cmdData)
	{
		META_CONPRINTF("[KZ] Failed to allocate CmdData!\n");
		return nullptr;
	}
	cmdData->data.serverTick = serverTick;
	cmdData->data.cmdNum = this->cmdNum;
	cmdData->data.mousedx = this->base().mousedx();
	cmdData->data.mousedy = this->base().mousedy();
	cmdData->data.weapon = this->base().weaponselect();
	cmdData->data.angles = {this->base().viewangles().x(), this->base().viewangles().y(), this->base().viewangles().z()};
	cmdData->data.buttons[0] = this->base().buttons_pb().buttonstate1();
	cmdData->data.buttons[1] = this->base().buttons_pb().buttonstate2();
	cmdData->data.buttons[2] = this->base().buttons_pb().buttonstate3();

	SubtickMoveNode *lastMove = nullptr;
	for (i32 i = 0; i < this->base().subtick_moves_size(); i++)
	{
		auto move = this->base().subtick_moves().at(i);
		SubtickMoveNode *node = subtickMovePool.Alloc();
		if (!node)
		{
			META_CONPRINTF("[KZ] Failed to allocate SubtickMove!\n");
			break;
		}
		node->move.when = move.when();
		node->move.button = move.button();
		if (!node->move.IsAnalogInput())
		{
			node->move.pressed = move.pressed();
		}
		else
		{
			node->move.analogMove.analog_forward_delta = move.analog_forward_delta();
			node->move.analogMove.analog_left_delta = move.analog_left_delta();
		}
		if (lastMove)
		{
			lastMove->next = node;
		}
		lastMove = node;
	}
	return cmdData;
}

void CmdDataNode::Free()
{
	SubtickMoveNode *node = this->data.subtickMoves;
	while (node)
	{
		SubtickMoveNode *next = node->next;
		subtickMovePool.Free(node);
		node = next;
	}
	cmdDataPool.Free(this);
}
