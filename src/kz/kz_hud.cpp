#include "kz.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

internal void AddSpeedText(KZPlayer *player, char* buffer, int size)
{
	char speed[128];
	speed[0] = 0;
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
	strncat(buffer, speed, size);
}

internal void AddKeyText(KZPlayer *player, char *buffer, int size)
{
	char keys[128];
	keys[0] = 0;
	snprintf(keys, sizeof(keys), "Keys: %s %s %s %s %s %s",
		player->IsButtonDown(IN_MOVELEFT) ? "A" : "_",
		player->IsButtonDown(IN_FORWARD) ? "W" : "_",
		player->IsButtonDown(IN_BACK) ? "S" : "_",
		player->IsButtonDown(IN_MOVERIGHT) ? "D" : "_",
		player->IsButtonDown(IN_DUCK) ? "C" : "_",
		player->IsButtonDown(IN_JUMP) ? "J" : "_");
	strncat(buffer, keys, size);
}

void KZ::HUD::DrawSpeedPanel(KZPlayer *player)
{
	char buffer[1024];
	buffer[0] = 0;
	AddSpeedText(player, buffer, sizeof(buffer));
	strcat(buffer, "\n"); 
	AddKeyText(player, buffer, sizeof(buffer));

	utils::PrintAlert(player->GetController(), "%s", buffer);
}