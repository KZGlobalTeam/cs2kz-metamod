/*
 * Credit to CS2Fixes: https://github.com/Source2ZE/CS2Fixes/blob/40a7f3d9f479aeb8f1d0a5acb61d615c07176e43/src/httpmanager.h
 */

#include "../cs2kz.h"
#undef snprintf
#include <steam/steam_gameserver.h>

#pragma once

class HTTPManager;
extern HTTPManager g_HTTPManager;

#define CompletedCallback std::function<void(HTTPRequestHandle, int, std::string)>

class HTTPHeader
{
public:
	HTTPHeader(std::string strName, std::string strValue)
	{
		m_strName = strName;
		m_strValue = strValue;
	}

	const char *GetName()
	{
		return m_strName.c_str();
	}

	const char *GetValue()
	{
		return m_strValue.c_str();
	}

private:
	std::string m_strName;
	std::string m_strValue;
};

class HTTPManager
{
public:
	void GET(const char *pszUrl, CompletedCallback callback, std::vector<HTTPHeader> *headers = nullptr);
	void POST(const char *pszUrl, const char *pszText, CompletedCallback callback, std::vector<HTTPHeader> *headers = nullptr);

	bool HasAnyPendingRequests() const
	{
		return m_PendingRequests.size() > 0;
	}

private:
	class TrackedRequest
	{
	public:
		TrackedRequest(const TrackedRequest &req) = delete;
		TrackedRequest(HTTPRequestHandle hndl, SteamAPICall_t hCall, std::string strUrl, std::string strText, CompletedCallback callback);
		~TrackedRequest();

	private:
		void OnHTTPRequestCompleted(HTTPRequestCompleted_t *arg, bool bFailed);

		HTTPRequestHandle m_hHTTPReq;
		CCallResult<TrackedRequest, HTTPRequestCompleted_t> m_CallResult;
		std::string m_strUrl;
		std::string m_strText;
		CompletedCallback m_callback;
	};

private:
	std::vector<HTTPManager::TrackedRequest *> m_PendingRequests;
	void GenerateRequest(EHTTPMethod method, const char *pszUrl, const char *pszText, CompletedCallback callback, std::vector<HTTPHeader> *headers);
};
