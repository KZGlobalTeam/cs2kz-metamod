#pragma once
#include "../kz.h"
#include "utils/utils.h"
#include "KeyValues.h"
#include "interfaces/interfaces.h"
#include "filesystem.h"
#include "keyvalues3.h"

class KZOptionServiceEventListener
{
public:
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player) {};
	virtual void OnPlayerPreferenceChanged(KZPlayer *player, const char *optionName) {};
};

class KZOptionService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	static bool RegisterEventListener(KZOptionServiceEventListener *eventListener);
	static bool UnregisterEventListener(KZOptionServiceEventListener *eventListener);

	static void InitOptions();
	static const char *GetOptionStr(const char *optionName, const char *defaultValue = "");
	static f64 GetOptionFloat(const char *optionName, f64 defaultValue = 0.0);
	static i64 GetOptionInt(const char *optionName, i64 defaultValue = 0);
	static KeyValues *GetOptionKV(const char *optionName);

private:
	static void LoadDefaultOptions();

	static CUtlVector<KZOptionServiceEventListener *> eventListeners;

private:
	enum
	{
		NONE = 0,
		LOCAL,
		GLOBAL
	} initState;

	KeyValues3 prefKV = KeyValues3(KV3_TYPEEX_TABLE, KV3_SUBTYPE_UNSPECIFIED);

public:
	void Reset()
	{
		initState = NONE;
		prefKV.SetToEmptyTable();
	}

	void InitializeLocalPrefs(CUtlString text);

	bool IsInitialized()
	{
		return initState > NONE;
	}

	void SaveLocalPrefs();

	void SaveGlobalPrefs() {}

	void OnClientDisconnect()
	{
		SaveLocalPrefs();
		SaveGlobalPrefs();
	}

	// Due to the way keyvalues3.h is written, we can't template these functions.
	void SetPreferenceBool(const char *optionName, bool value)
	{
		prefKV.FindOrCreateMember(optionName)->SetBool(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	bool GetPreferenceBool(const char *optionName, bool defaultValue = false)
	{
		if (!IsInitialized())
		{
			return defaultValue;
		}
		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			option->SetBool(defaultValue);
		}
		return option->GetBool(defaultValue);
	}

	void SetPreferenceFloat(const char *optionName, f64 value)
	{
		prefKV.FindOrCreateMember(optionName)->SetDouble(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	f64 GetPreferenceFloat(const char *optionName, f64 defaultValue = 0.0)
	{
		if (!IsInitialized())
		{
			return defaultValue;
		}
		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			option->SetDouble(defaultValue);
		}
		return option->GetDouble(defaultValue);
	}

	void SetPreferenceInt(const char *optionName, i64 value)
	{
		prefKV.FindOrCreateMember(optionName)->SetInt64(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	i64 GetPreferenceInt(const char *optionName, i64 defaultValue = 0)
	{
		if (!IsInitialized())
		{
			return defaultValue;
		}
		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			option->SetInt64(defaultValue);
		}
		return option->GetInt64(defaultValue);
	}

	void SetPreferenceStr(const char *optionName, const char *value)
	{
		prefKV.FindOrCreateMember(optionName)->SetString(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	const char *GetPreferenceStr(const char *optionName, const char *defaultValue = "")
	{
		if (!IsInitialized())
		{
			return defaultValue;
		}
		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			option->SetString(defaultValue);
		}
		return option->GetString(defaultValue);
	}

	void SetPreferenceVector(const char *optionName, const Vector &value)
	{
		prefKV.FindOrCreateMember(optionName)->SetVector(value);
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	Vector GetPreferenceVector(const char *optionName, const Vector &defaultValue = Vector(0.0f, 0.0f, 0.0f))
	{
		if (!IsInitialized())
		{
			return defaultValue;
		}
		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			option->SetVector(defaultValue);
		}
		return option->GetVector(defaultValue);
	}

	void SetPreferenceTable(const char *optionName, const KeyValues3 &value)
	{
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName);
		option->SetToEmptyTable();
		*option = value;
		CALL_FORWARD(eventListeners, OnPlayerPreferenceChanged, this->player, optionName);
	}

	void GetPreferenceTable(const char *optionName, KeyValues3 &output, const KeyValues3 &defaultValue = KeyValues3())
	{
		if (!IsInitialized())
		{
			output = defaultValue;
			return;
		}

		bool created = false;
		KeyValues3 *option = prefKV.FindOrCreateMember(optionName, &created);
		if (created)
		{
			*option = defaultValue;
		}
		output = *option;
	}
};
