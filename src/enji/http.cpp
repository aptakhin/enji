#include "http.h"
#include <fcntl.h>
#include <fstream>

#ifdef _WIN32
#   include <io.h>
#endif

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
    return handler.on_http_body(at, len);
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
    parser_.reset(new http_parser{});
    http_parser_init(parser_.get(), HTTP_REQUEST);
    parser_.get()->data = this;

    request_.reset(new HttpRequest{});
}

const HttpRequest& HttpConnection::request() const {
    return *request_.get();
}

int HttpConnection::on_http_url(const char* at, size_t len) {
    request_->url_ += String{at, len};
    return 0;
}

int HttpConnection::on_http_header_field(const char* at, size_t len) {
    check_header_finished();
    read_header_.first += String{at, len};
    return 0;
}

int HttpConnection::on_http_header_value(const char* at, size_t len) {
    read_header_.second += String{at, len};
    return 0;
}

int HttpConnection::on_http_headers_complete() {
    check_header_finished();
    return 0;
}

int HttpConnection::on_http_body(const char* at, size_t len) {
    request_->body_ += String{at, len};
    return 0;
}

const String MULTIPART_FORM_DATA = "multipart/form-data; boundary=";

int HttpConnection::on_message_complete() {
    message_completed_ = true;

    auto expect_iter = request_->headers_.find("Expect");

    if (expect_iter != request_->headers_.end()) {
        if (expect_iter->second == "100-continue") {
            message_completed_ = false;
            std::ostringstream buf;
            buf << "HTTP/1.1 100 Continue\r\n";
            write_chunk(buf);
        }
    }

    auto content_type_iter = request_->headers_.find("Content-Type");
    if (content_type_iter != request_->headers_.end()) {
        String boundary;
        auto multipart = content_type_iter->second.find(MULTIPART_FORM_DATA);
        if (multipart != String::npos) {
            auto boundary_start = multipart + MULTIPART_FORM_DATA.size();
            boundary = content_type_iter->second.substr(boundary_start);
        }

        std::vector<size_t> occurrences;
        size_t start = 0;

        boundary = "--" + boundary;

        while ((start = request_->body_.find(boundary, start)) != String::npos) {
            occurrences.push_back(start);
            start += boundary.length();
        }

        const char* body = &request_->body_.front();
        for (size_t occurence = 1; occurence < occurrences.size(); ++occurence) {
            auto file_content = String{
                body + occurrences[occurence - 1] + boundary.length() + 2, // Skip boundary + "\r\n"
                body + occurrences[occurence]
            };

            auto headers_finished = file_content.find("\r\n\r\n");
            auto headers = file_content.substr(0, headers_finished);

            std::regex regex("Content-Disposition: form-data; name=\"(.+)\"; filename=\"(.+)\"");
            std::smatch match_groups;
            std::regex_search(headers, match_groups, regex);

            File file;
           
            if (!match_groups.empty()) {
                file.name_ = match_groups[1].str();
                file.filename_ = match_groups[2].str();
            }

            if (file.name_.empty() && file.filename_.empty()) {
                continue;
            }
            
            auto file_body = file_content.substr(headers_finished + 4);

            file.body_ = std::move(file_body);

            request_->files_.emplace_back(std::move(file));
        }
    }

    return 0;
}

void HttpConnection::handle_input(TransferBlock data) {
    http_parser_execute(parser_.get(), &get_http_settings(), data.data, data.size);
    
    if (message_completed_) {
        request_->method_ = http_method_str(static_cast<http_method>(parser_.get()->method));
        parent_->call_handler(*request_.get(), this);
    }
}

void HttpConnection::check_header_finished() {
    if (!read_header_.first.empty() && !read_header_.second.empty()) {
        request_->headers_.insert(Header{std::move(read_header_)});
    }
}

HttpResponse::HttpResponse(HttpConnection* conn)
:   conn_{conn},
    response_{std::stringstream::in | std::stringstream::out | std::stringstream::binary},
    headers_{std::stringstream::in | std::stringstream::out | std::stringstream::binary},
    body_{std::stringstream::in | std::stringstream::out | std::stringstream::binary},
    full_response_{std::stringstream::in | std::stringstream::out | std::stringstream::binary} {
}

HttpResponse::~HttpResponse() {
    close();
}

HttpResponse& HttpResponse::response(int code) {
    code_ = code;
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
        throw std::runtime_error("Can't add headers to response. Headers already sent");
    }
    headers_ << name << ": " << value << "\r\n";
    return *this;
}

