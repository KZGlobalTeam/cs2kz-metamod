#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "keyvalues3.h"
#include "utils/eventlisteners.h"

class KZOptionServiceEventListener
{
public:
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player) {};
	virtual void OnPlayerPreferenceChanged(KZPlayer *player, const char *optionName) {};
};

class KZOptionService : public KZBaseService
{
	using KZBaseService::KZBaseService;

	DECLARE_CLASS_EVENT_LISTENER(KZOptionServiceEventListener);

public:
	static void InitOptions();
	static void Cleanup();
	static const char *GetOptionStr(const char *optionName, const char *defaultValue = "");
	static f64 GetOptionFloat(const char *optionName, f64 defaultValue = 0.0);
	static i64 GetOptionInt(const char *optionName, i64 defaultValue = 0);
	static KeyValues *GetOptionKV(const char *optionName);

private:
	static void LoadDefaultOptions();

private:
	enum
	{
		NONE = 0,
		LOCAL,
		GLOBAL
	} dataState, currentState;

	KeyValues3 prefKV = KeyValues3(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);
	CUtlVector<CUtlString> userSetPrefs; // Track user-modified preferences

public:
	void Reset()
	{
		dataState = NONE;
		currentState = NONE;
		prefKV.SetToEmptyTable();
		userSetPrefs.Purge();
	}

	void InitializeLocalPrefs(CUtlString text);
	void InitializeGlobalPrefs(std::string json);

	void SaveLocalPrefs();

	void SaveGlobalPrefs() {}

	void OnPlayerActive();

	void OnClientDisconnect()
	{
		SaveLocalPrefs();
		SaveGlobalPrefs();
	}

	void GetPreferencesAsJSON(CUtlString *error, CUtlString *output)
	{
		SaveKV3AsJSON(&this->prefKV, error, output);
	}

	// Due to the way keyvalues3.h is written, we can't template these functions.
	void SetPreferenceBool(const char *optionName, bool value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		prefKV.FindOrCreateMember(optionName)->SetBool(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	bool GetPreferenceBool(const char *optionName, bool defaultValue = false)
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			return defaultValue;
		}
		return option->GetBool(defaultValue);
	}

	void SetPreferenceFloat(const char *optionName, f64 value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		prefKV.FindOrCreateMember(optionName)->SetDouble(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	f64 GetPreferenceFloat(const char *optionName, f64 defaultValue = 0.0)
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			return defaultValue;
		}
		return option->GetDouble(defaultValue);
	}

	void SetPreferenceInt(const char *optionName, i64 value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		prefKV.FindOrCreateMember(optionName)->SetInt64(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	i64 GetPreferenceInt(const char *optionName, i64 defaultValue = 0)
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			return defaultValue;
		}
		return option->GetInt64(defaultValue);
	}

	void SetPreferenceStr(const char *optionName, const char *value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		prefKV.FindOrCreateMember(optionName)->SetString(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	const char *GetPreferenceStr(const char *optionName, const char *defaultValue = "")
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			return defaultValue;
		}
		// DebugPrintKV3(&prefKV);
		return option->GetString(defaultValue);
	}

	void SetPreferenceVector(const char *optionName, const Vector &value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		prefKV.FindOrCreateMember(optionName)->SetVector(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	Vector GetPreferenceVector(const char *optionName, const Vector &defaultValue = Vector(0.0f, 0.0f, 0.0f))
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			return defaultValue;
		}
		return option->GetVector(defaultValue);
	}

	void SetPreferenceTable(const char *optionName, const KeyValues3 &value)
	{
		if (userSetPrefs.Find(optionName) == userSetPrefs.InvalidIndex())
		{
			userSetPrefs.AddToTail(optionName); // Mark as user-set
		}
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName);
		option->SetToEmptyTable();
		*option = value;
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	void GetPreferenceTable(const char *optionName, KeyValues3 &output, const KeyValues3 &defaultValue = KeyValues3())
	{
		KeyValues3 *option = prefKV.FindMember(optionName);
		if (!option)
		{
			output = defaultValue;
			return;
		}
		output = *option;
	}
};
