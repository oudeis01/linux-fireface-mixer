#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "imgui_internal.h"
#include "gui_app.hpp"
#include "ui_helpers.hpp"
#include <iostream>
#include <string>
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>

namespace TotalMixer {

bool TotalMixerGUI::ShouldWrite(ImGuiID id) {
    auto now = std::chrono::steady_clock::now();
    if (last_widget_write_time.find(id) == last_widget_write_time.end()) {
        last_widget_write_time[id] = now;
        return true;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_widget_write_time[id]).count();
    if (elapsed > 50) { 
        last_widget_write_time[id] = now;
        return true;
    }
    return false;
}

// Helper: Square Slider Implementation
bool TotalMixerGUI::SquareSlider(const char* label, long* value, int min_v, int max_v, const ImVec2& size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImRect frame_bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    const ImRect total_bb(frame_bb.Min, frame_bb.Max);

    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &frame_bb)) return false;

    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0); 
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active) {
        const bool clicked = hovered && ImGui::IsMouseClicked(0, false);
        if (clicked) {
            ImGui::SetActiveID(id, window);
            ImGui::SetFocusID(id, window);
            ImGui::FocusWindow(window);
            g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        }
    }

    bool value_changed = false;

    // Mouse Wheel Support
    if (hovered && g.IO.MouseWheel != 0.0f) {
        float wheel_delta = g.IO.MouseWheel;
        float range = (float)(max_v - min_v);
        float step = range / 63.0f; // Step by one hardware knob unit
        
        float v_float = (float)*value;
        v_float += wheel_delta * step;
        
        if (v_float < (float)min_v) v_float = (float)min_v;
        if (v_float > (float)max_v) v_float = (float)max_v;
        
        // Snap to 0 if very close to bottom to ensure Mute
        if (v_float < (range / 200.0f)) v_float = 0.0f;

        *value = (long)v_float;
        value_changed = true;
        
        // Consume the wheel event so Table scroll doesn't also process it
        g.IO.MouseWheel = 0.0f;
    }

    if (g.ActiveId == id) {
        if (g.ActiveIdSource == ImGuiInputSource_Mouse) {
            if (!g.IO.MouseDown[0]) {
                ImGui::ClearActiveID();
            } else {
                float mouse_delta = g.IO.MouseDelta.y;
                if (mouse_delta != 0.0f) {
                    float speed = (g.IO.KeyShift ? 10.0f : 150.0f);
                    float range = (float)(max_v - min_v);
                    float step = range / (200.0f); 
                    
                    float v_float = (float)*value;
                    v_float -= mouse_delta * step; 
                    
                    if (v_float < (float)min_v) v_float = (float)min_v;
                    if (v_float > (float)max_v) v_float = (float)max_v;
                    
                    // Snap to 0 if very close to bottom
                    if (v_float < (range / 200.0f)) v_float = 0.0f;

                    *value = (long)v_float;
                    value_changed = true;
                }
            }
        }
    }

    ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderNavHighlight(frame_bb, id);
    
    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f)));
    
    float range = (float)(max_v - min_v);
    float t = (*value - min_v) / range;
    t = ImClamp(t, 0.0f, 1.0f);
    
    float fill_h = size.y * t;
    ImRect fill_bb = frame_bb;
    fill_bb.Min.y = frame_bb.Max.y - fill_h;
    
    ImVec4 fill_color = (*value > 59294) ? ImVec4(1.0f, 0.4f, 0.0f, 1.0f) : ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
    window->DrawList->AddRectFilled(fill_bb.Min, fill_bb.Max, ImGui::GetColorU32(fill_color));
    
    window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));
    
    std::string db_str = val_to_db_str(*value);
    ImVec2 text_size = ImGui::CalcTextSize(db_str.c_str());
    ImVec2 text_pos = ImVec2(frame_bb.Min.x + (size.x - text_size.x) * 0.5f, frame_bb.Min.y + (size.y - text_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)), db_str.c_str());

    // Right-click Popup for Matrix Slider
    std::string popup_id = "MatPopup_" + std::string(label);
    if (hovered && ImGui::IsMouseClicked(1)) {
        ImGui::OpenPopup(popup_id.c_str());
    }

    if (ImGui::BeginPopup(popup_id.c_str())) {
        ImGui::Text("Matrix Gain: %s -> %s", label, db_str.c_str());
        ImGui::Separator();
        
        static std::string input_buffer;
        if (ImGui::IsWindowAppearing()) {
            input_buffer = val_to_db_str(*value);
            if (!input_buffer.empty() && input_buffer[0] == '+') input_buffer = input_buffer.substr(1);
        }

        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("##db_input", &input_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            *value = db_str_to_val(input_buffer);
            value_changed = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); ImGui::Text("dB");

        ImGui::Spacing();
        ImGui::TextDisabled("Presets:");
        
        float presets[] = {6.0f, 0.0f, -5.0f, -10.0f, -15.0f, -20.0f, -30.0f, -40.0f, -50.0f};
        for (int i = 0; i < 9; i++) {
            char b_lab[16];
            snprintf(b_lab, sizeof(b_lab), "%+.1f", presets[i]);
            if (ImGui::Button(b_lab, ImVec2(50, 0))) {
                *value = db_str_to_val(std::string(b_lab));
                value_changed = true;
                ImGui::CloseCurrentPopup();
            }
            if ((i + 1) % 3 != 0) ImGui::SameLine();
        }
        
        if (ImGui::Button("-inf (Mute)", ImVec2(160, 0))) {
            *value = 0;
            value_changed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    return value_changed;
}

// ── Meter Helper Functions ──
static ImVec4 GetMeterColor(float normalized_db) {
    if (normalized_db < 0.2f) {
        float t = normalized_db / 0.2f;
        return ImVec4(0.0f, 0.5f + 0.3f * t, 0.0f, 1.0f);
    } else if (normalized_db < 0.6f) {
        float t = (normalized_db - 0.2f) / 0.4f;
        return ImVec4(t, 0.8f, 0.0f, 1.0f);
    } else if (normalized_db < 0.85f) {
        float t = (normalized_db - 0.6f) / 0.25f;
        return ImVec4(1.0f, 0.8f - 0.3f * t, 0.0f, 1.0f);
    } else {
        float t = (normalized_db - 0.85f) / 0.15f;
        return ImVec4(1.0f, 0.1f, 0.1f * t, 1.0f);
    }
}

// ── DrawMeterBar: Custom vertical meter bar using ImGui DrawList API ──
void TotalMixerGUI::DrawMeterBar(const char* label, const MeterLevel& meter, const ImVec2& size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + size);
    ImGui::ItemSize(frame_bb.GetSize());
    if (!ImGui::ItemAdd(frame_bb, window->GetID(label))) return;

    // Background (dark well)
    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, IM_COL32(15, 15, 15, 255));

    // Fill bar from bottom up to current normalized level
    if (meter.normalized > 0.0f) {
        float fill_height = meter.normalized * size.y;
        ImVec2 fill_min(frame_bb.Min.x, frame_bb.Max.y - fill_height);
        ImVec2 fill_max = frame_bb.Max;

        ImU32 fill_col = ImGui::ColorConvertFloat4ToU32(GetMeterColor(meter.normalized));
        window->DrawList->AddRectFilled(fill_min, fill_max, fill_col);
    }

    // Peak hold line (thin white line)
    if (meter.peak_norm > 0.0f && meter.peak_norm > meter.normalized) {
        float peak_y = frame_bb.Max.y - meter.peak_norm * size.y;
        window->DrawList->AddLine(
            ImVec2(frame_bb.Min.x, peak_y),
            ImVec2(frame_bb.Max.x, peak_y),
            IM_COL32(255, 255, 255, 220),
            2.0f
        );
    }

    // Border
    window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, IM_COL32(60, 60, 60, 255));

    // Overload indicator (red "OVR" text with dark background for legibility)
    if (meter.is_overload) {
        const char* ovr_text = "OVR";
        ImVec2 text_sz = ImGui::CalcTextSize(ovr_text);
        float text_x = frame_bb.Min.x + (size.x - text_sz.x) * 0.5f;
        float text_y = frame_bb.Min.y + 2.0f;
        float pad = 2.0f;
        ImRect bg_bb(
            ImVec2(text_x - pad, text_y),
            ImVec2(text_x + text_sz.x + pad, text_y + text_sz.y)
        );
        window->DrawList->AddRectFilled(bg_bb.Min, bg_bb.Max, IM_COL32(0, 0, 0, 200));
        window->DrawList->AddText(ImVec2(text_x, text_y), IM_COL32(255, 50, 50, 255), ovr_text);
    }
}

