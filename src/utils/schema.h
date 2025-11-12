#pragma once

#include "stdint.h"
#include "virtual.h"
#include "smartptr.h"
#include "utldelegate.h"
#include "entityinstance.h"
#undef schema
class CBasePlayerController;

struct SchemaKey
{
	uint32 offset;
	bool networked;
};

struct CNetworkVarChainer : public CSmartPtr<CEntityInstance>
{
	struct UnkStruct
	{
		void *unk0;
		CUtlDelegate<void(const CNetworkVarChainer &)> updateDelegate;
		uint unk3;
	};

	CUtlVector<UnkStruct> unk0;
	ChangeAccessorFieldPathIndex_t m_PathIndex;
	// If true, the entity instance will have its NetworkStateChanged called when the value changes.
	bool unknown2;
};

void EntityNetworkStateChanged(uintptr_t pEntity, uint nOffset);
void ChainNetworkStateChanged(uintptr_t pNetworkVarChainer, uint nOffset, int nArrayIndex = -1);
void NetworkVarStateChanged(uintptr_t pNetworkVar, uint32_t nOffset, uint32 nNetworkStateChangedOffset);

namespace schema
{
	int16_t FindChainOffset(const char *className, uint32_t classNameHash);
	SchemaKey GetOffset(const char *className, uint32_t classKey, const char *memberName, uint32_t memberKey);
} // namespace schema

constexpr uint32_t val_32_const = 0x811c9dc5;
constexpr uint32_t prime_32_const = 0x1000193;
constexpr uint64_t val_64_const = 0xcbf29ce484222325;
constexpr uint64_t prime_64_const = 0x100000001b3;

inline constexpr uint32_t hash_32_fnv1a_const(const char *const str, const uint32_t value = val_32_const) noexcept
{
	return (str[0] == '\0') ? value : hash_32_fnv1a_const(&str[1], (value ^ uint32_t(str[0])) * prime_32_const);
}

inline constexpr uint64_t hash_64_fnv1a_const(const char *const str, const uint64_t value = val_64_const) noexcept
{
	return (str[0] == '\0') ? value : hash_64_fnv1a_const(&str[1], (value ^ uint64_t(str[0])) * prime_64_const);
}

