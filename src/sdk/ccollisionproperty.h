#pragma once
#include "utils/schema.h"

class CBaseModelEntity;

struct VPhysicsCollisionAttribute_t
{
	DECLARE_SCHEMA_CLASS_INLINE(VPhysicsCollisionAttribute_t)

	SCHEMA_FIELD(uint8, m_nCollisionGroup)
	SCHEMA_FIELD(uint64, m_nInteractsAs)
	SCHEMA_FIELD(uint64, m_nInteractsWith)
};

class CCollisionProperty
{
public:
	DECLARE_SCHEMA_CLASS_INLINE(CCollisionProperty)

private:
	void **vtable;

public:
	CBaseModelEntity *m_pOuter;

	class m_collisionAttribute_prop
	{
	public:
		std::add_lvalue_reference_t<VPhysicsCollisionAttribute_t> Get()
		{
			static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClassName);
			static constexpr auto prop_hash = hash_32_fnv1a_const("m_collisionAttribute");
			static const auto m_key = schema::GetOffset(ThisClassName, datatable_hash, "m_collisionAttribute", prop_hash);
			static const size_t offset = ((::size_t) & reinterpret_cast<char const volatile &>((((ThisClass *)0)->m_collisionAttribute)));
			ThisClass *pThisClass = (ThisClass *)((byte *)this - offset);
			return *reinterpret_cast<std::add_pointer_t<VPhysicsCollisionAttribute_t>>((uintptr_t)(pThisClass) + m_key.offset + 0);
		}

		void Set(VPhysicsCollisionAttribute_t val)
		{
			static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClassName);
			static constexpr auto prop_hash = hash_32_fnv1a_const("m_collisionAttribute");
			static const auto m_key = schema::GetOffset(ThisClassName, datatable_hash, "m_collisionAttribute", prop_hash);
			static const auto m_chain = schema::FindChainOffset(ThisClassName);
			static const size_t offset = ((::size_t) & reinterpret_cast<char const volatile &>((((ThisClass *)0)->m_collisionAttribute)));
			ThisClass *pThisClass = (ThisClass *)((byte *)this - offset);
			if (m_chain != 0 && m_key.networked)
			{
				DevMsg("Found chain offset %d for %s::%s\n", m_chain, ThisClassName, "m_collisionAttribute");
				schema::NetworkStateChanged((uintptr_t)(pThisClass) + m_chain, m_key.offset + 0, 0xFFFFFFFF);
			}
			else if (m_key.networked)
			{
				DevMsg("Attempting to call SetStateChanged on on %s::%s\n", ThisClassName, "m_collisionAttribute");
				if (!IsStruct)
				{
					((CEntityInstance *)pThisClass)->NetworkStateChanged(m_key.offset + 0, -1, -1);
				}
				else if (false)
				{
					vmt::CallVirtual<void>(1, pThisClass, m_key.offset + 0, 0xFFFFFFFF, 0xFFFF);
				}
			}
			*reinterpret_cast<std::add_pointer_t<VPhysicsCollisionAttribute_t>>((uintptr_t)(pThisClass) + m_key.offset + 0) = val;
		}

		operator std::add_lvalue_reference_t<VPhysicsCollisionAttribute_t>()
		{
			return Get();
		}

		std::add_lvalue_reference_t<VPhysicsCollisionAttribute_t> operator()()
		{
			return Get();
		}

		std::add_lvalue_reference_t<VPhysicsCollisionAttribute_t> operator->()
		{
			return Get();
		}

		void operator()(VPhysicsCollisionAttribute_t val)
		{
			Set(val);
		}

		void operator=(VPhysicsCollisionAttribute_t val)
		{
			Set(val);
		}
	} m_collisionAttribute;
	SCHEMA_FIELD(SolidType_t, m_nSolidType)
	SCHEMA_FIELD(uint8, m_usSolidFlags)

	class m_CollisionGroup_prop
	{
	public:
		std::add_lvalue_reference_t<uint8> Get()
		{
			static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClassName);
			static constexpr auto prop_hash = hash_32_fnv1a_const("m_CollisionGroup");
			static const auto m_key = schema::GetOffset(ThisClassName, datatable_hash, "m_CollisionGroup", prop_hash);
			static const size_t offset = ((::size_t) & reinterpret_cast<char const volatile &>((((ThisClass *)0)->m_CollisionGroup)));
			ThisClass *pThisClass = (ThisClass *)((byte *)this - offset);
			return *reinterpret_cast<std::add_pointer_t<uint8>>((uintptr_t)(pThisClass) + m_key.offset + 0);
		}

		void Set(uint8 val)
		{
			static constexpr auto datatable_hash = hash_32_fnv1a_const(ThisClassName);
			static constexpr auto prop_hash = hash_32_fnv1a_const("m_CollisionGroup");
			static const auto m_key = schema::GetOffset(ThisClassName, datatable_hash, "m_CollisionGroup", prop_hash);
			static const auto m_chain = schema::FindChainOffset(ThisClassName);
			static const size_t offset = ((::size_t) & reinterpret_cast<char const volatile &>((((ThisClass *)0)->m_CollisionGroup)));
			ThisClass *pThisClass = (ThisClass *)((byte *)this - offset);
			if (m_chain != 0 && m_key.networked)
			{
				DevMsg("Found chain offset %d for %s::%s\n", m_chain, ThisClassName, "m_CollisionGroup");
				schema::NetworkStateChanged((uintptr_t)(pThisClass) + m_chain, m_key.offset + 0, 0xFFFFFFFF);
			}
			else if (m_key.networked)
			{
				DevMsg("Attempting to call SetStateChanged on on %s::%s\n", ThisClassName, "m_CollisionGroup");
				if (!IsStruct)
				{
					((CEntityInstance *)pThisClass)->NetworkStateChanged(m_key.offset + 0, -1, -1);
				}
				else if (false)
				{
					vmt::CallVirtual<void>(1, pThisClass, m_key.offset + 0, 0xFFFFFFFF, 0xFFFF);
				}
			}
			else
			{
				Msg("well crap.\n");
			}
			*reinterpret_cast<std::add_pointer_t<uint8>>((uintptr_t)(pThisClass) + m_key.offset + 0) = val;
		}

		operator std::add_lvalue_reference_t<uint8>()
		{
			return Get();
		}

		std::add_lvalue_reference_t<uint8> operator()()
		{
			return Get();
		}

		std::add_lvalue_reference_t<uint8> operator->()
		{
			return Get();
		}

		void operator()(uint8 val)
		{
			Set(val);
		}

		void operator=(uint8 val)
		{
			Set(val);
		}
	} m_CollisionGroup;
};
