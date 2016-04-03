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
    enji::ServerOptions opts;
    opts.port = 3001;
    enji::HttpServer server(std::move(opts));
    server.routes({
        {"^/upload/(.+)$", upload_file},
    });
    server.run();
    return 0;
}