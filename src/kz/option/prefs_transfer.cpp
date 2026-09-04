// Preferences travel between servers as userinfo convars named kzp_<prefKey>, pasted with setinfo.
// The server can only read them: a setinfo convar is FCVAR_USERINFO only, which the client's
// SetConVar handler refuses to write.
#include "kz/option/pref_registry.h"
#include "kz/option/kz_option.h"
#include "kz/language/kz_language.h"
#include "utils/cvarquery.h"
#include "utils/interfaces.h"
#include "utils/ctimer.h"
#include "utils/simplecmds.h"

#include "tier1/strtools.h"

#include <string>
#include <unordered_map>

#include "tier0/memdbgon.h"

#define KZ_PREF_CVAR_PREFIX "kzp_"
#define KZ_PREF_STAMP_CVAR  KZ_PREF_CVAR_PREFIX "_stamp"
#define KZ_PREF_LAST_IMPORT "lastImportStamp"
// A pasted block arrives one message per line; wait for all of them before applying.
#define KZ_PREF_IMPORT_DELAY 0.25f

struct ImportState
{
	std::unordered_map<std::string, std::string> staged;
	i64 stamp {};
	bool prefsLoaded {};
	bool drainPending {};
};

static_global ImportState imports[MAXPLAYERS];

// === Import ==========================================================================

static_function void DrainImport(KZPlayer *player, bool fromConnect)
{
	ImportState &state = imports[player->GetPlayerSlot().Get()];
	if (state.staged.empty())
	{
		return;
	}
	auto *opts = player->optionService;
	// An unstamped block was typed by hand: apply it, but don't record it.
	const bool stamped = state.stamp > 0;
	if (stamped ? state.stamp <= opts->GetPreferenceInt(KZ_PREF_LAST_IMPORT, 0) : fromConnect)
	{
		state.staged.clear();
		return;
	}

	i32 applied = 0;
	for (const auto &pair : state.staged)
	{
		const KZ::prefs::Entry *entry = KZ::prefs::FindEntry(pair.first.c_str());
		if (entry && KZ::prefs::ApplyValue(player, *entry, pair.second.c_str()))
		{
			applied++;
		}
	}
	state.staged.clear();
	if (stamped)
	{
		opts->SetPreferenceInt(KZ_PREF_LAST_IMPORT, state.stamp);
	}
	if (applied > 0)
	{
		player->languageService->PrintChat(true, false, "Prefs - Imported", applied);
	}
}

static_function f64 DrainImportTimer(CPlayerUserId userID)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(userID);
	if (player)
	{
		imports[player->GetPlayerSlot().Get()].drainPending = false;
		DrainImport(player, false);
	}
	return 0.0f;
}

static_function void OnClientConVar(CPlayerSlot slot, const char *name, const char *value)
{
	if (slot.Get() < 0 || slot.Get() >= MAXPLAYERS || V_strncmp(name, KZ_PREF_CVAR_PREFIX, sizeof(KZ_PREF_CVAR_PREFIX) - 1) != 0)
	{
		return;
	}
	// An empty value is how a player clears one of these.
	if (!value || !value[0])
	{
		return;
	}

	ImportState &state = imports[slot.Get()];
	if (V_strcmp(name, KZ_PREF_STAMP_CVAR) == 0)
	{
		int64 stamp = 0;
		if (V_StringToValue<int64>(value, stamp))
		{
			state.stamp = stamp;
		}
	}
	else
	{
		state.staged[name + sizeof(KZ_PREF_CVAR_PREFIX) - 1] = value;
	}

	// On connect there is no stored stamp yet, so that drain runs from OnPlayerPreferencesLoaded.
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	if (!player || !player->GetClient() || !state.prefsLoaded || state.drainPending)
	{
		return;
	}
	state.drainPending = true;
	StartTimer<CPlayerUserId>(DrainImportTimer, player->GetClient()->GetUserID(), KZ_PREF_IMPORT_DELAY, true, true);
}

// The convars a client already has on connect never reach ProcessSetConVar: that batch is applied
// from the connection handler, which is not in the vtable. ApplyConVars still puts them in the
// client's userinfo KeyValues, which is what GetClientConVarValue reads.
static_function void ReadImport(KZPlayer *player)
{
	const CPlayerSlot slot = player->GetPlayerSlot();
	const char *stampValue = interfaces::pEngine->GetClientConVarValue(slot, KZ_PREF_STAMP_CVAR);
	int64 stamp = 0;
	if (!stampValue || !stampValue[0] || !V_StringToValue<int64>(stampValue, stamp) || stamp <= 0)
	{
		return;
	}
	auto *opts = player->optionService;
	if (stamp <= opts->GetPreferenceInt(KZ_PREF_LAST_IMPORT, 0))
	{
		return;
	}

	i32 applied = 0;
	for (const KZ::prefs::Entry &entry : KZ::prefs::GetRegistry())
	{
		char cvar[128];
		V_snprintf(cvar, sizeof(cvar), "%s%s", KZ_PREF_CVAR_PREFIX, entry.key);
		const char *value = interfaces::pEngine->GetClientConVarValue(slot, cvar);
		if (value && value[0] && KZ::prefs::ApplyValue(player, entry, value))
		{
			applied++;
		}
	}
	opts->SetPreferenceInt(KZ_PREF_LAST_IMPORT, stamp);
	if (applied > 0)
	{
		player->languageService->PrintChat(true, false, "Prefs - Imported", applied);
	}
}

// Runs after the local and global stores are merged in, so the imported keys land in userSetPrefs
// and a later global load cannot merge over them.
static_global class KZOptionServiceEventListener_PrefsTransfer : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		ImportState &state = imports[player->GetPlayerSlot().Get()];
		if (!state.prefsLoaded)
		{
			state.prefsLoaded = true;
			ReadImport(player);
		}
		DrainImport(player, true);
	}
} optionEventListener;

// === Export ==========================================================================

static_function void ExportPrefs(KZPlayer *player, i64)
{
	player->languageService->PrintConsole(false, false, "Prefs - Export Header");
	player->PrintConsole(false, false, "setinfo %s %lli;", KZ_PREF_STAMP_CVAR, (long long)time(NULL));

	i32 count = 0;
	for (const KZ::prefs::Entry &entry : KZ::prefs::GetRegistry())
	{
		char value[256];
		if (!KZ::prefs::ReadValue(player, entry, value, sizeof(value)))
		{
			continue;
		}
		player->PrintConsole(false, false, "setinfo %s%s %s;", KZ_PREF_CVAR_PREFIX, entry.key, value);
		count++;
	}

	player->languageService->PrintConsole(false, false, "Prefs - Export Footer");
	player->languageService->PrintChat(true, false, "Prefs - Exported", count);
}

void KZ::prefs::Init()
{
	cvarquery::SetChangeCallback(OnClientConVar);
	KZOptionService::RegisterEventListener(&optionEventListener);
}

void KZ::prefs::RegisterMenu(KZOptNode *node)
{
	KZ::menu::AddButton(node, "Menu - Export Prefs", ExportPrefs);
}

void KZ::prefs::OnClientDisconnect(CPlayerSlot slot)
{
	if (slot.Get() >= 0 && slot.Get() < MAXPLAYERS)
	{
		imports[slot.Get()] = ImportState();
	}
}

SCMD(kz_exportprefs, SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	ExportPrefs(player, 0);
	return MRES_SUPERCEDE;
}
