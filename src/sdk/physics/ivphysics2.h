#pragma once
#include "common.h"
#include "utils/virtual.h"
#include "utils/gameconfig.h"
#include "sdk/physics/types.h"

class CVPhys2World;

extern CGameConfig *g_pGameConfig;

class IVPhysics2 : public IAppSystem
{
private:
	virtual ~IVPhysics2() = 0;
	virtual CVPhys2World *CreateWorld(uint) = 0;
	virtual void Destroy(CVPhys2World *) = 0;

public:
	virtual int NumWorlds(void) = 0;
	virtual CVPhys2World *GetWorld(int numWorld) = 0;
	virtual void DumpState() = 0;
};

class CVPhys2World
{
private:
	void **vtable1;
	void **vtable2;

public:
	int worldFlags;

	void CastBoxMultiple(CUtlVectorFixedGrowable<PhysicsTrace_t, 128> *result, const Ray_t *ray, const Vector *start, const Vector *extent,
						 const RnQueryAttr_t *filter)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("CastBoxMultiple"), this, result, ray, start, extent, filter);
	}
};
