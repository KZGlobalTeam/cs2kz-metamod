#include "common.h"
#include "kz_replays.h"
#include "utils/simplecmds.h"
#include "utils/utils.h"

#include "../language/kz_language.h"
#include "../timer/kz_timer.h"
#include "../noclip/kz_noclip.h"

static_global CUtlVector<Frame> srReplay = CUtlVector<Frame>(1, 0);
static_global KZPlayer *summoner = nullptr;
static_global KZPlayer *replayBot = nullptr;

static_global const Vector NULL_VECTOR = Vector(0, 0, 0);
static_global f64 botChillTimeAfterReplayBeforeGettingKicked = 3;

void KZReplayService::Init() 
{
	interfaces::pEngine->ServerCommand("sv_cheats 1");
	interfaces::pEngine->ServerCommand("mp_autoteambalance 0");
	interfaces::pEngine->ServerCommand("bot_kick");
	interfaces::pEngine->ServerCommand("bot_chatter 0");
	interfaces::pEngine->ServerCommand("bot_dont_shoot 1");
	interfaces::pEngine->ServerCommand("bot_zombie 1");
	interfaces::pEngine->ServerCommand("bot_controllable 0");
	interfaces::pEngine->ServerCommand("sv_cheats 0");
}

void KZReplayService::Reset() {}

void KZReplayService::OnProcessMovementPost() 
{
	if (!this->isReplayBot)
	{
		return;
	}
	if (this->frames.Count() == 0)
	{
		return;
	}

	Frame frame = this->frames.Element(this->lastIndex + 1);
	this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[0] = frame.buttons;
}

void KZReplayService::OnPhysicsSimulatePost()
{
	if (this->isReplayBot)
	{
		return Play();
	}

	// This is such a dumb way of doing this but I can't for the life of me figure out how to do it otherwise. The controller never seems to exist...
	if (this->player->GetPlayerPawn()->IsBot())
	{
		InitializeReplayBot();
		return;
	}

	if (!this->player->timerService->GetValidTimer())
	{
		return;
	}

	this->time += ENGINE_FIXED_TICK_INTERVAL;

	AddFrame();
}

void KZReplayService::AddFrame() 
{
	Vector origin;
	Vector velocity;
	QAngle angles;
	this->player->GetOrigin(&origin);
	this->player->GetAngles(&angles);
	this->player->GetVelocity(&velocity);

	Frame frame;
	frame.time = this->time;
	frame.orientation = angles;
	frame.position = origin;
	frame.velocity = velocity;
	frame.duckAmount = this->player->GetMoveServices()->m_flDuckAmount;
	frame.moveType = this->player->GetMoveType();
	frame.flags = this->player->GetPlayerPawn()->m_fFlags;
	frame.buttons = this->player->GetMoveServices()->m_nButtons()->m_pButtonStates[0];

	this->frames.AddToTail(frame);
}

void KZReplayService::Play() 
{
	if (this->frames.Count() == 0)
	{
		return;
	}

	this->time += ENGINE_FIXED_TICK_INTERVAL;

	Frame frame;
	int i = this->lastIndex - 1;
	do
	{
		frame = this->frames.Element(++i);
	} 
	while (i < this->frames.Count() - 1 && frame.time < this->time);

	this->lastIndex = i;

	if (i == this->frames.Count() - 1)
	{
		this->replayDone = true;

		if (this->time > frame.time + botChillTimeAfterReplayBeforeGettingKicked)
		{
			this->player->Kick("Replay over", NETWORK_DISCONNECT_KICKED);
			replayBot = nullptr;
			return;
		}
	}
	
	this->player->SetMoveType(frame.moveType);
	this->player->GetMoveServices()->m_bDucked(frame.duckAmount > .0001);
	this->player->GetMoveServices()->m_flDuckAmount(frame.duckAmount > .0001 ? frame.duckAmount : 0);
	this->player->GetPlayerPawn()->Teleport(&frame.position, &frame.orientation, &frame.velocity);
}

void KZReplayService::InitializeReplayBot()
{
	if (summoner)
	{
		summoner->specService->SpectatePlayer(this->player);
		//summoner = nullptr;
	}

	replayBot = this->player;

	this->player->GetName();

	StartReplay();
}

void KZReplayService::StartReplay() 
{
	this->replayDone = false;
	this->time = 0;
	this->lastIndex = 0;
	this->frames = srReplay;
	this->isReplayBot = true;
}

void KZReplayService::OnTimerStart()
{
	this->frames.Purge();
	this->frames = CUtlVector<Frame>(1, 0);
	this->time = 0;
	AddFrame();
}

void KZReplayService::OnTimerStop()
{
	srReplay.Purge();
	srReplay = CUtlVector<Frame>(1, 0);
	srReplay.CopyArray(this->frames.Base(), this->frames.Count());
}

static_function bool isValidInteger(const char *str)
{
	if (*str == '-' || *str == '+')
	{
		str++;
	}
	if (*str == '\0')
	{
		return false;
	}
	while (*str)
	{
		if (!std::isdigit(*str))
		{
			return false;
		}
		str++;
	}
	return true;
}

void KZReplayService::Skip(int seconds)
{
	Frame lastFrame = this->frames.Element(this->frames.Count() - 1);
	this->time = MAX(MIN(this->time + seconds, lastFrame.time - botChillTimeAfterReplayBeforeGettingKicked), 0);

	if (seconds < 0 && this->lastIndex >= 0)
	{
		Frame frame;
		int i = this->lastIndex;
		do
		{
			frame = this->frames.Element(i--);
		} while (i >= 0 && frame.time > this->time);

		this->lastIndex = i;
	}
}

void KZReplayService::Skip(const char *secondsStr)
{
	if (isValidInteger(secondsStr))
	{
		const int seconds = std::atoi(secondsStr);
		Skip(seconds);
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Replay Skip - Command Usage");
	}
}

static_function SCMD_CALLBACK(Command_KzReplaySkip)
{
	const char *secondsStr = args->Arg(1);
	if (replayBot)
	{
		replayBot->replayService->Skip(secondsStr);
	}

	return MRES_SUPERCEDE;
}

static_function SCMD_CALLBACK(Command_KzReplay)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	summoner = player;
	interfaces::pEngine->ServerCommand("bot_add_ct");
	return MRES_SUPERCEDE;
}

void KZReplayService::RegisterCommands() 
{
	scmd::RegisterCmd("kz_replay", Command_KzReplay);
	scmd::RegisterCmd("kz_replay_skip", Command_KzReplaySkip);
}

