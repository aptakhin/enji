#include <enji/http.h>
#include <fstream>

using enji::HttpRequest;
using enji::HttpResponse;
using enji::ServerConfig;
using enji::HttpServer;

void upload_file(const HttpRequest& req, HttpResponse& out) {
    for (auto&& file : req.files()) {
        std::stringstream buf;
        buf << "Got filename: " << file.filename() << 
            " with size " << file.body().size() << " bytes\n";
        out.body(std::move(buf));
    }
}

int main(int argc, char* argv[]) {
    ServerConfig["port"] = 3001;
    ServerConfig["worker_threads"] = 4;
    HttpServer server{ServerConfig};
    server.routes({
        {"^/upload/(.+)$", upload_file},
    });
    server.run();
    return 0;
}