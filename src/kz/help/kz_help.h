#pragma once
#include "kz/kz.h"

#include <string>
#include <unordered_map>

class CCSCustomHudLayout;
class CCheckTransmitInfo;

// Fixed slot counts, kept in step with help.xml.
#define KZ_HELP_CATEGORIES 20
#define KZ_HELP_ROWS       32

class KZHelpService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	~KZHelpService() override
	{
		this->Reset();
	}

	// Returns false when the UI is unavailable; the command then prints console help.
	bool Show(i32 category = 0);
	void Close();
	virtual void Reset() override;
	void OnClientDisconnect();

	static void OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId);
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
	static void Cleanup();

private:
	CCSCustomHudLayout *EnsureLayout();
	void DestroyLayout();
	void Render();
	void SetBoolClass(CCSCustomHudLayout *layout, const char *panelId, const char *className, bool &cache, bool want);
	void SetSwapClass(CCSCustomHudLayout *layout, const char *&cache, const char *want);
	void SetVar(CCSCustomHudLayout *layout, const char *panelId, const char *var, const char *value);

	bool open {};
	i32 category {};
	i32 page {};
	CHandle<CBaseEntity> layoutEntity {};
	std::unordered_map<std::string, std::string> writtenVars;

	struct Applied
	{
		const char *font {};
		const char *color {};
		bool fontReflow {};
		bool sounds {};
		bool emptyHidden {true};
		bool previousDisabled {};
		bool nextDisabled {};
		bool categoryHidden[KZ_HELP_CATEGORIES] {};
		bool categorySelected[KZ_HELP_CATEGORIES] {};
		bool rowHidden[KZ_HELP_ROWS] {};
		bool descriptionHidden[KZ_HELP_ROWS] {};

		Applied()
		{
			for (bool &hidden : this->categoryHidden)
			{
				hidden = true;
			}
			for (bool &hidden : this->rowHidden)
			{
				hidden = true;
			}
		}
	} applied;
};
