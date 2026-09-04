#include "utils/cvarquery.h"
#include "utils/interfaces.h"
#include "sdk/recipientfilters.h"

#include <igameeventsystem.h>
#include <networksystem/inetworkmessages.h>
#include <unordered_map>

#include "tier0/memdbgon.h"

static_global std::unordered_map<i32, cvarquery::Callback> pendingQueries[MAXPLAYERS];
static_global i32 queryCookie = 0;
static_global cvarquery::ChangeCallback changeCallback = NULL;

void cvarquery::SetChangeCallback(ChangeCallback callback)
{
	changeCallback = callback;
}

void cvarquery::OnCvarValueResponse(CPlayerSlot slot, i32 cookie, Status status, const char *name, const char *value)
{
	if (slot.Get() < 0 || slot.Get() >= MAXPLAYERS)
	{
		return;
	}
	auto &pending = pendingQueries[slot.Get()];
	auto it = pending.find(cookie);
	if (it == pending.end())
	{
		return;
	}
	// Taken out before running: the callback is free to start another query.
	Callback callback = std::move(it->second);
	pending.erase(it);
	callback(slot, status, name, value);
}

void cvarquery::OnClientConVar(CPlayerSlot slot, const char *name, const char *value)
{
	if (changeCallback && slot.Get() >= 0 && slot.Get() < MAXPLAYERS)
	{
		changeCallback(slot, name, value);
	}
}

void cvarquery::Shutdown()
{
	changeCallback = NULL;
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		pendingQueries[i].clear();
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
	const i32 cookie = ++queryCookie;
	auto msg = netmsg->AllocateMessage()->ToPB<CSVCMsg_GetCvarValue>();
	msg->set_cookie(cookie);
	msg->set_cvar_name(cvarName);
	CSingleRecipientFilter filter(slot.Get());
	interfaces::pGameEventSystem->PostEventAbstract(0, false, &filter, netmsg, msg, 0);
	delete msg;

	pendingQueries[slot.Get()][cookie] = std::move(callback);
	return true;
}

void cvarquery::OnClientDisconnect(CPlayerSlot slot)
{
	if (slot.Get() >= 0 && slot.Get() < MAXPLAYERS)
	{
		pendingQueries[slot.Get()].clear();
	}
}
