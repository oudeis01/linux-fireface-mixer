#pragma once

#include "IMixerBackend.hpp"
#include <string>
#include <vector>
#include <lo/lo.h>

namespace TotalMixer {

class OscClientBackend : public IMixerBackend {
public:
    explicit OscClientBackend(const std::string& target_ip, const std::string& port);
    virtual ~OscClientBackend();

    // --- IMixerBackend Implementation ---
    bool isConnected() const override;
    std::string getDeviceName() const override;
    std::string getStatusMessage() const override;
    bool isLocal() const override { return false; }

    bool setMatrixGain(int output_ch, int source_ch, float gain_db) override;
    bool setOutputVolume(int output_ch, float gain_db) override;

    // Not supported via OSC yet
    bool setPhantomPower(int channel, bool enabled) override { return false; }
    bool setPad(int channel, bool enabled) override { return false; }
    bool setInstrument(int channel, bool enabled) override { return false; }

    bool getMeterLevels(MeterData& out_meters) override;
    
    // Getters (read from local cache)
    float getMatrixGain(int output_ch, int source_ch) override;
    float getOutputVolume(int output_ch) override;

    void update() override;
    bool initialize() override;
    void shutdown() override;

private:
    std::string target_ip;
    std::string target_port;
    lo_address lo_addr = nullptr;
    
    bool connected = false;
    std::string status_msg = "Disconnected";

    // Local cache
    std::vector<float> matrix_cache; 
    std::vector<float> output_cache;
};

}
