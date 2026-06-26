#pragma once
#include <string>

namespace TotalMixer {

struct MeterPreferences; // forward declare
struct OscPreferences;   // forward declare

class ConfigManager {
public:
    static std::string GetConfigPath();
    // Load/Save persist both the meter and OSC preference blocks to preferences.json.
    // The single-file save writes both objects, so callers should pass the current value of
    // each even when only one changed (otherwise the untouched block would be dropped).
    static bool Load(MeterPreferences& meters, OscPreferences& osc);
    static bool Save(const MeterPreferences& meters, const OscPreferences& osc);
};

} // namespace TotalMixer
