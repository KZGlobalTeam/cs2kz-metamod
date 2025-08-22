#include "common.h"

// #include "sdk/datatypes.h"
#include "meshtrace.h"
#include "utils/utils.h"
#include "utils/interfaces.h"
#include "utils/detours.h"
#include "sdk/entity/cbasemodelentity.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/tracefilter.h"
#include "entity2/entityclass.h"

// max planes is 17
// 2 planes for the 2 faces of the triangle
// 6 planes for the 6 faces of the aabb
// 3*3 planes for the +x,+y,+z,-x,-y,-z projections of the 3 edges of the triangle
#define AABB_TRI_MINKOWSKI_MAX_PLANES 17

struct Triangle_t
{
	Vector verts[3];

	Triangle_t()
	{
		verts[0].Init();
		verts[1].Init();
		verts[2].Init();
	}

	Triangle_t(const Vector &v0, const Vector &v1, const Vector &v2)
	{
		verts[0] = v0;
		verts[1] = v1;
		verts[2] = v2;
	}
};

struct Plane_t
{
	Vector normal;
	f32 distance;

	Plane_t()
	{
		normal.Init();
		distance = 0;
	}

	Plane_t(const Vector &n, f32 d)
	{
		normal = n;
		distance = d;
	}
};

struct Brush_t
{
	i32 planeCount;
	Plane_t planes[AABB_TRI_MINKOWSKI_MAX_PLANES];
};

// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/game/q_shared.h#L1020
// a trace is returned when a box is swept through the world
struct TraceResult_t
{
	bool allsolid;   // if true, plane is not valid
	bool startsolid; // if true, the initial point was in a solid area
	float fraction;  // time completed, 1.0 = didn't hit anything
	float distance;  // distance from start to end where a hit happened
	Vector endpos;   // final position
	Plane_t plane;   // surface normal at impact, transformed to world space

	TraceResult_t()
	{
		endpos.Init();
	};
};

// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_local.h#L154
struct TraceRay_t
{
	Vector start;
	Vector end;
	AABB_t size; // size of the box being swept through the model
	f32 distance;

	// Vector offsets[8];  // [signbits][x] = either size[0][x] or size[1][x]
	// f32 maxOffset;  // longest corner length from origin
	// Vector extents;     // greatest of abs(size[0]) and abs(size[1])
	// Vector bounds[2];   // enclosing box of start and end surrounding by size
	// Vector modelOrigin; // origin of the model tracing through
	// bool isPoint;   // optimized case
	TraceRay_t(Vector start, Vector end, Vector mins, Vector maxs)
	{
		this->start = start;
		this->end = end;
		this->size.m_vMinBounds = mins;
		this->size.m_vMaxBounds = maxs;
		this->distance = (end - start).Length();
	}
};

static_function bool IsNormalAxisAligned(Vector normal)
{
	return fabs(normal.x) == 1 || fabs(normal.y) == 1 || fabs(normal.z) == 1;
}

static_function Brush_t GenerateTriangleAabbBevelPlanes(Triangle_t triangle)
{
	Brush_t result = {};

	// first 6 planes are the aabb of the triangle
	AABB_t triangleAabb;
	triangleAabb.m_vBounds[0] = triangle.verts[0];
	triangleAabb.m_vBounds[1] = triangle.verts[0];
	AddPointToBounds(triangle.verts[1], triangleAabb.m_vMinBounds, triangleAabb.m_vMaxBounds);
	AddPointToBounds(triangle.verts[2], triangleAabb.m_vMinBounds, triangleAabb.m_vMaxBounds);

	// -x, -y, -z, +x, +y, +z
	for (i32 dir = -1, aabbInd = 0; dir < 2; dir += 2, aabbInd++)
	{
		for (i32 axis = 0; axis < 3; axis++)
		{
			result.planes[result.planeCount].normal[axis] = (f32)dir;
			result.planes[result.planeCount].distance = triangleAabb.m_vBounds[aabbInd][axis] * (f32)dir;
			result.planeCount++;
		}
	}

	// next 2 planes are the 2 faces of the triangle
	Vector triangleNormal = CrossProduct(triangle.verts[1] - triangle.verts[0], triangle.verts[2] - triangle.verts[0]);
	// make sure the vector is actually normalised
	// TODO(GameChaos): is double normalisation the best solution for wonky normals?
	{
		f32 len = VectorNormalize(triangleNormal);
		if (len <= 0.9999 || len > 0.10001)
		{
			VectorNormalize(triangleNormal);
		}
	}

	// check if we have a valid triangle normal, aka if the triangle isn't a line or a point
	// also check whether the triangle normal is axis aligned, in which case skip it, because it
	//  would get duplicated by the aabb planes.
	if (!IsNormalAxisAligned(triangleNormal))
	{
		f32 triangleDistance = DotProduct(triangle.verts[0], triangleNormal);
		// front face of the triangle
		result.planes[result.planeCount].normal = triangleNormal;
		result.planes[result.planeCount].distance = triangleDistance;
		result.planeCount++;
		// back face of the triangle
		result.planes[result.planeCount].normal = triangleNormal * -1;
		result.planes[result.planeCount].distance = -triangleDistance;
		result.planeCount++;
	}

	// next 9 planes are planes made from the edges of the triangle
	for (i32 axis = 0; axis < 3; axis++)
	{
		for (i32 p1 = 0; p1 < 3; p1++)
		{
			i32 p2 = (p1 + 1) % 3;
			Vector edgeNormal = (triangle.verts[p2] - triangle.verts[p1]).Normalized();
			if (!IsNormalAxisAligned(edgeNormal))
			{
				Vector axisNormal = {0, 0, 0};
				axisNormal[axis] = triangleNormal[axis] < 0 ? 1 : -1;
				Vector planeNormal = CrossProduct(axisNormal, edgeNormal);
				f32 len = VectorNormalize(planeNormal);
				// TODO(GameChaos): is double normalisation the best solution for wonky normals?
				if (len <= 0.9999 || len > 0.10001)
				{
					VectorNormalize(planeNormal);
				}
				result.planes[result.planeCount].normal = planeNormal;
				result.planes[result.planeCount].distance = DotProduct(planeNormal, triangle.verts[p1]);
				result.planeCount++;
			}
		}
	}

	return result;
}

