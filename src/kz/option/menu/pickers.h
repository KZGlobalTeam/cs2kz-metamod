#pragma once
#include "kz/option/menu/kz_menu.h"

// Font picker. The page lists one row per family; a family with a single face selects on click,
// otherwise it opens the face list for that family.
class KZMenuFontPage : public KZMenuPage
{
public:
	virtual const char *GetCurrentSlug(KZPlayer *player, KZMenuContext context) = 0;
	virtual void OnFontPicked(KZPlayer *player, KZMenuContext context, const char *slug) = 0;

	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override;
	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override;
	virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) override;

private:
	// Faces of one family: context.index is the owner's subject, context.sub the family's first
	// index in the table.
	class FacePage : public KZMenuPage
	{
	public:
		FacePage(KZMenuFontPage *owner) : owner(owner) {}

		virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override;
		virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override;
		virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) override;

	private:
		KZMenuFontPage *owner;
	} facePage {this};
};

// Color picker: every color the generated sheet defines, each row drawn in the color it sets.
class KZMenuColorPage : public KZMenuPage
{
public:
	virtual Color GetCurrentColor(KZPlayer *player, KZMenuContext context) = 0;
	virtual void OnColorPicked(KZPlayer *player, KZMenuContext context, const Color &color) = 0;

	virtual std::string GetTitle(KZPlayer *player, KZMenuContext context) override;
	virtual void BuildRows(KZPlayer *player, KZMenuContext context, std::vector<KZMenuRow> &rows) override;
	virtual void OnRowPicked(KZPlayer *player, KZMenuContext context, KZMenuContext param) override;
};
