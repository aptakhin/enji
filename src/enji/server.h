#pragma once

#include "common.h"

namespace enji {

class Connection;

struct ServerOptions {
    String host;
    int port;
    int worker_threads = 0;
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

enum class ConnEventType {
    NONE,
    READ,
    WRITE,
    CLOSE,
    CLOSE_CONFIRMED,
};

struct ConnEvent {
    Connection* conn = nullptr;
    ConnEventType ev = ConnEventType::NONE;
    TransferBlock buf;

    ConnEvent() {}
    ConnEvent(Connection* conn, ConnEventType ev);
    ConnEvent(Connection* conn, ConnEventType ev, TransferBlock buf);
};

class Config {
public:
    Config();

    Value& operator [] (const char* key) { return root_[key]; }
    const Value& operator [] (const char* key) const { return root_[key]; }

private:
    Value root_;
};

extern Config ServerConfig;

class Server {
public:
    Server();

    Server(Config& config);

    ~Server();

    void setup(Config& config);

    void run();

    Server& create_connection(std::function<std::shared_ptr<Connection>()>);

    EventLoop* event_loop() { return event_loop_.get(); }

    void queue_read(Connection* conn, TransferBlock mem_block);
    void queue_write(Connection* conn, TransferBlock mem_block);
    void queue_close(Connection* conn);
    void queue_confirmed_close(Connection* conn);

private:
    virtual void on_connection(int status);
    virtual void on_loop();

    friend void cb_on_connection(uv_stream_t*, int);
    friend void cb_idle(uv_idle_t*);
    
protected:
    Config& config_;

    std::unique_ptr<EventLoop> event_loop_;

    ScopePtrExit<uv_idle_t> on_loop_;

    std::vector<std::shared_ptr<Connection>> connections_;

    SafeQueue<ConnEvent> input_queue_;
    SafeQueue<ConnEvent> output_queue_;

    std::vector<std::thread> threads_;

    size_t counter_ = 0;

    std::unique_ptr<uv_tcp_t> tcp_server_;

    std::function<std::shared_ptr<Connection>()> create_connection_;
};

class Connection {
public:
    Connection(Server* parent, size_t id);

    void write_chunk(TransferBlock block);
    void write_chunk(std::stringstream& buf);

    void close();

    uv_stream_t* sock() { return stream_.get(); }

    std::ostream& log();

private:
    friend class Server;

    void accept();

    virtual void handle_input(TransferBlock data) {}

    void on_after_read(ssize_t nread, const uv_buf_t* buf);

    void on_after_write(uv_write_t* req, int status);
    void on_after_shutdown(uv_shutdown_t* shutdown, int status);
    void notify_closed();

    friend void cb_on_connection(uv_stream_t*, int);
    friend void cb_close(uv_handle_t*);
    friend void cb_after_write(uv_write_t*, int);
    friend void cb_after_shutdown(uv_shutdown_t*, int);
    friend void cb_alloc_buffer(uv_handle_t*, size_t, uv_buf_t*);
    friend void cb_after_read(uv_stream_t*, ssize_t, const uv_buf_t*);

protected:
    Server* base_parent_;

    std::unique_ptr<uv_stream_t> stream_;

    size_t id_;

    bool is_closing_ = false;

protected:
    std::chrono::time_point<std::chrono::high_resolution_clock> tp_accepted_;
};

} // namespace enji