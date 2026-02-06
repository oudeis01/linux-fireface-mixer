#include "AlsaBackend.hpp"
#include "ui_helpers.hpp"
#include <iostream>

namespace TotalMixer {

AlsaBackend::AlsaBackend() {}
AlsaBackend::~AlsaBackend() { shutdown(); }

bool AlsaBackend::initialize() {
    try {
        alsa = std::make_unique<AlsaCore>();
        connected = true;
        device_name = alsa->get_card_name();
        status_msg = "Connected to " + device_name;
        return true;
    } catch (const std::exception& e) {
        connected = false;
        status_msg = std::string("ALSA Connection Failed: ") + e.what();
        return false;
    }
}

void AlsaBackend::shutdown() {
    alsa.reset();
    connected = false;
    status_msg = "Disconnected";
}

bool AlsaBackend::isConnected() const { return connected; }
std::string AlsaBackend::getDeviceName() const { return device_name; }
std::string AlsaBackend::getStatusMessage() const { return status_msg; }

bool AlsaBackend::setMatrixGain(int output_ch, int source_ch, float gain_db) {
    if (!connected || !alsa) return false;
    
    long raw_val = db_to_val(gain_db);
    
    // Determine ALSA control name based on source channel (Fireface 400 Mapping)
    // 0-17: Inputs (Analog 1-8, SPDIF L/R, ADAT 1-8)
    // 18-35: Playbacks (Analog 1-8, SPDIF L/R, ADAT 1-8)
    
    std::string ctl_name;
    int alsa_in_idx = 0;
    
    if (source_ch < 18) {
        // Inputs
        if (source_ch < 8) {
            ctl_name = "mixer:analog-source-gain";
            alsa_in_idx = source_ch; // 0-7
        } else if (source_ch < 10) {
            ctl_name = "mixer:spdif-source-gain";
            alsa_in_idx = source_ch - 8; // 0-1
        } else {
            ctl_name = "mixer:adat-source-gain";
            alsa_in_idx = source_ch - 10; // 0-7
        }
    } else {
        // Playbacks
        int pb_ch = source_ch - 18;
        if (pb_ch < 8) {
            ctl_name = "mixer:analog-playback-gain";
            alsa_in_idx = pb_ch;
        } else if (pb_ch < 10) {
            ctl_name = "mixer:spdif-playback-gain";
            alsa_in_idx = pb_ch - 8;
        } else {
            ctl_name = "mixer:adat-playback-gain";
            alsa_in_idx = pb_ch - 10;
        }
    }
    
    return alsa->set_matrix_gain(ctl_name, output_ch, alsa_in_idx, raw_val);
}

bool AlsaBackend::setOutputVolume(int output_ch, float gain_db) {
    if (!connected || !alsa) return false;
    long raw_val = db_to_val(gain_db);
    // "output-volume" control index maps directly to output channel
    return alsa->set_control_value("output-volume", output_ch, raw_val);
}

float AlsaBackend::getMatrixGain(int output_ch, int source_ch) {
    if (!connected || !alsa) return -100.0f;

    std::string ctl_name;
    int alsa_in_idx = 0;
    
    if (source_ch < 18) {
        if (source_ch < 8) {
            ctl_name = "mixer:analog-source-gain";
            alsa_in_idx = source_ch; 
        } else if (source_ch < 10) {
            ctl_name = "mixer:spdif-source-gain";
            alsa_in_idx = source_ch - 8; 
        } else {
            ctl_name = "mixer:adat-source-gain";
            alsa_in_idx = source_ch - 10; 
        }
    } else {
        int pb_ch = source_ch - 18;
        if (pb_ch < 8) {
            ctl_name = "mixer:analog-playback-gain";
            alsa_in_idx = pb_ch;
        } else if (pb_ch < 10) {
            ctl_name = "mixer:spdif-playback-gain";
            alsa_in_idx = pb_ch - 8;
        } else {
            ctl_name = "mixer:adat-playback-gain";
            alsa_in_idx = pb_ch - 10;
        }
    }

    auto row = alsa->get_matrix_row(ctl_name, alsa_in_idx, 18);
    if (row && output_ch < (int)row->size()) {
        return val_to_db((*row)[output_ch]);
    }
    return -100.0f;
}

float AlsaBackend::getOutputVolume(int output_ch) {
    if (!connected || !alsa) return -100.0f;
    auto val = alsa->get_control_value("output-volume", output_ch);
    if (val) {
        return val_to_db(val->as_int());
    }
    return -100.0f;
}

// TODO: Implement Preamp controls once control names are verified
bool AlsaBackend::setPhantomPower(int channel, bool enabled) { return false; }
bool AlsaBackend::setPad(int channel, bool enabled) { return false; }
bool AlsaBackend::setInstrument(int channel, bool enabled) { return false; }

bool AlsaBackend::getMeterLevels(MeterData& out_meters) {
    // TODO: Implement meter polling via ALSA if available
    return false;
}

void AlsaBackend::update() {
    // Perform periodic tasks or polling if needed
}

void AlsaBackend::pollHardware() {
    // Internal polling implementation
}

} // namespace TotalMixer
