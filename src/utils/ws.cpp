// required for ws library
#ifdef _WIN32
#pragma comment(lib, "Ws2_32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#endif

#include <vendor/ixwebsocket/ixwebsocket/IXNetSystem.h>

#include "ws.h"

void KZWebSocket::Message::Encode(Json &out) const
{
	out.Set("id", this->id);
	out.Set("tag", this->tag);
	out.Set("data", this->data);
}

bool KZWebSocket::Message::Decode(const Json &in)
{
	return in.Get("id", this->id) && in.Get("tag", this->tag) && in.Get("data", this->data);
}

bool KZWebSocket::Handle::IsConnected()
{
	std::shared_ptr<KZWebSocket> socket = this->socket.lock();
	return socket && socket->socket.getReadyState() == ix::ReadyState::Open;
}

bool KZWebSocket::Handle::MayReconnect()
{
	std::shared_ptr<KZWebSocket> socket = this->socket.lock();
	return socket && (socket->socket.getReadyState() == ix::ReadyState::Connecting || socket->socket.isAutomaticReconnectionEnabled());
}

size_t KZWebSocket::Handle::ReceiveMessages(std::vector<Message> &buf)
{
	std::shared_ptr<KZWebSocket> socket = this->socket.lock();

	if (!socket)
	{
		return 0;
	}

	if (buf.empty())
	{
		std::lock_guard<std::mutex> _guard(socket->mtx);
		buf.swap(socket->receiveQueue);
		return buf.size();
	}

	size_t count = 0;

	for (auto it = socket->receiveQueue.begin(); it != socket->receiveQueue.end(); it = socket->receiveQueue.erase(it))
	{
		buf.emplace_back(std::move(*it));
		count += 1;
	}

	return count;
}

bool KZWebSocket::Handle::SendMessage(KZWebSocket::Message &&message)
{
	std::shared_ptr<KZWebSocket> socket = this->socket.lock();

	if (!socket)
	{
		return false;
	}

	switch (socket->socket.getReadyState())
	{
		case ix::ReadyState::Connecting:
			/* fallthrough */
		case ix::ReadyState::Open:
		{
			{
				std::lock_guard<std::mutex> _guard(socket->mtx);
				socket->sendQueue.emplace_back(std::forward<KZWebSocket::Message>(message));
			}
			socket->dispatchThreadCvar.notify_one();
			return true;
		}

		case ix::ReadyState::Closing:
			/* fallthrough */
		case ix::ReadyState::Closed:
			return false;
	}
}

void KZWebSocket::Handle::Start()
{
	if (std::shared_ptr<KZWebSocket> socket = this->socket.lock())
	{
		KZ_LOG_DEBUG(LogChannel::WS, "starting socket");
		socket->socket.start();
	}
}

void KZWebSocket::Handle::Shutdown(bool join)
{
	if (std::shared_ptr<KZWebSocket> socket = this->socket.lock())
	{
		KZ_LOG_DEBUG(LogChannel::WS, "stopping socket");
		socket->socket.stop();
		socket->dispatchThreadCvar.notify_one();
	}

	if (join && this->thread.joinable())
	{
		this->thread.join();
	}
}

void KZWebSocket::Init()
{
	ix::initNetSystem();
}

void KZWebSocket::Cleanup()
{
	ix::uninitNetSystem();
}

void KZWebSocket::OnMessage(const ix::WebSocketMessagePtr &message)
{
	switch (message->type)
	{
		case ix::WebSocketMessageType::Message:
			return this->OnWebSocketMessage(message->str, message->binary);

		case ix::WebSocketMessageType::Open:
			return this->OnWebSocketOpen(message->openInfo);

		case ix::WebSocketMessageType::Close:
			return this->OnWebSocketClose(message->closeInfo);

		case ix::WebSocketMessageType::Error:
			return this->OnWebSocketError(message->errorInfo);

		case ix::WebSocketMessageType::Ping:
			return this->OnWebSocketPing();

		case ix::WebSocketMessageType::Pong:
			return this->OnWebSocketPong();

		default:
			return;
	}
}

static_function bool DispatchLoopShouldContinue(ix::ReadyState socketState)
{
	switch (socketState)
	{
		case ix::ReadyState::Connecting:
			/* fallthrough */
		case ix::ReadyState::Open:
			return true;

		case ix::ReadyState::Closing:
			/* fallthrough */
		case ix::ReadyState::Closed:
			return false;
	}
}

