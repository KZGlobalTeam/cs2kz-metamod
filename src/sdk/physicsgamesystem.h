#pragma once
#include "gametrace.h"
#include "tier1/utlmap.h"
#include "mathlib/transform.h"
#include "tier1/utlvector.h"
#include "tier1/utlstring.h"

typedef CUtlVector<Vector> CUtlVectorSIMDPaddedVector;

template<typename T>
struct SphereBase_t
{
	Vector m_vCenter;
	T m_flRadius;
};

struct alignas(8) RnSphereDesc_t;
struct alignas(8) RnCapsuleDesc_t;
struct alignas(8) RnHullDesc_t;
struct alignas(8) RnMeshDesc_t;

struct alignas(8) VPhysics2ShapeDef_t
{
public:
	CUtlVector<RnSphereDesc_t> m_spheres;
	CUtlVector<RnCapsuleDesc_t> m_capsules;
	CUtlVector<RnHullDesc_t> m_hulls;
	CUtlVector<RnMeshDesc_t> m_meshes;
	CUtlVector<uint16> m_CollisionAttributeIndices;
};

struct alignas(8) VPhysXBodyPart_t
{
	enum VPhysXFlagEnum_t : uint32
	{
		FLAG_STATIC = 0x1,
		FLAG_KINEMATIC = 0x2,
		FLAG_JOINT = 0x4,
		FLAG_MASS = 0x8,
	};

	VPhysXFlagEnum_t m_nFlags;
	float32 m_flMass;
	VPhysics2ShapeDef_t m_rnShape;
	uint16 m_nCollisionAttributeIndex;
	uint16 m_nReserved;
	float32 m_flInertiaScale;
	float32 m_flLinearDamping;
	float32 m_flAngularDamping;
	bool m_bOverrideMassCenter;
	Vector m_vMassCenterOverride;
};

struct alignas(8) RnShapeDesc_t
{
public:
	uint32 m_nCollisionAttributeIndex;
	uint32 m_nSurfacePropertyIndex;
	CUtlString m_UserFriendlyName;
	bool m_bUserFriendlyNameSealed;
	bool m_bUserFriendlyNameLong;
	uint32 m_nToolMaterialHash;
};

struct alignas(8) RnSphereDesc_t : public RnShapeDesc_t
{
public:
	SphereBase_t<float32> m_Sphere;
};

struct alignas(4) RnCapsule_t
{
public:
	Vector m_vCenter[2];
	float32 m_flRadius;
};

struct alignas(8) RnCapsuleDesc_t : public RnShapeDesc_t
{
public:
	RnCapsule_t m_Capsule;
};

struct alignas(4) AABB_t
{
public:
	Vector m_vMinBounds;
	Vector m_vMaxBounds;
};

struct alignas(1) RnVertex_t;
struct alignas(1) RnHalfEdge_t;
struct alignas(1) RnFace_t;
struct alignas(4) RnPlane_t;
struct alignas(8) CRegionSVM;

struct alignas(8) RnHull_t
{
public:
	Vector m_vCentroid;
	float32 m_flMaxAngularRadius;
	AABB_t m_Bounds;
	Vector m_vOrthographicAreas;
	matrix3x4_t m_MassProperties;
	float32 m_flVolume;
	float32 m_flSurfaceArea;
	CUtlVector<RnVertex_t> m_Vertices;
	CUtlVector<Vector> m_VertexPositions;
	CUtlVector<RnHalfEdge_t> m_Edges;
	CUtlVector<RnFace_t> m_Faces;
	CUtlVector<RnPlane_t> m_FacePlanes;
	uint32 m_nFlags;
	CRegionSVM *m_pRegionSVM;
};

struct alignas(8) RnHullDesc_t : public RnShapeDesc_t
{
public:
	RnHull_t m_Hull;
};

struct alignas(16) RnNode_t;

struct alignas(4) RnTriangle_t;
struct alignas(4) RnWing_t;

