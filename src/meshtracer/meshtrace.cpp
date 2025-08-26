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
	v3 verts[3];
};

struct Plane_t
{
	v3 normal;
	f32 distance;
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
	v3 endpos;       // final position
	v3 plane;        // surface normal at impact, transformed to world space
};

// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_local.h#L154
struct TraceRay_t
{
	v3 start;
	v3 end;
	v3 size[2]; // size of the box being swept through the model, mins & maxs
	f32 distance;

	void Init(v3 startIn, v3 endIn, v3 minsIn, v3 maxsIn)
	{
		this->start = startIn;
		this->end = endIn;
		this->size[0] = minsIn;
		this->size[1] = maxsIn;
		this->distance = v3::len(endIn - startIn);
	}

	void Init(const Vector &startIn, const Vector &endIn, const Vector &minsIn, const Vector &maxsIn)
	{
		this->start = V3(startIn);
		this->end = V3(endIn);
		this->size[0] = V3(minsIn);
		this->size[1] = V3(maxsIn);
		this->distance = v3::len(this->end - this->start);
	}
};

static_function bool IsNormalAxisAligned(v3 normal)
{
	return KZM_ABS(normal.x) == 1 || KZM_ABS(normal.y) == 1 || KZM_ABS(normal.z) == 1;
}

// Make doubly sure that the vector is normalised, though
//  this won't work on vectors with all components being 0
static_function v3 NormaliseStern(v3 vec)
{
	// TODO(GameChaos): is double normalisation the best solution for wonky normals?
	v3 result = vec;
	f32 len = v3::normInPlace(&result);
	if (len <= 0.9999f || len > 1.0001f)
	{
		result = v3::normalise(result);
	}
	return result;
}

static_function Brush_t GenerateTriangleAabbBevelPlanes(Triangle_t triangle)
{
	Brush_t result = {};
	Assert(result.planeCount == 0 && result.planes[0].normal.x == 0 && result.planes[0].normal.y == 0 && result.planes[0].normal.z == 0);

	// first 6 planes are the aabb of the triangle
	v3 triangleAabb[2] = {
		v3::min(v3::min(triangle.verts[0], triangle.verts[1]), triangle.verts[2]),
		v3::max(v3::max(triangle.verts[0], triangle.verts[1]), triangle.verts[2]),
	};

	// -x, -y, -z, +x, +y, +z
	for (i32 dir = -1, aabbInd = 0; dir < 2; dir += 2, aabbInd++)
	{
		for (i32 axis = 0; axis < 3; axis++)
		{
			result.planes[result.planeCount].normal[axis] = (f32)dir;
			result.planes[result.planeCount].distance = triangleAabb[aabbInd][axis] * (f32)dir;
			result.planeCount++;
		}
	}

	// next 2 planes are the 2 faces of the triangle
	v3 triangleNormal = NormaliseStern(v3::cross(triangle.verts[1] - triangle.verts[0], triangle.verts[2] - triangle.verts[0]));

	// check if we have a valid triangle normal, aka if the triangle isn't a line or a point
	// also check whether the triangle normal is axis aligned, in which case skip it, because it
	//  would get duplicated by the aabb planes.
	if (!IsNormalAxisAligned(triangleNormal))
	{
		f32 triangleDistance = v3::dot(triangle.verts[0], triangleNormal);
		// front face of the triangle
		result.planes[result.planeCount].normal = triangleNormal;
		result.planes[result.planeCount].distance = triangleDistance;
		result.planeCount++;
		// back face of the triangle
		result.planes[result.planeCount].normal = -triangleNormal;
		result.planes[result.planeCount].distance = -triangleDistance;
		result.planeCount++;
	}

	// next 9 planes are planes made from the edges of the triangle
	for (i32 axis = 0; axis < 3; axis++)
	{
		for (i32 p1 = 0; p1 < 3; p1++)
		{
			i32 p2 = (p1 + 1) % 3;
			v3 edgeNormal = v3::normalise(triangle.verts[p2] - triangle.verts[p1]);
			if (!IsNormalAxisAligned(edgeNormal))
			{
				v3 axisNormal = {};
				Assert(axisNormal.x == 0 && axisNormal.y == 0 && axisNormal.z == 0);
				axisNormal[axis] = triangleNormal[axis] < 0 ? 1 : -1;
				v3 planeNormal = NormaliseStern(v3::cross(axisNormal, edgeNormal));
				result.planes[result.planeCount].normal = planeNormal;
				result.planes[result.planeCount].distance = v3::dot(planeNormal, triangle.verts[p1]);
				result.planeCount++;
			}
		}
	}

	return result;
}