static_function f32 CalculatePlaneAabbExpansion(AABB_t aabb, Plane_t plane)
{
	Vector offset = {
		aabb.m_vBounds[plane.normal.x < 0 ? 1 : 0].x,
		aabb.m_vBounds[plane.normal.y < 0 ? 1 : 0].y,
		aabb.m_vBounds[plane.normal.z < 0 ? 1 : 0].z,
	};
	f32 result = DotProduct(offset, plane.normal);
	return result;
}

CConVar<float> kz_surface_clip_epsilon("kz_surface_clip_epsilon", FCVAR_NONE, "Surface clip epsilon", 0.03125f);

static_function Vector VectorNormaliseChebyshev(Vector vec)
{
	f32 chebyshevLength = abs(MAX(vec.x, MAX(vec.y, vec.z)));
	Vector result = vec;
	if (chebyshevLength > 0)
	{
		result /= chebyshevLength;
	}
	return result;
}

// #define SURFACE_CLIP_EPSILON 0
//  https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_trace.c#L483
static_function TraceResult_t TraceThroughBrush(const TraceRay_t &ray, const Brush_t &brush, f32 surfaceClipEpsilon = kz_surface_clip_epsilon.Get())
{
	TraceResult_t result = {};
	result.endpos = ray.end;
	result.fraction = 1;
	result.distance = ray.distance;
	if (!brush.planeCount)
	{
		return result;
	}

	f32 enterFrac = -1;
	f32 leaveFrac = 1;
	Plane_t clipplane = {};

	bool getout = false;
	bool startout = false;

	// compare the trace against all planes of the brush.
	//  find the latest time the trace crosses a plane towards the interior
	//  and the earliest time the trace crosses a plane towards the exterior
	for (i32 i = 0; i < brush.planeCount; i++)
	{
		Plane_t plane = brush.planes[i];

		// NOTE(GameChaos): GameChaos's chebyshev length minkowski sum epsilon modification!
		Vector chebyshevEpsilonVec = VectorNormaliseChebyshev(plane.normal) * surfaceClipEpsilon;
		f32 chebyshevEps = DotProduct(chebyshevEpsilonVec, plane.normal);

		// adjust the plane distance apropriately for mins/maxs
		// FIXME: use signbits into 8 way lookup for each mins/maxs
		// f32 dist = plane.dist - v3dot(ray.offsets[plane.signbits], plane.normal);
		f32 dist = plane.distance - CalculatePlaneAabbExpansion(ray.size, plane);

		f32 d1 = DotProduct(ray.start, plane.normal) - dist;
		f32 d2 = DotProduct(ray.end, plane.normal) - dist;
		if (d2 > 0)
		// if (d2 > chebyshevEps)
		{
			getout = true; // endpoint is not in solid
		}
		if (d1 > 0)
		// if (d1 > chebyshevEps)
		{
			startout = true;
		}

		// if completely in front of face, no intersection with the entire brush
		// if (d1 > 0 && d2 > 0)
		if (d1 > 0 && (d2 >= chebyshevEps || d2 >= d1))
		// if (d1 > chebyshevEps && d2 >= chebyshevEps)
		{
			return result;
		}

		// if it doesn't cross the plane, the plane isn't relevent
		if (d1 <= 0 && d2 <= 0)
		// if (d1 <= chebyshevEps && d2 <= chebyshevEps)
		{
			continue;
		}

		// crosses face
		if (d1 > d2)
		{ // enter
			f32 f = (d1 - chebyshevEps) / (d1 - d2);
			if (f < 0)
			{
				f = 0;
			}
			if (f > enterFrac)
			{
				enterFrac = f;
				clipplane = plane;
			}
		}
		else
		{ // leave
			// NOTE(GameChaos): Changed (d1 + chebyshevEps) to (d1 - chebyshevEps)
			//  to fix seamshot bug. https://www.youtube.com/watch?v=YnCG6fbTQ-I
			// f32 f = (d1 + chebyshevEps) / (d1 - d2);
			f32 f = (d1 - chebyshevEps) / (d1 - d2);
			if (f > 1)
			{
				f = 1;
			}
			if (f < leaveFrac)
			{
				leaveFrac = f;
			}
		}
	}

	//
	// all planes have been checked, and the trace was not
	// completely outside the brush
	//
	if (!startout)
	{
		// original point was inside brush
		result.startsolid = true;
		if (!getout)
		{
			result.allsolid = true;
			result.fraction = 0;
		}
	}
	else if (enterFrac < leaveFrac)
	{
		if (enterFrac > -1 && enterFrac < result.fraction)
		{
			if (enterFrac < 0)
			{
				enterFrac = 0;
			}
			result.fraction = enterFrac;
			result.plane = clipplane;
		}
	}

	if (result.fraction == 1)
	{
		result.endpos = ray.end;
	}
	else
	{
		VectorMA(ray.start, result.fraction, ray.end - ray.start, result.endpos);
	}
	result.distance *= result.fraction;
	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	assert(result.allsolid || result.fraction == 1 || result.plane.normal.Length() > 0.9999);
	return result;
}

