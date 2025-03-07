
/**
 * Contents:
 *
 *      SocketType
 *      ProcotolType
 *      FormatType
 *      RoutingSocketOptions
 *      ServerMessage
 *      RoutingMessage
 *      WebSocketInfo
 *      SocketMetadataInfo
 *      ClientInfo
 *      ApplicationInfo
 *      SchemaInfo
 *      FieldType
 *      TypeNames
 *      DYNAMIC_OBJECT
 *      FieldInfo
 *      MethodInfo
 *      ClassInfo
 *      ServiceInfo
 *      ServerInfo
 *
 *      g_all_types
 *      g_all_services
 *      g_all_servers
 *      g_class_id
 *      TypedClassManager
 *          register_members()
 *      get_class_name
 *      get_type_value
 *      ServerBase
 *      CommonInstanceManager
 *      TypedInstanceManager
 *      CommonFunctionManager
 *      TypedFunctionManager
 *      $rpcclass
 *          struct TypedClassManager<TP>
*               register_members()
 *      $field
 *      $method
 *      register_member_field
 *      register_member_service_method
 *      register_member_server_method
 *      type
 *      service
 *      service(server)
 *      construct_item
 *      destroy_item
 *      construct_json
 *      assign_values
 *      get_class_string
 *      get_simple_type
 *
 *      g_argv
 *      init
 *      CommandLine
 *      get_iso_time
 *      g_base64_alphabet
 *      g_base64_alphabet_rev
 *      base64_encode
 *      base64_decode
 *      get_string
 *      get_buffer
 *      get_buffer_json
 *      set_buffer
 *      get_json
 *      find
 *      find_contains
 *      same_sets
 */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#undef ZMQ_BUILD_DRAFT_API
#define ZMQ_BUILD_DRAFT_API

// clang-format off
namespace nrpc_cpp {
    class ServerBase;
    class CommonInstanceManager;
    class CommonFunctionManager;
}  // namespace nrpc_cpp
// clang-format on

namespace nrpc_cpp {

enum SocketType {
    BIND = 1,
    CONNECT = 2,
};

enum ProtocolType {
    TCP = 1,
    WS = 2,
    HTTP = 3,
};

enum FormatType {
    BINARY = 1,
    JSON = 2,
};

typedef nlohmann::json RoutingSocketOptions;

class RoutingSocketOptions_ {
public:
    SocketType type{SocketType::BIND};
    ProtocolType protocol{ProtocolType::TCP};
    FormatType format{FormatType::JSON};
    std::string caller;
    nlohmann::json types;
    int port{0};
};

class ServerMessage {
public:
    const static std::vector<uint8_t> AddClient;
    const static std::vector<uint8_t> ValidateClient;
    const static std::vector<uint8_t> ClientAdded;
    const static std::vector<uint8_t> ClientValidated;
    const static std::vector<uint8_t> ForwardCall;
};

class RoutingMessage {
public:
    const static std::vector<uint8_t> GetAppInfo;
    const static std::vector<uint8_t> GetSchema;
    const static std::vector<uint8_t> SetSchema;
};

class WebSocketInfo {
public:
};

class SocketMetadataInfo {
public:
    int server_id{0};
    int client_id{0};
    std::string lang;
    std::string ip_address;
    int main_port{0};
    int main_port_rev{0};
    std::string host;
    std::string entry_file;
    std::string start_time;
    std::string client_signature;
    std::string client_signature_rev;
    std::string server_signature;
    std::string server_signature_rev;
};

struct ClientInfo {
    int client_id{0};
    std::vector<uint8_t> client_signature;
    std::vector<uint8_t> client_signature_rev;
    nlohmann::json client_metadata;
    std::chrono::time_point<std::chrono::steady_clock> connect_time;
    bool is_validated{false};
    bool is_lost{false};
};

class ApplicationInfo {
public:
    class AppClientInfo {
    public:
        int client_id{0};
        bool is_validated{false};
        bool is_lost{false};
        std::string entry_file;
    };