static_function f32 CalculatePlaneAabbExpansion(v3 bounds[2], Plane_t plane)
{
	v3 offset = V3(bounds[plane.normal.x < 0 ? 1 : 0].x, bounds[plane.normal.y < 0 ? 1 : 0].y, bounds[plane.normal.z < 0 ? 1 : 0].z);
	f32 result = v3::dot(offset, plane.normal);
	return result;
}

static_function f32 CalculateEpsilonExpansion(f32 epsilon, v3 plane)
{
	v3 offset = V3(plane.x < 0 ? -epsilon : epsilon,
				   plane.y < 0 ? -epsilon : epsilon,
				   plane.z < 0 ? -epsilon : epsilon);
	f32 result = v3::dot(offset, plane);
	return result;
}

CConVar<float> kz_surface_clip_epsilon("kz_surface_clip_epsilon", FCVAR_NONE, "Surface clip epsilon", 0.03125f);

// #define SURFACE_CLIP_EPSILON 0
//  https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_trace.c#L483
static_function TraceResult_t TraceThroughBrush(TraceRay_t ray, Brush_t brush, f32 surfaceClipEpsilon = kz_surface_clip_epsilon.Get())
{
	TraceResult_t result = {};
	result.endpos = ray.end;
	result.fraction = 1;
	result.distance = ray.distance;
	// impossible to have brushes with less than 6 planes.
	assert(brush.planeCount >= 6);
	if (brush.planeCount < 6)
	{
		assert(false);
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
		
		// NOTE(GameChaos): expand epsilon by the correct amount, similar to CalculatePlaneAabbExpansion.
		//  this is so that the player doesn't hit backfacing normals.
		f32 epsilon = CalculateEpsilonExpansion(surfaceClipEpsilon, plane.normal);
		
		// adjust the plane distance apropriately for mins/maxs
		f32 dist = plane.distance - CalculatePlaneAabbExpansion(ray.size, plane);

		f32 d1 = v3::dot(ray.start, plane.normal) - dist;
		f32 d2 = v3::dot(ray.end, plane.normal) - dist;
		if (d2 > 0)
		//if (d2 > epsilon)
		{
			getout = true; // endpoint is not in solid
		}
		if (d1 > 0)
		//if (d1 > epsilon)
		{
			startout = true;
		}

		// if completely in front of face, no intersection with the entire brush
		//if (d1 > 0 && (d2 >= epsilon || d2 >= d1))
		if (d1 > epsilon && d2 > epsilon)
		{
			return result;
		}

		// if it doesn't cross the plane, the plane isn't relevent
		//if (d1 <= 0 && d2 <= 0)
		if (d1 <= epsilon && d2 <= epsilon)
		{
			continue;
		}

		// crosses face
		if (d1 > d2)
		{ // enter
			f32 f = (d1 - epsilon) / (d1 - d2);
			if (f < 0)
			{
				f = 0;
			}
			// NOTE(GameChaos): pick the plane that is the most aligned to ray direction
			//  otherwise traces that have the same fraction will be resolved by random chance
			if (f == enterFrac && enterFrac > -1)
			{
				v3 delta = ray.end - ray.start;
				f32 newDot = v3::dot(delta, plane.normal);
				f32 oldDot = v3::dot(delta, clipplane.normal);
				if (newDot > oldDot)
				{
					enterFrac = f;
					clipplane = plane;
				}
			}
			else if (f > enterFrac)
			{
				enterFrac = f;
				clipplane = plane;
			}
		}
		else
		{ // leave
			// NOTE(GameChaos): Changed (d1 + epsilon) to (d1 - epsilon)
			//  to fix seamshot bug. https://www.youtube.com/watch?v=YnCG6fbTQ-I
			// f32 f = (d1 + epsilon) / (d1 - d2);
			f32 f = (d1 - epsilon) / (d1 - d2);
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
			result.plane = clipplane.normal;
		}
	}

	if (result.fraction == 1)
	{
		result.endpos = ray.end;
	}
	else
	{
		result.endpos = v3::fma(ray.end - ray.start, result.fraction, ray.start);
		// result.endpos = ((ray.end - ray.start) * result.fraction) + ray.start;
	}
	result.distance *= result.fraction;
	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	Assert(result.allsolid || result.fraction == 1 || v3::len(result.plane) > 0.9999f && v3::len(result.plane) < 1.0001f);
	return result;
}

