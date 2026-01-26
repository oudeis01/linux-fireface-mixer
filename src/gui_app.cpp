#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "gui_app.hpp"
#include "ui_helpers.hpp"
#include <iostream>
#include <string>

namespace TotalMixer {

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

    // LastItemData.InFlags was removed in newer ImGui internal versions or is different
    // Use simple ItemHoverable
    const bool hovered = ImGui::ItemHoverable(frame_bb, id, 0); 
    bool temp_input_is_active = ImGui::TempInputIsActive(id);
    if (!temp_input_is_active) {
        // Explicitly cast to ImGuiMouseButton to resolve ambiguity if needed, or rely on context
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
                    float speed = (g.IO.KeyShift ? 10.0f : 150.0f); // Slower with Shift
                    float range = (float)(max_v - min_v);
                    float step = range / (200.0f); // 200px drag for full range
                    
                    float v_float = (float)*value;
                    v_float -= mouse_delta * step; // Drag Up increases value (delta y is negative up)
                    
                    if (v_float < (float)min_v) v_float = (float)min_v;
                    if (v_float > (float)max_v) v_float = (float)max_v;
                    *value = (long)v_float;
                    value_changed = true;
                }
            }
        }
    }

    // Render
    ImU32 frame_col = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
    ImGui::RenderNavHighlight(frame_bb, id);
    
    // Background
    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f)));
    
    // Fill Bar
    float range = (float)(max_v - min_v);
    float t = (*value - min_v) / range;
    t = ImClamp(t, 0.0f, 1.0f);
    
    float fill_h = size.y * t;
    ImRect fill_bb = frame_bb;
    fill_bb.Min.y = frame_bb.Max.y - fill_h;
    
    ImVec4 fill_color = (t > 0.9f) ? ImVec4(1.0f, 0.4f, 0.0f, 1.0f) : ImVec4(0.0f, 0.7f, 0.0f, 1.0f); // Orange if near peak
    window->DrawList->AddRectFilled(fill_bb.Min, fill_bb.Max, ImGui::GetColorU32(fill_color));
    
    // Border
    window->DrawList->AddRect(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)));
    
    // Text (dB Value)
    std::string db_str = val_to_db_str(*value);
    ImVec2 text_size = ImGui::CalcTextSize(db_str.c_str());
    ImVec2 text_pos = ImVec2(frame_bb.Min.x + (size.x - text_size.x) * 0.5f, frame_bb.Min.y + (size.y - text_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, ImGui::GetColorU32(ImVec4(0.9f, 0.9f, 0.9f, 1.0f)), db_str.c_str());

    return value_changed;
}

TotalMixerGUI::TotalMixerGUI() {
    // Initialize Labels
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

    // Initialize ALSA
    try {
        alsa = std::make_unique<AlsaCore>();
        std::cout << "GUI: Connected to " << alsa->get_card_name() << std::endl;
        PollHardware(); // Initial Sync
    } catch (const std::exception& e) {
        std::cerr << "GUI Warning: Failed to connect to ALSA: " << e.what() << std::endl;
        // We continue even if ALSA fails, UI will just be dummy
    }
}

