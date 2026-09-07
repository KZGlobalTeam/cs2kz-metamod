#pragma once
#include "kz/kz.h"
#include "kz/option/menu/model.h"

#include <vector>

namespace KZ::prefs
{
	struct Entry
	{
		const char *key {};
		KZOptStorage storage {};
		const KZOptItem *item {}; // where the default comes from; stable once every service has registered
		bool isY {};              // Position: this entry is the y key, not the x key
	};

	// Built from KZ::menu::GetTree() on first use, so registration order does not matter.
	const std::vector<Entry> &GetRegistry();
	const Entry *FindEntry(const char *key);

	// One console token, quoted where needed. Falls back to the registered default when the player
	// never set it; false when there is no default to fall back to either.
	bool ReadValue(KZPlayer *player, const Entry &entry, char *out, i32 outLen);

	bool ApplyValue(KZPlayer *player, const Entry &entry, const char *value);

	void Init();
	void RegisterMenu(KZOptNode *node);
	void OnClientDisconnect(CPlayerSlot slot);
} // namespace KZ::prefs
