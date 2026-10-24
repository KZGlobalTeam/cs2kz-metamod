#include "kz/hud/layout/layout.h"
#include "kz/language/kz_language.h"
#include "kz/racing/kz_racing.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

// Each row's panels, from the row itself to its value, for the course, mode, format, teleport limit, time and personal best.
static_global const char *const RACE_ROWS[MHUD_RACE_ROW_COUNT][3] = {
	{"mhud_race_r0", "mhud_race_l0", "mhud_race_v0"}, {"mhud_race_r1", "mhud_race_l1", "mhud_race_v1"},
	{"mhud_race_r2", "mhud_race_l2", "mhud_race_v2"}, {"mhud_race_r3", "mhud_race_l3", "mhud_race_v3"},
	{"mhud_race_r4", "mhud_race_l4", "mhud_race_v4"}, {"mhud_race_r5", "mhud_race_l5", "mhud_race_v5"},
};

// Slots in LayoutRaceState::phrases.
#define RACE_PHRASE_TITLE  0
#define RACE_PHRASE_STATUS 1
#define RACE_PHRASE_LABELS 2
#define RACE_PHRASE_VALUES (RACE_PHRASE_LABELS + MHUD_RACE_ROW_COUNT)

static_function void GetRaceSample(KZRacingService::HudInfo &out)
{
	out.show = true;
	out.course = "Main";
	out.mode = "CKZ";
	out.formatPhrase = "Racing HUD - Format Fastest Time";
	out.maxTeleports = 99;
	out.timeLimit = 3599;
	out.statusPhrase = "Racing HUD - Accepted";
	out.statusClass = "race-accepted";
}

bool KZHUDService::CanShowRaceHud()
{
	return this->IsUsingLayoutStyle() && this->IsShowingPanel() && !this->IsEditingHud();
}

