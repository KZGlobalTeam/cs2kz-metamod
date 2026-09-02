#pragma once
#include "cbaseentity.h"
#include "tier1/utlstring.h"
#ifndef IDA_IGNORE

// Panel ids, class names and dialog variable names are interned into the layout's networked string
// vectors. The game refuses to intern past this and warns, so match it rather than growing forever.
#define HUD_LAYOUT_MAX_INTERNED_STRINGS 1024

enum EHudPanelClassStatus_t : uint32
{
	k_eHudPanelClassStatus_Undefined = (uint32)-1,
	k_eHudPanelClassStatus_DoesNotHaveClass = 0,
	k_eHudPanelClassStatus_HasClass = 1,
};

// Elements of the layout state's networked vectors.
// Neither type notifies on its own: the game marks the owning vector field instead, so these exist
// only to name field offsets. Never call NetworkStateChanged() on them.
class HUDPanelHasClass_t
{
public:
	DECLARE_SCHEMA_CLASS_BASE(HUDPanelHasClass_t, 0)

	SCHEMA_FIELD_POINTER(uint16, m_nPanelIdIndex)
	SCHEMA_FIELD_POINTER(uint16, m_nClassNameIndex)
	SCHEMA_FIELD_POINTER(EHudPanelClassStatus_t, m_eClassStatus)
};

class HUDPanelDialogVariableString_t
{
public:
	DECLARE_SCHEMA_CLASS_BASE(HUDPanelDialogVariableString_t, 0)

	SCHEMA_FIELD_POINTER(uint16, m_nPanelIdIndex)
	SCHEMA_FIELD_POINTER(uint16, m_nDialogVariableIndex)
	SCHEMA_FIELD_POINTER(CUtlString, m_sValue)
	SCHEMA_FIELD_POINTER(bool, m_bIsSet)
};

// Per-player (or global) UI state of a custom_hud_layout entity.
// Not an entity itself: its NetworkStateChanged lives on vtable index 1.
class CCSCustomHudLayoutState
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CCSCustomHudLayoutState, 1)

	SCHEMA_FIELD(bool, m_bInputCaptureEnabled)
	SCHEMA_FIELD_COLLECTION(HUDPanelHasClass_t, m_vecHasClasses)
	SCHEMA_FIELD_COLLECTION(HUDPanelDialogVariableString_t, m_vecDialogVariableStrings)
	SCHEMA_FIELD(CPlayerSlot, m_playerSlot)

	// Marks the whole state (hence the whole entity) for a full network resend.
	void MarkFullChanged()
	{
		NetworkStateChangedData data(true);
		CALL_VIRTUAL(void, 1, this, &data);
	}

	void MarkElementChanged(const SchemaKey &key, int arrayIndex)
	{
		if (!key.networked)
		{
			MarkFullChanged();
			return;
		}
		NetworkStateChangedData data((uint32)key.offset, arrayIndex);
		CALL_VIRTUAL(void, 1, this, &data);
	}

	void MarkHasClassChanged(int arrayIndex)
	{
		static const SchemaKey key = schema::GetOffset(m_className, m_classNameHash, "m_vecHasClasses", hash_32_fnv1a_const("m_vecHasClasses"));
		MarkElementChanged(key, arrayIndex);
	}

	void MarkDialogVarChanged(int arrayIndex)
	{
		static const SchemaKey key =
			schema::GetOffset(m_className, m_classNameHash, "m_vecDialogVariableStrings", hash_32_fnv1a_const("m_vecDialogVariableStrings"));
		MarkElementChanged(key, arrayIndex);
	}

	// Sets the status of an interned (panelId, className) pair, appending the entry when missing.
	bool SetHasClass(uint16 panelIdIndex, uint16 classNameIndex, EHudPanelClassStatus_t status)
	{
		CSchemaCollection<HUDPanelHasClass_t> classes = m_vecHasClasses();

		for (int i = 0; i < classes.Count(); ++i)
		{
			HUDPanelHasClass_t *entry = classes.Element(i);

			if (*entry->m_nPanelIdIndex() == panelIdIndex && *entry->m_nClassNameIndex() == classNameIndex)
			{
				*entry->m_eClassStatus() = status;
				MarkHasClassChanged(i);
				return true;
			}
		}

		HUDPanelHasClass_t *added = classes.AddToTail();

		if (!added)
		{
			return false;
		}

		*added->m_nPanelIdIndex() = panelIdIndex;
		*added->m_nClassNameIndex() = classNameIndex;
		*added->m_eClassStatus() = status;
		MarkFullChanged();
		return true;
	}

	// Clears the value of an interned (panelId, variableName) pair, so this state defers to the
	// global one again. Does nothing when the pair was never set: the game does not append here.
	bool UnsetDialogVariableString(uint16 panelIdIndex, uint16 dialogVariableIndex)
	{
		CSchemaCollection<HUDPanelDialogVariableString_t> variables = m_vecDialogVariableStrings();

		for (int i = 0; i < variables.Count(); ++i)
		{
			HUDPanelDialogVariableString_t *entry = variables.Element(i);

			if (*entry->m_nPanelIdIndex() == panelIdIndex && *entry->m_nDialogVariableIndex() == dialogVariableIndex)
			{
				*entry->m_bIsSet() = false;
				MarkDialogVarChanged(i);
				return true;
			}
		}

		return false;
	}

	// Sets the value of an interned (panelId, variableName) pair, appending the entry when missing.
	bool SetDialogVariableString(uint16 panelIdIndex, uint16 dialogVariableIndex, const char *value)
	{
		CSchemaCollection<HUDPanelDialogVariableString_t> variables = m_vecDialogVariableStrings();

		for (int i = 0; i < variables.Count(); ++i)
		{
			HUDPanelDialogVariableString_t *entry = variables.Element(i);

			if (*entry->m_nPanelIdIndex() == panelIdIndex && *entry->m_nDialogVariableIndex() == dialogVariableIndex)
			{
				*entry->m_sValue() = value;
				*entry->m_bIsSet() = true;
				MarkDialogVarChanged(i);
				return true;
			}
		}

		HUDPanelDialogVariableString_t *added = variables.AddToTail();

		if (!added)
		{
			return false;
		}

		*added->m_nPanelIdIndex() = panelIdIndex;
		*added->m_nDialogVariableIndex() = dialogVariableIndex;
		*added->m_sValue() = value;
		*added->m_bIsSet() = true;
		MarkFullChanged();
		return true;
	}
};

