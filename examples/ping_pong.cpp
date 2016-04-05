#include <enji/http.h>

class PongConnection : public enji::Connection {
public:
    PongConnection(enji::Server* parent) : enji::Connection{parent, 0} {}

    void handle_input(enji::TransferBlock data) override {
        std::cout << "Got " << enji::String{data.data, data.data + data.size} << std::endl;
        auto mem = enji::TransferBlock{new char[data.size], data.size};
        std::memcpy((void*)mem.data, data.data, data.size);
        write_chunk(mem);
    }
};

int main(int argc, char* argv[]) {
    enji::ServerConfig["port"] = enji::Value{3001};
    enji::ServerConfig["worker_threads"] = enji::Value{4};
    enji::HttpServer server{enji::ServerConfig};
    server.create_connection([&server] { 
        return std::make_shared<PongConnection>(&server); 
    });
    server.run();
    return 0;
}