static_function void ComputeSweptAABB(const Vector &boxMins, const Vector &boxMaxs, const Vector &delta, f32 epsilon, Vector &outMins,
									  Vector &outMaxs)
{
	// AABB at end position
	Vector endMins = boxMins + delta;
	Vector endMaxs = boxMaxs + delta;

	// Swept AABB is the min/max of both boxes
	outMins.x = Min(boxMins.x, endMins.x) - epsilon;
	outMins.y = Min(boxMins.y, endMins.y) - epsilon;
	outMins.z = Min(boxMins.z, endMins.z) - epsilon;
	outMaxs.x = Max(boxMaxs.x, endMaxs.x) + epsilon;
	outMaxs.y = Max(boxMaxs.y, endMaxs.y) + epsilon;
	outMaxs.z = Max(boxMaxs.z, endMaxs.z) + epsilon;
}

static_function void FindTrianglesInBox(const RnNode_t *node, CUtlVector<uint32> &triangles, const Vector &mins, const Vector &maxs,
										const CTransform *transform = nullptr)
{
	// TODO(GameChaos): incorrect way to transform aabb, fix!
	Vector nodeMins = transform ? utils::TransformPoint(*transform, node->m_vMin) : node->m_vMin;
	Vector nodeMaxs = transform ? utils::TransformPoint(*transform, node->m_vMax) : node->m_vMax;
	if (!QuickBoxIntersectTest(nodeMins, nodeMaxs, mins, maxs))
	{
		return;
	}

	if (node->IsLeaf())
	{
		for (u32 i = 0; i < node->m_nChildrenOrTriangleCount; i++)
		{
			triangles.AddToTail(node->m_nTriangleOffset + i);
		}
	}
	else
	{
		FindTrianglesInBox(node->GetChild1(), triangles, mins, maxs, transform);
		FindTrianglesInBox(node->GetChild2(), triangles, mins, maxs, transform);
	}
}

// source: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
static_function bool AabbTriangleIntersects(Triangle_t triIn, Vector mins, Vector maxs)
{
	Vector centre = (maxs + mins) * 0.5f;
	Vector extents = (maxs - mins) * 0.5f;
	Triangle_t tri = Triangle_t(triIn.verts[0], triIn.verts[1], triIn.verts[2]);
	// Translate the triangle as conceptually moving the AABB to origin
	for (i32 i = 0; i < 3; i++)
	{
		tri.verts[i] -= centre;
	}

	// Compute the edge vectors of the triangle  (ABC)
	// That is, get the lines between the points as vectors
	Vector e0 = tri.verts[1] - tri.verts[0]; // B - A
	Vector e1 = tri.verts[2] - tri.verts[1]; // C - B
	Vector e2 = tri.verts[0] - tri.verts[2]; // A - C

	Vector xAxis = Vector(1, 0, 0);
	Vector yAxis = Vector(0, 1, 0);
	Vector zAxis = Vector(0, 0, 1);

	Vector axes[] = {
		CrossProduct(xAxis, e0),
		CrossProduct(xAxis, e1),
		CrossProduct(xAxis, e2),
		CrossProduct(yAxis, e0),
		CrossProduct(yAxis, e1),
		CrossProduct(yAxis, e2),
		CrossProduct(zAxis, e0),
		CrossProduct(zAxis, e1),
		CrossProduct(zAxis, e2),
		// aabb axes
		xAxis,
		yAxis,
		zAxis,
		// triangle axis
		CrossProduct(e0, e1),
	};

	for (i32 i = 0; i < Q_ARRAYSIZE(axes); i++)
	{
		// Project all 3 vertices of the triangle onto the Separating axis
		f32 p0 = DotProduct(tri.verts[0], axes[i]);
		f32 p1 = DotProduct(tri.verts[1], axes[i]);
		f32 p2 = DotProduct(tri.verts[2], axes[i]);

		// Project the AABB onto the separating axis
		// We don't care about the end points of the projection
		// just the length of the half-size of the AABB
		// That is, we're only casting the extents onto the
		// separating axis, not the AABB center. We don't
		// need to cast the center, because we know that the
		// aabb is at origin compared to the triangle!

		f32 r = extents.x * fabs(DotProduct(xAxis, axes[i])) + extents.y * fabs(DotProduct(yAxis, axes[i]))
				+ extents.z * fabs(DotProduct(zAxis, axes[i]));

		// Now do the actual test, basically see if either of
		// the most extreme of the triangle points intersects r
		if (Max(-Max(Max(p0, p1), p2), Min(Min(p0, p1), p2)) > r)
		{
			// This means BOTH of the points of the projected triangle
			// are outside the projected half-length of the AABB
			// Therefore the axis is separating and we can exit
			return false;
		}
	}
	return true;
}

