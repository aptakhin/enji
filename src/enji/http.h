#pragma once

#include <http_parser.h>

#include "server.h"

namespace enji {

class HttpRequestHandler : public IRequestHandler {
public:
    HttpRequestHandler(Server::Handler* parent);

    void handle(ConnectionContext context) override;

    const Request& request() const override;

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


} // namespace enji