// BUGS:
// 1. Switching which player you observe merges HUD states instead of replacing them.
// CustomHudLayout walks through m_vecHasClasses and m_vecDialogVariableStrings
// and applies states where m_playerSlot equals the **observed** slot, not the player's own slot.
// 2. SetHasClassForPlayer does not correctly propagate changes to its children,
// the children's styles will only be updated when they are directly updated,
// or when the layout file is completely reloaded.
class CCSCustomHudLayout : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSCustomHudLayout)

	SCHEMA_FIELD(CUtlSymbolLarge, m_strLayout)
	SCHEMA_FIELD_COLLECTION(CCSCustomHudLayoutState, m_vecPlayerLayoutStates)
	SCHEMA_FIELD_POINTER(CCSCustomHudLayoutState, m_globalLayoutState)
	SCHEMA_FIELD_COLLECTION(CUtlString, m_vecPanelIds)
	SCHEMA_FIELD_COLLECTION(CUtlString, m_vecClassNames)
	SCHEMA_FIELD_COLLECTION(CUtlString, m_vecDialogVariableNames)

	CCSCustomHudLayoutState *GetGlobalLayoutState()
	{
		return m_globalLayoutState();
	}

	CCSCustomHudLayoutState *GetPlayerLayoutState(CPlayerSlot slot)
	{
		CCSCustomHudLayoutState *state = m_vecPlayerLayoutStates().Element(slot.Get());

		// Every entry pre-sizes on slot 0; stamp the real slot so SetInputCaptureEnabled resolves
		// past slot 0.
		if (state && state->m_playerSlot().Get() != slot.Get())
		{
			state->m_playerSlot(slot);
			state->MarkFullChanged();
		}
		return state;
	}

	// Set if a panel has a class. Applies to all players.
	// Pass k_eHudPanelClassStatus_Undefined to revert to the original value.
	bool SetHasClass(const char *panelId, const char *className, EHudPanelClassStatus_t status)
	{
		return SetHasClassOnState(GetGlobalLayoutState(), panelId, className, status);
	}

	// Set the value of a dialog variable. Applies to all players.
	// The script API requires a value here; NULL clears the entry, which it has no way to express.
	bool SetDialogVariableString(const char *panelId, const char *variableName, const char *value)
	{
		return SetDialogVariableStringOnState(GetGlobalLayoutState(), panelId, variableName, value);
	}

	// Force a player into cursor mode and enable click detection on the panels of this layout.
	// Players get movement control back once all layouts have disabled input capture.
	void SetInputCaptureEnabled(CPlayerSlot slot, bool enabled)
	{
		CCSCustomHudLayoutState *state = GetPlayerLayoutState(slot);

		if (state && state->m_bInputCaptureEnabled() != enabled)
		{
			state->m_bInputCaptureEnabled(enabled);
		}
	}

	bool IsInputCaptureEnabled(CPlayerSlot slot)
	{
		CCSCustomHudLayoutState *state = GetPlayerLayoutState(slot);
		return state && state->m_bInputCaptureEnabled();
	}

