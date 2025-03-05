/**
 * Contents:
 *
 *      HelloRequest
 *      HelloResponse
 *      HelloService
 *      HelloServer
 *      ServerApplication
 *          bind
 *          main_loop
 *          Hello
 *          Hello2
 */
#include "../src/nrpc_cpp.hpp"

$rpcclass(HelloRequest, $field(name, 1), $field(value, 2), $field(newonserver, 3));
class HelloRequest {
public:
    std::string name{0};
    int value{0};
    int newonserver{6};
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

class ServerApplication {
public:
    ServerApplication() {
        cmd_ = nrpc_cpp::CommandLine({
            {"port", 9000},
            {"format", "json"},
            {"rate", 1.0},
            {"verbose", true},
        });
    }

    void bind() {
        std::cout << "Started server: " << (int)cmd_["port"] << std::endl;

        // clang-format off
        sock_ = std::make_shared<nrpc_cpp::RoutingSocket>(nrpc_cpp::RoutingSocketOptions({
            {"type", nrpc_cpp::SocketType::BIND},
            {"protocol", nrpc_cpp::ProtocolType::TCP},
            {"format", (std::string)cmd_["format"] == "json" ? nrpc_cpp::FormatType::JSON : nrpc_cpp::FormatType::BINARY},
            {"caller", "test_server_cpp"},
            {
                "types",
                {
                    nrpc_cpp::type<HelloRequest>(),
                    nrpc_cpp::type<HelloResponse>(),
                    nrpc_cpp::service<HelloService>(),
                    nrpc_cpp::service<HelloService>("ServerApplication", this)
                }
            }
        }));
        // clang-format on

        sock_->bind("127.0.0.1", (int)cmd_["port"]);
    }

    void main_loop() {
        auto command = boost::str(
            boost::format("build\\bin\\test_show_client.exe rate=%1% port=%2% from_server=1") % ((float)cmd_["rate"]) % ((int)cmd_["port"])
        );
        system(command.c_str());

        // while (true) {
        //     std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000 / (float)cmd_["rate"])));
        //     std::vector<int> clients = {1, 2, 3, 4, 5, 6, 7, 8};

        //     for (auto client_id : clients) {
        //         auto active_ids = sock_->get_client_ids();
        //         if (!nrpc_cpp::find_contains(active_ids, [client_id](auto x) { return x == client_id; })) {
        //             continue;
        //         }
        //         auto res = sock_->client_call(client_id, nrpc_cpp::get_string(nrpc_cpp::RoutingMessage::GetAppInfo),
        //                                     nlohmann::json({}));
        //         if ((bool)cmd_["verbose"]) {
        //             std::cout << "Called client: GetAppInfo, " << client_id << ", " << ((std::string)res["entry_file"])
        //                     << ", " << ((std::string)res["this_socket"]) << std::endl;
        //         }
        //     }
        // }

        sock_->close();
    }

    /** HelloService's method */
    HelloResponse Hello(HelloRequest req) {
        std::cout << "CALL ServerApplication.Hello, name=" << req.name << ", value=" << req.value << std::endl;
        HelloResponse resp;
        resp.summary = "test";
        resp.echo = req;
        return resp;
    }

    /** HelloService's method */
    nlohmann::json Hello2(nlohmann::json req) {
        std::cout << "CALL ServerApplication.Hello2, " << req.dump() << std::endl;
        return {
            {"summary", "test2"},
            {"echo", req},
        };
    }

private:
    nrpc_cpp::CommandLine cmd_;
    std::shared_ptr<nrpc_cpp::RoutingSocket> sock_;
};

int main(int argc, char* argv[]) {
    nrpc_cpp::init(argc, argv);
    ServerApplication app;
    app.bind();
    app.main_loop();
    return 0;
}
