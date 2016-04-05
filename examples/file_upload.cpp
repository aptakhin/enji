#include <enji/http.h>
#include <fstream>

void upload_file(const enji::HttpRequest& req, enji::HttpResponse& out) {
    for (auto&& file : req.files()) {
        std::stringstream buf;
        buf << "Got filename: " << file.filename() << 
            " with size " << file.body().size() << " bytes\n";
        out.body(std::move(buf));
    }
}

int main(int argc, char* argv[]) {
    enji::ServerConfig["port"] = enji::Value{3001};
    enji::ServerConfig["worker_threads"] = enji::Value{4};
    enji::HttpServer server{enji::ServerConfig};
    server.routes({
        {"^/upload/(.+)$", upload_file},
    });
    server.run();
    return 0;
}