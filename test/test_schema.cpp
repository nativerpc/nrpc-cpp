#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

#include "../src/nrpc_cpp.hpp"

$rpcclass(TestClass, $field(name, 1), $field(value, 2), $field(newonserver, 3));
class TestClass {
public:
    std::string name;
    int value{0};
    int newonserver{0};
};

$rpcclass(TestService, $method(Hello, 1), $method(Hello2, 2));
class TestService {
public:
    TestClass Hello(TestClass request) { return TestClass(); }
    TestClass Hello2(TestClass request) { return TestClass(); }
};

class TestApplication {
public:
    void start() {
        auto sock = std::make_shared<nrpc_cpp::RoutingSocket>(nrpc_cpp::RoutingSocketOptions({
            {"type", nrpc_cpp::SocketType::CONNECT},
            {"protocol", nrpc_cpp::ProtocolType::TCP},
            {"format", nrpc_cpp::FormatType::JSON},
            {
                "types",
                {
                    nrpc_cpp::type<TestClass>(), 
                    nrpc_cpp::service<TestService>("TestApplication", this),
                }
            },
            {"caller", "test_schema_cpp"}
        }));

        std::cout << "Schema:" << std::endl;
        for (auto kvp : sock->get_types()) {
            std::cout << "    TYPE name=" << kvp.first << " size=" << kvp.second.size << std::endl;
            for (auto kvp2 : kvp.second.fields) {
                auto& field_info = kvp2.second;
                std::cout << "        FIELD name=" << field_info.field_name << " type=" << field_info.field_type_str << " id="
                        << field_info.id_value << " offset=" << field_info.offset << " size=" << field_info.size << std::endl;
            }
        }
        std::cout << std::endl;

        for (auto kvp : sock->get_services()) {
            std::cout << "    SERVICE name=" << kvp.first << " methods=" << kvp.second.methods.size() << std::endl;
            for (auto kvp2 : kvp.second.methods) {
                auto& method_info = kvp2.second;
                std::cout << "        METHOD name=" << method_info.method_name << " req=" << method_info.request_type
                        << " res=" << method_info.response_type << " id=" << method_info.id_value << std::endl;
            }
        }
        std::cout << std::endl;

        for (auto kvp : sock->get_servers()) {
            std::cout << "    SERVER name=" << kvp.first << " methods=" << kvp.second.methods.size() << std::endl;
            for (auto kvp2 : kvp.second.methods) {
                auto& method_info = kvp2.second;
                std::cout << "        METHOD name=" << method_info.method_name << " req=" << method_info.request_type
                        << " res=" << method_info.response_type << " id=" << method_info.id_value << std::endl;
            }
        }
        std::cout << std::endl;
    }

    TestClass Hello(TestClass request) { return TestClass(); }
    TestClass Hello2(TestClass request) { return TestClass(); }
};

int main(int argc, char* argv[]) {
    nrpc_cpp::init(argc, argv);
    TestApplication app;
    app.start();
    return 0;
}