TotalMixerGUI::~TotalMixerGUI() {
    // Unique_ptr handles cleanup
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
        // 1. Poll Master Volumes (Output Levels)
        // Control: "output-volume", index 0, returns 18 values
        auto mv = alsa->get_matrix_row("output-volume", 0, 18);
        if (mv) {
            for (size_t i = 0; i < mv->size() && i < 18; ++i) {
                // Only update if not currently being dragged? 
                // For now, raw update. ImGui interaction logic usually handles override if active.
                master_states[i].value = (*mv)[i];
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollInputMatrix() {
    if (!alsa) return;
    try {
        // Poll Matrix (Iterate all 18 Outputs)
        for (int o = 0; o < 18; ++o) {
            // Control: "mixer:analog-source-gain", index = Output Channel
            // Returns: Array of 8 values (Analog Inputs)
            auto r_ana = alsa->get_matrix_row("mixer:analog-source-gain", o, 8);
            // SPDIF (2 ch)
            auto r_spdif = alsa->get_matrix_row("mixer:spdif-source-gain", o, 2);
            // ADAT (8 ch)
            auto r_adat = alsa->get_matrix_row("mixer:adat-source-gain", o, 8);

            // Merge into input_matrix_cache
            // Input map: 0-7: Analog, 8-9: SPDIF, 10-17: ADAT
            if (r_ana) {
                for (size_t i = 0; i < r_ana->size(); ++i) input_matrix_cache[{static_cast<int>(i), o}] = (*r_ana)[i];
            }
            if (r_spdif) {
                for (size_t i = 0; i < r_spdif->size(); ++i) input_matrix_cache[{static_cast<int>(8 + i), o}] = (*r_spdif)[i];
            }
            if (r_adat) {
                for (size_t i = 0; i < r_adat->size(); ++i) input_matrix_cache[{static_cast<int>(10 + i), o}] = (*r_adat)[i];
            }
        }
    } catch (...) {}
}

void TotalMixerGUI::PollPlaybackMatrix() {
    if (!alsa) return;
    try {
        // Poll Matrix (Iterate all 18 Outputs)
        for (int o = 0; o < 18; ++o) {
            // Control: "mixer:stream-source-gain", index = Output Channel
            // Returns: Array of 18 values (Software Playback Channels)
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
    // 1. Hardware Poll (Throttled)
    double current_time = ImGui::GetTime();
    if (current_time - last_poll_time > 0.2) {
        PollHardware();
        last_poll_time = current_time;
    }

    // 2. UI Scaling Logic (Scale-to-Fit)
    ImGuiIO& io = ImGui::GetIO();
    float base_w = 1400.0f; // Design resolution
    float base_h = 950.0f;
    float scale_x = io.DisplaySize.x / base_w;
    float scale_y = io.DisplaySize.y / base_h;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;
    
    // Clamp min/max scale
    if (scale < 0.5f) scale = 0.5f;
    if (scale > 2.0f) scale = 2.0f;
    
    io.FontGlobalScale = scale;
    // Note: Style scaling needs careful handling to avoid cumulative application.
    // Instead of ScaleAllSizes, we rely on FontGlobalScale and manual size adjustments if needed.
    
    // 3. Main Window Layout
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Calculate Available Height
    float total_h = ImGui::GetContentRegionAvail().y;
    float header_h = ImGui::GetTextLineHeight() * 8.0f * scale; // Approx header height
    float master_h = 240.0f * scale; // Master section height scaled
    
    // A. Header
    DrawHeader();
    
    // B. Tab Area (Flexible Height)
    float tab_h = ImGui::GetContentRegionAvail().y - master_h;
    // Ensure minimum height
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
    
    // C. Master Section (Fixed Bottom)
    DrawMasterSection(); // This now fills remaining if logic is correct, or use fixed height child within DrawMasterSection

    ImGui::End();
}

void TotalMixerGUI::DrawHeader() {
    if (!alsa) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Hardware Disconnected");
        return;
    }

    std::string device_info = alsa->get_card_name();
    std::string device_name, device_guid;
    size_t comma_pos = device_info.find(',');
    if (comma_pos != std::string::npos) {
        device_name = device_info.substr(0, comma_pos);
        device_guid = device_info.substr(comma_pos + 1);
        size_t guid_pos = device_guid.find("GUID");
        if (guid_pos != std::string::npos) {
            device_guid = device_guid.substr(guid_pos + 4);
        }
        device_guid.erase(0, device_guid.find_first_not_of(" \t"));
        device_guid.erase(device_guid.find_last_not_of(" \t") + 1);
    } else {
        device_name = device_info;
        device_guid = "N/A";
    }

    // Hardware Info
    auto hw_info = alsa->get_hw_info();
    
    // Lock Info
    auto lock_val = alsa->get_control_value("external-source-lock", 0);
    bool locked = lock_val && lock_val->as_int() != 0; 

    // Construct Multi-line Info String
    std::stringstream ss;
    ss << "Device: " << device_name << "\n";
    ss << "GUID:   " << device_guid << "\n";
    ss << "Rate:   " << hw_info.rate_str << " Hz\n";
    ss << "Format: " << hw_info.format_str << " (" << hw_info.bits << " bit)\n";
    ss << "Lock:   " << (locked ? "Locked" : "No Lock");

    std::string info_text = ss.str();

    // Read-only Multi-line InputText
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f)); // Dark background
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)); // Light text
    
    ImGui::InputTextMultiline("##header_info", 
        const_cast<char*>(info_text.c_str()), 
        info_text.size() + 1, 
        ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5.5f), // Height for ~5 lines
        ImGuiInputTextFlags_ReadOnly
    );

    ImGui::PopStyleColor(2);
    ImGui::Separator();
}