// source: https://gdbooks.gitbooks.io/3dcollisions/content/Chapter4/aabb-triangle.html
static_function bool IntersectAabbTriangle(TraceRay_t ray, Triangle_t tri, f32 surfaceClipEpsilon)
{
	v3 centre = (ray.size[1] + ray.size[0]) * 0.5f + ray.start;
	v3 extents = (ray.size[1] - ray.size[0]) * 0.5f + surfaceClipEpsilon;
	// Translate the triangle as conceptually moving the AABB to origin
	for (i32 i = 0; i < 3; i++)
	{
		tri.verts[i] -= centre;
	}

	// Compute the edge vectors of the triangle  (ABC)
	// That is, get the lines between the points as vectors
	v3 e0 = tri.verts[1] - tri.verts[0]; // B - A
	v3 e1 = tri.verts[2] - tri.verts[1]; // C - B
	v3 e2 = tri.verts[0] - tri.verts[2]; // A - C

	v3 xAxis = V3(1, 0, 0);
	v3 yAxis = V3(0, 1, 0);
	v3 zAxis = V3(0, 0, 1);

	v3 axes[] = {
		v3::cross(xAxis, e0),
		v3::cross(xAxis, e1),
		v3::cross(xAxis, e2),
		v3::cross(yAxis, e0),
		v3::cross(yAxis, e1),
		v3::cross(yAxis, e2),
		v3::cross(zAxis, e0),
		v3::cross(zAxis, e1),
		v3::cross(zAxis, e2),
		// aabb axes
		xAxis,
		yAxis,
		zAxis,
		// triangle axis
		v3::cross(e0, e1),
	};

	for (i32 i = 0; i < Q_ARRAYSIZE(axes); i++)
	{
		// Project all 3 vertices of the triangle onto the Separating axis
		f32 p0 = v3::dot(tri.verts[0], axes[i]);
		f32 p1 = v3::dot(tri.verts[1], axes[i]);
		f32 p2 = v3::dot(tri.verts[2], axes[i]);

		// Project the AABB onto the separating axis
		// We don't care about the end points of the projection
		// just the length of the half-size of the AABB
		// That is, we're only casting the extents onto the
		// separating axis, not the AABB center. We don't
		// need to cast the center, because we know that the
		// aabb is at origin compared to the triangle!

		f32 r = extents.x * KZM_ABS(v3::dot(xAxis, axes[i])) + extents.y * KZM_ABS(v3::dot(yAxis, axes[i]))
				+ extents.z * KZM_ABS(v3::dot(zAxis, axes[i]));

		// Now do the actual test, basically see if either of
		// the most extreme of the triangle points intersects r
		if (KZM_MAX(-KZM_MAX3(p0, p1, p2), KZM_MIN3(p0, p1, p2)) > r)
		{
			// This means BOTH of the points of the projected triangle
			// are outside the projected half-length of the AABB
			// Therefore the axis is separating and we can exit
			return false;
		}
	}
	return true;
}

