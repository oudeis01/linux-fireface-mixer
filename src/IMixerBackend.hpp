#pragma once

#include <vector>
#include <string>
#include <optional>

namespace TotalMixer {

// Channel Constants for Fireface 400
constexpr int NUM_INPUTS = 18;
constexpr int NUM_PLAYBACKS = 18;
constexpr int NUM_OUTPUTS = 18;

// Input Source Indices for Matrix:
// 0  - 17: Physical Inputs
// 18 - 35: Software Playbacks

struct MeterData {
    std::vector<float> input_levels;    // [18] (Linear or dB? let's stick to hardware raw value or normalized float 0.0-1.0)
    std::vector<float> playback_levels; // [18]
    std::vector<float> output_levels;   // [18]
    
    MeterData() {
        input_levels.resize(NUM_INPUTS, 0.0f);
        playback_levels.resize(NUM_PLAYBACKS, 0.0f);
        output_levels.resize(NUM_OUTPUTS, 0.0f);
    }
};

class IMixerBackend {
public:
    virtual ~IMixerBackend() = default;

    // --- Connection Status ---
    virtual bool isConnected() const = 0;
    virtual std::string getDeviceName() const = 0;
    virtual std::string getStatusMessage() const = 0;
    
    // Returns true if this backend controls local hardware (requires system service)
    virtual bool isLocal() const = 0;

    // --- Matrix Mixing ---
    // Sets the gain for a specific routing point.
    // output_ch: 0-17
    // source_ch: 0-35 (0-17: Inputs, 18-35: Playbacks)
    // gain_db: Gain in decibels (e.g. 0.0, -6.0, -inf)
    virtual bool setMatrixGain(int output_ch, int source_ch, float gain_db) = 0;

    // --- Output Faders (Main Output Volume) ---
    // Controls the final output volume of a physical output channel
    virtual bool setOutputVolume(int output_ch, float gain_db) = 0;

    // --- Getters ---
    virtual float getMatrixGain(int output_ch, int source_ch) = 0;
    virtual float getOutputVolume(int output_ch) = 0;

    // --- Preamp & Hardware Controls ---
    // These might not be available on all channels
    virtual bool setPhantomPower(int channel, bool enabled) = 0;
    virtual bool setPad(int channel, bool enabled) = 0;
    virtual bool setInstrument(int channel, bool enabled) = 0;
    
    // --- Global Controls ---
    // Assuming we map one output as "Main" (Monitor)
    // virtual bool setMasterMute(bool enabled) = 0;
    // virtual bool setMasterDim(bool enabled) = 0;

    // --- Data Sync ---
    // Poll for metering data. Returns true if meters were updated.
    virtual bool getMeterLevels(MeterData& out_meters) = 0;
    
    // Main update loop (handle network events, ALSA events)
    virtual void update() = 0;
    
    // Initialize the backend
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
};

} // namespace TotalMixer
