#include "config_manager.hpp"
#include "gui_app.hpp"  // for MeterPreferences
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <cctype>

namespace TotalMixer {

// ── Minimal JSON reader/writer (no external dependency) ──
// Handles only the subset needed for preferences.json:
// { "meters": { "key": value, ... } }

static std::string trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r'))
        start++;
    size_t end = s.size();
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\n' || s[end-1] == '\r'))
        end--;
    return s.substr(start, end - start);
}

// Find the value string for a given key inside a JSON object body.
// body is the content between { } of the object.
// Returns the raw value string (quoted string or number literal) or empty.
static std::string json_get_value(const std::string& body, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++; // skip ':'
    // Read until ',' or '}' or end
    size_t end = body.size();
    int brace_depth = 0;
    for (size_t i = pos; i < body.size(); ++i) {
        char c = body[i];
        if (c == '{') brace_depth++;
        else if (c == '}') {
            if (brace_depth == 0) { end = i; break; }
            brace_depth--;
        }
        else if (c == ',' && brace_depth == 0) { end = i; break; }
    }
    std::string val = trim(body.substr(pos, end - pos));
    // Strip surrounding quotes for string values
    if (val.size() >= 2 && val[0] == '"' && val[val.size()-1] == '"') {
        val = val.substr(1, val.size() - 2);
    }
    return val;
}

// Extract the content of a named object (e.g. "meters" { ... })
static std::string json_get_object(const std::string& body, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = body.find(search);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos = body.find('{', pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    int depth = 1;
    size_t end = body.size();
    for (size_t i = start; i < body.size(); ++i) {
        if (body[i] == '{') depth++;
        else if (body[i] == '}') {
            depth--;
            if (depth == 0) { end = i; break; }
        }
    }
    return body.substr(start, end - start);
}

static bool parse_bool(const std::string& s, bool default_val) {
    std::string lower;
    for (char c : s) lower += (char)std::tolower((unsigned char)c);
    if (lower == "true") return true;
    if (lower == "false") return false;
    return default_val;
}

std::string ConfigManager::GetConfigPath() {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    std::string base;
    if (xdg && xdg[0] != '\0') {
        base = xdg;
    } else {
        const char* home = getenv("HOME");
        base = home ? std::string(home) + "/.config" : "/tmp";
    }
    return base + "/totalmix/preferences.json";
}

static bool load_impl(MeterPreferences& prefs) {
    std::string path = ConfigManager::GetConfigPath();
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    // Strip outer { } to get root body
    size_t first_brace = content.find('{');
    size_t last_brace = content.rfind('}');
    if (first_brace == std::string::npos || last_brace == std::string::npos)
        return false;
    std::string root_body = content.substr(first_brace + 1, last_brace - first_brace - 1);

    // Get "meters" object
    std::string meters_body = json_get_object(root_body, "meters");
    if (meters_body.empty()) return false;

    // Parse each field
    std::string v;
    v = json_get_value(meters_body, "ovr_sample_count");
    if (!v.empty()) prefs.ovr_sample_count = std::stoi(v);

    v = json_get_value(meters_body, "peak_hold_seconds");
    if (!v.empty()) prefs.peak_hold_seconds = std::stof(v);

    v = json_get_value(meters_body, "rms_plus_3db");
    if (!v.empty()) prefs.rms_plus_3db = parse_bool(v, prefs.rms_plus_3db);

    v = json_get_value(meters_body, "rms_tau_seconds");
    if (!v.empty()) prefs.rms_tau_seconds = std::stof(v);

    return true;
}

static bool save_impl(const MeterPreferences& prefs) {
    std::string path = ConfigManager::GetConfigPath();
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());

    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "{\n";
    f << "  \"meters\": {\n";
    f << "    \"ovr_sample_count\": " << prefs.ovr_sample_count << ",\n";
    f << "    \"peak_hold_seconds\": " << prefs.peak_hold_seconds << ",\n";
    f << "    \"rms_plus_3db\": " << (prefs.rms_plus_3db ? "true" : "false") << ",\n";
    f << "    \"rms_tau_seconds\": " << prefs.rms_tau_seconds << "\n";
    f << "  }\n";
    f << "}\n";
    return true;
}

bool ConfigManager::Load(MeterPreferences& prefs) {
    return load_impl(prefs);
}

bool ConfigManager::Save(const MeterPreferences& prefs) {
    return save_impl(prefs);
}

} // namespace TotalMixer
