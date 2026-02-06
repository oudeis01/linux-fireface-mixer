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
    if (lo_addr) {
        connected = true;
        status_msg = "OSC Target: " + target_ip + ":" + target_port;
        return true;
    } else {
        status_msg = "Failed to create OSC address";
        return false;
    }
}

void OscClientBackend::shutdown() {
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

void OscClientBackend::sendOsc(const char* path, const char* types, ...) {
    if (!lo_addr) return;
    va_list ap;
    va_start(ap, types);
    lo_send_message_from_args(lo_addr, lo_address_get_entry(lo_addr), path, types, ap);
    va_end(ap);
}

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

} // namespace TotalMixer