static_function TraceResult_t TraceAabbTriangle(TraceRay_t ray, Triangle_t tri, f32 surfaceClipEpsilon = kz_surface_clip_epsilon.Get())
{
	TraceResult_t result = {};
	result.fraction = 1;
#if 1
	v3 delta = ray.end - ray.start;
	// if start and end are the same use static check
	if (delta.x == 0 && delta.y == 0 && delta.z == 0)
	{
		if (IntersectAabbTriangle(ray, tri, 0))
		{
			result.startsolid = true;
			result.fraction = 0;
			result.distance = 0;
			// include the normal of the triangle for funsies
			result.plane = NormaliseStern(v3::cross(tri.verts[1] - tri.verts[0], tri.verts[2] - tri.verts[0]));
		}
		result.endpos = ray.start;
	}
	else
#endif
	{
		Brush_t triangleBrush = GenerateTriangleAabbBevelPlanes(tri);
		result = TraceThroughBrush(ray, triangleBrush, surfaceClipEpsilon);
	}
	return result;
}

static_function void ComputeSweptAABB(v3 boxMins, v3 boxMaxs, v3 delta, f32 epsilon, Vector &outMins, Vector &outMaxs)
{
	// Swept AABB is the min/max of both boxes
	outMins = (v3::min(boxMins, boxMins + delta) - epsilon).Vec();
	outMaxs = (v3::max(boxMaxs, boxMaxs + delta) + epsilon).Vec();
}

