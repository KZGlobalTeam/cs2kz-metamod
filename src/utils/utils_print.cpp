#include "utils.h"
#include "recipientfilters.h"

#include "tier0/memdbgon.h"

/*
* Credit to Szwagi
*/

internal char ConvertColorStringToByte(const char* str, size_t length) {
	switch (length) {
		case 3:
			if (!V_memcmp(str, "red", length)) return 7;
			break;
		case 4:
			if (!V_memcmp(str, "blue", length)) return 11;
			if (!V_memcmp(str, "lime", length)) return 6;
			if (!V_memcmp(str, "grey", length)) return 8;
			if (!V_memcmp(str, "gold", length)) return 16;
			break;
		case 5:
			if (!V_memcmp(str, "green", length)) return 4;
			if (!V_memcmp(str, "grey2", length)) return 13;
			if (!V_memcmp(str, "olive", length)) return 5;
			break;
		case 6:
			if (!V_memcmp(str, "purple", length)) return 3;
			if (!V_memcmp(str, "orchid", length)) return 14;
			if (!V_memcmp(str, "yellow", length)) return 9;
			break;
		case 7:
			if (!V_memcmp(str, "default", length)) return 1;
			if (!V_memcmp(str, "darkred", length)) return 2;
			break;
		case 8:
			if (!V_memcmp(str, "darkblue", length)) return 12;
			if (!V_memcmp(str, "lightred", length)) return 15;
			if (!V_memcmp(str, "bluegrey", length)) return 10;
			break;
	}
	return 0;
}

enum CFormatResult {
	CFORMAT_NOT_US,
	CFORMAT_OK,
	CFORMAT_OUT_OF_SPACE,
};

struct CFormatContext {
	const char* current;
	char* result;
	char* result_end;
};

internal bool HasEnoughSpace(const CFormatContext* ctx, uintptr_t space) {
	return (uintptr_t)(ctx->result_end - ctx->result) > space;
}

internal CFormatResult EscapeChars(CFormatContext* ctx) {
	if (*ctx->current == '{' && *(ctx->current+1) == '{') {
		if (!HasEnoughSpace(ctx, 1))
			return CFORMAT_OUT_OF_SPACE;
		
		ctx->current += 2;
		*ctx->result++ = '{';
		return CFORMAT_OK;
	}
	return CFORMAT_NOT_US;
}

internal CFormatResult ParseColors(CFormatContext* ctx) {
	const char* current = ctx->current;
	if (*current == '{') {
		current++;
		const char* start = current;
		while (*current && *current != '}')
			current++;
		if (*current == '}') {
			int length = current - start;
			current++;
			if (char byte = ConvertColorStringToByte(start, length); byte) {
				if (!HasEnoughSpace(ctx, 1))
					return CFORMAT_OUT_OF_SPACE;
				
				*ctx->result++ = byte;
				ctx->current = current;
				return CFORMAT_OK;
			}
		}
	}
	return CFORMAT_NOT_US;
}

internal CFormatResult ReplaceNewlines(CFormatContext* ctx) {
	if (*ctx->current == '\n') {
		if (!HasEnoughSpace(ctx, 3))
			return CFORMAT_OUT_OF_SPACE;
		
		ctx->current++;
		*ctx->result++ = '\xe2';
		*ctx->result++ = '\x80';
		*ctx->result++ = '\xa9';
		return CFORMAT_OK;
	}
	return CFORMAT_NOT_US;
}

internal CFormatResult AddSpace(CFormatContext* ctx)
{
    if (!HasEnoughSpace(ctx, 1))
        return CFORMAT_OUT_OF_SPACE;
    *ctx->result++ = ' ';
    return CFORMAT_OK;
}

