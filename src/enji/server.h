#pragma once

#include "common.h"
#include "coroutines.h"

#include <boost/context/all.hpp>

namespace enji {

class UvRequest;

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

    friend class HttpRequestHandler;

    Response() { }

    Response(String&& buf) : buf_(buf) { }

private:
    String buf_;
};

struct Route {
public:
    typedef std::function<Response(const Request&)> Func;

    Route(String&& path, Func func);

//protected:
public:
    String method;
    String name;
    String path;
    std::function<Response(const Request&)> func;
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

    virtual const Request& request() const = 0;

    virtual ~IRequestHandler() { }
};

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

enum SignalAddType {
    NONE,
    CLOSE,
    CLOSE_CONFIRMED,
};

struct SignalEvent {
    UvRequest* recv;
    uv_buf_t buf;
    SignalAddType signal;
};

void switch2handler(UvRequest* request);

void switch2loop();

void handleri(intptr_t in);

class Server::Handler {
public:
    friend class UvRequest;
    friend void handleri(intptr_t in);

    Handler(Server* parent, uv_tcp_t* server);

    void run();

    void on_connection(int status);
    void on_loop();

    Response find_route(const String& url, IRequestHandler* handler);

    UvProc& proc() { return proc_; }

    void request_finished(UvRequest* pRequest);

protected:
    Server* parent_;

    UvProc proc_;

    ScopePtrExit<uv_idle_t> on_loop_;

    std::vector<std::shared_ptr<UvRequest>> requests_;

    SafeQueue<SignalEvent> input_queue_;
    SafeQueue<SignalEvent> output_queue_;

    std::vector<std::thread> threads_;

};

class UvRequest {
public:
    friend void switch2handler(UvRequest* request);
    friend void switch2loop();
    friend void handleri(intptr_t in);
    friend class Server::Handler;

    UvRequest(Server::Handler* parent, IRequestHandler* handler, const UvProc* proc);

    uv_stream_t* tcp() const { return stream_.get(); }

    void accept();

    void on_after_read(ssize_t nread, const uv_buf_t* buf);

    void on_after_write(uv_write_t* req, int status);

    void handle_event(SignalEvent&& event);

    void notify_closed();

protected:
    Server::Handler* parent_;
    IRequestHandler* handler_;

    std::unique_ptr<uv_stream_t> stream_;
    const UvProc* proc_;

    std::stringstream input_;
    std::stringstream output_;
    ConnectionContext ctx_;

    std::unique_ptr<char> handler_ctx_stack_;
    boost::context::fcontext_t handler_ctx_;

};

} // namespace enji