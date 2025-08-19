#include "meshtrace.h"
#include "utils/utils.h"
#include "utils/interfaces.h"
#include "sdk/entity/cbasemodelentity.h"
#include "sdk/cskeletoninstance.h"
#include "entity2/entityclass.h"

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

bool RetraceHull(const Ray_t &ray, const Vector &start, const Vector &end, CTraceFilter &filter, CGameTrace *trace)
{
	Vector to = end - start;

	
	if (trace->m_nTriangle != -1)
	{
		META_CONPRINTF("  Original tri index %i\n  hit offset %f hit point %.2f %.2f %.2f\n  fraction %f endpos %.2f %.2f %.2f end %.2f %.2f %.2f\n",
					   trace->m_nTriangle,
					   trace->m_flHitOffset,
					   trace->m_vHitPoint.x,
					   trace->m_vHitPoint.y,
					   trace->m_vHitPoint.z,
					   trace->m_flFraction,
					   trace->m_vEndPos.x,
					   trace->m_vEndPos.y,
					   trace->m_vEndPos.z,
					   end.x,
					   end.y,
					   end.z);
	}
	f32 epsilon = kz_surface_clip_epsilon.Get();
	// overtrace a little bit to make sure we don't miss any triangles!
	Ray_t::Hull_t hull = ray.m_Hull;
	Vector hullOffset = (hull.m_vMaxs + hull.m_vMins) * 0.5f;
	Vector extents = (hull.m_vMaxs - hull.m_vMins) * 0.5f;
	extents += epsilon * 200.0f;
	Vector start2 = start + hullOffset;
	CUtlVectorFixedGrowable<PhysicsTrace_t, 128> results;
	g_pKZUtils->CastBoxMultiple(&results, &start2, &to, &extents, &filter);
	Vector finalv0, finalv1, finalv2;
	
	{
		Ray_t::Hull_t sweptAabb;
		ComputeSweptAABB(ray.m_Hull.m_vMins, ray.m_Hull.m_vMaxs, to, epsilon, sweptAabb.m_vMins, sweptAabb.m_vMaxs);
		Ray_t queryRay;
		queryRay.Init(sweptAabb.m_vMins, sweptAabb.m_vMaxs);
		
		CUtlVectorFixedGrowable<HPhysicsShape, 128> shapes;
		//g_pKZUtils->IVPhys2World_Query(shapes, queryRay, start, &filter);
	}
	
	TraceResult_t finalBrushTrace = {};
	CGameTrace finalTrace = CGameTrace();
	finalBrushTrace.fraction = 1;
	PhysicsTrace_t *finalPhysTrace = nullptr;
	
#if 0
	FOR_EACH_VEC(results, i)
	{
		PhysicsTrace_t *physTrace = &results[i];
		PhysicsShapeType_t shapeType = physTrace->m_HitShape->GetShapeType();
		if (shapeType != SHAPE_MESH)
		{
			continue;
		}
		META_CONPRINTF("  Tri index %i\n", physTrace->m_nTriangle);
		CTransform transform;
		g_pKZUtils->GetPhysicsBodyTransform(physTrace->m_HitObject, transform);
		RnMesh_t *mesh = physTrace->m_HitShape->GetMesh();
		if (mesh->m_Nodes.Count() > 0)
		{
			// const RnTriangle_t &triangle = mesh->m_Triangles[triangles[i]];
			const RnTriangle_t &triangle = mesh->m_Triangles[physTrace->m_nTriangle];
			Vector v0 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[0]]);
			Vector v1 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[1]]);
			Vector v2 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[2]]);

			// Reconstruct the ray
			TraceRay_t traceRay;
			traceRay.start = start;
			traceRay.end = end;
			traceRay.size.m_vMinBounds = ray.m_Hull.m_vMins;
			traceRay.size.m_vMaxBounds = ray.m_Hull.m_vMaxs;

			Triangle_t tri(v0, v1, v2);
			Brush_t triangleBrush = GenerateTriangleAabbBevelPlanes(tri);

			// Do our own trace calculation
			TraceResult_t brushTrace = TraceThroughBrush(traceRay, triangleBrush);

			g_pKZUtils->AddTriangleOverlay(v0, v1, v2, 255, 0, 0, 32, true, -1.0f);
			if (brushTrace.fraction < finalBrushTrace.fraction)
			{
				finalBrushTrace = brushTrace;
				finalPhysTrace = physTrace;
				META_CONPRINTF("  Overriding triangle %i with %i\n", trace->m_nTriangle, physTrace->m_nTriangle);
				META_CONPRINTF("  Triangle %i: normal %f %f %f, endpos %f %f %f, fraction %f\n", physTrace->m_nTriangle, brushTrace.plane.normal.x,
							   brushTrace.plane.normal.y, brushTrace.plane.normal.z, brushTrace.endpos.x, brushTrace.endpos.y, brushTrace.endpos.z,
							   brushTrace.fraction);
				finalv0 = v0;
				finalv1 = v1;
				finalv2 = v2;
			}
		}
	}
