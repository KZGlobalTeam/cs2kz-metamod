#pragma once
#include "common.h"

#include "sdk/datatypes.h"
#include "sdk/physics/gamesystem.h"

struct Edge_t
{
	Vector verts[2];

	Edge_t()
	{
		verts[0].Init();
		verts[1].Init();
	}

	Edge_t(const Vector &v0, const Vector &v1)
	{
		verts[0] = v0;
		verts[1] = v1;
	}
};

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
	f32 distance = 0.0f;

	Plane_t()
	{
		normal.Init();
	}

	Plane_t(const Vector &n, f32 d)
	{
		normal = n;
		distance = d;
	}
};

// max planes is 17
// 2 planes for the 2 faces of the triangle
// 6 planes for the 6 faces of the aabb
// 3*3 planes for the +x,+y,+z,-x,-y,-z projections of the 3 edges of the triangle
#define AABB_TRI_MINKOWSKI_MAX_PLANES 17

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
	Vector endpos;   // final position
	Plane_t plane;   // surface normal at impact, transformed to world space
};

// https://github.com/id-Software/Quake-III-Arena/blob/dbe4ddb10315479fc00086f08e25d968b4b43c49/code/qcommon/cm_local.h#L154
struct TraceRay_t
{
	Vector start;
	Vector end;
	AABB_t size; // size of the box being swept through the model
				 // Vector offsets[8];  // [signbits][x] = either size[0][x] or size[1][x]
				 // f32 maxOffset;  // longest corner length from origin
				 // Vector extents;     // greatest of abs(size[0]) and abs(size[1])
				 // Vector bounds[2];   // enclosing box of start and end surrounding by size
				 // Vector modelOrigin; // origin of the model tracing through
				 // bool isPoint;   // optimized case
};
