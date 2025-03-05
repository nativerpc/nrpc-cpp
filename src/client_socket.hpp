/**
 * Contents:
 *
 *       ClientSocket
 *          ClientSocket
 *          connect
 *          send_norm
 *          recv_norm
 *          recv_rev
 *          send_rev
 *          _validate_client
 *          _track_client
 *          _recv_norm_step
 *          _recv_rev_step
 *          add_metadata
 *          is_validated
 *          get_request_lock
 *          get_client_id
 *          get_metadata
 *          get_server_metadata
 *          wait
 *          close
 * 
 */
#pragma once
#include "common_base.hpp"
#include <mutex>

namespace nrpc_cpp {

class ClientSocket {
public:
    ClientSocket(std::string ip_address, int port, int port_rev, std::string entry_file);
    void connect();
    void send_norm(std::vector<std::vector<uint8_t>>& request);
    bool recv_norm(std::vector<uint8_t>& response);
    bool recv_rev(std::vector<std::vector<uint8_t>>& request, int timeout_ms = 0);
    void send_rev(std::vector<std::vector<uint8_t>>& response);
    void _validate_client(std::vector<std::vector<uint8_t>>& req);
    void _track_client();
    bool _recv_norm_step(std::vector<std::vector<uint8_t>>& request);
    bool _recv_rev_step(std::vector<std::vector<uint8_t>>& response);
    void add_metadata(nlohmann::json data);
    bool is_validated();
    std::recursive_mutex& get_request_lock();
    int get_client_id();
    nlohmann::json get_metadata();
    nlohmann::json get_server_metadata();
    void wait();
    void close();

private:
    int client_id_{0};
    std::string ip_address_;
    int port_{0};
    int port_rev_{0};
    std::string entry_file_;
    std::vector<uint8_t> server_signature_;
    std::vector<uint8_t> server_signature_rev_;
    std::vector<uint8_t> client_signature_;
    std::vector<uint8_t> client_signature_rev_;
    bool is_alive_{false};
    bool is_connected_{false};
    bool is_validated_{false};
    bool is_lost_{false};
    std::string last_error_;
    nlohmann::json metadata_;
    nlohmann::json server_metadata_;
    void* zmq_context_{0};
    void* zmq_client_{0};
    void* zmq_client_rev_{0};
    void* zmq_monitor_{0};
    std::shared_ptr<std::thread> zmq_monitor_thread_;
    std::recursive_mutex request_lock_;
    std::vector<std::vector<uint8_t>> norm_messages_;
    std::vector<std::vector<uint8_t>> rev_messages_;
};

}  // namespace nrpc_cpp