bool RetraceHull(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter &filter, CGameTrace *trace)
{
	Vector delta = end - start;

	f32 epsilon = kz_surface_clip_epsilon.Get();
	// overtrace a little bit to make sure we don't miss any triangles!
	Vector finalv0, finalv1, finalv2;

	TraceRay_t traceRay = TraceRay_t(start, end, ray.m_Hull.m_vMins, ray.m_Hull.m_vMaxs);
	TraceResult_t finalBrushTrace = {};
	CGameTrace finalTrace = CGameTrace();
	finalTrace.m_pSurfaceProperties = trace->m_pSurfaceProperties;
	finalTrace.m_pHitbox = trace->m_pHitbox;
	finalTrace.m_eRayType = RAY_TYPE_HULL;
	finalTrace.m_vEndPos = end;
	finalTrace.m_vStartPos = start;

	finalBrushTrace.fraction = 1;
	finalBrushTrace.distance = traceRay.distance;
	PhysicsTrace_t *finalPhysTrace = nullptr;

	CUtlVector<HPhysicsShape> shapes;
	Ray_t::Hull_t sweptAabb;
	// extend the extents a bit to make sure we get every possible triangle we could theoretically hit!
	ComputeSweptAABB(ray.m_Hull.m_vMins, ray.m_Hull.m_vMaxs, delta, epsilon + 1, sweptAabb.m_vMins, sweptAabb.m_vMaxs);
	Ray_t queryRay;
	queryRay.Init(sweptAabb.m_vMins, sweptAabb.m_vMaxs);
	g_pKZUtils->Query(&shapes, &queryRay, &start, &filter, false);

	CUtlVector<uint32> triangles;
	EntityInstanceIter_t iter;
	FOR_EACH_VEC(shapes, i)
	{
		IPhysicsShape *shape = shapes[i];
		PhysicsShapeType_t shapeType = shape->GetShapeType();
		if (shapeType != SHAPE_MESH)
		{
			continue;
		}
		RnMesh_t *mesh = shape->GetMesh();
		if (!mesh)
		{
			continue;
		}
		// TODO(GameChaos): transform mesh bbox?
		if (!QuickBoxIntersectTest(mesh->m_vMin, mesh->m_vMax, sweptAabb.m_vMins + start, sweptAabb.m_vMaxs + start))
		{
			continue;
		}

		RnCollisionAttr_t *collisionAttr = shape->GetCollisionAttr();
		if (!collisionAttr)
		{
			continue;
		}
		CEntityHandle ehandle = CEntityHandle(collisionAttr->m_nEntityId);
		auto ent = static_cast<CBaseEntity *>(ehandle.Get());
		auto body = shape->GetOwnerBody();
		if (!ent || !body)
		{
			continue;
		}

		// auto surfaceProps = aggregateData->m_SurfaceProperties[mesh->m_nSurfacePropertyIndex];
		CTransform transform;
		transform.SetToIdentity();
		transform.m_vPosition = ent->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
		transform.m_orientation = Quaternion(ent->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
		triangles.Purge();
		FindTrianglesInBox(&mesh->m_Nodes[0], triangles, sweptAabb.m_vMins + start, sweptAabb.m_vMaxs + start, &transform);
		FOR_EACH_VEC(triangles, k)
		{
			const RnTriangle_t *triangle = &mesh->m_Triangles[triangles[k]];
			// const RnTriangle_t *triangle = &triangles[k];
			Triangle_t tri = {utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[0]]),
							  utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[1]]),
							  utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[2]])};

			if (CloseEnough(delta.x, 0, FLT_EPSILON) && CloseEnough(delta.y, 0, FLT_EPSILON) && CloseEnough(delta.z, 0, FLT_EPSILON))
			{
				if (AabbTriangleIntersects(tri, ray.m_Hull.m_vMins + start, ray.m_Hull.m_vMaxs + start))
				{
					// finalTrace.m_pSurfaceProperties = surfaceProps;
					finalTrace.m_pEnt = ent;
					// finalTrace.m_pHitbox = ;
					finalTrace.m_hBody = body;
					finalTrace.m_hShape = shape;
					finalTrace.m_BodyTransform = transform;
					finalTrace.m_ShapeAttributes = *collisionAttr;
					finalTrace.m_nContents = finalTrace.m_ShapeAttributes.m_nInteractsAs;
					finalTrace.m_vStartPos = start;
					finalTrace.m_vEndPos = start;
					finalTrace.m_vHitPoint = start;
					finalTrace.m_flFraction = 0;
					finalTrace.m_nTriangle = k;
					finalTrace.m_eRayType = RAY_TYPE_HULL;
					finalTrace.m_bStartInSolid = finalBrushTrace.startsolid;
					finalTrace.m_bExactHitPoint = false;
					*trace = finalTrace;
					return true;
				}
				continue;
			}

			Brush_t triangleBrush = GenerateTriangleAabbBevelPlanes(tri);

			// Do our own trace calculation
			TraceResult_t brushTrace = TraceThroughBrush(traceRay, triangleBrush);

			if (brushTrace.fraction <= finalBrushTrace.fraction)
			{
				// try to handle traces with an identical fraction gracefully
				if (brushTrace.fraction == finalBrushTrace.fraction - 1 && finalBrushTrace.fraction < 1)
				{
					f32 newDot = DotProduct(brushTrace.plane.normal, delta);
					f32 oldDot = DotProduct(finalBrushTrace.plane.normal, delta);
					// If the old normal is more in line with the
					//  trace delta (end-start), then prefer the old one.
					if (oldDot >= newDot)
					{
						continue;
					}
				}
				finalBrushTrace = brushTrace;
				// finalPhysTrace = physTrace;
				// META_CONPRINTF("  Overriding triangle %i with %i\n", trace->m_nTriangle, physTrace->m_nTriangle);
				// META_CONPRINTF("  Triangle %i: normal %f %f %f, endpos %f %f %f, fraction %f\n", physTrace->m_nTriangle, brushTrace.plane.normal.x,
				// brushTrace.plane.normal.y, brushTrace.plane.normal.z, brushTrace.endpos.x, brushTrace.endpos.y, brushTrace.endpos.z,
				// brushTrace.fraction);
				finalv0 = tri.verts[0];
				finalv1 = tri.verts[1];
				finalv2 = tri.verts[2];

				// we hit something!
				// finalTrace.m_pSurfaceProperties = surfaceProps;
				finalTrace.m_pEnt = ent;
				// finalTrace.m_pHitbox = ;
				finalTrace.m_hBody = body;
				finalTrace.m_hShape = shape;
				finalTrace.m_BodyTransform = transform;
				finalTrace.m_ShapeAttributes = *collisionAttr;
				finalTrace.m_nContents = finalTrace.m_ShapeAttributes.m_nInteractsAs;
				finalTrace.m_vStartPos = start;
				finalTrace.m_vEndPos = finalBrushTrace.endpos;
				finalTrace.m_vHitNormal = finalBrushTrace.plane.normal;
				finalTrace.m_vHitPoint = finalBrushTrace.endpos;
				finalTrace.m_flHitOffset = finalBrushTrace.plane.distance;
				finalTrace.m_flFraction = finalBrushTrace.fraction;
				finalTrace.m_nTriangle = k;
				finalTrace.m_eRayType = RAY_TYPE_HULL;
				finalTrace.m_bStartInSolid = finalBrushTrace.startsolid;
				finalTrace.m_bExactHitPoint = false;
			}
		}
	}
	triangles.Purge();
	shapes.Purge();

	*trace = finalTrace;
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	// assert(trace->m_flFraction == 1 || (trace->m_vHitNormal.Length() > 0.9999 && trace->m_vHitNormal.Length() < 1.0001));
	return trace->m_flFraction < 1;
}

