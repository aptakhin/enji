#include <enji/http.h>

using enji::Server;
using enji::Connection;
using enji::TransferBlock;
using enji::ServerConfig;
using enji::HttpServer;
using enji::String;

class PongConnection : public Connection {
public:
    PongConnection(Server* parent) : Connection{parent, 0} {}

    void handle_input(TransferBlock data) override {
        std::cout << "Got " << String{data.data, data.data + data.size} << std::endl;
        auto mem = TransferBlock{new char[data.size], data.size};
        std::memcpy((void*)mem.data, data.data, data.size);
        write_chunk(mem);
    }
};

int main(int argc, char* argv[]) {
    ServerConfig["port"] = 3001;
    ServerConfig["worker_threads"] = 4;
    HttpServer server{ServerConfig};
    server.create_connection([&server] {
        return std::make_shared<PongConnection>(&server);
    });
    server.run();
    return 0;
}