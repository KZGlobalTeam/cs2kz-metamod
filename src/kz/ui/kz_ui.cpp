#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"
#include "sdk/entity/ccscustomhudlayout.h"

#include <algorithm>
#include <iterator>

#include <vendor/mm-cs2menus/src/public/ics2menus.h>
extern ICS2Menus *g_pMenus;

#include "tier0/memdbgon.h"

using namespace KZ::ui;

bool KZUIService::Open(Window *window, OpenMode mode)
{
	this->PruneStale();
	auto it = std::find(this->stack.begin(), this->stack.end(), window);
	if (it != this->stack.end())
	{
		if (it + 1 != this->stack.end())
		{
			this->Close(*(it + 1));
		}
		return true;
	}
	if (!window->CanOpen())
	{
		return false;
	}

	// The stack is settled before any callback runs, so a callback that opens or closes a window acts on the new state.
	std::vector<Window *> closed;
	Window *suspended = nullptr;
	if (mode == OpenMode::Replace)
	{
		closed.assign(this->stack.rbegin(), this->stack.rend());
		this->stack.clear();
	}
	else
	{
		suspended = this->GetTop();
	}
	this->stack.push_back(window);
	window->openId++;

	for (Window *other : closed)
	{
		other->OnClose(CloseReason::Replaced);
	}
	if (suspended)
	{
		suspended->OnSuspend();
	}
	if (g_pMenus)
	{
		g_pMenus->CancelMenu(this->player->GetPlayerSlot().Get());
	}
	this->ApplyInputState(closed);
	if (this->GetTop() == window)
	{
		window->OnOpen();
	}
	return true;
}

void KZUIService::Close(Window *window, CloseReason reason)
{
	auto it = std::find(this->stack.begin(), this->stack.end(), window);
	if (it == this->stack.end())
	{
		return;
	}
	std::vector<Window *> closed(std::make_reverse_iterator(this->stack.end()), std::make_reverse_iterator(it));
	this->stack.erase(it, this->stack.end());
	Window *resumed = this->GetTop();

	for (Window *other : closed)
	{
		other->OnClose(reason);
	}
	this->ApplyInputState(closed);
	if (resumed && this->GetTop() == resumed)
	{
		resumed->OnResume();
	}
}

void KZUIService::Toggle(Window *window, OpenMode mode)
{
	this->PruneStale();
	auto it = std::find(this->stack.begin(), this->stack.end(), window);
	if (it == this->stack.end())
	{
		this->Open(window, mode);
	}
	else if (it + 1 == this->stack.end())
	{
		this->Close(window);
	}
	else
	{
		this->Close(*(it + 1));
	}
}

void KZUIService::CloseAll(CloseReason reason)
{
	if (!this->stack.empty())
	{
		this->Close(this->stack.front(), reason);
	}
}

bool KZUIService::IsOpen(const Window *window) const
{
	return std::find(this->stack.begin(), this->stack.end(), window) != this->stack.end();
}

void KZUIService::PruneStale()
{
	for (size_t i = 0; i < this->stack.size(); i++)
	{
		PlayerLayout *layout = this->stack[i]->GetLayout();
		if (!layout || !layout->Get())
		{
			this->Close(this->stack[i]);
			return;
		}
	}
}

void KZUIService::ApplyInputState(const std::vector<Window *> &touched)
{
	Window *top = this->GetTop();
	PlayerLayout *topLayout = top ? top->GetLayout() : nullptr;
	for (Window *window : touched)
	{
		if (window->GetLayout() != topLayout)
		{
			window->GetLayout()->SetInputCapture(false);
		}
	}
	for (Window *window : this->stack)
	{
		if (window->GetLayout() != topLayout)
		{
			window->GetLayout()->SetInputCapture(false);
		}
	}
	if (topLayout)
	{
		topLayout->SetInputCapture(true);
	}
	if (g_pMenus)
	{
		g_pMenus->SetExternalBusy(this->player->GetPlayerSlot().Get(), !this->stack.empty());
	}
}

void KZUIService::DestroyOwnedWindows()
{
	for (OwnedWindow &owned : this->ownedWindows)
	{
		owned.window->GetLayout()->Destroy();
	}
	this->ownedWindows.clear();
}

void KZUIService::Reset()
{
	this->CloseAll(CloseReason::Disconnect);
	this->DestroyOwnedWindows();
}

void KZUIService::OnClientDisconnect()
{
	this->Reset();
}

void KZUIService::OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(slot);
	Window *top = player && player->uiService ? player->uiService->GetTop() : nullptr;
	if (top && top->GetLayout()->Owns(layout))
	{
		top->OnClick(buttonId);
	}
}

void KZUIService::OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount)
{
	PlayerLayout::OnCheckTransmit(pInfo, infoCount);
}

void KZUIService::Cleanup()
{
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(CPlayerSlot(i));
		if (player && player->uiService)
		{
			player->uiService->CloseAll(CloseReason::Unload);
			player->uiService->DestroyOwnedWindows();
		}
	}
	PlayerLayout::DestroyAll();
}
