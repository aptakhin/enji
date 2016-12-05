#include <enji/http.h>

using namespace enji;

void index(const HttpRequest& req, HttpResponse& out) {
    out.add_headers({
        { "Content-Type", "text/html; charset=utf-8" },
    });
    out.body("Hello, world!\n");
}

int main(int argc, char* argv[]) {
    ServerConfig["port"] = 3001;
    ServerConfig["worker_threads"] = 1;
    HttpServer server{ServerConfig};
    server.routes({
        { "/", index },
    });
    server.run();
    return 0;
}
