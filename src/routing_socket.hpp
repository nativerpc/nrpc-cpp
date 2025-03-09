/**
 * Contents:
 *
 *      RoutingSocket
 *          RoutingSocket
 *          bind
 *          connect
 *          cast
 *          server_thread
 *          client_thread
 *          client_call
 *          forward_call
 *          server_call
 *          _incoming_call
 *          _add_types
 *          _add_server
 *          _get_app_info
 *          _get_schema
 *          _set_schema
 *          _assign_values
 *          _sync_with_server
 *          _sync_with_client
 *          _find_new_fields
 *          _find_new_methods
 *          _find_missing_methods
 *          get_client_id
 *          get_client_ids
 *          get_socket_type
 *          get_types
 *          get_services
 *          get_servers
 *          wait
 *          close
 *
 */
#pragma once
#include "common_base.hpp"
#include <thread>

namespace nrpc_cpp {

class ServerSocket;
class ClientSocket;

class RoutingSocket {
public:
    // See also: RoutingSocketOptions
    RoutingSocket(nlohmann::json options);
    ~RoutingSocket();

    void bind(std::string ip_address, int port);
    void connect(std::string ip_address, int port, bool wait=true, bool sync=true);
    
    template<class T>
    std::shared_ptr<T> cast(std::shared_ptr<RoutingSocket> self) {
        assert(this == self.get());
        return std::make_shared<T>(self);
    }

    void server_thread();
    void client_thread();
    nlohmann::json client_call(int client_id, std::string method_name, nlohmann::json params);
    nlohmann::json forward_call(int client_id, std::string method_name, nlohmann::json params);
    nlohmann::json server_call(std::string method_name, nlohmann::json params);
    
    template<class RQ, class RS>
    RS server_call(std::string method_name, RQ request, std::shared_ptr<RS> response_) {
        nlohmann::json req_data;
        auto& req_type = known_types_[get_class_name<RQ>()];
        auto& res_type = known_types_[get_class_name<RS>()];
        assert(req_type.type_name != "");
        assert(res_type.type_name != "");
        assert(req_type.size == sizeof(RQ));
        assert(res_type.size == sizeof(RS));
        _assign_values(req_type.type_name, reinterpret_cast<uint8_t*>(&request), 0, sizeof(RQ), req_data, 1);
        auto res_data = server_call(method_name, req_data);
        RS response;
        _assign_values(res_type.type_name, reinterpret_cast<uint8_t*>(&response), 0, sizeof(RS), res_data, 0);
        return response;
    }
    
    void _incoming_call(std::string method_name, nlohmann::json req, std::vector<uint8_t>& response_data);
    void _add_types(nlohmann::json types);
    void _add_server(nlohmann::json types);
    void _get_app_info(nlohmann::json& request, std::vector<uint8_t>& response);
    void _get_schema(nlohmann::json& request, std::vector<uint8_t>& response, int active_client_id);
    void _set_schema(nlohmann::json& request, std::vector<uint8_t>& response);

    void _assign_values(std::string type_name, uint8_t* res_data, int res_offset, int res_size, nlohmann::json &data, int target);
    void _sync_with_server();
    void _sync_with_client();
    int _find_new_fields(nlohmann::json schema, bool do_add);
    int _find_new_methods(nlohmann::json schema, bool do_add);
    void _find_missing_methods(nlohmann::json schema);
    
    int get_client_id();
    std::vector<int> get_client_ids();
    SocketType get_socket_type();
    std::map<std::string, ClassInfo> get_types();
    std::map<std::string, ServiceInfo> get_services();
    std::map<std::string, ServerInfo> get_servers();

    void wait();
    void close();

private:
    SocketType socket_type_{SocketType::BIND};
    ProtocolType protocol_type_{ProtocolType::TCP};
    FormatType format_type_{FormatType::JSON};
    std::string socket_name_;
    std::string ip_address_;
    int port_{0};
    bool is_alive_{false};
    std::shared_ptr<ServerSocket> server_socket_;
    std::shared_ptr<ClientSocket> client_socket_;
    std::shared_ptr<std::thread> processor_;
    std::map<std::string, ClassInfo> known_types_;
    std::map<std::string, ServiceInfo> known_services_;
    std::map<std::string, ServerInfo> known_servers_;
    int call_count_{0};
    bool do_sync_{false};
    bool is_ready_{false};
};

}  // namespace nrpc_cpp