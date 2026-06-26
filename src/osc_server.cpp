#include "osc_server.hpp"

#include <lo/lo.h>
#include <iostream>
#include <cstdlib>

namespace TotalMixer {

// ── File-local liblo glue ──────────────────────────────────────────────────

static void osc_err_handler(int num, const char* msg, const char* where) {
    std::cerr << "[OSC] liblo error " << num << ": " << (msg ? msg : "")
              << " (" << (where ? where : "") << ")" << std::endl;
}

static OscCmdType map_type(const std::string& g, const std::string& c) {
    if (g == "out") {
        if (c == "fader") return OscCmdType::OutFader;
        if (c == "mute")  return OscCmdType::OutMute;
        if (c == "solo")  return OscCmdType::OutSolo;
        if (c == "link")  return OscCmdType::OutLink;
    } else if (g == "in") {
        if (c == "fader") return OscCmdType::InFader;
        if (c == "mute")  return OscCmdType::InMute;
    } else if (g == "pb") {
        if (c == "fader") return OscCmdType::PbFader;
        if (c == "mute")  return OscCmdType::PbMute;
    } else if (g == "submix") {
        if (c == "select") return OscCmdType::SubmixSelect;
    }
    return OscCmdType::Unknown;
}

// Catch-all handler: runs on the liblo server thread. Parses "/group/control/N" + one numeric
// argument into an OscCommand and hands it to the OscServer instance (user pointer).
static int osc_handler(const char* path, const char* types, lo_arg** argv, int argc,
                       lo_message msg, void* user) {
    OscServer* self = static_cast<OscServer*>(user);
    if (!self || !path) return 0;

    // Tokenize the path by '/'.
    std::vector<std::string> tok;
    {
        std::string cur;
        for (const char* p = path; *p; ++p) {
            if (*p == '/') { if (!cur.empty()) { tok.push_back(cur); cur.clear(); } }
            else cur += *p;
        }
        if (!cur.empty()) tok.push_back(cur);
    }
    if (tok.empty()) return 0;

    // First argument -> float (accept several numeric/bool OSC types defensively).
    float v = 0.0f;
    if (argc >= 1 && types && types[0]) {
        switch (types[0]) {
            case 'f': v = argv[0]->f; break;
            case 'd': v = (float)argv[0]->d; break;
            case 'i': v = (float)argv[0]->i; break;
            case 'h': v = (float)argv[0]->h; break;
            case 'T': v = 1.0f; break;
            case 'F': v = 0.0f; break;
            default: break;
        }
    }

    OscCommand cmd;
    cmd.value = v;
    if (tok[0] == "query") {
        cmd.type = OscCmdType::QueryAll;
    } else if (tok.size() >= 3) {
        cmd.type = map_type(tok[0], tok[1]);
        cmd.index = atoi(tok[2].c_str()) - 1;  // 1-based path -> 0-based index
    }
    if (cmd.type == OscCmdType::Unknown) return 0;

    const char* host = nullptr;
    lo_address src = lo_message_get_source(msg);
    if (src) host = lo_address_get_hostname(src);
    self->EnqueueCommand(cmd, host);
    return 0;
}

// ── OscServer ──────────────────────────────────────────────────────────────

OscServer::OscServer() {}

OscServer::~OscServer() { Stop(); }

bool OscServer::Start(int in_port, int out_port) {
    if (running_.load()) Stop();
    out_port_ = out_port;

    std::string port = std::to_string(in_port);
    lo_server_thread st = lo_server_thread_new(port.c_str(), osc_err_handler);
    if (!st) {
        std::cerr << "[OSC] failed to bind UDP port " << in_port << std::endl;
        return false;
    }
    lo_server_thread_add_method(st, NULL, NULL, osc_handler, this);
    if (lo_server_thread_start(st) < 0) {
        std::cerr << "[OSC] failed to start server thread" << std::endl;
        lo_server_thread_free(st);
        return false;
    }
    server_ = (void*)st;
    running_.store(true);
    std::cout << "[OSC] listening on UDP " << in_port
              << ", feedback -> port " << out_port << std::endl;
    return true;
}

void OscServer::Stop() {
    running_.store(false);
    if (server_) {
        lo_server_thread_stop((lo_server_thread)server_);
        lo_server_thread_free((lo_server_thread)server_);
        server_ = nullptr;
    }
    {
        std::lock_guard<std::mutex> lk(client_mtx_);
        if (client_) { lo_address_free((lo_address)client_); client_ = nullptr; }
        client_host_.clear();
    }
    {
        std::lock_guard<std::mutex> lk(queue_mtx_);
        std::queue<OscCommand> empty;
        std::swap(queue_, empty);
    }
    client_changed_.store(false);
}

void OscServer::EnqueueCommand(const OscCommand& cmd, const char* client_host) {
    {
        std::lock_guard<std::mutex> lk(queue_mtx_);
        queue_.push(cmd);
    }
    if (client_host && *client_host) {
        std::lock_guard<std::mutex> lk(client_mtx_);
        if (client_host_ != client_host || !client_) {
            client_host_ = client_host;
            if (client_) { lo_address_free((lo_address)client_); client_ = nullptr; }
            std::string port = std::to_string(out_port_);
            client_ = (void*)lo_address_new(client_host, port.c_str());
            client_changed_.store(true);
        }
    }
}

std::vector<OscCommand> OscServer::DrainCommands() {
    std::vector<OscCommand> out;
    std::lock_guard<std::mutex> lk(queue_mtx_);
    while (!queue_.empty()) {
        out.push_back(queue_.front());
        queue_.pop();
    }
    return out;
}

bool OscServer::HasClient() const {
    std::lock_guard<std::mutex> lk(client_mtx_);
    return client_ != nullptr;
}

bool OscServer::TakeClientChanged() {
    return client_changed_.exchange(false);
}

void OscServer::SendFloat(const std::string& path, float value) {
    std::lock_guard<std::mutex> lk(client_mtx_);
    if (!client_) return;
    lo_send((lo_address)client_, path.c_str(), "f", value);
}

} // namespace TotalMixer
