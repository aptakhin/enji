#include <enji/http.h>
#include <fcntl.h>
#include <io.h>
#include <fstream>

using namespace enji;

void static_files(const HttpRequest& req, HttpResponse& out) {
    uv_fs_t open_req;
    const auto&& filename = req.match()[1].str();
    uv_fs_open(nullptr, &open_req, filename.c_str(), O_RDONLY, _S_IREAD, nullptr);
    const auto fd = static_cast<uv_file>(open_req.result);

    if (fd < 0) {
        out.response(404);
        return;
    }

    auto open_req_exit = Defer{[&open_req] { uv_fs_req_cleanup(&open_req); }};
   
    uv_fs_t read_req;
    std::vector<char> mem;
    mem.resize(4096);
    while (true) {
        uv_buf_t buf[] = {uv_buf_init(&mem.front(), static_cast<unsigned int>(mem.size()))};
        int read = uv_fs_read(nullptr, &read_req, fd, buf, 1, 0, nullptr);
        auto read_req_exit = Defer{[&read_req] { uv_fs_req_cleanup(&read_req); }};
        out.body(buf[0].base, read);
        if (read < buf[0].len) {
            break;
        }
    }

    uv_fs_t close_req;
    uv_fs_close(nullptr, &close_req, fd, nullptr);
    auto close_req_exit = Defer{[&close_req] { uv_fs_req_cleanup(&close_req); }};
}

int main(int argc, char* argv[]) {
    std::ofstream index_file{"index.html"};
    index_file << "<html><h2>Hello world!</h2>From index.html</html>";
    index_file.close();

    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
    server.routes({
        {"^/(.+)$", static_files},
    });
    server.run();
    return 0;
}