static_function void FindTrianglesInBox(const RnNode_t *node, CUtlVector<uint32> &triangles, Vector mins, Vector maxs,
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

static_function void TraceResultToCGameTrace(CGameTrace *out, TraceResult_t tr, v3 startPos, HPhysicsShape shape, i32 triangleInd)
{
	v3 delta = tr.endpos - startPos;

	CGameTrace newTrace;
	newTrace.Init();
	// TODO(GameChaos): defaults!!!
	// out->m_pSurfaceProperties = ;
	// out->m_pHitbox = &defaultHitBox;

	newTrace.m_pSurfaceProperties = out->m_pSurfaceProperties;
	newTrace.m_pHitbox = out->m_pHitbox;
	newTrace.m_eRayType = RAY_TYPE_HULL;
	newTrace.m_vEndPos = tr.endpos.Vec();
	newTrace.m_vStartPos = startPos.Vec();
	// static_persist CDefaultHitbox defaultHitBox = CDefaultHitbox();
	if (tr.fraction < 1)
	{
		RnCollisionAttr_t *collisionAttr = shape ? shape->GetCollisionAttr() : nullptr;
		CEntityHandle ehandle = CEntityHandle(collisionAttr ? collisionAttr->m_nEntityId : INVALID_EHANDLE_INDEX);
		CBaseEntity *ent = ehandle.IsValid() ? static_cast<CBaseEntity *>(ehandle.Get()) : nullptr;
		CPhysicsBody *body = shape ? static_cast<CPhysicsBody *>(shape->GetOwnerBody()) : nullptr;
		RnMesh_t *mesh = shape ? shape->GetMesh() : nullptr;

		CRnBody *rnBody = body ? body->m_pRnBody : nullptr;
		CPhysAggregateInstance *aggregateInstance = rnBody ? rnBody->m_pAggregateInstance : nullptr;
		CPhysAggregateData *aggregateData = aggregateInstance ? aggregateInstance->aggregateData : nullptr;

		CTransform transform;
		transform.SetToIdentity();
		if (ent)
		{
			transform.m_vPosition = ent->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
			transform.m_orientation = Quaternion(ent->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
		}

		// TODO(GameChaos):
		// newTrace.m_pSurfaceProperties = aggregateData && mesh ? aggregateData->m_SurfaceProperties[mesh->m_nSurfacePropertyIndex] : nullptr;
		newTrace.m_pEnt = ent;
		// TODO(GameChaos):
		// newTrace.m_pHitbox = hitbox;
		newTrace.m_hBody = body;
		newTrace.m_hShape = shape;
		newTrace.m_BodyTransform = transform;
		if (collisionAttr)
		{
			newTrace.m_ShapeAttributes = *collisionAttr;
			newTrace.m_nContents = newTrace.m_ShapeAttributes.m_nInteractsAs;
		}
		else
		{
			newTrace.m_ShapeAttributes = RnCollisionAttr_t();
			newTrace.m_nContents = 0;
		}
		newTrace.m_vStartPos = startPos.Vec();
		newTrace.m_vEndPos = tr.endpos.Vec();
		newTrace.m_vHitNormal = tr.plane.Vec();
		newTrace.m_vHitPoint = tr.endpos.Vec();
		// NOTE(GameChaos): idk what this should be exactly
		newTrace.m_flHitOffset = DotProduct(newTrace.m_vHitPoint, newTrace.m_vHitNormal);
		newTrace.m_flFraction = tr.fraction;
		newTrace.m_nTriangle = triangleInd;
		newTrace.m_eRayType = RAY_TYPE_HULL;
		newTrace.m_bStartInSolid = tr.startsolid;
		newTrace.m_bExactHitPoint = false;
	}
	*out = newTrace;
}

bool RetraceHull(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter &filter, CGameTrace *trace)
{
	f32 epsilon = kz_surface_clip_epsilon.Get();
	TraceRay_t traceRay;
	traceRay.Init(start, end, ray.m_Hull.m_vMins, ray.m_Hull.m_vMaxs);

	TraceResult_t finalBrushTrace = {};
	finalBrushTrace.endpos = V3(end);
	finalBrushTrace.fraction = 1;
	finalBrushTrace.distance = traceRay.distance;

	CUtlVector<HPhysicsShape> shapes;
	Ray_t::Hull_t sweptAabb;
	// extend the extents a bit to make sure we get every possible triangle we could theoretically hit!
	v3 delta = V3(end - start);
	ComputeSweptAABB(V3(ray.m_Hull.m_vMins), V3(ray.m_Hull.m_vMaxs), delta, epsilon + 1, sweptAabb.m_vMins, sweptAabb.m_vMaxs);
	Ray_t queryRay;
	queryRay.Init(sweptAabb.m_vMins, sweptAabb.m_vMaxs);
	g_pKZUtils->Query(&shapes, &queryRay, &start, &filter, false);

	HPhysicsShape finalShape = nullptr;
	i32 finalTriangle = -1;
	CUtlVector<uint32> triangles;
	EntityInstanceIter_t iter;
	FOR_EACH_VEC(shapes, i)
	{
		HPhysicsShape shape = shapes[i];
		PhysicsShapeType_t shapeType = shape->GetShapeType();
		if (shapeType != SHAPE_MESH)
		{
			continue;
		}
		RnMesh_t *mesh = shape->GetMesh();
		if (!mesh)
		{
			assert(false);
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
			assert(false);
			continue;
		}
		CEntityHandle ehandle = CEntityHandle(collisionAttr->m_nEntityId);
		auto ent = static_cast<CBaseEntity *>(ehandle.Get());
		auto body = shape->GetOwnerBody();
		if (!ent || !body)
		{
			assert(false);
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
			Triangle_t tri = {V3(utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[0]])),
							  V3(utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[1]])),
							  V3(utils::TransformPoint(transform, mesh->m_Vertices[triangle->m_nIndex[2]]))};

			// Do our own trace calculation
			TraceResult_t brushTrace = TraceAabbTriangle(traceRay, tri);
			// META_CONPRINTF("start %f %f %fbrushTrace as%i ss%i f%f d%f e%f %f %f p%f %f %f\n",
			// start.x, start.y, start.z, brushTrace.allsolid, brushTrace.startsolid, brushTrace.fraction, brushTrace.distance, brushTrace.endpos.x,
			// brushTrace.endpos.y, brushTrace.endpos.z, brushTrace.plane.x, brushTrace.plane.y, brushTrace.plane.z);
			
			
			if (brushTrace.fraction == finalBrushTrace.fraction && finalBrushTrace.fraction < 1)
			{
				// try to handle traces with an identical fraction gracefully
				f32 newDot = v3::dot(brushTrace.plane, delta);
				f32 oldDot = v3::dot(finalBrushTrace.plane, delta);
				// If the new normal is more in line with the
				//  trace delta (end-start), then use the new one.
				if (newDot > oldDot)
				{
					// use new trace
					finalBrushTrace = brushTrace;
					finalShape = shape;
					finalTriangle = k;
				}
			}
			else if (brushTrace.fraction < finalBrushTrace.fraction)
			{
				finalBrushTrace = brushTrace;
				finalShape = shape;
				finalTriangle = k;
			}
		}
	}
	triangles.Purge();
	shapes.Purge();

