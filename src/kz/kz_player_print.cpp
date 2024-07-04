#include "kz.h"
#include "utils/utils.h"
#include "../kz/option/kz_option.h"

#include "sdk/recipientfilters.h"
#include "tier0/memdbgon.h"

#define FORMAT_STRING(buffer, addPrefix) \
	va_list args; \
	va_start(args, format); \
	char buffer[512]; \
	if (addPrefix) \
	{ \
		const char *prefix = KZOptionService::GetOptionStr("chatPrefix", KZ_DEFAULT_CHAT_PREFIX); \
		snprintf(buffer, sizeof(buffer), "%s ", prefix); \
		vsnprintf(buffer + strlen(prefix) + 1, sizeof(buffer) - (strlen(prefix) + 1), format, args); \
	} \
	else \
	{ \
		vsnprintf(buffer, sizeof(buffer), format, args); \
	} \
	va_end(args);

static_function CRecipientFilter *CreateRecipientFilter(KZPlayer *targetPlayer, bool addSpectators)
{
	if (!targetPlayer->GetController())
	{
		return nullptr;
	}
	CRecipientFilter *filter = new CRecipientFilter();
	CPlayerSlot slot = targetPlayer->GetPlayerSlot();
	filter->AddRecipient(slot);
	if (!addSpectators)
	{
		return filter;
	}
	if (!targetPlayer->IsAlive())
	{
		return filter;
	}
	CCSPlayerPawn *tarGetPlayerPawn = targetPlayer->GetPlayerPawn();
	if (!tarGetPlayerPawn)
	{
		return nullptr;
	}
	for (int i = 0; i < g_pKZUtils->GetServerGlobals()->maxClients; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || player->IsAlive())
		{
			continue;
		}
		if (!player->GetController() || !player->GetController()->m_hObserverPawn())
		{
			continue;
		}
		CPlayer_ObserverServices *obsService = player->GetController()->m_hObserverPawn()->m_pObserverServices;
		if (!obsService)
		{
			continue;
		}
		if (obsService->m_hObserverTarget().IsValid() && obsService->m_hObserverTarget().GetEntryIndex() == tarGetPlayerPawn->GetEntityIndex().Get())
		{
			filter->AddRecipient(player->GetPlayerSlot());
		}
	}
	return filter;
}

void KZPlayer::PrintConsole(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter *filter = CreateRecipientFilter(this, includeSpectators);
	if (!filter)
	{
		return;
	}
	utils::ClientPrintFilter(filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
	delete filter;
}

void KZPlayer::PrintChat(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	FORMAT_STRING(buffer, addPrefix);
	char coloredBuffer[512];
	if (!utils::CFormat(coloredBuffer, sizeof(coloredBuffer), buffer))
	{
		Warning("utils::CPrintChat did not have enough space to print: %s\n", buffer);
		return;
	}
	CRecipientFilter *filter = CreateRecipientFilter(this, includeSpectators);
	if (!filter)
	{
		return;
	}
	utils::ClientPrintFilter(filter, HUD_PRINTTALK, coloredBuffer, "", "", "", "");
	delete filter;
}

void KZPlayer::PrintCentre(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter *filter = CreateRecipientFilter(this, includeSpectators);
	if (!filter)
	{
		return;
	}
	utils::ClientPrintFilter(filter, HUD_PRINTCENTER, buffer, "", "", "", "");
	delete filter;
}

void KZPlayer::PrintAlert(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter *filter = CreateRecipientFilter(this, includeSpectators);
	if (!filter)
	{
		return;
	}
	utils::ClientPrintFilter(filter, HUD_PRINTALERT, buffer, "", "", "", "");
	delete filter;
}

void KZPlayer::PrintHTMLCentre(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	FORMAT_STRING(buffer, addPrefix);
	if (!includeSpectators)
	{
		utils::PrintHTMLCentre(this->GetController(), buffer);
		return;
	}
	CRecipientFilter *filter = CreateRecipientFilter(this, includeSpectators);
	if (!filter)
	{
		return;
	}
	CBasePlayerController *controller = this->GetController();
	if (!controller)
	{
		return;
	}

	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event)
	{
		return;
	}
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 5);
	event->SetInt("userid", -1);

	for (int i = 0; i < filter->GetRecipientCount(); i++)
	{
		IGameEventListener2 *listener = g_pKZUtils->GetLegacyGameEventListener(filter->GetRecipientIndex(i));
		listener->FireGameEvent(event);
	}
	interfaces::pGameEventManager->FreeEvent(event);
	delete filter;
}
