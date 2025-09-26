
#ifndef KZ_REPLAYSYSTEM_H
#define KZ_REPLAYSYSTEM_H

class KZPlayer;
class CCSPlayerPawnBase;
class PlayerCommand;
class CUserCmd;
class CBasePlayerWeapon;
struct EconInfo;

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

	namespace item
	{
		void InitItemAttributes();
		std::string GetItemAttributeName(u16 id);
		std::string GetWeaponName(u16 id);
		bool DoesPaintKitUseLegacyModel(float paintKit);
		void ApplyItemAttributesToWeapon(CBasePlayerWeapon &weapon, const EconInfo &info);
	} // namespace item

} // namespace KZ::replaysystem

#endif // KZ_REPLAYSYSTEM_H
