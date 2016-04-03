#pragma once

#include <http_parser.h>
#include <regex>
#include "server.h"

namespace enji {

class HttpConnection;

typedef std::pair<String, String> Header;

class File {
public:
    const String& name() const { return name_; }
    const String& filename() const { return filename_; }
    const String& body() const { return body_; }

public:
    String name_;
    String filename_;
    String body_;
};

class HttpRequest {
public:
    friend class HttpConnection;

    const String& method() const { return method_; }

    const String& url() const { return url_; }

    const String& body() const { return body_; }

    const std::vector<File>& files() const { return files_; }

    void set_match(const std::smatch& match) { match_ = match; }
    const std::smatch& match() const { return match_; }

private:
    String method_;
    String url_;
    bool url_ready_;

    std::smatch match_;

    std::multimap<String, String> headers_;

    String body_;

    std::vector<File> files_;
};

class HttpResponse {
public:
    HttpResponse(HttpConnection* conn);
    ~HttpResponse();

    HttpResponse& response(int code);
    HttpResponse& add_headers(std::vector<std::pair<String, String>> headers);
    HttpResponse& add_header(const String& name, const String& value);
    HttpResponse& body(const String& value);
    HttpResponse& body(std::stringstream& buf);
    HttpResponse& body(const void* data, size_t length);

    int code() const { return code_; }

    void flush();
    void close();

private:
    HttpConnection* conn_;

    std::stringstream response_;
    std::stringstream headers_;
    std::stringstream body_;

    std::stringstream full_response_;

    bool headers_sent_ = false;

    int code_ = 200;
};

struct HttpRoute {
public:
    typedef void (*FuncHandler)(const HttpRequest&, HttpResponse&);
    typedef std::function<void (const HttpRequest&, HttpResponse&)> Handler;

    HttpRoute(const char* path, Handler handler);
    HttpRoute(String&& path, Handler handler);

    HttpRoute(const char* path, FuncHandler handler);
    HttpRoute(String&& path, FuncHandler handler);

    std::smatch match(const String& url) const;

    void call_handler(const HttpRequest&, HttpResponse&);

private:
    String method_;
    String name_;
    String path_;
    Handler handler_;

    std::regex path_match_;
};

class HttpServer : public Server {
public:
    HttpServer(ServerOptions&& options);

    void routes(std::vector<HttpRoute>&& routes);
    void add_route(HttpRoute&& route);
    std::vector<HttpRoute>& routes() { return routes_; }
    const std::vector<HttpRoute>& routes() const { return routes_; }

    void call_handler(HttpRequest& request, HttpConnection* bind);

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

    int on_http_body(const char* at, size_t len);
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


String match1_filename(const HttpRequest& req);

HttpRoute::Handler serve_static(std::function<String(const HttpRequest& req)> request2file, const Config& config = ServerConfig);
HttpRoute::Handler serve_static(const String& root_dir, std::function<String(const HttpRequest& req)> request2file);

void static_file(const String& filename, HttpResponse& out, const Config& config = ServerConfig);
void response_file(const String& filename, HttpResponse& out);

void temporary_redirect(String redirect_to, HttpResponse& out);

} // namespace enji