internal bool CFormat(char* buffer, u64 buffer_size, const char* text) {
    assert(buffer_size != 0);

    CFormatContext ctx;
    ctx.current = text;
    ctx.result = buffer;
    ctx.result_end = buffer + buffer_size;

    if (AddSpace(&ctx) != CFORMAT_OK)
        return false;
    
    while (*ctx.current) {
        auto escape_chars = EscapeChars(&ctx);
        if (escape_chars == CFORMAT_OK) continue;
        if (escape_chars == CFORMAT_OUT_OF_SPACE) return false;
        
        auto parse_colors = ParseColors(&ctx);
        if (parse_colors == CFORMAT_OK) continue;
        if (parse_colors == CFORMAT_OUT_OF_SPACE) return false;
        
        auto replace_newlines = ReplaceNewlines(&ctx);
        if (replace_newlines == CFORMAT_OK) continue;
        if (replace_newlines == CFORMAT_OUT_OF_SPACE) return false;

        // Everything else
        if (!HasEnoughSpace(&ctx, 1)) return false;
        *ctx.result++ = *ctx.current++;
    }

    // Null terminate
    if (!HasEnoughSpace(&ctx, 1)) return false;
    *ctx.result++ = 0;

    return true;
}

#define FORMAT_STRING(buffer) \
	va_list args; \
	va_start(args, format); \
	char buffer[512]; \
	vsnprintf(buffer, sizeof(buffer), format, args); \
	va_end(args);

void utils::PrintConsole(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
}

void utils::PrintChat(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, buffer, "", "", "", "");
}

void utils::PrintCentre(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, buffer, "", "", "", "");
}

void utils::PrintAlert(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	UTIL_ClientPrintFilter(filter, HUD_PRINTALERT, buffer, "", "", "", "");
}

void utils::PrintHTMLCentre(CBaseEntity *entity, const char *format, ...)
{
	CBasePlayerController *controller = utils::GetController(entity);
	if (!controller) return;

	FORMAT_STRING(buffer);

	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event) return;
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 5);
	event->SetInt("userid", -1);

	CPlayerSlot slot = controller->entindex() - 1;
	IGameEventListener2 *listener = utils::GetLegacyGameEventListener(slot);
	listener->FireGameEvent(event);
	interfaces::pGameEventManager->FreeEvent(event);
}

void utils::PrintConsoleAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTCONSOLE, buffer, "", "", "", "");
}

void utils::PrintChatAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, buffer, "", "", "", "");
}

void utils::PrintCentreAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTCENTER, buffer, "", "", "", "");
}

void utils::PrintAlertAll(const char *format, ...)
{
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	UTIL_ClientPrintFilter(filter, HUD_PRINTALERT, buffer, "", "", "", "");
}

void utils::PrintHTMLCentreAll(const char *format, ...)
{
	FORMAT_STRING(buffer);

	IGameEvent *event = interfaces::pGameEventManager->CreateEvent("show_survival_respawn_status");
	if (!event) return;
	event->SetString("loc_token", buffer);
	event->SetInt("duration", 5);
	event->SetInt("userid", -1);

	interfaces::pGameEventManager->FireEvent(event);
}

void utils::CPrintChat(CBaseEntity *entity, const char *format, ...)
{
	FORMAT_STRING(buffer);
	CSingleRecipientFilter filter(utils::GetEntityPlayerSlot(entity).Get());
	char coloredBuffer[512];
	if (CFormat(coloredBuffer, sizeof(coloredBuffer), buffer))
	{
		UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, coloredBuffer, "", "", "", "");	
	}
	else
	{
		Warning("utils::CPrintChat did not have enough space to print: %s\n", buffer);
	}
}

void utils::CPrintChatAll(const char *format, ...)
{	
	FORMAT_STRING(buffer);
	CBroadcastRecipientFilter filter;
	char coloredBuffer[512];
	if (CFormat(coloredBuffer, sizeof(coloredBuffer), buffer))
	{
		UTIL_ClientPrintFilter(filter, HUD_PRINTTALK, coloredBuffer, "", "", "", "");	
	}
	else
	{
		Warning("utils::CPrintChatAll did not have enough space to print: %s\n", buffer);
	}
}