struct alignas(8) RnMesh_t
{
public:
	Vector m_vMin;
	Vector m_vMax;
	CUtlVector<RnNode_t> m_Nodes;
	CUtlVectorSIMDPaddedVector m_Vertices;
	CUtlVector<RnTriangle_t> m_Triangles;
	CUtlVector<RnWing_t> m_Wings;
	CUtlVector<uint8> m_TriangleEdgeFlags;
	CUtlVector<uint8> m_Materials;
	Vector m_vOrthographicAreas;
	uint32 m_nFlags;
	uint32 m_nDebugFlags;
};

struct alignas(8) RnMeshDesc_t : public RnShapeDesc_t
{
public:
	RnMesh_t m_Mesh;
};

struct alignas(1) RnVertex_t
{
public:
	uint8 m_nEdge;
};

struct alignas(1) RnHalfEdge_t
{
public:
	uint8 m_nNext;
	uint8 m_nTwin;
	uint8 m_nOrigin;
	uint8 m_nFace;
};

struct alignas(4) RnPlane_t
{
public:
	Vector m_vNormal;
	float32 m_flOffset;
};

struct alignas(1) RnFace_t
{
public:
	uint8 m_nEdge;
};

struct alignas(8) CRegionSVM
{
public:
	CUtlVector<RnPlane_t> m_Planes;
	CUtlVector<uint32> m_Nodes;
};

struct alignas(16) RnNode_t
{
public:
	Vector m_vMin;
	uint32 m_nChildren;
	Vector m_vMax;
	uint32 m_nTriangleOffset;
};

struct alignas(4) RnTriangle_t
{
public:
	int32 m_nIndex[3];
};

struct alignas(4) RnWing_t
{
public:
	int32 m_nIndex[3];
};

struct alignas(4) QuaternionStorage
{
	uint8 pad_0000[16];
};

struct alignas(4) VPhysXConstraintParams_t
{
public:
	int8 m_nType;
	int8 m_nTranslateMotion;
	int8 m_nRotateMotion;
	int8 m_nFlags;
	Vector m_anchor[2];
	QuaternionStorage m_axes[2];
	float32 m_maxForce;
	float32 m_maxTorque;
	float32 m_linearLimitValue;
	float32 m_linearLimitRestitution;
	float32 m_linearLimitSpring;
	float32 m_linearLimitDamping;
	float32 m_twistLowLimitValue;
	float32 m_twistLowLimitRestitution;
	float32 m_twistLowLimitSpring;
	float32 m_twistLowLimitDamping;
	float32 m_twistHighLimitValue;
	float32 m_twistHighLimitRestitution;
	float32 m_twistHighLimitSpring;
	float32 m_twistHighLimitDamping;
	float32 m_swing1LimitValue;
	float32 m_swing1LimitRestitution;
	float32 m_swing1LimitSpring;
	float32 m_swing1LimitDamping;
	float32 m_swing2LimitValue;
	float32 m_swing2LimitRestitution;
	float32 m_swing2LimitSpring;
	float32 m_swing2LimitDamping;
	Vector m_goalPosition;
	QuaternionStorage m_goalOrientation;
	Vector m_goalAngularVelocity;
	float32 m_driveSpringX;
	float32 m_driveSpringY;
	float32 m_driveSpringZ;
	float32 m_driveDampingX;
	float32 m_driveDampingY;
	float32 m_driveDampingZ;
	float32 m_driveSpringTwist;
	float32 m_driveSpringSwing;
	float32 m_driveSpringSlerp;
	float32 m_driveDampingTwist;
	float32 m_driveDampingSwing;
	float32 m_driveDampingSlerp;
	int32 m_solverIterationCount;
	float32 m_projectionLinearTolerance;
	float32 m_projectionAngularTolerance;
};

struct alignas(4) VPhysXRange_t
{
public:
	float32 m_flMin;
	float32 m_flMax;
};

