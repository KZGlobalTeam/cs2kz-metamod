#pragma once

#include <optional>
#include "error.h"
#include "utils/json.h"

namespace KZ::API
{
	// The two official KZ modes.
	enum class Mode
	{
		VANILLA,
		CLASSIC,
	};

	// Deserializes a `Mode` from a JSON value.
	std::optional<KZ::API::ParseError> DeserializeMode(const json &json, Mode &mode);
} // namespace KZ::API
