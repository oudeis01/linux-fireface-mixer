#pragma once

// GUI-free plain-data types shared between the mixer engine (headless-safe) and the
// GUI frontend. Nothing here may depend on ImGui/GLFW/OpenGL so that libmixer_engine
// links without any X11/GL toolchain. GUI-only view types (MeterLevel, Device_Info,
// ConnectionStatus) intentionally stay in gui_app.hpp.

namespace TotalMixer {

// One master output channel's mixer state (fader value + toggles).
struct ChannelState {
    long value = 0;
    bool is_linked = false;
    bool is_muted = false;
    bool is_soloed = false;
    long saved_value = 0;
};

// Meter display tuning (persisted in preferences.json under "meters"). The engine does
// not consume these, but it owns config load/save so the type must be GUI-free.
struct MeterPreferences {
    int ovr_sample_count = 3;       // Consecutive overload samples for OVR (1-10)
    float peak_hold_seconds = 1.5f; // Peak hold duration (0.1-9.9s)
    bool rms_plus_3db = false;      // RMS +3dB correction checkbox
    float rms_tau_seconds = 0.3f;   // RMS integration time (0.05-1.0s)
};

// OSC remote endpoint settings (persisted in preferences.json under "osc").
struct OscPreferences {
    bool enabled = false;   // Start the OSC server on launch
    int in_port = 7001;     // UDP port we listen on for control messages
    int out_port = 9001;    // UDP port we send state feedback to (on the client host)
};

} // namespace TotalMixer
