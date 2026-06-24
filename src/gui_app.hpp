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
    float peak_norm = 0.0f;        // Peak normalized value (for hold)
    float peak_hold_time = 0.0f;   // Seconds since peak detected
    bool is_overload = false;      // OVR flag
    int overload_count = 0;        // Consecutive overload samples
};

struct MeterPreferences {
    int ovr_sample_count = 3;       // Consecutive overload samples for OVR (1-10)
    float peak_hold_seconds = 1.5f; // Peak hold duration (0.1-9.9s)
    bool rms_plus_3db = false;      // RMS +3dB correction checkbox
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
    void DrawMasterSection(float height);
    void DrawFader(const char* label, long* value, int min_v, int max_v, int ch_idx);
    bool SquareSlider(const char* label, long* value, int min_v, int max_v, const ImVec2& size);

    // Meter Methods
    void PollMeters();
    void DrawMeterBar(const char* label, const MeterLevel& meter, const ImVec2& size);

    // Data / State
    std::vector<std::string> out_labels;
    std::vector<std::string> in_labels;
    Device_Info device_info;

    // Meter State
    std::vector<MeterLevel> master_meters;   // 18 channels, indexed by master ch_idx
    MeterPreferences meter_prefs;
    std::chrono::steady_clock::time_point last_meter_poll_time;

    // Cache State
    // Matrix: map (out_idx, in_idx) -> value
    std::map<std::pair<int, int>, long> input_matrix_cache;
    std::map<std::pair<int, int>, long> playback_matrix_cache;
    
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
