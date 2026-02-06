#pragma once

#include "IMixerBackend.hpp"
#include "alsa_core.hpp"
#include <memory>
#include <string>

namespace TotalMixer {

class AlsaBackend : public IMixerBackend {
public:
    AlsaBackend();
    virtual ~AlsaBackend();

    // --- Connection Status ---
    bool isConnected() const override;
    std::string getDeviceName() const override;
    std::string getStatusMessage() const override;
    bool isLocal() const override { return true; }

    // --- Matrix Mixing ---
    bool setMatrixGain(int output_ch, int source_ch, float gain_db) override;

    // --- Output Faders ---
    bool setOutputVolume(int output_ch, float gain_db) override;

    // --- Getters ---
    float getMatrixGain(int output_ch, int source_ch) override;
    float getOutputVolume(int output_ch) override;

    // --- Preamp & Hardware Controls ---
    bool setPhantomPower(int channel, bool enabled) override;
    bool setPad(int channel, bool enabled) override;
    bool setInstrument(int channel, bool enabled) override;

    // --- Data Sync ---
    bool getMeterLevels(MeterData& out_meters) override;
    
    // --- Lifecycle ---
    void update() override;
    bool initialize() override;
    void shutdown() override;

private:
    std::unique_ptr<AlsaCore> alsa;
    bool connected = false;
    std::string status_msg = "Disconnected";
    std::string device_name = "";

    void pollHardware();
};

} // namespace TotalMixer
