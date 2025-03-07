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
 *          _forward_call
 *          _recv_norm_step
 *          _recv_rev_step
 *          get_client_ids
 *          get_client_full
 *          get_client_info
 *          add_metadata
 *          get_request_lock
 *          get_metadata
 *          set_closing
 *          update
 *          wait
 *          close
 */
#include "server_socket.hpp"

#include <zmq.h>
#ifdef _WIN32
#include <winsock2.h>
#endif

namespace nrpc_cpp {

ServerSocket::ServerSocket(std::string ip_address, int port, int port_rev, std::string entry_file) {
    server_id_ = 0;
    ip_address_ = ip_address;
    port_ = port;
    port_rev_ = port_rev;
    entry_file_ = entry_file;
    next_index_ = 0;
    server_signature_ = get_buffer("server:0");
    server_signature_rev_ = get_buffer("rev:server:0");
    clients_.clear();

    char hostname[1024];
    hostname[0] = 0;
    gethostname(hostname, sizeof(hostname));

    SocketMetadataInfo metadata;
    metadata.server_id = 0;
    metadata.lang = "cpp";
    metadata.ip_address = ip_address;
    metadata.main_port = port;
    metadata.main_port_rev = port_rev;
    metadata.host = std::string(hostname);
    metadata.entry_file = entry_file_;
    metadata.start_time = get_iso_time(std::chrono::system_clock::now());
    metadata.server_signature = nrpc_cpp::base64_encode(server_signature_);
    metadata.server_signature_rev = nrpc_cpp::base64_encode(server_signature_rev_);

    metadata_ = {
        {"server_id", metadata.server_id},
        {"lang", metadata.lang},
        {"ip_address", metadata.ip_address},
        {"main_port", metadata.main_port},
        {"main_port_rev", metadata.main_port_rev},
        {"host", metadata.host},
        {"entry_file", metadata.entry_file},
        {"start_time", metadata.start_time},
        {"server_signature", metadata.server_signature},
        {"server_signature_rev", metadata.server_signature_rev},
    };

    is_alive_ = true;

    zmq_context_ = 0;
    zmq_server_ = 0;
    zmq_server_rev_ = 0;
    zmq_monitor_ = 0;
    zmq_monitor_thread_.reset();

    zmq_context_ = zmq_ctx_new();

    zmq_server_ = zmq_socket(zmq_context_, ZMQ_ROUTER);
    auto rc = zmq_setsockopt(zmq_server_, ZMQ_IDENTITY, &server_signature_[0], server_signature_.size());
    assert(rc == 0);

    zmq_server_rev_ = zmq_socket(zmq_context_, ZMQ_ROUTER);
    rc = zmq_setsockopt(zmq_server_rev_, ZMQ_IDENTITY, &server_signature_rev_[0], server_signature_rev_.size());
    assert(rc == 0);

    // self.zmq_monitor = zmq_server.get_monitor_socket(zmq.Event.ALL)
    // self.zmq_monitor_thread = threading.Thread(target=self._track_client)
    // self.zmq_monitor_thread.start()
}

void ServerSocket::bind() {
    auto addr = boost::str(boost::format("tcp://%1%:%2%") % ip_address_ % port_);
    auto addr_rev = boost::str(boost::format("tcp://%1%:%2%") % ip_address_ % port_rev_);
    auto rc = zmq_bind(zmq_server_, addr.c_str());
    assert(rc == 0);
    rc = zmq_bind(zmq_server_rev_, addr_rev.c_str());
    assert(rc == 0);
}

bool ServerSocket::get_client_change(int timeout_ms, std::vector<int>& expected_clients) {
    auto started = std::chrono::steady_clock::now();
    while (is_alive_) {
        auto client_ids = get_client_ids();
        if (!same_sets(client_ids, expected_clients)) {
            return true;
        }
        if (timeout_ms == 0) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();

        if (elapsed_ms > timeout_ms) {
            break;
        }
    }
    return false;
}

bool ServerSocket::recv_norm(int& client_id, std::vector<std::vector<uint8_t>>& response) {
    std::vector<std::vector<uint8_t>> req;
    response.resize(0);
    auto found = false;
    auto found_client_id = 0;

    while (is_alive_) {
        _recv_norm_step(req);
        if (!req.size()) {
            continue;
        }

        if (req[1] == ServerMessage::AddClient) {
            _add_client(req);
        } else if (req[1] == ServerMessage::ForwardCall) {
            _forward_call(req);
        } else {
            auto client = nrpc_cpp::find(clients_, [&req](auto x) { return x->client_signature == req[0]; });
            if (!client) {
                std::cerr << boost::str(boost::format("Dropping unknown client: %1%") % base64_encode(req[0]))
                          << std::endl;
                continue;
            }
            found = true;
            found_client_id = client->client_id;
            break;
        }
    }

    if (!found) {
        return false;
    }

    client_id = found_client_id;
    response.resize(2);
    response[0] = req[1];
    response[1] = req[2];
    return true;
}

void ServerSocket::send_norm(int client_id, std::vector<std::vector<uint8_t>>& response) {
    zmq_msg_t msg;
    auto client = nrpc_cpp::find(clients_, [client_id](auto x) { return x->client_id == client_id; });
    assert(client);
    assert(response.size() == 2);
    auto& part0 = client->client_signature;
    auto& part1 = response[0];
    auto& part2 = response[1];

    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_server_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);
}