bool stream2stream(std::stringstream& output, std::stringstream& input) {
    const size_t alloc_block = 32 * 1024;
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
        size_t alloc_block = 32 * 1024;
        char* data = new char[alloc_block];
        buf.read(data, alloc_block);
        const size_t size = buf.gcount();
        TransferBlock block = {data, size};
        conn->write_chunk(block);
    }
}

HttpResponse& HttpResponse::body(const String& value) {
    body_ << value;
    return *this;
}

HttpResponse& HttpResponse::body(std::stringstream& buf) {
    stream2stream(body_, buf);
    return *this;
}

HttpResponse& HttpResponse::body(const void* data, size_t length) {
    body_.write(static_cast<const char*>(data), length);
    return *this;
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
:   path_{path},
    handler_{handler},
    path_match_(path) {
}

HttpRoute::HttpRoute(String&& path, Handler handler)
:   path_{path},
    handler_{handler},
    path_match_(path) {
}

HttpRoute::HttpRoute(const char* path, FuncHandler handler)
:   path_{path},
    handler_{handler},
    path_match_(path) {
}

HttpRoute::HttpRoute(String&& path, FuncHandler handler)
:   path_{path},
    handler_{handler},
    path_match_(path) {
}

std::smatch HttpRoute::match(const String& url) const {
    std::smatch match_groups;
    std::regex_search(url, match_groups, path_match_);
    return match_groups;
}

void HttpRoute::call_handler(const HttpRequest& req, HttpResponse& out) {
    handler_(req, out);
}

HttpServer::HttpServer(ServerOptions&& options)
:   Server{std::move(options)} {
    create_connection([this]() {
        return std::make_shared<HttpConnection>(this, counter_++); });
}

void HttpServer::routes(std::vector<HttpRoute>&& routes) {
    routes_ = std::move(routes);
}

void HttpServer::add_route(HttpRoute&& route) {
    routes_.emplace_back(route);
}

void HttpServer::call_handler(HttpRequest& request, HttpConnection* bind) {
    bool matched = false;
    HttpResponse out{bind};

    for (auto&& route : routes_) {
        auto matches = route.match(request.url());
        if (!matches.empty()) {
            request.set_match(matches);
            route.call_handler(request, out);
            matched = true;
        }
    }

    if (!matched) {
        out.response(404);
    }

    std::cout << request.method() << " " << request.url() << " " << out.code() << std::endl;
}

String match1_filename(const HttpRequest& req) {
   return req.match()[1].str();
}

HttpRoute::Handler serve_static(std::function<String(const HttpRequest& req)> request2file, const Config& config) {
    return HttpRoute::Handler{
        [&] (const HttpRequest& req, HttpResponse& out)
    {
        static_file(request2file(req), out, config);
    }};
}

HttpRoute::Handler serve_static(const String& root_dir, std::function<String(const HttpRequest& req)> request2file) {
    return HttpRoute::Handler{
        [&] (const HttpRequest& req, HttpResponse& out)
    {
        const auto request_file = request2file(req);
        response_file(path_join(root_dir, request_file), out);
    }};
}

void static_file(const String& filename, HttpResponse& out, const Config& config) {
    response_file(path_join(config["STATIC_ROOT_DIR"].str(), filename), out);
}

void response_file(const String& filename, HttpResponse& out) {
    uv_fs_t open_req;

#ifdef _WIN32
    int open_mode = _S_IREAD;
#else
    int open_mode = S_IRUSR;
#endif
    
    uv_fs_open(nullptr, &open_req, filename.c_str(), O_RDONLY, open_mode, nullptr);
    auto open_req_exit = Defer{[&open_req] { uv_fs_req_cleanup(&open_req); }};
    const auto fd = static_cast<uv_file>(open_req.result);

    if (fd < 0) {
        out.response(404);
        return;
    }

    uv_fs_t read_req;
    const size_t alloc_block = 64 * 1024;
    size_t offset = 0;
    char mem[alloc_block];
    while (true) {
        uv_buf_t buf[] = {uv_buf_init(mem, sizeof(mem))};
        int read = uv_fs_read(nullptr, &read_req, fd, buf, 1, offset, nullptr);
        auto read_req_exit = Defer{[&read_req] { uv_fs_req_cleanup(&read_req); }};
        out.body(buf[0].base, read);
        offset += read;
        if (static_cast<unsigned int>(read) < buf[0].len) {
            break;
        }
    }

    uv_fs_t close_req;
    uv_fs_close(nullptr, &close_req, fd, nullptr);
    auto close_req_exit = Defer{[&close_req] { uv_fs_req_cleanup(&close_req); }};
}

void temporary_redirect(const String& redirect_to, HttpResponse& out) {
    out.response(307);
    out.add_header("Location", redirect_to);
    out.close();
}

} // namespace enji