#include "server/MixerOscService.hpp"
#include "ui_helpers.hpp"
#include <iostream>

namespace TotalMixer {

MixerOscService::MixerOscService(std::shared_ptr<AlsaBackend> backend, const std::string& port)
    : backend(backend), port(port) {}

MixerOscService::~MixerOscService() {
    stop();
}

bool MixerOscService::start() {
    // Create new server thread
    st = lo_server_thread_new(port.c_str(), NULL);
    if (!st) {
        std::cerr << "Failed to create OSC server on port " << port << std::endl;
        return false;
    }

    // Add methods
    lo_server_thread_add_method(st, "/totalmixer/matrix", "iif", generic_handler, this);
    lo_server_thread_add_method(st, "/totalmixer/output", "if", generic_handler, this);
    
    // Add generic handler for debugging
    // lo_server_thread_add_method(st, NULL, NULL, generic_handler, this);

    lo_server_thread_start(st);
    std::cout << "OSC Server listening on port " << port << std::endl;
    return true;
}

void MixerOscService::stop() {
    if (st) {
        lo_server_thread_free(st);
        st = nullptr;
    }
}

int MixerOscService::generic_handler(const char *path, const char *types, lo_arg **argv,
                                     int argc, lo_message msg, void *user_data) {
    (void)msg; // Unused
    auto* self = static_cast<MixerOscService*>(user_data);
    if (!self || !self->backend) return 1;

    std::string path_str = path;
    
    if (path_str == "/totalmixer/matrix" && argc == 3) {
        // iif: out, src, gain
        int out = argv[0]->i;
        int src = argv[1]->i;
        float gain = argv[2]->f;
        self->backend->setMatrixGain(out, src, gain);
        // std::cout << "OSC Matrix: " << out << " <- " << src << " : " << gain << "dB" << std::endl;
        return 0;
    }
    
    if (path_str == "/totalmixer/output" && argc == 2) {
        // if: out, gain
        int out = argv[0]->i;
        float gain = argv[1]->f;
        self->backend->setOutputVolume(out, gain);
        return 0;
    }

    return 1; // Unhandled
}

} // namespace TotalMixer
