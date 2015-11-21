#pragma once

#include <http_parser.h>

#include "common.h"

struct ServerOptions {
    String host;
    int port;
    int worker_threads = 1;
};

typedef std::pair<String, String> Header;
class HttpRequestHandler;

class Request {
public:
    friend class UvRequest;
    friend class HttpRequestHandler;

    const String& url() const { return url_; }

private:
    String method_;
    String url_;
    bool url_ready_;

    std::multimap<String, String> headers_;
};

class Response {
public:
    friend class UvRequest;

    Response() {}
    Response(String&& buf) : buf_(buf) {}

private:
    String buf_;
};

struct Route {
public:
    typedef std::function<Response (const Request&)> Func;
    Route(String&& path, Func func);

//protected:
public:
    String method;
    String name;
    String path;
    std::function<Response (const Request&)> func;
};

class Server {
public:
    class Handler;
    friend class Handler;

    Server();
    Server(ServerOptions&& options);
    ~Server();

    void setup(ServerOptions&& options);

    void add_route(Route&& route);

    void run();

protected:
    Handler* handler_ = nullptr;

    std::unique_ptr<uv_tcp_t> tcp_server_;

    std::vector<Route> routes_;
};

struct ConnectionContext {
    StdInputStream input;
    StdOutputStream output;
};

class IRequestHandler {
public:
    virtual void handle(ConnectionContext context) = 0;

    virtual ~IRequestHandler() {}
};
