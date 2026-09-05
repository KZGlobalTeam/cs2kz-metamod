#include "kz/option/menu/model.h"
#include "kz/option/kz_option.h"

#include "tier0/memdbgon.h"

namespace KZ::menu
{
	static std::vector<KZOptNode *> g_tree;

	const std::vector<KZOptNode *> &GetTree()
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

	void SetItemPref(KZOptNode *node, const char *prefKey, KZOptStorage storage)
	{
		if (!node->items.empty())
		{
			node->items.back().prefKey = prefKey;
			node->items.back().storage = storage;
		}
	}

	void SetItemUnit(KZOptNode *node, const char *unit)
	{
		if (!node->items.empty())
		{
			node->items.back().unit = unit;
		}
	}

	void SetItemEnabledBy(KZOptNode *node, const char *prefKey)
	{
		if (node->items.empty())
		{
			return;
		}
		KZOptItem &item = node->items.back();
		for (i32 i = 0; i < KZ_ARRAYSIZE(item.enabledBy); i++)
		{
			if (!item.enabledBy[i])
			{
				item.enabledBy[i] = prefKey;
				return;
			}
		}
	}

	void SetItemScale(KZOptNode *node, i32 scale)
	{
		if (!node->items.empty())
		{
			node->items.back().scale = scale;
		}
	}

	void SetItemSolidOnly(KZOptNode *node)
	{
		if (!node->items.empty())
		{
			node->items.back().solidOnly = true;
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
		item.storage = KZOptStorage::Bool;
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
		item.storage = KZOptStorage::Int;
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
		item.storage = KZOptStorage::Str;
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
		item.storage = KZOptStorage::Float;
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
		item.storage = KZOptStorage::Float;
		item.unit = "px";
		item.scale = 1;
		item.prefKey = prefKey;
		item.lo = lo;
		item.hi = hi;
		item.idef = def;
		item.tag = tag;
		item.onEdit = onEdit;
		node->items.push_back(item);
	}

	void AddVector(KZOptNode *node, const char *phraseKey, const char *prefKey, const Vector &def, i32 lo, i32 hi, i64 tag,
				   void (*onEdit)(KZPlayer *, i64, bool))
	{
		KZOptItem item;
		item.phraseKey = phraseKey;
		item.type = KZOptItemType::Vector;
		item.storage = KZOptStorage::Vector;
		item.prefKey = prefKey;
		item.lo = lo;
		item.hi = hi;
		item.idef = (i32)def.x;
		item.iydef = (i32)def.y;
		item.izdef = (i32)def.z;
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

	void ResetNode(KZPlayer *player, KZOptNode *node)
	{
		if (!node)
		{
			return;
		}
		auto *opts = player->optionService;
		for (const KZOptItem &item : node->items)
		{
			if (!item.prefKey)
			{
				continue;
			}
			switch (item.type)
			{
				case KZOptItemType::Toggle:
					opts->SetPreferenceBool(item.prefKey, item.idef != 0);
					break;
				case KZOptItemType::Color:
					opts->SetPreferenceColor(item.prefKey, item.cdef);
					break;
				case KZOptItemType::Font:
					opts->SetPreferenceStr(item.prefKey, item.sdef);
					break;
				case KZOptItemType::Position:
					opts->SetPreferenceFloat(item.prefKey, (f32)item.idef);
					opts->SetPreferenceFloat(item.yKey, (f32)item.iydef);
					break;
				case KZOptItemType::Vector:
					opts->SetPreferenceVector(item.prefKey, Vector((f32)item.idef, (f32)item.iydef, (f32)item.izdef));
					break;
				case KZOptItemType::Size:
					if (item.storage == KZOptStorage::Int)
					{
						opts->SetPreferenceInt(item.prefKey, item.idef);
					}
					else
					{
						opts->SetPreferenceFloat(item.prefKey, (f64)item.idef / MAX(1, item.scale));
					}
					break;
				default:
					break;
			}
		}
	}
} // namespace KZ::menu
