#pragma once

#include "ehandle.h"
#include "cbaseentity.h"
#include "ccsplayerpawn.h"
#include "utils/interfaces.h"

class CCSPlayerPawn;

enum class PlayerConnectedState : uint32_t
{
	PlayerNeverConnected = 0xFFFFFFFF,
	PlayerConnected = 0x0,
	PlayerConnecting = 0x1,
	PlayerReconnecting = 0x2,
	PlayerDisconnecting = 0x3,
	PlayerDisconnected = 0x4,
	PlayerReserved = 0x5,
};

class CBasePlayerController : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CBasePlayerController);

	SCHEMA_FIELD(uint64, m_steamID)
	SCHEMA_FIELD(bool, m_bIsHLTV)
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPawn)
	SCHEMA_FIELD_POINTER(char, m_iszPlayerName)
	SCHEMA_FIELD(PlayerConnectedState, m_iConnected)

	void SetOrRefreshPlayerName(const char *name = nullptr)
	{
		if (name)
		{
			V_strncpy(this->m_iszPlayerName(), name, 128);
		}
		static constexpr auto datatable_hash = hash_32_fnv1a_const("CBasePlayerController");
		static constexpr auto prop_hash = hash_32_fnv1a_const("m_iszPlayerName");
		static const auto m_key = schema::GetOffset(m_className, datatable_hash, "m_iszPlayerName", prop_hash);
		static const auto m_chain = schema::FindChainOffset(m_className, m_classNameHash);

		this->NetworkStateChanged({m_key.offset});
	}

	CBasePlayerPawn *GetCurrentPawn()
	{
		return m_hPawn.Get();
	}

	const char *GetPlayerName()
	{
		return m_iszPlayerName();
	}

	int GetPlayerSlot()
	{
		return entindex() - 1;
	}

	void SetPawn(CCSPlayerPawn *pawn)
	{
		g_pKZUtils->SetPawn(this, pawn, true, false, false, false);
	}
};
