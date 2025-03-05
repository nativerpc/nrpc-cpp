/**
 * Contents:
 *
 *      ServerMessage
 *      RoutingMessage
 * 
 *      g_all_types
 *      g_all_services
 *      g_all_servers
 *      g_class_id
 *      construct_item
 *      destroy_item
 *      construct_json
 *      assign_values
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
 */
#include "common_base.hpp"
#include <sstream>
// #include <boost/beast/core/detail/base64.hpp>

namespace nrpc_cpp {

const std::vector<uint8_t> ServerMessage::AddClient = nrpc_cpp::get_buffer("ServerMessage.AddClient");
const std::vector<uint8_t> ServerMessage::ClientAdded = nrpc_cpp::get_buffer("ServerMessage.ClientAdded");
const std::vector<uint8_t> ServerMessage::ValidateClient = nrpc_cpp::get_buffer("ServerMessage.ValidateClient");
const std::vector<uint8_t> ServerMessage::ClientValidated = nrpc_cpp::get_buffer("ServerMessage.ClientValidated");
const std::vector<uint8_t> ServerMessage::ForwardCall = nrpc_cpp::get_buffer("ServerMessage.ForwardCall");

const std::vector<uint8_t> RoutingMessage::GetAppInfo = nrpc_cpp::get_buffer("RoutingMessage.GetAppInfo");
const std::vector<uint8_t> RoutingMessage::GetSchema = nrpc_cpp::get_buffer("RoutingMessage.GetSchema");
const std::vector<uint8_t> RoutingMessage::SetSchema = nrpc_cpp::get_buffer("RoutingMessage.SetSchema");

std::map<std::string, ClassInfo> g_all_types;
std::map<std::string, ServiceInfo> g_all_services;
std::map<std::string, ServerInfo> g_all_servers;
int g_class_id = 1000;

void construct_item(std::string type_name, std::vector<uint8_t>& data) {
    g_all_types[type_name].instance_manager->_construct_item(data);
}

void destroy_item(std::string type_name, std::vector<uint8_t>& data) {
    g_all_types[type_name].instance_manager->_destroy_item(data);
}

void assign_values(std::string type_name, uint8_t* res_data, int res_offset, int res_size, nlohmann::json& json_data, int target) {
    if (type_name == DYNAMIC_OBJECT) {
        auto obj_data = reinterpret_cast<nlohmann::json*>(res_data);
        if (target == 0) {
            *obj_data = json_data;
        } else {
            json_data = *obj_data;
        }
        return;
    }
    
    auto& type_info = g_all_types[type_name];
    for (auto& kvp : type_info.fields) {
        auto& item = kvp.second;
        // std::cout << "ASSIGN " << item.name << std::endl;
        if (target == 0 && !json_data.contains(item.field_name)) {
            // keep default
        } else if (!item.local) {
            // skip
        } else if (item.field_type == FieldType::Complex) {
            auto& child_type = g_all_types[item.field_type_str];
            assert(child_type.type_name == item.field_type_str);
            auto res_child_offset = res_offset + item.offset;
            assert(res_child_offset + child_type.size <= res_size);
            if (target == 0) {
                assign_values(item.field_type_str, res_data, res_child_offset, res_size, json_data.at(item.field_name), 0);
            } else {
                json_data[item.field_name] = nlohmann::json();
                assign_values(item.field_type_str, res_data, res_child_offset, res_size, json_data.at(item.field_name), 1);
            }
        } else if (item.field_type == FieldType::Int) {
            assert(res_offset + item.offset + 4 <= res_size);
            if (target == 0) {
                json_data.at(item.field_name).get_to(*reinterpret_cast<int*>(&res_data[res_offset + item.offset]));
            } else {
                json_data[item.field_name] = *reinterpret_cast<int*>(&res_data[res_offset + item.offset]);
            }
        } else if (item.field_type == FieldType::Float) {
            assert(res_offset + item.offset + 4 <= res_size);
            if (target == 0) {
                json_data.at(item.field_name).get_to(*reinterpret_cast<float*>(&res_data[res_offset + item.offset]));
            } else {
                json_data[item.field_name] = *reinterpret_cast<float*>(&res_data[res_offset + item.offset]);
            }
        } else if (item.field_type == FieldType::String) {
            assert(res_offset + item.offset + sizeof(std::string) <= res_size);
            auto str = reinterpret_cast<std::string*>(&res_data[res_offset + item.offset]);
            if (target == 0) {
                json_data.at(item.field_name).get_to(*str);
            } else {
                json_data[item.field_name] = *str;
            }
        } else if (item.field_type == FieldType::Json) {
            assert(res_offset + item.offset + sizeof(nlohmann::json) <= res_size);
            auto item2 = reinterpret_cast<nlohmann::json*>(&res_data[res_offset + item.offset]);
            if (target == 0) {
                json_data.at(item.field_name).get_to(*item2);
            } else {
                json_data[item.field_name] = *item2;
            }
        } else {
            assert(false);
        }
    }
}

std::vector<std::string> g_argv;

void init(int argc, char* argv[]) {
    g_argv.clear();
    for (int j = 0; j < argc; j++) {
        g_argv.push_back(argv[j]);
    }
}

CommandLine::CommandLine() {
}

CommandLine::CommandLine(nlohmann::json params) {
    if (params.is_array()) {
        values_[(std::string)params[0]] = params[1];
    }else {
        values_ = params;
    }

    for (int j = 1; j < g_argv.size(); j++) {
        if (!g_argv[j].size()) {
            continue;
        }
        if (g_argv[j][0] == '-') {
            continue;
        }
        if (g_argv[j].find("=") == std::string::npos) {
            continue;
        }
        std::vector<std::string> parts;
        boost::algorithm::split(parts, g_argv[j], boost::is_any_of("="));
        assert(parts.size() == 2);
        
        if (values_.contains(parts[0])) {
            if (values_[parts[0]].is_number_float()) {
                values_[parts[0]] = atof(parts[1].c_str());
            }
            else if (values_[parts[0]].is_number_integer()) {
                values_[parts[0]] = atoi(parts[1].c_str());
            }
            else if (values_[parts[0]].is_boolean()) {
                values_[parts[0]] = parts[1] == "1" || parts[1] == "true" || parts[1] == "True";
            }
            else {
                values_[parts[0]] = parts[1];
            }
        }
        else {
            std::cout<<"222 " << parts[0]<<", "<<g_argv[j]<<std::endl;
            values_[parts[0]] = parts[1];
        }
    }
}

CommandLine::~CommandLine() {

}
    
nlohmann::json CommandLine::operator[](std::string name) {
    return values_[name];
}

bool CommandLine::contains(std::string name) {
    return values_.contains(name);
}

std::string get_iso_time(const std::chrono::system_clock::time_point& time_point) {
    auto time_c = std::chrono::system_clock::to_time_t(time_point);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_c), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

const std::string g_base64_alphabet = {
    "ABCDEFGHIJKLMNOP"
    "QRSTUVWXYZabcdef"
    "ghijklmnopqrstuv"
    "wxyz0123456789+/"};

const std::vector<int8_t> g_base64_alphabet_rev = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //   0-15
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  //  16-31
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,  //  32-47
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,  //  48-63
    -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,  //  64-79
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,  //  80-95
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,  //  96-111
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,  // 112-127
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 128-143
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 144-159
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 160-175
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 176-191
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 192-207
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 208-223
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 224-239
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1   // 240-255
};

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string encoded;
    encoded.reserve(data.size() * 4 / 3 + 4);

    auto next = &data[0];
    auto alphabet = &g_base64_alphabet[0];
    auto len = data.size();

    for (auto n = len / 3; n--;) {
        encoded.push_back(alphabet[(next[0] & 0xfc) >> 2]);
        encoded.push_back(alphabet[((next[0] & 0x03) << 4) + ((next[1] & 0xf0) >> 4)]);
        encoded.push_back(alphabet[((next[2] & 0xc0) >> 6) + ((next[1] & 0x0f) << 2)]);
        encoded.push_back(alphabet[next[2] & 0x3f]);
        next += 3;
    }

    if ((len % 3) == 2) {
        encoded.push_back(alphabet[(next[0] & 0xfc) >> 2]);
        encoded.push_back(alphabet[((next[0] & 0x03) << 4) + ((next[1] & 0xf0) >> 4)]);
        encoded.push_back(alphabet[(next[1] & 0x0f) << 2]);
        encoded.push_back('=');
    } else if ((len % 3) == 1) {
        encoded.push_back(alphabet[(next[0] & 0xfc) >> 2]);
        encoded.push_back(alphabet[((next[0] & 0x03) << 4)]);
        encoded.push_back('=');
        encoded.push_back('=');
    }

    return encoded;
}