private:
	static int InternString(const CSchemaCollection<CUtlString> &strings, const char *str, bool &appended)
	{
		appended = false;

		if (!str)
		{
			str = "";
		}

		int count = strings.Count();

		for (int i = 0; i < count; ++i)
		{
			CUtlString *entry = strings.Element(i);

			if (V_strcmp(entry->Get(), str) == 0)
			{
				return i;
			}
		}

		if (count >= HUD_LAYOUT_MAX_INTERNED_STRINGS)
		{
			Warning("CCSCustomHudLayout: cannot intern '%s', the layout is already at %i strings.\n", str, count);
			return -1;
		}

		CUtlString *added = strings.AddToTail();

		if (!added)
		{
			return -1;
		}

		*added = str;
		appended = true;
		return count;
	}

	int InternPanelId(const char *panelId)
	{
		bool appended = false;
		int index = InternString(m_vecPanelIds(), panelId, appended);

		if (appended)
		{
			NetworkStateChanged(NetworkStateChangedData(true));
		}

		return index;
	}

	int InternClassName(const char *className)
	{
		bool appended = false;
		int index = InternString(m_vecClassNames(), className, appended);

		if (appended)
		{
			NetworkStateChanged(NetworkStateChangedData(true));
		}

		return index;
	}

	int InternDialogVariableName(const char *variableName)
	{
		bool appended = false;
		int index = InternString(m_vecDialogVariableNames(), variableName, appended);

		if (appended)
		{
			NetworkStateChanged(NetworkStateChangedData(true));
		}

		return index;
	}

	bool SetHasClassOnState(CCSCustomHudLayoutState *state, const char *panelId, const char *className, EHudPanelClassStatus_t status)
	{
		if (!state)
		{
			return false;
		}

		int panelIdIndex = InternPanelId(panelId);
		int classNameIndex = InternClassName(className);

		if (panelIdIndex < 0 || classNameIndex < 0)
		{
			return false;
		}

		return state->SetHasClass((uint16)panelIdIndex, (uint16)classNameIndex, status);
	}

	bool SetDialogVariableStringOnState(CCSCustomHudLayoutState *state, const char *panelId, const char *variableName, const char *value)
	{
		if (!state)
		{
			return false;
		}

		int panelIdIndex = InternPanelId(panelId);
		int variableIndex = InternDialogVariableName(variableName);

		if (panelIdIndex < 0 || variableIndex < 0)
		{
			return false;
		}

		// A NULL value means "unset", not "empty string": the game routes that to a separate path
		// that only clears m_bIsSet on an existing entry and never appends one.
		if (!value)
		{
			return state->UnsetDialogVariableString((uint16)panelIdIndex, (uint16)variableIndex);
		}

		return state->SetDialogVariableString((uint16)panelIdIndex, (uint16)variableIndex, value);
	}

public:
	static CCSCustomHudLayout *FromClickHandle(uint32 packedHandle)
	{
		CEntityInstance *instance = CEntityHandle::FromPackedInt((int)packedHandle).Get();
		if (!instance || V_strcmp(instance->GetClassname(), "custom_hud_layout") != 0)
		{
			return NULL;
		}
		return (CCSCustomHudLayout *)instance;
	}
};

#endif
