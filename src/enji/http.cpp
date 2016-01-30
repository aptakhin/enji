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

HttpOutput::HttpOutput(HttpConnection* conn)
:   conn_(conn) {

}

HttpOutput& HttpOutput::header(const String& name, const String& value) {
    return *this;
}

HttpOutput& HttpOutput::body(const String& value) {
    if (!value.empty()) {
        OweMem mem = {new char[value.length()], value.length() };
        std::memcpy((void*)mem.data, &value.front(), value.length());
        conn_->write_chunk(mem);
    }
    return *this;
}

void HttpOutput::close() {
    conn_->close();
}

HttpRoute::HttpRoute(String&& path, Handler handler)
:   path(path),
    handler(handler) {
}

HttpServer::HttpServer(ServerOptions&& options)
:   Server(std::move(options)) {
}

void HttpServer::on_connection(int status) {
    auto new_connection = std::make_shared<HttpConnection>(this, counter_++);
    new_connection->accept();
    connections_.push_back(new_connection);
}

void HttpServer::add_route(HttpRoute&& route) {
    routes_.emplace_back(route);
}

void HttpServer::call_handler(const HttpRequest& request, HttpConnection* bind) {
    for (auto&& route : routes_) {
        if (route.path == request.url()) {
            HttpOutput out(bind);
            route.handler(request, out);
            out.close();
        }
    }
}

} // namespace enji