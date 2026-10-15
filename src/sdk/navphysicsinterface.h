#pragma once
#include "common.h"

#include "gametrace.h"
#include "utils/addresses.h"
#include "utils/virtual.h"

class INavPhysicsInterface
{
private:
	virtual ~INavPhysicsInterface() = 0;
	virtual void Nav_TraceLine(const Vector &vStart, const Vector &vEnd, CBaseEntity *pIgnore, uint64 nInteractsWith, uint8 nCollisionGroup,
							   uint8 nObjectSetMask, CGameTrace *trace) = 0;
	virtual void Nav_TraceLine(const Vector &vStart, const Vector &vEnd, CTraceFilter *pFilter, CGameTrace *pTraceOut) = 0;
	virtual void Nav_TraceShape(const Ray_t &ray, const Vector &vStart, const Vector &vEnd, CBaseEntity *pIgnore, uint64 nInteractsWith,
								uint8 nCollisionGroup, uint8 nObjectSetMask, CGameTrace *trace) = 0;
	virtual void Nav_TraceShape(const Ray_t &ray, const Vector &vStart, const Vector &vEnd, CTraceFilter *pFilter, CGameTrace *trace) = 0;
	virtual uint64 Nav_PointContents(const Vector *const vTestPos, uint64 nContentsMask) = 0;
	virtual bool Nav_CheckAreaOverlappingEntity(const void *const rArea, /*CNavArea*/
												const CBaseEntity *const rEntity, bool bExtrudeHullHeight) = 0;
	virtual void Nav_GetEntityWorldSpaceAABB(const CBaseEntity *const rEntity, Vector *pMinsOut, Vector *pMaxsOut) = 0;

	virtual void Unk(void *) = 0; // Calls delete on the object passed in which is located at the start of the vtable.

private:
	static inline void **vTable {};

public:
	static void TraceLine(const Vector &vStart, const Vector &vEnd, CTraceFilter *pFilter, CGameTrace *trace)
	{
		if (!vTable)
		{
			vTable = static_cast<void **>(modules::server->FindVirtualTable("CNavPhysicsInterface"));
		}

		assert(vTable);
		auto *iface = reinterpret_cast<INavPhysicsInterface *>(&vTable);
		iface->Nav_TraceLine(vStart, vEnd, pFilter, trace);
	}

	static void TraceShape(const Ray_t &ray, const Vector &vStart, const Vector &vEnd, CTraceFilter *pFilter, CGameTrace *trace)
	{
		if (!vTable)
		{
			vTable = static_cast<void **>(modules::server->FindVirtualTable("CNavPhysicsInterface"));
		}

		assert(vTable);
		auto *iface = reinterpret_cast<INavPhysicsInterface *>(&vTable);
		iface->Nav_TraceShape(ray, vStart, vEnd, pFilter, trace);
	}
};
