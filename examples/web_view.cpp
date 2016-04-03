#include <enji/http.h>
#include <fstream>

int main(int argc, char* argv[]) {
    std::ofstream index_file{"index.html"};
    index_file << "<html><h2>Hello world!</h2>From index.html</html>";
    index_file.close();

    enji::ServerOptions opts;
    opts.port = 3001;
    enji::HttpServer server(std::move(opts));
    server.routes({
        {"^/(.+)$", enji::serve_static(".", enji::match1_filename)},
    });
    server.run();
    return 0;
}