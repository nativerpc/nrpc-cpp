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
 *          set_closing
 *          wait
 *          close
 *
 */
#include "client_socket.hpp"

#include <zmq.h>
#ifdef _WIN32
#include <winsock2.h>
#endif

namespace nrpc_cpp {

ClientSocket::ClientSocket(std::string ip_address, int port, int port_rev, std::string socket_name) {
    client_id_ = 0;
    ip_address_ = ip_address;
    port_ = port;
    port_rev_ = port_rev;
    socket_name_ = socket_name;
    server_signature_ = get_buffer("server:0");
    server_signature_rev_ = get_buffer("rev:server:0");
    client_signature_ = get_buffer("");
    client_signature_rev_ = get_buffer("");
    is_alive_ = true;
    is_connected_ = false;
    is_validated_ = false;
    is_lost_ = false;

    char hostname[1024];
    hostname[0] = 0;
    gethostname(hostname, sizeof(hostname));

    SocketMetadataInfo metadata;
    metadata.client_id = 0;
    metadata.lang = "cpp";
    metadata.ip_address = ip_address;
    metadata.main_port = port;
    metadata.main_port_rev = port_rev;
    metadata.host = std::string(hostname);
    metadata.socket_name = socket_name_;
    metadata.start_time = get_iso_time(std::chrono::system_clock::now());
    metadata.client_signature = "";
    metadata.client_signature_rev = "";

    metadata_ = {
        {"client_id", metadata.client_id},
        {"lang", metadata.lang},
        {"ip_address", metadata.ip_address},
        {"main_port", metadata.main_port},
        {"main_port_rev", metadata.main_port_rev},
        {"host", metadata.host},
        {"socket_name", metadata.socket_name},
        {"start_time", metadata.start_time},
        {"client_signature", metadata.client_signature},
        {"client_signature_rev", metadata.client_signature_rev},
    };

    server_metadata_ = {};
    zmq_context_ = 0;
    zmq_client_ = 0;
    zmq_client_rev_ = 0;
    zmq_monitor_ = 0;
    zmq_monitor_thread_ = 0;
}

void ClientSocket::connect() {
    assert(!is_validated_);

    zmq_context_ = zmq_ctx_new();

    zmq_client_ = zmq_socket(zmq_context_, ZMQ_ROUTER);
    assert(zmq_client_);

    auto rc = zmq_connect(zmq_client_, boost::str(boost::format("tcp://%1%:%2%") % ip_address_ % port_).c_str());
    assert(rc == 0);

    rc = zmq_socket_monitor(zmq_client_, "inproc://monitor-client", ZMQ_EVENT_ALL);
    assert(rc == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    zmq_monitor_ = zmq_socket(zmq_context_, ZMQ_PAIR);
    assert(zmq_monitor_);
    rc = zmq_connect(zmq_monitor_, "inproc://monitor-client");
    assert(rc == 0);
    int linger = 0;
    zmq_setsockopt(zmq_monitor_, ZMQ_LINGER, &linger, sizeof(linger));
    zmq_monitor_thread_ = std::make_shared<std::thread>([this]() { _track_client(); });

    while (!is_connected_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::vector<std::vector<uint8_t>> resp;
    {
        std::lock_guard<std::recursive_mutex> lock(request_lock_);
        zmq_msg_t msg;
        auto part0 = server_signature_;
        auto part1 = ServerMessage::AddClient;
        auto part2 = get_buffer_json(metadata_);

        auto rc = zmq_msg_init_size(&msg, part0.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
        rc = zmq_msg_send(&msg, zmq_client_, ZMQ_SNDMORE);
        assert(rc == part0.size());
        zmq_msg_close(&msg);

        rc = zmq_msg_init_size(&msg, part1.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
        rc = zmq_msg_send(&msg, zmq_client_, ZMQ_SNDMORE);
        assert(rc == part1.size());
        zmq_msg_close(&msg);

        rc = zmq_msg_init_size(&msg, part2.size());
        assert(rc == 0);
        memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
        rc = zmq_msg_send(&msg, zmq_client_, 0);
        assert(rc == part2.size());
        zmq_msg_close(&msg);

        while (is_alive_ && !is_lost_) {
            _recv_norm_step(resp);
            if (!resp.size()) {
                continue;
            }
            break;
        }
    }

    assert(resp[1] == ServerMessage::ClientAdded);
    auto resp2 = get_json(resp[2]);

    client_id_ = (int)resp2["client_id"];
    client_signature_ = base64_decode((std::string)resp2["client_signature"]);
    client_signature_rev_ = base64_decode((std::string)resp2["client_signature_rev"]);
    metadata_["client_id"] = client_id_;
    metadata_["client_signature"] = base64_encode(client_signature_);
    metadata_["client_signature_rev"] = base64_encode(client_signature_rev_);

    zmq_client_rev_ = zmq_socket(zmq_context_, ZMQ_ROUTER);

    rc = zmq_setsockopt(zmq_client_rev_, ZMQ_IDENTITY, &client_signature_rev_[0], client_signature_rev_.size());
    assert(rc == 0);
    rc = zmq_connect(zmq_client_rev_, boost::str(boost::format("tcp://%1%:%2%") % ip_address_ % port_rev_).c_str());
    assert(rc == 0);

    std::vector<std::vector<uint8_t>> req;
    while (is_alive_ && !is_lost_) {
        _recv_rev_step(req);
        if (!req.size()) {
            continue;
        }

        if (req[1] == ServerMessage::ValidateClient) {
            _validate_client(req);
            break;
        } else {
            std::cerr << "Early message on client side!" << std::endl;
            auto method_name = get_string(req[1]);
            auto part0 = server_signature_rev_;
            auto part1 = boost::str(boost::format("message_dropped:%1%") % method_name);
            auto part2 = get_buffer_json({{"error", "Early message dropped"}});

            zmq_msg_t msg;
            auto rc = zmq_msg_init_size(&msg, part0.size());
            assert(rc == 0);
            memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
            rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
            assert(rc == part0.size());
            zmq_msg_close(&msg);

            rc = zmq_msg_init_size(&msg, part1.size());
            assert(rc == 0);
            memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
            rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
            assert(rc == part1.size());
            zmq_msg_close(&msg);

            rc = zmq_msg_init_size(&msg, part2.size());
            assert(rc == 0);
            memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
            rc = zmq_msg_send(&msg, zmq_client_rev_, 0);
            assert(rc == part2.size());
            zmq_msg_close(&msg);
        }
    }

    assert(is_validated_);
}

void ClientSocket::send_norm(std::vector<std::vector<uint8_t>>& request) {
    assert(zmq_client_rev_);
    assert(client_id_);
    assert(is_validated_);
    assert(request.size() == 2);
    auto part0 = server_signature_;
    auto part1 = request[0];
    auto part2 = request[1];

    zmq_msg_t msg;
    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_client_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_client_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_client_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);
}

bool ClientSocket::recv_norm(std::vector<uint8_t>& response) {
    std::vector<std::vector<uint8_t>> resp;
    while (is_alive_) {
        _recv_norm_step(resp);
        if (!resp.size()) {
            continue;
        }
        break;
    }
    if (!is_alive_) {
        return false;
    }
    assert(resp.size());
    response = resp[2];
    assert(response[0] == (uint8_t)'{');
    return true;
}

bool ClientSocket::recv_rev(std::vector<std::vector<uint8_t>>& request, int timeout_ms) {
    request.resize(0);
    std::vector<std::vector<uint8_t>> req;

    if (!is_validated_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!is_validated_) {
            return false;
        }
    }

    auto started = std::chrono::steady_clock::now();
    auto found = false;

    while (is_alive_ && !is_lost_) {
        auto elapsed_ms =
            timeout_ms > 0 ? std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started)
                .count() : 0;
        if (timeout_ms > 0 && elapsed_ms > timeout_ms) {
            break;
        }
        _recv_rev_step(req);
        if (!req.size()) {
            continue;
        }

        if (req[1] == ServerMessage::ValidateClient) {
            _validate_client(req);
        } else {
            break;
        }
    }

    if (!(is_alive_ && !is_lost_)) {
        return false;
    }
    if (!req.size()) {
        return false;
    }

    request.resize(2);
    request[0] = req[1];
    request[1] = req[2];
    return true;
}

void ClientSocket::send_rev(std::vector<std::vector<uint8_t>>& response) {
    assert(zmq_client_rev_);
    assert(client_id_);
    assert(is_validated_);
    assert(response.size() == 2);
    auto part0 = server_signature_rev_;
    auto part1 = response[0];
    auto part2 = response[1];

    zmq_msg_t msg;
    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);
}

void ClientSocket::_validate_client(std::vector<std::vector<uint8_t>>& req) {
    assert(req[0] == server_signature_rev_);
    auto req2 = get_json(req[2]);
    assert(client_id_ == (int)req2["client_id"]);
    assert(client_signature_ == base64_decode((std::string)req2["client_signature"]));
    assert(client_signature_rev_ == base64_decode((std::string)req2["client_signature_rev"]));
    server_metadata_ = (nlohmann::json)req2["server_metadata"];

    auto part0 = server_signature_rev_;
    auto part1 = ServerMessage::ClientValidated;
    auto part2 = get_buffer_json(metadata_);

    zmq_msg_t msg;
    auto rc = zmq_msg_init_size(&msg, part0.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part0[0], part0.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
    assert(rc == part0.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part1.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part1[0], part1.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, ZMQ_SNDMORE);
    assert(rc == part1.size());
    zmq_msg_close(&msg);

    rc = zmq_msg_init_size(&msg, part2.size());
    assert(rc == 0);
    memcpy(zmq_msg_data(&msg), &part2[0], part2.size());
    rc = zmq_msg_send(&msg, zmq_client_rev_, 0);
    assert(rc == part2.size());
    zmq_msg_close(&msg);

    is_validated_ = true;
}

void ClientSocket::_track_client() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(zmq_monitor_);
    std::vector<std::vector<uint8_t>> resp;

    while (is_alive_) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        auto rc = zmq_msg_recv(&msg, zmq_monitor_, ZMQ_DONTWAIT);
        if (rc == -1) {
            assert(zmq_errno() == EAGAIN);
            zmq_msg_close(&msg);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        resp.resize(resp.size() + 1);
        auto& last = resp.back();
        set_buffer(last, zmq_msg_data(&msg), zmq_msg_size(&msg));

        if (zmq_msg_more(&msg)) {
            zmq_msg_close(&msg);
            continue;
        }

        zmq_msg_close(&msg);
        assert(resp.size() == 2);

        auto event_id = *(uint16_t*)&resp[0][0];
        auto event_value = *(uint32_t*)&resp[0][2];

        // std::cout<<"EVENT! "<<event_id<<std::endl;

        if (event_id == ZMQ_EVENT_CONNECTED) {
        } else if (event_id == ZMQ_EVENT_HANDSHAKE_SUCCEEDED) {
            is_connected_ = true;
        } else if (event_id == ZMQ_EVENT_DISCONNECTED) {
            is_lost_ = true;
            client_errors_ += boost::str(boost::format("\nConnection lost %1%") % event_value);
        }

        resp.resize(0);
        // if (address) {
        //     uint8_t *data = (uint8_t *) zmq_msg_data (&msg);
        //     size_t size = zmq_msg_size (&msg);
        //     *address = (char *) malloc (size + 1);
        //     memcpy (*address, data, size);
        //     (*address)[size] = 0;
        // }
    }

    // std::cout << "MON EXITED" << std::endl;
}

bool ClientSocket::_recv_norm_step(std::vector<std::vector<uint8_t>>& response) {
    auto ready = false;
    auto ready_timeout = false;
    response.resize(0);
    zmq_msg_t msg;

    while (is_alive_) {
        auto timeout_ms = 100;
        zmq_setsockopt(zmq_client_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        auto rc = zmq_msg_init(&msg);
        assert(rc == 0);
        rc = zmq_msg_recv(&msg, zmq_client_, 0);
        if (rc == -1) {
            ready_timeout = true;
            assert(zmq_errno() == EAGAIN);
        }
        if (!is_alive_) {
            zmq_msg_close(&msg);
            break;
        }
        timeout_ms = -1;
        zmq_setsockopt(zmq_client_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

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

    response = norm_messages_;
    norm_messages_.clear();
    assert(response.size() == 3);
    assert(response[0] == server_signature_);
    return true;
}

bool ClientSocket::_recv_rev_step(std::vector<std::vector<uint8_t>>& request) {
    auto ready = false;
    auto ready_timeout = false;
    request.resize(0);
    zmq_msg_t msg;

    while (is_alive_ && !is_lost_) {
        auto timeout_ms = 100;
        zmq_setsockopt(zmq_client_rev_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        auto rc = zmq_msg_init(&msg);
        assert(rc == 0);
        rc = zmq_msg_recv(&msg, zmq_client_rev_, 0);
        if (rc == -1) {
            ready_timeout = true;
            assert(zmq_errno() == EAGAIN);
        }
        if (!(is_alive_ && !is_lost_)) {
            zmq_msg_close(&msg);
            break;
        }
        timeout_ms = -1;
        zmq_setsockopt(zmq_client_rev_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

        if (!(is_alive_ && !is_lost_)) {
            zmq_msg_close(&msg);
            break;
        }
        if (ready_timeout) {
            zmq_msg_close(&msg);
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

    request = rev_messages_;
    rev_messages_.clear();
    assert(request.size() == 3);
    assert(request[0] == server_signature_rev_);
    return true;
}

void ClientSocket::add_metadata(nlohmann::json data) { assert(false); }

bool ClientSocket::is_validated() { return zmq_client_rev_ && is_validated_; }

std::recursive_mutex& ClientSocket::get_request_lock() { return request_lock_; }

int ClientSocket::get_client_id() { return client_id_; }

nlohmann::json ClientSocket::get_metadata() { return metadata_; }

nlohmann::json ClientSocket::get_server_metadata() { return server_metadata_; }

void ClientSocket::set_closing() {
    is_alive_ = false;
}

void ClientSocket::wait() {
    zmq_pollitem_t pollitems[1] = {0};
    pollitems[0].socket = zmq_client_;
    pollitems[0].fd = 0;
    pollitems[0].events = ZMQ_POLLIN;
    pollitems[0].revents = 0;

    while (is_alive_ && !is_lost_) {
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

void ClientSocket::close() {
    auto zmq_context = zmq_context_;
    std::vector<void*> clients = {zmq_client_, zmq_client_rev_, zmq_monitor_};

    is_alive_ = false;

    zmq_monitor_thread_->join();

    auto rc = zmq_socket_monitor(zmq_client_, 0, 0);
    assert(rc == 0);

    zmq_client_ = 0;
    zmq_client_rev_ = 0;
    zmq_monitor_ = 0;
    zmq_monitor_thread_.reset();
    zmq_context_ = 0;

    for (int j = 0; j < 3; j++) {
        if (clients[j]) {
            zmq_close(clients[j]);
        }
    }

    zmq_close(zmq_context);
}

}  // namespace nrpc_cpp