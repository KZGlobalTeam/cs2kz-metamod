#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "common.h"
#include "utils/json.h"
#include "utils/uuid.h"

#include <vendor/ixwebsocket/ixwebsocket/IXWebSocket.h>

/**
 * A utility class that deals with common WebSocket logic.
 *
 * Both `KZGlobalService` and `KZRacingService` use this class for their WebSocket connections.
 */
class KZWebSocket
{
public:
	struct Message
	{
		std::string id;
		std::string tag;
		Json data;

		Message() = default;

		template<typename Data>
		inline Message(const Data &data) : Message(UUID_t().ToString(), data)
		{
		}

		template<typename ID, typename Data>
		inline Message(ID &&id, const Data &data) : id(std::forward<ID>(id)), tag(Data::tag)
		{
			data.ToJson(this->data);
		}

		void Encode(Json &out) const;
		bool Decode(const Json &in);
	};

	class Handle
	{
		friend class KZWebSocket;

		std::weak_ptr<KZWebSocket> socket;
		std::thread thread;

		template<typename F>
		explicit inline Handle(const std::shared_ptr<KZWebSocket> &socket, F &&f) : socket(socket), thread(std::forward<F>(f), socket)
		{
		}

	public:
		/**
		 * Returns whether the socket is currently connected.
		 */
		bool IsConnected();

		/**
		 * Returns whether the socket may reconnect after a disconnect.
		 */
		bool MayReconnect();

		/**
		 * Receives as many messages as are currently available into the given buffer.
		 *
		 * Returns how many messages have been received.
		 */
		size_t ReceiveMessages(std::vector<Message> &buf);

		/**
		 * Enqueues a message to be sent as soon as possible.
		 */
		bool SendMessage(Message &&message);

		/**
		 * Starts the receiving thread, which also opens the connection.
		 */
		void Start();

		/**
		 * Signals the dispatch thread to shut down.
		 */
		void Shutdown(bool join = true);
	};

	template<typename F>
	inline void Configure(const std::string &url, F &&onMessageCallback)
	{
		this->socket.setUrl(url);
		this->socket.setPingInterval(/* pingIntervalSecs= */ 30);
		this->socket.enablePerMessageDeflate();
		this->socket.enableAutomaticReconnection();
		this->socket.setAutoThreadName(/* enabled= */ false);
		this->socket.setOnMessageCallback(std::forward<F>(onMessageCallback));
	}

	template<typename F>
	inline void Configure(const std::string &url, const ix::WebSocketHttpHeaders &extraHeaders, F &&onMessageCallback)
	{
		Configure(url, std::forward<F>(onMessageCallback));
		this->socket.setExtraHeaders(extraHeaders);
	}

	/**
	 * A default implementation of `ix::OnMessageCallback` that consumers of this class may call at the start of their own callback.
	 *
	 * It performs work that all consumers will likely want to do anyway, such as logging and message data decoding.
	 */
	void OnMessage(const ix::WebSocketMessagePtr &message);

	/**
	 * Spawns a thread to drive the dispatch loop.
	 */
	inline static Handle SpawnDispatchThread(const std::shared_ptr<KZWebSocket> &socket)
	{
		return Handle(socket, [](std::shared_ptr<KZWebSocket> socket) { return socket->RunDispatchLoop(); });
	}

private:
	struct EncodedMessage
	{
		bool binary;
		std::string data;
	};

	ix::WebSocket socket;

	// Protects the message queues and synchronizes condition variable(s).
	std::mutex mtx {};

	// Used for waking up the dispatch thread when there is a new message for it to send, or during shutdown.
	std::condition_variable dispatchThreadCvar {};

	// Temporary buffer for holding messages that have been received but not yet processed.
	std::vector<Message> receiveQueue {};

	// Temporary buffer for holding messages to be sent.
	std::vector<Message> sendQueue {};

	/**
	 * Runs the send queue dispatch loop.
	 *
	 * This will block until `Handle::Shutdown()` is called, so this shold be called on a dedicated thread.
	 */
	void RunDispatchLoop();

private:
	void OnWebSocketMessage(const std::string &data, bool binary);
	void OnWebSocketOpen(const ix::WebSocketOpenInfo &);
	void OnWebSocketClose(const ix::WebSocketCloseInfo &);
	void OnWebSocketError(const ix::WebSocketErrorInfo &);
	void OnWebSocketPing();
	void OnWebSocketPong();
};
