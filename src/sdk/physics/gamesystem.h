#pragma once
#include "types.h"

class AABB_t;

class CPhysAggregateInstance
{
	// virtual ~CPhysAggregateInstance() = 0;                                                                      // 0
	// virtual int GetNumBodies() = 0;                                                                             // 1
	// virtual void *GetBodyPart(int index) = 0;                                                                   // 2
	// virtual void *GetBodyPart(int index) const = 0;                                                             // 3
	// virtual void *GetWorld() = 0;                                                                               // 4
	// virtual void unk5() = 0;                                                                                    // 5
	// virtual void unk6() = 0;                                                                                    // 6
	// virtual void unk7() = 0;                                                                                    // 7
	// virtual void unk8() = 0;                                                                                    // 8
	// virtual void Clear() = 0;                                                                                   // 9
	// virtual void unk10() = 0;                                                                                   // 10
	// virtual void GetOrigin(void) = 0;                                                                           // 11
	// virtual void unk12() = 0;                                                                                   // 12
	// virtual void unk13() = 0;                                                                                   // 13
	// virtual void unk14() = 0;                                                                                   // 14
	// virtual void unk15() = 0;                                                                                   // 15
	// virtual void unk16() = 0;                                                                                   // 16
	// virtual float GetScale() = 0;                                                                               // 17
	// virtual void Rescale(float scale) = 0;                                                                      // 18
	// virtual void GetBbox(AABB_t *) const = 0;                                                                   // 19
	// virtual void GetBboxFromProxies(AABB_t *) const = 0;                                                        // 20
	// virtual void GetBboxAfterRootTransform(CTransform const &, AABB_t *) const = 0;                             // 21
	// virtual void *GetBody(uint) = 0;                                                                            // 22
	// virtual void *GetBody(uint) const = 0;                                                                      // 23
	// virtual int GetBodyIndex(void*) = 0;                                                                        // 24
	// virtual void GetBoneNameFromIndex(int) = 0;                                                                 // 25
	// virtual void GetBoneHashFromIndex(int) = 0;                                                                 // 26
	// virtual void unk27() = 0;                                                                                   // 27
	// virtual void unk28() = 0;                                                                                   // 28
	// virtual void SetCollisionAttributes(RnCollisionAttr_t const&) = 0;                                          // 29
	// virtual void SetSurfaceProperties(CUtlStringToken) = 0;                                                     // 30
	// virtual void unk31() = 0;                                                                                   // 31
	// virtual void unk32() = 0;                                                                                   // 32
	// virtual void GetTouchingEntities(CUtlVector<uint>&) = 0;                                                    // 33
	// virtual int GetJointCount() = 0;                                                                            // 34
	// virtual void* GetJoint(uint) = 0;                                                                           // 35
	// virtual void* GetJoint(uint) const = 0;                                                                     // 36
	// virtual CTransform* GetTransform() = 0;                                                                     // 37
	// virtual void unk38() = 0;                                                                                   // 38
	// virtual void unk39() = 0;                                                                                   // 39
	// virtual void GetTotalMass(void) const = 0;                                                                  // 40
	// virtual void SetTotalMass(float) const = 0;                                                                 // 41
	// virtual void unk40() = 0;                                                                                   // 42
	// virtual void unk41() = 0;                                                                                   // 43
	// virtual void unk42() = 0;                                                                                   // 44
	// virtual void unk43() = 0;                                                                                   // 45
	// virtual void unk44() = 0;                                                                                   // 46
	// virtual void unk45() = 0;                                                                                   // 47
	// virtual void unk46() = 0;                                                                                   // 48
	// virtual void unk47() = 0;                                                                                   // 49
	// virtual void unk48() = 0;                                                                                   // 50
	// virtual void unk49() = 0;                                                                                   // 51
	// virtual void unk50() = 0;                                                                                   // 52
	// virtual void unk51() = 0;                                                                                   // 53
	// virtual void unk52() = 0;                                                                                   // 54
	// virtual void unk53() = 0;                                                                                   // 55
	// virtual void unk54() = 0;                                                                                   // 56
	// virtual void SweepShape(CTransform const &, CTransform const &, AABB_t const &, RnQueryAttr_t const &) = 0; // 57
	// virtual void unk56() = 0;                                                                                   // 58
	// virtual void unk57() = 0;                                                                                   // 59
	// virtual void unk58() = 0;                                                                                   // 60
	// virtual void unk59() = 0;                                                                                   // 61
	// virtual void unk60() = 0;                                                                                   // 62
	// virtual void unk61() = 0;                                                                                   // 63
	// virtual void unk62() = 0;                                                                                   // 64
	// virtual void CheckShapeOverlap(CTransform const &, RnHull_t const &, RnQueryAttr_t const &) = 0;            // 65
	// virtual void retzero() = 0;                                                                                 // 66
	// virtual void CheckOverlap(IPhysAggregateInstance const*) = 0;                                               // 67
	// virtual float DistanceToPoint(Vector const&) = 0;                                                           // 68
	// virtual float DistanceToAggregate(IPhysAggregateInstance const*) = 0;                                       // 69
	// virtual void IsAttachedToBone() = 0;                                                                        // 70
	// virtual void SetEntityHandle(ulong long) = 0;                                                               // 71
	// virtual void unk70() = 0;                                                                                   // 72
	// virtual void SetDebugName(char const *) = 0;                                                                // 73
	// virtual void AppendDebugInfo(CUtlString &) const = 0;                                                       // 74

	uint8 unknown[0x18];
	void *physicsWorld;

public:
	CPhysAggregateData *aggregateData;
	float scale;
	int instanceIndex;
	bool unk30; // Set to false with unk12, true with unk13. If true, make unk14/15 do nothing.
	CUtlVector<void *> physicsBodies;
	CUtlVector<void *> physicsJoints;
	CTransform transform;
	CEntityHandle handle;
};

static_assert(offsetof(CPhysAggregateInstance, handle) == 0x90, "CPhysAggregateInstance handle offset mismatch");
static_assert(offsetof(CPhysAggregateInstance, physicsBodies) == 0x38, "CPhysAggregateInstance physicsBodies offset mismatch");
static_assert(sizeof(CPhysAggregateInstance) == 0xA0, "CPhysAggregateInstance size mismatch");

class CPhysicsGameSystem
{
public:
	struct PhysicsSpawnGroups_t
	{
		uint8 unknown[0x10];
		CPhysAggregateInstance *m_pLevelAggregateInstance;
		uint8 unknown2[0x58];
	};

	uint8 unk[0x78];
	CUtlMap<uint, PhysicsSpawnGroups_t> m_PhysicsSpawnGroups;
};
