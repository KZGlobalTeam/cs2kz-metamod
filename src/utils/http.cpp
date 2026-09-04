#include "http.h"

namespace HTTP
{
	CSteamGameServerAPIContext g_steamAPI;
	ISteamHTTP *g_pHTTP = nullptr;
	std::vector<InFlightRequest *> g_InFlightRequests;

	void Request::SetQuery(std::string key, std::string value)
	{
		url += hasQueryParams ? "&" : "?";
		url += key;
		url += "=";
		url += value;
		hasQueryParams = true;
	}

	void Request::SetHeader(std::string name, std::string value)
	{
		headers[name] = value;
	}

	void Request::SetBody(std::string body)
	{
		this->body = body;
	}

	void Request::Send(ResponseCallback onResponse, ErrorCallback onError) const
	{
		if (!g_pHTTP)
		{
			KZ_LOG_INFO(LogChannel::General, "[HTTP] Initializing HTTP client...\n");
			if (g_steamAPI.Init())
			{
				g_pHTTP = g_steamAPI.SteamHTTP();
			}
			else
			{
				KZ_LOG_WARN(LogChannel::General, "[HTTP] Failed to send HTTP request as the steam API is not yet initialized.\n");
				if (onError)
				{
					onError();
				}
				return;
			}
		}

		EHTTPMethod volvoMethod;

		switch (method)
		{
			case Method::GET:
				volvoMethod = k_EHTTPMethodGET;
				break;
			case Method::POST:
				volvoMethod = k_EHTTPMethodPOST;
				break;
			case Method::PUT:
				volvoMethod = k_EHTTPMethodPUT;
				break;
			case Method::PATCH:
				volvoMethod = k_EHTTPMethodPATCH;
				break;
		}

		auto handle = g_pHTTP->CreateHTTPRequest(volvoMethod, url.c_str());

		if (method >= Method::POST)
		{
			if (!g_pHTTP->SetHTTPRequestRawPostBody(handle, "application/json", (u8 *)body.data(), body.size()))
			{
				KZ_LOG_WARN(LogChannel::General, "[HTTP] Failed to set request body.\n");
				g_pHTTP->ReleaseHTTPRequest(handle);
				if (onError)
				{
					onError();
				}
				return;
			}
		}

		for (const auto &[name, value] : headers)
		{
			g_pHTTP->SetHTTPRequestHeaderValue(handle, name.c_str(), value.c_str());
		}

		SteamAPICall_t steamCallHandle = k_uAPICallInvalid;

		if (!g_pHTTP->SendHTTPRequest(handle, &steamCallHandle))
		{
			KZ_LOG_WARN(LogChannel::General, "[HTTP] Failed to send HTTP request.\n");
			// No call handle means the completion callback will never fire, so clean up here.
			g_pHTTP->ReleaseHTTPRequest(handle);
			if (onError)
			{
				onError();
			}
			return;
		}
		// Log detailed HTTP requests for debugging.
		if (LoggingSystem_IsChannelEnabled(GetServiceChannel(LogChannel::General), LS_DETAILED))
		{
			std::string methodStr;
			switch (method)
			{
				case Method::GET:
					methodStr = "GET";
					break;
				case Method::POST:
					methodStr = "POST";
					break;
				case Method::PUT:
					methodStr = "PUT";
					break;
				case Method::PATCH:
					methodStr = "PATCH";
					break;
			}
			KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Sending HTTP %s request to `%s`\n", methodStr.c_str(), url.c_str());
			if (!body.empty())
			{
				KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Body: %s\n", body.c_str());
			}
			if (!headers.empty())
			{
				KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Headers:\n");
				for (const auto &[name, value] : headers)
				{
					KZ_LOG_DEBUG(LogChannel::General, "[HTTP]   %s: %s\n", name.c_str(), value.c_str());
				}
			}
		}
		new InFlightRequest(handle, steamCallHandle, url, body, onResponse, onError);
	}

	std::optional<std::string> Response::Header(const char *name) const
	{
		u32 headerValueSize;

		if (!g_pHTTP->GetHTTPResponseHeaderSize(requestHandle, name, &headerValueSize))
		{
			return std::nullopt;
		}

		u8 *rawHeaderValue = new u8[headerValueSize + 1];

		if (!g_pHTTP->GetHTTPResponseHeaderValue(requestHandle, name, rawHeaderValue, headerValueSize))
		{
			delete[] rawHeaderValue;
			return std::nullopt;
		}

		rawHeaderValue[headerValueSize] = '\0';

		std::string headerValue = std::string((char *)rawHeaderValue);
		delete[] rawHeaderValue;

		return headerValue;
	}

	std::optional<std::string> Response::Body() const
	{
		std::optional<std::vector<char>> rawBody = RawBody();

		if (rawBody.has_value())
		{
			return std::make_optional(std::string(rawBody->data(), rawBody->size()));
		}

		return std::nullopt;
	}

	std::optional<std::vector<char>> Response::RawBody() const
	{
		u32 responseBodySize;

		if (!g_pHTTP->GetHTTPResponseBodySize(requestHandle, &responseBodySize))
		{
			return std::nullopt;
		}

		std::vector<char> rawResponseBody(responseBodySize);

		if (!g_pHTTP->GetHTTPResponseBodyData(requestHandle, (u8 *)rawResponseBody.data(), responseBodySize))
		{
			return std::nullopt;
		}

		return std::make_optional(std::move(rawResponseBody));
	}

	void InFlightRequest::OnRequestCompleted(HTTPRequestCompleted_t *completedRequest, bool failed)
	{
		if (failed)
		{
			KZ_LOG_WARN(LogChannel::General, "[HTTP] request to `%s` failed with code %d\n", url.c_str(), completedRequest->m_eStatusCode);
			if (onError)
			{
				onError();
			}
			// ~InFlightRequest releases the underlying HTTP request.
			delete this;
			return;
		}

		Response response(completedRequest->m_eStatusCode, completedRequest->m_hRequest);
		if (LoggingSystem_IsChannelEnabled(GetServiceChannel(LogChannel::General), LS_DETAILED))
		{
			KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Received response for request to `%s` with status code %d\n", url.c_str(), response.status);
			if (!response.Body().has_value())
			{
				KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Response has no body.\n");
			}
			else
			{
				KZ_LOG_DEBUG(LogChannel::General, "[HTTP] Response body: %s\n", response.Body()->c_str());
			}
		}
		if (onResponse)
		{
			onResponse(response);
		}

		// ~InFlightRequest releases the underlying HTTP request. It has to outlive onResponse
		// because Response reads the body lazily through that handle.
		delete this;
	}

	void Cleanup()
	{
		while (!g_InFlightRequests.empty())
		{
			// The destructor removes the request from g_InFlightRequests and releases its handle.
			delete g_InFlightRequests.back();
		}
	}
} // namespace HTTP
