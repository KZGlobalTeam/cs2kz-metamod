#include "utils/cvarquery.h"
#include "utils/interfaces.h"
#include "utils/addresses.h"
#include "utils/logging.h"
#include "sdk/serversideclient.h"
#include "sdk/recipientfilters.h"

#include <igameeventsystem.h>
#include <networksystem/inetworkmessages.h>
#include <unordered_map>

#include "tier0/memdbgon.h"

SH_DECL_HOOK1(CServerSideClientBase, ProcessRespondCvarValue, SH_NOATTRIB, 0, bool, const CNetMessagePB<CCLCMsg_RespondCvarValue> &);

static_global std::unordered_map<i32, cvarquery::Callback> g_pendingQueries[MAXPLAYERS];
static_global i32 g_queryCookie = 0;
static_global i32 g_respondHook = 0;

static_function bool Hook_ProcessRespondCvarValue(const CNetMessagePB<CCLCMsg_RespondCvarValue> &msg)
{
	CServerSideClientBase *client = META_IFACEPTR(CServerSideClientBase);
	const i32 slot = client ? client->GetPlayerSlot().Get() : -1;
	if (slot >= 0 && slot < MAXPLAYERS)
	{
		auto &pending = g_pendingQueries[slot];
		auto it = pending.find(msg.cookie());
		if (it != pending.end())
		{
			// Taken out before running: the callback is free to start another query.
			cvarquery::Callback callback = std::move(it->second);
			pending.erase(it);
			callback(CPlayerSlot(slot), (cvarquery::Status)msg.status_code(), msg.name().c_str(), msg.value().c_str());
		}
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

bool cvarquery::Init()
{
	CServerSideClientBase *vtable = (CServerSideClientBase *)modules::engine->FindVirtualTable("CServerSideClient");
	if (!vtable)
	{
		KZ_LOG_WARN(LogChannel::General, "Failed to resolve CServerSideClient's virtual table; client convar queries are unavailable.\n");
		return false;
	}
	g_respondHook = SH_ADD_DVPHOOK(CServerSideClientBase, ProcessRespondCvarValue, vtable, SH_STATIC(Hook_ProcessRespondCvarValue), true);
	return true;
}

void cvarquery::Shutdown()
{
	if (g_respondHook)
	{
		SH_REMOVE_HOOK_ID(g_respondHook);
		g_respondHook = 0;
	}
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		g_pendingQueries[i].clear();
	}
}

bool cvarquery::Query(CPlayerSlot slot, const char *cvarName, Callback callback)
{
	if (!cvarName || !callback || slot.Get() < 0 || slot.Get() >= MAXPLAYERS)
	{
		return false;
	}
	if (!interfaces::pEngine->GetPlayerNetInfo(slot))
	{
		return false;
	}
	INetworkMessageInternal *netmsg = g_pNetworkMessages->FindNetworkMessagePartial("CSVCMsg_GetCvarValue");
	if (!netmsg)
	{
		return false;
	}
	const i32 cookie = ++g_queryCookie;
	auto msg = netmsg->AllocateMessage()->ToPB<CSVCMsg_GetCvarValue>();
	msg->set_cookie(cookie);
	msg->set_cvar_name(cvarName);
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;

	g_pendingQueries[slot.Get()][cookie] = std::move(callback);
	return true;
}

void cvarquery::OnClientDisconnect(CPlayerSlot slot)
{
	if (slot.Get() >= 0 && slot.Get() < MAXPLAYERS)
	{
		g_pendingQueries[slot.Get()].clear();
	}
}
