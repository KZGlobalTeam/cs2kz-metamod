#include "error.h"
#include "kz/language/kz_language.h"

namespace KZ::API
{
	Error::Error(u16 status, std::string message) : status(status)
	{
		if (!json::accept(message))
		{
			this->message = message;
			return;
		}

		const json error = json::parse(message);

		if (!error.is_object())
		{
			META_CONPRINTF("[KZ::Global] API error is not an object: `%s`\n", error.dump().c_str());
			return;
		}

		if (!error.contains("message"))
		{
			META_CONPRINTF("[KZ::Global] API error does not contain a message: `%s`\n", error.dump().c_str());
			return;
		}

		if (!error["message"].is_string())
		{
			META_CONPRINTF("[KZ::Global] API error message is not a string: `%s`\n", error.dump().c_str());
			return;
		}

		this->message = error["message"];

		if (error.contains("details"))
		{
			this->details = error["details"];
		}
	}
} // namespace KZ::API
