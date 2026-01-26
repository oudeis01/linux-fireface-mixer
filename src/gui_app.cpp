#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "gui_app.hpp"
#include "ui_helpers.hpp"
#include <iostream>
#include <string>
#include <cmath>
#include <cstdio>

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
    
    float db = ((float)(*value) - 59294.0f) / 1040.25f;
    char db_buf[32];
    if (*value <= 0) snprintf(db_buf, 32, "-inf");
    else if (db < -65.0f) snprintf(db_buf, 32, "-inf");
    else snprintf(db_buf, 32, "%+.2f", db);

    ImVec2 text_size = ImGui::CalcTextSize(db_buf);
    ImVec2 text_pos = ImVec2(frame_bb.Min.x + (size.x - text_size.x) * 0.5f, frame_bb.Min.y + (size.y - text_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)), db_buf);

    return value_changed;
}

TotalMixerGUI::TotalMixerGUI() {
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

    try {
        alsa = std::make_unique<AlsaCore>();
        std::cout << "GUI: Connected to " << alsa->get_card_name() << std::endl;
        PollHardware(); 
    } catch (const std::exception& e) {
        std::cerr << "GUI Warning: Failed to connect to ALSA: " << e.what() << std::endl;
    }
}

TotalMixerGUI::~TotalMixerGUI() {}

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

    ImGui::End();
}

void TotalMixerGUI::DrawHeader() {
    if (!alsa) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Hardware Disconnected");
        return;
    }
    ImGui::TextColored(ImVec4(0,1,0,1), "%s", alsa->get_card_name().c_str());
    ImGui::SameLine(300);
    ImGui::Text("Rate: --"); 
    ImGui::Separator();
}

void TotalMixerGUI::DrawControlTab() {
    ImGui::Text("Control Tab (TODO: Port Enum/Bool controls to C++)");
}

void TotalMixerGUI::DrawMatrixTab(const char* title, bool is_playback) {
    ImGui::BeginChild(title, ImVec2(0, 0), true);
    
    if (ImGui::BeginTable("MatrixTable", 19, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        for (int i = 0; i < 18; ++i) ImGui::TableSetupColumn(out_labels[i].c_str(), ImGuiTableColumnFlags_WidthFixed, 45.0f);
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
    if (ImGui::VSliderFloat(id.c_str(), ImVec2(fader_w, 140), &v_float, (float)min_v, (float)max_v, "")) {
        *value = (long)v_float;
    }
    
    if (alsa && ImGui::IsItemDeactivatedAfterEdit()) {
        alsa->set_matrix_gain("output-volume", 0, ch_idx, *value);
        std::cout << "Write Master [" << ch_idx+1 << "]: " << *value << std::endl;
    }
    
    float db = ((float)(*value) - 59294.0f) / 1040.25f;
    char db_buf[32];
    if (*value <= 0) snprintf(db_buf, 32, "-inf");
    else if (db < -65.0f) snprintf(db_buf, 32, "-inf");
    else snprintf(db_buf, 32, "%+.2f", db);

    float db_width = ImGui::CalcTextSize(db_buf).x;
    ImGui::SetCursorScreenPos(ImVec2(current_x + (group_w - db_width)/2.0f, ImGui::GetCursorScreenPos().y));
    ImGui::TextColored(ImVec4(0,1,0,1), "%s", db_buf);
    
    ImGui::EndGroup();
}

} // namespace TotalMixer
