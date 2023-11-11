#pragma once
// from Valve's GameNetworkingSockets
#include "common.h"
typedef int64 SteamNetworkingMicroseconds;

typedef enum : unsigned short
{ 
	// Reserved invalid/dummy address type
	k_EIPTypeInvalid = 0,
	k_EIPTypeLoopbackDeprecated,
	k_EIPTypeBroadcastDeprecated,
	k_EIPTypeV4,
	k_EIPTypeV6,
} ipadrtype_t;

struct netadr_t_s2
{
	union {

		//
		// IPv4
		//
		uint32			m_unIPv4;			// IP stored in host order (little-endian)
		struct
		{
			#ifdef VALVE_BIG_ENDIAN
				uint8 b1, b2, b3, b4;
			#else
				uint8 b4, b3, b2, b1;
			#endif
		} m_IPv4Bytes;

		//
		// IPv6
		//
		byte		m_rgubIPv6[16];	// Network order! Same as inaddr_in6.  (0011:2233:4455:6677:8899:aabb:ccdd:eeff)
		uint64	m_ipv6Qword[2];	// In a few places we can use these to avoid memcmp. BIG ENDIAN!
	};

	uint32		m_unIPv6Scope;			// IPv6 scope
	ipadrtype_t		m_usType;				// ipadrtype_t
	unsigned short	m_usPort;
};

struct RecvPktInfo_t
{
	const void *m_pPkt;
	int m_cbPkt;
	SteamNetworkingMicroseconds m_usecNow; // Current time
	// FIXME - coming soon!
	//SteamNetworkingMicroseconds m_usecRecvMin; // Earliest possible time when the packet might have actually arrived
	//SteamNetworkingMicroseconds m_usecRecvMax; // Latest possible time when the packet might have actually arrived
	netadr_t_s2 m_adrFrom;
	void *m_pSock; //IRawUDPSocket
};