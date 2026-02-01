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
    if (last_write_time.find(id) == last_write_time.end()) {
        last_write_time[id] = now;
        return true;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_write_time[id]).count();
    if (elapsed > 50) { 
        last_write_time[id] = now;
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

    return value_changed;
}

TotalMixerGUI::TotalMixerGUI() 
    : connection_status(ConnectionStatus::HardwareNotFound),
      service_status(ServiceStatus::NotRunning) {
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
            for (size_t i = 0; i < mv->size() && i < 18; ++i) {
                master_states[i].value = (*mv)[i];
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollInputMatrix() {
    if (!alsa) return;
    try {
        for (int o = 0; o < 18; ++o) {
            auto r_ana = alsa->get_matrix_row("mixer:analog-source-gain", o, 8);
            auto r_spdif = alsa->get_matrix_row("mixer:spdif-source-gain", o, 2);
            auto r_adat = alsa->get_matrix_row("mixer:adat-source-gain", o, 8);

            if (r_ana) { for (size_t i = 0; i < r_ana->size(); ++i) input_matrix_cache[{static_cast<int>(i), o}] = (*r_ana)[i]; }
            if (r_spdif) { for (size_t i = 0; i < r_spdif->size(); ++i) input_matrix_cache[{static_cast<int>(8 + i), o}] = (*r_spdif)[i]; }
            if (r_adat) { for (size_t i = 0; i < r_adat->size(); ++i) input_matrix_cache[{static_cast<int>(10 + i), o}] = (*r_adat)[i]; }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollPlaybackMatrix() {
    if (!alsa) return;
    try {
        for (int o = 0; o < 18; ++o) {
            auto r_pb = alsa->get_matrix_row("mixer:stream-source-gain", o, 18);
            if (r_pb) {
                for (size_t i = 0; i < r_pb->size(); ++i) {
                    playback_matrix_cache[{static_cast<int>(i), o}] = (*r_pb)[i];
                }
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::Render() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_time).count();
    if (elapsed > 500) {
        PollHardware();
        last_poll_time = now;
    }

    ImGuiIO& io = ImGui::GetIO();
    float scale = io.DisplaySize.x / 1400.0f;
    if (scale < 0.5f) scale = 0.5f; if (scale > 2.0f) scale = 2.0f;
    io.FontGlobalScale = scale;
    
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    DrawHeader();
    
    bool ui_enabled = (connection_status == ConnectionStatus::Connected);
    
    if (!ui_enabled) {
        ImGui::BeginDisabled();
    }
    
    float master_h = 240.0f * scale;
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
    
    DrawMasterSection();
    
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
    ImGui::EndChild();
}

void TotalMixerGUI::DrawMatrixTab(const char* title, bool is_playback) {
    ImGui::BeginChild(title, ImVec2(0, 0), true);
    
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | 
                            ImGuiTableFlags_Borders | 
                            ImGuiTableFlags_ScrollX | 
                            ImGuiTableFlags_ScrollY;
    
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
                
                bool changed = SquareSlider(id.c_str(), &val, 0, 65536, ImVec2(40, 40));
                
                // SAFETY: Write only on release
                if (alsa && ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string mixer_name = is_playback ? "mixer:stream-source-gain" : 
                                            (r < 8 ? "mixer:analog-source-gain" : 
                                            (r < 10 ? "mixer:spdif-source-gain" : "mixer:adat-source-gain"));
                    int hw_in_idx = is_playback ? r : (r < 8 ? r : (r < 10 ? r-8 : r-10));
                    alsa->set_matrix_gain(mixer_name, c, hw_in_idx, val);
                    std::cout << "Write Matrix [" << c+1 << "<-" << r+1 << "]: " << val << std::endl;
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void TotalMixerGUI::DrawMasterSection() {
    ImGui::BeginChild("MasterSection", ImVec2(0, 220), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::Text("Hardware Outputs");
    ImGui::Separator();
    
    for (size_t i = 0; i < out_labels.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        ImGui::PushID((int)i);
        DrawFader(out_labels[i].c_str(), &master_states[i].value, 0, 65536, i);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void TotalMixerGUI::DrawFader(const char* label, long* value, int min_v, int max_v, int ch_idx) {
    ImGui::BeginGroup();
    
    float fader_w = 40.0f;
    float group_w = 70.0f; 
    float offset = (group_w - fader_w) / 2.0f;
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
    if (ImGui::VSliderFloat(id.c_str(), ImVec2(fader_w, 140), &v_float, (float)min_v, (float)max_v, "")) {
        *value = (long)v_float;
        fader_changed = true;
        
        if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
            int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
            if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                master_states[pair_idx].value = *value;
            }
        }
    }
    
    std::string popup_id = "FaderInput_" + std::string(label);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(popup_id.c_str());
    }
    
    if (alsa && ImGui::IsItemDeactivatedAfterEdit()) {
        alsa->set_matrix_gain("output-volume", 0, ch_idx, *value);
        std::cout << "Write Master [" << ch_idx+1 << "]: " << *value << std::endl;
        
        if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
            int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
            if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                alsa->set_matrix_gain("output-volume", 0, pair_idx, master_states[pair_idx].value);
                std::cout << "Write Master [" << pair_idx+1 << "] (linked): " << master_states[pair_idx].value << std::endl;
            }
        }
    }
    
    if (ImGui::BeginPopup(popup_id.c_str())) {
        ImGui::Text("Enter dB value:");
        ImGui::Separator();
        
        static std::string input_buffer;
        std::string input_id = "##input_" + std::string(label);
        
        if (ImGui::IsWindowAppearing()) {
            input_buffer = val_to_db_str(*value);
            if (!input_buffer.empty() && input_buffer[0] == '+') {
                input_buffer = input_buffer.substr(1);
            }
        }
        
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText(input_id.c_str(), &input_buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            int new_val = db_str_to_val(input_buffer);
            if (new_val >= min_v && new_val <= max_v) {
                *value = new_val;
                if (alsa) {
                    alsa->set_matrix_gain("output-volume", 0, ch_idx, *value);
                    std::cout << "Write Master [" << ch_idx+1 << "] from input: " << *value << std::endl;
                    
                    if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
                        int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
                        if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                            master_states[pair_idx].value = *value;
                            alsa->set_matrix_gain("output-volume", 0, pair_idx, *value);
                            std::cout << "Write Master [" << pair_idx+1 << "] from input (linked): " << *value << std::endl;
                        }
                    }
                }
            }
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::TextDisabled("Range: -inf to +6.00 dB");
        
        ImGui::EndPopup();
    }
    
    std::string db_str = val_to_db_str(*value);
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
