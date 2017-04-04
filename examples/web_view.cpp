#include <enji/http.h>
#include <fstream>

using enji::ServerConfig;
using enji::HttpServer;

int main(int argc, char* argv[]) {
    std::ofstream index_file{"index.html"};
    index_file << "<html><h2>Hello world!</h2>From index.html</html>\n";
    index_file.close();

    ServerConfig["port"] = 3001;
    ServerConfig["worker_threads"] = 4;
    HttpServer server{ServerConfig};
    server.routes({
        {"^/(.+)$", enji::serve_static(".", enji::match1_filename)},
    });
    server.run();
    return 0;
}