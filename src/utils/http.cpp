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

	void Request::Send(ResponseCallback onResponse) const
	{
		if (!g_pHTTP)
		{
			META_CONPRINTF("[KZ::HTTP] Initializing HTTP client...\n");
			g_steamAPI.Init();
			g_pHTTP = g_steamAPI.SteamHTTP();
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
				META_CONPRINTF("[KZ::HTTP] Failed to set request body.\n");
				return;
			}
		}

		for (const auto &[name, value] : headers)
		{
			g_pHTTP->SetHTTPRequestHeaderValue(handle, name.c_str(), value.c_str());
		}

		SteamAPICall_t steamCallHandle;

		if (!g_pHTTP->SendHTTPRequest(handle, &steamCallHandle))
		{
			META_CONPRINTF("[KZ::HTTP] Failed to send HTTP request.\n");
		}

		new InFlightRequest(handle, steamCallHandle, url, body, onResponse);
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
		u32 responseBodySize;

		if (!g_pHTTP->GetHTTPResponseBodySize(requestHandle, &responseBodySize))
		{
			return std::nullopt;
		}

		u8 *rawResponseBody = new u8[responseBodySize + 1];

		if (!g_pHTTP->GetHTTPResponseBodyData(requestHandle, rawResponseBody, responseBodySize))
		{
			delete[] rawResponseBody;
			return std::nullopt;
		}

		rawResponseBody[responseBodySize] = '\0';

		std::string responseBody = std::string((char *)rawResponseBody);
		delete[] rawResponseBody;

		return responseBody;
	}

	void InFlightRequest::OnRequestCompleted(HTTPRequestCompleted_t *completedRequest, bool failed)
	{
		if (failed)
		{
			META_CONPRINTF("[KZ::HTTP] request to `%s` failed with code %d\n", url.c_str(), completedRequest->m_eStatusCode);
			delete this;
			return;
		}

		Response response(completedRequest->m_eStatusCode, completedRequest->m_hRequest);
		onResponse(response);

		if (g_pHTTP)
		{
			g_pHTTP->ReleaseHTTPRequest(completedRequest->m_hRequest);
		}

		delete this;
	}
} // namespace HTTP
