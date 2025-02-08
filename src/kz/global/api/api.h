#pragma once

namespace KZ::API
{
	enum class Mode : u8
	{
		Vanilla = 1,
		Classic = 2,
	};

	static bool DecodeModeString(std::string_view modeString, Mode &mode)
	{
		if (KZ_STREQI(modeString.data(), "vanilla") || KZ_STREQI(modeString.data(), "vnl"))
		{
			mode = Mode::Vanilla;
			return true;
		}
		else if (KZ_STREQI(modeString.data(), "classic") || KZ_STREQI(modeString.data(), "ckz"))
		{
			mode = Mode::Classic;
			return true;
		}
		else
		{
			return false;
		}
	}
} // namespace KZ::API