void ServerSocket::send_rev(int client_id, std::vector<std::vector<uint8_t>>& request) {
    zmq_msg_t msg;
    auto client = nrpc_cpp::find(clients_, [client_id](auto x) { return x->client_id == client_id; });
    assert(client);
    assert(request.size() == 2);

    if (client->is_lost) {
        // std::cerr << "Old client: " << client_id << std::endl;
        return;
    }

    auto peer_state =
        zmq_socket_get_peer_state(zmq_server_, &client->client_signature[0], client->client_signature.size()) + 1;
    auto peer_state_rev =
        zmq_socket_get_peer_state(zmq_server_rev_, &client->client_signature_rev[0], client->client_signature_rev.size()) +
        1;
    if (peer_state == 0 || peer_state_rev == 0) {
        client->is_lost = true;
        // std::cerr << "Lost client: " << client_id << std::endl;
        return;
    }

    auto& part0 = client->client_signature_rev;
    auto& part1 = request[0];
    auto& part2 = request[1];
    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_server_rev_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_server_rev_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_server_rev_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);
}

bool ServerSocket::recv_rev(int client_id, std::vector<uint8_t>& response) {
    std::vector<std::vector<uint8_t>> resp;
    auto client = nrpc_cpp::find(clients_, [client_id](auto x) { return x->client_id == client_id; });
    assert(client);
    response.resize(0);

    if (!(is_alive_ && !client->is_lost)) {
        return false;
    }

    while (is_alive_ && !client->is_lost) {
        _recv_rev_step(client, resp);
        if (!resp.size()) {
            continue;
        }
        break;
    }

    if (!(is_alive_ && !client->is_lost)) {
        return false;
    }

    assert(resp.size());
    response = resp[2];
    return true;
}

