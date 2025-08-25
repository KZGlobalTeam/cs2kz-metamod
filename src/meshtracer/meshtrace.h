#pragma once

#include "sdk/physics/gamesystem.h"
#include "sdk/services.h"
#include "movement/movement.h"
#include "utils/kz_maths.h"

bool RetraceHull(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter &filter, CGameTrace *trace);
void TryPlayerMove_Custom(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing,
						  MovementPlayer *player);