    int server_id{0};
    int client_id{0};
    bool is_alive{false};
    bool is_ready{false};
    std::string socket_type;
    std::string protocol_type;
    int methods{0};
    int types{0};
    int services{0};
    int servers{0};
    SocketMetadataInfo metadata;
    std::string this_socket;
    int client_count{0};
    std::vector<AppClientInfo> clients;
    std::vector<int> client_ids;
    std::string entry_file;
    std::string ip_address;
    int port{0};
    std::string format;
};

class SchemaInfo {
public:
    class SchemaTypeInfo {
    public:
        std::string type_name;
        int size{0};
        int fields{0};
        bool local{false};
        std::string type_errors;
    };

    class SchemaServiceInfo {
    public:
        std::string service_name;
        int methods{0};
        bool local{false};
        bool has_server{false};
        std::string service_errors;
    };

    class SchemaFieldInfo {
    public:
        std::string type_name;
        std::string field_name;
        std::string field_type;
        int id_value{0};
        int offset{0};
        int size{0};
        bool local{false};
        std::string field_errors;
    };

    class SchemaMethodInfo {
    public:
        std::string service_name;
        std::string method_name;
        std::string request_type;
        std::string response_type;
        int id_value{0};
        bool local{false};
        std::string method_errors;
    };

    class SchemaClientInfo {
    public:
        int main_port{0};
        int client_id{0};
        bool is_validated{false};
        bool is_lost{false};
        std::string entry_file;
        SocketMetadataInfo client_metadata;
    };

    class SchemaServerInfo {
    public:
        int port{0};
        std::string entry_file;
        SocketMetadataInfo server_metadata;
    };

    int server_id{0};
    int client_id{0};
    std::vector<SchemaTypeInfo> types;
    std::vector<SchemaServiceInfo> services;
    std::vector<SchemaFieldInfo> fields;
    std::vector<SchemaMethodInfo> methods;
    SocketMetadataInfo metadata;
    int active_client{0};
    std::string this_socket;
    std::vector<SchemaClientInfo> clients;
    std::vector<SchemaServerInfo> servers;
    std::string entry_file;
};

enum FieldType {
    Unknown,
    Complex,
    Int,
    Float,
    String,
    // FieldType::Json == nlohmann::json == DYNAMIC_OBJECT
    Json,
};

// clang-format off
const std::string TypeNames[] = {
    "unknown",
    "complex",
    "int",
    "float",
    "str",
    "dict"
};

// FieldType::Json, nlohmann::json type
const std::string DYNAMIC_OBJECT = "dict";
// clang-format on

struct FieldInfo {
    std::string field_name;
    FieldType field_type{FieldType::Unknown};
    std::string field_type_str;
    int id_value{0};
    int offset{0};
    int size{0};
    bool local{false};
    std::string field_errors;
};

struct MethodInfo {
    std::string method_name;
    std::string request_type;
    std::string response_type;
    std::string handler;
    int id_value{0};
    bool local{false};
    std::shared_ptr<CommonFunctionManager> function_manager;
    std::string method_errors;
};

struct ClassInfo {
    std::string type_name;
    std::map<std::string, FieldInfo> fields;
    int size{0};
    int class_id{0};
    bool local{false};
    std::string type_errors;
    std::shared_ptr<CommonInstanceManager> instance_manager;
};

struct ServiceInfo {
    std::string service_name;
    std::map<std::string, MethodInfo> methods;
    bool local{false};
    std::string service_errors;
};

struct ServerInfo {
    std::string server_name;
    std::string service_name;
    ServerBase *instance{0};
    std::map<std::string, MethodInfo> methods;
    std::string server_errors;
};

extern std::map<std::string, ClassInfo> g_all_types;
extern std::map<std::string, ServiceInfo> g_all_services;
extern std::map<std::string, ServerInfo> g_all_servers;
extern int g_class_id;

template <class TP>
struct TypedClassManager {
    void register_members(int type, ServerBase* server_instance, std::string server_name) {
        assert(false);
    }
};

template <class TP>
std::string get_class_name() {
    return "unknown";
}

template<>
inline std::string get_class_name<nlohmann::json>() {
    return DYNAMIC_OBJECT;
}

template <class TP>
FieldType get_type_value() {
    assert(false);
    return FieldType::Unknown;
}
template <>
inline FieldType get_type_value<int>() {
    return FieldType::Int;
}
template <>
inline FieldType get_type_value<float>() {
    return FieldType::Float;
}
template <>
inline FieldType get_type_value<std::string>() {
    return FieldType::String;
}
template <>
inline FieldType get_type_value<nlohmann::json>() {
    return FieldType::Json;
}

class ServerBase {};

class CommonInstanceManager {
public:
    virtual void _construct_item(std::vector<uint8_t> &obj) = 0;
    virtual void _destroy_item(std::vector<uint8_t> &obj) = 0;
    virtual int _get_class_id() = 0;
};

template <class TP>
struct TypedInstanceManager : public CommonInstanceManager {
public:
    TypedInstanceManager(int class_id) { class_id_ = class_id; }

