#ifndef SERVERSIDECLIENT_H
#define SERVERSIDECLIENT_H

#if COMPILER_MSVC
#pragma once
#endif

#include "clientframe.h"
#include "netmessages.h"

#include <inetchannel.h>
#include <playerslot.h>
#include "circularbuffer.h"
#include "utlsignalslot.h"
#include "tier1/netadr.h"
#include <entity2/entityidentity.h>
#include <networksystem/inetworksystem.h>
#include <steam/steamclientpublic.h>
#include <tier1/utlstring.h>

#include <netmessages.pb.h>
#include <network_connection.pb.h>

class CHLTVServer;
class INetMessage;
class CNetworkGameServerBase;
class CNetworkGameServer;

struct HltvReplayStats_t
{
	enum FailEnum_t
	{
		FAILURE_ALREADY_IN_REPLAY,
		FAILURE_TOO_FREQUENT,
		FAILURE_NO_FRAME,
		FAILURE_NO_FRAME2,
		FAILURE_CANNOT_MATCH_DELAY,
		FAILURE_FRAME_NOT_READY,
		NUM_FAILURES
	};

	uint nClients;
	uint nStartRequests;
	uint nSuccessfulStarts;
	uint nStopRequests;
	uint nAbortStopRequests;
	uint nUserCancels;
	uint nFullReplays;
	uint nNetAbortReplays;
	uint nFailedReplays[NUM_FAILURES];
}; // sizeof 56

COMPILE_TIME_ASSERT(sizeof(HltvReplayStats_t) == 56);

struct Spike_t
{
public:
	CUtlString m_szDesc;
	int m_nBits;
};

COMPILE_TIME_ASSERT(sizeof(Spike_t) == 16);

class CNetworkStatTrace
{
public:
	CUtlVector<Spike_t> m_Records;
	int m_nMinWarningBytes;
	int m_nStartBit;
	int m_nCurBit;
};

COMPILE_TIME_ASSERT(sizeof(CNetworkStatTrace) == 40);

class CServerSideClientBase : public CUtlSlot, public INetworkChannelNotify, public INetworkMessageProcessingPreFilter
{
public:
	virtual ~CServerSideClientBase() = 0;

public:
	CPlayerSlot GetPlayerSlot() const
	{
		return m_nClientSlot;
	}

	CPlayerUserId GetUserID() const
	{
		return m_UserID;
	}

	CEntityIndex GetEntityIndex() const
	{
		return m_nEntityIndex;
	}

	CSteamID GetClientSteamID() const
	{
		return m_SteamID;
	}

	const char *GetClientName() const
	{
		return m_Name.Get();
	}

	INetChannel *GetNetChannel() const
	{
		return m_NetChannel;
	}

	const netadr_t *GetRemoteAddress() const
	{
		return &m_nAddr.GetAddress();
	}

	CNetworkGameServerBase *GetServer() const
	{
		return m_Server;
	}

	virtual void Connect(int socket, const char *pszName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, uint32 uChallengeNumber) = 0;
	virtual void Inactivate(const char *pszAddons) = 0;
	virtual void Reactivate(CPlayerSlot nSlot) = 0;
	virtual void SetServer(CNetworkGameServer *pNetServer) = 0;
	virtual void Reconnect() = 0;
	virtual void Disconnect(ENetworkDisconnectionReason reason, const char *pszInternalReason) = 0;
	virtual bool CheckConnect() = 0;
	virtual void Create(CPlayerSlot &nSlot, CSteamID nSteamID, const char *pszName) = 0;
	virtual void SetRate(int nRate) = 0;
	virtual void SetUpdateRate(float fUpdateRate) = 0;
	virtual int GetRate() = 0;

	virtual void Clear() = 0;

	virtual bool ExecuteStringCommand(const CNetMessagePB<CNETMsg_StringCmd> &msg) = 0; // "false" trigger an anti spam counter to kick a client.
	virtual bool SendNetMessage(const CNetMessage *pData, NetChannelBufType_t bufType = BUF_DEFAULT) = 0;
	virtual bool FilterMessage(const CNetMessage *pData, INetChannel *pChannel) = 0; // "Client %d(%s) tried to send a RebroadcastSourceId msg.\n"

public:
	virtual void ClientPrintf(PRINTF_FORMAT_STRING const char *, ...) = 0;

	bool IsConnected() const
	{
		return m_nSignonState >= SIGNONSTATE_CONNECTED;
	}

	bool IsInGame() const
	{
		return m_nSignonState == SIGNONSTATE_FULL;
	}

	bool IsSpawned() const
	{
		return m_nSignonState >= SIGNONSTATE_NEW;
	}

	bool IsActive() const
	{
		return m_nSignonState == SIGNONSTATE_FULL;
	}

	virtual bool IsFakeClient() const
	{
		return m_bFakePlayer;
	}

	bool IsHLTV() const
	{
		return m_bIsHLTV;
	}

	virtual bool IsHumanPlayer() const
	{
		return false;
	}

	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	virtual bool IsHearingClient(CPlayerSlot nSlot) const
	{
		return false;
	}

