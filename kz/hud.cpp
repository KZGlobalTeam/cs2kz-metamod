#include "hud.h"
#include "utils/utils.h"
#include "movement/movement.h"

void KZ::HUD::OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(slot);
	if (!player->GetController()) return;
	float speed = player->GetVelocity().Length2D();
	utils::PrintAlert(g_pEntitySystem->GetBaseEntity(CEntityIndex(slot.Get() + 1)), "%.2f", speed);
}