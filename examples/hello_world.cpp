#include <enji/http.h>

void index(const enji::HttpRequest& req, enji::HttpResponse& out) {
    out.add_headers({
        { "Content-Type", "text/html; charset=utf-8" },
    });
    out.body("Hello, world!\n");
}

int main(int argc, char* argv[]) {
    enji::ServerOptions opts;
    opts.port = 3001;
    enji::HttpServer server(std::move(opts));
    server.routes({
        { "/", index },
    });
    server.run();
    return 0;
}