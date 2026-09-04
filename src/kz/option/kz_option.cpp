#include "kz_option.h"
#include "kz/db/kz_db.h"
#include "kz/global/kz_global.h"
#include "kz/racing/kz_racing.h"
#include "utils/eventlisteners.h"
#include "utils/logging.h"

#include <mutex>

static_global KeyValues *pServerCfgKeyValues;

// Recursive because logging itself reads options in `KZLoggingListener::OpenFile()`
// so a log line emitted under this lock would cause deadlock otherwise.
static_global std::recursive_mutex serverCfgMutex;

static_function void GetServerCfgPath(char (&path)[1024])
{
	V_snprintf(path, sizeof(path), "%s%s", g_SMAPI->GetBaseDir(), "/cfg/cs2kz-server-config.txt");
}

IMPLEMENT_CLASS_EVENT_LISTENER(KZOptionService, KZOptionServiceEventListener);

// Helper function to merge loaded preferences with existing ones
// Source values overwrite target values, except for keys in excludeKeys
// excludeKeys: list of keys to never overwrite (user-set preferences)
static_function void MergePreferences(KeyValues3 *target, KeyValues3 *source, const CUtlVector<CUtlString> *excludeKeys = nullptr)
{
	if (!source || !target)
	{
		return;
	}

	// Iterate through all members in the source
	int memberCount = source->GetMemberCount();
	for (int i = 0; i < memberCount; i++)
	{
		const char *name = source->GetMemberName(i);

		// Skip if this key is in the exclude list (user-set)
		if (excludeKeys && excludeKeys->Find(name) != excludeKeys->InvalidIndex())
		{
			continue;
		}

		KeyValues3 *sourceMember = source->GetMember(i);
		KeyValues3 *targetMember = target->FindOrCreateMember(name);

		// Copy the value based on type
		KV3TypeEx_t type = sourceMember->GetTypeEx();
		KV3Type_t baseType = (KV3Type_t)(type & 0x0F);

		switch (baseType)
		{
			case KV3_TYPE_BOOL:
				targetMember->SetBool(sourceMember->GetBool());
				break;
			case KV3_TYPE_INT:
				targetMember->SetInt(sourceMember->GetInt());
				break;
			case KV3_TYPE_UINT:
				targetMember->SetUInt(sourceMember->GetUInt());
				break;
			case KV3_TYPE_DOUBLE:
				targetMember->SetDouble(sourceMember->GetDouble());
				break;
			case KV3_TYPE_STRING:
				targetMember->SetString(sourceMember->GetString());
				break;
			case KV3_TYPE_TABLE:
				// Recursively merge tables
				targetMember->SetToEmptyTable();
				MergePreferences(targetMember, sourceMember, excludeKeys);
				break;
			case KV3_TYPE_ARRAY:
			{
				// Copy arrays
				int arrayCount = sourceMember->GetArrayElementCount();
				targetMember->SetArrayElementCount(arrayCount);
				for (int j = 0; j < arrayCount; j++)
				{
					KeyValues3 *sourceElement = sourceMember->GetArrayElement(j);
					KeyValues3 *targetElement = targetMember->GetArrayElement(j);
					if (sourceElement && targetElement)
					{
						KV3TypeEx_t elemType = sourceElement->GetTypeEx();
						KV3Type_t elemBaseType = (KV3Type_t)(elemType & 0x0F);

						switch (elemBaseType)
						{
							case KV3_TYPE_BOOL:
								targetElement->SetBool(sourceElement->GetBool());
								break;
							case KV3_TYPE_INT:
								targetElement->SetInt(sourceElement->GetInt());
								break;
							case KV3_TYPE_UINT:
								targetElement->SetUInt(sourceElement->GetUInt());
								break;
							case KV3_TYPE_DOUBLE:
								targetElement->SetDouble(sourceElement->GetDouble());
								break;
							case KV3_TYPE_STRING:
								targetElement->SetString(sourceElement->GetString());
								break;
							case KV3_TYPE_TABLE:
								targetElement->SetToEmptyTable();
								MergePreferences(targetElement, sourceElement, excludeKeys);
								break;
							default:
								break;
						}
					}
				}
				break;
			}
			case KV3_TYPE_BINARY_BLOB:
			{
				// Copy binary blobs
				const byte *blob = sourceMember->GetBinaryBlob();
				int size = sourceMember->GetBinaryBlobSize();
				if (blob && size > 0)
				{
					targetMember->SetToBinaryBlob(blob, size);
				}
				break;
			}
			default:
				break;
		}
	}
}