void KZHUDService::UpdateRaceElements(CCSCustomHudLayout *layout, bool showPanel, bool showCountdown, bool force, bool preview)
{
	LayoutRaceState &race = this->layoutRace;
	if (force)
	{
		race = LayoutRaceState();
	}
	KZRacingService::HudInfo info;
	if (preview)
	{
		GetRaceSample(info);
	}
	else if (showPanel || showCountdown)
	{
		KZRacingService::GetHudInfo(this->player, info);
	}
	// A preview has to be replaced in full once the panel shows for real.
	if (race.preview != preview)
	{
		race.preview = preview;
		for (i32 i = 0; i < MHUD_RACE_ROW_COUNT; i++)
		{
			race.values[i].clear();
			race.phrases[RACE_PHRASE_VALUES + i] = NULL;
		}
	}
	this->UpdateLayoutElement(layout, MHUDElement::Race, showPanel && info.show, NULL, MHUD_DEF_BASE_COLOR, force);

	auto setHidden = [&](const char *panelId, i32 &cache, bool hidden)
	{
		if (cache != (i32)hidden)
		{
			cache = hidden;
			layout->SetHasClass(panelId, "hidden", hidden ? k_eHudPanelClassStatus_HasClass : k_eHudPanelClassStatus_DoesNotHaveClass);
		}
	};
	setHidden("mhud_race_countdown", race.countdownHidden, !showCountdown || !info.countdown);
	if (!info.show)
	{
		return;
	}

	KZLanguageService *lang = this->player->languageService;
	if (race.language != lang->GetLanguage())
	{
		race.language = lang->GetLanguage();
		for (const char *&phrase : race.phrases)
		{
			phrase = NULL;
		}
		race.countdownTenths = INT_MIN;
	}

	if (showCountdown && info.countdown)
	{
		const i32 tenths = info.countdownSeconds > 0 ? RoundFloatToInt(info.countdownSeconds * 10.0f) : -1;
		if (race.countdownTenths != tenths)
		{
			race.countdownTenths = tenths;
			const std::string text =
				tenths >= 0 ? lang->PrepareMessage("Racing - Race Starting In", info.countdownSeconds) : lang->PrepareMessage("Racing - Go!");
			layout->SetDialogVariableString("mhud_race_countdown", "v", text.c_str());
		}
	}
	if (!showPanel)
	{
		return;
	}

	// js-scale.css scales the whole panel by the element's size in percent.
	const i32 scale = Clamp((i32)this->GetLayoutSize(MHUDElement::Race), MHUD_JS_SIZE_MIN, MHUD_JS_SIZE_MAX);
	if (race.scale != scale)
	{
		const char *panelId = MHUD_ELEMENTS[(i32)MHUDElement::Race].panelId;
		char className[32];
		if (race.scale != INT_MIN)
		{
			V_snprintf(className, sizeof(className), "js-scale--%i", race.scale);
			layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_DoesNotHaveClass);
		}
		V_snprintf(className, sizeof(className), "js-scale--%i", scale);
		layout->SetHasClass(panelId, className, k_eHudPanelClassStatus_HasClass);
		race.scale = scale;
	}

	// A label only picks up a new font when it is restyled itself, so the class goes on every label.
	const char *fontClass = KZHUDService::GetMHUDFontClass(this->player, MHUDElement::Race);
	if (race.fontClass != fontClass)
	{
		auto setFont = [&](const char *panelId)
		{
			if (race.fontClass)
			{
				layout->SetHasClass(panelId, race.fontClass, k_eHudPanelClassStatus_DoesNotHaveClass);
			}
			layout->SetHasClass(panelId, fontClass, k_eHudPanelClassStatus_HasClass);
		};
		setFont("mhud_race_title");
		setFont("mhud_race_status");
		for (const auto &row : RACE_ROWS)
		{
			setFont(row[1]);
			setFont(row[2]);
		}
		race.fontClass = fontClass;
	}

	auto setPhrase = [&](i32 slot, const char *panelId, const char *phrase)
	{
		if (race.phrases[slot] != phrase)
		{
			race.phrases[slot] = phrase;
			layout->SetDialogVariableString(panelId, "v", phrase ? lang->PrepareMessage(phrase).c_str() : "");
		}
	};
	setPhrase(RACE_PHRASE_TITLE, "mhud_race_title", "Racing HUD - Title");
	setPhrase(RACE_PHRASE_STATUS, "mhud_race_status", info.statusPhrase);
	this->SetLayoutClass(layout, "mhud_race_status", race.statusClass, info.statusClass);

	char teleports[16] {};
	char time[64] {};
	char pb[32] {};
	if (info.maxTeleports >= 0)
	{
		V_snprintf(teleports, sizeof(teleports), "%i", info.maxTeleports);
	}
	if (info.showTimeLeft)
	{
		char left[32], limit[32];
		utils::FormatTime(info.timeLeft, left, sizeof(left), false);
		utils::FormatTime(info.timeLimit, limit, sizeof(limit), false);
		V_snprintf(time, sizeof(time), "%s / %s", left, limit);
	}
	else if (info.timeLimit > 0)
	{
		utils::FormatTime(info.timeLimit, time, sizeof(time), false);
	}
	if (info.pb > 0)
	{
		utils::FormatTime(info.pb, pb, sizeof(pb));
	}

	struct Row
	{
		const char *phrase;
		const char *value;
		// Used instead of the value when there is none.
		const char *valuePhrase;
		bool shown;
	};

	// The course and mode are known once the race has started, and the time limit is part of the rules, so before the start it
	// shows even when there is none.
	const Row rows[MHUD_RACE_ROW_COUNT] = {
		{"Racing HUD - Course", info.course, NULL, !info.started},
		{"Racing HUD - Mode", info.mode, NULL, !info.started},
		{"Racing HUD - Format", NULL, info.formatPhrase, true},
		{"Racing HUD - Teleport Limit", teleports, NULL, info.maxTeleports >= 0},
		{info.showTimeLeft ? "Racing HUD - Time Left" : "Racing HUD - Time Limit", time[0] ? time : NULL, "Racing HUD - None",
		 time[0] || !info.started},
		{info.pro ? "Racing HUD - Pro PB" : "Racing HUD - PB", pb, NULL, info.pb > 0},
	};
	for (i32 i = 0; i < MHUD_RACE_ROW_COUNT; i++)
	{
		setHidden(RACE_ROWS[i][0], race.rowHidden[i], !rows[i].shown);
		if (!rows[i].shown)
		{
			continue;
		}
		setPhrase(RACE_PHRASE_LABELS + i, RACE_ROWS[i][1], rows[i].phrase);
		// Each cache is cleared while the other one is in use, so switching between them always writes.
		if (rows[i].value)
		{
			race.phrases[RACE_PHRASE_VALUES + i] = NULL;
			if (race.values[i] != rows[i].value)
			{
				race.values[i] = rows[i].value;
				layout->SetDialogVariableString(RACE_ROWS[i][2], "v", rows[i].value);
			}
		}
		else
		{
			race.values[i].clear();
			setPhrase(RACE_PHRASE_VALUES + i, RACE_ROWS[i][2], rows[i].valuePhrase);
		}
	}
}