TotalMixerGUI::TotalMixerGUI()
    : connection_status(ConnectionStatus::HardwareNotFound),
      service_status(ServiceStatus::NotRunning),
      last_write_time(std::chrono::steady_clock::now()),
      active_widget_id(0),
      has_active_matrix_cell(false) {
    out_labels = {
        "Line 1", "Line 2", "Line 3", "Line 4", "Line 5", "Line 6", 
        "Phones L", "Phones R", "SPDIF L", "SPDIF R", 
        "ADAT 1", "ADAT 2", "ADAT 3", "ADAT 4", "ADAT 5", "ADAT 6", "ADAT 7", "ADAT 8"
    };
    in_labels = {
        "In 1", "In 2", "In 3", "In 4", "In 5", "In 6", "In 7", "In 8", 
        "SPDIF L", "SPDIF R", 
        "ADAT 1", "ADAT 2", "ADAT 3", "ADAT 4", "ADAT 5", "ADAT 6", "ADAT 7", "ADAT 8"
    };

    master_states.resize(18);
    master_last_write_time.resize(18, std::chrono::steady_clock::now() - std::chrono::seconds(10));
    master_meters.resize(18);
    last_meter_poll_time = std::chrono::steady_clock::now();

    // Check service status first
    CheckServiceStatus();

    // Service must be running to use the GUI
    if (service_status != ServiceStatus::Running) {
        std::cerr << "GUI Error: snd-fireface-ctl.service is not running" << std::endl;
        if (service_status == ServiceStatus::Failed) {
            connection_status = ConnectionStatus::ServiceFailed;
        } else {
            connection_status = ConnectionStatus::ServiceNotRunning;
        }
        return;
    }

    // Try to connect to ALSA
    try {
        alsa = std::make_unique<AlsaCore>();
        connection_status = ConnectionStatus::Connected;
        std::cout << "GUI: Connected to " << alsa->get_card_name() << std::endl;
        PollHardware(); 
    } catch (const std::exception& e) {
        std::cerr << "GUI Warning: Failed to connect to ALSA: " << e.what() << std::endl;
        connection_status = ConnectionStatus::HardwareNotFound;
    }
}

