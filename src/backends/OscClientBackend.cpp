#include "backends/OscClientBackend.hpp"
#include <iostream>
#include <stdarg.h>

namespace TotalMixer {

OscClientBackend::OscClientBackend(const std::string& target_ip, const std::string& port) 
    : target_ip(target_ip), target_port(port) {
    matrix_cache.resize(18 * 36, -144.0f);
    output_cache.resize(18, -144.0f);
}

OscClientBackend::~OscClientBackend() {
    shutdown();
}

bool OscClientBackend::initialize() {
    lo_addr = lo_address_new(target_ip.c_str(), target_port.c_str());
    if (!lo_addr) {
        status_msg = "Failed to create OSC address";
        return false;
    }

    // Start Listener for Sync
    listener = lo_server_thread_new(NULL, NULL); // Random port
    if (listener) {
        lo_server_thread_add_method(listener, "/totalmixer/output", "if", output_handler, this);
        lo_server_thread_add_method(listener, "/totalmixer/matrix", "iif", matrix_handler, this);
        lo_server_thread_start(listener);
        
        int listen_port = lo_server_thread_get_port(listener);
        std::cout << "OSC Client listening on port " << listen_port << ", requesting sync..." << std::endl;
        
        // Request Sync
        lo_send(lo_addr, "/totalmixer/sync", "i", listen_port);
    } else {
        std::cerr << "Failed to start OSC listener thread" << std::endl;
    }

    connected = true;
    status_msg = "OSC Target: " + target_ip + ":" + target_port;
    return true;
}

void OscClientBackend::shutdown() {
    if (listener) {
        lo_server_thread_free(listener);
        listener = nullptr;
    }
    if (lo_addr) {
        lo_address_free(lo_addr);
        lo_addr = nullptr;
    }
    connected = false;
    status_msg = "Disconnected";
}

bool OscClientBackend::isConnected() const { return connected; }
std::string OscClientBackend::getDeviceName() const { return "OSC Remote (" + target_ip + ")"; }
std::string OscClientBackend::getStatusMessage() const { return status_msg; }

bool OscClientBackend::setMatrixGain(int output_ch, int source_ch, float gain_db) {
    if (!connected) return false;

    // OSC Path: /totalmixer/matrix <out> <src> <gain>
    if (lo_send(lo_addr, "/totalmixer/matrix", "iif", output_ch, source_ch, gain_db) != -1) {
        // Optimistic update
        int idx = source_ch * 18 + output_ch;
        if (idx < (int)matrix_cache.size()) {
            matrix_cache[idx] = gain_db;
        }
        return true;
    }
    return false;
}

bool OscClientBackend::setOutputVolume(int output_ch, float gain_db) {
    if (!connected) return false;

    // OSC Path: /totalmixer/output <out> <gain>
    if (lo_send(lo_addr, "/totalmixer/output", "if", output_ch, gain_db) != -1) {
        if (output_ch < (int)output_cache.size()) {
            output_cache[output_ch] = gain_db;
        }
        return true;
    }
    return false;
}

float OscClientBackend::getMatrixGain(int output_ch, int source_ch) {
    int idx = source_ch * 18 + output_ch;
    if (idx >= 0 && idx < (int)matrix_cache.size()) {
        return matrix_cache[idx];
    }
    return -100.0f;
}

float OscClientBackend::getOutputVolume(int output_ch) {
    if (output_ch >= 0 && output_ch < (int)output_cache.size()) {
        return output_cache[output_ch];
    }
    return -100.0f;
}

bool OscClientBackend::getMeterLevels(MeterData& out_meters) {
    // OSC metering requires a listener server, simpler to skip for now
    return false;
}

void OscClientBackend::update() {
    // Process incoming OSC bundles if we implement a listener later
}

int OscClientBackend::output_handler(const char *path, const char *types, lo_arg **argv,
                                     int argc, lo_message msg, void *user_data) {
    auto* self = static_cast<OscClientBackend*>(user_data);
    int ch = argv[0]->i;
    float val = argv[1]->f;
    if (ch < (int)self->output_cache.size()) {
        self->output_cache[ch] = val;
    }
    return 0;
}

int OscClientBackend::matrix_handler(const char *path, const char *types, lo_arg **argv,
                                     int argc, lo_message msg, void *user_data) {
    auto* self = static_cast<OscClientBackend*>(user_data);
    int out = argv[0]->i;
    int src = argv[1]->i;
    float val = argv[2]->f;
    
    int idx = src * 18 + out;
    if (idx < (int)self->matrix_cache.size()) {
        self->matrix_cache[idx] = val;
    }
    return 0;
}

} // namespace TotalMixer
