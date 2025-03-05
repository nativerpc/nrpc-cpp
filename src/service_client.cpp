/**
 * Contents:
 *      ServiceClientBase
 *          ServiceClientBase
 */
#include "service_client.hpp"

namespace nrpc_cpp {
ServiceClientBase::ServiceClientBase(std::shared_ptr<nrpc_cpp::RoutingSocket> socket) {
    socket_ = socket; // nowrap
    assert(socket_->get_socket_type() == nrpc_cpp::CONNECT);
}
}  // namespace nrpc_cpp