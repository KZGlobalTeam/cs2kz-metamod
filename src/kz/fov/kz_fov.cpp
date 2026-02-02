#include "kz_fov.h"
#include "utils/simplecmds.h"
#include "kz/language/kz_language.h"

void KZFOVService::OnPhysicsSimulate()
{
	CCSPlayerController *controller = this->player->GetController();
	if (!controller) {
		return;
	}
	u32 currentFOV = this->GetFOV();
	if (controller->m_iDesiredFOV() != currentFOV) {
		controller->m_iDesiredFOV(currentFOV);
	}
}

SCMD(kz_fov, SCFL_PLAYER | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	if (V_strlen(args->ArgS()) > 0)
	{
		unsigned long newFOV = std::strtoul(args->ArgS(), nullptr, 10);
		u32 minFOV = player->fovService->GetMinFOV();
		u32 maxFOV = player->fovService->GetMaxFOV();
		if (newFOV < minFOV || newFOV > maxFOV) {
			player->languageService->PrintChat(true, false, "Error Message (Invalid FOV)", args->ArgS(), minFOV, maxFOV);
			return MRES_SUPERCEDE;
		}

		player->fovService->SetFOV(newFOV);
	}

	player->languageService->PrintChat(true, false, "Quiet Option - FOV - Show", player->fovService->GetFOV());
	return MRES_SUPERCEDE;
}
