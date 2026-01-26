#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <alsa/asoundlib.h>

namespace TotalMixer {

struct ControlInfo {
    std::string type; // "Bool", "Int", "Enum", "Other"
    long min = 0;
    long max = 0;
    std::vector<std::string> enum_items;
    unsigned int count = 0;
};

// Helper variant-like structure to hold return values
struct ControlValue {
    std::vector<long> int_values;
    std::string enum_string; 
    bool is_enum = false;
    
    // Helpers
    long as_int() const { return int_values.empty() ? 0 : int_values[0]; }
    const std::vector<long>& as_array() const { return int_values; }
    std::string as_string() const { return enum_string; }
};

class AlsaCore {
public:
    // If card_index is -1 (or unspecified), it attempts to find "Fireface" automatically.
    explicit AlsaCore(int card_index = -1);
    ~AlsaCore();

    // Disable copy/move to keep resource management simple for this port
    AlsaCore(const AlsaCore&) = delete;
    AlsaCore& operator=(const AlsaCore&) = delete;

    std::string get_card_name();
    
    // Returns list of (name, index)
    std::vector<std::pair<std::string, unsigned int>> list_all_controls();

    std::optional<ControlInfo> get_control_info(const std::string& name, unsigned int index = 0);
    std::optional<ControlValue> get_control_value(const std::string& name, unsigned int index = 0);

    bool set_control_value(const std::string& name, unsigned int index, long value);
    bool set_control_value(const std::string& name, unsigned int index, const std::string& enum_value);
    bool set_control_value(const std::string& name, unsigned int index, const std::vector<long>& values);

    // Matrix helper
    std::optional<std::vector<long>> get_matrix_row(const std::string& name, unsigned int index, unsigned int count = 18);
    bool set_matrix_gain(const std::string& name, unsigned int out_idx, unsigned int in_idx, long val);

    // Hardware Info Helper
    struct HwInfo {
        std::string rate_str;
        std::string format_str;
        int bits;
    };
    HwInfo get_hw_info();

private:
    snd_ctl_t* handle = nullptr;
    int card_index = -1; 
    
    // We keep these allocated to avoid malloc/free on every call (like the Python script does)
    snd_ctl_elem_id_t* id_ptr = nullptr;
    snd_ctl_elem_value_t* val_ptr = nullptr;
    snd_ctl_elem_info_t* info_ptr = nullptr;
    snd_ctl_card_info_t* card_info_ptr = nullptr;

    std::map<std::string, int> ctl_iface_cache;

    int _find_iface(const std::string& name);
    static int find_fireface_card();
    
    void allocate_resources();
    void free_resources();
};

} // namespace TotalMixer