// clang-format off
#define SCHEMA_FIELD_OFFSET(type, varName, extra_offset)                                                                     \
	class varName##_prop                                                                                                     \
	{                                                                                                                        \
	public:                                                                                                                  \
		std::add_lvalue_reference_t<type> Get()                                                                              \
		{                                                                                                                    \
			static const auto m_key = schema::GetOffset(m_className, m_classNameHash, #varName, m_varNameHash);              \
			static const auto m_offset = offsetof(ThisClass, varName);                                                       \
																															 \
			uintptr_t pThisClass = ((uintptr_t)this - m_offset);                                                             \
                                                                                                                             \
			return *reinterpret_cast<std::add_pointer_t<type>>(pThisClass + m_key.offset + extra_offset);                    \
		}                                                                                                                    \
		void Set(type& val)                                                                                                  \
		{                                                                                                                    \
			static const auto m_key = schema::GetOffset(m_className, m_classNameHash, #varName, m_varNameHash);              \
			static const auto m_chain = schema::FindChainOffset(m_className, m_classNameHash);                               \
			static const auto m_offset = offsetof(ThisClass, varName);                                                       \
																															 \
			uintptr_t pThisClass = ((uintptr_t)this - m_offset);                                                             \
                                                                                                                             \
			NetworkStateChanged();                                                                                           \
			*reinterpret_cast<std::add_pointer_t<type>>(pThisClass + m_key.offset + extra_offset) = val;                     \
		}                                                                                                                    \
		void NetworkStateChanged()                                                                                           \
		{                                                                                                                    \
			static const auto m_key = schema::GetOffset(m_className, m_classNameHash, #varName, m_varNameHash);				 \
			static const auto m_chain = schema::FindChainOffset(m_className, m_classNameHash);								 \
			static const auto m_offset = offsetof(ThisClass, varName);														 \
																															 \
			uintptr_t pThisClass = ((uintptr_t)this - m_offset);                                                             \
                                                                                                                             \
			if (m_chain != 0 && m_key.networked)                                                                             \
			{                                                                                                                \
				::ChainNetworkStateChanged(pThisClass + m_chain, m_key.offset + extra_offset);                               \
			}                                                                                                                \
			else if (m_key.networked)                                                                                        \
			{                                                                                                                \
				if (m_networkStateChangedOffset == -1)                                                                       \
					::EntityNetworkStateChanged(pThisClass, m_key.offset + extra_offset);                                    \
				else                                                                                                         \
					::NetworkVarStateChanged(pThisClass, m_key.offset + extra_offset, m_networkStateChangedOffset);          \
			}                                                                                                                \
		}                                                                                                                    \
		operator std::add_lvalue_reference_t<type>()                                                                         \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
		std::add_lvalue_reference_t<type> operator()()                                                                       \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
		std::add_lvalue_reference_t<type> operator->()                                                                       \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
		void operator()(type val)                                                                                            \
		{                                                                                                                    \
			Set(val);                                                                                                        \
		}                                                                                                                    \
		std::add_lvalue_reference_t<type> operator=(type val)                                                                \
		{                                                                                                                    \
			Set(val);                                                                                                        \
			return Get();                                                                                                    \
		}                                                                                                                    \
		std::add_lvalue_reference_t<type> operator=(varName##_prop& val)                                                     \
		{                                                                                                                    \
			Set(val());                                                                                                      \
			return Get();                                                                                                    \
		}                                                                                                                    \
	private:                                                                                                                 \
		/*Prevent accidentally copying this wrapper class instead of the underlying field*/                                  \
		varName##_prop(const varName##_prop&) = delete;                                                                      \
		static constexpr auto m_varNameHash = hash_32_fnv1a_const(#varName);                                                 \
	} varName;

#define SCHEMA_FIELD_POINTER_OFFSET(type, varName, extra_offset)                                                             \
	class varName##_prop                                                                                                     \
	{                                                                                                                        \
	public:                                                                                                                  \
		type* Get()                                                                                                          \
		{                                                                                                                    \
			static const auto m_key = schema::GetOffset(m_className, m_classNameHash, #varName, m_varNameHash);              \
			static const auto m_offset = offsetof(ThisClass, varName);                                                       \
																															 \
			uintptr_t pThisClass = ((uintptr_t)this - m_offset);                                                             \
                                                                                                                             \
			return reinterpret_cast<std::add_pointer_t<type>>(pThisClass + m_key.offset + extra_offset);                     \
		}                                                                                                                    \
		void NetworkStateChanged() /*Call this after editing the field*/                                                     \
		{                                                                                                                    \
			static const auto m_key = schema::GetOffset(m_className, m_classNameHash, #varName, m_varNameHash);              \
			static const auto m_chain = schema::FindChainOffset(m_className, m_classNameHash);                               \
			static const auto m_offset = offsetof(ThisClass, varName);                                                       \
																															 \
			uintptr_t pThisClass = ((uintptr_t)this - m_offset);                                                             \
																															 \
			if (m_chain != 0 && m_key.networked)                                                                             \
			{                                                                                                                \
				::ChainNetworkStateChanged(pThisClass + m_chain, m_key.offset + extra_offset);                               \
			}                                                                                                                \
			else if (m_key.networked)                                                                                        \
			{                                                                                                                \
				if (m_networkStateChangedOffset == -1)                                                                       \
					::EntityNetworkStateChanged(pThisClass, m_key.offset + extra_offset);                                    \
				else                                                                                                         \
					::NetworkVarStateChanged(pThisClass, m_key.offset + extra_offset, m_networkStateChangedOffset);          \
			}                                                                                                                \
		}                                                                                                                    \
		operator type*()                                                                                                     \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
		type* operator()()                                                                                                   \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
		type* operator->()                                                                                                   \
		{                                                                                                                    \
			return Get();                                                                                                    \
		}                                                                                                                    \
	private:                                                                                                                 \
		/*Prevent accidentally copying this wrapper class instead of the underlying field*/                                  \
		varName##_prop(const varName##_prop&) = delete;                                                                      \
		static constexpr auto m_varNameHash = hash_32_fnv1a_const(#varName);                                                 \
	} varName;

// Use this when you want the member's value itself
#define SCHEMA_FIELD(type, varName) \
	SCHEMA_FIELD_OFFSET(type, varName, 0)

// Use this when you want a pointer to a member
#define SCHEMA_FIELD_POINTER(type, varName) \
	SCHEMA_FIELD_POINTER_OFFSET(type, varName, 0)

// Use DECLARE_SCHEMA_CLASS_BASE for non-entity classes such as CCollisionProperty or CGlowProperty
// Their NetworkStateChanged function is elsewhere on their vtable rather than being CEntityInstance::NetworkStateChanged (usually 1)
// Though some classes like CGameRules will instead use their CNetworkVarChainer as a link back to the parent entity
// Some classes, such as CModelState, have offset 0 despite not being entity.
#define DECLARE_SCHEMA_CLASS_BASE(ClassName, offset)				\
	private:																		\
		typedef ClassName ThisClass;												\
		static constexpr const char* m_className = #ClassName;						\
		static constexpr uint32_t m_classNameHash = hash_32_fnv1a_const(#ClassName);\
		static constexpr int m_networkStateChangedOffset = offset;					\
	public:

#define DECLARE_SCHEMA_CLASS_ENTITY(className) DECLARE_SCHEMA_CLASS_BASE(className, -1)

// clang-format on