void KZOptionService::LoadDefaultOptions()
{
	char serverCfgPath[1024];
	GetServerCfgPath(serverCfgPath);
	{
		std::lock_guard _guard(serverCfgMutex);
		pServerCfgKeyValues = new KeyValues("ServerConfig");
		pServerCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);
	}
}

bool KZOptionService::ReloadOptions()
{
	char serverCfgPath[1024];
	GetServerCfgPath(serverCfgPath);

	// Check if the file is valid before replacing the existing.
	KeyValues *probe = new KeyValues("ServerConfig");
	bool valid = probe->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);
	delete probe;

	if (!valid)
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to parse `cfg/cs2kz-server-config.txt`; keeping the current configuration.\n");
		return false;
	}

	std::lock_guard _guard(serverCfgMutex);
	if (!pServerCfgKeyValues)
	{
		return false;
	}

	pServerCfgKeyValues->Clear();
	return pServerCfgKeyValues->LoadFromFile(g_pFullFileSystem, serverCfgPath, nullptr);
}

const char *KZOptionService::GetOptionStr(const char *optionName, const char *defaultValue)
{
	std::lock_guard _guard(serverCfgMutex);
	return pServerCfgKeyValues ? pServerCfgKeyValues->GetString(optionName, defaultValue) : defaultValue;
}

f64 KZOptionService::GetOptionFloat(const char *optionName, f64 defaultValue)
{
	std::lock_guard _guard(serverCfgMutex);
	return pServerCfgKeyValues ? pServerCfgKeyValues->GetFloat(optionName, defaultValue) : defaultValue;
}

i64 KZOptionService::GetOptionInt(const char *optionName, i64 defaultValue)
{
	std::lock_guard _guard(serverCfgMutex);
	return pServerCfgKeyValues ? pServerCfgKeyValues->GetInt(optionName, defaultValue) : defaultValue;
}

KeyValues *KZOptionService::GetOptionKV(const char *optionName)
{
	std::lock_guard _guard(serverCfgMutex);
	return pServerCfgKeyValues ? pServerCfgKeyValues->FindKey(optionName) : nullptr;
}

void KZOptionService::InitOptions()
{
	LoadDefaultOptions();
}

void KZOptionService::Cleanup()
{
	std::lock_guard _guard(serverCfgMutex);
	if (pServerCfgKeyValues)
	{
		delete pServerCfgKeyValues;
		pServerCfgKeyValues = nullptr;
	}
}

CON_COMMAND_F(kz_reload_config, "Reload server config file and reconnect to the API.", FCVAR_NONE)
{
	if (!KZOptionService::ReloadOptions())
	{
		return;
	}

	KZ_LOG_INFO(LogChannel::General, "Reloaded `cfg/cs2kz-server-config.txt`.\n");

	kz_log_to_file.Set((bool)KZOptionService::GetOptionInt("logToFile", true));
	KZGlobalService::ReloadConfig();
	KZRacingService::ReloadConfig();

	KZ_LOG_INFO(LogChannel::General, "Changes to the local database and tip intervals will only take effect after a server restart.\n");
}

// 0 when there is no stamp, or when it is far enough ahead of this server's clock to be untrusted.
static_function i64 ReadPrefsStamp(KeyValues3 *prefs)
{
	KeyValues3 *member = prefs->FindMember(KZ_PREF_UPDATED_AT);
	if (!member)
	{
		return 0;
	}
	const i64 stamp = member->GetInt64(0);
	time_t now = 0;
	time(&now);
	if (stamp <= 0 || stamp > (i64)now + KZ_PREF_STAMP_MAX_SKEW)
	{
		return 0;
	}
	return stamp;
}

