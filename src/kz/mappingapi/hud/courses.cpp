#include "kz/mappingapi/hud/courses.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/global/kz_global.h"
#include "kz/hud/kz_hud.h"
#include "kz/language/kz_language.h"
#include "kz/mode/kz_mode.h"
#include "kz/timer/kz_timer.h"
#include "sdk/entity/ccscustomhudlayout.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

bool CoursesWindow::CanOpen()
{
	if (this->player->hudService->IsEditingHud() || !KZHUDService::IsLayoutHudAvailable())
	{
		return false;
	}
	bool created = false;
	CCSCustomHudLayout *entity = this->layout.Ensure(created);
	if (created)
	{
		this->applied = Applied();
		this->chips.Reset();
	}
	return entity && entity->GetPlayerLayoutState(this->player->GetPlayerSlot());
}

bool CoursesWindow::IsShown()
{
	return this->player->uiService->GetTop() == this && this->layout.Get();
}

bool CoursesWindow::Toggle()
{
	if (this->IsShown())
	{
		this->player->uiService->Close(this);
		return true;
	}
	return this->player->uiService->Open(this);
}

void CoursesWindow::OnOpen()
{
	this->Render();
}

void CoursesWindow::Hide()
{
	if (CCSCustomHudLayout *entity = this->layout.Get())
	{
		KZ::ui::SetBoolClass(entity, "courses_root", "hidden", this->applied.rootHidden, true);
	}
}

void CoursesWindow::OnClose(KZ::ui::CloseReason reason)
{
	this->Hide();
}

void CoursesWindow::OnSuspend()
{
	this->Hide();
}

void CoursesWindow::OnResume()
{
	this->Render();
}

void CoursesWindow::Render()
{
	CCSCustomHudLayout *entity = this->layout.Get();
	if (!entity || this->player->uiService->GetTop() != this)
	{
		return;
	}
	auto *language = this->player->languageService;
	KZ::ui::SetBoolClass(entity, "courses_root", "hidden", this->applied.rootHidden, false);
	KZ::ui::ApplyWindowStyle(this->player, entity, "courses_root", this->applied.style);

	// The chips follow the player's mode and the hint names it because tiers and ranked states differ per mode.
	// Any other mode falls back to CKZ because the API only rates VNL and CKZ.
	const KZModeManager::ModePluginInfo modeInfo = KZ::mode::GetModeInfo(this->player->modeService);
	const bool classic = modeInfo.id != KZ::mode::GetModeInfo(KZ::api::Mode::Vanilla).id;

	this->layout.SetVar("courses_window_title", "courses_window_title", g_pKZUtils->GetCurrentMapName().c_str());
	this->layout.SetVar("courses_credit_label", "courses_credit_label", language->PrepareMessage("Courses - Made By").c_str());
	const char *dataModeName = classic ? "CKZ" : modeInfo.shortModeName.c_str();
	this->layout.SetVar("courses_hint", "courses_hint", language->PrepareMessage("Courses - Hint", dataModeName).c_str());
	const KZCourseDescriptor *current = this->player->timerService->GetCourse();
	this->chips.Render(this->player, this->layout, entity, classic ? KZ::api::Mode::Classic : KZ::api::Mode::Vanilla, false, current);

	// The same map details /mapinfo prints in the console.
	// The credit names the map's mappers if the player isn't on a course.
	bool hasMap = false;
	const char *stateClass = nullptr;
	std::string state;
	std::string workshop;
	std::string mappers;
	std::string description;
	KZGlobalService::WithCurrentMap(
		[&](const std::optional<KZ::api::Map> &currentMap)
		{
			if (!currentMap)
			{
				return;
			}
			hasMap = true;
			auto joinNames = [](const std::vector<KZ::api::PlayerInfo> &players)
			{
				std::string names;
				for (u32 m = 0; m < players.size(); m++)
				{
					names += m > 0 ? ", " : "";
					names += players[m].name;
				}
				return names;
			};
			// Workshop IDs exceed 2^31 so they are passed to the phrase as text.
			const std::string workshopId = std::to_string(currentMap->workshopId);
			switch (currentMap->state)
			{
				case KZ::api::Map::State::Approved:
				{
					stateClass = "state-approved";
					state = language->PrepareMessage("Courses - State Approved");
					// approved_at is an RFC 3339 timestamp and only the date is shown.
					const std::string day = currentMap->approvedAt.substr(0, 10);
					workshop = language->PrepareMessage("Courses - Workshop Approved", workshopId.c_str(), day.c_str());
					break;
				}
				case KZ::api::Map::State::InTesting:
				{
					stateClass = "state-testing";
					state = language->PrepareMessage("Map Info - In Testing");
					workshop = language->PrepareMessage("Courses - Workshop", workshopId.c_str());
					break;
				}
				default:
				{
					stateClass = "state-invalid";
					state = language->PrepareMessage("Courses - State Invalid");
					workshop = language->PrepareMessage("Courses - Workshop", workshopId.c_str());
					break;
				}
			}
			description = currentMap->description.value_or("");
			mappers = joinNames(currentMap->mappers);
			if (!current)
			{
				return;
			}
			for (const KZ::api::Map::Course &course : currentMap->courses)
			{
				if (course.id == current->globalDatabaseID)
				{
					mappers = joinNames(course.mappers);
				}
			}
		});
	KZ::ui::SetBoolClass(entity, "courses_meta", "hidden", this->applied.metaHidden, !hasMap);
	KZ::ui::SetSwapClass(entity, "courses_state", this->applied.stateClass, stateClass);
	this->layout.SetVar("courses_state", "courses_state", state.c_str());
	this->layout.SetVar("courses_workshop", "courses_workshop", workshop.c_str());
	KZ::ui::SetBoolClass(entity, "courses_credit", "hidden", this->applied.creditHidden, mappers.empty());
	this->layout.SetVar("courses_credit_names", "courses_credit_names", mappers.c_str());
	KZ::ui::SetBoolClass(entity, "courses_description", "hidden", this->applied.descriptionHidden, description.empty());
	this->layout.SetVar("courses_description_label", "courses_description_label", language->PrepareMessage("Courses - Description").c_str());
	this->layout.SetVar("courses_description_text", "courses_description_text", description.c_str());
}

void CoursesWindow::OnClick(const char *buttonId)
{
	if (KZ_STREQ(buttonId, "courses_window_close"))
	{
		this->player->uiService->Close(this);
		return;
	}
	const KZCourseDescriptor *course = this->chips.GetClickedCourse(buttonId);
	if (!course)
	{
		return;
	}
	if (!course->hasStartPosition)
	{
		this->player->languageService->PrintChat(true, false, "No Start Position For Course", course->name);
		return;
	}
	this->player->uiService->Close(this);
	KZ::misc::TeleportToCourse(this->player, course);
}
