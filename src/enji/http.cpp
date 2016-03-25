#include "http.h"

namespace enji {

int cb_http_message_begin(http_parser* parser) {
    return 0;
}

int cb_http_url(http_parser* parser, const char* at, size_t len) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_http_url(at, len);
}

int cb_http_status(http_parser* parser, const char* at, size_t len) {
    std::cerr << "Got unhandled status: " << String(at, len) << std::endl;
    return 0;
}

int cb_http_header_field(http_parser* parser, const char* at, size_t len) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_http_header_field(at, len);
}

int cb_http_header_value(http_parser* parser, const char* at, size_t len) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_http_header_value(at, len);
}

int cb_http_headers_complete(http_parser* parser) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_http_headers_complete();
}

int cb_http_body(http_parser* parser, const char* at, size_t len) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_http_body();
}

int cb_http_message_complete(http_parser* parser) {
    HttpConnection& handler = *reinterpret_cast<HttpConnection*>(parser->data);
    return handler.on_message_complete();
}

http_parser_settings& get_http_settings() {
    static http_parser_settings http_settings = {};
    http_settings.on_message_begin = cb_http_message_begin;
    http_settings.on_url = cb_http_url;
    http_settings.on_status = cb_http_status;
    http_settings.on_header_field = cb_http_header_field;
    http_settings.on_header_value = cb_http_header_value;
    http_settings.on_headers_complete = cb_http_headers_complete;
    http_settings.on_body = cb_http_body;
    http_settings.on_message_complete = cb_http_message_complete;
    return http_settings;
}

HttpConnection::HttpConnection(HttpServer* parent, size_t id)
:   Connection(parent, id),
    parent_(parent) {
    parser_.reset(new http_parser);
    http_parser_init(parser_.get(), HTTP_REQUEST);
    parser_.get()->data = this;

    request_.reset(new HttpRequest{});
}

const HttpRequest& HttpConnection::request() const {
    return *request_.get();
}

int HttpConnection::on_http_url(const char* at, size_t len) {
    request_->url_ += String(at, len);
    return 0;
}

int HttpConnection::on_http_header_field(const char* at, size_t len) {
    check_header_finished();
    read_header_.first += String(at, len);
    return 0;
}

int HttpConnection::on_http_header_value(const char* at, size_t len) {
    read_header_.second += String(at, len);
    return 0;
}

int HttpConnection::on_http_headers_complete() {
    check_header_finished();
    return 0;
}

int HttpConnection::on_http_body() {
    return 0;
}

int HttpConnection::on_message_complete() {
    message_completed_ = true;
    return 0;
}

void HttpConnection::handle_input(StringView data) {
    //std::cout << String(data.data, data.data + data.size);
    http_parser_execute(parser_.get(), &get_http_settings(), data.data, data.size);
    
    if (message_completed_) {
        request_->method_ = http_method_str(static_cast<http_method>(parser_.get()->method));
        parent_->call_handler(*request_.get(), this);
    }
}

void HttpConnection::check_header_finished() {
    if (!read_header_.first.empty() && !read_header_.second.empty()) {
        request_->headers_.insert(
            Header(std::move(read_header_.first), std::move(read_header_.second)));
    }
}

HttpResponse::HttpResponse(HttpConnection* conn)
:   conn_(conn),
    response_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
    headers_(std::stringstream::in | std::stringstream::out | std::stringstream::binary), 
    body_(std::stringstream::in | std::stringstream::out | std::stringstream::binary),
    full_response_(std::stringstream::in | std::stringstream::out | std::stringstream::binary) {
}

HttpResponse::~HttpResponse() {
    close();
}

HttpResponse& HttpResponse::response(int code) {
    response_ << "HTTP/1.1 " << code << "\r\n";
    return *this;
}

HttpResponse& HttpResponse::add_headers(std::vector<std::pair<String, String>> headers) {
    for (auto&& h : headers) {
        add_header(h.first, h.second);
    }
    return *this;
}

HttpResponse& HttpResponse::add_header(const String& name, const String& value) {
    if (headers_sent_) {
        throw std::runtime_error("Can add headers to response. Headers already sent");
    }
    headers_ << name << ": " << value << "\r\n";
    return *this;
}

HttpResponse& HttpResponse::body(const String& value) {
    body_ << value;
    return *this;
}

bool stream2stream(std::stringstream& output, std::stringstream& input) {
    const size_t alloc_block = 4096;
    char tmp[alloc_block];
    bool written_smth = false;
    while (input) {
        input.read(tmp, sizeof(tmp));
        size_t size = input.gcount();
        output.write(tmp, size);
        if (size) {
            written_smth = true;
        }
    }
    return written_smth;
}

void stream2conn(Connection* conn, std::stringstream& buf) {
    while (buf) {
        size_t alloc_block = 4096;
        char* data = new char[alloc_block];
        buf.read(data, alloc_block);
        size_t size = buf.gcount();
        OweMem mem = { data, size };
        conn->write_chunk(mem);
    }
}

void HttpResponse::flush() {
    if (!headers_sent_) {
        if (!stream2stream(full_response_, response_)) {
            full_response_ << "HTTP/1.1 200\r\n";
        }

        body_.seekp(0, std::ios::end);
        auto body_size = body_.tellp();

        std::ostringstream body_size_stream;
        body_size_stream << body_size;
        add_header("Content-length", body_size_stream.str());

        stream2stream(full_response_, headers_);
        headers_sent_ = true;

        full_response_ << "\r\n";
    }

    stream2stream(full_response_, body_);
    stream2conn(conn_, full_response_);
}

void HttpResponse::close() {
    flush();
    conn_->close();
}

HttpRoute::HttpRoute(const char* path, Handler handler)
:   path(path),
    handler(handler) {
}

HttpRoute::HttpRoute(String&& path, Handler handler)
:   path(path),
    handler(handler) {
}

HttpRoute::HttpRoute(const char* path, FuncHandler handler)
:   path(path),
    handler(handler) {
}

HttpRoute::HttpRoute(String&& path, FuncHandler handler)
:   path(path),
    handler(handler) {
}

HttpServer::HttpServer(ServerOptions&& options)
:   Server(std::move(options)) {
    create_connection([this]() {
        return std::make_shared<HttpConnection>(this, counter_++); });
}

void HttpServer::routes(std::vector<HttpRoute>&& routes) {
    routes_ = std::move(routes);
}

void HttpServer::add_route(HttpRoute&& route) {
    routes_.emplace_back(route);
}

bool route_matches(const HttpRequest& request, const HttpRoute& route) {
    return route.path == request.url();
}

void HttpServer::call_handler(const HttpRequest& request, HttpConnection* bind) {
    for (auto&& route : routes_) {
        if (route_matches(request, route)) {
            HttpResponse out(bind);
            route.handler(request, out);
            out.close();
        }
    }
}

} // namespace enji