#include "kz/kz.h"

constexpr const char *blockedCommands[] = {
	"playerchatwheel", "player_ping", "holdpos",     "regroup",    "followme", "takingfire", "go",        "fallback",
	"sticktog",        "cheer",       "thanks",      "compliment", "report",   "roger",      "enemyspot", "needbackup",
	"sectorclear",     "inposition",  "reportingin", "getout",     "negative", "enemydown",
};

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
