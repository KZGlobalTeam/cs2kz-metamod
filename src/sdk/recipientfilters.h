#pragma once
#include "recipientfilter.h"
#include "utils/utils.h"
#include "interfaces/interfaces.h"
#include "networksystem/inetchannel.h"
#include "sdk/entity/cbaseentity.h"
#include "engine2/serversideclient.h"
#include "engine2/inetworkgameserver.h"

// The SDK's CBroadcastRecipientFilter also reaches clients that are still connecting.
class CInGameRecipientFilter : public CRecipientFilter
{
public:
	CInGameRecipientFilter()
	{
		MakeReliable();
		CUtlVector<CServerSideClient *> *clients = g_pKZUtils->GetClientList();
		if (!clients)
		{
			return;
		}
		for (int i = 0; i < clients->Count(); i++)
		{
			if (clients->Element(i)->IsActive())
			{
				AddRecipient(clients->Element(i)->GetPlayerSlot());
			}
		}
	}
};