    void _construct_item(std::vector<uint8_t> &obj) {
        assert(obj.size() == sizeof(TP));
        new (reinterpret_cast<TP *>(&obj.at(0))) TP();
    }

    void _destroy_item(std::vector<uint8_t> &obj) {
        assert(obj.size() == sizeof(TP));
        reinterpret_cast<TP *>(&obj.at(0))->~TP();
        obj.resize(0);
    }

    int _get_class_id() { return class_id_; }

private:
    int class_id_{0};
};

class CommonFunctionManager {
public:
    virtual void invoke_function(ServerBase *obj, std::vector<uint8_t> &req, std::vector<uint8_t> &res) = 0;
};

template <class REQ, class RES>
class TypedFunctionManager : public CommonFunctionManager {
public:
    TypedFunctionManager(RES (ServerBase::*method_pointer)(REQ)) {
        _ptr = method_pointer;
    }

    void invoke_function(ServerBase *obj, std::vector<uint8_t> &req, std::vector<uint8_t> &res) override {
        *reinterpret_cast<RES *>(&res.at(0)) = (obj->*_ptr)(*reinterpret_cast<REQ *>(&req.at(0)));
    }

private:
    RES (ServerBase::*_ptr)(REQ);
};

// nrpc::type<TP>() invokes "TypedClassManager::register_members" once.
// nrpc::service<TP>() invokes "TypedClassManager::register_members" once.
// TypedClassManager::register_members() invokes methods like:
//      register_member_field()
//      register_member_service_method()
//      register_member_server_method()
//
#define $rpcclass(TP, ...)                                                                                  \
    class TP;                                                                                               \
    namespace nrpc_cpp {                                                                                    \
        template <>                                                                                         \
        inline std::string get_class_name<TP>() {                                                           \
            return #TP;                                                                                     \
        }                                                                                                   \
        template <>                                                                                         \
        struct TypedClassManager<TP> {                                                                               \
            template <class SR>                                                                             \
            void register_members(int type, SR* server_instance, std::string server_name) { \
                static bool ready = false;                                                                  \
                if (ready) {                                                                                \
                    return;                                                                                 \
                }                                                                                           \
                ready = true;                                                                               \
                int m_type = type;                                                                          \
                std::string m_class_name = #TP;                                                             \
                std::string m_server_name = server_name;                                                    \
                SR* m_server_instance = server_instance;                                    \
                TP *field_ref = nullptr;                                                                    \
                __VA_ARGS__;                                                                                \
            }                                                                                               \
        };                                                                                                  \
        template <>                                                                                         \
        inline FieldType get_type_value<TP>() {                                                             \
            return FieldType::Complex;                                                                      \
        }                                                                                                   \
    } //namespace nrpc_cpp

#define $field(Y, ID) register_member_field(m_type, m_class_name, #Y, field_ref, &field_ref->Y, ID)

// clang-format off
#define $method(Y, ID)                                                                                                      \
    register_member_service_method(m_type, m_class_name, #Y, ID, &SR::Y),                                                   \
    (m_type == 3 ? register_member_server_method(m_type, m_class_name, m_server_name, m_server_instance, #Y, ID, &SR::Y) : 0)
// clang-format on

template <class TP, class F>
int register_member_field(int type, std::string class_name, std::string field_name, TP *field_ref, F *field_ref_member,
                          int id) {
    assert(type == 1);
    if (g_all_types.find(class_name) == g_all_types.end()) {
        ClassInfo info1;
        info1.type_name = class_name;
        info1.size = sizeof(TP);
        info1.class_id = g_class_id++;
        info1.instance_manager = std::make_shared<TypedInstanceManager<TP>>(info1.class_id);
        info1.local = true;
        g_all_types[class_name] = info1;
        assert(field_ref == 0);
        assert(!info1.type_name.empty());
    }

    if (g_all_types[class_name].fields.find(field_name) == g_all_types[class_name].fields.end()) {
        auto info = FieldInfo();
        info.field_name = field_name;
        info.field_type = get_type_value<F>();
        if (info.field_type == FieldType::Complex) {
            info.field_type_str = get_class_name<F>();
        } else {
            info.field_type_str = TypeNames[(int)info.field_type];
        }
        info.id_value = id;
        info.offset = reinterpret_cast<uint8_t *>(field_ref_member) - reinterpret_cast<uint8_t *>(field_ref);
        info.size = sizeof(F);
        info.local = true;
        g_all_types[class_name].fields[field_name] = info;
    }
    return 0;
}

template <class SR, class RES, class REQ>
int register_member_service_method(int type, std::string class_name, std::string method_name, int id,
                                   RES (SR::*dummy)(REQ)) {
    assert(type == 2 || type == 3);
    if (g_all_services.find(class_name) == g_all_services.end()) {
        ServiceInfo info1;
        info1.service_name = class_name;
        info1.local = true;
        g_all_services[class_name] = info1;
        assert(g_all_services.find(class_name) != g_all_services.end());
    }

    if (g_all_services[class_name].methods.find(method_name) == g_all_services[class_name].methods.end()) {
        auto info = MethodInfo();
        info.method_name = method_name;
        info.request_type = get_class_name<REQ>();
        info.response_type = get_class_name<RES>();
        info.id_value = id;
        info.handler = method_name;
        info.local = true;
        g_all_services[class_name].methods[method_name] = info;
    }
    return 0;
}

template <class SR, class RES, class REQ>
int register_member_server_method(int type, std::string class_name, std::string server_name,
                                  SR* server_instance, std::string method_name, int id,
                                  RES (SR::*method_pointer)(REQ)) {
    assert(type == 3);
    if (g_all_servers.find(class_name) == g_all_servers.end()) {
        ServerInfo info1;
        info1.server_name = server_name;
        info1.service_name = class_name;
        info1.instance = reinterpret_cast<ServerBase *>(server_instance);
        g_all_servers[class_name] = info1;
    }

    if (g_all_servers[class_name].methods.find(method_name) == g_all_servers[class_name].methods.end()) {
        auto info = MethodInfo();
        info.method_name = method_name;
        info.request_type = get_class_name<REQ>();
        info.response_type = get_class_name<RES>();
        info.id_value = id;
        info.handler = method_name;
        info.local = true;
        typedef RES (ServerBase::*FN)(REQ);
        auto ptr2 = reinterpret_cast<FN>(method_pointer);
        info.function_manager = std::make_shared<TypedFunctionManager<REQ, RES>>(ptr2);
        g_all_servers[class_name].methods[method_name] = info;
    }

    assert(g_all_servers[class_name].instance == reinterpret_cast<ServerBase *>(server_instance));
    assert(server_name != "");
    return 0;
}

template <class TP>
std::string type() {
    TP* dummy = 0;
    // Invoke field registration calls:
    //
    //      register_member_field(1, "HelloRequest", "name", field_ref, &field_ref->name, id_value)
    //      register_member_field(1, "HelloRequest", "value", field_ref, &field_ref->value, id_value)
    //      etc
    //
    TypedClassManager<TP>().register_members(1, dummy, "");
    return get_class_name<TP>();
}

template <class TP>
std::string service() {
    TP* dummy = 0;
    // Invoke method registration calls:
    //
    //      register_member_service_method(2, "HelloRequest", "Hello", id_value, ...)
    //      register_member_service_method(2, "HelloRequest", "Hello2", id_value, ...)
    //      etc
    //
    TypedClassManager<TP>().register_members<TP>(2, dummy, "");
    return get_class_name<TP>();
}

template <class TP, class SR>
nlohmann::json service(std::string server_name, SR* server_instance) {
    // Invoke server method registration calls:
    //
    //      register_member_server_method(3, "HelloRequest", "Hello", id_value, ...)
    //      register_member_server_method(3, "HelloRequest", "Hello2", id_value, ...)
    //      etc
    //
    TypedClassManager<TP>().register_members<SR>(3, server_instance, server_name);
    nlohmann::json result = nlohmann::json::array();
    result.push_back(get_class_name<TP>());
    result.push_back(server_name);
    return result;
}

void construct_item(std::string type_name, std::vector<uint8_t> &data);

template <class TP>
TP construct_item(nlohmann::json json_data) {
    auto &type_info = g_all_types[type<TP>()];
    assert(type_info.size == sizeof(TP));
    TP obj_data;
    assign_values(type_info.type_name, reinterpret_cast<uint8_t *>(&obj_data), 0, sizeof(TP), json_data, 0);
    return obj_data;
}

void destroy_item(std::string type_name, std::vector<uint8_t> &data);

template <class TP>
nlohmann::json construct_json(TP &data) {
    auto &type_info = g_all_types[type<TP>()];
    assert(type_info.size == sizeof(TP));
    nlohmann::json json_data;
    assign_values(type_info.type_name, reinterpret_cast<uint8_t *>(&data), 0, sizeof(TP), json_data, 1);
    return json_data;
}

void assign_values(std::string type_name, uint8_t *res_data, int res_offset, int res_size, nlohmann::json &data,
                   int target);

template <class TP>
std::string get_class_string(TP& data) {
    return construct_json(data).dump();
}

template <class TP>
std::string get_simple_type() {
    return get_class_name<TP>();
}

extern std::vector<std::string> g_argv;
 
void init(int argc, char *argv[]);

class CommandLine {
public:
    CommandLine();    
    CommandLine(nlohmann::json params);
    ~CommandLine();
    nlohmann::json operator[](std::string name);
    bool contains(std::string name);

private:
    nlohmann::json values_;
};

std::string get_iso_time(const std::chrono::system_clock::time_point &time_point);

extern const std::string g_base64_alphabet;
extern const std::vector<int8_t> g_base64_alphabet_rev;

std::string base64_encode(const std::vector<uint8_t> &data);
std::vector<uint8_t> base64_decode(std::string encoded);

std::string get_string(std::vector<uint8_t> data);
std::vector<uint8_t> get_buffer(std::string str);
std::vector<uint8_t> get_buffer_json(const std::vector<uint8_t> &data);
std::vector<uint8_t> get_buffer_json(const nlohmann::json &data);
std::vector<uint8_t> get_buffer(std::vector<uint8_t> a, std::vector<uint8_t> b);
void set_buffer(std::vector<uint8_t> &dest, void *data, size_t size);
nlohmann::json get_json(std::vector<uint8_t> data);

template <typename T, typename P>
std::shared_ptr<T> find(std::vector<std::shared_ptr<T>> &list, P pred) {
    for (auto &item : list) {
        if (pred(item)) {
            return item;
        }
    }
    return std::shared_ptr<T>();
}

template <typename P>
nlohmann::json find_all_json(const nlohmann::json &list, P pred) {
    nlohmann::json result;
    for (auto &item : list) {
        if (pred(item)) {
            result.push_back(item);
        }
    }
    return result;
}

template <typename P>
bool find_contains_json(const nlohmann::json &list, P pred) {
    for (auto &item : list) {
        if (pred(item)) {
            return true;
        }
    }
    return false;
}

template <typename T, typename P>
bool find_contains(const std::vector<T> &list, P pred) {
    for (auto &item : list) {
        if (pred(item)) {
            return true;
        }
    }
    return false;
}

template <typename T>
bool find_contains(const std::vector<T> &list, T value) {
    for (const auto &item : list) {
        if (item == value) {
            return true;
        }
    }
    return false;
}

bool same_sets(std::vector<int>& a, std::vector<int>& b);
}  // namespace nrpc_cpp