void TotalMixerGUI::DrawControlTab() {
    // Groups from main_window.py
    struct GroupDef {
        const char* name;
        std::vector<const char*> controls;
    };
    
    static const std::vector<GroupDef> groups = {
        {"Clock Settings", {"primary-clock-source", "word-clock-single-speed", "active-clock-source"}},
        {"Input Options", {"line-input-level", "line-3/4-inst", "line-3/4-pad", "mic-1/2-powering"}},
        {"Output Levels", {"line-output-level", "headphone-output-level", "optical-output-signal"}},
        {"S/PDIF Config", {"spdif-input-interface", "spdif-output-format", "spdif-output-non-audio"}}
    };

    ImGui::BeginChild("ControlTab", ImVec2(0,0), true);
    
    // Grid Layout: 2 Columns
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

            // Handle multiple items (count > 1) like 'line-input-level'
            int count = info->count;
            std::vector<long> values = val->int_values;
            if (values.size() < (size_t)count) values.resize(count, values.empty() ? 0 : values[0]);

            for (int i = 0; i < count; ++i) {
                ImGui::PushID(c_name); ImGui::PushID(i);
                
                std::string label = std::string(c_name);
                if (count > 1) label += " (" + std::to_string(i+1) + ")";
                
                // Align labels
                ImGui::Text("%s:", label.c_str()); 
                ImGui::SameLine(200); 
                
                if (info->type == "Enum") {
                    // Combo Box
                    int current_idx = (int)values[i];
                    if (current_idx >= 0 && current_idx < (int)info->enum_items.size()) {
                        if (ImGui::BeginCombo("##combo", info->enum_items[current_idx].c_str())) {
                            for (int n = 0; n < (int)info->enum_items.size(); n++) {
                                bool is_selected = (current_idx == n);
                                if (ImGui::Selectable(info->enum_items[n].c_str(), is_selected)) {
                                    values[i] = n;
                                    alsa->set_control_value(c_name, 0, values);
                                }
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }
                } else if (info->type == "Bool") {
                    // Checkbox or Button
                    bool b_val = (values[i] != 0);
                    if (ImGui::Checkbox("##chk", &b_val)) {
                        values[i] = b_val ? 1 : 0;
                        alsa->set_control_value(c_name, 0, values);
                    }
                    ImGui::SameLine(); ImGui::Text(b_val ? "ON" : "OFF");
                } else {
                    // Int/Other
                    int i_val = (int)values[i];
                    if (ImGui::InputInt("##int", &i_val)) {
                        values[i] = i_val;
                        alsa->set_control_value(c_name, 0, values);
                    }
                }
                
                ImGui::PopID(); ImGui::PopID();
            }
        }
        ImGui::EndGroup();
        ImGui::Spacing(); ImGui::Spacing();
        
        ImGui::NextColumn(); // Simple alternation for now
    }
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

void TotalMixerGUI::DrawMatrixTab(const char* title, bool is_playback) {
    ImGui::BeginChild(title, ImVec2(0, 0), true); // Fill remaining
    
    // Table Setup: 1 Label Column + 18 Channel Columns
    if (ImGui::BeginTable("MatrixTable", 19, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollX)) {
        
        // Setup Columns
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 60.0f); // Fixed label width
        for (int i = 0; i < 18; ++i) {
            ImGui::TableSetupColumn(out_labels[i].c_str(), ImGuiTableColumnFlags_WidthFixed, 45.0f); // Fixed slider width
        }
        ImGui::TableHeadersRow();

        // Rows (Inputs)
        for (int r = 0; r < 18; ++r) {
            ImGui::TableNextRow();
            
            // Column 0: Label
            ImGui::TableSetColumnIndex(0);
            std::string row_label = is_playback ? "PB " + std::to_string(r+1) : in_labels[r];
            ImGui::Text("%s", row_label.c_str());

            // Columns 1-18: Sliders
            for (int c = 0; c < 18; ++c) {
                ImGui::TableSetColumnIndex(c + 1);
                
                std::string id = "##Mat" + std::to_string(r) + "_" + std::to_string(c);
                auto& cache = is_playback ? playback_matrix_cache : input_matrix_cache;
                long& val = cache[{c, r}]; 
                
                // USE CUSTOM SQUARE SLIDER
                if (SquareSlider(id.c_str(), &val, 0, 65536, ImVec2(40, 40))) {
                    if (alsa) {
                        std::string mixer_name = is_playback ? "mixer:stream-source-gain" : 
                                                (r < 8 ? "mixer:analog-source-gain" : 
                                                (r < 10 ? "mixer:spdif-source-gain" : "mixer:adat-source-gain"));
                        int hw_in_idx = is_playback ? r : (r < 8 ? r : (r < 10 ? r-8 : r-10));
                        alsa->set_matrix_gain(mixer_name, c, hw_in_idx, val);
                    }
                }
                
                // Context Menu
                if (ImGui::BeginPopupContextItem()) {
                    float f_val = (float)val;
                    if (ImGui::SliderFloat("Gain", &f_val, 0.0f, 65536.0f)) {
                        val = (long)f_val;
                        if (alsa) {
                            std::string mixer_name = is_playback ? "mixer:stream-source-gain" : 
                                                    (r < 8 ? "mixer:analog-source-gain" : 
                                                    (r < 10 ? "mixer:spdif-source-gain" : "mixer:adat-source-gain"));
                            int hw_in_idx = is_playback ? r : (r < 8 ? r : (r < 10 ? r-8 : r-10));
                            alsa->set_matrix_gain(mixer_name, c, hw_in_idx, val);
                        }
                    }
                    ImGui::EndPopup();
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
    
    // Calculate total width needed: 18 faders * 50px (40 width + padding)
    // Actually standard layout handles this if we just keep SameLine
    
    for (size_t i = 0; i < out_labels.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        ImGui::PushID((int)i);
        long old_val = master_states[i].value;
        DrawFader(out_labels[i].c_str(), &master_states[i].value, 0, 65536);
        if (old_val != master_states[i].value && alsa) {
             alsa->set_matrix_gain("output-volume", 0, i, master_states[i].value);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void TotalMixerGUI::DrawFader(const char* label, long* value, int min_v, int max_v) {
    ImGui::BeginGroup();
    
    // Label with wrapping if needed, but Fader width is fixed 40
    // To prevent overlap, we might need more spacing or handle long text
    // Let's truncate or push wrap width
    float fader_w = 40.0f;
    
    // Center label logic with wrapping
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + fader_w + 10.0f);
    // Actually wrapping in a small group is hard. Let's just limit text length display or allow overlap if spaced enough.
    // The issue "overlapping" means spacing isn't enough.
    // Let's reserve 60px width for each fader group
    
    float group_w = 70.0f; // Increased for better label spacing
    float offset = (group_w - fader_w) / 2.0f;
    
    // Invisible item to enforce width
    ImGui::Dummy(ImVec2(group_w, 0));
    
    // Label - Center manually
    float label_width = ImGui::CalcTextSize(label).x;
    // If label is wider than group, it will overlap neighbors. 
    // We can't easily fix that without wider spacing or abbreviation.
    // For now, center it.
    float current_x = ImGui::GetItemRectMin().x;
    float text_x = current_x + (group_w - label_width) / 2.0f;
    
    ImGui::SetCursorScreenPos(ImVec2(text_x, ImGui::GetCursorScreenPos().y));
    ImGui::Text("%s", label);
    
    // Slider centered
    ImGui::SetCursorScreenPos(ImVec2(current_x + offset, ImGui::GetCursorScreenPos().y));
    
    std::string id = "##" + std::string(label);
    float v_float = (float)*value;
    if (ImGui::VSliderFloat(id.c_str(), ImVec2(fader_w, 140), &v_float, (float)min_v, (float)max_v, "")) {
        *value = (long)v_float;
    }
    
    // DB Value
    std::string db_str = val_to_db_str(*value);
    float db_width = ImGui::CalcTextSize(db_str.c_str()).x;
    ImGui::SetCursorScreenPos(ImVec2(current_x + (group_w - db_width)/2.0f, ImGui::GetCursorScreenPos().y));
    ImGui::TextColored(ImVec4(0,1,0,1), "%s", db_str.c_str());
    
    ImGui::EndGroup();
}

} // namespace TotalMixer
