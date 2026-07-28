#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include "imgui.h" // Needed for ImVec2, ImGuiID
#include "mixer_types.hpp" // ChannelState, MeterPreferences, OscPreferences (GUI-free)
#include "mixer_engine.hpp" // MixerEngine: owns ALSA + mixer state + OSC + polling
#include "alsa_core.hpp"
#include "service_checker.hpp"
#include "bridge_process.hpp" // Optional web-remote bridge child process (GUI-managed)

namespace TotalMixer {

// Font atlas is rasterized once at this (high) pixel size; at runtime we only ever
// scale DOWN from it via io.FontGlobalScale so text stays crisp at any window size.
// gui_main.cpp rasterizes at this size; gui_app.cpp Render() computes the downscale.
inline constexpr float kBaseFontPx = 30.0f;

enum class ConnectionStatus {
    Connected,
    ServiceNotRunning,
    ServiceFailed,
    HardwareNotFound
};

struct Device_Info {
    std::string name;
    std::string guid;
    std::string id;
    std::string bus_speed;
};

// ── Meter Data Structures ──
struct MeterLevel {
    float normalized = 0.0f;       // Current level normalized to [0.0, 1.0]
    float rms_normalized = 0.0f;   // RMS level normalized to [0.0, 1.0]
    float peak_norm = 0.0f;        // Peak normalized value (for hold)
    float peak_hold_time = 0.0f;   // Seconds since peak detected
    bool is_overload = false;      // OVR flag
    int overload_count = 0;        // Consecutive overload samples
    float rms_sq_ema = 0.0f;       // EMA of squared linear amplitude (internal)
};

class TotalMixerGUI {
public:
    TotalMixerGUI();
    ~TotalMixerGUI();

    // Disable copy
    TotalMixerGUI(const TotalMixerGUI&) = delete;
    TotalMixerGUI& operator=(const TotalMixerGUI&) = delete;

    // Call this every frame inside the ImGui loop
    void Render();

private:
    // The GUI-free mixer core: owns the ALSA connection, mixer state, apply primitives,
    // hardware polling, and the OSC endpoint. All mixer edits/reads go through it.
    MixerEngine engine_;

    // GUI-side connection view (mapped from engine_.Init() / Retry results).
    ConnectionStatus connection_status;
    ServiceStatus service_status;

    // UI Draw Methods
    void DrawHeader();
    void DrawControlTab();
    void DrawMatrixTab(const char* title, bool is_playback);
    void DrawCombinedMatrixTab();
    void DrawMixerTab();
    void DrawMasterSection(float height);
    void DrawFader(const char* label, long* value, int min_v, int max_v, int ch_idx);
    void DrawSourceStrip(bool is_playback, int src_idx, float fader_h);
    bool SquareSlider(const char* label, long* value, int min_v, int max_v, const ImVec2& size);

    // Meter Methods (display-only; poll the hardware through engine_.alsa()).
    void PollMeters();
    void DrawMeterBar(const char* label, const MeterLevel& meter, const ImVec2& size);
    void DrawCompactMeterStrip(const char* label, const MeterLevel& meter, float height = 90.0f);
    void DrawInputSection(float height);
    void DrawStreamSection(float height);

    // Preferences
    void DrawPreferencesDialog();
    bool show_prefs_dialog = false;

    // Optional web-remote bridge, launched as a child process from the Web Remote section.
    // Reaped once per frame in Render() so an exited child never lingers as a zombie.
    BridgeProcess bridge_;
    bool bridge_allow_external_ = true; // default ON: the point is phone access over the LAN

    // Data / State
    std::vector<std::string> out_labels;
    std::vector<std::string> in_labels;
    Device_Info device_info;

    // Meter State
    std::vector<MeterLevel> master_meters;   // 18 channels, indexed by master ch_idx
    std::vector<MeterLevel> input_meters;    // 18 channels, hardware inputs
    std::vector<MeterLevel> stream_meters;   // 18 channels, playback streams
    std::vector<std::string> stream_labels;  // Labels for playback streams
    std::chrono::steady_clock::time_point last_meter_poll_time;

    // Safety: per-widget input throttle (GUI-only; the actual hardware write lives in the engine).
    std::map<ImGuiID, std::chrono::steady_clock::time_point> last_widget_write_time;
    bool ShouldWrite(ImGuiID id);
};

} // namespace TotalMixer
