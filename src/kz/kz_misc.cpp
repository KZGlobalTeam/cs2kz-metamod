#include "common.h"
#include "utils/utils.h"
#include "kz.h"
#include "utils/simplecmds.h"

#include "tier0/memdbgon.h"

internal SCMD_CALLBACK(Command_KzNoclip)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->ToggleNoclip();
	return MRES_SUPERCEDE;
}

internal SCMD_CALLBACK(Command_KzHidelegs)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(controller);
	player->UpdatePlayerModelAlpha();
	return MRES_SUPERCEDE;
}

void KZ::misc::RegisterCommands()
{
	ScmdRegisterCmd("kz_noclip", Command_KzNoclip);
	ScmdRegisterCmd("kz_hidelegs", Command_KzHidelegs);
}
