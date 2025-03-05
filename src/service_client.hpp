/**
 * Contents:
 *      ServiceClientBase
 *          ServiceClientBase
 */
#pragma once
#include "routing_socket.hpp"

namespace nrpc_cpp {
class ServiceClientBase {
public:
    ServiceClientBase(std::shared_ptr<nrpc_cpp::RoutingSocket> socket);

protected:
    std::shared_ptr<nrpc_cpp::RoutingSocket> socket_;
};
}  // namespace nrpc_cpp