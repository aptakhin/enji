#include <enji/http.h>
#include <fstream>

int main(int argc, char* argv[]) {
    std::ofstream index_file{"index.html"};
    index_file << "<html><h2>Hello world!</h2>From index.html</html>";
    index_file.close();

    enji::ServerConfig["port"] = enji::Value{3001};
    enji::ServerConfig["worker_threads"] = enji::Value{4};
    enji::HttpServer server{enji::ServerConfig};
    server.routes({
        {"^/(.+)$", enji::serve_static(".", enji::match1_filename)},
    });
    server.run();
    return 0;
}