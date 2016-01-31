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
    READ,
    WRITE,
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

    void create_connection(std::function<std::shared_ptr<Connection>()>);

    virtual void on_connection(int status);
    virtual void on_loop();

    EventLoop* event_loop() { return event_loop_.get(); }

    void queue_read(Connection* conn, OweMem mem_block);
    void queue_write(Connection* conn, OweMem mem_block);
    void queue_close(Connection* conn);
    void queue_confirmed_close(Connection* conn);

protected:
    ServerOptions base_options_;

    std::unique_ptr<EventLoop> event_loop_;

    ScopePtrExit<uv_idle_t> on_loop_;

    std::vector<std::shared_ptr<Connection>> connections_;

    SafeQueue<SignalEvent> input_queue_;
    SafeQueue<SignalEvent> output_queue_;

    std::vector<std::thread> threads_;

    size_t counter_ = 0;

    std::unique_ptr<uv_tcp_t> tcp_server_;

    std::function<std::shared_ptr<Connection>()> create_connection_;
};

class Connection {
public:
    Connection(Server* parent, size_t id);

    virtual void handle_input(StringView data) {}

    void write_chunk(OweMem mem_block);
    void close();

    void accept();

    void on_after_read(ssize_t nread, const uv_buf_t* buf);

    void on_after_write(uv_write_t* req, int status);
    void on_after_shutdown(uv_shutdown_t* shutdown, int status);

    void notify_closed();

    uv_stream_t* sock() { return stream_.get(); }

protected:
    std::ostream& log();

protected:
    Server* base_parent_;

    std::unique_ptr<uv_stream_t> stream_;

    size_t id_;
    bool is_closing_ = false;
};

} // namespace enji