#if 0
	if (finalBrushTrace.fraction < 1)
	{
		META_CONPRINTF("start(%f, %f, %f) delta(%f,%f,%f) end(%f,%f,%f) brushTrace as(%i) ss(%i) f(%f) d(%f) e(%f, %f, %f) p(%f, %f, %f)\n",
					   start.x, start.y, start.z, delta.x, delta.y, delta.z, end.x, end.y, end.z, finalBrushTrace.allsolid, finalBrushTrace.startsolid, finalBrushTrace.fraction, finalBrushTrace.distance, finalBrushTrace.endpos.x, finalBrushTrace.endpos.y, finalBrushTrace.endpos.z, finalBrushTrace.plane.x, finalBrushTrace.plane.y, finalBrushTrace.plane.z);
	}
#endif
	TraceResultToCGameTrace(trace, finalBrushTrace, V3(start), finalShape, finalTriangle);
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	assert(finalBrushTrace.allsolid || trace->m_flFraction == 1 || (trace->m_vHitNormal.Length() > 0.9999 && trace->m_vHitNormal.Length() < 1.0001));
	return trace->m_flFraction < 1;
}

#define KZ_TPM_VALVE 1
#if KZ_TPM_VALVE
#define KZ_MAX_CLIP_PLANES          4
#define KZ_NUMBUMPS                 4
#else
#define KZ_MAX_CLIP_PLANES          5
#define KZ_NUMBUMPS                 8
#endif
#define RAMP_INITIAL_RETRACE_LENGTH 0.03125f
#define KZ_CLIP_PUSHAWAY 0.0f

#if KZ_TPM_VALVE
static_function void ClipVelocity(const Vector &in, const Vector &normal, Vector &out, float overbounce)
{
	f32 backoff = -DotProduct(in, normal) * overbounce;
	backoff = fmaxf(backoff, 0) + KZ_CLIP_PUSHAWAY;
	out = in + normal * backoff;
}
#else
static_function void ClipVelocity(const Vector &in, const Vector &normal, Vector &out, float overbounce)
{
	// Determine how far along plane to slide based on incoming direction.
	float backoff = DotProduct(in, normal) * overbounce;
	out = in - (normal * backoff);

	// iterate once to make sure we aren't still moving through the plane
	float adjust = DotProduct(out, normal);
	if (adjust < KZ_CLIP_PUSHAWAY)
	{
		adjust = KZM_MIN(adjust, -KZ_CLIP_PUSHAWAY);
		out -= (normal * adjust);
	}
}
#endif

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
	if (KZM_ABS(tr.m_vHitNormal.x) > 1.0f || KZM_ABS(tr.m_vHitNormal.y) > 1.0f || KZM_ABS(tr.m_vHitNormal.z) > 1.0f)
	{
		return false;
	}

	g_pKZUtils->TracePlayerBBox(tr.m_vEndPos, tr.m_vEndPos, bounds, filter, stuck);
	if (stuck.m_bStartInSolid || stuck.m_flFraction != 1)
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
	CConVarRef<float> sv_walkable_normal("sv_walkable_normal");
	CConVarRef<float> sv_bounce("sv_bounce");
	f32 standableZ = sv_standable_normal.GetFloat();
	f32 walkableZ = sv_walkable_normal.GetFloat();
	f32 bounce = sv_bounce.GetFloat();
	f32 fullBounce = 1 + bounce * (1.0f - ms->m_flSurfaceFriction());
