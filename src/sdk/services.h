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
class CCSPlayer_MovementServices;

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
	virtual void unk_18() = 0;

public:
	CNetworkVarChainer chainEntity;
	uint8 __pad0028[0x8];
	CBasePlayerPawn *pawn;
	uint8 __pad0038[0x8];
};

static_assert(sizeof(CPlayerPawnComponent) == 0x48);

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
	SCHEMA_FIELD(CInButtonState, m_nButtons)
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

	SCHEMA_FIELD(Vector, m_vecLastMovementImpulses)
};

class CPlayer_MovementServices_Humanoid : public CPlayer_MovementServices
{
	virtual ~CPlayer_MovementServices_Humanoid() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CPlayer_MovementServices_Humanoid);
	SCHEMA_FIELD(float, m_flSurfaceFriction)
};

// Not a real class, just something that the two classes below can inherit from
class CCSPlayerBaseJump
{
	void **vtable;

public:
	CCSPlayer_MovementServices *m_pMovementServices;
};

class alignas(8) CCSPlayerLegacyJump : public CCSPlayerBaseJump
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CCSPlayerLegacyJump, 0)
	SCHEMA_FIELD(bool, m_bOldJumpPressed)
	SCHEMA_FIELD(float, m_flJumpPressedTime)
};

class alignas(8) CCSPlayerModernJump : public CCSPlayerBaseJump
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CCSPlayerModernJump, 0)
	SCHEMA_FIELD(int, m_nLastActualJumpPressTick)
	SCHEMA_FIELD(float, m_flLastActualJumpPressFrac)
	SCHEMA_FIELD(int, m_nLastUsableJumpPressTick)
	SCHEMA_FIELD(float, m_flLastUsableJumpPressFrac)
	SCHEMA_FIELD(int, m_nLastLandedTick)
	SCHEMA_FIELD(float, m_flLastLandedFrac)
	SCHEMA_FIELD(float, m_flLastLandedVelocityX)
	SCHEMA_FIELD(float, m_flLastLandedVelocityY)
	SCHEMA_FIELD(float, m_flLastLandedVelocityZ)
};

class CCSPlayer_MovementServices : public CPlayer_MovementServices_Humanoid

{
	virtual ~CCSPlayer_MovementServices() = 0;

public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayer_MovementServices);
	SCHEMA_FIELD(Vector, m_vecLadderNormal)
	SCHEMA_FIELD(float, m_flAccumulatedJumpError)
	SCHEMA_FIELD(bool, m_bDucked)
	SCHEMA_FIELD(float, m_flDuckSpeed)
	SCHEMA_FIELD(float, m_flDuckAmount)
	SCHEMA_FIELD(float, m_flStamina)
	SCHEMA_FIELD(bool, m_bDuckOverride)
	SCHEMA_FIELD(float, m_flLastDuckTime)
	SCHEMA_FIELD(float, m_bDesiresDuck)
	SCHEMA_FIELD(bool, m_bDucking)
	SCHEMA_FIELD(float, m_flDuckOffset)
	SCHEMA_FIELD(bool, m_duckUntilOnGround)
	SCHEMA_FIELD(CCSPlayerLegacyJump, m_LegacyJump)
	SCHEMA_FIELD(CCSPlayerModernJump, m_ModernJump)
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
	virtual void RemoveAllItems(bool removeSuit) = 0;
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
