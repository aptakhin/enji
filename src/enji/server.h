#pragma once

#include "common.h"

namespace enji {

class Connection;

struct ServerOptions {
    String host;
    int port;
    int worker_threads = 1;
};

typedef std::pair<String, String> Header;

class Response {
public:

	Response(Connection* conn);

	void write(const char* data, size_t size);

	void flush();

	void close();

private:
	Connection* conn_;

	String buf_;
};

class EventLoop {
public:
	EventLoop(uv_stream_t* server);

	EventLoop(uv_stream_t* server, uv_loop_t* loop);

	uv_loop_t* loop() const { return ~loop_; }

	uv_stream_t* server() const { return server_; }

	void run();

protected:
	ScopePtrExit<uv_loop_t> loop_;
	uv_stream_t* server_;
};

enum RequestSignalType {
	NONE,
	WRITE,
	INPUT_EOF,
	CLOSE,
	CLOSE_CONFIRMED,
};

struct SignalEvent {
	Connection* conn;
	OweMem buf;
	RequestSignalType signal;
};

class Server {
public:
    Server();

    Server(ServerOptions&& options);

    virtual ~Server();

    void setup(ServerOptions&& options);

    void run();

	friend class Connection;
	friend void handleri(intptr_t in);

	virtual void on_connection(int status);
	virtual void on_loop();

	EventLoop* event_loop() { return event_loop_.get(); }

	void request_finished(Connection* pRequest);

	void queue_write(Connection* conn, OweMem mem_block);
	void queue_close(Connection* conn);

protected:
	std::unique_ptr<EventLoop> event_loop_;

	ScopePtrExit<uv_idle_t> on_loop_;

	std::vector<std::shared_ptr<Connection>> connections_;

	SafeQueue<SignalEvent> input_queue_;
	SafeQueue<SignalEvent> output_queue_;

	std::vector<std::thread> threads_;

	size_t counter_ = 0;

    std::unique_ptr<uv_tcp_t> tcp_server_;
};

class IConnectionHandler {
public:
    virtual void handle(const String& data, Response& response) = 0;

    virtual ~IConnectionHandler() { }
};

class Connection {
public:
	Connection(Server* parent, size_t id);

    uv_stream_t* tcp() const { return stream_.get(); }

	virtual void handle_input(StringView data) {}

	void write_chunk(OweMem mem_block);
	void close();

    void accept();

    void on_after_read(ssize_t nread, const uv_buf_t* buf);

    void on_after_write(uv_write_t* req, int status);
    void on_after_shutdown(uv_shutdown_t* shutdown, int status);

    void handle_event(SignalEvent&& event);

    void notify_closed();

	friend void handleri(intptr_t in);

	uv_stream_t* sock() { return stream_.get(); }

protected:
    std::ostream& log();

protected:
    Server* base_parent_;

    std::unique_ptr<uv_stream_t> stream_;

    std::unique_ptr<char> handler_ctx_stack_;

    size_t id_;
	bool is_closing_ = false;
};

} // namespace enji