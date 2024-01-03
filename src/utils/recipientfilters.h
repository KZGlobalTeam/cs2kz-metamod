#pragma once
#include "irecipientfilter.h"
#include "utils.h"
#include "interfaces/interfaces.h"

class CBroadcastRecipientFilter : public IRecipientFilter
{
public:
	CBroadcastRecipientFilter(bool bReliable = true, bool bInitMessage = false) :
		m_bReliable(bReliable), m_bInitMessage(bInitMessage) 
	{
		m_Recipients.RemoveAll();
		if (!g_pEntitySystem)
		{
			return;
		}
		for (int i = 0; i <= g_pKZUtils->GetServerGlobals()->maxClients; i++)
		{
			CBaseEntity *ent = g_pEntitySystem->GetBaseEntity(CEntityIndex(i));
			if (ent)
			{
				m_Recipients.AddToTail(i);
			}
		}
	}

	~CBroadcastRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }

	bool IsInitMessage(void) const override { return m_bInitMessage; }

	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CPlayerSlot GetRecipientIndex(int slot) const override { return CPlayerSlot(m_Recipients[slot]); }

private:
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVector<int> m_Recipients;
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

class CCopyRecipientFilter : public IRecipientFilter
{
public:
	CCopyRecipientFilter(IRecipientFilter *source, int iExcept)
	{
		m_bReliable = source->IsReliable();
		m_bInitMessage = source->IsInitMessage();
		m_Recipients.RemoveAll();

		for (int i = 0; i < source->GetRecipientCount(); i++)
		{
			if (source->GetRecipientIndex(i).Get() != iExcept)
				m_Recipients.AddToTail(source->GetRecipientIndex(i));
		}
	}

	~CCopyRecipientFilter() override {}

	bool IsReliable(void) const override { return m_bReliable; }

	bool IsInitMessage(void) const override { return m_bInitMessage; }

	int GetRecipientCount(void) const override { return m_Recipients.Count(); }

	CPlayerSlot GetRecipientIndex(int slot) const override
	{
		if (slot < 0 || slot >= GetRecipientCount())
			return CPlayerSlot(-1);

		return m_Recipients[slot];
	}

private:
	bool m_bReliable;
	bool m_bInitMessage;
	CUtlVectorFixed<CPlayerSlot, 64> m_Recipients;
};
