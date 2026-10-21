# UI windows

`KZUIService` (`player->uiService`) keeps each player's stack of open windows. Only the window on top captures
the cursor and receives clicks. The service also owns the cs2menus busy flag, so a window never touches
`g_pMenus`, `SetInputCaptureEnabled` or the transmit hooks itself.

## Writing a window

A window derives from `KZ::ui::Window` and owns a `KZ::ui::PlayerLayout`, which is the player's own
`custom_hud_layout` entity. The entity is spawned on demand, hidden from every other client, and destroyed on
disconnect and unload.

```cpp
#include "kz/ui/kz_ui.h"
#include "kz/ui/player_layout.h"

class FooWindow : public KZ::ui::Window
{
public:
	FooWindow(KZPlayer *player) : player(player) {}

	virtual KZ::ui::PlayerLayout *GetLayout() override
	{
		return &this->layout;
	}

	virtual bool CanOpen() override
	{
		bool created = false;
		CCSCustomHudLayout *entity = KZHUDService::IsLayoutHudAvailable() ? this->layout.Ensure(created) : NULL;
		if (created)
		{
			this->applied = Applied();
		}
		return entity && entity->GetPlayerLayoutState(this->player->GetPlayerSlot());
	}

	virtual void OnOpen() override;                        // set up state and render
	virtual void OnClose(KZ::ui::CloseReason) override;    // hide the root panel
	virtual void OnClick(const char *buttonId) override;   // buttonId is the clicked Button's panel id

private:
	KZPlayer *player;
	KZ::ui::PlayerLayout layout {this->player, "panorama/layout/custom_game/cs2kz/foo.xml", "kzfoo"};
};
```

The layout and its styles go in `workshop/panorama/layout/custom_game/cs2kz/` and
`workshop/panorama/styles/custom_game/cs2kz/`.

## Opening and closing

```cpp
KZUIService *ui = player->uiService;
FooWindow *foo = ui->GetWindow<FooWindow>();   // created on first use, freed when the player leaves
ui->Toggle(foo);                               // close if on top, bring back if suspended, open otherwise
ui->Open(foo, KZ::ui::OpenMode::Push);         // keep the current window underneath, resumed when foo closes
ui->Close(foo);                                // also closes anything above it
```

`OpenMode::Replace` (the default) closes every open window first. Use `Push` for a window the player should come
back from, such as HUD edit started from the options menu.

A window that belongs to an existing service can be a base of that service instead of going through
`GetWindow`, as `KZMenuService` is. `KZHUDService::HudEditWindow` shows a window that borrows another layout
(the MHUD's) instead of owning one.

## Callbacks

- `CanOpen` runs before anything else changes. Return false, and print why if useful, to leave the open windows as
  they were.
- `OnOpen` runs once the window is on top and has the cursor.
- `OnSuspend` / `OnResume` run when a pushed window covers it and when that window closes. Hide on suspend and
  keep state, so resume only has to show and re-render.
- `OnClose` gets a `CloseReason`: `Closed`, `Replaced`, `Disconnect` or `Unload`. Use `layout.Get()` here, not
  `Ensure`, because `Ensure` refuses to spawn while the plugin is unloading.
- It is safe to open or close windows from inside a callback. The stack is updated before callbacks run.

## Writing to the layout

The first write of a variable or class (and any new panel id or name) adds an entry and marks the whole entity
changed. After that, a write marks only its own entry. Skip writes that change nothing:

- `layout.SetVar(panelId, var, value)` skips a value that is already set.
- `KZ::ui::SetBoolClass` / `SetSwapClass` take a cache field the window keeps (see `KZMenuService::Applied`).
  Reset those caches whenever `Ensure` reports `created`.
- `KZ::ui::ApplyWindowStyle(player, entity, rootPanelId, style)` applies the shared font, colour and sound
  preferences to the window's root panel.

For async work (API queries), store `GetOpenId()` when the request goes out. When the result arrives, drop it
unless the window is still open and the id hasn't changed.
