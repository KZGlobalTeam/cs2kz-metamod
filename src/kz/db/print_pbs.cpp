#include "kz_db.h"

void QueryPB(KZPlayer *callingPlayer, u64 steamID64, CUtlString mapName, CUtlString courseName, CUtlString modeName, CUtlString styleNames) {}

SCMD_CALLBACK(KZDatabaseService::CommandKZPB)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	return MRES_SUPERCEDE;
}
