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
		size_t prefixLen = strlen(buffer); \
		vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen, format, args); \
	} \
	else \
	{ \
		vsnprintf(buffer, sizeof(buffer), format, args); \
	} \
	va_end(args);

static_function bool ShouldSkipMessage(KZPlayer *targetPlayer, bool addSpectators)
{
	return !addSpectators && targetPlayer->IsFakeClient() && !targetPlayer->IsCSTV();
}

static_function bool BuildRecipientFilter(CRecipientFilter &filter, KZPlayer *targetPlayer, bool addSpectators)
{
	if (!targetPlayer->GetController())
	{
		return false;
	}
	CPlayerSlot slot = targetPlayer->GetPlayerSlot();
	filter.AddRecipient(slot);
	if (!addSpectators)
	{
		return true;
	}
	if (!targetPlayer->IsAlive())
	{
		return true;
	}
	CCSPlayerPawn *targetPlayerPawn = targetPlayer->GetPlayerPawn();
	if (!targetPlayerPawn)
	{
		return false;
	}
	for (int i = 0; i < MAXPLAYERS + 1; i++)
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
		if (obsService->m_hObserverTarget().IsValid() && obsService->m_hObserverTarget().GetEntryIndex() == targetPlayerPawn->GetEntityIndex().Get())
		{
			filter.AddRecipient(player->GetPlayerSlot());
		}
	}
	return true;
}

void KZPlayer::PrintConsole(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	if (ShouldSkipMessage(this, includeSpectators))
	{
		return;
	}
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter filter;
	if (!BuildRecipientFilter(filter, this, includeSpectators))
	{
		return;
	}
	utils::ClientPrintFilter(&filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
}

void KZPlayer::PrintChat(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	if (ShouldSkipMessage(this, includeSpectators))
	{
		return;
	}
	FORMAT_STRING(buffer, addPrefix);
	char coloredBuffer[512];
	if (!utils::CFormat(coloredBuffer, sizeof(coloredBuffer), buffer))
	{
		Warning("utils::CPrintChat did not have enough space to print: %s\n", buffer);
		return;
	}
	CRecipientFilter filter;
	if (!BuildRecipientFilter(filter, this, includeSpectators))
	{
		return;
	}
	utils::ClientPrintFilter(&filter, HUD_PRINTTALK, coloredBuffer, "", "", "", "");
}

void KZPlayer::PrintCentre(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	if (ShouldSkipMessage(this, includeSpectators))
	{
		return;
	}
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter filter;
	if (!BuildRecipientFilter(filter, this, includeSpectators))
	{
		return;
	}
	utils::ClientPrintFilter(&filter, HUD_PRINTCENTER, buffer, "", "", "", "");
}

void KZPlayer::PrintAlert(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	if (ShouldSkipMessage(this, includeSpectators))
	{
		return;
	}
	FORMAT_STRING(buffer, addPrefix);
	CRecipientFilter filter;
	if (!BuildRecipientFilter(filter, this, includeSpectators))
	{
		return;
	}
	utils::ClientPrintFilter(&filter, HUD_PRINTALERT, buffer, "", "", "", "");
}

void KZPlayer::PrintHTMLCentre(bool addPrefix, bool includeSpectators, const char *format, ...)
{
	if (ShouldSkipMessage(this, includeSpectators))
	{
		return;
	}
	CUtlString buffer;
	va_list args;
	va_start(args, format);
	buffer.FormatV(format, args);

	if (addPrefix)
	{
		const char *prefix = KZOptionService::GetOptionStr("chatPrefix", KZ_DEFAULT_CHAT_PREFIX);
		buffer.Format("%s %s", prefix, buffer.Get());
	}

	if (!includeSpectators)
	{
		utils::PrintHTMLCentre(this->GetController(), buffer.Get());
		return;
	}

	CRecipientFilter filter;
	if (!BuildRecipientFilter(filter, this, includeSpectators))
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
	event->SetString("loc_token", buffer.Get());
	event->SetInt("duration", 1);
	event->SetInt("userid", -1);

	auto recipients = filter.GetRecipients();
	int index = recipients.FindNextSetBit(0);

	while (index > -1)
	{
		IGameEventListener2 *listener = g_pKZUtils->GetLegacyGameEventListener(index);
		listener->FireGameEvent(event);

		index = recipients.FindNextSetBit(index + 1);
	}
	interfaces::pGameEventManager->FreeEvent(event);
}
