#include "kz.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

void KZ::HUD::OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused)
{
}

internal void AddSpeedText(KZPlayer *player, char* buffer)
{
	char speed[128];
	Vector velocity;
	player->GetVelocity(&velocity);
	// Keep the takeoff velocity on for a while after landing so the speed values flicker less.
	if ((player->GetPawn()->m_fFlags & FL_ONGROUND && utils::GetServerGlobals()->curtime - player->landingTime > 0.07)
		|| (player->GetPawn()->m_MoveType == MOVETYPE_LADDER && !player->IsButtonDown(IN_JUMP)))
	{
		snprintf(speed, sizeof(speed), "Speed: %.0f", velocity.Length2D());
	}
	else
	{
		snprintf(speed, sizeof(speed), "Speed: %.0f (%.0f)", velocity.Length2D(), player->takeoffVelocity.Length2D());
	}
	strcat(buffer, speed);
}

internal void AddKeyText(KZPlayer *player, char *buffer)
{
	char keys[128];
	snprintf(keys, sizeof(keys), "Keys: %s %s %s %s %s %s",
		player->IsButtonDown(IN_MOVELEFT) ? "A" : "_",
		player->IsButtonDown(IN_FORWARD) ? "W" : "_",
		player->IsButtonDown(IN_BACK) ? "S" : "_",
		player->IsButtonDown(IN_MOVERIGHT) ? "D" : "_",
		player->IsButtonDown(IN_DUCK) ? "C" : "_",
		player->IsButtonDown(IN_JUMP) ? "J" : "_");
	strcat(buffer, keys);
}

void KZ::HUD::DrawSpeedPanel(KZPlayer *player)
{
	char buffer[1024] = "";
	AddSpeedText(player, buffer);
	strcat(buffer, "\n"); 
	AddKeyText(player, buffer);

	utils::PrintAlert(g_pEntitySystem->GetBaseEntity(CEntityIndex(player->index)), "%s", buffer);
	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event) return;
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 12345);
	event->SetInt("userid", -1);
	
	IGameEventListener2 *listener = utils::GetLegacyGameEventListener(player->GetController()->entindex() - 1);
	listener->FireGameEvent(event);
	interfaces::pGameEventManager->FreeEvent(event);
}