#define KZ_MAX_CLIP_PLANES          5
#define RAMP_INITIAL_RETRACE_LENGTH 0.03125f

static_function void ClipVelocity(Vector in, Vector normal, Vector &out)
{
	f32 backoff = -((in.x * normal.x) + ((normal.z * in.z) + (in.y * normal.y))) * 1;
	backoff = fmaxf(backoff, 0.0) + 0.03125;
	out = normal * backoff + in;
}

static_function bool IsValidMovementTrace(trace_t &tr, bbox_t bounds, CTraceFilterPlayerMovementCS *filter)
{
	trace_t stuck;

	if (tr.m_bStartInSolid)
	{
		return false;
	}

	// Maybe we don't need this one
	if (CloseEnough(tr.m_flFraction, 0.0f, FLT_EPSILON))
	{
		return false;
	}

	if (CloseEnough(tr.m_flFraction, 0.0f, FLT_EPSILON) && CloseEnough(tr.m_vHitNormal, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON))
	{
		return false;
	}

	// Is the plane deformed or some stupid shit?
	if (fabs(tr.m_vHitNormal.x) > 1.0f || fabs(tr.m_vHitNormal.y) > 1.0f || fabs(tr.m_vHitNormal.z) > 1.0f)
	{
		return false;
	}

	g_pKZUtils->TracePlayerBBox(tr.m_vEndPos, tr.m_vEndPos, bounds, filter, stuck);
	if (stuck.m_bStartInSolid || !CloseEnough(stuck.m_flFraction, 1.0f, FLT_EPSILON))
	{
		return false;
	}

	return true;
}

CConVar<bool> kz_rampbugfix("kz_rampbugfix", FCVAR_NONE, "Fix rampbugs", true);

