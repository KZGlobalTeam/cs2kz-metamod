#include "kz/option/menu/model.h"

#include "tier0/memdbgon.h"

namespace KZMenu
{
	static std::vector<KZOptNode *> g_tree;

	const std::vector<KZOptNode *> &Tree()
	{
		return g_tree;
	}

	void SetItemSubtext(KZOptNode *node, const char *phraseKey)
	{
		if (!node->items.empty())
		{
			node->items.back().subKey = phraseKey;
		}
	}

	void SetItemDivider(KZOptNode *node)
	{
		if (!node->items.empty())
		{
			node->items.back().dividerAfter = true;
		}
	}

	KZOptNode *AddCategory(const char *phraseKey)
	{
		KZOptNode *node = new KZOptNode();
		node->phraseKey = phraseKey;
		g_tree.push_back(node);
		return node;
	}

	KZOptNode *AddSub(KZOptNode *parent, const char *phraseKey)
	{
		KZOptNode *node = new KZOptNode();
		node->phraseKey = phraseKey;
		parent->subs.push_back(node);
		return node;
	}

	void AddToggle(KZOptNode *node, const char *phraseKey, const char *prefKey, bool def, i64 tag)
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Toggle;
		item.prefKey = prefKey;
		item.idef = def ? 1 : 0;
		item.tag = tag;
		node->items.push_back(item);
	}

	void AddActionToggle(KZOptNode *node, const char *phraseKey, i64 (*getCurrent)(KZPlayer *, i64), void (*onActivate)(KZPlayer *, i64), i64 tag)
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Toggle;
		item.getCurrent = getCurrent;
		item.onActivate = onActivate;
		item.tag = tag;
		node->items.push_back(item);
	}

	void AddColor(KZOptNode *node, const char *phraseKey, const char *prefKey, const Color &def, i64 tag, void (*onEdit)(KZPlayer *, i64, bool))
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Color;
		item.prefKey = prefKey;
		item.cdef = def;
		item.tag = tag;
		item.onEdit = onEdit;
		node->items.push_back(item);
	}

	void AddFont(KZOptNode *node, const char *phraseKey, const char *prefKey, const char *def, i64 tag, void (*onEdit)(KZPlayer *, i64, bool))
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Font;
		item.prefKey = prefKey;
		item.sdef = def;
		item.tag = tag;
		item.onEdit = onEdit;
		node->items.push_back(item);
	}

	void AddPosition(KZOptNode *node, const char *phraseKey, const char *xKey, const char *yKey, i32 xDef, i32 yDef, i64 tag,
					 void (*onEdit)(KZPlayer *, i64, bool))
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Position;
		item.prefKey = xKey;
		item.yKey = yKey;
		item.lo = -100;
		item.hi = 100;
		item.idef = xDef;
		item.iydef = yDef;
		item.tag = tag;
		item.onEdit = onEdit;
		node->items.push_back(item);
	}

	void AddSize(KZOptNode *node, const char *phraseKey, const char *prefKey, i32 def, i32 lo, i32 hi, i64 tag, void (*onEdit)(KZPlayer *, i64, bool))
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Size;
		item.prefKey = prefKey;
		item.lo = lo;
		item.hi = hi;
		item.idef = def;
		item.tag = tag;
		item.onEdit = onEdit;
		node->items.push_back(item);
	}

	void AddButton(KZOptNode *node, const char *phraseKey, void (*onActivate)(KZPlayer *, i64), i64 tag)
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Button;
		item.onActivate = onActivate;
		item.tag = tag;
		node->items.push_back(item);
	}

	void AddChoice(KZOptNode *node, const char *phraseKey, void (*getChoices)(KZPlayer *, i64, std::vector<KZChoice> &),
				   i64 (*getCurrent)(KZPlayer *, i64), void (*onPick)(KZPlayer *, i64, i64), i64 tag)
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Choice;
		item.getChoices = getChoices;
		item.getCurrent = getCurrent;
		item.onPick = onPick;
		item.tag = tag;
		node->items.push_back(item);
	}
} // namespace KZMenu