struct alignas(16) VPhysXJoint_t
{
public:
	uint16 m_nType;
	uint16 m_nBody1;
	uint16 m_nBody2;
	uint16 m_nFlags;
	CTransform m_Frame1;
	CTransform m_Frame2;
	bool m_bEnableCollision;
	bool m_bEnableLinearLimit;
	VPhysXRange_t m_LinearLimit;
	bool m_bEnableLinearMotor;
	Vector m_vLinearTargetVelocity;
	float32 m_flMaxForce;
	bool m_bEnableSwingLimit;
	VPhysXRange_t m_SwingLimit;
	bool m_bEnableTwistLimit;
	VPhysXRange_t m_TwistLimit;
	bool m_bEnableAngularMotor;
	Vector m_vAngularTargetVelocity;
	float32 m_flMaxTorque;
	float32 m_flLinearFrequency;
	float32 m_flLinearDampingRatio;
	float32 m_flAngularFrequency;
	float32 m_flAngularDampingRatio;
	float32 m_flFriction;
	float32 m_flElasticity;
	float32 m_flElasticDamping;
	float32 m_flPlasticity;
};

struct CPhysConstraintData
{
	uint32 m_nFlags;
	uint16 m_nParent;
	uint16 m_nChild;
	const VPhysXConstraintParams_t *m_pParams;
};

class PhysShapeMarkup_t
{
	int32 m_nBodyInAggregate;
	int32 m_nShapeInBody;
	uint64 m_sHitGroup; // CGlobalSymbol
};

/*
 * Tutorial on how to update this struct:
 Plan A:
 - Find CPhysicsGameSystem::NotifyAggregateInitialized with "Install updater " string
 - XRef it to the only function calling it, should be something like:
		v15 = Physics_CreateAggregate(a2, a3, a4, v14, a5, v17);
		*(a1 + 272) = v15;
		if ( v15 )
		{
		  CPhysicsGameSystem::NotifyAggregateInitialized(qword_17AB370, a2, v15);
  - Inside Physics_CreateAggregate, find Physics_GetAggregateData (the one with all arguments going into its parameters)
  - The first (only?) function having two arguments with the second being 1 is CPhysAggregateData::CPhysAggregateData
  Plan B:
  - Find function that has "shatter_mesh_%i.vmesh" string in it.
  - The first function having two arguments with the second being 1 is CPhysAggregateData::CPhysAggregateData
*/
struct CPhysAggregateData
{
	CInterlockedUInt m_nRefCounter;
	uint16 m_nFlags;
	int16 m_nIncrementalVectorIndex;
	int16 m_nReserved;
	CUtlVectorUltraConservative<char const *> m_BoneNames;
	CUtlMemory<uint32> m_BonesHash;
	CUtlMemory<uint16> m_IndexNames;
	CUtlMemory<uint16> m_IndexHash;
	CUtlMemory<uint16> m_BoneParents;
	CUtlMemory<matrix3x4_t, int> m_BindPose;
	CUtlMemory<PhysShapeMarkup_t> m_shapeMarkups;
	CUtlVectorConservative<const VPhysXBodyPart_t *> m_Parts;
	CUtlVectorUltraConservative<CPhysConstraintData> m_Constraints;
	CUtlVectorUltraConservative<const VPhysXJoint_t *> m_Joints;
	void *m_pFeModel;
	CUtlVectorUltraConservative<RnCollisionAttr_t> m_CollisionAttributes;
	CUtlVectorUltraConservative<const CPhysSurfaceProperties *> m_SurfaceProperties;
	CUtlVectorUltraConservative<const CPhysAggregateData *> m_ParentResources;
	const void *m_pDebugPartNames;
	const char *m_pEmbeddedKeyvalues;
};

class CPhysAggregateInstance
{
	uint8_t unknown[0x18];
	void *physicsWorld;

public:
	CPhysAggregateData *aggregateData;
};

class CPhysicsGameSystem
{
public:
	struct PhysicsSpawnGroups_t
	{
		uint8_t unknown[0x10];
		CPhysAggregateInstance *m_pLevelAggregateInstance;
		uint8_t unknown2[0x58];
	};

	uint8_t unk[0x78];
	CUtlMap<uint, PhysicsSpawnGroups_t> m_PhysicsSpawnGroups;
};
