#include <enji/http.h>

using namespace enji;

//Response test(const HttpRequest& req) {
//	return Response{ "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nYah2o\n" };
//}

class ListProjects {
public:
	void on_request() {
		// mongo load
		//
		//write(json.dump());
	}
};

void list_projects(const HttpRequest& req, HttpOutput& out) {
	// mongo load
	//
	//write(json.dump());
	out.body("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\nYah2o\n");
}

int main(int argc, char* argv[]) {
    ServerOptions opts;
    opts.port = 3001;
    HttpServer server(std::move(opts));
	server.add_route(HttpRoute{ "/", list_projects });
    server.run();
    return 0;
}