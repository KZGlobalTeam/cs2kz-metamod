#pragma once
#include "common.h"
#include "utils/virtual.h"
#include "utils/gameconfig.h"
#include "sdk/physics/types.h"

class CVPhys2World;

extern CGameConfig *g_pGameConfig;

class IVPhysics2 : public IAppSystem
{
	virtual ~IVPhysics2() = 0;
	virtual CVPhys2World *CreateWorld(uint) = 0;
	virtual void Destroy(CVPhys2World *) = 0;

public:
	virtual int NumWorlds(void) = 0;
	virtual CVPhys2World *GetWorld(int numWorld) = 0;

private:
	virtual void unk_016() = 0; // used by server
	virtual void unk_017() = 0; // used by server
	virtual void unk_018() = 0;

public:
	virtual bool IsMultiThreaded(void) = 0;
	virtual void SetMultiThreaded(bool) = 0; // used by server
	virtual bool IsSimdEnabled(void) = 0;
	virtual void SetSimdEnabled(bool) = 0;
	virtual void *GetSurfacePropertyController(void) const = 0; // used by server
	virtual void *GetIntersectionDictionary(void) = 0;          // used by server
	virtual void *GetIntersectionDictionaryInternal(void) = 0;  // used by server

private:
	virtual void unk_026() = 0;

public:
	virtual HPhysicsShape CreateSphereShape(const Vector &, float) = 0;
	virtual HPhysicsShape CreateCapsuleShape(const Vector &, const Vector &, float) = 0;
	virtual HPhysicsShape CreateHullShape(const RnHull_t *) = 0;
	virtual HPhysicsShape CreateMeshShape(const RnMesh_t *) = 0;
	virtual void DestroyShape(HPhysicsShape) = 0;

private:
	virtual void unk_032() = 0;
	virtual void unk_033() = 0;
	virtual void unk_034() = 0;
	virtual void unk_035() = 0;
	virtual void unk_036() = 0;
	virtual void unk_037() = 0;
	virtual void unk_038() = 0;

public:
	// Cast against certain ray_t with transform, return true if hit
	virtual bool GeneralCastAgainstRay(PhysicsTrace_t *, Ray_t *, Vector *, Vector *, Ray_t *, CTransform *) = 0; // used by server

private:
	virtual void unk_040() = 0;
	virtual void unk_041() = 0;
	virtual void unk_042() = 0;
	virtual void unk_043() = 0; // used by server
	virtual void unk_044() = 0;
	virtual void unk_045() = 0;
	virtual void unk_046() = 0;
	virtual void unk_047() = 0;
	// Use exported functions instead!
	virtual void *HullCreateBox() = 0; // used by server
	virtual void *HullCreateCone() = 0;
	virtual void *HullCylinder() = 0; // used by server

	virtual void HullCreateUnk() = 0;

	virtual void *HullCreate() = 0; // used by server
	virtual void *HullRuntime() = 0;
	virtual void *HullCreateRuntimeFromPlanes() = 0;
	virtual void *HullCreateRandom() = 0;
	virtual void *HullClone() = 0;    // used by server
	virtual void RnHullDestroy() = 0; // used by server

	virtual void MeshCreateUnk() = 0;
	virtual void MeshCreate(void /*CMesh*/ *, void /*CResourceStream*/ *, RnMesh_t *) = 0;
	virtual void MeshCreate(int, const uint *, unsigned char *, int, const Vector *, void /*CResourceStream*/ **, RnMesh_t *) = 0;
	virtual void MeshClone(RnMesh_t *, const RnMesh_t &, void /*CResourceStream*/ *) = 0; // used by server
	virtual void MeshDestroy(RnMesh_t *, void /*CResourceStream*/ *, bool) = 0;           // used by server
	virtual void *MeshUpdateChangedVertices(RnMesh_t *) = 0;

	virtual void unk_064() = 0;

