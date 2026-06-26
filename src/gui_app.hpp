#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include "imgui.h" // Needed for ImVec2, ImGuiID
#include "alsa_core.hpp"
#include "service_checker.hpp"

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

struct ChannelState {
    long value = 0;
    bool is_linked = false;
    bool is_muted = false;
    bool is_soloed = false;
    long saved_value = 0;
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

struct MeterPreferences {
    int ovr_sample_count = 3;       // Consecutive overload samples for OVR (1-10)
    float peak_hold_seconds = 1.5f; // Peak hold duration (0.1-9.9s)
    bool rms_plus_3db = false;      // RMS +3dB correction checkbox
    float rms_tau_seconds = 0.3f;   // RMS integration time (0.05-1.0s)
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
    // Core Logic
    std::unique_ptr<AlsaCore> alsa;
    ConnectionStatus connection_status;
    ServiceStatus service_status;
    
    void PollHardware();
    void CheckServiceStatus();
    void PollMasterVolumes();
    void PollPlaybackMatrix();
    void PollInputMatrix();

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
    // Write a source->output crosspoint gain to ALSA (analog/spdif/adat or stream).
    bool WriteSourceGain(bool is_playback, int src_idx, int output, long val);

    // Submix selection: the output (0-17) whose mix the input/playback rows currently edit.
    int selected_output = 0;
    // Returns the stereo-linked partner of an output channel, or -1 if not linked.
    int OutputLinkPartner(int ch) const;
    // True if output ch is the selected submix or its linked partner.
    bool IsOutputSelected(int ch) const;

    // Meter Methods
    void PollMeters();
    void DrawMeterBar(const char* label, const MeterLevel& meter, const ImVec2& size);
    void DrawCompactMeterStrip(const char* label, const MeterLevel& meter, float height = 90.0f);
    void DrawInputSection(float height);
    void DrawStreamSection(float height);

    // Preferences
    void DrawPreferencesDialog();
    bool show_prefs_dialog = false;

    // Data / State
    std::vector<std::string> out_labels;
    std::vector<std::string> in_labels;
    Device_Info device_info;

    // Meter State
    std::vector<MeterLevel> master_meters;   // 18 channels, indexed by master ch_idx
    std::vector<MeterLevel> input_meters;    // 18 channels, hardware inputs
    std::vector<MeterLevel> stream_meters;   // 18 channels, playback streams
    std::vector<std::string> stream_labels;  // Labels for playback streams
    MeterPreferences meter_prefs;
    std::chrono::steady_clock::time_point last_meter_poll_time;

    // Cache State
    // Matrix: map (out_idx, in_idx) -> value
    std::map<std::pair<int, int>, long> input_matrix_cache;
    std::map<std::pair<int, int>, long> playback_matrix_cache;

    // Per-submix source mute: key {output, source} present == muted; value = saved gain to
    // restore on unmute. Separate from the cache because a crosspoint may legitimately be 0.
    std::map<std::pair<int, int>, long> input_mute_state;
    std::map<std::pair<int, int>, long> playback_mute_state;
    
    // Masters: index -> state
    std::vector<ChannelState> master_states;
    std::vector<std::chrono::steady_clock::time_point> master_last_write_time;

    // Safety: Throttling
    std::chrono::steady_clock::time_point last_poll_time;
    std::chrono::steady_clock::time_point last_write_time;
    std::map<ImGuiID, std::chrono::steady_clock::time_point> last_widget_write_time;
    
    // Active widget tracking (to prevent poll overwriting active slider)
    ImGuiID active_widget_id;
    std::pair<int, int> active_matrix_cell;
    bool has_active_matrix_cell;
    
    bool ShouldWrite(ImGuiID id);
};

} // namespace TotalMixer