std::vector<uint8_t> base64_decode(std::string encoded) {
    std::vector<uint8_t> data;
    data.reserve(encoded.size() * 3 / 4 + 3);
    auto next = encoded.c_str();
    unsigned char c3[3];
    unsigned char c4[4];
    auto alphabet_rev = &g_base64_alphabet_rev[0];
    auto len = encoded.length();
    auto pending = 0;

    while (len-- && *next != '=') {
        auto value = alphabet_rev[*next];
        if (value == -1) {
            break;
        }
        ++next;
        c4[pending] = value;
        if (++pending == 4) {
            c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
            c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
            c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

            for (pending = 0; pending < 3; pending++) {
                data.push_back(c3[pending]);
            }
            pending = 0;
        }
    }

    if (pending) {
        c3[0] = (c4[0] << 2) + ((c4[1] & 0x30) >> 4);
        c3[1] = ((c4[1] & 0xf) << 4) + ((c4[2] & 0x3c) >> 2);
        c3[2] = ((c4[2] & 0x3) << 6) + c4[3];

        for (auto j = 0; j < pending - 1; j++) data.push_back(c3[j]);
    }

    return data;
}

std::string get_string(std::vector<uint8_t> data) { 
    return std::string((const char *)&data[0], data.size()); 
}

std::vector<uint8_t> get_buffer(std::string str) {
    return std::vector<uint8_t>(str.c_str(), str.c_str() + str.length());
}

