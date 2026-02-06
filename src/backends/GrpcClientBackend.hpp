#pragma once

#include "IMixerBackend.hpp"
#include "fireface.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>

namespace TotalMixer {

class GrpcClientBackend : public IMixerBackend {
public:
    explicit GrpcClientBackend(const std::string& target_address);
    virtual ~GrpcClientBackend();

    // --- IMixerBackend Implementation ---
    bool isConnected() const override;
    std::string getDeviceName() const override;
    std::string getStatusMessage() const override;
    bool isLocal() const override { return false; }

    bool setMatrixGain(int output_ch, int source_ch, float gain_db) override;
    bool setOutputVolume(int output_ch, float gain_db) override;

    // Preamp controls (not implemented in protocol yet)
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
    std::string target_address;
    std::unique_ptr<fireface::MixerService::Stub> stub;
    bool connected = false;
    std::string status_msg = "Disconnected";

    // Local cache for GUI rendering
    // Matrix: [Source][Output] or [Output][Source]? 
    // Protocol sends flat array. We need to match GetFullState implementation.
    // Server impl: src * 18 + out
    // Size: 36 * 18 = 648
    std::vector<float> matrix_cache; 
    std::vector<float> output_cache;

    void syncFullState();
};

}
