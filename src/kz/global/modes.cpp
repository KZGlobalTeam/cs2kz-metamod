#include "modes.h"
#include <string>

namespace KZ::API
{
	std::optional<KZ::API::ParseError> DeserializeMode(const json &json, Mode &mode)
	{
		if (!json.is_string())
		{
			return KZ::API::ParseError("mode is not a string");
		}

		std::string value = json;

		if (value == "vanilla")
		{
			mode = Mode::VANILLA;
		}
		else if (value == "classic")
		{
			mode = Mode::CLASSIC;
		}
		else
		{
			return KZ::API::ParseError("mode has unknown value");
		}

		return std::nullopt;
	}
} // namespace KZ::API
