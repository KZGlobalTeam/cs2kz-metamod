#include "hud.h"
#include "utils/utils.h"
#include "movement/movement.h"

void KZ::hud::OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
	MovementPlayer *player = g_pPlayerManager->ToPlayer(slot);
	if (!player->GetController()) return;
	float speed = player->GetVelocity().Length2D();
	utils::PrintAlert(player->GetController(), "%.2f\n", speed);
	utils::PrintCentre(player->GetController(), "%.2f\n%.2f", speed);
}