TotalMixerGUI::~TotalMixerGUI() {}

void TotalMixerGUI::CheckServiceStatus() {
    service_status = ServiceChecker::check_systemd("snd-fireface-ctl.service");
    if (service_status == ServiceStatus::NotInstalled) {
        std::cerr << "GUI Warning: snd-fireface-ctl.service not found" << std::endl;
    }
}

void TotalMixerGUI::PollHardware() {
    if (!alsa) return;
    try {
        PollMasterVolumes();
        PollPlaybackMatrix();
        PollInputMatrix();
    } catch (...) {}
}

void TotalMixerGUI::PollMasterVolumes() {
    if (!alsa) return;
    try {
        auto mv = alsa->get_matrix_row("output-volume", 0, 18);
        if (mv) {
            auto now = std::chrono::steady_clock::now();
            for (size_t i = 0; i < mv->size() && i < 18; ++i) {
                // Skip if muted or soloed (user control in progress)
                if(master_states[i].is_muted || master_states[i].is_soloed) continue;
                // Skip updating if this specific fader was written to in the last 2000ms
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - master_last_write_time[i]).count();
                if (elapsed < 2000) continue; 
                
                master_states[i].value = (*mv)[i];
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollInputMatrix() {
    try {
        std::vector<std::string> ctl_names = {"mixer:analog-source-gain", "mixer:spdif-source-gain", "mixer:adat-source-gain"};
        std::vector<int> offsets = {0, 8, 10};
        for (size_t grp = 0; grp < ctl_names.size(); ++grp) {
            int base_in = offsets[grp];
            int count = (grp == 0) ? 8 : ((grp == 1) ? 2 : 8);
            for (int local_in = 0; local_in < count; ++local_in) {
                auto r = alsa->get_matrix_row(ctl_names[grp], local_in, 18);
                if (r) {
                    int global_in = base_in + local_in;
                    for (size_t o = 0; o < r->size(); ++o) {
                        if (has_active_matrix_cell && 
                            active_matrix_cell.first == static_cast<int>(o) && 
                            active_matrix_cell.second == global_in) {
                            continue;
                        }
                        input_matrix_cache[{static_cast<int>(o), global_in}] = (*r)[o];
                    }
                }
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollMeters() {
    if (!alsa) return;
    try {
        // ── Cache raw value ranges for each meter control (query once from ALSA) ──
        static long ao_min = 0, ao_range = 1;
        static long so_min = 0, so_range = 1;
        static long ad_min = 0, ad_range = 1;
        static bool range_initialized = false;
        if (!range_initialized) {
            // Enable hardware metering (one-shot, required for meter:* to report PCM levels)
            if (alsa->set_control_value("metering", 0, 1)) {
                std::cout << "[METER] Hardware metering enabled" << std::endl;
            } else {
                std::cerr << "[METER] Warning: failed to enable metering" << std::endl;
            }

            auto init_range = [this](const std::string& name, long& out_min, long& out_range) {
                auto info = alsa->get_control_info(name, 0);
                if (info) {
                    out_min = info->min;
                    out_range = info->max - info->min;
                    if (out_range <= 0) out_range = 1;
                    std::cout << "[METER] " << name << " raw range: "
                              << info->min << " .. " << info->max << std::endl;
                }
            };
            init_range("meter:analog-output", ao_min, ao_range);
            init_range("meter:spdif-output", so_min, so_range);
            init_range("meter:adat-output", ad_min, ad_range);
            range_initialized = true;
        }

        // Compute delta time for peak hold decay
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_meter_poll_time).count() / 1000.0f;
        if (dt < 0.001f) dt = 0.1f;

        // Helper: read one meter control, normalize all elements, update master_meters
        auto read_meter = [&, this](const std::string& name, long raw_min, long raw_range,
                              int start_idx, int count) {
            auto val = alsa->get_control_value(name, 0);
            if (!val || (int)val->int_values.size() < count) return;

            for (int i = 0; i < count; ++i) {
                int idx = start_idx + i;
                if (idx < 0 || idx >= (int)master_meters.size()) break;

                // Normalize raw value to [0, 1] linear
                float norm = (val->int_values[i] - raw_min) / (float)raw_range;
                norm = ImClamp(norm, 0.0f, 1.0f);

                // Convert linear to dBFS display scale: -60 dBFS = 0%, 0 dBFS = 100%
                static const float kMeterRangeDB = 60.0f;
                float display_norm = 0.0f;
                if (norm > 1e-10f) {
                    float db = 20.0f * log10f(norm);
                    display_norm = ImClamp((db + kMeterRangeDB) / kMeterRangeDB, 0.0f, 1.0f);
                }

                MeterLevel& m = master_meters[idx];
                // Peak hold
                if (display_norm >= m.peak_norm) {
                    m.peak_norm = display_norm;
                    m.peak_hold_time = 0.0f;
                } else {
                    m.peak_hold_time += dt;
                    if (m.peak_hold_time >= meter_prefs.peak_hold_seconds) {
                        m.peak_norm = display_norm;
                    }
                }
                // Overload detection: -6 dBFS threshold (= 0.9 in dBFS display)
                if (display_norm >= 0.9f) {
                    m.overload_count++;
                    m.is_overload = (m.overload_count >= meter_prefs.ovr_sample_count);
                } else {
                    m.overload_count = 0;
                    m.is_overload = false;
                }
                m.normalized = display_norm;
            }
        };

        // Master section: ch 0-7 = analog-out, 8-9 = spdif-out, 10-17 = adat-out
        read_meter("meter:analog-output", ao_min, ao_range, 0, 8);
        read_meter("meter:spdif-output",   so_min, so_range, 8, 2);
        read_meter("meter:adat-output",    ad_min, ad_range, 10, 8);
    } catch (...) {}
}

void TotalMixerGUI::PollPlaybackMatrix() {
    try {
        for (int o = 0; o < 18; ++o) {
            auto r_pb = alsa->get_matrix_row("mixer:stream-source-gain", o, 18);
            if (r_pb) {
                for (size_t i = 0; i < r_pb->size(); ++i) {
                    if (has_active_matrix_cell && 
                        active_matrix_cell.first == static_cast<int>(i) && 
                        active_matrix_cell.second == o) {
                        continue;
                    }
                    playback_matrix_cache[{static_cast<int>(i), o}] = (*r_pb)[i];
                }
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::Render() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_time).count();
    auto since_write = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_write_time).count();
    
    // Skip polling if:
    // 1. Any widget is currently active (being dragged)
    // 2. Less than 200ms since last write to hardware
    bool any_widget_active = (ImGui::GetActiveID() != 0);
    bool should_skip_poll = any_widget_active || (since_write < 200);
    
    if (elapsed > 500 && !should_skip_poll) {
        std::cout << "[POLL] Executing PollHardware()" << std::endl;
        PollHardware();
        last_poll_time = now;
    } else if (elapsed > 500 && should_skip_poll) {
        std::cout << "[POLL] Skipping poll - active:" << any_widget_active 
                  << " since_write:" << since_write << "ms" << std::endl;
    }

    // Meter polling at ~33ms interval (≈30Hz, every ~2 frames at 60fps)
    auto meter_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_meter_poll_time).count();
    if (meter_elapsed > 33) {
        PollMeters();
        last_meter_poll_time = now;
    }

    ImGuiIO& io = ImGui::GetIO();
    float scale = io.DisplaySize.x / 1600.0f; // Adjusted reference width for 18 channels
    if (scale < 0.5f) scale = 0.5f; if (scale > 2.0f) scale = 2.0f;
    io.FontGlobalScale = scale;
    
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    DrawHeader();
    
    bool ui_enabled = (connection_status == ConnectionStatus::Connected);
    
    if (!ui_enabled) {
        ImGui::BeginDisabled();
    }
    
    float master_h = 360.0f * scale;
    float tab_h = ImGui::GetContentRegionAvail().y - master_h;
    if (tab_h < 100.0f) tab_h = 100.0f;

    ImGui::BeginChild("TabArea", ImVec2(0, tab_h), false);
    if (ImGui::BeginTabBar("Tabs")) {
        if (ImGui::BeginTabItem("Control")) {
            DrawControlTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Input Mixer")) {
            DrawMatrixTab("InputMixer", false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Playback Mixer")) {
            DrawMatrixTab("PlaybackMixer", true);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::EndChild();
    
    DrawMasterSection(master_h);
    
    if (!ui_enabled) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

void TotalMixerGUI::DrawHeader() {
    std::string info_str;
    
    if (connection_status != ConnectionStatus::Connected) {
        ImVec4 error_color = ImVec4(1, 0, 0, 1);
        
        switch (connection_status) {
            case ConnectionStatus::ServiceNotRunning:
                info_str = "ERROR: snd-fireface-ctl.service is NOT RUNNING\n\n";
                info_str += "This application requires snd-fireface-ctl-service to be running.\n";
                info_str += "Installation guide:\n";
                info_str += "https://github.com/oudeis01/linux-fireface-mixer#installation";
                break;
            case ConnectionStatus::ServiceFailed:
                info_str = "ERROR: snd-fireface-ctl.service FAILED\n\n";
                info_str += ServiceChecker::get_help_message(service_status) + "\n\n";
                info_str += "Installation guide:\n";
                info_str += "https://github.com/oudeis01/linux-fireface-mixer#installation";
                break;
            case ConnectionStatus::HardwareNotFound:
                info_str = "ERROR: Hardware Not Found\n\n";
                info_str += "Fireface device not detected.\n";
                info_str += "Check hardware connections and ensure snd-fireface-ctl.service is running.\n\n";
                info_str += "Setup guide:\n";
                info_str += "https://github.com/oudeis01/linux-fireface-mixer#installation";
                break;
            default:
                info_str = "ERROR: Hardware Disconnected";
                break;
        }
        
        int line_count = 1;
        for (char c : info_str) {
            if (c == '\n') line_count++;
        }
        line_count++;
        
        ImGui::PushStyleColor(ImGuiCol_Text, error_color);
        ImGui::InputTextMultiline("##error_info", &info_str, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()*line_count), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        if (connection_status == ConnectionStatus::ServiceNotRunning) {
            if (ImGui::Button("Start Service")) {
                if (ServiceChecker::try_start("snd-fireface-ctl.service")) {
                    std::cout << "Service start requested, waiting for initialization..." << std::endl;
                } else {
                    std::cerr << "Failed to start service via systemd" << std::endl;
                }
            }
            ImGui::SameLine();
        } else if (connection_status == ConnectionStatus::ServiceFailed) {
            if (ImGui::Button("Restart Service")) {
                if (ServiceChecker::try_start("snd-fireface-ctl.service")) {
                    std::cout << "Service restart requested" << std::endl;
                }
            }
            ImGui::SameLine();
        }
        
        if (ImGui::Button("Retry Connection")) {
            CheckServiceStatus();
            try {
                alsa = std::make_unique<AlsaCore>();
                connection_status = ConnectionStatus::Connected;
                std::cout << "GUI: Reconnected to " << alsa->get_card_name() << std::endl;
                PollHardware();
            } catch (const std::exception& e) {
                std::cerr << "Reconnection failed: " << e.what() << std::endl;
            }
        }
        
        ImGui::Separator();
        return;
    }

    std::string hw_info = alsa->get_card_name();
    
    size_t guid_pos = hw_info.find("GUID");
    if (guid_pos != std::string::npos) {
        device_info.name = hw_info.substr(0, guid_pos - 2);
        size_t at_pos = hw_info.find("at", guid_pos);
        if (at_pos != std::string::npos) {
            device_info.guid = hw_info.substr(guid_pos + 5, at_pos - guid_pos - 6);
            size_t comma_pos = hw_info.find(",", at_pos);
            if (comma_pos != std::string::npos) {
                device_info.id = hw_info.substr(at_pos + 3, comma_pos - at_pos - 3);
                device_info.bus_speed = hw_info.substr(comma_pos + 2);
            } else {
                device_info.id = hw_info.substr(at_pos + 3);
            }
        }
    } else {
        device_info.name = hw_info;
        device_info.guid = "Unknown";
        device_info.id = "Unknown";
        device_info.bus_speed = "Unknown";
    }
    
    info_str = "Device: " + device_info.name + "\n";
    info_str += "GUID: " + device_info.guid + "\n";
    info_str += "ID: " + device_info.id + "\n";
    info_str += "Bus Speed: " + device_info.bus_speed + (
        device_info.bus_speed.find("S400") != std::string::npos ? " (400 Mbps)" : 
        device_info.bus_speed.find("FW800") != std::string::npos ? " (800 Mbps)" : 
        device_info.bus_speed.find("S1600") != std::string::npos ? " (1600 Mbps)" : 
        device_info.bus_speed.find("S3200") != std::string::npos ? " (3200 Mbps)" : 
        " (Unknown Speed)");
    
    info_str += "\n";
    
    std::string service_status_str;
    ImVec4 service_color;
    switch (service_status) {
        case ServiceStatus::Running:
            service_status_str = "Service: snd-fireface-ctl.service [RUNNING]";
            service_color = ImVec4(0, 1, 0, 1);
            break;
        case ServiceStatus::NotRunning:
            service_status_str = "Service: snd-fireface-ctl.service [NOT RUNNING]";
            service_color = ImVec4(1, 0, 0, 1);
            break;
        case ServiceStatus::Failed:
            service_status_str = "Service: snd-fireface-ctl.service [FAILED]";
            service_color = ImVec4(1, 0, 0, 1);
            break;
        case ServiceStatus::NotInstalled:
            service_status_str = "Service: snd-fireface-ctl.service [NOT INSTALLED]";
            service_color = ImVec4(1, 0, 0, 1);
            break;
    }
    
    int line_count_before = 1;
    for (char c : info_str) {
        if (c == '\n') line_count_before++;
    }
    line_count_before++;

    ImGui::InputTextMultiline("##device_info", &info_str, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight()*line_count_before), ImGuiInputTextFlags_ReadOnly);
    
    ImGui::PushStyleColor(ImGuiCol_Text, service_color);
    ImGui::Text("%s", service_status_str.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
}

void TotalMixerGUI::DrawControlTab() {
    // Groups
    struct GroupDef { const char* name; std::vector<const char*> controls; };
    static const std::vector<GroupDef> groups = {
        {"Clock Settings", {"primary-clock-source", "word-clock-single-speed", "active-clock-source"}},
        {"Input Options", {"line-input-level", "line-3/4-inst", "line-3/4-pad", "mic-1/2-powering"}},
        {"Output Levels", {"line-output-level", "headphone-output-level", "optical-output-signal"}},
        {"S/PDIF Config", {"spdif-input-interface", "spdif-output-format", "spdif-output-non-audio"}}
    };

    ImGui::BeginChild("ControlTab", ImVec2(0,0), true);
    ImGui::Columns(2, "ControlCols", false);
    
    for (const auto& grp : groups) {
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "[ %s ]", grp.name);
        ImGui::Separator();
        
        for (const char* c_name : grp.controls) {
            if (!alsa) continue;
            auto info = alsa->get_control_info(c_name, 0);
            if (!info) continue;
            auto val = alsa->get_control_value(c_name, 0);
            if (!val) continue;

            int count = info->count;
            std::vector<long> values = val->int_values;
            if (values.size() < (size_t)count) values.resize(count, values.empty() ? 0 : values[0]);

            for (int i = 0; i < count; ++i) {
                ImGui::PushID(c_name); ImGui::PushID(i);
                std::string label = std::string(c_name);
                if (count > 1) label += " (" + std::to_string(i+1) + ")";
                
                ImGui::Text("%s:", label.c_str()); ImGui::SameLine(200); 
                
                if (info->type == "Enum") {
                    int current_idx = (int)values[i];
                    if (current_idx >= 0 && current_idx < (int)info->enum_items.size()) {
                        // ImGui Combo
                        // Need const char* array for combo
                        std::vector<const char*> items;
                        for (const auto& s : info->enum_items) items.push_back(s.c_str());
                        
                        int temp_idx = current_idx;
                        if (ImGui::Combo("##combo", &temp_idx, items.data(), items.size())) {
                            values[i] = temp_idx;
                            alsa->set_control_value(c_name, 0, values);
                        }
                    }
                } else if (info->type == "Bool") {
                    bool b_val = (values[i] != 0);
                    if (ImGui::Checkbox("##chk", &b_val)) {
                        values[i] = b_val ? 1 : 0;
                        alsa->set_control_value(c_name, 0, values);
                    }
                    ImGui::SameLine(); ImGui::Text(b_val ? "ON" : "OFF");
                }
                ImGui::PopID(); ImGui::PopID();
            }
        }
        ImGui::EndGroup();
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    
    // ── Meter Preferences Section ──
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "[ Meter Settings ]");
    ImGui::Separator();
    
    ImGui::Spacing();
    
    ImGui::Text("OVR Sample Count:"); ImGui::SameLine(200);
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderInt("##ovr_cnt", &meter_prefs.ovr_sample_count, 1, 10)) {
        // Clamp happens automatically via SliderInt range
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Consecutive overload samples to trigger OVR indicator");
    }
    
    ImGui::Text("Peak Hold Time:"); ImGui::SameLine(200);
    ImGui::SetNextItemWidth(100);
    if (ImGui::SliderFloat("##peak_hold", &meter_prefs.peak_hold_seconds, 0.1f, 9.9f, "%.1fs")) {
        if (meter_prefs.peak_hold_seconds < 0.1f) meter_prefs.peak_hold_seconds = 0.1f;
        if (meter_prefs.peak_hold_seconds > 9.9f) meter_prefs.peak_hold_seconds = 9.9f;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Duration the peak indicator stays visible after signal drops");
    }
    
    ImGui::Text("RMS +3dB Correction:"); ImGui::SameLine(200);
    ImGui::Checkbox("##rms_corr", &meter_prefs.rms_plus_3db);
    ImGui::SameLine();
    ImGui::TextDisabled("?");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Shift RMS display +3dB for 0dBFS alignment with peak meter");
    }
    
    ImGui::EndChild();
}

void TotalMixerGUI::DrawMatrixTab(const char* title, bool is_playback) {
    ImGui::BeginChild(title, ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | 
                            ImGuiTableFlags_Borders;
    
    if (ImGui::BeginTable("MatrixTable", 19, flags)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        for (int i = 0; i < 18; ++i) ImGui::TableSetupColumn(out_labels[i].c_str(), ImGuiTableColumnFlags_WidthFixed, 45.0f);
        
        ImGui::TableSetupScrollFreeze(1, 1);
        
        ImGui::TableHeadersRow();

        for (int r = 0; r < 18; ++r) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            std::string row_label = is_playback ? "PB " + std::to_string(r+1) : in_labels[r];
            ImGui::Text("%s", row_label.c_str());

            for (int c = 0; c < 18; ++c) {
                ImGui::TableSetColumnIndex(c + 1);
                
                std::string id = "##Mat" + std::to_string(r) + "_" + std::to_string(c);
                auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
                long& val = cache[{c, r}]; 
                long val_before = val;
                
                bool changed = SquareSlider(id.c_str(), &val, 0, 65536, ImVec2(40, 40));
                
                if (ImGui::IsItemActive()) {
                    active_matrix_cell = {c, r};
                    has_active_matrix_cell = true;
                }
                
                if (changed && val != val_before) {
                    std::cout << "[SLIDER] Mat[" << r << "," << c << "] changed: " 
                              << val_before << " -> " << val << std::endl;
                    
                    if (alsa && ShouldWrite(ImGui::GetID(id.c_str()))) {
                        std::string mixer_name = is_playback ? "mixer:stream-source-gain" : 
                                                (r < 8 ? "mixer:analog-source-gain" : 
                                                (r < 10 ? "mixer:spdif-source-gain" : "mixer:adat-source-gain"));
                        int hw_in_idx = is_playback ? r : (r < 8 ? r : (r < 10 ? r-8 : r-10));
                        bool success = alsa->set_matrix_gain(mixer_name, hw_in_idx, c, val);
                        if (success) {
                            last_write_time = std::chrono::steady_clock::now();
                            std::cout << "Write Matrix [" << c+1 << "<-" << r+1 << "]: " << val << " SUCCESS" << std::endl;
                        } else {
                            std::cerr << "Write Matrix [" << c+1 << "<-" << r+1 << "]: " << val << " FAILED" << std::endl;
                        }
                    }
                }
                
                if (ImGui::IsItemActive()) {
                    active_matrix_cell = {c, r};
                    has_active_matrix_cell = true;
                } else if (has_active_matrix_cell && active_matrix_cell.first == c && active_matrix_cell.second == r) {
                    has_active_matrix_cell = false;
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void TotalMixerGUI::DrawMasterSection(float height) {
    ImGui::BeginChild("MasterSection", ImVec2(0, height), true, ImGuiWindowFlags_None); // No forced scroll if possible
    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "HARDWARE OUTPUTS");
    ImGui::Separator();

    ImGui::Spacing();
    for (size_t i = 0; i < out_labels.size(); ++i) {
        if (i > 0) ImGui::SameLine(0, 15.0f); // More space between fader groups
        ImGui::PushID((int)i);
        DrawFader(out_labels[i].c_str(), &master_states[i].value, 0, 65536, (int)i);
        ImGui::PopID();
    }
    ImGui::EndChild();
}


void TotalMixerGUI::DrawFader(const char* label, long* value, int min_v, int max_v, int ch_idx) {
    ImGui::BeginGroup();
    
    std::string db_str = val_to_db_str(*value);
    float fader_w = 40.0f;
    float meter_w = 13.0f;        // ~1/3 of fader width
    float gap = 2.0f;
    float group_w = fader_w + gap + meter_w + 24.0f; // ~79 — side padding included
    float offset = 12.0f;
    ImGui::Dummy(ImVec2(group_w, 0));
    
    float label_width = ImGui::CalcTextSize(label).x;
    float current_x = ImGui::GetItemRectMin().x;
    float text_x = current_x + (group_w - label_width) / 2.0f;
    
    ImGui::SetCursorScreenPos(ImVec2(text_x, ImGui::GetCursorScreenPos().y));
    ImGui::Text("%s", label);
    
    ImGui::SetCursorScreenPos(ImVec2(current_x + offset, ImGui::GetCursorScreenPos().y));
    
    std::string id = "##" + std::string(label);
    float v_float = (float)*value;
    bool fader_changed = false;
    bool force_write = false;
    
    // ── Fader + Meter side-by-side in same row, same height ──
    if (ImGui::VSliderFloat(id.c_str(), ImVec2(fader_w, 140), &v_float, (float)min_v, (float)max_v, "")) {
        *value = (long)v_float;
        // Mute Snap
        if (v_float < ((float)(max_v - min_v) / 200.0f)) *value = 0;
        
        fader_changed = true;
    }

    // Meter bar alongside the fader
    ImGui::SameLine(0, gap);
    if (ch_idx >= 0 && ch_idx < (int)master_meters.size()) {
        DrawMeterBar("##mtr", master_meters[ch_idx], ImVec2(meter_w, 140));
    }

    // Mouse Wheel Support for Master Fader (must be AFTER VSliderFloat so IsItemHovered checks the slider)
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
        float wheel_delta = ImGui::GetIO().MouseWheel;
        float range = (float)(max_v - min_v);
        float step = range / 63.0f;
        v_float += wheel_delta * step;
        if (v_float < (float)min_v) v_float = (float)min_v;
        if (v_float > (float)max_v) v_float = (float)max_v;
        
        // Mute Snap
        if (v_float < (range / 200.0f)) v_float = 0.0f;
        
        *value = (long)v_float;
        fader_changed = true;
        
        // Claim ownership to prevent parent scroll from consuming the event
        ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    }

    // Link handling
    if (fader_changed && ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
        int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
        if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
            master_states[pair_idx].value = *value;
        }
    }

    std::string popup_id = "FaderInput_" + std::string(label);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(popup_id.c_str());
    }
    
    if (ImGui::BeginPopup(popup_id.c_str())) {
        ImGui::Text("Volume: %s -> %s", label, db_str.c_str());
        ImGui::Separator();
        
        static std::string input_buffer;
        if (ImGui::IsWindowAppearing()) {
            input_buffer = val_to_db_str(*value);
            if (!input_buffer.empty() && input_buffer[0] == '+') input_buffer = input_buffer.substr(1);
        }
        
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputText("##db_input_master", &input_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            *value = db_str_to_val(input_buffer);
            fader_changed = true;
            force_write = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(); ImGui::Text("dB");
        
        ImGui::Spacing();
        ImGui::TextDisabled("Presets:");
        
        float presets[] = {6.0f, 0.0f, -5.0f, -10.0f, -15.0f, -20.0f, -30.0f, -40.0f, -50.0f};
        for (int i = 0; i < 9; i++) {
            char b_lab[16];
            snprintf(b_lab, sizeof(b_lab), "%+.1f", presets[i]);
            if (ImGui::Button(b_lab, ImVec2(50, 0))) {
                *value = db_str_to_val(std::string(b_lab));
                fader_changed = true;
                force_write = true;
                ImGui::CloseCurrentPopup();
            }
            if ((i + 1) % 3 != 0) ImGui::SameLine();
        }
        
        if (ImGui::Button("-inf (Mute)", ImVec2(160, 0))) {
            *value = 0;
            fader_changed = true;
            force_write = true;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Mute and Solo buttons (placed BEFORE write-back so they trigger ALSA write)
    if (ch_idx < (int)master_states.size()) {
        float ms_button_w = 24.0f;
        float total_ms_w = ms_button_w * 2.0f + 7.0f;
        ImGui::SetCursorScreenPos(ImVec2(current_x + (group_w - total_ms_w) / 2.0f, ImGui::GetCursorScreenPos().y));
        
        // Mute button
        {
            bool is_muted = master_states[ch_idx].is_muted;
            ImVec4 m_color = is_muted ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
            ImGui::PushStyleColor(ImGuiCol_Button, m_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(m_color.x * 1.2f, m_color.y * 1.2f, m_color.z * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(m_color.x * 1.4f, m_color.y * 1.4f, m_color.z * 1.4f, 1.0f));
            
            std::string mute_id = "M##mute_" + std::to_string(ch_idx);
            if (ImGui::Button(mute_id.c_str(), ImVec2(ms_button_w, 20))) {
                if (!is_muted) {
                    master_states[ch_idx].is_muted = true;
                    master_states[ch_idx].saved_value = *value;
                    *value = 0;
                    fader_changed = true;
                    force_write = true;
                } else {
                    master_states[ch_idx].is_muted = false;
                    *value = master_states[ch_idx].saved_value;
                    if (*value > max_v) *value = max_v;
                    if (*value < min_v) *value = min_v;
                    fader_changed = true;
                    force_write = true;
                }
            }
            ImGui::PopStyleColor(3);
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Mute %s", label);
            }
        }
        
        ImGui::SameLine();
        
        // Solo button
        {
            bool is_soloed = master_states[ch_idx].is_soloed;
            ImVec4 s_color = is_soloed ? ImVec4(0.8f, 0.8f, 0.2f, 1.0f) : ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
            ImGui::PushStyleColor(ImGuiCol_Button, s_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(s_color.x * 1.2f, s_color.y * 1.2f, s_color.z * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(s_color.x * 1.4f, s_color.y * 1.4f, s_color.z * 1.4f, 1.0f));
            
            std::string solo_id = "S##solo_" + std::to_string(ch_idx);
            if (ImGui::Button(solo_id.c_str(), ImVec2(ms_button_w, 20))) {
                master_states[ch_idx].is_soloed = !is_soloed;
                fader_changed = true;
                force_write = true;
            }
            ImGui::PopStyleColor(3);
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Solo %s", label);
            }
        }
    }
    
    // Real-time Update (Throttled by ShouldWrite, unless force_write is true)
    if (alsa && (fader_changed || ImGui::IsItemDeactivatedAfterEdit())) {
        ImGuiID widget_id = ImGui::GetID(id.c_str());
        if (force_write || ShouldWrite(widget_id)) {
            auto now = std::chrono::steady_clock::now();
            
            // Link handling
            if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
                int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
                if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                    master_states[pair_idx].value = *value;
                    master_last_write_time[pair_idx] = now;
                }
            }

            // ATOMIC WRITE: Send all 18 channels to ALSA
            std::vector<long> all_v(18);
            // Check if any channel has solo active
            bool any_solo = false;
            for(int i=0; i<18; ++i) {
                if(master_states[i].is_soloed) { any_solo = true; break; }
            }
            for(int i=0; i<18; ++i) {
                if(any_solo && !master_states[i].is_soloed) {
                    all_v[i] = 0;  // Non-soloed channels get 0 (muted)
                } else {
                    all_v[i] = master_states[i].value;
                }
            }
            
            if (alsa->set_control_value("output-volume", 0, all_v)) {
                last_write_time = now;
                master_last_write_time[ch_idx] = now;
                last_widget_write_time[widget_id] = now;
            }
        }
    }
    
    float db_width = ImGui::CalcTextSize(db_str.c_str()).x;
    ImGui::SetCursorScreenPos(ImVec2(current_x + (group_w - db_width)/2.0f, ImGui::GetCursorScreenPos().y));
    ImGui::TextColored(ImVec4(0,1,0,1), "%s", db_str.c_str());
    
    if (ch_idx < (int)master_states.size()) {
        bool is_linked = master_states[ch_idx].is_linked;
        int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
        
        ImVec4 link_color = is_linked ? ImVec4(0.2f, 0.8f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 0.6f);
        ImGui::PushStyleColor(ImGuiCol_Button, link_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(link_color.x * 1.2f, link_color.y * 1.2f, link_color.z * 1.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(link_color.x * 1.4f, link_color.y * 1.4f, link_color.z * 1.4f, 1.0f));
        
        float button_width = 50.0f;
        ImGui::SetCursorScreenPos(ImVec2(current_x + (group_w - button_width)/2.0f, ImGui::GetCursorScreenPos().y));
        
        std::string link_label = is_linked ? "==" : "||";
        std::string link_btn_id = link_label + "##link_" + std::to_string(ch_idx);
        
        if (ImGui::Button(link_btn_id.c_str(), ImVec2(button_width, 20))) {
            master_states[ch_idx].is_linked = !is_linked;
            
            if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                master_states[pair_idx].is_linked = master_states[ch_idx].is_linked;
            }
        }
        
        ImGui::PopStyleColor(3);
        
        if (ImGui::IsItemHovered()) {
            std::string tooltip = "Link with ";
            if (pair_idx >= 0 && pair_idx < (int)out_labels.size()) {
                tooltip += out_labels[pair_idx];
            }
            ImGui::SetTooltip("%s", tooltip.c_str());
        }
    }

    ImGui::EndGroup();
}

} // namespace TotalMixer
