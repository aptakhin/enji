#include <enji/http.h>

using namespace enji;

void index(const HttpRequest& req, HttpOutput& out) {
    out.body("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nHello, world!\n");
}

int main(int argc, char* argv[]) {
    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.add_route(HttpRoute{ "/", index });
    server.run();
    return 0;
}