void ServerSocket::_add_client(std::vector<std::vector<uint8_t>>& req) {
    zmq_msg_t msg;
    next_index_ += 1;
    auto client = std::make_shared<ClientInfo>();
    client->client_id = next_index_;
    client->client_signature = req[0];
    client->client_signature_rev = get_buffer(get_buffer("rev:"), client->client_signature);
    client->client_metadata = get_json(req[2]);
    client->connect_time = std::chrono::steady_clock::now();
    client->is_validated = false;
    client->is_lost = false;
    clients_.push_back(client);

    auto resp = nlohmann::json({
        {"client_id", client->client_id},
        {"client_signature", nrpc_cpp::base64_encode(client->client_signature)},
        {"client_signature_rev", nrpc_cpp::base64_encode(client->client_signature_rev)},
        {"client_metadata", client->client_metadata},
        {"server_metadata", metadata_},
    });

    auto part0 = client->client_signature;
    auto part1 = ServerMessage::ClientAdded;
    auto part2 = get_buffer_json(resp);

    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_server_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // TODO: check client_signature_rev peer status

    // Validate reverse direction
    {
        std::lock_guard<std::recursive_mutex> lock(request_lock_);
        part0 = client->client_signature_rev;
        part1 = ServerMessage::ValidateClient;
        // part2 = part2;

        auto rc = zmq_msg_init_size(&msg, part0.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
        rc = zmq_msg_send(&msg, zmq_server_rev_, ZMQ_SNDMORE);
        assert(rc == part0.size());
        zmq_msg_close(&msg);

        rc = zmq_msg_init_size(&msg, part1.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
        rc = zmq_msg_send(&msg, zmq_server_rev_, ZMQ_SNDMORE);
        assert(rc == part1.size());
        zmq_msg_close(&msg);

        rc = zmq_msg_init_size(&msg, part2.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
        rc = zmq_msg_send(&msg, zmq_server_rev_, 0);
        assert(rc == part2.size());
        zmq_msg_close(&msg);

        rc = zmq_msg_init(&msg);
        assert(rc == 0);
        rc = zmq_msg_recv(&msg, zmq_server_rev_, 0);
        assert(rc != -1);
        assert(zmq_msg_more(&msg));
        set_buffer(part0, zmq_msg_data(&msg), zmq_msg_size(&msg));
        rc = zmq_msg_recv(&msg, zmq_server_rev_, 0);
        assert(rc != -1);
        assert(zmq_msg_more(&msg));
        set_buffer(part1, zmq_msg_data(&msg), zmq_msg_size(&msg));
        rc = zmq_msg_recv(&msg, zmq_server_rev_, 0);
        assert(rc != -1);
        assert(!zmq_msg_more(&msg));
        set_buffer(part2, zmq_msg_data(&msg), zmq_msg_size(&msg));
        zmq_msg_close(&msg);

        assert(part0 == client->client_signature_rev);
        assert(part1 == ServerMessage::ClientValidated);
        auto resp3 = nrpc_cpp::get_json(part2);
        assert((int)resp3["client_id"] == client->client_id);
        assert(nrpc_cpp::base64_decode((std::string)resp3["client_signature"]) == client->client_signature);

        client->is_validated = true;
    }
}

void ServerSocket::_track_client() {
    assert(false);
}

bool ServerSocket::_recv_norm_step(std::vector<std::vector<uint8_t>>& request) {
    auto ready = false;
    auto ready_timeout = false;
    request.resize(0);
    zmq_msg_t msg;

    while (is_alive_) {
        auto timeout_ms = 100;
        zmq_setsockopt(zmq_server_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        auto rc = zmq_msg_init(&msg);
        assert(rc == 0);
        rc = zmq_msg_recv(&msg, zmq_server_, 0);
        if (rc == -1) {
            ready_timeout = true;
            assert(zmq_errno() == EAGAIN);
        }
        if (!is_alive_) {
            zmq_msg_close(&msg);
            break;
        }
        timeout_ms = -1;
        zmq_setsockopt(zmq_server_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

        if (!is_alive_) {
            zmq_msg_close(&msg);
            break;
        }
        if (ready_timeout) {
            zmq_msg_close(&msg);
            break;
        }
                
        norm_messages_.resize(norm_messages_.size() + 1);
        set_buffer(norm_messages_.back(), zmq_msg_data(&msg), zmq_msg_size(&msg));
        auto ready_last = !zmq_msg_more(&msg);
        zmq_msg_close(&msg);
        if (ready_last) {
            ready = true;
            break;
        }
    }

    if (!ready) {
        return false;
    }

    request = norm_messages_;
    norm_messages_.clear();
    assert(request.size() == 3);
    return true;
}

bool ServerSocket::_recv_rev_step(std::shared_ptr<ClientInfo> client, std::vector<std::vector<uint8_t>>& response) {
    auto ready = false;
    auto ready_timeout = false;
    response.resize(0);
    zmq_msg_t msg;

    while (is_alive_ && !client->is_lost) {
        auto timeout_ms = 100;
        zmq_setsockopt(zmq_server_rev_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        auto rc = zmq_msg_init(&msg);
        assert(rc == 0);
        rc = zmq_msg_recv(&msg, zmq_server_rev_, 0);
        if (rc == -1) {
            ready_timeout = true;
            assert(zmq_errno() == EAGAIN);
        }
        if (!(is_alive_ && !client->is_lost)) {
            zmq_msg_close(&msg);
            break;
        }
        timeout_ms = -1;
        zmq_setsockopt(zmq_server_rev_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

        if (!(is_alive_ && !client->is_lost)) {
            zmq_msg_close(&msg);
            break;
        }
        if (ready_timeout) {
            zmq_msg_close(&msg);
            auto peer_state =
                zmq_socket_get_peer_state(zmq_server_, &client->client_signature[0], client->client_signature.size()) + 1;
            auto peer_state_rev =
                zmq_socket_get_peer_state(zmq_server_rev_, &client->client_signature_rev[0], client->client_signature_rev.size()) +
                1;
            if (peer_state == 0 || peer_state_rev == 0) {
                client->is_lost = true;
                // std::cerr << "Lost client: " << client_id << std::endl;
                break;
            }
            break;
        }

        rev_messages_.resize(rev_messages_.size() + 1);
        set_buffer(rev_messages_.back(), zmq_msg_data(&msg), zmq_msg_size(&msg));
        auto ready_last = !zmq_msg_more(&msg);
        zmq_msg_close(&msg);
        if (ready_last) {
            ready = true;
            break;
        }
    }

    if (!ready) {
        return false;
    }

    response = rev_messages_;
    rev_messages_.clear();
    assert(response.size() == 3);
    assert(response[0] == client->client_signature_rev);
    return true;
}

void ServerSocket::_forward_call(std::vector<std::vector<uint8_t>>& req) {
    auto req2 = nrpc_cpp::get_json(req[2]);
    assert(req2.contains("client_id"));
    auto client_id = (int)req2["client_id"];
    auto method_name = (std::string)req2["method_name"];
    auto method_params = (nlohmann::json)req2["method_params"];
    auto client1 = nrpc_cpp::find(clients_, [&req, client_id](auto x) { return x->client_signature == req[0]; });
    auto client2 = nrpc_cpp::find(clients_, [&req, client_id](auto x) { return x->client_id == client_id; });
    assert(client1);
    assert(client2);

    std::vector<uint8_t> res;
    {
        std::lock_guard<std::recursive_mutex> lock(request_lock_);
        std::vector<std::vector<uint8_t>> req3;
        req3.resize(2);
        req3[0] = nrpc_cpp::get_buffer(method_name);
        req3[1] = nrpc_cpp::get_buffer_json(method_params);
        send_rev(client_id, req3);
        recv_rev(client_id, res);
    }

    auto part0 = client1->client_signature;
    auto part1 = get_buffer(get_buffer("fwd_response:"), get_buffer(method_name));
    auto part2 = res;

    zmq_msg_t msg;
    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_server_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_server_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);
}

std::vector<int> ServerSocket::get_client_ids() {
    std::vector<int> result;
    for (auto client : clients_) {
        if (client->is_lost) continue;
        if (client->client_signature_rev.empty()) continue;
        if (!client->is_validated) continue;
        auto peer_state =
            zmq_socket_get_peer_state(zmq_server_, &client->client_signature[0], client->client_signature.size()) + 1;
        auto peer_state_rev = zmq_socket_get_peer_state(zmq_server_rev_, &client->client_signature_rev[0],
                                                        client->client_signature_rev.size()) +
                              1;
        if (peer_state == 0 || peer_state_rev == 0) {
            client->is_lost = true;
            // std::cerr << "Lost client: " << client->client_id << std::endl;
            continue;
        }
        result.push_back(client->client_id);
    }
    return result;
}

std::vector<std::shared_ptr<ClientInfo>> ServerSocket::get_client_full() { return clients_; }

std::shared_ptr<ClientInfo> ServerSocket::get_client_info(int client_id) {
    return find(
        clients_,
        [client_id](auto x){
            return x->client_id == client_id;
        }
    );
}

void ServerSocket::add_metadata(nlohmann::json data) { assert(false); }

std::recursive_mutex& ServerSocket::get_request_lock() { return request_lock_; }

nlohmann::json ServerSocket::get_metadata() { return metadata_; }

void ServerSocket::set_closing() {
    is_alive_ = false;
}

void ServerSocket::update() {
    assert(this);
    for (auto client : clients_) {
        if (client->is_lost) continue;
        auto peer_state =
            zmq_socket_get_peer_state(zmq_server_, &client->client_signature[0], client->client_signature.size()) + 1;
        auto peer_state_rev = zmq_socket_get_peer_state(zmq_server_rev_, &client->client_signature_rev[0],
                                                        client->client_signature_rev.size()) +
                              1;
        if (peer_state == 0 || peer_state_rev == 0) {
            client->is_lost = true;
            // std::cerr << "Lost client: " << client->client_id << std::endl;
        }
    }
}

void ServerSocket::wait() {
    zmq_pollitem_t pollitems[1] = {0};
    pollitems[0].socket = zmq_server_;
    pollitems[0].fd = 0;
    pollitems[0].events = ZMQ_POLLIN;
    pollitems[0].revents = 0;

    while (true) {
        auto rc = zmq_poll(pollitems, 1, 0);
        if (pollitems[0].revents <= 0) {
            // std::cerr << "Poll failed!" << std::endl;
            break;
        }
        // rc = zmq_getsockopt(
        //     s.handle, ZMQ_TYPE, cast(p_void, address(stype)), address(sz)
        // )
        // if rc < 0:
        //     errno = zmq_errno()
        //     if errno == ENOTSOCK:
        //         s._closed = True
        //         return True
        //     elif errno == ZMQ_ETERM:
        //         # don't raise ETERM when checking if we're closed
        //         return False
        // else:
        //     _check_rc(rc)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ServerSocket::close() {
    auto zmq_context = zmq_context_;
    std::vector<void*> servers = {zmq_server_, zmq_server_rev_, zmq_monitor_};

    is_alive_ = false;

    if (zmq_monitor_thread_) {
        zmq_monitor_thread_->join();
    }

    auto rc = zmq_socket_monitor(zmq_server_, 0, 0);
    assert(rc == 0);

    zmq_server_ = 0;
    zmq_server_rev_ = 0;
    zmq_monitor_ = 0;
    zmq_monitor_thread_.reset();
    zmq_context_ = 0;

    for (int j = 0; j < 3; j++) {
        if (servers[j]) {
            zmq_close(servers[j]);
        }
    }

    zmq_close(zmq_context);
}

}  // namespace nrpc_cpp