void KZOptionService::StampPreferences()
{
	time_t now = 0;
	time(&now);
	this->prefKV.FindOrCreateMember(KZ_PREF_UPDATED_AT)->SetInt64((i64)now);
}

bool KZOptionService::ShouldApplyPrefs(i64 incomingStamp, i32 incomingTier)
{
	if (this->dataState == NONE)
	{
		return true;
	}

	if (incomingStamp && this->loadedStamp)
	{
		return incomingStamp > this->loadedStamp;
	}
	return incomingTier > (i32)this->dataState;
}

void KZOptionService::InitializeLocalPrefs(CUtlString text)
{
	if (this->localLoaded)
	{
		return;
	}
	this->localLoaded = true;
	if (text.IsEmpty())
	{
		text = "{\n}";
	}

	// Load the preferences from the database into a temporary KV
	KeyValues3 loadedPrefs(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);
	CUtlString error;
	LoadKV3FromJSON(&loadedPrefs, &error, text.Get(), "");
	if (!error.IsEmpty())
	{
		KZ_LOG_WARN(LogChannel::DB, "Error fetching local preference: %s\n", error.Get());
		return;
	}

	const i64 stamp = ReadPrefsStamp(&loadedPrefs);
	if (!this->ShouldApplyPrefs(stamp, LOCAL))
	{
		return;
	}

	// Merge loaded preferences, excluding user-set preferences
	MergePreferences(&this->prefKV, &loadedPrefs, &this->userSetPrefs);
	this->loadedStamp = stamp;

	this->dataState = LOCAL;
	// Calling this before the player is ingame will create unwanted race conditions.
	// We need to make sure the player is both authenticated and ingame.
	if (this->player->IsInGame())
	{
		CALL_FORWARD(eventListeners, OnPlayerPreferencesLoaded, this->player);
		this->currentState = this->dataState;
	}
}

void KZOptionService::InitializeGlobalPrefs(std::string json)
{
	assert(!json.empty() && "API always sends at least an empty object");

	if (this->globalLoaded)
	{
		return;
	}
	this->globalLoaded = true;
	// Load the preferences from the API into a temporary KV
	KeyValues3 loadedPrefs(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);
	CUtlString error;
	LoadKV3FromJSON(&loadedPrefs, &error, json.c_str(), "");

	if (!error.IsEmpty())
	{
		KZ_LOG_WARN(LogChannel::Option, "Error loading global preferences: %s\n", error.Get());
		return;
	}

	const i64 stamp = ReadPrefsStamp(&loadedPrefs);
	if (!this->ShouldApplyPrefs(stamp, GLOBAL))
	{
		return;
	}

	// Merge loaded preferences, excluding user-set preferences
	MergePreferences(&this->prefKV, &loadedPrefs, &this->userSetPrefs);
	this->loadedStamp = stamp;

	this->dataState = GLOBAL;

	KZ_LOG_INFO(LogChannel::Option, "Loaded global preferences for %s (%llu).\n", this->player->GetName(), this->player->GetSteamId64());

	// Calling this before the player is ingame will create unwanted race conditions.
	// We need to make sure the player is both authenticated and ingame.
	if (this->player->IsInGame())
	{
		CALL_FORWARD(eventListeners, OnPlayerPreferencesLoaded, this->player);
		this->currentState = this->dataState;
	}
}

void KZOptionService::SaveLocalPrefs()
{
	if (this->player->IsFakeClient() || !this->player->IsAuthenticated())
	{
		return;
	}
	this->StampPreferences();
	CUtlString error, output;
	SaveKV3AsJSON(&this->prefKV, &error, &output);
	if (!error.IsEmpty())
	{
		KZ_LOG_WARN(LogChannel::DB, "Error saving local preference: %s\n", error.Get());
		return;
	}
	this->player->databaseService->SavePrefs(output);
}

void KZOptionService::OnPlayerActive()
{
	if (this->currentState <= this->dataState)
	{
		CALL_FORWARD(eventListeners, OnPlayerPreferencesLoaded, this->player);
		this->currentState = this->dataState;
	}
}
