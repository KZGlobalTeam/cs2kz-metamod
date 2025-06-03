
#ifndef KZ_REPLAYSYSTEM_H
#define KZ_REPLAYSYSTEM_H

class CCSPlayerPawnBase;

namespace KZ::replaysystem
{
	void Init();
	void Cleanup();
	void OnRoundStart();
	void FinishMovePre(KZPlayer *player, CMoveData *mv);
} // namespace KZ::replaysystem

#endif // KZ_REPLAYSYSTEM_H
