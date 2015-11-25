#include "server.h"
#include "http.h"

namespace enji {

__thread boost::context::fcontext_t* g_parent_ctx;
__thread boost::context::fcontext_t* g_handler_ctx;

Server::Server() { }

Server::Server(ServerOptions&& opts) {
    setup(std::move(opts));
}

#define assert(expr) {}


UvProc::UvProc(uv_stream_t* server)
    : server_(server) {
    uv_loop_t* loop = new uv_loop_t;
    uv_loop_init(loop);
    loop_.reset(loop, [](uv_loop_t* loop) {
        uv_loop_close(loop);
        delete loop;
    });
}

UvProc::UvProc(uv_stream_t* server, uv_loop_t* loop)
    : server_(server) {
    loop_.reset(loop, [](uv_loop_t* loop) {
        uv_loop_close(loop);
        delete loop;
    });
}

void UvProc::run() {
    int r = uv_run(loop(), UV_RUN_DEFAULT);
    if (r != 0) {
        const char* error = uv_strerror(r);
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }
}

void cb_close(uv_handle_t* handle) {
    delete handle;
}

void cb_after_write(uv_write_t* req, int status) {
    UvRequest& request = *reinterpret_cast<UvRequest*>(req->data);
    request.on_after_write(req, status);
}

void cb_after_shutdown(uv_shutdown_t* req, int status) {
    /*assert(status == 0);*/
    if (status < 0)
        fprintf(stderr, "err: %s\n", uv_strerror(status));
    fprintf(stderr, "data received\n");
    uv_close((uv_handle_t*) req->handle, cb_close);
    delete req;
}

void cb_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    std::cout << "Callback alloc " << suggested_size << std::endl;
    buf->base = new char[suggested_size];
    buf->len = suggested_size;
}


int requests = 0;

void cb_after_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
    UvRequest& req = *reinterpret_cast<UvRequest*>(handle->data);
    req.on_after_read(nread, buf);
//    requests += 1;
//    if (requests >= 1000) {
//        exit(-1);
//    }
}

void handleri(intptr_t in) {
    std::cout << "Handler" << std::endl;

}

UvRequest::UvRequest(Server::Handler* parent, IRequestHandler* handler, const UvProc* proc)
    : parent_(parent),
      handler_(handler),
      proc_(proc),
      input_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
      output_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
      ctx_{StdInputStream(input_), StdOutputStream(output_)} {
    uv_tcp_t* stream = new uv_tcp_t;
    stream_.reset(reinterpret_cast<uv_stream_t*>(stream));
    int r = uv_tcp_init(proc_->loop(), stream);
    stream->data = this;

    std::size_t size = 8192;
    void* sp = std::malloc(size);
    handler_ctx_ = boost::context::make_fcontext(sp, size, handleri);
}

void UvRequest::accept() {
    int r = uv_accept(proc_->server(), stream_.get());
//    assert(r == 0);

    r = uv_read_start(stream_.get(), cb_alloc_buffer, cb_after_read);
//    assert(r == 0);
}

Server::Handler::Handler(Server* parent, uv_tcp_t* server)
    : parent_(parent),
      proc_((uv_stream_t*) server, uv_default_loop()) {
}

void Server::Handler::on_connection(int status) {
    auto new_req = std::make_shared<UvRequest>(this, new HttpRequestHandler{this}, &proc_);
    new_req->accept();
    requests_.push_back(new_req);
}

void Server::Handler::run() {
    for (size_t i = 0; i < 4; ++i) {
        threads_.push_back(std::thread{[&]() {
            while (true) {
                SignalEvent msg = queue_.pop();
                msg.recv->handle_event(std::move(msg));
            }
        }});
    }

    proc_.run();
}

Response Server::Handler::find_route(const String& url, IRequestHandler* handler) {
    for (auto&& route : parent_->routes_) {
        //std::cout << "Url" << "_" << route.path << std::endl;
        if (route.path == url) {
            auto&& response = route.func(handler->request());
            return response;
        }
    }
    return Response{};
}

