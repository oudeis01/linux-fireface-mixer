#pragma once

#include "AlsaBackend.hpp"
#include <lo/lo.h>
#include <memory>
#include <string>

namespace TotalMixer {

class MixerOscService {
public:
    MixerOscService(std::shared_ptr<AlsaBackend> backend, const std::string& port);
    ~MixerOscService();

    bool start();
    void stop();

private:
    std::shared_ptr<AlsaBackend> backend;
    std::string port;
    lo_server_thread st = nullptr;

    static int generic_handler(const char *path, const char *types, lo_arg **argv,
                               int argc, lo_message msg, void *user_data);

    static int sync_handler(const char *path, const char *types, lo_arg **argv,
                            int argc, lo_message msg, void *user_data);
};

}
