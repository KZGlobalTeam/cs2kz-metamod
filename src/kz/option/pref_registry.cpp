#include "kz/option/pref_registry.h"
#include "kz/option/kz_option.h"

#include "tier1/strtools.h"

#include "tier0/memdbgon.h"

static_global std::vector<KZ::prefs::Entry> registry;
static_global bool registryBuilt = false;

static_function void AddEntry(const char *key, KZOptStorage storage)
{
	if (!key || storage == KZOptStorage::None)
	{
		return;
	}
	for (const KZ::prefs::Entry &entry : registry)
	{
		if (V_strcmp(entry.key, key) == 0)
		{
			return;
		}
	}
	registry.push_back({key, storage});
}

static_function void CollectNode(KZOptNode *node)
{
	if (!node)
	{
		return;
	}
	for (const KZOptItem &item : node->items)
	{
		AddEntry(item.prefKey, item.storage);
		AddEntry(item.yKey, item.storage);
	}
	for (KZOptNode *sub : node->subs)
	{
		CollectNode(sub);
	}
}

const std::vector<KZ::prefs::Entry> &KZ::prefs::GetRegistry()
{
	if (!registryBuilt)
	{
		registryBuilt = true;
		for (KZOptNode *category : KZ::menu::GetTree())
		{
			CollectNode(category);
		}
	}
	return registry;
}

const KZ::prefs::Entry *KZ::prefs::FindEntry(const char *key)
{
	for (const KZ::prefs::Entry &entry : KZ::prefs::GetRegistry())
	{
		if (V_strcmp(entry.key, key) == 0)
		{
			return &entry;
		}
	}
	return NULL;
}

bool KZ::prefs::ReadValue(KZPlayer *player, const KZ::prefs::Entry &entry, char *out, i32 outLen)
{
	auto *opts = player->optionService;
	if (!opts->HasPreference(entry.key))
	{
		return false;
	}
	switch (entry.storage)
	{
		case KZOptStorage::Bool:
			V_snprintf(out, outLen, "%i", opts->GetPreferenceBool(entry.key) ? 1 : 0);
			return true;
		case KZOptStorage::Int:
			V_snprintf(out, outLen, "%lli", (long long)opts->GetPreferenceInt(entry.key));
			return true;
		case KZOptStorage::Float:
			V_snprintf(out, outLen, "%g", opts->GetPreferenceFloat(entry.key));
			return true;
		case KZOptStorage::Str:
		{
			const char *value = opts->GetPreferenceStr(entry.key);
			V_snprintf(out, outLen, "\"%s\"", value ? value : "");
			return true;
		}
		case KZOptStorage::Vector:
		{
			// The layout the engine uses for vector convars.
			const Vector value = opts->GetPreferenceVector(entry.key);
			V_snprintf(out, outLen, "\"%f %f %f\"", value.x, value.y, value.z);
			return true;
		}
		default:
			return false;
	}
}

bool KZ::prefs::ApplyValue(KZPlayer *player, const KZ::prefs::Entry &entry, const char *value)
{
	if (!value || !value[0])
	{
		return false;
	}
	auto *opts = player->optionService;
	switch (entry.storage)
	{
		case KZOptStorage::Bool:
		{
			// Takes "true"/"false" as well as the "1"/"0" the export writes.
			bool parsed = false;
			if (!V_StringToValue<bool>(value, parsed))
			{
				return false;
			}
			opts->SetPreferenceBool(entry.key, parsed);
			return true;
		}
		case KZOptStorage::Int:
		{
			int64 parsed = 0;
			if (!V_StringToValue<int64>(value, parsed))
			{
				return false;
			}
			opts->SetPreferenceInt(entry.key, parsed);
			return true;
		}
		case KZOptStorage::Float:
		{
			float64 parsed = 0.0;
			if (!V_StringToValue<float64>(value, parsed))
			{
				return false;
			}
			opts->SetPreferenceFloat(entry.key, parsed);
			return true;
		}
		case KZOptStorage::Str:
			opts->SetPreferenceStr(entry.key, value);
			return true;
		case KZOptStorage::Vector:
		{
			Vector parsed;
			if (!V_StringToValue<Vector>(value, parsed))
			{
				return false;
			}
			opts->SetPreferenceVector(entry.key, parsed);
			return true;
		}
		default:
			return false;
	}
}
