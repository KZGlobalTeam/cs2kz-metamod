#pragma once
#include "kz/kz.h"

#include <string>
#include <unordered_map>

class CCSCustomHudLayout;
class CCheckTransmitInfo;

// The appearance every window shares, set from the options menu's own preferences.
#define KZ_UI_DEFAULT_FONT  "stratum2-medium-tf"
#define KZ_UI_DEFAULT_COLOR Color(255, 255, 255, 255)

namespace KZ::ui
{
	// A custom_hud_layout entity owned by one player and masked away from every other client.
	class PlayerLayout
	{
	public:
		PlayerLayout(KZPlayer *owner, const char *layoutPath, const char *namePrefix);
		~PlayerLayout();

		PlayerLayout(const PlayerLayout &) = delete;
		PlayerLayout &operator=(const PlayerLayout &) = delete;

		// Spawns the entity if it does not exist yet. `created` is set when this call spawned it, so the caller can reset its
		// own caches.
		CCSCustomHudLayout *Ensure(bool &created);
		CCSCustomHudLayout *Get();
		void Destroy();

		bool Owns(CCSCustomHudLayout *layout);
		void SetInputCapture(bool enabled);

		// Skips a value that is already set. Only the first write of a variable marks the whole entity changed; later ones mark
		// just their own entry.
		void SetVar(const char *panelId, const char *var, const char *value);

		void ClearVarCache()
		{
			this->writtenVars.clear();
		}

		static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
		static void DestroyAll();

	private:
		KZPlayer *owner;
		const char *layoutPath;
		const char *namePrefix;
		CHandle<CBaseEntity> entity {};
		std::unordered_map<std::string, std::string> writtenVars;
	};

	void SetClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool on);
	void SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want);
	// Swaps the class that cache holds for want. NULL stands for no class.
	void SetSwapClass(CCSCustomHudLayout *layout, const char *panelId, const char *&cache, const char *want);

	struct WindowStyle
	{
		const char *font {};
		const char *color {};
		bool fontReflow {};
		bool sounds {};
	};

	// Applies the player's window font, color and sound preferences to a window's root panel.
	void ApplyWindowStyle(KZPlayer *player, CCSCustomHudLayout *layout, const char *rootPanelId, WindowStyle &cache);
} // namespace KZ::ui
