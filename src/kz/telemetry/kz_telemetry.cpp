#include "kz_telemetry.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"
#include "sdk/usercmd.h"

#include "utils/simplecmds.h"
#define BUFFER_TICKS  (u32)(120 * 64) // 2 minutes
#define AFK_THRESHOLD 30.0f
f64 KZTelemetryService::lastActiveCheckTime = 0.0f;

void KZTelemetryService::OnPhysicsSimulatePost()
{
	// AFK check
	if (!this->player->GetMoveServices())
	{
		return;
	}
	if (this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[1] != 0
		|| this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[2] != 0)
	{
		this->activeStats.lastActionTime = g_pKZUtils->GetServerGlobals()->realtime;
		return;
	}

	// Remove all commands that are too old.
	auto currentCmdNode = this->cmdDataNodeHead;
	auto thresholdTick = g_pKZUtils->GetGlobals()->tickcount - BUFFER_TICKS;
	while (currentCmdNode)
	{
		if (currentCmdNode->data.serverTick > thresholdTick)
		{
			break;
		}
		auto nextCmdNode = currentCmdNode->next;
		currentCmdNode->Free();
		currentCmdNode = nextCmdNode;
		if (currentCmdNode)
		{
			currentCmdNode->prev = nullptr;
		}
	}
	this->cmdDataNodeHead = currentCmdNode;
}

void KZTelemetryService::OnProcessUsercmds(CUserCmd *cmds, int numcmds)
{
	for (int i = 0; i < numcmds; i++)
	{
		i32 cmdNum = cmds[i].cmdNum;
		i32 lastCmdNumReceived = this->cmdDataNodeTail ? this->cmdDataNodeTail->data.cmdNum : -1;
		if (cmdNum > lastCmdNumReceived)
		{
			auto node = cmds[i].MakeDataNode(g_pKZUtils->GetServerGlobals()->tickcount);
			// Should not happen.
			if (!node)
			{
				META_CONPRINTF("[KZ] Failed to allocate CmdDataNode!\n");
				continue;
			}
			if (!this->cmdDataNodeHead)
			{
				this->cmdDataNodeHead = node;
			}
			node->prev = this->cmdDataNodeTail;
			if (this->cmdDataNodeTail)
			{
				this->cmdDataNodeTail->next = node;
			}
			this->cmdDataNodeTail = node;
		}
	}
}

void KZTelemetryService::OnClientDisconnect()
{
	CmdDataNode *node = this->cmdDataNodeHead;
	while (node)
	{
		CmdDataNode *next = node->next;
		node->Free();
		node = next;
	}
	this->cmdDataNodeHead = this->cmdDataNodeTail = nullptr;
}

void KZTelemetryService::DumpCmdStats()
{
	if (!this->cmdDataNodeHead)
	{
		return;
	}
	this->player->PrintAlert(false, false, "CmdStats: %i @%i -> %i @%i\n", this->cmdDataNodeHead->data.cmdNum, this->cmdDataNodeHead->data.serverTick,
							 this->cmdDataNodeTail->data.cmdNum, this->cmdDataNodeTail->data.serverTick);
}

void KZTelemetryService::ActiveCheck()
{
	f64 currentTime = g_pKZUtils->GetServerGlobals()->realtime;
	f64 duration = currentTime - KZTelemetryService::lastActiveCheckTime;
	for (u32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player->IsInGame() || player->IsFakeClient() || player->IsCSTV())
		{
			continue;
		}
		player->telemetryService->activeStats.timeSpentInServer += duration;
		if (player->IsAlive())
		{
			if (currentTime - player->telemetryService->activeStats.lastActionTime > AFK_THRESHOLD)
			{
				player->telemetryService->activeStats.afkDuration += duration;
			}
			else
			{
				player->telemetryService->activeStats.activeTime += duration;
			}
		}
		player->telemetryService->DumpCmdStats();
	}
	KZTelemetryService::lastActiveCheckTime = currentTime;
}

SCMD_CALLBACK(Kz_CmdStats)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->telemetryService->DumpCmdStats();
	return MRES_SUPERCEDE;
}

void KZTelemetryService::RegisterCommands()
{
	scmd::RegisterCmd("kz_cmdstats", Kz_CmdStats);
}
