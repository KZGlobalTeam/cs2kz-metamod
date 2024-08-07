#include "common.h"
#include "kz_replays.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

#include "../language/kz_language.h"
#include "../timer/kz_timer.h"
#include "../noclip/kz_noclip.h"

void KZReplayService::Init() {}
void KZReplayService::Reset() {}

void KZReplayService::OnPhysicsSimulatePost()
{
	if (this->replay)
	{
		this->player->languageService->PrintChat(true, false, "Playing bot...");
		return Play();
	}

	if (!this->player->timerService->GetValidTimer())
	{
		return;
	}

	Vector origin;
	Vector velocity;
	QAngle angles;
	this->player->GetOrigin(&origin);
	this->player->GetAngles(&angles);
	this->player->GetVelocity(&velocity);

	Frame frame;
	frame.tick = this->player->timerService->GetTime();
	frame.orientation = angles;
	frame.position = origin;
	frame.velocity = velocity;
	frame.duckAmount = this->player->GetMoveServices()->m_flDuckAmount;
	frame.moveType = this->player->GetMoveType();
	frame.flags = this->player->GetPlayerPawn()->m_fFlags;

	this->frames.AddToTail(frame);
}

void KZReplayService::Play() 
{
	this->time += ENGINE_FIXED_TICK_INTERVAL;

	Frame frame;
	int i = this->lastIndex;
	do
	{
		frame = this->frames.Element(i++);
	} 
	while (i < this->frames.Count() - 1 && frame.tick < this->time);

	this->lastIndex = i;

	if (i == this->frames.Count())
	{
		this->replayDone = true;

		META_CONPRINTF("Bot done, restart in 3...\n");

		if (this->time > frame.tick + 3)
		{
			this->StartReplay(this->frames);
		}
		return;
	}

	//Vector origin;
	//this->player->GetOrigin(&origin);
	//f32 distance = (origin - frame.position).Length2D();

	if (frame.flags & FL_ONGROUND)
	{
		this->player->noclipService->EnableNoclip();
	}
	else
	{
		this->player->noclipService->DisableNoclip();
	}

	this->player->GetPlayerPawn()->Teleport(&frame.position, &frame.orientation, &frame.velocity);
	this->player->SetMoveType(frame.moveType);
	this->player->GetMoveServices()->m_flDuckAmount = frame.duckAmount;
	//this->player->GetMoveServices()->m_nButtons = IN_FORWARD;
}

void KZReplayService::StartReplay(CUtlVector<Frame> &frames) 
{
	this->replayDone = false;
	this->time = 0;
	this->lastIndex = 0;
	this->frames = frames;
	this->replay = true;
}

void KZReplayService::OnTimerStart()
{
	this->frames = CUtlVector<Frame>(1, 0);
	interfaces::pEngine->ServerCommand("sv_cheats 1");
	interfaces::pEngine->ServerCommand("bot_kick");
	interfaces::pEngine->ServerCommand("bot_quota_mode fill");
	interfaces::pEngine->ServerCommand("bot_quota 1");
	interfaces::pEngine->ServerCommand("bot_join_team CT");
	interfaces::pEngine->ServerCommand("bot_add_ct");
	interfaces::pEngine->ServerCommand("bot_stop 1");
	interfaces::pEngine->ServerCommand("bot_dont_shoot 1");
	interfaces::pEngine->ServerCommand("bot_zombie 1");
	interfaces::pEngine->ServerCommand("bot_controllable 0");
	interfaces::pEngine->ServerCommand("sv_cheats 0");
}

void KZReplayService::OnTimerStop()
{
	for (i32 i = 0; i <= g_pKZUtils->GetGlobals()->maxClients; i++)
	{
		CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController();
		KZPlayer *otherPlayer = g_pKZPlayerManager->ToPlayer(i);

		if (controller)
		{
			this->player->languageService->PrintConsole(true, false, "asd %s", otherPlayer->GetName());

			if (V_strstr(V_strlower((char *)otherPlayer->GetName()), "bot"))
			{
				this->player->languageService->PrintConsole(true, false, "Spectating %s", otherPlayer->GetName());
				otherPlayer->replayService->StartReplay(this->frames);
			}

			return;
		}
	}
}

static_function KZPlayer *GetReplayBot(const char *playerName)
{
	for (i32 i = 0; i <= g_pKZUtils->GetGlobals()->maxClients; i++)
	{
		KZPlayer *otherPlayer = g_pKZPlayerManager->ToPlayer(i);
		CBaseEntity *ent = static_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(CEntityIndex(i)));

		if (ent && !V_stricmp(otherPlayer->GetName(), playerName))
		{
			return otherPlayer;
		}
	}

	return nullptr;
}

void KZReplayService::Skip(int seconds)
{
	this->time += seconds;
}

static_function SCMD_CALLBACK(Command_KzReplaySkip)
{
	const char *secondsStr = args->Arg(1);
	const char *playerName = args->Arg(2);
	KZPlayer *replayBot = GetReplayBot(playerName);

	if (replayBot)
	{
		int seconds = std::stoi(secondsStr);
		replayBot->replayService->Skip(seconds);
	}

	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzReplay)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	const char *playerName = args->Arg(1);
	KZPlayer *replayBot = GetReplayBot(playerName);

	if (replayBot)
	{
		player->timerService->InvalidateRun();
		player->timerService->TimerStop();
		replayBot->replayService->StartReplay(player->replayService->frames);
	}

	return MRES_SUPERCEDE;
}

void KZReplayService::RegisterCommands() {
	scmd::RegisterCmd("kz_replay", Command_KzReplay);
	scmd::RegisterCmd("kz_replay_skip", Command_KzReplaySkip);
}

