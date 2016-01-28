#include "http.h"

namespace enji {

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
    HttpRequestHandler& handler = *reinterpret_cast<HttpRequestHandler*>(parser->data);
    return handler.on_http_body();
}

int cb_http_message_complete(http_parser* parser) {
    return 0;
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

HttpRequestHandler::HttpRequestHandler(Server::Handler* parent)
    : parent_(parent) {
    parser_.reset(new http_parser);
    http_parser_init(parser_.get(), HTTP_REQUEST);
    parser_.get()->data = this;

    request_.reset(new Request);
}

const Request& HttpRequestHandler::request() const {
    return *request_.get();
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
    return 0;
}

int HttpRequestHandler::on_http_body() {
    return 0;
}

void HttpRequestHandler::handle(ConnectionContext context) {
    char buf[1024];
    while (!context.input.eof()) {
        size_t read = context.input.read(buf, sizeof(buf));
        http_parser_execute(parser_.get(), &get_http_settings(), buf, read);
    }

    /*
     * std::future
     */

    request_->method_ = http_method_str(static_cast<http_method>(parser_.get()->method));

    response_ = parent_->find_route(request_->url_, this);

    context.output.write(&response_.buf_[0], response_.buf_.size());
}

void HttpRequestHandler::check_header_finished() {
    if (!read_header_.first.empty() && !read_header_.second.empty()) {
        request_->headers_.insert(
            Header(std::move(read_header_.first), std::move(read_header_.second)));
    }
}


} // namespace enji