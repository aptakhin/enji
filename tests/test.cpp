#include <enji/server.h>

Response test(const Request& req) {
    return Response{};
}

int main() {
    ServerOptions opts;
    opts.port = 3001;
    Server server(std::move(opts));
    server.add_route(Route("/", test));
    server.run();
    return 0;
}