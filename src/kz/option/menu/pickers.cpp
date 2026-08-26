#include "kz/option/menu/pickers.h"
#include "kz/option/menu/tables.h"
#include "kz/language/kz_language.h"

#include "tier0/memdbgon.h"

// === Fonts ==========================================================================

static_function i32 FamilySize(i32 first)
{
	i32 count = 1;
	while (first + count < PANORAMA_FONT_COUNT && V_strcmp(PANORAMA_FONTS[first + count].family, PANORAMA_FONTS[first].family) == 0)
	{
		count++;
	}
	return count;
}

std::string KZMenuFontPage::GetTitle(KZPlayer *player, KZMenuContext context)
{
	return player->languageService->PrepareMessage("Menu - Font");
}

void KZMenuFontPage::BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows)
{
	const char *current = this->GetCurrentSlug(player, context);
	for (i32 i = 0; i < PANORAMA_FONT_COUNT; i += FamilySize(i))
	{
		const i32 size = FamilySize(i);
		bool selected = false;
		for (i32 j = i; j < i + size; j++)
		{
			selected = selected || V_strcmp(current, PANORAMA_FONTS[j].slug) == 0;
		}
		KZMenuRow row;
		row.label = std::string(selected ? "* " : "") + PANORAMA_FONTS[i].family + (size > 1 ? " >" : "");
		row.param = {i};
		rows.push_back(row);
	}
}

void KZMenuFontPage::OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param)
{
	if (FamilySize(param.index) > 1)
	{
		player->menuService->OpenPage(&this->facePage, {context.index, param.index});
		return;
	}
	this->OnFontPicked(player, context, PANORAMA_FONTS[param.index].slug);
}

std::string KZMenuFontPage::FacePage::GetTitle(KZPlayer *player, KZMenuContext context)
{
	return PANORAMA_FONTS[context.sub].family;
}

void KZMenuFontPage::FacePage::BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows)
{
	const char *current = this->owner->GetCurrentSlug(player, {context.index});
	for (i32 i = context.sub; i < PANORAMA_FONT_COUNT && V_strcmp(PANORAMA_FONTS[i].family, PANORAMA_FONTS[context.sub].family) == 0; i++)
	{
		KZMenuRow row;
		row.label = std::string(V_strcmp(current, PANORAMA_FONTS[i].slug) == 0 ? "* " : "") + PANORAMA_FONTS[i].variant;
		row.param = {i};
		rows.push_back(row);
	}
}

void KZMenuFontPage::FacePage::OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param)
{
	this->owner->OnFontPicked(player, {context.index}, PANORAMA_FONTS[param.index].slug);
}

// === Colors ========================================================================

std::string KZMenuColorPage::GetTitle(KZPlayer *player, KZMenuContext context)
{
	return player->languageService->PrepareMessage("Menu - Color");
}

void KZMenuColorPage::BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows)
{
	const char *current = PanoramaNearestColorClass(this->GetCurrentColor(player, context));
	for (i32 i = 0; i < PANORAMA_COLOR_COUNT; i++)
	{
		KZMenuRow row;
		row.label = std::string(current == PANORAMA_COLORS[i].className ? "* " : "") + (PANORAMA_COLORS[i].className + PANORAMA_COLOR_NAME_OFFSET);
		row.colorClass = PANORAMA_COLORS[i].className;
		row.param = {i};
		rows.push_back(row);
	}
}

void KZMenuColorPage::OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param)
{
	this->OnColorPicked(player, context, Color(PANORAMA_COLORS[param.index].r, PANORAMA_COLORS[param.index].g, PANORAMA_COLORS[param.index].b, 255));
}
