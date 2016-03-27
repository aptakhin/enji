#include <enji/http.h>
#include <fstream>

using namespace enji;

int main(int argc, char* argv[]) {
    std::ofstream index_file{"index.html"};
    index_file << "<html><h2>Hello world!</h2>From index.html</html>";
    index_file.close();

    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.routes({
        {"^/(.+)$", serve_static(".", match1_filename)},
    });
    server.run();
    return 0;
}