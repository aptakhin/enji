#include "server.h"
#include "common.h"

Server::Server() {}

Server::Server(ServerOptions&& opts) {
    setup(std::move(opts));
}

#define assert(expr) {}

class UvProc {
public:
    UvProc(uv_stream_t* server);
    UvProc(uv_stream_t* server, uv_loop_t* loop);

    uv_loop_t* loop() const { return ~loop_; }

    uv_stream_t* server() const { return server_; }

    void run();

protected:
    ScopePtrExit<uv_loop_t> loop_;
    uv_stream_t* server_;
};

UvProc::UvProc(uv_stream_t* server)
    :   server_(server) {
    uv_loop_t* loop = new uv_loop_t;
    uv_loop_init(loop);
    loop_.reset(loop, [] (uv_loop_t* loop) {
        uv_loop_close(loop);
        delete loop;
    });
}

UvProc::UvProc(uv_stream_t* server, uv_loop_t* loop)
    :   server_(server) {
    loop_.reset(loop, [] (uv_loop_t* loop) {
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

class UvRequest {
public:
    UvRequest(Server::Handler* parent, IRequestHandler* handler, const UvProc* proc);
    uv_stream_t* tcp() const { return stream_.get(); }

    void accept();

    void on_after_read(ssize_t nread, const uv_buf_t* buf);

    void on_after_write(uv_write_t* req, int status);

protected:
    Server::Handler* parent_;
    IRequestHandler* handler_;

    std::unique_ptr<uv_stream_t> stream_;
    const UvProc* proc_;

    std::stringstream input_;
    std::stringstream output_;
    ConnectionContext ctx_;
};

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
    uv_close((uv_handle_t*)req->handle, cb_close);
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

UvRequest::UvRequest(Server::Handler* parent, IRequestHandler* handler, const UvProc* proc)
    :   parent_(parent),
        handler_(handler),
        proc_(proc),
        input_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
        output_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
        ctx_{StdInputStream(input_), StdOutputStream(output_)} {
    uv_tcp_t* stream = new uv_tcp_t;
    stream_.reset(reinterpret_cast<uv_stream_t*>(stream));
    int r = uv_tcp_init(proc_->loop(), stream);
    stream->data = this;
}

void UvRequest::accept() {
    int r = uv_accept(proc_->server(), stream_.get());
//    assert(r == 0);

//
    r = uv_read_start(stream_.get(), cb_alloc_buffer, cb_after_read);
//    assert(r == 0);


}

class Server::Handler {
public:
    Handler(Server* parent, uv_tcp_t* server);

    void run();

    void on_connection(int status);
    Response find_route(const String& url, UvRequest* req);

    UvProc& proc() { return proc_; }

protected:
    Server* parent_;

    UvProc proc_;

    std::vector<std::shared_ptr<UvRequest>> requests_;
};

class HttpRequestHandler : public IRequestHandler {
public:
    HttpRequestHandler(Server::Handler* parent);
    void handle(ConnectionContext context) override;

    int on_http_url(const char* at, size_t len);
    int on_http_header_field(const char* at, size_t len);
    int on_http_header_value(const char* at, size_t len);
    int on_http_headers_complete();

    void check_header_finished();

private:
    Server::Handler* parent_;

    std::unique_ptr<Request> request_;
    Response response_;
    std::unique_ptr<http_parser> parser_;

    typedef std::pair<String, String> RHeader;

    RHeader read_header_;
};

Server::Handler::Handler(Server* parent, uv_tcp_t* server)
    :   parent_(parent),
        proc_((uv_stream_t*) server, uv_default_loop()) {
}

void Server::Handler::on_connection(int status) {
    auto new_req = std::make_shared<UvRequest>(this, new HttpRequestHandler{this}, &proc_);
    new_req->accept();
    requests_.push_back(new_req);
}

void Server::Handler::run() {
    proc_.run();
}

Response Server::Handler::find_route(const String& url, UvRequest* req) {
//    for (auto&& route : parent_->routes_) {
//        //std::cout << "Url" << "_" << route.path << std::endl;
//        if (route.path == url) {
//            auto&& response = route.func(*req->request());
//            return response;
//        }
//    }
    return Response{};
}

void UvRequest::on_after_read(ssize_t nread, const uv_buf_t* buf) {
    std::cout << "Got " << nread << std::endl;

    input_.write(buf->base, nread);
    handler_->handle(ctx_);

    while (output_) {
        char tmp_buf[1024];
        output_.read(tmp_buf, sizeof(tmp_buf));
        size_t size = output_.gcount();
        write_req_t* wr = new write_req_t;
        wr->buf = uv_buf_init(tmp_buf, size);
        if (size < sizeof(tmp_buf)) {
            wr->close = true;
        }
        int r = uv_write(&wr->req, stream_.get(), &wr->buf, 1, cb_after_write);
    }

    if (nread <= 0 && buf->base != nullptr)
        delete[] buf->base;

    if (nread == 0)
        return;

    if (nread < 0) {
        /*assert(nread == UV_EOF);*/
        fprintf(stderr, "err: %s\n", uv_strerror(nread));

        uv_shutdown_t *shutdown = new uv_shutdown_t;
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
        uv_close((uv_handle_t *) req->handle, cb_close);
    }
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
    r = uv_tcp_bind(tcp_server, (const struct sockaddr*)&addr, 0);
    if (r != 0) {
        const char* error = uv_strerror(r);
        std::cerr << error << std::endl;
        throw std::runtime_error(error);
    }

    r = uv_listen((uv_stream_t*)tcp_server, SOMAXCONN, on_connection);
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
:   path(path),
    func(func) {
}

int cb_http_message_begin(http_parser* parser) {
    return 0;
}

int cb_http_url(http_parser* parser, const char* at, size_t len) {
    HttpRequestHandler& handler = *reinterpret_cast<HttpRequestHandler*>(parser->data);
    return handler.on_http_url(at, len);
}

int cb_http_status(http_parser* parser, const char* at, size_t len) {
    std::cerr << "Got unhandled status: " << String(at, len) << std::endl;
    return 0;
}

int cb_http_header_field(http_parser* parser, const char* at, size_t len) {
    HttpRequestHandler& handler = *reinterpret_cast<HttpRequestHandler*>(parser->data);
    return handler.on_http_header_field(at, len);
}

int cb_http_header_value(http_parser* parser, const char* at, size_t len) {
    HttpRequestHandler& handler = *reinterpret_cast<HttpRequestHandler*>(parser->data);
    return handler.on_http_header_value(at, len);
}

int cb_http_headers_complete(http_parser* parser) {
    HttpRequestHandler& handler = *reinterpret_cast<HttpRequestHandler*>(parser->data);
    return handler.on_http_headers_complete();
}

int cb_http_body(http_parser* parser, const char* at, size_t len) {
    std::cerr << "Got unhandled body: " << String(at, len) << std::endl;
    return 0;
}

int cb_http_message_complete(http_parser* parser) {
    std::cout << "Message complete!" << std::endl;
    return 0;
}

static http_parser_settings HttpSettings = {
    .on_message_begin    = cb_http_message_begin,
    .on_url              = cb_http_url,
    .on_status           = cb_http_status,
    .on_header_field     = cb_http_header_field,
    .on_header_value     = cb_http_header_value,
    .on_headers_complete = cb_http_headers_complete,
    .on_body             = cb_http_body,
    .on_message_complete = cb_http_message_complete
};

HttpRequestHandler::HttpRequestHandler(Server::Handler* parent)
    :   parent_(parent) {
    parser_.reset(new http_parser);
    http_parser_init(parser_.get(), HTTP_REQUEST);
    parser_.get()->data = this;

    request_.reset(new Request);
}

int HttpRequestHandler::on_http_url(const char* at, size_t len) {
    request_->url_ += String(at, len);
    return 0;
}

int HttpRequestHandler::on_http_header_field(const char* at, size_t len) {
    check_header_finished();
    read_header_.first += String(at, len);
    return 0;
}

int HttpRequestHandler::on_http_header_value(const char* at, size_t len) {
    read_header_.second += String(at, len);
    return 0;
}

int HttpRequestHandler::on_http_headers_complete() {
    check_header_finished();

    std::cout << "Headers" << std::endl;
    for (auto&& i : request_->headers_) {
        std::cout << i.first << ": " << i.second << std::endl;
    }
    return 0;
}

void HttpRequestHandler::handle(ConnectionContext context) {
    std::cout << "Handler!" << std::endl;
    char buf[1024];
    while (!context.input.eof()) {
        size_t read = context.input.read(buf, sizeof(buf));
        http_parser_execute(parser_.get(), &HttpSettings, buf, read);
    }

    request_->method_ = http_method_str(static_cast<http_method>(parser_.get()->method));

    std::cout << "Parsed: " << request_->url_ << std::endl;

    String resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nYah2o";
    context.output.write(resp.c_str(), resp.size());

    //response_ = parent_->find_route(request_->url_, this);

//    if (!response_.buf_.empty()) {
//        write_req_t* wr = new write_req_t;
//        size_t size = response_.buf_.size();
//        wr->buf = uv_buf_init(&response_.buf_[0], size);
//        r = uv_write(&wr->req, stream_.get(), &wr->buf, 1, cb_after_write);
//
//    }
    //assert(r == 0);
}

void HttpRequestHandler::check_header_finished() {
    if (!read_header_.first.empty() && !read_header_.second.empty()) {
        request_->headers_.insert(
            Header(std::move(read_header_.first), std::move(read_header_.second)));
    }
}