void UvRequest::on_after_read(ssize_t nread, const uv_buf_t* buf) {
    std::cout << "Got " << nread << std::endl;

    parent_->queue_.push(SignalEvent{this, String(buf->base, nread)});

//    input_.write(buf->base, nread);
//    handler_->handle(ctx_);
//
//    while (output_) {
//        char tmp_buf[1024];
//        output_.read(tmp_buf, sizeof(tmp_buf));
//        size_t size = output_.gcount();
//        write_req_t* wr = new write_req_t;
//        wr->buf = uv_buf_init(tmp_buf, size);
//        if (size < sizeof(tmp_buf)) {
//            wr->close = true;
//        }
//        int r = uv_write(&wr->req, stream_.get(), &wr->buf, 1, cb_after_write);
//    }

    if (nread <= 0 && buf->base != nullptr)
        delete[] buf->base;

    if (nread == 0)
        return;

    if (nread < 0) {
        /*assert(nread == UV_EOF);*/
        fprintf(stderr, "err: %s\n", uv_strerror(nread));

        uv_shutdown_t* shutdown = new uv_shutdown_t;
        int r = uv_shutdown(shutdown, stream_.get(), cb_after_shutdown);
        assert(r == 0);
        return;
    }
}

void UvRequest::on_after_write(uv_write_t* req, int status) {
    write_req_t* wr = reinterpret_cast<write_req_t*>(req);

    delete wr;

    std::cout << "Wrote: " << status << std::endl;

//    if (status == 0)
//        return;
//
//    fprintf(stderr, "uv_write error: %s\n", uv_strerror(status));
//
//    if (status == UV_ECANCELED)
//        return;
//
//    assert(status == UV_EPIPE);
    if (wr->close) {
        uv_close((uv_handle_t*) req->handle, cb_close);
    }
}

void UvRequest::handle_event(SignalEvent&& event) {
    input_.write(&event.msg[0], event.msg.size());
    handler_->handle(ctx_);
    //thread_->
}

void on_connection(uv_stream_t* server, int status) {
    Server::Handler& serv = *reinterpret_cast<Server::Handler*>(server->data);
    serv.on_connection(status);
}

Server::~Server() {
    delete handler_;
}

void Server::setup(ServerOptions&& opts) {
    sockaddr_in addr;

    int r = uv_ip4_addr("127.0.0.1", opts.port, &addr);
    //assert(r == 0);

    uv_tcp_t* tcp_server = new uv_tcp_t;
    tcp_server_.reset(tcp_server);

    //assert(tcp_server != NULL);
    handler_ = new Server::Handler(this, tcp_server);
    r = uv_tcp_init(handler_->proc().loop(), tcp_server);
    //assert(r == 0);

    tcp_server->data = handler_;
    r = uv_tcp_bind(tcp_server, (const struct sockaddr*) &addr, 0);
    if (r != 0) {
        const char* error = uv_strerror(r);
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }

    r = uv_listen((uv_stream_t*) tcp_server, SOMAXCONN, on_connection);
    if (r != 0) {
        const char* error = uv_strerror(r);
        std::cerr << error << " at " << __FILE__ << ":" << __LINE__ << std::endl;
        throw std::runtime_error(error);
    }
}

void Server::run() {
    handler_->run();
}

void Server::add_route(Route&& route) {
    routes_.push_back(std::move(route));
}

Route::Route(String&& path, Func func)
    : path(path),
      func(func) {
}

void switch2handler(UvRequest* request) {
    g_handler_ctx = &request->handler_ctx_;
    boost::context::jump_fcontext(g_parent_ctx, *g_handler_ctx, (intptr_t) request);
    /// init here
}

void switch2loop() {
    boost::context::fcontext_t* handler = g_handler_ctx;
    g_handler_ctx = nullptr;
    boost::context::jump_fcontext(handler, *g_parent_ctx, 0);
}

} // namespace enji