// adapted from momentum mod 0.8.7 https://github.com/momentum-mod/game/blob/develop/mp/src/game/shared/momentum/mom_gamemovement.cpp
void TryPlayerMove_Custom(CCSPlayer_MovementServices *ms, CMoveData *mv, Vector *pFirstDest, trace_t *pFirstTrace, bool *bIsSurfing,
						  MovementPlayer *player)
{
	Vector dir;
	f32 d;
	Vector planes[KZ_MAX_CLIP_PLANES];
	int i, j, h;
	trace_t pm;
	Vector end;
	i32 numbumps = 8;

	i32 numplanes = 0; //  and not sliding along any planes

	bool stuck_on_ramp = false;   // lets assume client isn't stuck already
	bool has_valid_plane = false; // no plane info gathered yet

	Vector original_velocity = mv->m_vecVelocity;
	Vector primal_velocity = mv->m_vecVelocity;
	Vector fixed_origin = mv->m_vecAbsOrigin;

	f32 allFraction = 0;
	f32 time_left = g_pKZUtils->GetGlobals()->frametime; // Total time for this movement operation.

	Vector new_velocity;
	Vector valid_plane;
	new_velocity.Init();
	valid_plane.Init();

	bbox_t bounds;
	player->GetBBoxBounds(&bounds);
	auto pawn = player->GetPlayerPawn();
	CTraceFilterPlayerMovementCS filter(pawn);
	CConVarRef<float> sv_standable_normal("sv_standable_normal");
	f32 standableZ = sv_standable_normal.GetFloat();

	for (i32 bumpcount = 0; bumpcount < numbumps; bumpcount++)
	{
		if (mv->m_vecVelocity.LengthSqr() == 0)
		{
			break;
		}

		if (stuck_on_ramp && kz_rampbugfix.Get())
		{
			if (!has_valid_plane)
			{
				if (!CloseEnough(pm.m_vHitNormal, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON) && valid_plane != pm.m_vHitNormal)
				{
					valid_plane = pm.m_vHitNormal;
					has_valid_plane = true;
				}
				else
				{
					for (i = numplanes; i-- > 0;)
					{
						if (!CloseEnough(planes[i], Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON) && fabs(planes[i].x) <= 1.0f && fabs(planes[i].y) <= 1.0f
							&& fabs(planes[i].z) <= 1.0f && valid_plane != planes[i])
						{
							valid_plane = planes[i];
							has_valid_plane = true;
							break;
						}
					}
				}
			}

			if (has_valid_plane)
			{
				if (valid_plane.z >= standableZ && valid_plane.z <= 1.0f)
				{
					ClipVelocity(mv->m_vecVelocity, valid_plane, mv->m_vecVelocity);
					VectorCopy(mv->m_vecVelocity, original_velocity);
				}
				else
				{
					ClipVelocity(mv->m_vecVelocity, valid_plane, mv->m_vecVelocity);
					VectorCopy(mv->m_vecVelocity, original_velocity);
				}
			}
			else // We were actually going to be stuck, lets try and find a valid plane..
			{
				// this way we know fixed_origin isn't going to be stuck
				float offsets[] = {(bumpcount * 2) * -RAMP_INITIAL_RETRACE_LENGTH, 0.0f, (bumpcount * 2) * RAMP_INITIAL_RETRACE_LENGTH};
				int valid_planes = 0;
				valid_plane.Init(0.0f, 0.0f, 0.0f);

				// we have 0 plane info, so lets increase our bbox and search in all 27 directions to get a valid plane!
				for (i = 0; i < 3; i++)
				{
					for (j = 0; j < 3; j++)
					{
						for (h = 0; h < 3; h++)
						{
							Vector offset = {offsets[i], offsets[j], offsets[h]};

							Vector offset_mins = offset / 2.0f;
							Vector offset_maxs = offset / 2.0f;

							if (offset.x > 0.0f)
							{
								offset_mins.x /= 2.0f;
							}
							if (offset.y > 0.0f)
							{
								offset_mins.y /= 2.0f;
							}
							if (offset.z > 0.0f)
							{
								offset_mins.z /= 2.0f;
							}

							if (offset.x < 0.0f)
							{
								offset_maxs.x /= 2.0f;
							}
							if (offset.y < 0.0f)
							{
								offset_maxs.y /= 2.0f;
							}
							if (offset.z < 0.0f)
							{
								offset_maxs.z /= 2.0f;
							}

							bbox_t modifiedBounds = {bounds.mins - offset_mins, bounds.maxs + offset_maxs};
							g_pKZUtils->TracePlayerBBox(fixed_origin + offset, end - offset, modifiedBounds, &filter, pm);

							// Only use non deformed planes and planes with values where the start point is not from a
							//  solid
							if (fabs(pm.m_vHitNormal.x) <= 1.0f && fabs(pm.m_vHitNormal.y) <= 1.0f && fabs(pm.m_vHitNormal.z) <= 1.0f
								&& pm.m_flFraction > 0.0f && pm.m_flFraction < 1.0f && !pm.m_bStartInSolid)
							{
								valid_planes++;
								valid_plane += pm.m_vHitNormal;
							}
						}
					}
				}

				if (valid_planes && !CloseEnough(valid_plane, Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON))
				{
					has_valid_plane = true;
					valid_plane.NormalizeInPlace();
					continue;
				}
			}

			if (has_valid_plane)
			{
				VectorMA(fixed_origin, RAMP_INITIAL_RETRACE_LENGTH, valid_plane, fixed_origin);
			}
			else
			{
				stuck_on_ramp = false;
				continue;
			}
		}

		// Assume we can move all the way from the current origin to the
		//  end point.

		VectorMA(fixed_origin, time_left, mv->m_vecVelocity, end);

		// See if we can make it from origin to end point.
		// If their velocity Z is 0, then we can avoid an extra trace here during WalkMove.
		if (pFirstDest && end == *pFirstDest)
		{
			pm = *pFirstTrace;
		}
		else
		{
			if (stuck_on_ramp && has_valid_plane && kz_rampbugfix.Get())
			{
				g_pKZUtils->TracePlayerBBox(fixed_origin, end, bounds, &filter, pm);
				pm.m_vHitNormal = valid_plane;
			}
			else
			{
				g_pKZUtils->TracePlayerBBox(mv->m_vecAbsOrigin, end, bounds, &filter, pm);
#if 0
			// rngfix things
                if (sv_rngfix_enable.GetBool() && sv_slope_fix.GetBool() && pawn->m_MoveType() == MOVETYPE_WALK &&
                    pawn->m_hGroundEntity().Get() == nullptr && player->GetWaterLevel() < WL_Waist &&
                    g_pGameModeSystem->GetGameMode()->CanBhop() && !g_pGameModeSystem->GameModeIs(GAMEMODE_AHOP))
                {
					// TODO(GameChaos): is startsolid equivalent enough to allsolid?
                    //bool bValidHit = !pm.m_allsolid && pm.m_flFraction < 1.0f;
                    bool bValidHit = !pm.m_bStartInSolid && pm.m_flFraction < 1.0f;
					
                    bool bCouldStandHere = pm.m_vHitNormal.z >= standableZ && 
						mv->m_vecVelocity.z <= NON_JUMP_VELOCITY &&
						m_pPlayer->m_CurrentSlideTrigger == nullptr;
					
                    bool bMovingIntoPlane2D = (pm.m_vHitNormal.x * mv->m_vecVelocity.x) + (pm.m_vHitNormal.y * mv->m_vecVelocity.y) < 0.0f;
					
                    // Don't perform this fix on additional collisions this tick which have trace fraction == 0.0.
                    // This situation occurs when wedged between a standable slope and a ceiling.
                    // The player would be locked in place with full velocity (but no movement) without this check.
                    bool bWedged = m_pPlayer->GetInteraction(0).tick == g_pKZUtils->GetGlobals()->tickcount && pm.m_flFraction == 0.0f;
					
                    if (bValidHit && bCouldStandHere && bMovingIntoPlane2D && !bWedged)
                    {
                        Vector vecNewVelocity;
                        ClipVelocity(mv->m_vecVelocity, pm.m_vHitNormal, vecNewVelocity);
						
                        // Make sure allowing this collision would not actually be beneficial (2D speed gain)
                        if (vecNewVelocity.Length2DSqr() <= mv->m_vecVelocity.Length2DSqr())
                        {
                            // A fraction of 1.0 implies no collision, which means ClipVelocity will not be called.
                            // It also suggests movement for this tick is complete, so TryPlayerMove won't perform
                            // additional movement traces and the tick will essentially end early. We want this to
                            // happen because we need landing/jumping logic to be applied before movement continues.
                            // Ending the tick early is a safe and easy way to do this.
							
                            pm.m_flFraction = 1.0f;
                        }
                    }
                }
#endif
			}
		}

		if (bumpcount && kz_rampbugfix.Get() && pawn->m_hGroundEntity().Get() == nullptr && !IsValidMovementTrace(pm, bounds, &filter))
		{
			has_valid_plane = false;
			stuck_on_ramp = true;
			continue;
		}

		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.

#if 0
		// TODO(GameChaos): no allsolid exists, delete?
        if (pm.allsolid && !kz_rampbugfix.Get())
        {
            // entity is trapped in another solid
            VectorCopy(vec3_origin, mv->m_vecVelocity);
			
            if (m_pPlayer->m_nWallRunState >= WALLRUN_RUNNING)
            {
                //Msg( "EndWallrun because blocked\n" );
                EndWallRun();
            }
            return 4;
        }
#endif

		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and
		//  zero the plane counter.
		if (pm.m_flFraction > 0.0f)
		{
#if 0
			// TODO(GameChaos): This isn't a thing in source, delete?
            if ((!bumpcount || pawn->m_hGroundEntity().Get() != nullptr || !kz_rampbugfix.Get()) && numbumps > 0 && pm.m_flFraction == 1.0f)
            {
                // There's a precision issue with terrain tracing that can cause a swept box to successfully trace
                // when the end position is stuck in the triangle.  Re-run the test with an unswept box to catch that
                // case until the bug is fixed.
                // If we detect getting stuck, don't allow the movement
                trace_t stuck;
                g_pKZUtils->TracePlayerBBox(pm.m_vEndPos, pm.m_vEndPos, bounds, &filter, stuck);
				
                if ((stuck.m_bStartInSolid || stuck.m_flFraction != 1.0f) && !bumpcount && kz_rampbugfix.Get())
                {
                    has_valid_plane = false;
                    stuck_on_ramp = true;
                    continue;
                }
                else if (stuck.m_bStartInSolid || stuck.m_flFraction != 1.0f)
                {
                    Msg("Player will become stuck!!! allfrac: %f pm: %i, %f, %f, %f vs stuck: %i, %f, %f\n",
                        allFraction, pm.m_bStartInSolid, pm.m_flFraction, pm.m_vHitNormal.z, pm.fractionleftsolid,
                        stuck.m_bStartInSolid, stuck.m_flFraction, stuck.plane.normal.z);
                    VectorCopy(vec3_origin, mv->m_vecVelocity);
                    break;
                }
            }
#endif

			if (kz_rampbugfix.Get())
			{
				has_valid_plane = false;
				stuck_on_ramp = false;
			}

			// actually covered some distance
			VectorCopy(mv->m_vecVelocity, original_velocity);
			mv->m_vecAbsOrigin = pm.m_vEndPos;
			VectorCopy(mv->m_vecAbsOrigin, fixed_origin);
			allFraction += pm.m_flFraction;
			numplanes = 0;
		}

		// If we covered the entire distance, we are done
		//  and can return.
		if (CloseEnough(pm.m_flFraction, 1.0f, FLT_EPSILON))
		{
			break; // moved the entire distance
		}
		// Reduce amount of m_flFrameTime left by total time left * fraction
		//  that we covered.
		time_left -= time_left * pm.m_flFraction;

		// Did we run out of planes to clip against?
		if (numplanes >= KZ_MAX_CLIP_PLANES)
		{
			// this shouldn't really happen
			//  Stop our movement if so.
			VectorCopy(vec3_origin, mv->m_vecVelocity);
			break;
		}

		// Set up next clipping plane
		VectorCopy(pm.m_vHitNormal, planes[numplanes]);
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by
		// jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
		if (numplanes == 1 && pawn->m_MoveType() == MOVETYPE_WALK && pawn->m_hGroundEntity().Get() == nullptr)
		{
			// Is this a floor/slope that the player can walk on?
			if (planes[0][2] >= standableZ)
			{
				ClipVelocity(original_velocity, planes[0], new_velocity);
				VectorCopy(new_velocity, original_velocity);
			}
			else // either the player is surfing or slammed into a wall
			{
				ClipVelocity(original_velocity, planes[0], new_velocity);
			}

			VectorCopy(new_velocity, mv->m_vecVelocity);
			VectorCopy(new_velocity, original_velocity);
		}
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], mv->m_vecVelocity);
				for (j = 0; j < numplanes; j++)
				{
					if (j != i)
					{
						// Are we now moving against this plane?
						if (mv->m_vecVelocity.Dot(planes[j]) < 0)
						{
							break; // not ok
						}
					}
				}

				if (j == numplanes) // Didn't have to clip, so we're ok
				{
					break;
				}
			}

			// Did we go all the way through plane set
			if (i != numplanes)
			{
				// go along this plane
				// pmove.velocity is set in clipping call, no need to set again.
			}
			else
			{ // go along the crease
				if (numplanes != 2)
				{
					VectorCopy(vec3_origin, mv->m_vecVelocity);
					break;
				}

				// Fun fact time: these next five lines of code fix (vertical) rampbug
				if (CloseEnough(planes[0], planes[1], FLT_EPSILON))
				{
					// Why did the above return true? Well, when surfing, you can "clip" into the
					// ramp, due to the ramp not pushing you away enough, and when that happens,
					// a surfer cries. So the game thinks the surfer is clipping along two of the exact
					// same planes. So what we do here is take the surfer's original velocity,
					// and add the along the normal of the surf ramp they're currently riding down,
					// essentially pushing them away from the ramp.

					// Note: Technically the 20.0 here can be 2.0, but that causes "jitters" sometimes, so I found
					// 20 to be pretty safe and smooth. If it causes any unforeseen consequences, tweak it!
					VectorMA(original_velocity, 2.0f, planes[0], new_velocity);
					mv->m_vecVelocity.x = new_velocity.x;
					mv->m_vecVelocity.y = new_velocity.y;
					// Note: We don't want the player to gain any Z boost/reduce from this, gravity should be the
					// only force working in the Z direction!

					// Lastly, let's get out of here before the following lines of code make the surfer lose speed.
					break;
				}

				// Though now it's good to note: the following code is needed for when a ramp creates a "V" shape,
				// and pinches the surfer between two planes of differing normals.
				CrossProduct(planes[0], planes[1], dir);
				dir.NormalizeInPlace();
				d = dir.Dot(mv->m_vecVelocity);
				VectorScale(dir, d, mv->m_vecVelocity);
			}

			//
			// if original velocity is against the original velocity, stop dead
			// to avoid tiny oscillations in sloping corners
			//
			d = mv->m_vecVelocity.Dot(primal_velocity);
			if (d <= 0)
			{
				VectorCopy(vec3_origin, mv->m_vecVelocity);
				break;
			}
		}
	}

	if (CloseEnough(allFraction, 0.0f, FLT_EPSILON))
	{
		// We don't want to touch this!
		// If a client is triggering this, and if they are on a surf ramp they will stand still but gain velocity
		// that can build up for ever.
		// ...
		// However, if the player is currently sliding, another trace is needed to make sure the player does not
		// get stuck on an obtuse angle (slope to a flat ground) [eg bhop_w1s2]
#if 0
		// TODO(GameChaos): replicate this because we have slide triggers too?
        if (m_pPlayer->m_CurrentSlideTrigger.Get())
        {
            // Let's retrace in case we can go on our wanted direction.
            g_pKZUtils->TracePlayerBBox(mv->m_vecAbsOrigin, end, bounds, &filter, pm);
			
            // If we found something we stop.
            if (pm.m_flFraction < 1.0f)
            {
                VectorCopy(vec3_origin, mv->m_vecVelocity);
            }
            // Otherwise we just set our next pos and we ignore the bug.
            else
            {
                mv->m_vecAbsOrigin = end;
				
                // Adjust to be sure that we are on ground.
                StayOnGround();
            }
        }
        else
#endif
		{
			// otherwise default behavior
			VectorCopy(vec3_origin, mv->m_vecVelocity);
		}
	}
}
