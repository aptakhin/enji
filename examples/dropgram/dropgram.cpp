#include <enji/http.h>

#include <stdio.h>
#include <sqlite3.h>

using namespace enji;

sqlite3* db;

void index(const HttpRequest& req, HttpResponse& out) {
    static_file("index.html", out);
}

void api_upload(const HttpRequest& req, HttpResponse& out) {

}

int main(int argc, char* argv[]) {
    ServerConfig["STATIC_ROOT_DIR"] = path_join(path_dirname(__FILE__), "static");

    ZEROCHECK(sqlite3_open("test.db", &db),
        std::runtime_error, "Can't open database");
    auto close_db = Defer{[] { sqlite3_close(db); }};

    char* err_msg = 0;

    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.routes({
        {"^/$", index},
        {"^/static/(.+)$", serve_static(match1_filename)},
        {"^/api/upload", api_upload},
    });
    server.run();
    return 0;
}
