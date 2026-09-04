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
	};

	// Built from KZ::menu::GetTree() on first use, so registration order does not matter.
	const std::vector<Entry> &GetRegistry();
	const Entry *FindEntry(const char *key);

	// One console token, quoted where needed. False when the player never set it.
	bool ReadValue(KZPlayer *player, const Entry &entry, char *out, i32 outLen);

	bool ApplyValue(KZPlayer *player, const Entry &entry, const char *value);

	void Init();
	void RegisterMenu(KZOptNode *node);
	void OnClientDisconnect(CPlayerSlot slot);
} // namespace KZ::prefs
