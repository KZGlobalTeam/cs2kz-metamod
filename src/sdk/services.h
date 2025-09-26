#pragma once

class CBaseEntity;
class CBaseModelEntity;
class CBasePlayerPawn;
class CBasePlayerWeapon;

#include "utils/schema.h"
#include "entity/cbaseplayerpawn.h"
#include "cinbuttonstate.h"
#include "datatypes.h"
#include "econ/ccsplayerinventory.h"
#include "entity/cbaseplayerweapon.h"

class CPlayerPawnComponent
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayerPawnComponent);

private:
	virtual void unk_00() = 0;
	virtual void unk_01() = 0;
	virtual ~CPlayerPawnComponent() = 0;
	virtual void unk_03() = 0;
	virtual void unk_04() = 0;
	virtual void unk_05() = 0;
	virtual void unk_06() = 0;
	virtual void unk_07() = 0;
	virtual void unk_08() = 0;
	virtual void unk_09() = 0;
	virtual void unk_10() = 0;
	virtual void unk_11() = 0;
	virtual void unk_12() = 0;
	virtual void unk_13() = 0;
	virtual void unk_14() = 0;
	virtual void unk_15() = 0;
	virtual void unk_16() = 0;
	virtual void unk_17() = 0;

public:
	uint8 chainEntity[0x28]; // Unused
	CBasePlayerPawn *pawn;   // 0x16
	uint8 __pad0030[0x6];    // 0x0
};

static_assert(sizeof(CPlayerPawnComponent) == 0x40);

class CPlayer_WeaponServices : public CPlayerPawnComponent
{
	virtual ~CPlayer_WeaponServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_WeaponServices)
	SCHEMA_FIELD(CHandle<CBasePlayerWeapon>, m_hActiveWeapon)
	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerWeapon>>, m_hMyWeapons)
};

class CPlayer_ObserverServices : public CPlayerPawnComponent
{
	virtual ~CPlayer_ObserverServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_ObserverServices)
	SCHEMA_FIELD(ObserverMode_t, m_iObserverMode)
	SCHEMA_FIELD(ObserverMode_t, m_iObserverLastMode)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hObserverTarget)
};

class CPlayer_MovementServices : public CPlayerPawnComponent
{
	virtual ~CPlayer_MovementServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_MovementServices);
	SCHEMA_FIELD_POINTER(CInButtonState, m_nButtons)
	SCHEMA_FIELD_POINTER(float, m_arrForceSubtickMoveWhen)

	void SetForcedSubtickMove(i32 index, f32 when, bool network = true)
	{
		if (index > 3)
		{
			DevMsg("SetForcedSubtickMove: index %d out of range\n", index);
			return;
		}
		static constexpr auto datatable_hash = hash_32_fnv1a_const("CPlayer_MovementServices");
		static constexpr auto prop_hash = hash_32_fnv1a_const("m_arrForceSubtickMoveWhen");
		static const auto m_key = schema::GetOffset(m_className, datatable_hash, "m_arrForceSubtickMoveWhen", prop_hash);
		static const auto m_chain = schema::FindChainOffset(m_className, m_classNameHash);
		static const size_t offset =
			((::size_t)&reinterpret_cast<char const volatile &>((((CPlayer_MovementServices *)0)->m_arrForceSubtickMoveWhen)));
		if (m_chain != 0 && m_key.networked && network)
		{
			::ChainNetworkStateChanged((uintptr_t)(this) + m_chain, m_key.offset, index);
		}
		m_arrForceSubtickMoveWhen[index] = when;
	}
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
	virtual ~CPlayer_MovementServices_Humanoid() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_MovementServices_Humanoid);
	SCHEMA_FIELD(bool, m_bDucking)
	SCHEMA_FIELD(bool, m_bDucked)
	SCHEMA_FIELD(float, m_flSurfaceFriction)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid
{
	virtual ~CCSPlayer_MovementServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayer_MovementServices);
	SCHEMA_FIELD(Vector, m_vecLadderNormal)
	SCHEMA_FIELD(bool, m_bOldJumpPressed)
	SCHEMA_FIELD(float, m_flJumpPressedTime)
	SCHEMA_FIELD(float, m_flAccumulatedJumpError)
	SCHEMA_FIELD(float, m_flDuckSpeed)
	SCHEMA_FIELD(float, m_flDuckAmount)
	SCHEMA_FIELD(float, m_flStamina)
	SCHEMA_FIELD(bool, m_bDuckOverride)
	SCHEMA_FIELD(float, m_flLastDuckTime)
	SCHEMA_FIELD(float, m_bDesiresDuck)
	SCHEMA_FIELD(float, m_flDuckOffset)
	SCHEMA_FIELD(bool, m_duckUntilOnGround)
};

class CCSPlayer_WaterServices : public CPlayerPawnComponent
{
	virtual ~CCSPlayer_WaterServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayer_WaterServices);
	SCHEMA_FIELD(float, m_flWaterJumpTime)
	SCHEMA_FIELD(Vector, m_vecWaterJumpVel)
};

class CPlayer_ItemServices : public CPlayerPawnComponent
{
	virtual ~CPlayer_ItemServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_ItemServices);
};

class CCSPlayer_ItemServices : public CPlayer_ItemServices
{
	virtual ~CCSPlayer_ItemServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayer_ItemServices);

private:
	virtual CBasePlayerWeapon *_GiveNamedItem(const char *pchName) = 0;

public:
	virtual bool GiveNamedItemBool(const char *pchName, bool keepGear) = 0;
	virtual CBasePlayerWeapon *GiveNamedItem(const char *pchName) = 0;
	virtual void DropActiveWeapon(CBasePlayerWeapon *pWeapon) = 0;
	virtual void StripPlayerWeapons(bool removeSuit) = 0;
};

class CCSPlayerController_InventoryServices
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayerController_InventoryServices)

	SCHEMA_FIELD_POINTER_OFFSET(CCSPlayerInventory, m_nPersonaDataXpTrailLevel, 4)

	CCSPlayerInventory *GetInventory()
	{
		return m_nPersonaDataXpTrailLevel();
	}
};
