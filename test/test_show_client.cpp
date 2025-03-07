/**
 * Contents:
 *
 *      HelloRequest
 *      HelloResponse
 *      HelloService
 *      HelloServer
 *      ClientApplication
 *          bind
 *          main_loop
 *          Hello
 *          Hello2
 */
#include "../src/nrpc_cpp.hpp"

$rpcclass(HelloRequest, $field(name, 1), $field(value, 2), $field(newonclient, 5));
class HelloRequest {
public:
    std::string name{0};
    int value{0};
    int newonclient{0};
};

$rpcclass(HelloResponse, $field(summary, 1), $field(echo, 2));
class HelloResponse {
public:
    std::string summary;
    HelloRequest echo;
};

$rpcclass(HelloService, $method(Hello, 1), $method(Hello2, 2));
class HelloService {
public:
    HelloResponse Hello(HelloRequest request) { return HelloResponse(); }
    nlohmann::json Hello2(nlohmann::json request) { return {}; }
};

class HelloClient : public nrpc_cpp::ServiceClientBase {
public:
    HelloClient(std::shared_ptr<nrpc_cpp::RoutingSocket> socket) : ServiceClientBase(socket) {}

    HelloResponse Hello(HelloRequest request) {
        return socket_->server_call("HelloService.Hello", request, std::shared_ptr<HelloResponse>());
    }

    nlohmann::json Hello2(nlohmann::json request) {
        return socket_->server_call("HelloService.Hello2", request, std::shared_ptr<nlohmann::json>());
    }
};

class ClientApplication {
public:
    ClientApplication() {
        cmd_ = nrpc_cpp::CommandLine({
            {"port", 9003},
            {"format", "json"},
            {"rate", 1.0},
            {"verbose", true},
            {"from_server", false},
        });
    }

    void connect() {
        std::cout << "Started client: " << (int)cmd_["port"] << std::endl;

        // clang-format off
        sock_ = std::make_shared<nrpc_cpp::RoutingSocket>(nrpc_cpp::RoutingSocketOptions({
            {"type", nrpc_cpp::SocketType::CONNECT},
            {"protocol", nrpc_cpp::ProtocolType::TCP},
            {"format", (std::string)cmd_["format"] == "json" ? nrpc_cpp::FormatType::JSON : nrpc_cpp::FormatType::BINARY},
            {"caller", "test_show_client_cpp"},
            {
                "types",
                {
                    nrpc_cpp::type<HelloRequest>(), 
                    nrpc_cpp::type<HelloResponse>(), 
                    nrpc_cpp::service<HelloService>()
                }
            }
        }));
        // clang-format on

        sock_->connect("127.0.0.1", (int)cmd_["port"], true, true);
    }

    void main_loop() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000 / (float)cmd_["rate"])));

            auto res = sock_->server_call("HelloService.Hello", {{"name", "tester1"}});
            std::cout << "SEND HelloService.Hello, 1, " << res << std::endl;

            auto req2 = HelloRequest();
            req2.name = "tester2";
            req2.value = 234;
            req2.newonclient = 555;
            auto res2 = sock_->server_call("HelloService.Hello", req2, std::shared_ptr<HelloResponse>());
            auto res2b = nrpc_cpp::construct_json(res2);
            std::cout << "SEND HelloService.Hello, 2, " << res2b << std::endl;

            auto client = sock_->cast<HelloClient>(sock_);
            auto req3 = HelloRequest();
            req3.name = "tester3";
            req3.value = 444;
            req3.newonclient = 555;
            auto res3 = client->Hello(req3);
            auto res3b = nrpc_cpp::construct_json(res3);
            std::cout << "SEND HelloService.Hello, 3, " << res3b << std::endl;

            auto res4 = client->Hello2({{"name", "tester4"}, {"value", 777}});
            std::cout << "SEND HelloService.Hello2, 4, " << res4 << std::endl;
        }

        sock_->close();
    }

private:
    nrpc_cpp::CommandLine cmd_;
    std::shared_ptr<nrpc_cpp::RoutingSocket> sock_;
};

int main(int argc, char* argv[]) {
    nrpc_cpp::init(argc, argv);
    ClientApplication app;
    app.connect();
    app.main_loop();
    return 0;
}
