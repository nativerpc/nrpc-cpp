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
 *          get_client_ids
 *          get_socket_type
 *          get_types
 *          get_services
 *          get_servers
 *          wait
 *          close
 */
#include "routing_socket.hpp"

#include "client_socket.hpp"
#include "server_socket.hpp"

namespace nrpc_cpp {

RoutingSocket::RoutingSocket(nlohmann::json options_) {
    RoutingSocketOptions_ options;
    options.type = (SocketType)(int)options_["type"];
    options.protocol = (ProtocolType)(int)options_["protocol"];
    options.format = (FormatType)(int)options_["format"];
    options.caller = (std::string)options_["caller"];
    options.types = options_["types"];
    options.port = options_.contains("port") ? (int)options_["port"] : 0;

    socket_type_ = options.type;
    protocol_type_ = options.protocol;
    format_type_ = options.format;
    ip_address_ = "";
    port_ = options.port;
    entry_file_ = options.caller;
    processor_.reset();
    is_ready_ = false;
    is_alive_ = true;
    call_count_ = 0;
    do_sync_ = false;

    // FieldType::Json, nlohmann::json type
    if (g_all_types.find(DYNAMIC_OBJECT) == g_all_types.end()) {
        ClassInfo info;
        info.type_name = DYNAMIC_OBJECT;
        info.size = sizeof(nlohmann::json);
        info.class_id = g_class_id++;
        info.instance_manager = std::make_shared<TypedInstanceManager<nlohmann::json>>(info.class_id);
        info.local = true;
        g_all_types[DYNAMIC_OBJECT] = info;
    }

    known_types_[DYNAMIC_OBJECT] = g_all_types[DYNAMIC_OBJECT];
    assert(known_types_[DYNAMIC_OBJECT].instance_manager);

    _add_types(options.types);
}

RoutingSocket::~RoutingSocket() { close(); }

void RoutingSocket::bind(std::string ip_address, int port) {
    assert(socket_type_ == SocketType::BIND);

    ip_address_ = ip_address;
    port_ = port;
    server_socket_ = std::make_shared<ServerSocket>(ip_address, port, port + 10000, entry_file_);

    assert(server_socket_->get_client_ids().size() == 0);

    server_socket_->bind();
    processor_ = std::make_shared<std::thread>([this]() { server_thread(); });
}

void RoutingSocket::connect(std::string ip_address, int port, bool wait, bool sync) {
    assert(socket_type_ == SocketType::CONNECT);

    ip_address_ = ip_address;
    port_ = port;
    client_socket_ = std::make_shared<ClientSocket>(ip_address, port, port + 10000, entry_file_);

    do_sync_ = sync;
    processor_ = std::make_shared<std::thread>([this]() { client_thread(); });

    if (wait) {
        while (is_alive_ && !is_ready_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// template<class T>
// std::shared_ptr<T> RoutingSocket::cast();

void RoutingSocket::server_thread() {
    assert(socket_type_ == BIND);
    while (is_alive_) {
        int client_id = 0;
        std::vector<std::vector<uint8_t>> req;
        server_socket_->recv_norm(client_id, req);
        if (!is_alive_) {
            break;
        }
        assert(req.size() == 2);
        auto method_name = req[0];
        auto command_parameters = get_json(req[1]);

        std::vector<std::vector<uint8_t>> resp;
        resp.resize(2);
        resp[0] = get_buffer(get_buffer("response:"), method_name);

        // print(f"{Fore.BLUE}server{Fore.RESET} received request")
        // print(f"{Fore.BLUE}server{Fore.RESET} responding")

        if (method_name == RoutingMessage::GetAppInfo) {
            _get_app_info(command_parameters, resp[1]);
        } else if (method_name == RoutingMessage::GetSchema) {
            _get_schema(command_parameters, resp[1], client_id);
        } else if (method_name == RoutingMessage::SetSchema) {
            _set_schema(command_parameters, resp[1]);
        } else {
            _incoming_call(get_string(method_name), command_parameters, resp[1]);
        }

        server_socket_->send_norm(client_id, resp);
    }
}

void RoutingSocket::client_thread() {
    assert(socket_type_ == CONNECT);
    client_socket_->connect();

    if (do_sync_) {
        assert(client_socket_->is_validated());
        _sync_with_server();
        _sync_with_client();
    }
    is_ready_ = true;

    while (is_alive_) {
        std::vector<std::vector<uint8_t>> req;
        client_socket_->recv_rev(req);
        if (!is_alive_) {
            break;
        }

        auto method_name = req[0];
        auto command_parameters = get_json(req[1]);
        std::vector<std::vector<uint8_t>> resp;
        resp.resize(2);
        resp[0] = get_buffer(get_buffer("response:"), method_name);

        // Reverse client is bright red
        // print(f"{Fore.RED}client:{socket.client_id}{Fore.RESET} received request, {method_name}")
        // print(f"{Fore.RED}client:{socket.client_id}{Fore.RESET} responding")

        if (req[0] == RoutingMessage::GetAppInfo) {
            _get_app_info(command_parameters, resp[1]);
            // check_serializable(resp)
        } else if (req[0] == RoutingMessage::GetSchema) {
            _get_schema(command_parameters, resp[1], 0);
            // check_serializable(resp)
        } else if (req[0] == RoutingMessage::SetSchema) {
            assert(false);
        } else {
            _incoming_call(get_string(method_name), command_parameters, resp[1]);
            // check_serializable(resp)
        }

        client_socket_->send_rev(resp);
    }
}

nlohmann::json RoutingSocket::client_call(int client_id, std::string method_name, nlohmann::json params) {
    assert(socket_type_ == BIND);
    assert(find_contains(server_socket_->get_client_ids(), client_id));

    // Server rev is dark red
    // print(f"{Style.DIM}{Fore.RED}server{Fore.RESET}{Style.NORMAL} sending request")

    std::vector<uint8_t> res;
    {
        std::lock_guard<std::recursive_mutex> lock(server_socket_->get_request_lock());
        std::vector<std::vector<uint8_t>> req;
        req.resize(2);
        req[0] = get_buffer(method_name);
        req[1] = get_buffer_json(params);
        server_socket_->send_rev(client_id, req);
        server_socket_->recv_rev(client_id, res);
    }
    return get_json(res);
}

nlohmann::json RoutingSocket::forward_call(int client_id, std::string method_name, nlohmann::json params) {
    return server_call(get_string(ServerMessage::ForwardCall), nlohmann::json({
                                                                   {"client_id", client_id},
                                                                   {"method_name", method_name},
                                                                   {"method_params", params},
                                                               }));
}

nlohmann::json RoutingSocket::server_call(std::string method_name, nlohmann::json params) {
    assert(socket_type_ == CONNECT);

    call_count_ += 1;

    // Server rev is dark red
    // print(f"{Style.DIM}{Fore.RED}server{Fore.RESET}{Style.NORMAL} sending request")

    std::vector<uint8_t> res;
    {
        std::lock_guard<std::recursive_mutex> lock(client_socket_->get_request_lock());
        std::vector<std::vector<uint8_t>> req;
        req.resize(2);
        req[0] = get_buffer(method_name);
        req[1] = get_buffer_json(params);
        client_socket_->send_norm(req);
        auto rc = client_socket_->recv_norm(res);
        assert(rc);
    }
    return get_json(res);
}

// template<class RQ, class RS>
// RS server_call(std::string method_name, RQ request, std::shared_ptr<RS> response_);

void RoutingSocket::_incoming_call(std::string method_name, nlohmann::json request_data,
                                   std::vector<uint8_t>& response_data) {
    std::vector<std::string> parts;
    boost::algorithm::split(parts, method_name, boost::is_any_of("."));

    if (known_services_.find(parts[0]) == known_services_.end()) {
        std::cerr << "Missing service! " << method_name << std::endl;
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }
    if (known_servers_.find(parts[0]) == known_servers_.end()) {
        auto& service_info = known_services_[parts[0]];
        if (service_info.service_errors.empty()) {
            service_info.service_errors += boost::str(boost::format("Missing server! %1%") % method_name);
        }
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }

    auto& service_info = known_services_[parts[0]];
    auto& server = known_servers_[parts[0]];

    if (service_info.methods.find(parts[1]) == service_info.methods.end()) {
        if (service_info.service_errors.empty()) {
            service_info.service_errors += boost::str(boost::format("Missing method! %1%") % method_name);
        }
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }
    if (server.methods.find(parts[1]) == server.methods.end()) {
        if (service_info.service_errors.empty()) {
            service_info.service_errors += boost::str(boost::format("Missing server methods! %1%") % method_name);
        }
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }

    auto& info3 = server.methods[parts[1]];

    if (known_types_.find(info3.request_type) == known_types_.end()) {
        if (info3.method_errors.empty()) {
            info3.method_errors +=
                boost::str(boost::format("Unknown method request type! %1%, %2%") % method_name % info3.request_type);
        }
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }
    if (known_types_.find(info3.response_type) == known_types_.end()) {
        if (info3.method_errors.empty()) {
            info3.method_errors +=
                boost::str(boost::format("Unknown method response type! %1%, %2%") % method_name % info3.response_type);
        }
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }
    if (!info3.method_errors.empty()) {
        response_data = get_buffer_json(nlohmann::json::object());
        return;
    }

    assert(known_types_[info3.request_type].class_id);
    assert(known_types_[info3.response_type].class_id);

    auto req_type = known_types_[info3.request_type];  // TODO!
    auto res_type = known_types_[info3.response_type];
    std::vector<uint8_t> req_data;
    std::vector<uint8_t> res_data;
    req_data.resize(req_type.size);
    res_data.resize(res_type.size);

    assert(req_type.type_name != "");
    assert(res_type.type_name != "");
    assert(req_type.class_id == req_type.instance_manager->_get_class_id());
    assert(res_type.class_id == res_type.instance_manager->_get_class_id());

    nrpc_cpp::construct_item(req_type.type_name, req_data);
    nrpc_cpp::construct_item(res_type.type_name, res_data);

    assert(info3.local);
    assert(req_type.local);
    assert(res_type.local);

    _assign_values(info3.request_type, &req_data[0], 0, req_data.size(), request_data, 0);

    info3.function_manager->invoke_function(server.instance, req_data, res_data);

    nlohmann::json resp;
    _assign_values(info3.response_type, &res_data[0], 0, res_data.size(), resp, 1);

    nrpc_cpp::destroy_item(req_type.type_name, req_data);
    nrpc_cpp::destroy_item(res_type.type_name, res_data);

    response_data = get_buffer_json(resp);
}

void RoutingSocket::_add_types(nlohmann::json types) {
    for (nlohmann::json item : types) {
        if (item.is_array()) {
            auto service_name = (std::string)item[0];
            auto server_name = (std::string)item[1];
            auto service_info = g_all_services[service_name];
            known_services_[service_name] = service_info;
            _add_server(item);
            assert(known_services_[service_name].service_name != "");
        } else {
            auto type_name = (std::string)item;
            auto service_name = (std::string)item;
            if (g_all_types.find(type_name) != g_all_types.end()) {
                auto type_info = g_all_types[type_name];
                known_types_[type_name] = type_info;
                assert(known_types_[type_name].type_name != "");
            } else {
                assert(g_all_services.find(service_name) != g_all_services.end());
                auto service_info = g_all_services[service_name];
                known_services_[service_name] = service_info;
                assert(known_services_[service_name].service_name != "");
            }
        }
    }
}

void RoutingSocket::_add_server(nlohmann::json item) {
    auto service_name = (std::string)item[0];
    auto server_name = (std::string)item[1];
    auto service_info = g_all_services[service_name];
    auto server_info = g_all_servers[service_name];
    assert(server_name == server_info.server_name);
    known_servers_[service_name] = server_info;
    assert(known_servers_[service_name].service_name != "");
    assert(service_info.methods.size() == server_info.methods.size());

    for (auto kvp : service_info.methods) {
        auto& server_method = server_info.methods[kvp.first];
        auto& service_method = kvp.second;
        if (server_method.request_type != service_method.request_type) {
        }
        if (server_method.response_type != service_method.response_type) {
            service_method.method_errors +=
                boost::str(boost::format("Method signature mismatch! %1%, %2%, %3%, %4%, %5%") %
                           server_info.server_name % service_info.service_name % server_method.method_name %
                           server_method.response_type % service_method.response_type);
            continue;
        }
    }
}

void RoutingSocket::_get_app_info(nlohmann::json& request, std::vector<uint8_t>& response) {
    std::string this_socket;
    if (socket_type_ == BIND) {
        this_socket = boost::str(boost::format("%1%") % port_);
    } else if (socket_type_ == CONNECT) {
        this_socket = boost::str(boost::format("%1%:%2%") % port_ % client_socket_->get_client_id());
    }

    nlohmann::json clients = nlohmann::json::array();
    if (socket_type_ == BIND) {
        server_socket_->update();
        for (auto& item : server_socket_->get_client_full()) {
            ApplicationInfo::AppClientInfo client;
            client.client_id = item->client_id;
            client.is_validated = item->is_validated;
            client.is_lost = item->is_lost;
            client.entry_file = (std::string)item->client_metadata["entry_file"];

            clients.push_back(nlohmann::json({
                {"client_id", client.client_id},
                {"is_validated", client.is_validated},
                {"is_lost", client.is_lost},
                {"entry_file", client.entry_file},
            }));
        }
    }

    ApplicationInfo info;
    info.server_id = port_;
    info.client_id = socket_type_ == BIND ? 0 : client_socket_->get_client_id();

    response = get_buffer_json(nlohmann::json({
        {"server_id", info.server_id},
        {"client_id", info.client_id},
        {"clients", clients},
        {"types", known_types_.size()},
        {"services", known_services_.size()},
        {"servers", known_servers_.size()},
        {"metadata", socket_type_ == CONNECT ? client_socket_->get_server_metadata() : server_socket_->get_metadata()},
        {"this_socket", this_socket},
        {"clients", socket_type_ == CONNECT ? 0 : server_socket_->get_client_ids().size()},
        {"entry_file", entry_file_},
        {"ip_address", ip_address_},
        {"port", port_},
        {"format", format_type_},
    }));
}

void RoutingSocket::_get_schema(nlohmann::json& request, std::vector<uint8_t>& response, int active_client_id) {
    nlohmann::json types;
    for (auto kvp : known_types_) {
        if (kvp.first == DYNAMIC_OBJECT) {
            continue;
        }
        assert(kvp.second.type_name != "");
        assert(kvp.first == kvp.second.type_name);

        SchemaInfo::SchemaTypeInfo type;
        type.type_name = kvp.second.type_name;
        type.size = kvp.second.size;
        type.fields = kvp.second.fields.size();
        type.local = kvp.second.local;
        type.type_errors = kvp.second.type_errors;

        types.push_back({
            {"type_name", type.type_name},
            {"size", type.size},
            {"fields", type.fields},
            {"local", type.local},
            {"type_errors", type.type_errors},
        });
    }

    nlohmann::json fields;
    for (auto kvp : known_types_) {
        if (kvp.first == DYNAMIC_OBJECT) {
            continue;
        }
        for (auto kvp2 : kvp.second.fields) {
            SchemaInfo::SchemaFieldInfo field;
            field.type_name = kvp.second.type_name;
            field.field_name = kvp2.second.field_name;
            field.field_type = kvp2.second.field_type_str;
            field.id_value = kvp2.second.id_value;
            field.offset = kvp2.second.offset;
            field.size = kvp2.second.size;
            field.local = kvp2.second.local;
            field.field_errors = kvp2.second.field_errors;

            fields.push_back({
                {"type_name", field.type_name},
                {"field_name", field.field_name},
                {"field_type", field.field_type},
                {"id_value", field.id_value},
                {"offset", field.offset},
                {"size", field.size},
                {"local", field.local},
                {"field_errors", field.field_errors},
            });
        }
    }

    nlohmann::json services;
    for (auto kvp : known_services_) {
        SchemaInfo::SchemaServiceInfo service;
        service.service_name = kvp.second.service_name;
        service.methods = kvp.second.methods.size();
        service.local = kvp.second.local;
        service.has_server = known_servers_.find(kvp.second.service_name) != known_servers_.end();
        service.service_errors = kvp.second.service_errors;

        services.push_back({
            {"service_name", service.service_name},
            {"methods", service.methods},
            {"local", service.local},
            {"has_server", service.has_server},
            {"service_errors", service.service_errors},
        });
    }

    nlohmann::json methods;
    for (auto& kvp : known_services_) {
        for (auto& kvp2 : kvp.second.methods) {
            SchemaInfo::SchemaMethodInfo method;
            method.service_name = kvp.second.service_name;
            method.method_name = kvp2.second.method_name;
            method.request_type = kvp2.second.request_type;
            method.response_type = kvp2.second.response_type;
            method.id_value = kvp2.second.id_value;
            method.local = kvp2.second.local;
            method.method_errors = kvp2.second.method_errors;

            methods.push_back({
                {"service_name", method.service_name},
                {"method_name", method.method_name},
                {"request_type", method.request_type},
                {"response_type", method.response_type},
                {"id_value", method.id_value},
                {"local", method.local},
                {"method_errors", method.method_errors},
            });
        }
    }

    nlohmann::json clients;
    if (socket_type_ == BIND) {
        server_socket_->update();
        for (auto& item : server_socket_->get_client_full()) {
            SchemaInfo::SchemaClientInfo client;
            client.main_port = port_;
            client.client_id = item->client_id;
            client.is_validated = item->is_validated;
            client.is_lost = item->is_lost;
            client.entry_file = (std::string)item->client_metadata["entry_file"];
            client.client_metadata;

            clients.push_back(nlohmann::json({
                {"main_port", client.main_port},
                {"client_id", client.client_id},
                {"is_validated", client.is_validated},
                {"is_lost", client.is_lost},
                {"entry_file", client.entry_file},
                {"client_metadata", item->client_metadata},
            }));
        }
    }

    nlohmann::json servers;
    if (socket_type_ == CONNECT) {
        SchemaInfo::SchemaServerInfo server;
        server.port = port_;
        server.entry_file = (std::string)client_socket_->get_server_metadata()["entry_file"];
        server.server_metadata;

        servers.push_back(nlohmann::json({
            {"port", server.port},
            {"entry_file", server.entry_file},
            {"server_metadata", client_socket_->get_server_metadata()},
        }));
    }

    std::string this_socket;
    if (socket_type_ == BIND) {
        this_socket = boost::str(boost::format("%1%") % port_);
    } else if (socket_type_ == CONNECT) {
        this_socket = boost::str(boost::format("%1%:%2%") % port_ % client_socket_->get_client_id());
    }

    SchemaInfo schema;
    schema.server_id = port_;
    schema.client_id = socket_type_ == SocketType::BIND ? 0 : client_socket_->get_client_id();
    schema.types;
    schema.services;
    schema.fields;
    schema.methods;
    schema.metadata;
    schema.active_client = active_client_id;
    schema.this_socket = this_socket;
    schema.clients;
    schema.servers;
    schema.entry_file = entry_file_;

    response = get_buffer_json(nlohmann::json({
        {"server_id", schema.server_id},
        {"client_id", schema.client_id},
        {"types", types},
        {"services", services},
        {"fields", fields},
        {"methods", methods},
        {"metadata", socket_type_ == BIND ? server_socket_->get_metadata() : client_socket_->get_metadata()},
        {"active_client", schema.active_client},
        {"this_socket", schema.this_socket},
        {"clients", clients},
        {"servers", servers},
        {"entry_file", schema.entry_file},
    }));
}

void RoutingSocket::_set_schema(nlohmann::json& request, std::vector<uint8_t>& response) {
    auto added1 = _find_new_fields(request, true);
    auto added2 = _find_new_methods(request, true);
    _get_schema(request, response, 0);
}

void RoutingSocket::_assign_values(std::string type_name, uint8_t* res_data, int res_offset, int res_size,
                                   nlohmann::json& data, int target) {
    assign_values(type_name, res_data, res_offset, res_size, data, target);
}

void RoutingSocket::_sync_with_server() {
    auto res = server_call(get_string(RoutingMessage::GetSchema), nlohmann::json({}));

    _find_missing_methods(res);
    auto added1 = _find_new_fields(res, true);
    auto added2 = _find_new_methods(res, true);
}

void RoutingSocket::_sync_with_client() {
    nlohmann::json dummy;
    std::vector<uint8_t> req;
    _get_schema(dummy, req, 0);
    auto res = server_call(get_string(RoutingMessage::SetSchema), get_json(req));
    auto added1 = _find_new_fields(res, false);
    auto added2 = _find_new_methods(res, false);
    assert(added1 == 0);
    assert(added2 == 0);
}

int RoutingSocket::_find_new_fields(nlohmann::json schema, bool do_add) {
    nlohmann::json to_add;
    for (auto server_type_info : schema["types"]) {
        auto type_name = (std::string)server_type_info["type_name"];
        assert(type_name != "");

        if (known_types_.find(type_name) == known_types_.end()) {
        } else {
            auto& known_type = known_types_[type_name];
            assert(known_type.type_name != "");
            for (auto field_info : schema["fields"]) {
                if ((std::string)field_info["type_name"] != type_name) {
                    continue;
                }
                auto field_name = (std::string)field_info["field_name"];
                assert(field_name != "");
                assert(field_info.contains("id_value"));
                if (known_type.fields.find(field_name) == known_type.fields.end()) {
                    for (auto kvp : known_type.fields) {
                        if (kvp.second.id_value != (int)field_info["id_value"]) {
                            kvp.second.field_errors +=
                                boost::str(boost::format("\nDuplicate id! %1%.%2%, %3%=%4%") % type_name % field_name %
                                           kvp.first % kvp.second.id_value);
                            continue;
                        }
                    }
                    to_add.push_back(nlohmann::json({{"type_name", type_name},
                                                     {"field_name", field_name},
                                                     {"id_value", (int)field_info["id_value"]}}));
                } else {
                    assert(known_type.fields.find(field_name) != known_type.fields.end());
                    if ((int)field_info["id_value"] != known_type.fields[field_name].id_value) {
                        known_type.fields[field_name].field_errors += boost::str(
                            boost::format("\nField numbering mismatch! %1%.%2%, %3%, %4%") % type_name % field_name %
                            ((int)field_info["id_value"]) % known_type.fields[field_name].id_value);
                        continue;
                    }
                }
            }
        }
    }

    // Add missing fields
    //
    if (do_add) {
        for (auto item : to_add) {
            assert(known_types_.find((std::string)item["type_name"]) != known_types_.end());
            auto& known_type = known_types_[(std::string)item["type_name"]];
            assert(known_type.type_name != "");
            // clang-format off
            FieldInfo info;
            info.field_name = (std::string)item["field_name"];
            info.id_value = (int)item["id_value"];
            info.field_type_str = (std::string)item["type_name"];
            info.field_type = 
                info.field_type_str == "complex" ? FieldType::Complex :
                info.field_type_str == "int"     ? FieldType::Int : 
                info.field_type_str == "float"   ? FieldType::Float : 
                info.field_type_str == "str"     ? FieldType::String : 
                info.field_type_str == "dict"    ? FieldType::Json : 
                FieldType::Unknown;
            // clang-format on
            info.offset = -1;
            info.size = -1;
            info.local = false;

            known_type.fields[(std::string)item["field_name"]] = info;
        }

        auto temp = _find_new_fields(schema, false);
        assert(temp == 0);
    }

    return to_add.size();
}

int RoutingSocket::_find_new_methods(nlohmann::json schema, bool do_add) {
    nlohmann::json to_add;
    for (auto service_info : schema["services"]) {
        auto service_name = (std::string)service_info["service_name"];
        if (known_services_.find(service_name) == known_services_.end()) {
        } else {
            auto& my_service_info = known_services_[service_name];
            for (auto method_info : schema["methods"]) {
                if ((std::string)method_info["service_name"] != service_name) {
                    continue;
                }
                auto method_name = (std::string)method_info["method_name"];
                assert((int)method_info["id_value"] > 0);
                if (my_service_info.methods.find(method_name) == my_service_info.methods.end()) {
                    for (auto& kvp : my_service_info.methods) {
                        if (kvp.second.id_value == (int)method_info["id_value"]) {
                            kvp.second.method_errors +=
                                boost::str(boost::format("\nDuplicate id! %1%, %2%, %3%, %4%") % service_name %
                                           method_name % kvp.second.id_value % ((int)method_info["id_value"]));
                            continue;
                        }

                        // key2 = kvp.second.method_name
                        // f"Duplicate id! {service_name}.{method_name}, {key2}={item2["id_value"]}"
                    }
                    to_add.push_back(nlohmann::json({
                        {"service_name", service_name},
                        {"method_name", method_name},
                        {"id_value", (int)method_info["id_value"]},
                        {"handler", (std::string)method_info["handler"]},
                        {"request_type", (std::string)method_info["request_type"]},
                        {"response_type", (std::string)method_info["response_type"]},
                    }));
                } else {
                    auto& my_method = my_service_info.methods[method_name];
                    if ((int)method_info["id_value"] != my_method.id_value) {
                        my_method.method_errors +=
                            boost::str(boost::format("\nMethod numbering mismatch! %1%, %2%, %3%, %4%") % service_name %
                                       method_name % ((int)method_info["id_value"]) % my_method.id_value);
                        continue;
                    }
                }
            }
        }
    }

    // Add missing methods
    //
    if (do_add) {
        for (auto item : to_add) {
            MethodInfo info;
            info.method_name = (std::string)item["method_name"];
            info.request_type = (std::string)item["request_type"];
            info.response_type = (std::string)item["response_type"];
            info.id_value = (int)item["id_value"];
            info.handler = (std::string)item["handler"];
            info.local = false;

            known_services_[(std::string)item["service_name"]].methods[(std::string)item["method_name"]] = info;
        }

        auto temp = _find_new_methods(schema, false);
        assert(temp == 0);
    }
    return to_add.size();
}

void RoutingSocket::_find_missing_methods(nlohmann::json schema) {
    for (auto kvp : known_services_) {
        auto service_name = kvp.second.service_name;
        auto has_remote_service = nrpc_cpp::find_contains_json(
            schema["services"],
            [service_name](const nlohmann::json& x) { return (std::string)x["service_name"] == service_name; });
        auto remote_service_methods = nrpc_cpp::find_all_json(
            schema["methods"], [service_name](auto x) { return (std::string)x["service_name"] == service_name; });
        if (!has_remote_service) {
            kvp.second.service_errors += boost::str(boost::format("Missing remote service! %1%") % service_name);
            continue;
        }
        // f"Missing remote service! {service_name}"
        for (auto& kvp : kvp.second.methods) {
            auto method_name = kvp.second.method_name;
            assert(kvp.second.id_value > 0);
            if (!find_contains_json(remote_service_methods,
                                    [method_name](auto x) { return (std::string)x["method_name"] == method_name; })) {
                kvp.second.method_errors +=
                    boost::str(boost::format("Missing remote method! %1%, %2%") % service_name % method_name);
                continue;
            }
        }
    }
}

std::vector<int> RoutingSocket::get_client_ids() {
    assert(socket_type_ == BIND);
    return server_socket_->get_client_ids();
}

SocketType RoutingSocket::get_socket_type() { return socket_type_; }

std::map<std::string, ClassInfo> RoutingSocket::get_types() { return known_types_; }

std::map<std::string, ServiceInfo> RoutingSocket::get_services() { return known_services_; }

std::map<std::string, ServerInfo> RoutingSocket::get_servers() { return known_servers_; }

void RoutingSocket::wait() {
    if (socket_type_ == SocketType::BIND) {
        server_socket_->wait();
    } else {
        client_socket_->wait();
    }
}

void RoutingSocket::close() {
    is_alive_ = false;
    if (socket_type_ == SocketType::BIND) {
        server_socket_->set_closing();
    } else {
        client_socket_->set_closing();
    }
    processor_->join();
    if (socket_type_ == SocketType::BIND) {
        server_socket_->close();
    } else {
        client_socket_->close();
    }
    client_socket_.reset();
    processor_.reset();
}

}  // namespace nrpc_cpp
