#include <enji/server.h>

using namespace enji;

Response test(const Request& req) {
    return Response{"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nYah2o\n"};
}

boost::context::fcontext_t fcm, fc1, fc2;


void f1(intptr_t) {
    std::cout << "f1: entered" << std::endl;
    std::cout << "f1: call jump_fcontext( & fc1, fc2, 0)" << std::endl;
    boost::context::jump_fcontext(&fc1, fc2, 0);
    std::cout << "f1: return" << std::endl;
    boost::context::jump_fcontext(&fc1, fcm, 0);
}

void f2(intptr_t) {
    std::cout << "f2: entered" << std::endl;
    std::cout << "f2: call jump_fcontext( & fc2, fc1, 0)" << std::endl;
    boost::context::jump_fcontext(&fc2, fc1, 0);
    BOOST_ASSERT(false && !"f2: never returns");
}

int main(int argc, char* argv[]) {
    std::size_t size(8192);
    void* sp1(std::malloc(size));
    void* sp2(std::malloc(size));

    fc1 = boost::context::make_fcontext(sp1, size, f1);
    fc2 = boost::context::make_fcontext(sp2, size, f2);

    std::cout << "main: call jump_fcontext( & fcm, fc1, 0)" << std::endl;
    boost::context::jump_fcontext(&fcm, fc1, 0);

    ServerOptions opts;
    opts.port = 3001;
    Server server(std::move(opts));
    server.add_route(Route("/", test));
    server.run();
    return 0;
}