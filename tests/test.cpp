#include <enji/http.h>

using namespace enji;

void index(const HttpRequest& req, HttpOutput& out) {
    out.response();
    out.add_headers({
        { "Content-Type", "text/html; charset=utf-8" },
    });
    out.body("Hello, world!\n");
}

int main(int argc, char* argv[]) {
    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.routes({ 
        { "/", [] (const enji::HttpRequest& req, enji::HttpOutput& out) { return index(req, out); } },
    });
    server.run();
    return 0;
}