#include "kz.h"
#include "utils/utils.h"

void KZ::HUD::OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	KZPlayer *player = GetKZPlayerManager()->ToPlayer(slot);
	if (!player->GetController()) return;
	Vector velocity;
	player->GetVelocity(&velocity);
	float speed = velocity.Length2D();
	utils::PrintAlert(g_pEntitySystem->GetBaseEntity(CEntityIndex(player->index)), "%.2f", speed);
}