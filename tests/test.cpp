#include <enji/server.h>

using namespace enji;

Response test(const Request& req) {
    return Response{"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nYah2o\n"};
}

int main() {
    ServerOptions opts;
    opts.port = 3001;
    Server server(std::move(opts));
    server.add_route(Route("/", test));
    server.run();
    return 0;
}