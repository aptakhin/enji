#pragma once

#include <http_parser.h>

#include "server.h"

namespace enji {

class HttpConnection;

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

class HttpResponse {
public:
    HttpResponse(HttpConnection* conn);
    ~HttpResponse();

    HttpResponse& response(int code=200);
    HttpResponse& add_headers(std::vector<std::pair<String, String>> headers);
    HttpResponse& add_header(const String& name, const String& value);
    HttpResponse& body(const String& value);

    void flush();
    void close();

private:
    HttpConnection* conn_;

    std::stringstream response_;
    std::stringstream headers_;
    std::stringstream body_;

    std::stringstream full_response_;

    bool headers_sent_ = false;
};

struct HttpRoute {
public:
    typedef void (*FuncHandler)(const HttpRequest&, HttpResponse&);
    typedef std::function<void(const HttpRequest&, HttpResponse&)> Handler;

    HttpRoute(const char* path, Handler handler);
    HttpRoute(String&& path, Handler handler);

    HttpRoute(const char* path, FuncHandler handler);
    HttpRoute(String&& path, FuncHandler handler);

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

    void routes(std::vector<HttpRoute>&& routes);
    void add_route(HttpRoute&& route);
    std::vector<HttpRoute>& routes() { return routes_; }
    const std::vector<HttpRoute>& routes() const { return routes_; }

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

    std::unique_ptr<http_parser> parser_;

    typedef std::pair<String, String> RHeader;

    RHeader read_header_;
    bool message_completed_ = false;
};

} // namespace enji