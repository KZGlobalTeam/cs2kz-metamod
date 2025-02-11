#pragma once

#undef snprintf
#include <steam/steam_gameserver.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.h"

namespace HTTP
{
	class Response;
	class InFlightRequest;

	typedef std::unordered_map<std::string, std::string> HeaderMap;
	typedef std::unordered_map<std::string, std::string> QueryParameters;
	typedef std::function<void(Response)> ResponseCallback;

	extern std::vector<InFlightRequest *> g_InFlightRequests;

	// HTTP methods we care about.
	enum class Method
	{
		GET,
		POST,
		PUT,
		PATCH,
	};

	// An HTTP request.
	class Request
	{
	public:
		Request(Method method, std::string url) : method(method), url(url) {}

		// Set a query parameter.
		void SetQuery(std::string key, std::string value);

		// Set a header.
		void SetHeader(std::string name, std::string value);

		// Set the request body.
		void SetBody(std::string body);

		// Send the request.
		void Send(ResponseCallback onResponse) const;

	private:
		const Method method;
		std::string url {};
		bool hasQueryParams {};
		HeaderMap headers {};
		std::string body {};
	};

	// An HTTP response.
	class Response
	{
	public:
		const u16 status;

		Response(u16 status, HTTPRequestHandle requestHandle) : status(status), requestHandle(requestHandle) {}

		// Retrieves a header if it exists.
		std::optional<std::string> Header(const char *name) const;

		// Extracts the body if it exists.
		std::optional<std::string> Body() const;

	private:
		HTTPRequestHandle requestHandle;
	};

	class InFlightRequest
	{
	public:
		InFlightRequest(const InFlightRequest &req) = delete;

		InFlightRequest(HTTPRequestHandle handle, SteamAPICall_t steamCallHandle, std::string url, std::string body, ResponseCallback onResponse)
			: url(url), body(body), handle(handle), onResponse(onResponse)
		{
			callResult.SetGameserverFlag();
			callResult.Set(steamCallHandle, this, &InFlightRequest::OnRequestCompleted);
			g_InFlightRequests.push_back(this);
		}

		~InFlightRequest()
		{
			for (auto req = g_InFlightRequests.begin(); req != g_InFlightRequests.end(); req++)
			{
				if (*req == this)
				{
					g_InFlightRequests.erase(req);
					break;
				}
			}
		}

	private:
		std::string url;
		std::string body;
		HTTPRequestHandle handle;
		CCallResult<InFlightRequest, HTTPRequestCompleted_t> callResult;
		ResponseCallback onResponse;

		void OnRequestCompleted(HTTPRequestCompleted_t *completedRequest, bool failed);
	};
} // namespace HTTP
