#include <enji/http.h>
#include <fstream>

using namespace enji;

void upload_file(const HttpRequest& req, HttpResponse& out) {
    for (auto&& file : req.files()) {
        std::stringstream buf;
        buf << "Got filename: " << file.filename() << 
            " with size " << file.body().size() << " bytes\n";
        out.body(std::move(buf));
    }
}

int main(int argc, char* argv[]) {
    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.routes({
        {"^/upload/(.+)$", upload_file},
    });
    server.run();
    return 0;
}