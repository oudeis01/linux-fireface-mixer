#define IMGUI_DEFINE_MATH_OPERATORS
#include "gui_app.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"
#include "ui_helpers.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace TotalMixer {

bool TotalMixerGUI::ShouldWrite(ImGuiID id) {
    auto now = std::chrono::steady_clock::now();
    if (last_widget_write_time.find(id) != last_widget_write_time.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_widget_write_time[id]).count();
        if (elapsed < 200) return false;
    }
    return true;
}

bool TotalMixerGUI::SquareSlider(const char* label, long* value, int min_v, int max_v, const ImVec2& size) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    const ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    ImGui::ItemSize(bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    const bool hovered = ImGui::ItemHoverable(bb, id, g.LastItemData.ItemFlags);
    bool temp_active = ImGui::TempInputIsActive(id);
    if (temp_active) {
        // If input active, don't handle slider
        return false; 
    }

    bool pressed = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left, 0, id);
    if (pressed) {
        ImGui::SetActiveID(id, window);
        ImGui::SetFocusID(id, window);
    }
    
    bool value_changed = false;
    if (g.ActiveId == id) {
        if (g.IO.MouseDown[ImGuiMouseButton_Left]) {
            float mouse_delta_y = g.IO.MouseDelta.y;
            if (mouse_delta_y != 0.0f) {
                // Determine speed based on dB range logic
                // For simplicity, linear raw mapping but scaled
                // Range is 65536. 
                // Let's use a dynamic scale based on current value?
                // Or just fixed raw steps.
                
                float speed = 300.0f; // raw units per pixel
                if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) speed = 50.0f;
                
                long change = (long)(-mouse_delta_y * speed);
                long new_val = *value + change;
                if (new_val < min_v) new_val = min_v;
                if (new_val > max_v) new_val = max_v;
                
                if (new_val != *value) {
                    *value = new_val;
                    value_changed = true;
                }
            }
        } else {
            ImGui::ClearActiveID();
        }
    }

    // Render
    float fraction = (float)(*value - min_v) / (float)(max_v - min_v);
    
    // Background
    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
    
    // Fill (Volume bar)
    // Vertical fill
    float fill_h = (bb.Max.y - bb.Min.y) * fraction;
    ImRect fill_bb(ImVec2(bb.Min.x, bb.Max.y - fill_h), bb.Max);
    window->DrawList->AddRectFilled(fill_bb.Min, fill_bb.Max, ImGui::GetColorU32(ImGuiCol_SliderGrab), style.FrameRounding);
    
    // Border
    window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding);
    
    // Text (dB value)
    char buf[32];
    std::string db_str = val_to_db_str(*value);
    snprintf(buf, 32, "%s", db_str.c_str());
    
    ImVec2 text_size = ImGui::CalcTextSize(buf);
    ImVec2 text_pos = ImVec2(bb.Min.x + (bb.GetSize().x - text_size.x) * 0.5f, bb.Max.y - text_size.y - 2);
    window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), buf);

    return value_changed;
}

TotalMixerGUI::TotalMixerGUI(std::shared_ptr<IMixerBackend> backend)
    : backend(backend),
      connection_status(ConnectionStatus::HardwareNotFound),
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

    // Try to connect backend
    if (backend->initialize()) {
        connection_status = ConnectionStatus::Connected;
        std::cout << "GUI: Connected via backend" << std::endl;
        PollHardware(); 
    } else {
        std::cerr << "GUI Warning: Backend initialization failed: " << backend->getStatusMessage() << std::endl;
        connection_status = ConnectionStatus::HardwareNotFound;
    }
}

TotalMixerGUI::~TotalMixerGUI() {}

void TotalMixerGUI::CheckServiceStatus() {
    service_status = ServiceChecker::check_systemd("snd-fireface-ctl.service");
}

void TotalMixerGUI::PollHardware() {
    if (!backend) return;
    try {
        PollMasterVolumes();
        PollPlaybackMatrix();
        PollInputMatrix();
    } catch (...) {}
}

void TotalMixerGUI::PollMasterVolumes() {
    if (!backend || !backend->isConnected()) return;
    try {
        for (int i = 0; i < 18; ++i) {
            float db = backend->getOutputVolume(i);
            master_states[i].value = db_to_val(db);
        }
    } catch (...) {}
}

