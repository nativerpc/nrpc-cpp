#include <zmq.h>

#include <iostream>
#include <memory>
#include <string>
#include "../src/nrpc_cpp.hpp"

class TestApplication {
public:
    void start() {
        auto context = zmq_ctx_new();
        std::cout << "hello world! " << reinterpret_cast<uint64_t>(context) << std::endl;
    }
};

int main(int argc, char* argv[]) {
    nrpc_cpp::init(argc, argv);
    TestApplication app;
    app.start();
    return 0;
}