std::vector<uint8_t> get_buffer_json(const std::vector<uint8_t> &data) { 
    assert(false); 
    return std::vector<uint8_t>();
}

std::vector<uint8_t> get_buffer_json(const nlohmann::json &data) {
    auto text = data.dump();
    return std::vector<uint8_t>(text.c_str(), text.c_str() + text.length());

    // std::vector<uint8_t> result;
    // for (auto item : data) {
    //     assert(item.is_number_integer());
    //     auto value = (int)item;
    //     assert(value >= 0 and value < 256);
    //     result.push_back((uint8_t)value);
    // }
    // return result;
}

std::vector<uint8_t> get_buffer(std::vector<uint8_t> a, std::vector<uint8_t> b) {
    std::vector<uint8_t> res(a.size() + b.size());
    res.resize(0);
    for (auto item : a) {
        res.push_back(item);
    }
    for (auto item : b) {
        res.push_back(item);
    }
    return res;
}

void set_buffer(std::vector<uint8_t> &dest, void *data, size_t size) {
    auto buf = reinterpret_cast<uint8_t *>(data);
    dest.assign(buf, buf + size);
}

nlohmann::json get_json(std::vector<uint8_t> data) { 
    return nlohmann::json::parse(get_string(data)); 
}

bool same_sets(std::vector<int>& a, std::vector<int>& b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (auto& aa : a) {
        bool found = false;
        for (auto& bb : b) {
            if (aa == bb) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

}  // namespace nrpc_cpp
