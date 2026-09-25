#pragma once
#include "cbaseentity.h"
#include "ccsplayerpawn.h"
#include "sdk/services.h"
#include "utils/utils.h"
#include "entity2/entitykeyvalues.h"
#ifndef IDA_IGNORE

enum CustomCameraMode_t : uint8
{
	// Position and angles come from the eye position and angles of the player.
	CUSTOM_CAMERA_MODE_DISABLED = 0,
	// Position and angles come from the origin and angles of the camera entity.
	CUSTOM_CAMERA_MODE_CONTROLLED = 1,
	// Position comes from the origin of the camera entity. Angles are player controlled.
	CUSTOM_CAMERA_MODE_CONTROLLED_POSITION = 2,
	// Position comes from an offset around a followed position. Angles are player controlled.
	CUSTOM_CAMERA_MODE_FOLLOW_POSITION = 3,
};

class CCSCustomPlayerCamera : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSCustomPlayerCamera)

	SCHEMA_FIELD(CHandle<CCSPlayerPawnBase>, m_hPawn)
	SCHEMA_FIELD(CustomCameraMode_t, m_nCameraMode)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hFollowEntity)
	SCHEMA_FIELD(bool, m_bFollowEyes)
	SCHEMA_FIELD(Vector, m_vecFollowOffset)
	SCHEMA_FIELD(Vector, m_vecCameraOffset)
	SCHEMA_FIELD(bool, m_bClipCameraOffset)
	SCHEMA_FIELD(float, m_flCameraOffsetReturnStrength)

	// The camera whose m_hPawn is this pawn, if one was spawned.
	static CCSCustomPlayerCamera *Find(CBasePlayerPawn *pawn)
	{
		if (!pawn || !GameEntitySystem())
		{
			return NULL;
		}
		EntityInstanceByClassIter_t iter(NULL, "custom_player_camera");
		for (CEntityInstance *ent = iter.First(); ent; ent = iter.Next())
		{
			CCSCustomPlayerCamera *camera = static_cast<CCSCustomPlayerCamera *>(ent);
			if (camera->m_hPawn().Get() == pawn)
			{
				return camera;
			}
		}
		return NULL;
	}

	// CSPlayerPawn.GetCustomCamera(): at most one camera per pawn, spawned on demand.
	static CCSCustomPlayerCamera *GetOrCreate(CCSPlayerPawn *pawn)
	{
		if (CCSCustomPlayerCamera *existing = Find(pawn))
		{
			return existing;
		}
		return Create(pawn);
	}

	// The spawning half of GetOrCreate: at the pawn, with no owner.
	static CCSCustomPlayerCamera *Create(CCSPlayerPawn *pawn)
	{
		if (!pawn)
		{
			return NULL;
		}
		CCSCustomPlayerCamera *camera = utils::CreateEntityByName<CCSCustomPlayerCamera>("custom_player_camera");
		if (!camera)
		{
			return NULL;
		}
		CEntityKeyValues *keyValues = new CEntityKeyValues();
		keyValues->SetVector("origin", pawn->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin());
		keyValues->SetQAngle("angles", pawn->m_angEyeAngles());
		camera->DispatchSpawn(keyValues);
		// The game links the pawn after spawning.
		if (camera->m_hPawn().Get() != pawn)
		{
			camera->m_hPawn(pawn->GetRefEHandle());
		}
		return camera;
	}

	// Any mode but DISABLED makes the camera the pawn's view entity; DISABLED only lets go of it while
	// it is still this camera, so it never clears a view entity someone else set.
	void SetMode(CustomCameraMode_t mode)
	{
		if (m_nCameraMode() != mode)
		{
			m_nCameraMode(mode);
		}
		CCSPlayerPawnBase *pawn = m_hPawn().Get();
		CPlayer_CameraServices *cameraServices = pawn ? pawn->m_pCameraServices() : NULL;
		if (!cameraServices)
		{
			return;
		}
		CBaseEntity *viewEntity = cameraServices->m_hViewEntity().Get();
		if (mode != CUSTOM_CAMERA_MODE_DISABLED)
		{
			if (viewEntity != this)
			{
				cameraServices->m_hViewEntity(GetRefEHandle());
			}
		}
		else if (viewEntity == this)
		{
			cameraServices->m_hViewEntity(CHandle<CBaseEntity>());
		}
	}

	// CustomPlayerCamera.SetFollowConfig(). The game then also recomputes the clipped offset right away;
	// here that waits for the entity's next think.
	void SetFollowConfig(CBaseEntity *followEntity, bool followEyes = false, const Vector &followOffset = vec3_origin,
						 const Vector &cameraOffset = vec3_origin, bool clipCameraOffset = false, float cameraOffsetReturnStrength = 1.0f)
	{
		if (m_hFollowEntity().Get() != followEntity)
		{
			m_hFollowEntity(followEntity ? CHandle<CBaseEntity>(followEntity->GetRefEHandle()) : CHandle<CBaseEntity>());
		}
		if (m_bFollowEyes() != followEyes)
		{
			m_bFollowEyes(followEyes);
		}
		if (m_vecFollowOffset() != followOffset)
		{
			m_vecFollowOffset(followOffset);
		}
		if (m_vecCameraOffset() != cameraOffset)
		{
			m_vecCameraOffset(cameraOffset);
		}
		if (m_bClipCameraOffset() != clipCameraOffset)
		{
			m_bClipCameraOffset(clipCameraOffset);
		}
		const float strength = cameraOffsetReturnStrength >= 0.0f ? fminf(1.0f, cameraOffsetReturnStrength) : 0.0f;
		if (m_flCameraOffsetReturnStrength() != strength)
		{
			m_flCameraOffsetReturnStrength(strength);
		}
	}
};

#endif