void TotalMixerGUI::PollInputMatrix() {
    if (!backend || !backend->isConnected()) return;
    try {
        for (int in = 0; in < 18; ++in) {
            for (int out = 0; out < 18; ++out) {
                if (has_active_matrix_cell && 
                    active_matrix_cell.first == out && 
                    active_matrix_cell.second == in) {
                    continue;
                }
                float db = backend->getMatrixGain(out, in);
                input_matrix_cache[{out, in}] = db_to_val(db);
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollPlaybackMatrix() {
    if (!backend || !backend->isConnected()) return;
    try {
        for (int pb = 0; pb < 18; ++pb) {
            int src_idx = 18 + pb;
            for (int out = 0; out < 18; ++out) {
                if (has_active_matrix_cell && 
                    active_matrix_cell.first == out && 
                    active_matrix_cell.second == src_idx) {
                    continue;
                }
                float db = backend->getMatrixGain(out, src_idx);
                playback_matrix_cache[{out, pb}] = db_to_val(db);
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::Render() {
    // Poll every 100ms? 
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_time).count();
    auto since_write = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_write_time).count();
    
    // Don't poll if we just wrote something (wait for hardware to settle)
    bool should_skip_poll = (since_write < 200);
    
    // Also skip if any widget is active? (Already handled by active_matrix_cell)
    bool any_widget_active = ImGui::IsAnyItemActive();
    
    if (elapsed > 100 && !should_skip_poll) {
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
                break;
            case ConnectionStatus::ServiceFailed:
                info_str = "ERROR: snd-fireface-ctl.service FAILED\n\n";
                info_str += ServiceChecker::get_help_message(service_status) + "\n\n";
                break;
            case ConnectionStatus::HardwareNotFound:
                info_str = "ERROR: Hardware Not Found or Connection Failed\n\n";
                info_str += "Backend Status: " + (backend ? backend->getStatusMessage() : "NULL") + "\n";
                break;
            default:
                info_str = "ERROR: Unknown Error";
                break;
        }
        
        ImGui::PushStyleColor(ImGuiCol_Text, error_color);
        ImGui::TextWrapped("%s", info_str.c_str());
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        if (ImGui::Button("Retry Connection")) {
            CheckServiceStatus();
            if (backend->initialize()) {
                connection_status = ConnectionStatus::Connected;
                PollHardware();
            }
        }
        
        ImGui::Separator();
        return;
    }

    std::string hw_info = backend->getDeviceName();
    // Parse info if needed (GUID, etc.) - Simplified for now
    
    info_str = "Device: " + hw_info + "\n";
    info_str += "Backend: " + (backend ? backend->getStatusMessage() : "Unknown") + "\n";
    
    std::string service_status_str = "Service: snd-fireface-ctl.service [RUNNING]";
    ImVec4 service_color = ImVec4(0, 1, 0, 1);
    
    ImGui::Text("%s", info_str.c_str());
    ImGui::SameLine(400);
    ImGui::PushStyleColor(ImGuiCol_Text, service_color);
    ImGui::Text("%s", service_status_str.c_str());
    ImGui::PopStyleColor();

    ImGui::Separator();
}

void TotalMixerGUI::DrawControlTab() {
    ImGui::Text("Device Settings are disabled in this version.");
    ImGui::Text("Please use alsamixer or vendor tools for Clock/SPDIF settings.");
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
                long val_before = val;
                
                bool changed = SquareSlider(id.c_str(), &val, 0, 65536, ImVec2(40, 40));
                
                if (ImGui::IsItemActive()) {
                    active_matrix_cell = {c, r};
                    has_active_matrix_cell = true;
                }
                
                if (changed && val != val_before) {
                    // std::cout << "[SLIDER] Changed" << std::endl;
                }
                
                if (backend && ImGui::IsItemDeactivatedAfterEdit()) {
                    has_active_matrix_cell = false;
                    float db = val_to_db(val);
                    int src_ch = is_playback ? (18 + r) : r;
                    backend->setMatrixGain(c, src_ch, db);
                    last_write_time = std::chrono::steady_clock::now();
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
    
    if (ImGui::VSliderFloat(id.c_str(), ImVec2(fader_w, 140), &v_float, (float)min_v, (float)max_v, "")) {
        *value = (long)v_float;
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
    
    if (backend && ImGui::IsItemDeactivatedAfterEdit()) {
        float db = val_to_db(*value);
        backend->setOutputVolume(ch_idx, db);
        last_write_time = std::chrono::steady_clock::now();
        
        if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
            int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
            if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                float pair_db = val_to_db(master_states[pair_idx].value);
                backend->setOutputVolume(pair_idx, pair_db);
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
                if (backend) {
                    float db = val_to_db(*value);
                    backend->setOutputVolume(ch_idx, db);
                    last_write_time = std::chrono::steady_clock::now();
                    
                    if (ch_idx < (int)master_states.size() && master_states[ch_idx].is_linked) {
                        int pair_idx = (ch_idx % 2 == 0) ? ch_idx + 1 : ch_idx - 1;
                        if (pair_idx >= 0 && pair_idx < (int)master_states.size()) {
                            master_states[pair_idx].value = *value;
                            float pair_db = val_to_db(*value);
                            backend->setOutputVolume(pair_idx, pair_db);
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
