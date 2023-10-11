#include "common.h"
#include "utils/utils.h"
#include "kz.h"

META_RES KZ::misc::OnClientCommand(CPlayerSlot &slot, const CCommand &args)
{
	KZPlayer *player = KZ::GetKZPlayerManager()->ToPlayer(slot);
	if (!V_stricmp("kz_noclip", args[0]))
	{
		player->ToggleNoclip();
		return MRES_HANDLED;
	}
	if (!V_stricmp("kz_hidelegs", args[0]))
	{
		player->UpdatePlayerModelAlpha();
		return MRES_HANDLED;
	}
	return MRES_IGNORED;
}