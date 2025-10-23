#ifndef KZ_REPLAYBOT_H
#define KZ_REPLAYBOT_H

#include "sdk/datatypes.h"

class CCSPlayerController;
class KZPlayer;
struct GeneralReplayHeader;

namespace KZ::replaysystem::bot
{
	// Bot lifecycle management
	void CheckBots();
	void SpawnBot();
	void KickBot();
	void MakeBotAlive();
	void MoveBotToSpec();

	// Bot state management
	CCSPlayerController *GetBot();
	bool IsValidBot(CCSPlayerController *controller);
	KZPlayer *GetBotPlayer();

	// Bot setup and configuration
	void InitializeBotForReplay(const GeneralReplayHeader &header);

	// Bot spectator handling
	void SpectateBot(KZPlayer *spectator);
} // namespace KZ::replaysystem::bot

#endif // KZ_REPLAYBOT_H
