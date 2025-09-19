#pragma once

#include "common.h"
#include "utils/schema.h"

enum EInButtonState : uint64_t
{
	IN_BUTTON_UP = 0x0,
	IN_BUTTON_DOWN = 0x1,
	IN_BUTTON_DOWN_UP = 0x2,
	IN_BUTTON_UP_DOWN = 0x3,
	IN_BUTTON_UP_DOWN_UP = 0x4,
	IN_BUTTON_DOWN_UP_DOWN = 0x5,
	IN_BUTTON_DOWN_UP_DOWN_UP = 0x6,
	IN_BUTTON_UP_DOWN_UP_DOWN = 0x7,
	IN_BUTTON_STATE_COUNT = 0x8,
};

class CInButtonState
{
	DECLARE_SCHEMA_CLASS_ENTITY(CInButtonState);

public:
	SCHEMA_FIELD_POINTER(uint64, m_pButtonStates);

	void GetButtons(uint64 buttons[3])
	{
		for (int i = 0; i < 3; i++)
		{
			buttons[i] = this->m_pButtonStates[i];
		}
	}

	EInButtonState GetButtonState(uint64 button)
	{
		return (EInButtonState)(!!(this->m_pButtonStates[0] & button) + !!(this->m_pButtonStates[1] & button) * 2
								+ !!(this->m_pButtonStates[2] & button) * 4);
	};

	bool IsButtonNewlyPressed(uint64 button)
	{
		return this->GetButtonState(button) >= 3;
	}

	static bool IsButtonPressed(uint64 buttons[3], uint64 button, bool onlyDown = false)
	{
		if (onlyDown)
		{
			return buttons[0] & button;
		}
		else
		{
			bool multipleKeys = (button & (button - 1));
			if (multipleKeys)
			{
				u64 currentButton = button;
				u64 key = 0;
				if (button)
				{
					while (true)
					{
						if (currentButton & 1)
						{
							u64 keyMask = 1ull << key;
							EInButtonState keyState =
								(EInButtonState)(keyMask && buttons[0] + (keyMask && buttons[1]) * 2 + (keyMask && buttons[2]) * 4);
							if (keyState > IN_BUTTON_DOWN_UP)
							{
								return true;
							}
						}
						key++;
						currentButton >>= 1;
						if (!currentButton)
						{
							return !!(buttons[0] & button);
						}
					}
				}
				return false;
			}
			else
			{
				EInButtonState keyState = (EInButtonState)(!!(button & buttons[0]) + !!(button & buttons[1]) * 2 + !!(button & buttons[2]) * 4);
				if (keyState > IN_BUTTON_DOWN_UP)
				{
					return true;
				}
				return !!(buttons[0] & button);
			}
		}
	}

	bool IsButtonPressed(uint64 button, bool onlyDown)
	{
		return CInButtonState::IsButtonPressed(this->m_pButtonStates, button, onlyDown);
	}
};
