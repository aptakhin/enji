#include "server.h"

namespace enji {

Config ServerConfig;

Server::Server() { }

Server::Server(ServerOptions&& opts) {
    setup(std::move(opts));
}

Server::~Server() {}

void cb_on_connection(uv_stream_t* stream, int status) {
    Server& server = *reinterpret_cast<Server*>(stream->data);
    server.on_connection(status);
}

void cb_close(uv_handle_t* handle) {
    Connection& req = *reinterpret_cast<Connection*>(handle->data);
    req.notify_closed();
    delete handle;
}

void cb_after_write(uv_write_t* write, int status) {
    Connection& req = *reinterpret_cast<Connection*>(write->data);
    req.on_after_write(write, status);
}

void cb_after_shutdown(uv_shutdown_t* shutdown, int status) {
    Connection& req = *reinterpret_cast<Connection*>(shutdown->data);
    req.on_after_shutdown(shutdown, status);
}

void cb_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = new char[suggested_size];
    buf->len = size_t(suggested_size);
}

void cb_after_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    Connection& req = *reinterpret_cast<Connection*>(handle->data);
    req.on_after_read(nread, buf);
}

void Server::setup(ServerOptions&& opts) {
    base_options_ = std::move(opts);
    sockaddr_in addr;

    UVCHECK(uv_ip4_addr("127.0.0.1", opts.port, &addr),
        std::runtime_error, "Can't parse address to socketaddr");

    auto tcp_server = new uv_tcp_t;
    tcp_server_.reset(tcp_server);

    event_loop_.reset(new EventLoop{ reinterpret_cast<uv_stream_t*>(tcp_server) });

    UVCHECK(uv_tcp_init(event_loop_->loop(), tcp_server),
        std::runtime_error, "Can't init tcp");

    tcp_server->data = this;
    UVCHECK(uv_tcp_bind(tcp_server, (const struct sockaddr*) &addr, 0),
        std::runtime_error, "Can't bind tcp port");

    UVCHECK(uv_listen((uv_stream_t*)tcp_server, SOMAXCONN, cb_on_connection),
        std::runtime_error, "Can't listen tcp port");
}

void cb_idle(uv_idle_t* handle);

void Server::run() {
    uv_idle_t* on_loop = new uv_idle_t;
    UVCHECK(uv_idle_init(event_loop_->loop(), on_loop),
        std::runtime_error, "Can't init loop events handling");
    on_loop->data = this;
    on_loop_.reset(on_loop, [](uv_idle_t* idle) { uv_idle_stop(idle); delete idle; });
    uv_idle_start(on_loop, cb_idle);

    event_loop_->run();
}

void Server::create_connection(std::function<std::shared_ptr<Connection>()> create) {
    create_connection_ = create;
}

void Server::on_connection(int status) {
    auto new_connection = create_connection_();
    new_connection->accept();
    connections_.push_back(new_connection);
}

void Server::on_loop() {
    SignalEvent msg;
    while (output_queue_.pop(msg)) {
        if (msg.signal == RequestSignalType::WRITE || msg.signal == RequestSignalType::CLOSE) {
            auto wr = new WriteContext{};
            wr->conn = msg.conn;
            msg.buf.to_uv_buf(&wr->buf);
            wr->req.data = wr->conn;
            if (msg.signal == RequestSignalType::CLOSE) {
                wr->close = true;
            }
            UVCHECK(uv_write(&wr->req, msg.conn->sock(), &wr->buf, 1, cb_after_write),
                std::runtime_error, "Can't write data");
        }
        else if (msg.signal == RequestSignalType::CLOSE_CONFIRMED) {
            Connection* remove_conn = msg.conn;
            auto found = std::find_if(connections_.begin(), connections_.end(),
                [remove_conn](std::shared_ptr<Connection> req) {
                    return req.get() == remove_conn; });
            connections_.erase(found);
        }
    }
}

void Server::queue_read(Connection* conn, OweMem mem_block) {
    if (base_options_.worker_threads == 1) {
        conn->handle_input(StringView{mem_block.data, ssize_t(mem_block.size)});
        delete[] mem_block.data;
    }
    else {
        input_queue_.push(SignalEvent{conn, mem_block, READ});
    }
}

void Server::queue_write(Connection* conn, OweMem mem_block) {
    output_queue_.push(SignalEvent{conn, mem_block, WRITE});
}

