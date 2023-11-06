#include "kz_mode.h"
#include "kz_mode_vnl.h"

void KZ::mode::InitModeService(KZPlayer *player)
{
	player->modeService = new KZVanillaModeService(player);
}