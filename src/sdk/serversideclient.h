#pragma once

#include "playerslot.h"
#include "steam/steamclientpublic.h"
#include "utlstring.h"
#include "inetchannel.h"

enum SignonState_t : int;

class CServerSideClient
{
	virtual ~CServerSideClient() = 0;

public:
	INetChannel *GetNetChannel() const
	{
		return m_NetChannel;
	}

	CPlayerSlot GetPlayerSlot() const
	{
		return m_nClientSlot;
	}

	CPlayerUserId GetUserID() const
	{
		return m_UserID;
	}

	int GetSignonState() const
	{
		return m_nSignonState;
	}

	CSteamID *GetClientSteamID() const
	{
		return (CSteamID *)&m_SteamID;
	}

	const char *GetClientName() const
	{
		return m_Name.Get();
	}

	netadr_t *GetRemoteAddress() const
	{
		return (netadr_t *)&m_NetAdr0;
	}

	void ForceFullUpdate()
	{
		m_nForceWaitForTick = -1;
	}

	bool IsFakePlayer()
	{
		return m_bFakePlayer;
	}

	bool IsHLTV()
	{
		return m_bIsHLTV;
	}

	bool IsConnected()
	{
		return m_nSignonState >= 2;
	}

	bool IsInGame()
	{
		return m_nSignonState == 6;
	}

private:
	[[maybe_unused]] void *m_pVT1; // INetworkMessageProcessingPreFilter
	[[maybe_unused]] char pad1[0x30];
#ifdef __linux__
	[[maybe_unused]] char pad2[0x10];
#endif
	CUtlString m_Name;           // 64 | 80
	CPlayerSlot m_nClientSlot;   // 72 | 88
	CEntityIndex m_nEntityIndex; // 76 | 92
	[[maybe_unused]] char pad3[0x8];
	INetChannel *m_NetChannel; // 88 | 104
	[[maybe_unused]] char pad4[0x4];
	int32 m_nSignonState; // 100 | 116
	[[maybe_unused]] char pad5[0x38];
	bool m_bFakePlayer; // 160 | 176
	[[maybe_unused]] char pad6[0x7];
	CPlayerUserId m_UserID; // 168 | 184
	[[maybe_unused]] char pad7[0x1];
	CSteamID m_SteamID; // 171 | 187
	[[maybe_unused]] char pad8[0x19];
	netadr_t m_NetAdr0; // 204 | 220
	[[maybe_unused]] char pad9[0x14];
	netadr_t m_NetAdr1; // 236 | 252
	[[maybe_unused]] char pad10[0x22];
	bool m_bIsHLTV;
	[[maybe_unused]] char pad11[0x19];
	int32 m_nForceWaitForTick; // 308 | 324
	[[maybe_unused]] char pad12[0x882];
	bool m_bFullyAuthenticated; // 2490 | 2506
};
