#include "kz_style.h"
#include "version_gen.h"

#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"
#include "sdk/navphysicsinterface.h"
#include "sdk/tracefilter.h"

#define STYLE_NAME       "AutoUnduck"
#define STYLE_NAME_SHORT "AUD"

class KZAutoUnduckStylePlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);

public:
	const char *GetAuthor()
	{
		return PLUGIN_AUTHOR;
	}

	const char *GetName()
	{
		return "CS2KZ-Style-AutoUnduck";
	}

	const char *GetDescription()
	{
		return "AutoUnduck style plugin for CS2KZ";
	}

	const char *GetURL()
	{
		return PLUGIN_URL;
	}

	const char *GetLicense()
	{
		return PLUGIN_LICENSE;
	}

	const char *GetVersion()
	{
		return PLUGIN_FULL_VERSION;
	}

	const char *GetDate()
	{
		return __DATE__;
	}

	const char *GetLogTag()
	{
		return PLUGIN_LOGTAG;
	}
} g_KZAutoUnduckStylePlugin;

class KZAutoUnduckStyleService : public KZStyleService
{
	using KZStyleService::KZStyleService;

public:
	virtual const char *GetStyleName() override
	{
		return STYLE_NAME;
	}

	virtual const char *GetStyleShortName() override
	{
		return STYLE_NAME_SHORT;
	}

	virtual void Cleanup() override;
	virtual void OnProcessMovement() override;

	float lastActualDuckTime = -1.0f;
	bool applyingUnduck = false;
	void UpdateDuckTime();
	void ApplyForcedUnduck();
	void CancelForcedUnduck();
};

CGameConfig *g_pGameConfig = NULL;
KZUtils *g_pKZUtils = NULL;
KZStyleManager *g_pStyleManager = NULL;
StyleServiceFactory g_StyleFactory = [](KZPlayer *player) -> KZStyleService * { return new KZAutoUnduckStyleService(player); };
PLUGIN_EXPOSE(KZAutoUnduckStylePlugin, g_KZAutoUnduckStylePlugin);

bool KZAutoUnduckStylePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pStyleManager = (KZStyleManager *)g_SMAPI->MetaFactory(KZ_STYLE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_STYLE_MANAGER_INTERFACE);
		return false;
	}
	g_pKZUtils = (KZUtils *)g_SMAPI->MetaFactory(KZ_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", KZ_UTILS_INTERFACE);
		return false;
	}
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		V_snprintf(error, maxlen, "Failed to initialize interfaces");
		return false;
	}

	if (nullptr == (g_pGameConfig = g_pKZUtils->GetGameConfig()))
	{
		V_snprintf(error, maxlen, "Failed to get game config");
		return false;
	}

	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		V_snprintf(error, maxlen, "Failed to register style");
		return false;
	}
	ConVar_Register();

	return true;
}

bool KZAutoUnduckStylePlugin::Unload(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZAutoUnduckStylePlugin::Pause(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool KZAutoUnduckStylePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		return false;
	}
	return true;
}

CGameEntitySystem *GameEntitySystem()
{
	return g_pKZUtils->GetGameEntitySystem();
}

void KZAutoUnduckStyleService::OnProcessMovement()
{
	this->UpdateDuckTime();
	CMoveData *mv = this->player->currentMoveData;
	// If already applying unduck and the player is still holding duck, keep forced unduck state
	if (this->applyingUnduck && this->player->GetMoveServices()->m_nButtons().GetButtonState(IN_DUCK) == IN_BUTTON_DOWN)
	{
		this->ApplyForcedUnduck();
		return;
	}
	if (!this->player->GetMoveServices()->m_bDucked() || this->player->GetMoveType() != MOVETYPE_WALK
		|| this->player->GetPlayerPawn()->m_fFlags & FL_ONGROUND)
	{
		if (this->applyingUnduck)
		{
			this->CancelForcedUnduck();
			this->applyingUnduck = false;
		}
		return;
	}
	CTraceFilterPlayerMovementCS filter(this->player->GetPlayerPawn());
	bbox_t bounds;
	bounds.mins = {-16.0, -16.0, 0.0};
	bounds.maxs = {16.0, 16.0, 54.0};

	trace_t trace;

	Vector ground = mv->m_vecAbsOrigin;
	ground.z -= 9.0f;
	INavPhysicsInterface::TraceShape(Ray_t(bounds.mins, bounds.maxs), mv->m_vecAbsOrigin, ground, &filter, &trace);

	if (trace.DidHit())
	{
		return;
	}

	f32 standableZ = 0.7;

	CConVarRef<float> sv_standable_normal("sv_standable_normal");
	if (sv_standable_normal.IsValidRef() && sv_standable_normal.IsConVarDataAvailable())
	{
		standableZ = sv_standable_normal.Get();
	}

	ground -= 2.0f;
	INavPhysicsInterface::TraceShape(Ray_t(bounds.mins, bounds.maxs), mv->m_vecAbsOrigin, ground, &filter, &trace);

	// Doesn't hit anything, fall back to the original ground
	if (!trace.DidHit() || trace.m_vHitNormal.z < standableZ)
	{
		this->CancelForcedUnduck();
		return;
	}
	this->applyingUnduck = true;
	this->ApplyForcedUnduck();
	this->player->GetMoveServices()->m_nButtons().m_pButtonStates[0] &= ~IN_DUCK;
}

void KZAutoUnduckStyleService::Cleanup()
{
	this->player->GetMoveServices()->m_flLastDuckTime(this->lastActualDuckTime);
}

void KZAutoUnduckStyleService::UpdateDuckTime()
{
	if (this->player->GetMoveServices()->m_flLastDuckTime() < 99999.0f)
	{
		this->lastActualDuckTime = this->player->GetMoveServices()->m_flLastDuckTime();
	}
}

void KZAutoUnduckStyleService::ApplyForcedUnduck()
{
	// Set crouch time to a very large value so that the player cannot reduck.
	this->player->GetMoveServices()->m_flLastDuckTime(100000.0f);
	this->player->GetMoveServices()->m_nButtons().m_pButtonStates[0] &= ~IN_DUCK;
}

void KZAutoUnduckStyleService::CancelForcedUnduck()
{
	this->player->GetMoveServices()->m_flLastDuckTime(this->lastActualDuckTime);
}
