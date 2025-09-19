
#ifndef KZ_REPLAYSYSTEM_H
#define KZ_REPLAYSYSTEM_H

class KZPlayer;
class CCSPlayerPawnBase;
class PlayerCommand;
class CUserCmd;
class CBasePlayerWeapon;

namespace KZ::replaysystem
{
	void Init();
	void Cleanup();
	void OnRoundStart();
	void OnPhysicsSimulate(KZPlayer *player);
	void OnProcessMovement(KZPlayer *player);
	void OnDuck(KZPlayer *player);
	void OnProcessMovementPost(KZPlayer *player);
	void OnPhysicsSimulatePost(KZPlayer *player);
	void OnPlayerRunCommandPre(KZPlayer *player, PlayerCommand *command);
	void OnWeaponGiven(KZPlayer *player, CBasePlayerWeapon *weapon);
} // namespace KZ::replaysystem

#endif // KZ_REPLAYSYSTEM_H
