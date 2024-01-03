#include "utils.h"

internal KZUtils kzUtils;
KZUtils *g_pKZUtils = &kzUtils;

void *KZUtils::GetSchemaSystemPointer()
{
	return (void *)interfaces::pSchemaSystem;
}

void *KZUtils::GetSchemaStateChangedPointer()
{
	return (void *)schema::StateChanged;
}

void *KZUtils::GetSchemaNetworkStateChangedPointer()
{
	return (void *)schema::NetworkStateChanged;
}

CGlobalVars *KZUtils::GetServerGlobals()
{
	return interfaces::pEngine->GetServerGlobals();
}

void KZUtils::SetEntityMoveType(CBaseEntity2 *entity, MoveType_t movetype)
{
	utils::SetEntityMoveType(entity, movetype);
}

void KZUtils::EntityCollisionRulesChanged(CBaseEntity2 *entity)
{
	utils::EntityCollisionRulesChanged(entity);
}

CBaseEntity2 *KZUtils::FindEntityByClassname(CEntityInstance *start, const char *name)
{
	return utils::FindEntityByClassname(start, name);
}

CBasePlayerController *KZUtils::GetController(CBaseEntity2 *entity)
{
	return utils::GetController(entity);
}

CBasePlayerController *KZUtils::GetController(CPlayerSlot slot)
{
	return utils::GetController(slot);
}

CPlayerSlot KZUtils::GetEntityPlayerSlot(CBaseEntity2 *entity)
{
	return utils::GetEntityPlayerSlot(entity);
}

void KZUtils::InitPlayerMovementTraceFilter(CTraceFilterPlayerMovementCS &pFilter, CEntityInstance *pHandleEntity, uint64_t interactWith, int collisionGroup)
{
	utils::InitPlayerMovementTraceFilter(pFilter, pHandleEntity, interactWith, collisionGroup);
}

void KZUtils::InitGameTrace(trace_t_s2 *trace)
{
	utils::InitGameTrace(trace);
}

void KZUtils::TracePlayerBBox(const Vector &start, const Vector &end, const bbox_t &bounds, CTraceFilterPlayerMovementCS *filter, trace_t_s2 &pm)
{
	utils::TracePlayerBBox(start, end, bounds, filter, pm);
}

bool KZUtils::IsButtonDown(CInButtonState *buttons, u64 button, bool onlyDown)
{
	return utils::IsButtonDown(buttons, button, onlyDown);
}

f32 KZUtils::NormalizeDeg(f32 a)
{
	return utils::NormalizeDeg(a);
}

f32 KZUtils::GetAngleDifference(const f32 source, const f32 target, const f32 c, bool relative)
{
	return utils::GetAngleDifference(source, target, c, relative);
}