#endif
	
	EntityInstanceIter_t iter;
	for (CEntityInstance *pEnt = iter.First(); pEnt; pEnt = iter.Next())
	{
		if (!pEnt || !pEnt->m_pEntity || !pEnt->m_pEntity->m_pClass)
		{
			continue;
		}
		if (filter.ShouldHitEntity(pEnt))
		{
			continue;
		}
		//pEnt->m_pEntity->m_pClass;
		auto classInfo = pEnt->m_pEntity->m_pClass->m_pClassInfo;
		bool found = false;
		while (classInfo)
		{
			if (KZ_STREQ("CBaseModelEntity", classInfo->m_pszCPPClassname))
			{
				found = true;
				break;
			}
			classInfo = classInfo->m_pBaseClassInfo;
		}
		if (!found)
		{
			continue;
		}
		auto schemaClassInfo = pEnt->m_pEntity->m_pClass->GetSchemaBinding();
		if (!schemaClassInfo)
		{
			continue;
		}
		META_CONPRINTF("class %s", pEnt->m_pEntity->m_pClass->m_pClassInfo->m_pszCPPClassname);
		for (i32 i = 0; i < schemaClassInfo->m_nBaseClassCount; i++)
		{
			META_CONPRINTF(": %s", schemaClassInfo->m_pBaseClasses[i].m_pClass->m_pszName);
		}
		META_CONPRINTF("\n");
		i32 i = 0;
		auto baseClass = schemaClassInfo;
		
		auto ent = static_cast<CBaseModelEntity *>(pEnt);
		auto bodyComponent = ent->m_CBodyComponent();
		auto sceneNode = bodyComponent ? bodyComponent->m_pSceneNode() : nullptr;
		CSkeletonInstance *pSkeleton = sceneNode ? sceneNode->GetSkeletonInstance() : nullptr;
		auto *modelstate = pSkeleton ? &pSkeleton->m_modelState() : nullptr;
		CPhysAggregateInstance *pPhysInstance = modelstate ? (CPhysAggregateInstance *)modelstate->m_pVPhysicsAggregate() : nullptr;
		CPhysAggregateData *aggregateData = pPhysInstance ? pPhysInstance->aggregateData : nullptr;
		if (!aggregateData)
		{
			continue;
		}
		
		FOR_EACH_VEC(aggregateData->m_Parts, i)
		{
			const VPhysXBodyPart_t *part = aggregateData->m_Parts[i];
			if (!part)
			{
				continue;
			}
			// TODO: HULL COLLISION
#if 0
			FOR_EACH_VEC(part->m_rnShape.m_hulls, j)
			{
				const RnHullDesc_t &hull = part->m_rnShape.m_hulls[j];
				const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[hull.m_nCollisionAttributeIndex];
				
				CTransform transform;
				transform.SetToIdentity();
				transform.m_vPosition = ent->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
				transform.m_orientation = Quaternion(ent->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
				Ray_t ray;
				//ray.Init(hull.m_Hull.m_Bounds.m_vMinBounds, hull.m_Hull.m_Bounds.m_vMaxBounds, hull.m_Hull.m_VertexPositions.Base(),
						 //hull.m_Hull.m_Vertices.Count());
			}
#endif
			FOR_EACH_VEC(part->m_rnShape.m_meshes, j)
			{
				const RnMeshDesc_t &mesh = part->m_rnShape.m_meshes[j];
				const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[mesh.m_nCollisionAttributeIndex];
				auto surfaceProps = aggregateData->m_SurfaceProperties[mesh.m_nSurfacePropertyIndex];
				CTransform transform;
				transform.SetToIdentity();
				transform.m_vPosition = ent->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
				transform.m_orientation = Quaternion(ent->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
				FOR_EACH_VEC(mesh.m_Mesh.m_Triangles, k)
				{
					const RnTriangle_t &triangle = mesh.m_Mesh.m_Triangles[k];
					Triangle_t tri(utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[0]]),
						utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[1]]),
									  utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[2]]));
					//g_pKZUtils->AddTriangleOverlay(tri[0], tri[1], tri[2], triggerColor.r(), triggerColor.g(), triggerColor.b(), triggerColor.a(),
					//false, -1.0f);
					
					// Reconstruct the ray
					TraceRay_t traceRay;
					traceRay.start = start;
					traceRay.end = end;
					traceRay.size.m_vMinBounds = ray.m_Hull.m_vMins;
					traceRay.size.m_vMaxBounds = ray.m_Hull.m_vMaxs;
					
					Brush_t triangleBrush = GenerateTriangleAabbBevelPlanes(tri);
					
					// Do our own trace calculation
					TraceResult_t brushTrace = TraceThroughBrush(traceRay, triangleBrush);
					
					g_pKZUtils->AddTriangleOverlay(tri.verts[0], tri.verts[1], tri.verts[2], 255, 0, 0, 32, true, -1.0f);
					if (brushTrace.fraction < finalBrushTrace.fraction)
					{
						finalBrushTrace = brushTrace;
						//finalPhysTrace = physTrace;
						//META_CONPRINTF("  Overriding triangle %i with %i\n", trace->m_nTriangle, physTrace->m_nTriangle);
						//META_CONPRINTF("  Triangle %i: normal %f %f %f, endpos %f %f %f, fraction %f\n", physTrace->m_nTriangle, brushTrace.plane.normal.x,
									   //brushTrace.plane.normal.y, brushTrace.plane.normal.z, brushTrace.endpos.x, brushTrace.endpos.y, brushTrace.endpos.z,
									   //brushTrace.fraction);
						finalv0 = tri.verts[0];
						finalv1 = tri.verts[1];
						finalv2 = tri.verts[2];
						
						// we hit something!
						finalTrace.m_pSurfaceProperties = surfaceProps;
						finalTrace.m_pEnt = pEnt;
						//finalTrace.m_pHitbox = ;
						//finalTrace.m_hBody = finalPhysTrace->m_HitObject;
						//finalTrace.m_hShape = finalPhysTrace->m_HitShape;
						finalTrace.m_BodyTransform = transform; // TODO: this right?
						finalTrace.m_ShapeAttributes = collisionAttr;
						finalTrace.m_ShapeAttributes.m_nEntityId = pEnt->m_pEntity->m_EHandle.ToInt();
						finalTrace.m_nContents = finalTrace.m_ShapeAttributes.m_nInteractsAs;
						finalTrace.m_vStartPos = start;
						finalTrace.m_vEndPos = finalBrushTrace.endpos;
						finalTrace.m_vHitNormal = finalBrushTrace.plane.normal;
						finalTrace.m_vHitPoint = finalBrushTrace.endpos + hullOffset;
						finalTrace.m_flHitOffset = finalBrushTrace.plane.distance;
						finalTrace.m_flFraction = finalBrushTrace.fraction;
						finalTrace.m_nTriangle = k; // TODO: this right?
						finalTrace.m_eRayType = RAY_TYPE_HULL;
						finalTrace.m_bStartInSolid = finalBrushTrace.startsolid;
						finalTrace.m_bExactHitPoint = false;
					}
				}
			}
		}
	}
	
	// didn't hit, or didn't move
	if (finalBrushTrace.fraction >= 1) // || (!finalBrushTrace.startsolid && finalBrushTrace.endpos.x == start.x && finalBrushTrace.endpos.y == start.y && finalBrushTrace.endpos.z == start.z))
	{
#if 0
		// save and restore hitbox and surface props, because we don't have a default ones right now
		//auto surfaceProps = trace->m_pSurfaceProperties;
		//auto hitbox = trace->m_pHitbox;
		// reset trace result
		//trace->Init();
		
		//trace->m_pSurfaceProperties = NULL;
		//trace->m_pEnt = NULL;
		//trace->m_pHitbox = NULL;
		trace->m_hBody = NULL;
		trace->m_hShape = NULL;
		//trace->m_nContents = 0;
		trace->m_BodyTransform.SetToIdentity();
		//trace->m_ShapeAttributes = {}; ?
		trace->m_vHitNormal.Init();
		trace->m_vHitPoint.Init();
		trace->m_flHitOffset = 0.0f;
		trace->m_flFraction = 1.0f;
		trace->m_nTriangle = -1;
		trace->m_nHitboxBoneIndex = -1;
		trace->m_eRayType = RAY_TYPE_LINE;
		trace->m_bStartInSolid = false;
		trace->m_bExactHitPoint = false;
		
		trace->m_vStartPos = start;
		trace->m_vEndPos = end;
		
		//trace->m_pSurfaceProperties = surfaceProps;
		//trace->m_pHitbox = hitbox;
#else
		*trace = finalTrace;
#endif
	}
	else
	{
#if 0
		// we hit something!
		trace->m_pSurfaceProperties = finalPhysTrace->m_pSurfaceProperties;
		trace->m_pEnt = g_pKZUtils->PhysicsBodyToEntity(finalPhysTrace->m_HitObject);
		//trace->m_pHitbox = ;
		trace->m_hBody = finalPhysTrace->m_HitObject;
		trace->m_hShape = finalPhysTrace->m_HitShape;
		g_pKZUtils->GetPhysicsBodyTransform(finalPhysTrace->m_HitObject, trace->m_BodyTransform);
		trace->m_ShapeAttributes = *trace->m_hShape->GetCollisionAttr();
		trace->m_ShapeAttributes.m_nEntityId = g_pKZUtils->PhysicsBodyToEntityHandle(finalPhysTrace->m_HitObject).ToInt();
		trace->m_nContents = trace->m_ShapeAttributes.m_nInteractsAs;
		trace->m_vStartPos = start;
		trace->m_vEndPos = finalBrushTrace.endpos;
		trace->m_vHitNormal = finalBrushTrace.plane.normal;
		trace->m_vHitPoint = finalBrushTrace.endpos + hullOffset;
		trace->m_flHitOffset = finalBrushTrace.plane.distance;
		trace->m_flFraction = finalBrushTrace.fraction;
		trace->m_nTriangle = finalPhysTrace->m_nTriangle;
		trace->m_eRayType = RAY_TYPE_HULL;
		trace->m_bStartInSolid = finalBrushTrace.startsolid;
		trace->m_bExactHitPoint = false;
#else
		*trace = finalTrace;
#endif
		// we hit something
		g_pKZUtils->AddTriangleOverlay(finalv0, finalv1, finalv2, 0, 0, 255, 32, true, -1.0f);
	}
	
	return trace->m_flFraction < 1;
}
