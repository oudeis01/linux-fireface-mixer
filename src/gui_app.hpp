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
};

struct Device_Info {
    std::string name;
    std::string guid;
    std::string id;
    std::string bus_speed;
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
    void DrawMasterSection();
    void DrawFader(const char* label, long* value, int min_v, int max_v, int ch_idx);
    bool SquareSlider(const char* label, long* value, int min_v, int max_v, const ImVec2& size);

    // Data / State
    std::vector<std::string> out_labels;
    std::vector<std::string> in_labels;
    Device_Info device_info;

    // Cache State
    // Matrix: map (out_idx, in_idx) -> value
    std::map<std::pair<int, int>, long> input_matrix_cache;
    std::map<std::pair<int, int>, long> playback_matrix_cache;
    
    // Masters: index -> state
    std::vector<ChannelState> master_states;

    // Safety: Throttling
    std::chrono::steady_clock::time_point last_poll_time;
    std::map<ImGuiID, std::chrono::steady_clock::time_point> last_write_time;
    
    bool ShouldWrite(ImGuiID id);
};

} // namespace TotalMixer
