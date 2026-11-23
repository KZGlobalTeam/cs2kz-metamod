#pragma once
#include "kz/kz.h"
#include "utils/utils.h"
#include "utils/simplecmds.h"
#include "tier0/keyvalues.h"
#include "interfaces/interfaces.h"
#include "filesystem/ifilesystem.h"
#include "utils/ctimer.h"
#include "kz/option/kz_option.h"

class KZTipService : public KZBaseService
{
	using KZBaseService::KZBaseService;

private:
	bool teamJoinedAtLeastOnce {};
	// Set if the welcome window should open once the player joins a team.
	bool welcomePending {};

public:
	virtual void Reset() override;
	void ToggleTips();
	static void Init();
	static f64 PrintTips();
	void OnPlayerJoinTeam(i32 team);
	void OnTimerStartPost();
	// Called after the database setup with whether the player has runs on this server.
	// Players without runs or a global rating who haven't seen the welcome window get it.
	void OnLocalRunsChecked(bool hasRuns);
	bool ShowWelcome();

private:
	bool ShouldPrintTip();
	void PrintTip();
	void ShowPendingWelcome();
	static void LoadTips();
	static void ShuffleTips();
};
