// The checkpoint category in the options menu.
#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

void KZCheckpointService::RegisterMenu()
{
	KZOptNode *cat = KZ::menu::AddCategory("Menu - Checkpoints");
	KZ::menu::AddToggle(cat, "Menu - Checkpoint Message", "checkpointMessage", true);
	KZ::menu::AddToggle(cat, "Menu - Checkpoint Sound", "checkpointSound", true);
	KZ::menu::AddToggle(cat, "Menu - Teleport Sound", "teleportSound", true);
}
