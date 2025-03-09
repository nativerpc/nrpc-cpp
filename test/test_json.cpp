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

class TestApplication {
public:
    void start() {
        nrpc_cpp::CommandLine cmd({
            {"value", 1},
        });

        nlohmann::json empty = nlohmann::json::array();
        nlohmann::json test = {{"name", "tester1"}, {"empty", empty}};
        std::cout << "SIMPLE " << test.dump() << std::endl;
        std::cout << "TEST value=" << cmd["value"] << std::endl;
        TestClass x;
        x.name = "hello";
        x.value = (int)cmd["value"];
        nlohmann::json y;
        TestClass z;
        nrpc_cpp::assign_values(nrpc_cpp::type<TestClass>(), reinterpret_cast<uint8_t*>(&x), 0, sizeof(x), y, 1);
        nrpc_cpp::assign_values(nrpc_cpp::type<TestClass>(), reinterpret_cast<uint8_t*>(&z), 0, sizeof(x), y, 0);
        std::cout << "COPY " << y << std::endl;
        std::cout << "COPY {name=" << z.name << ", value=" << z.value << ", newonserver=" << z.newonserver << "}"
                  << std::endl;
        std::cout << "CONV " << nrpc_cpp::construct_json(z) << std::endl;
        auto z2 = nrpc_cpp::construct_item<TestClass>(y);
        std::cout << "CONV " << nrpc_cpp::construct_json(z2) << std::endl;
    }
};

int main(int argc, char* argv[]) {
    nrpc_cpp::init(argc, argv);
    TestApplication app;
    app.start();
    return 0;
}
