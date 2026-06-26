#pragma once
#include <string>

namespace TotalMixer {

struct MeterPreferences; // forward declare

class ConfigManager {
public:
    static std::string GetConfigPath();
    static bool Load(MeterPreferences& prefs);
    static bool Save(const MeterPreferences& prefs);
};

} // namespace TotalMixer