	virtual void PhysDataTypeManagerReallocate() = 0;
	virtual void unk_066() = 0; // used by server
	virtual void unk_067() = 0; // used by server
	virtual void unk_068() = 0; // used by server
	virtual void unk_069() = 0;
	virtual void unk_070() = 0;
	virtual void unk_071() = 0;
	virtual void unk_072() = 0;
	virtual void unk_073() = 0;
	virtual void unk_074() = 0;
	virtual void unk_075() = 0;
	virtual void unk_076() = 0;
	virtual void unk_077() = 0;
	virtual void unk_078() = 0;
	virtual void unk_079() = 0;
	virtual void unk_080() = 0;
	virtual void unk_081() = 0;
	virtual void unk_082() = 0;
	virtual void unk_083() = 0; // used by server
	virtual void unk_084() = 0;
	virtual void unk_085() = 0;
	virtual void unk_086() = 0;
	virtual void unk_087() = 0; // used by server
	virtual void unk_088() = 0;
	virtual void unk_089() = 0;
	virtual void unk_090() = 0; // used by server
	virtual void unk_091() = 0; // used by server
	virtual void unk_092() = 0; // used by server
	virtual void unk_093() = 0; // used by server

public:
	virtual uint64 /*InteractionLayers_t*/ ParseInteractionLayerList(const char *) = 0; // used by server

private:
	virtual void *CreateDebugShape() = 0; // used by server
	// All these functions are just returning pointers to CFnPhysics* vtables.
	virtual void unk_096() = 0; // used by server
	virtual void unk_097() = 0; // used by server
	virtual void unk_098() = 0;
	virtual void unk_099() = 0; // used by server
	virtual void unk_100() = 0; // used by server
	virtual void unk_101() = 0; // used by server
	virtual void unk_102() = 0;
	virtual void unk_103() = 0;
	virtual void unk_104() = 0; // used by server
	virtual void unk_105() = 0; // used by server
	virtual void unk_106() = 0; // used by server
	virtual void unk_107() = 0; // used by server
	virtual void unk_108() = 0;
	virtual void unk_109() = 0;
	virtual void unk_110() = 0;
	virtual void unk_111() = 0; // used by server
	virtual void unk_112() = 0;
	virtual void unk_113() = 0; // used by server
	virtual void unk_114() = 0;

	virtual void StartConComm() = 0;
	virtual void UpdateConComm(void) = 0;
	virtual bool IsConCommActive(void) = 0;

	virtual void unk_118() = 0;
	virtual void unk_119() = 0;
	virtual void unk_120() = 0; // used by server
	virtual void unk_121() = 0; // returns false

	virtual void unk_122() = 0; // used by server
	virtual void unk_123() = 0; // used by server
	virtual void unk_124() = 0;

public:
	virtual void DumpState(CCommand *) = 0;

private:
	virtual void unk_126() = 0;

	virtual void *
		CreateDebugSceneObject(/* const DebugSceneObjectParams_t &, SceneObjectBuffers_t * */) = 0; // CSceneAnimatableObject * // used by server
	virtual void DestroyDebugSceneObject(/* CSceneAnimatableObject * */) = 0;

	virtual void unk_129() = 0;
	virtual void unk_130() = 0; // returns false
	virtual void unk_131() = 0;

public:
	virtual const char *GetWeightTypeStringFromWeight(float weight) = 0;
};

class CVPhys2World
{
private:
	void **vtable1;
	void **vtable2;

public:
	int worldFlags;

	void CastBoxMultiple(CUtlVectorFixedGrowable<PhysicsTrace_t, 128> *result, const Vector *start, const Vector *direction,
						 const Vector *bboxExtents, const RnQueryAttr_t *filter)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("CastBoxMultiple"), this, result, start, direction, bboxExtents, filter);
	}

	void Query(CUtlVector<HPhysicsShape> *result, const Ray_t *ray, const Vector *position, RnQueryAttr_t *filter, bool overlapTest)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("CVPhys2World::QueryBox"), this, result, ray, position, filter, overlapTest);
	}
};
