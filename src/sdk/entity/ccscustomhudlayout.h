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

	// Marking a single vector element dirty needs a serializer-registered field path, cached in
	// non-schema members of the state that the schema system does not expose. The game hits the
	// same wall whenever that cache is cold and falls back to marking the whole edict (networkvar.h,
	// "CNetworkUtlVectorFull"), so take the same fallback for every vector write.
	// ponytail: full edict resend per HUD write; revisit only if a layout is updated per tick.
	void MarkFullChanged()
	{
		NetworkStateChangedData data(true);
		CALL_VIRTUAL(void, 1, this, &data);
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
				MarkFullChanged();
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
				MarkFullChanged();
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
				MarkFullChanged();
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

// CCSUsrMsg_CustomHudClicked packs the clicked layout into 24 bits: a 14 bit entity index and the
// low 10 bits of the serial. 0xFFFFFF is the "no layout" default the proto declares.
#define HUD_CLICK_HANDLE_INVALID      0xFFFFFF
#define HUD_CLICK_HANDLE_INDEX_MASK   0x3FFF
#define HUD_CLICK_HANDLE_SERIAL_SHIFT 14
#define HUD_CLICK_HANDLE_SERIAL_MASK  0x3FF

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

		// The client does not index this vector, it scans it for the entry whose m_playerSlot is
		// its own: that is the only reason the field exists. The game pre-sizes the vector at
		// spawn and leaves every entry on slot 0, so an unstamped entry means every player past
		// slot 0 reads slot 0's classes, dialog variables and input capture flag instead of their
		// own. Stamp it once, here, where every per-player write already funnels through.
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

	// Set if a panel has a class for a single player. Overrides the all player value.
	// Pass k_eHudPanelClassStatus_Undefined to defer to the all player value.
	bool SetHasClassForPlayer(CPlayerSlot slot, const char *panelId, const char *className, EHudPanelClassStatus_t status)
	{
		return SetHasClassOnState(GetPlayerLayoutState(slot), panelId, className, status);
	}

	// Set the value of a dialog variable. Applies to all players.
	// The script API requires a value here; NULL clears the entry, which it has no way to express.
	bool SetDialogVariableString(const char *panelId, const char *variableName, const char *value)
	{
		return SetDialogVariableStringOnState(GetGlobalLayoutState(), panelId, variableName, value);
	}

	// Set the value of a dialog variable for a single player. Overrides the all player value.
	// Pass NULL to defer to the all player value again.
	bool SetDialogVariableStringForPlayer(CPlayerSlot slot, const char *panelId, const char *variableName, const char *value)
	{
		return SetDialogVariableStringOnState(GetPlayerLayoutState(slot), panelId, variableName, value);
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
	// The game keeps a CUtlHashtable next to each of these vectors to accelerate the same lookup.
	// It is not a schema field, so this scans the networked vector instead and leaves the hashtable
	// untouched. Harmless while the layout is only driven from here; a map script writing to the
	// same layout would miss in its stale hashtable and intern a duplicate string, which still
	// resolves correctly client side but burns an entry against the cap.
	// ponytail: linear scan, fine at the game's own 1024 entry ceiling.
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
	// Resolves the packed layout handle out of a CCSUsrMsg_CustomHudClicked, NULL when it no longer
	// names a live custom_hud_layout.
	static CCSCustomHudLayout *FromClickHandle(uint32 packedHandle)
	{
		if (packedHandle == HUD_CLICK_HANDLE_INVALID)
		{
			return NULL;
		}

		int entityIndex = packedHandle & HUD_CLICK_HANDLE_INDEX_MASK;
		CEntityInstance *instance = GameEntitySystem()->GetEntityInstance(CEntityIndex(entityIndex));

		if (!instance || V_strcmp(instance->GetClassname(), "custom_hud_layout") != 0)
		{
			return NULL;
		}

		// The serial is only carried as its low bits, so compare the same width the client sent.
		uint32 serial = (packedHandle >> HUD_CLICK_HANDLE_SERIAL_SHIFT) & HUD_CLICK_HANDLE_SERIAL_MASK;

		if ((instance->GetRefEHandle().GetSerialNumber() & HUD_CLICK_HANDLE_SERIAL_MASK) != serial)
		{
			return NULL;
		}

		return (CCSCustomHudLayout *)instance;
	}
};

#endif
