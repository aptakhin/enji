#include <enji/http.h>

void index(const enji::HttpRequest& req, enji::HttpResponse& out) {
    out.add_headers({
        { "Content-Type", "text/html; charset=utf-8" },
    });
    out.body("Hello, world!\n");
}

int main(int argc, char* argv[]) {
    enji::ServerConfig["port"] = enji::Value{3001};
    enji::ServerConfig["worker_threads"] = enji::Value{4};
    enji::HttpServer server{enji::ServerConfig};
    server.routes({
        { "/", index },
    });
    server.run();
    return 0;
}