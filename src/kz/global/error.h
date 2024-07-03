#pragma once

#include <optional>
#include <string>
#include "common.h"
#include "utils/json.h"

namespace KZ::API
{
	/// An error object returned by the API.
	struct Error
	{
		/// The HTTP status code returned alongside the error.
		u16 status;

		/// The error message.
		std::string message;

		/// Additional details that may or may not exist.
		json details;

		/// Parses an error from a response.
		///
		/// If `message` is JSON, it will be deserialized.
		Error(u16 status, std::string message);
	};

	/// An error that occurred while parsing JSON.
	struct ParseError
	{
		/// The reason we failed.
		std::string reason;

		ParseError(std::string reason) : reason(reason) {}
	};
} // namespace KZ::API