	virtual bool IsProximityHearingClient() const = 0;

	virtual bool IsLowViolenceClient() const
	{
		return m_bLowViolence;
	}

	virtual bool IsSplitScreenUser() const
	{
		return m_bSplitScreenUser;
	}

public: // Message Handlers
	virtual bool ProcessTick(const CNetMessagePB<CNETMsg_Tick> &msg) = 0;
	virtual bool ProcessStringCmd(const CNetMessagePB<CNETMsg_StringCmd> &msg) = 0;

public:
	virtual bool ApplyConVars(const CMsg_CVars &list) = 0;

private:
	virtual bool unk_28() = 0;

public:
	virtual bool ProcessSpawnGroup_LoadCompleted(const CNetMessagePB<CNETMsg_SpawnGroup_LoadCompleted> &msg) = 0;
	virtual bool ProcessClientInfo(const CNetMessagePB<CCLCMsg_ClientInfo> &msg) = 0;
	virtual bool ProcessBaselineAck(const CNetMessagePB<CCLCMsg_BaselineAck> &msg) = 0;
	virtual bool ProcessLoadingProgress(const CNetMessagePB<CCLCMsg_LoadingProgress> &msg) = 0;
	virtual bool ProcessSplitPlayerConnect(const CNetMessagePB<CCLCMsg_SplitPlayerConnect> &msg) = 0;
	virtual bool ProcessSplitPlayerDisconnect(const CNetMessagePB<CCLCMsg_SplitPlayerDisconnect> &msg) = 0;
	virtual bool ProcessCmdKeyValues(const CNetMessagePB<CCLCMsg_CmdKeyValues> &msg) = 0;

private:
	virtual bool unk_36() = 0;
	virtual bool unk_37() = 0;

public:
	virtual bool ProcessMove(const CNetMessagePB<CCLCMsg_Move> &msg) = 0;
	virtual bool ProcessVoiceData(const CNetMessagePB<CCLCMsg_VoiceData> &msg) = 0;
	virtual bool ProcessRespondCvarValue(const CNetMessagePB<CCLCMsg_RespondCvarValue> &msg) = 0;

	virtual bool ProcessPacketStart(const CNetMessagePB<NetMessagePacketStart> &msg) = 0;
	virtual bool ProcessPacketEnd(const CNetMessagePB<NetMessagePacketEnd> &msg) = 0;
	virtual bool ProcessConnectionClosed(const CNetMessagePB<NetMessageConnectionClosed> &msg) = 0;
	virtual bool ProcessConnectionCrashed(const CNetMessagePB<NetMessageConnectionCrashed> &msg) = 0;

public:
	virtual bool ProcessChangeSplitscreenUser(const CNetMessagePB<NetMessageSplitscreenUserChanged> &msg) = 0;

private:
	virtual bool unk_47() = 0;
	virtual bool unk_48() = 0;
	virtual bool unk_49() = 0;

public:
	virtual void ConnectionStart(INetChannel *pNetChannel) = 0;

private: // SpawnGroup something.
	virtual void unk_51() = 0;
	virtual void unk_52() = 0;

public:
	virtual void ExecuteDelayedCall(void *) = 0;

	virtual bool UpdateAcknowledgedFramecount(int tick) = 0;

	void ForceFullUpdate()
	{
		m_nDeltaTick = -1;
	}

	virtual bool ShouldSendMessages() = 0;
	virtual void UpdateSendState() = 0;

	virtual const CMsgPlayerInfo &GetPlayerInfo() const
	{
		return m_playerInfo;
	}

	virtual void UpdateUserSettings() = 0;
	virtual void ResetUserSettings() = 0;

private:
	virtual void unk_60() = 0;

public:
	virtual void SendSignonData() = 0;
	virtual void SpawnPlayer() = 0;
	virtual void ActivatePlayer() = 0;

	virtual void SetName(const char *name) = 0;
	virtual void SetUserCVar(const char *cvar, const char *value) = 0;

	SignonState_t GetSignonState() const
	{
		return m_nSignonState;
	}

	virtual void FreeBaselines() = 0;

	bool IsFullyAuthenticated(void)
	{
		return m_bFullyAuthenticated;
	}

	void SetFullyAuthenticated(void)
	{
		m_bFullyAuthenticated = true;
	}

	virtual CServerSideClientBase *GetSplitScreenOwner()
	{
		return m_pAttachedTo;
	}

	virtual int GetNumPlayers() = 0;

	virtual void ShouldReceiveStringTableUserData() = 0;

private:
	virtual void unk_70(CPlayerSlot nSlot) = 0;
	virtual void unk_71() = 0;
	virtual void unk_72() = 0;

public:
	virtual int GetHltvLastSendTick() = 0;

private:
	virtual void unk_74() = 0;
	virtual void unk_75() = 0;
	virtual void unk_76() = 0;

public:
	virtual void Await() = 0;

	virtual void MarkToKick() = 0;
	virtual void UnmarkToKick() = 0;

