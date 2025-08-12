#include "meshtrace.h"
#include "utils/utils.h"
#include "utils/interfaces.h"
#include "sdk/entity/cbaseentity.h"

static_function bool IsNormalAxisAligned(Vector normal)
{
	return fabs(normal.x) == 1 || fabs(normal.y) == 1 || fabs(normal.z) == 1;
}

static_function Brush_t GenerateTriangleAabbBevelPlanes(Triangle_t triangle)
{
	Brush_t result = {0};

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
	Vector triangleNormal = CrossProduct(triangle.verts[1] - triangle.verts[0], triangle.verts[2] - triangle.verts[0]).Normalized();

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
				Vector planeNormal = CrossProduct(axisNormal, edgeNormal).Normalized();
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

// #define SURFACE_CLIP_EPSILON 0
//  https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_trace.c#L483
static_function TraceResult_t TraceThroughBrush(TraceRay_t ray, Brush_t brush, f32 surfaceClipEpsilon = kz_surface_clip_epsilon.Get())
{
	TraceResult_t result = {0};
	result.endpos = ray.end;
	result.fraction = 1;
	if (!brush.planeCount)
	{
		return result;
	}

	f32 enterFrac = -1;
	f32 leaveFrac = 1;
	Plane_t clipplane;

	bool getout = false;
	bool startout = false;

	// compare the trace against all planes of the brush.
	//  find the latest time the trace crosses a plane towards the interior
	//  and the earliest time the trace crosses a plane towards the exterior
	for (i32 i = 0; i < brush.planeCount; i++)
	{
		Plane_t plane = brush.planes[i];

		// adjust the plane distance apropriately for mins/maxs
		// FIXME: use signbits into 8 way lookup for each mins/maxs
		// f32 dist = plane.dist - v3dot(ray.offsets[plane.signbits], plane.normal);
		f32 dist = plane.distance - CalculatePlaneAabbExpansion(ray.size, plane);

		f32 d1 = DotProduct(ray.start, plane.normal) - dist;
		f32 d2 = DotProduct(ray.end, plane.normal) - dist;
		if (d2 > 0)
		{
			getout = true; // endpoint is not in solid
		}
		if (d1 > 0)
		{
			startout = true;
		}

		// if completely in front of face, no intersection with the entire brush
		if (d1 > 0 && (d2 >= surfaceClipEpsilon || d2 >= d1))
		{
			return result;
		}

		// if it doesn't cross the plane, the plane isn't relevent
		if (d1 <= 0 && d2 <= 0)
		{
			continue;
		}

		// crosses face
		if (d1 > d2)
		{ // enter
			f32 f = (d1 - surfaceClipEpsilon) / (d1 - d2);
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
			f32 f = (d1 + surfaceClipEpsilon) / (d1 - d2);
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
	// If allsolid is set (was entirely inside something solid), the plane is not valid.
	// If fraction == 1.0, we never hit anything, and thus the plane is not valid.
	// Otherwise, the normal on the plane should have unit length
	assert(result.allsolid || result.fraction == 1 || result.plane.normal.Length() > 0.9999);
	return result;
}

static_function void ComputeSweptAABB(const Vector &boxMins, const Vector &boxMaxs, const Vector &delta, Vector &outMins, Vector &outMaxs)
{
	// AABB at end position
	Vector endMins = boxMins + delta;
	Vector endMaxs = boxMaxs + delta;

	// Swept AABB is the min/max of both boxes
	outMins.x = Min(boxMins.x, endMins.x);
	outMins.y = Min(boxMins.y, endMins.y);
	outMins.z = Min(boxMins.z, endMins.z);
	outMaxs.x = Max(boxMaxs.x, endMaxs.x);
	outMaxs.y = Max(boxMaxs.y, endMaxs.y);
	outMaxs.z = Max(boxMaxs.z, endMaxs.z);
}

static_function void FindTrianglesInBox(const RnNode_t *node, CUtlVector<uint32> &triangles, const Vector &mins, const Vector &maxs,
										const CTransform *transform = nullptr)
{
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

bool RetraceShape(const Ray_t &ray, const Vector &start, const Vector &end, const CTraceFilter &filter, CGameTrace &trace)
{
	// No need to retrace missed rays.
	if (!trace.DidHit())
	{
		return true;
	}
	g_pKZUtils->ClearOverlays();
	Vector extent = end - start;

	CUtlVectorFixedGrowable<PhysicsTrace_t, 128> results;
	g_pKZUtils->CastBoxMultiple(&results, &ray, &start, &extent, &filter);
	// Early exit if we run into non-mesh shapes because we can't trace against them yet.
	FOR_EACH_VEC(results, i)
	{
		PhysicsTrace_t &result = results[i];
		PhysicsShapeType_t shapeType = result.m_HitShape->GetShapeType();
		if (shapeType != SHAPE_MESH)
		{
			return false;
		}
	}

	FOR_EACH_VEC(results, i)
	{
		PhysicsTrace_t &result = results[i];
		CTransform transform;
		g_pKZUtils->GetPhysicsBodyTransform(result.m_HitObject, transform);
		RnMesh_t *mesh = result.m_HitShape->GetMesh();
		if (mesh->m_Nodes.Count() > 0)
		{
			RnNode_t &node = mesh->m_Nodes[i];
			CUtlVector<u32> triangles;
			Vector sweptMins, sweptMaxs;
			ComputeSweptAABB(start + ray.m_Hull.m_vMins, start + ray.m_Hull.m_vMaxs, extent, sweptMins, sweptMaxs);
			FindTrianglesInBox(&node, triangles, sweptMins, sweptMaxs, &transform);
			if (triangles.Count() == 0)
			{
				return false;
			}
			META_CONPRINTF("  Number of relevant triangles: %i\n", triangles.Count());
			META_CONPRINTF("  Original triangle (%i): normal %f %f %f, endpos %f %f %f, fraction %f\n", trace.m_nTriangle, trace.m_vHitNormal.x,
						   trace.m_vHitNormal.y, trace.m_vHitNormal.z, trace.m_vEndPos.x, trace.m_vEndPos.y, trace.m_vEndPos.z, trace.m_flFraction);
			FOR_EACH_VEC(triangles, i)
			{
				const RnTriangle_t &triangle = mesh->m_Triangles[triangles[i]];
				Vector v0 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[0]]);
				Vector v1 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[1]]);
				Vector v2 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[2]]);

				if (triangles[i] == trace.m_nTriangle)
				{
					g_pKZUtils->AddTriangleOverlay(v0, v1, v2, 0, 255, 0, 128, true, -1.0f);
					continue; // skip the original triangle, we already know it hit
				}
				// Reconstruct the ray
				TraceRay_t traceRay;
				traceRay.start = start;
				traceRay.end = end;
				traceRay.size.m_vMinBounds = ray.m_Hull.m_vMins;
				traceRay.size.m_vMaxBounds = ray.m_Hull.m_vMaxs;

				Triangle_t tri(v0, v1, v2);
				Brush_t triangleBrush = GenerateTriangleAabbBevelPlanes(tri);

				// Do our own trace calculation
				TraceResult_t result = TraceThroughBrush(traceRay, triangleBrush);
				if (result.fraction < 1.0f)
				{
					META_CONPRINTF("  Triangle %i: normal %f %f %f, endpos %f %f %f, fraction %f\n", triangles[i], result.plane.normal.x,
								   result.plane.normal.y, result.plane.normal.z, result.endpos.x, result.endpos.y, result.endpos.z, result.fraction);
				}
				if (trace.DidHit() && trace.m_nTriangle != triangles[i])
				{
					g_pKZUtils->AddTriangleOverlay(v0, v1, v2, 255, 0, 0, 128, true, -1.0f);
				}
				if (result.fraction < trace.m_flFraction)
				{
					META_CONPRINTF("  Overriding triangle %i with %i\n", trace.m_nTriangle, triangles[i]);
					trace.m_flFraction = result.fraction;
					trace.m_vHitNormal = result.plane.normal;
					trace.m_vEndPos = result.endpos;
					trace.m_nTriangle = triangles[i];
				}
			}
		}
	}
	return true;
}
