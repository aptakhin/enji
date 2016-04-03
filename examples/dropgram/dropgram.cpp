#include <enji/http.h>

#include <fstream>
#include <iomanip>
#include <functional>
#include <stdio.h>
#include <sqlite3.h>

using namespace enji;

sqlite3* db;
String WEBCACHE_DIR;

void index(const HttpRequest& req, HttpResponse& out) {
    static_file("index.html", out);
}

int select_callback(void* values_ptr, int argc, char** argv, char** col_name) {
    auto values = reinterpret_cast<Value*>(values_ptr);
    auto row = Value::make_dict();
    for (int i = 0; i < argc; i++) {
        if (argv[i]) {
            row[col_name[i]] = argv[i];
        }
    }
    values->array().push_back(row);
    return 0;
}

void api_view(const HttpRequest& req, HttpResponse& out) {
    char* err_msg = 0;
    auto grams = Value::make_array();
    sqlite3_exec(db, "SELECT * FROM grams;", select_callback, (void*)&grams, &err_msg);

    std::stringstream grams_json;
    grams_json << "{ " << std::quoted("grams") << ": [";

    for (size_t row_iter = 0; row_iter < grams.array().size(); ++row_iter) {
        Value& row = grams.array()[row_iter];
        grams_json << "{";
        grams_json << std::quoted("filename") << ": " << std::quoted(path_join("/", "gram", row["filename"].str())) << ", ";
        grams_json << std::quoted("published") << ": " << std::quoted(row["published"].str());
        grams_json << "}";
        if (row_iter + 1 < grams.array().size()) {
            grams_json << ", ";
        }
    }

    grams_json << "]}";
    out.body(grams_json);
}

void api_upload(const HttpRequest& req, HttpResponse& out) {
    for (auto&& file : req.files()) {
        std::ostringstream sql;
        
        std::ostringstream filename_buf;
        std::time_t timestamp = std::time(nullptr);
        filename_buf << std::hash<String>{}(file.filename()) << timestamp;
        filename_buf << "." << path_extension(file.filename());

        const auto filename = filename_buf.str();

        auto gram_file = std::ofstream{path_join(WEBCACHE_DIR, filename).c_str(), std::ios::binary};
        gram_file.write(&file.body().front(), file.body().length());
        gram_file.close();

        sql << "INSERT INTO grams (filename, published) VALUES ('";
        sql << filename << "', " << "datetime('now'));";
        //
        // FIXME: bind here
        //

        char* err_msg = 0;
        ZEROCHECK(sqlite3_exec(db, sql.str().c_str(), select_callback, 0, &err_msg),
            std::runtime_error, "Can't insert into grams", &err_msg);
    }

    temporary_redirect("/", out);
}

int main(int argc, char* argv[]) {
    ServerConfig["STATIC_ROOT_DIR"] = path_join(path_dirname(__FILE__), "static");

    char home_dir[260];
    size_t home_size = sizeof(home_dir);
    UVCHECK(uv_os_homedir(home_dir, &home_size),
         std::runtime_error, "Can't get user home dir");

    WEBCACHE_DIR = path_join(home_dir, "dropgram");
    uv_fs_t web_cache_dir_req;
    uv_fs_mkdir(nullptr, &web_cache_dir_req, WEBCACHE_DIR.c_str(), 0, nullptr);

    char* err_msg = 0;
    ZEROCHECK(sqlite3_open("test.db", &db),
        std::runtime_error, "Can't open database", &err_msg);
    auto close_db = Defer{[] { sqlite3_close(db); }};

    auto sql = "CREATE TABLE IF NOT EXISTS grams ("  \
        "id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL," \
        "filename CHAR(50)," \
        "published DATETIME);";

    ZEROCHECK(sqlite3_exec(db, sql, select_callback, 0, &err_msg),
        std::runtime_error, "Can't create grams table", &err_msg);

    ServerOptions opts;
    opts.port = 3001;
    opts.worker_threads = 4;
    HttpServer server(std::move(opts));
    server.routes({
        {"^/$", index},
        {"^/static/(.+)$", serve_static(match1_filename)},
        {"^/gram/(.+)$", serve_static(WEBCACHE_DIR, match1_filename)},
        {"^/api/view", api_view},
        {"^/api/upload", api_upload},
    });
    server.run();
    return 0;
}