void Server::queue_close(Connection* conn) {
    output_queue_.push(SignalEvent{conn, OweMem{}, CLOSE});
}

void Server::queue_confirmed_close(Connection* conn) {
    output_queue_.push(SignalEvent{conn, OweMem{}, RequestSignalType::CLOSE_CONFIRMED});
}

EventLoop::EventLoop(uv_stream_t* server)
:   server_{server} {
    uv_loop_t* loop = new uv_loop_t;
    UVCHECK(uv_loop_init(loop),
        std::runtime_error, "Can't init event loop");
    loop_.reset(loop, [](uv_loop_t* loop) {
        uv_loop_close(loop);
        delete loop;
    });
}

EventLoop::EventLoop(uv_stream_t* server, uv_loop_t* loop)
:   server_{server} {
    loop_.reset(loop, [](uv_loop_t* loop) {
        uv_loop_close(loop);
        delete loop;
    });
}

void EventLoop::run() {
    UVCHECK(uv_run(loop(), UV_RUN_DEFAULT),
        std::runtime_error, "Can't run event loop");
}

Connection::Connection(Server* parent, size_t id)
:   base_parent_{parent},
    id_{id} {
    uv_tcp_t* stream = new uv_tcp_t{};
    stream_.reset(reinterpret_cast<uv_stream_t*>(stream));
    UVCHECK(uv_tcp_init(base_parent_->event_loop()->loop(), stream),
        std::runtime_error, "Can't init tcp in Connection");
    stream->data = this;
}

std::ostream& Connection::log() {
    std::cout << "[" << id_ << "] ";
    return std::cout;
}

void Connection::accept() {
    UVCHECK(uv_accept(base_parent_->event_loop()->server(), stream_.get()),
        std::runtime_error, "Can't accept socket");

    UVCHECK(uv_read_start(stream_.get(), cb_alloc_buffer, cb_after_read),
        std::runtime_error, "Can't start read");
}

void cb_idle(uv_idle_t* handle) {
    Server& that = *reinterpret_cast<Server*>(handle->data);
    that.on_loop();
}

void on_work_cb(uv_work_t* req) {

}

void on_after_work_cb(uv_work_t* req, int status) {

}

void Connection::on_after_read(ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0) {
        //std::cout << String(buf->base, buf->base + nread);
        uv_buf_t send_buf = uv_buf_init(buf->base, (unsigned int)nread);
        base_parent_->queue_read(this, OweMem{ buf->base, size_t(nread) });
    }

    if (nread <= 0) {
        delete[] buf->base;
    }

    if (nread == 0) {
        return;
    }

    if (nread < 0) {
        //
        // FIXME: Write properly status == UV_EOF
        //

        auto shutdown = new uv_shutdown_t;
        shutdown->data = this;
        UVCHECK(uv_shutdown(shutdown, stream_.get(), cb_after_shutdown),
            std::runtime_error, "Can't start connection shutdown");
    }
}

void Connection::on_after_write(uv_write_t* req, int status) {
    auto write_result = reinterpret_cast<WriteContext*>(req);
    req->handle->data = write_result->conn;

    UVCHECK(status,
        std::runtime_error, "Bad status of write operation");

    //
    // FIXME: Write properly status == 0, UV_ECANCELED, error
    //

    if (write_result->close) {
        uv_close((uv_handle_t*) req->handle, cb_close);
    }

    delete[] write_result->buf.base;
    delete write_result;
}

void Connection::on_after_shutdown(uv_shutdown_t* shutdown, int status) {
    UVCHECK(status,
        std::runtime_error, "Bad status of shutdown operation");
    //
    // FIXME: Write properly status < 0
    //

    uv_close((uv_handle_t*) shutdown->handle, cb_close);
    delete shutdown;
}

void Connection::notify_closed() {
    stream_.release();
    base_parent_->queue_confirmed_close(this);
}

void Connection::write_chunk(OweMem mem_block) {
    base_parent_->queue_write(this, mem_block);
}

void Connection::write_chunk(std::ostringstream& buf) {
    auto&& str = buf.str();
    auto size = str.size();
    char* data = new char[size];
    std::memcpy(data, &str.front(), size);
    OweMem mem_block{data, size};
    write_chunk(mem_block);
}

void Connection::close() {
    if (!is_closing_) {
        is_closing_ = true;
        base_parent_->queue_close(this);
    }
}

Config::Config()
:   root_(std::map<Value, Value>{}) {
}

} // namespace enji