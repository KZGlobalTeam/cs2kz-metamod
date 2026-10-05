#pragma once
#include "kz/kz.h"

#include <memory>
#include <vector>

class CCSCustomHudLayout;
class CCheckTransmitInfo;
class KZUIService;

namespace KZ::ui
{
	class PlayerLayout;

	enum class CloseReason
	{
		Closed,
		Replaced,
		Disconnect,
		Unload,
	};

	enum class OpenMode
	{
		// Closes every open window first.
		Replace,
		// Suspends the window on top, which resumes once this one closes.
		Push,
	};

	// Something that takes the player's cursor. Only the window on top of the player's stack captures input and gets
	// clicks.
	class Window
	{
	public:
		virtual ~Window() = default;

		// The layout whose clicks this window handles and whose input capture it holds while on top.
		virtual PlayerLayout *GetLayout() = 0;
		// Spawns what the window needs. Returning false leaves the open windows as they were.
		virtual bool CanOpen() = 0;
		virtual void OnOpen() = 0;
		virtual void OnClose(CloseReason reason) = 0;

		virtual void OnSuspend() {}

		virtual void OnResume() {}

		virtual void OnClick(const char *buttonId) = 0;

		// Bumped on every open, so an async result can tell whether it still belongs to the window it was requested for.
		u32 GetOpenId() const
		{
			return this->openId;
		}

	private:
		friend class ::KZUIService;
		u32 openId {};
	};
} // namespace KZ::ui

// The player's stack of open windows. It alone decides input capture and the cs2menus busy state.
class KZUIService : public KZBaseService
{
	using KZBaseService::KZBaseService;

public:
	virtual void Reset() override;

	bool Open(KZ::ui::Window *window, KZ::ui::OpenMode mode = KZ::ui::OpenMode::Replace);
	// Also closes every window above it.
	void Close(KZ::ui::Window *window, KZ::ui::CloseReason reason = KZ::ui::CloseReason::Closed);
	// Closes the window when it is on top, brings it back to the top when it is suspended, and opens it otherwise.
	void Toggle(KZ::ui::Window *window, KZ::ui::OpenMode mode = KZ::ui::OpenMode::Replace);
	void CloseAll(KZ::ui::CloseReason reason);

	bool IsOpen(const KZ::ui::Window *window) const;

	KZ::ui::Window *GetTop() const
	{
		return this->stack.empty() ? nullptr : this->stack.back();
	}

	// Windows that no service owns, created on first use and destroyed when the player leaves.
	template<class T>
	T *GetWindow()
	{
		const void *key = GetWindowKey<T>();
		for (const OwnedWindow &owned : this->ownedWindows)
		{
			if (owned.key == key)
			{
				return static_cast<T *>(owned.window.get());
			}
		}
		T *window = new T(this->player);
		this->ownedWindows.push_back({key, std::unique_ptr<KZ::ui::Window>(window)});
		return window;
	}

	void OnClientDisconnect();

	static void OnCustomHudClicked(CPlayerSlot slot, CCSCustomHudLayout *layout, const char *buttonId);
	static void OnCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount);
	static void Cleanup();

private:
	template<class T>
	static const void *GetWindowKey()
	{
		static_persist const char key {};
		return &key;
	}

	struct OwnedWindow
	{
		const void *key;
		std::unique_ptr<KZ::ui::Window> window;
	};

	// Drops windows whose layout entity is gone, which happens on a map change.
	void PruneStale();
	// Applies input capture to every layout in touched and on the stack, then updates the cs2menus busy state.
	void ApplyInputState(const std::vector<KZ::ui::Window *> &touched);
	void DestroyOwnedWindows();

	std::vector<KZ::ui::Window *> stack;
	std::vector<OwnedWindow> ownedWindows;
};
