#pragma once

#include <http_parser.h>

#include "server.h"

namespace enji {

class HttpOutput;

class HttpRequest {
public:
    friend class HttpConnection;

    const String& url() const { return url_; }

    const String& body() const { return body_; }

private:
    String method_;
    String url_;
    bool url_ready_;

    std::multimap<String, String> headers_;

    String body_;
};

class HttpOutput {
public:
    HttpOutput(HttpConnection* conn);

    HttpOutput& header(const String& name, const String& value);
    HttpOutput& body(const String& value);

    void close();

private:
    HttpConnection* conn_;
};

struct HttpRoute {
public:
    typedef std::function<void(const HttpRequest&, HttpOutput&)> Handler;

    HttpRoute(String&& path, Handler handler);

    //protected:
public:
    String method;
    String name;
    String path;
    Handler handler;
};

class HttpServer : public Server {
public:
    HttpServer(ServerOptions&& options);

    void on_connection(int status) override;

    void add_route(HttpRoute&& route);

    void call_handler(const HttpRequest& request, HttpConnection* bind);

protected:
    std::vector<HttpRoute> routes_;
};

class HttpConnection : public Connection {
public:
    HttpConnection(HttpServer* parent, size_t id);

    void handle_input(StringView data) override;

    const HttpRequest& request() const;

    int on_http_url(const char* at, size_t len);

    int on_http_header_field(const char* at, size_t len);

    int on_http_header_value(const char* at, size_t len);

    int on_http_headers_complete();

    int on_http_body();
    int on_message_complete();

    void check_header_finished();

private:
    HttpServer* parent_;

    std::unique_ptr<HttpRequest> request_;
    //Response response_;
    std::unique_ptr<http_parser> parser_;

    typedef std::pair<String, String> RHeader;

    RHeader read_header_;
    bool message_completed_ = false;
};

} // namespace enji