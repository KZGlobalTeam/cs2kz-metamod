#pragma once

#include "common.h"
#include "movement/movement.h"
#include "utils/datatypes.h"


#define KZ_COLLISION_GROUP_STANDARD LAST_SHARED_COLLISION_GROUP
#define KZ_COLLISION_GROUP_NOTRIGGER COLLISION_GROUP_DEBRIS

#define KZ_SND_SET_CP "UIPanorama.round_report_odds_none"
#define KZ_SND_DO_TP "UIPanorama.round_report_odds_none"

#define KZ_CHAT_PREFIX "{lime}KZ {grey}|{default}"

extern CMovementPlayerManager *g_pPlayerManager;

class KZPlayer;
//class Jump;
class KZAnticheatService;
class KZCheckpointService;
class KZGlobalService;
class KZHUDService;
class KZJumpstatsService;
class KZMeasureService;
class KZModeService;
class KZOptionService;
class KZQuietService;
class KZRacingService;
class KZSavelocService;
class KZStyleService;
class KZTimerService;
class KZTipService;

class KZPlayer : public MovementPlayer
{
public:
	KZPlayer(i32 i) : MovementPlayer(i)
	{
		this->Init();
	}
	void Init();
	virtual void Reset() override;
	virtual void OnStartProcessMovement() override;
	virtual void OnStopProcessMovement() override;

	virtual void OnStartTouchGround() override;
	virtual void OnStopTouchGround() override;
	virtual void OnChangeMoveType(MoveType_t oldMoveType) override;
	virtual void OnAirAcceleratePre(Vector &wishdir, f32 &wishspeed, f32 &accel) override;
	virtual void OnAirAcceleratePost(Vector wishdir, f32 wishspeed, f32 accel) override;
	
	virtual void StartZoneStartTouch();
	virtual void StartZoneEndTouch();
	virtual void EndZoneStartTouch();

private:
	bool inNoclip{};
	bool hideLegs{};
	TurnState previousTurnState{};
public:
	KZAnticheatService *anticheatService;
	KZCheckpointService *checkpointService;
	KZGlobalService *globalService;
	KZHUDService *hudService;
	KZJumpstatsService *jumpstatsService;
	KZMeasureService *measureService;
	KZModeService *modeService;
	KZOptionService *optionsService;
	KZQuietService *quietService;
	KZRacingService *racingService;
	KZSavelocService *savelocService;
	KZStyleService *styleService;
	KZTimerService *timerService;
	KZTipService *tipService;
	
	// Misc stuff that doesn't belong into any service.
	
	// Noclip
	void DisableNoclip();
	void ToggleNoclip();
	
	void EnableGodMode();
	void HandleMoveCollision();
	
	// Leg stuff
	void ToggleHideLegs();
	void UpdatePlayerModelAlpha();
	void PlayCheckpointSound();
	void PlayTeleportSound();
};

class KZBaseService
{
public:
	KZPlayer *player;

	KZBaseService(KZPlayer *player)
	{
		this->player = player;
	}
	// To be implemented by each service class
	virtual void Reset() {};
};

class CKZPlayerManager : public CMovementPlayerManager
{
public:
	CKZPlayerManager()
	{
		for (int i = 0; i < MAXPLAYERS + 1; i++)
		{
			delete players[i];
			players[i] = new KZPlayer(i);
		}
	}
public:
	KZPlayer *ToPlayer(CCSPlayer_MovementServices *ms);
	KZPlayer *ToPlayer(CCSPlayerController *controller);
	KZPlayer *ToPlayer(CBasePlayerPawn *pawn);
	KZPlayer *ToPlayer(CPlayerSlot slot);
	KZPlayer *ToPlayer(CEntityIndex entIndex);
	KZPlayer *ToPlayer(CPlayerUserId userID);
	KZPlayer *ToPlayer(u32 index);

	KZPlayer *ToKZPlayer(MovementPlayer *player) { return static_cast<KZPlayer *>(player); }
};

namespace KZ
{
	CKZPlayerManager *GetKZPlayerManager();
	namespace HUD
	{
		void DrawSpeedPanel(KZPlayer *player);
	}
	namespace misc
	{
		void RegisterCommands();
		void OnClientPutInServer(CPlayerSlot slot);
	}
};