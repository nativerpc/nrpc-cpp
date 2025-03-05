/**
 * Contents:
 *
 *       ServerSocket
 *          ServerSocket
 *          bind
 *          get_client_change
 *          recv_norm
 *          send_norm
 *          send_rev
 *          recv_rev
 *          _add_client
 *          _track_client
 *          _recv_norm_step
 *          _recv_rev_step
 *          _forward_call
 *          get_client_ids
 *          get_client_full
 *          get_client_info
 *          add_metadata
 *          get_request_lock
 *          get_metadata
 *          update
 *          wait
 *          close
 */
#pragma once
#include "common_base.hpp"
#include <mutex>

namespace nrpc_cpp {

class ServerSocket {
public:
    ServerSocket(std::string ip_address, int port, int port_rev, std::string entry_file);
    void bind();
    bool get_client_change(int timeout_ms, std::vector<int>& expected_clients);

    bool recv_norm(int& client_id, std::vector<std::vector<uint8_t>>& response);
    void send_norm(int client_id, std::vector<std::vector<uint8_t>>& request);
    void send_rev(int client_id, std::vector<std::vector<uint8_t>>& request);
    bool recv_rev(int client_id, std::vector<uint8_t>& response);
    void _add_client(std::vector<std::vector<uint8_t>>& req);
    void _track_client();
    bool _recv_norm_step(std::vector<std::vector<uint8_t>>& request);
    bool _recv_rev_step(std::shared_ptr<ClientInfo> client, std::vector<std::vector<uint8_t>>& response);
    void _forward_call(std::vector<std::vector<uint8_t>>& req);
    std::vector<int> get_client_ids();
    std::vector<std::shared_ptr<ClientInfo>> get_client_full();
    std::shared_ptr<ClientInfo> get_client_info(int client_id);
    void add_metadata(nlohmann::json data);
    std::recursive_mutex& get_request_lock();
    nlohmann::json get_metadata();
    void update();
    void wait();
    void close();

private:
    int server_id_{0};
    std::string ip_address_;
    int port_{0};
    int port_rev_{0};
    std::string entry_file_;
    int next_index_{0};
    std::vector<uint8_t> server_signature_;
    std::vector<uint8_t> server_signature_rev_;
    std::vector<std::shared_ptr<ClientInfo>> clients_;
    nlohmann::json metadata_;
    void* zmq_context_{0};
    void* zmq_server_{0};
    void* zmq_server_rev_{0};
    void* zmq_monitor_{0};
    std::shared_ptr<std::thread> zmq_monitor_thread_;
    std::recursive_mutex request_lock_;
    bool is_alive_{false};
    std::vector<std::vector<uint8_t>> norm_messages_;
    std::vector<std::vector<uint8_t>> rev_messages_;
};

}  // namespace nrpc_cpp