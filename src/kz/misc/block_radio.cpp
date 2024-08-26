#include "kz/kz.h"

constexpr const char *blockedCommands[] = {"playerchatwheel", "player_ping", "roger",     "negative",    "cheer",      "holdpos",
										   "followme",        "thanks",      "go",        "fallback",    "sticktog",   "holdpos",
										   "followme",        "roger",       "negative",  "cheer",       "compliment", "thanks",
										   "enemyspot",       "needbackup",  "takepoint", "sectorclear", "inposition"};

META_RES KZ::misc::CheckBlockedRadioCommands(const char *cmd)
{
	for (size_t i = 0; i < sizeof(blockedCommands) / sizeof(blockedCommands[0]); i++)
	{
		if (!V_stricmp(cmd, blockedCommands[i]))
		{
			return MRES_SUPERCEDE;
		}
	}
	return MRES_IGNORED;
}