#if KZ_TPM_VALVE
	bool inAir = (pawn->m_fFlags & FL_ONGROUND) == 0;
#endif

	for (i32 bumpcount = 0; bumpcount < KZ_NUMBUMPS; bumpcount++)
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
						if (!CloseEnough(planes[i], Vector(0.0f, 0.0f, 0.0f), FLT_EPSILON) && KZM_ABS(planes[i].x) <= 1.0f
							&& KZM_ABS(planes[i].y) <= 1.0f && KZM_ABS(planes[i].z) <= 1.0f && valid_plane != planes[i])
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
					ClipVelocity(mv->m_vecVelocity, valid_plane, mv->m_vecVelocity, 1);
					VectorCopy(mv->m_vecVelocity, original_velocity);
				}
				else
				{
					ClipVelocity(mv->m_vecVelocity, valid_plane, mv->m_vecVelocity, fullBounce);
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
							if (KZM_ABS(pm.m_vHitNormal.x) <= 1.0f && KZM_ABS(pm.m_vHitNormal.y) <= 1.0f && KZM_ABS(pm.m_vHitNormal.z) <= 1.0f
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
		
#if KZ_TPM_VALVE
		// AG2 Update: Fixed several cases where a player would get stuck on map geometry while surfing
		if (numplanes == 1)
		{
			VectorMA(end, 0.03125f, planes[0], end);
		}
#endif

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
                        ClipVelocity(mv->m_vecVelocity, pm.m_vHitNormal, vecNewVelocity, 1);
						
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

#if 1
		// If we started in a solid object, or we were in solid space
		//  the whole way, zero out our velocity and return that we
		//  are blocked by floor and wall.

		// TODO(GameChaos): no allsolid exists, delete?
        //if (pm.allsolid && !kz_rampbugfix.Get())
        if (pm.m_bStartInSolid && !kz_rampbugfix.Get())
        {
            // entity is trapped in another solid
			mv->m_vecVelocity.Init();
            return;
        }
#endif
#if KZ_TPM_VALVE
		if (allFraction == 0.0f)
		{
			if (pm.m_flFraction < 1.0f && mv->m_vecVelocity.Length() * time_left >= 0.03125f && (pm.m_flFraction * mv->m_vecVelocity.Length() * time_left) < 0.03125f)
			{
				pm.m_flFraction = 0.0f;
			}
		}
#endif
		// If we moved some portion of the total distance, then
		//  copy the end position into the pmove.origin and
		//  zero the plane counter.
#if KZ_TPM_VALVE
		if (pm.m_flFraction * MAX(1.0, mv->m_vecVelocity.Length()) > 0.03125f)
#else
		if (pm.m_flFraction > 0.0f)
#endif
		{
#if 1
			// TODO(GameChaos): This isn't a thing in source 2, delete?
			if ((!bumpcount || pawn->m_hGroundEntity().Get() != nullptr || !kz_rampbugfix.Get()) && KZ_NUMBUMPS > 0 && pm.m_flFraction == 1.0f)
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
					META_CONPRINTF("Player will become stuck!!! allfrac: %f pm: %i, %f, %f vs stuck: %i, %f, %f\n", allFraction, pm.m_bStartInSolid,
								   pm.m_flFraction, pm.m_vHitNormal.z, stuck.m_bStartInSolid, stuck.m_flFraction, stuck.m_vHitNormal.z);
					mv->m_vecVelocity.Init();
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
			original_velocity = mv->m_vecVelocity;
			mv->m_vecAbsOrigin = pm.m_vEndPos;
			fixed_origin = mv->m_vecAbsOrigin;
#if !KZ_TPM_VALVE
			allFraction += pm.m_flFraction;
#endif
			numplanes = 0;
		}
#if KZ_TPM_VALVE
		allFraction += pm.m_flFraction;
#endif

		// If we covered the entire distance, we are done
		//  and can return.
		if (CloseEnough(pm.m_flFraction, 1, FLT_EPSILON))
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
			mv->m_vecVelocity.Init();
			break;
		}
		
#if KZ_TPM_VALVE
		// 2024-11-07 update also adds a low velocity check... This is only correct as long as you don't collide with other players.
		if (inAir && mv->m_vecVelocity.z < 0.0f)
		{
			if (pm.m_vHitNormal.z >= standableZ /*&& this->player->GetMoveServices()->CanStandOn(pm)*/ && mv->m_vecVelocity.Length2D() < 1.0f)
			{
				mv->m_vecVelocity.Init();
				break;
			}
		}
#endif

		// Set up next clipping plane
		planes[numplanes] = pm.m_vHitNormal;
		numplanes++;

		// modify original_velocity so it parallels all of the clip planes
		//

		// reflect player velocity
		// Only give this a try for first impact plane because you can get yourself stuck in an acute corner by
		// jumping in place
		//  and pressing forward and nobody was really using this bounce/reflection feature anyway...
#if KZ_TPM_VALVE
		// standard sdk 2013
		if (numplanes == 1)
		{
			for (i = 0; i < numplanes; i++)
			{
				if (planes[i][2] > walkableZ)
				{
					// floor or slope
					ClipVelocity(original_velocity, planes[i], new_velocity, 1);
					original_velocity = new_velocity;
				}
				else
				{
					ClipVelocity(original_velocity, planes[i], new_velocity, fullBounce);
				}
			}
			mv->m_vecVelocity = new_velocity;
			original_velocity = new_velocity;
		}
#else
		// momentum mod implementation
		if (numplanes == 1 && pawn->m_MoveType() == MOVETYPE_WALK && pawn->m_hGroundEntity().Get() == nullptr)
		{
			// Is this a floor/slope that the player can walk on?
			if (planes[0][2] >= walkableZ)
			{
				ClipVelocity(original_velocity, planes[0], new_velocity, 1);
				original_velocity = new_velocity;
			}
			else // either the player is surfing or slammed into a wall
			{
				ClipVelocity(original_velocity, planes[0], new_velocity, fullBounce);
			}
			mv->m_vecVelocity = new_velocity;
			original_velocity = new_velocity;
		}
#endif
		else
		{
			for (i = 0; i < numplanes; i++)
			{
				ClipVelocity(original_velocity, planes[i], mv->m_vecVelocity, 1);
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
					mv->m_vecVelocity.Init();
					break;
				}

				// Fun fact time: these next five lines of code fix (vertical) rampbug
				if (CloseEnough(planes[0], planes[1], FLT_EPSILON) && kz_rampbugfix.Get())
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
				dir = NormaliseStern(V3(dir)).Vec();
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
				mv->m_vecVelocity.Init();
				break;
			}
		}
	}

	if (CloseEnough(allFraction, 0.0f, FLT_EPSILON))
	//if (allFraction == 0)
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
				mv->m_vecVelocity.Init();
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
			mv->m_vecVelocity.Init();
		}
	}
}