	virtual bool ProcessSignonStateMsg(int state) = 0;
	virtual void PerformDisconnection(ENetworkDisconnectionReason reason) = 0;

public:
	CUtlString m_UserIDString;
	CUtlString m_Name;
	CPlayerSlot m_nClientSlot;
	CEntityIndex m_nEntityIndex;
	CNetworkGameServerBase *m_Server;
	INetChannel *m_NetChannel;
	uint16 m_nConnectionTypeFlags;
	bool m_bMarkedToKick;
	SignonState_t m_nSignonState;
	bool m_bSplitScreenUser;
	bool m_bSplitAllowFastDisconnect;
	int m_nSplitScreenPlayerSlot;
	CServerSideClientBase *m_SplitScreenUsers[4];
	CServerSideClientBase *m_pAttachedTo;
	bool m_bSplitPlayerDisconnecting;
	int m_UnkVariable172;
	bool m_bFakePlayer;
	bool m_bSendingSnapshot;

private:
	[[maybe_unused]] char pad6[0x5];

public:
	CPlayerUserId m_UserID = -1;
	bool m_bReceivedPacket; // true, if client received a packet after the last send packet
	CSteamID m_SteamID;
	CSteamID m_DisconnectedSteamID;
	CSteamID m_AuthTicketSteamID; // Auth ticket
	CSteamID m_nFriendsID;
	ns_address m_nAddr;
	ns_address m_nAddr2;
	KeyValues *m_ConVars;
	bool m_bUnk0;

private:
	[[maybe_unused]] char pad273[0x28];

public:
	bool m_bConVarsChanged;
	bool m_bIsHLTV;

private:
	[[maybe_unused]] char pad29[0xB];

public:
	uint32 m_nSendtableCRC;
	uint32 m_uChallengeNumber;
	int m_nSignonTick;
	int m_nDeltaTick;
	int m_UnkVariable3;
	int m_nStringTableAckTick;
	int m_UnkVariable4;
	CFrameSnapshot *m_pLastSnapshot; // last send snapshot
	CUtlVector<void *> m_vecLoadedSpawnGroups;
	CMsgPlayerInfo m_playerInfo;
	CFrameSnapshot *m_pBaseline;
	int m_nBaselineUpdateTick;
	CBitVec<MAX_EDICTS> m_BaselinesSent;
	int m_nBaselineUsed;    // 0/1 toggling flag, singaling client what baseline to use
	int m_nLoadingProgress; // 0..100 progress, only valid during loading

	// This is used when we send out a nodelta packet to put the client in a state where we wait
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int m_nForceWaitForTick = -1;

	CCircularBuffer m_UnkBuffer = {1024};
	bool m_bLowViolence = false; // true if client is in low-violence mode (L4D server needs to know)
	bool m_bSomethingWithAddressType = true;
	bool m_bFullyAuthenticated = false;
	bool m_bUnk1 = false;
	int m_nUnk;

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	float m_fNextMessageTime = 0.0f;
	float m_fAuthenticatedTime = -1.0f;

	// Default time to wait for next message
	float m_fSnapshotInterval = 0.0f;

private:
	// CSVCMsg_PacketEntities_t m_packetmsg;
	[[maybe_unused]] char pad2544[0x138];

public:
	CNetworkStatTrace m_Trace;
	int m_spamCommandsCount = 0; // if the value is greater than 16, the player will be kicked with reason 39
	int m_unknown = 0;
	double m_lastExecutedCommand = 0.0; // if command executed more than once per second, ++m_spamCommandCount

private:
	[[maybe_unused]] char pad2912[0x38];
#ifdef __linux__
	[[maybe_unused]] char pad3000[0x8];
#endif

public:
	CCommand *m_pCommand;
};
#ifdef __linux__
COMPILE_TIME_ASSERT(sizeof(CServerSideClientBase) == 3016);
#endif

class CServerSideClient : public CServerSideClientBase
{
public:
	virtual ~CServerSideClient() = 0;

public:
	CPlayerBitVec m_VoiceStreams;
	CPlayerBitVec m_VoiceProximity;
	CCheckTransmitInfo m_PackInfo;

private:
	uint8_t m_PackInfo2[576];

public:
	CClientFrameManager m_FrameManager;

private:
	[[maybe_unused]] char pad3904[8];

public:
	float m_flLastClientCommandQuotaStart = 0.0f;
	float m_flTimeClientBecameFullyConnected = -1.0f;
	bool m_bVoiceLoopback = false;
	bool m_bUnk10 = false;
	int m_nHltvReplayDelay = 0;
	CHLTVServer *m_pHltvReplayServer;
	int m_nHltvReplayStopAt;
	int m_nHltvReplayStartAt;
	int m_nHltvReplaySlowdownBeginAt;
	int m_nHltvReplaySlowdownEndAt;
	float m_flHltvReplaySlowdownRate;
	int m_nHltvLastSendTick;
	float m_flHltvLastReplayRequestTime;
	CUtlVector<INetMessage *> m_HltvQueuedMessages;
	HltvReplayStats_t m_HltvReplayStats;
};
#endif // SERVERSIDECLIENT_H