void KZWebSocket::RunDispatchLoop()
{
	std::unique_lock<std::mutex> guard(this->mtx);
	Json encodeBuffer;

	KZ_LOG_DEBUG(LogChannel::WS, "entering dispatch loop");

	for (ix::ReadyState socketState = this->socket.getReadyState(); DispatchLoopShouldContinue(socketState);
		 socketState = this->socket.getReadyState())
	{
		for (auto it = this->sendQueue.begin(); it != this->sendQueue.end();)
		{
			it->Encode(encodeBuffer);
			KZ_LOG_DEBUG(LogChannel::WS, "sending message %s (%s)", it->id.c_str(), it->tag.c_str());
			ix::WebSocketSendInfo sendInfo = socket.send(encodeBuffer.ToString());
			encodeBuffer.Clear();

			if (!sendInfo.success)
			{
				KZ_LOG_WARN(LogChannel::WS, "failed to send message %s (%s)", it->id.c_str(), it->tag.c_str());
				break;
			}

			KZ_LOG_DEBUG(LogChannel::WS, "sent message %s (%s)", it->id.c_str(), it->tag.c_str());

			it = this->sendQueue.erase(it);
		}

		KZ_LOG_DEBUG(LogChannel::WS, "(dispatch loop) waiting");

		// clang-format off
		this->dispatchThreadCvar.wait(guard, [&] {
			return !this->sendQueue.empty() || !DispatchLoopShouldContinue(socketState = this->socket.getReadyState());
		});
		// clang-format on

		if (!DispatchLoopShouldContinue(socketState))
		{
			break;
		}
	}

	KZ_LOG_DEBUG(LogChannel::WS, "exiting dispatch loop");
}

void KZWebSocket::OnWebSocketMessage(const std::string &data, bool binary)
{
	KZ_LOG_INFO(LogChannel::WS, "received %s message", binary ? "binary" : "text");
	KZ_LOG_DEBUG(LogChannel::WS, "message payload:\n```\n%s\n```", data.c_str());

	Json json(data);
	Message message;

	if (message.Decode(json))
	{
		KZ_LOG_DEBUG(LogChannel::WS, "decoded message %s (%s)", message.id.c_str(), message.tag.c_str());
		std::lock_guard<std::mutex> _guard(this->mtx);
		this->receiveQueue.push_back(std::move(message));
	}
}

void KZWebSocket::OnWebSocketOpen(const ix::WebSocketOpenInfo &info)
{
	KZ_LOG_INFO(LogChannel::WS, "connection established (uri=%s, protocol=%s)", info.uri.c_str(), info.protocol.c_str());
}

void KZWebSocket::OnWebSocketClose(const ix::WebSocketCloseInfo &info)
{
	if (!info.remote)
	{
		KZ_LOG_INFO(LogChannel::WS, "closed the connection (code=%i, reason=%s)", info.code, info.reason.c_str());
		return;
	}

	KZ_LOG_WARN(LogChannel::WS, "remote closed the connection (code=%i, reason=%s)", info.code, info.reason.c_str());

	switch (info.code)
	{
		case 1000 /* NORMAL */:
		case 1001 /* GOING AWAY */:
		case 1006 /* ABNORMAL */:
		{
			this->socket.enableAutomaticReconnection();
			this->socket.setMinWaitBetweenReconnectionRetries(10'000 /* ms */);
		}
		break;

		case 1008 /* POLICY VIOLATION */:
		{
			this->socket.disableAutomaticReconnection();
		}
		break;

		default:
		{
			this->socket.enableAutomaticReconnection();
			this->socket.setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
		}
	}
}

void KZWebSocket::OnWebSocketError(const ix::WebSocketErrorInfo &info)
{
	KZ_LOG_WARN(LogChannel::WS, "failed to establish connection (status=%i, reason=%s, decompressionError=%s, retries=%i, wait_time=%.2f)",
				info.http_status, info.reason.c_str(), info.decompressionError ? "true" : "false", info.retries, info.wait_time);

	switch (info.http_status)
	{
		case 401:
		case 403:
		{
			this->socket.disableAutomaticReconnection();
		}
		break;

		default:
		{
			this->socket.enableAutomaticReconnection();
			this->socket.setMinWaitBetweenReconnectionRetries(60'000 /* ms */);
		}
	}
}

void KZWebSocket::OnWebSocketPing()
{
	KZ_LOG_WARN(LogChannel::WS, "received ping");
}

void KZWebSocket::OnWebSocketPong()
{
	KZ_LOG_DEBUG(LogChannel::WS, "received pong");
}
