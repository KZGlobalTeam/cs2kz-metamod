#include "kz_telemetry.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"
#include "sdk/usercmd.h"

#define BUFFER_TIME   120.0f
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
	auto cmdHead = this->cmdDataHead;
	while (cmdHead)
	{
		if (cmdHead->data.serverTick > g_pKZUtils->GetGlobals()->tickcount - BUFFER_TIME * ENGINE_FIXED_TICK_INTERVAL)
		{
			break;
		}
		this->cmdDataHead = cmdHead->next;
		if (this->cmdDataHead)
		{
			this->cmdDataHead->prev = nullptr;
		}
		cmdHead->Free();
		cmdHead = this->cmdDataHead;
	}
}

void KZTelemetryService::OnProcessUsercmds(CUserCmd *cmds, int numcmds)
{
	for (int i = 0; i < numcmds; i++)
	{
		i32 cmdNum = cmds[i].cmdNum;
		if (cmdNum > this->lastCmdNumReceived)
		{
			this->lastCmdNumReceived = cmdNum;
			auto node = cmds[i].MakeDataNode(g_pKZUtils->GetServerGlobals()->tickcount);
			// Should not happen.
			if (!node)
			{
				META_CONPRINTF("[KZ] Failed to allocate CmdDataNode!\n");
				continue;
			}
			if (!this->cmdDataHead)
			{
				this->cmdDataHead = this->cmdDataTail = node;
			}
			else
			{
				this->cmdDataTail->next = node;
				node->prev = this->cmdDataTail;
				this->cmdDataTail = node;
			}
		}
	}
}

void KZTelemetryService::OnClientDisconnect()
{
	CmdDataNode *node = this->cmdDataHead;
	while (node)
	{
		CmdDataNode *next = node->next;
		node->Free();
		node = next;
	}
	this->cmdDataHead = this->cmdDataTail = nullptr;
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
	}
	KZTelemetryService::lastActiveCheckTime = currentTime;
}
