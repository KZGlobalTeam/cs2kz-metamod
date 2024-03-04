#pragma once
#include "irecipientfilter.h"
#include "utils/utils.h"
#include "interfaces/interfaces.h"

class CRecipientFilter : public IRecipientFilter
{
public:
	CRecipientFilter(bool bReliable = true, bool bInitMessage = false) :
		m_bReliable(bReliable), m_bInitMessage(bInitMessage) 
	{}
	~CRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }
	bool IsInitMessage(void) const override { return m_bInitMessage; }
	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
			return CPlayerSlot(-1);

		return m_Recipients[slot];
	}

	void AddRecipient(CPlayerSlot slot)
	{
		// Don't add if it already exists
		if (m_Recipients.Find(slot) != m_Recipients.InvalidIndex())
			return;

		m_Recipients.AddToTail(slot);
	}

	void AddAllPlayers()
	{
		m_Recipients.RemoveAll();
		if (!GameEntitySystem())
		{
			return;
		}
		for (int i = 0; i <= g_pKZUtils->GetServerGlobals()->maxClients; i++)
		{
			CBaseEntity *ent = GameEntitySystem()->GetBaseEntity(CEntityIndex(i));
			if (ent)
			{
				AddRecipient(i);
			}
		}
	}
private:
	// Can't copy this unless we explicitly do it!
	CRecipientFilter(CRecipientFilter const &source) { Assert(0); }
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVectorFixed<CPlayerSlot, MAXPLAYERS> m_Recipients;
};

class CBroadcastRecipientFilter : public CRecipientFilter
{
public:
	CBroadcastRecipientFilter( void )
	{
		AddAllPlayers();
	}
};

class CCopyRecipientFilter : public CRecipientFilter
{
public:
	CCopyRecipientFilter(IRecipientFilter *source, int iExcept)
	{
		for (int i = 0; i < source->GetRecipientCount(); i++)
		{
			if (source->GetRecipientIndex(i).Get() != iExcept)
				this->AddRecipient(source->GetRecipientIndex(i));
		}
	}
};

class CSingleRecipientFilter : public IRecipientFilter
{
public:
	CSingleRecipientFilter(int iRecipient, bool bReliable = true, bool bInitMessage = false) :
		m_bReliable(bReliable), m_bInitMessage(bInitMessage), m_iRecipient(iRecipient) {}

	~CSingleRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }

	bool IsInitMessage(void) const override { return m_bInitMessage; }

	int GetRecipientCount(void) const override { return 1; }

	CPlayerSlot GetRecipientIndex(int slot) const override { return CPlayerSlot(m_iRecipient); }

private:
	bool m_bReliable;
	bool m_bInitMessage;
	int m_iRecipient;
};
