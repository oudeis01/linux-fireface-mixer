#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <iomanip>

namespace TotalMixer {

// RME Fireface 400 Volume Mapping (from Linux kernel ff-protocol-former.c)
// Two-step conversion: ALSA raw value (0-65536) → Hardware Knob (0-63) → dB
//
// Step 1: amp = (63 * (65536 - val)) / 65536
// Step 2: Lookup amp in HARDWARE_DB_TABLE
//
// CRITICAL: Must use exact formula from Rust implementation to avoid integer division errors.
// Mathematical simplification amp = 63 - (val * 63 / 65536) produces OFF-BY-ONE errors!
//
// Note: Hardware skips -55 dB and -57 dB (hardcoded in RME hardware)
// Reference: Linux kernel sound/firewire/fireface/ff-protocol-former.c lines 618-633
//            snd-firewire-ctl-services ff400.rs lines 288-299

static const float HARDWARE_DB_TABLE[64] = {
    6.0f,   // knob 0
    5.0f,   // knob 1
    4.0f,   // knob 2
    3.0f,   // knob 3
    2.0f,   // knob 4
    1.0f,   // knob 5
    0.0f,   // knob 6
    -1.0f,  // knob 7
    -2.0f,  // knob 8
    -3.0f,  // knob 9
    -4.0f,  // knob 10
    -5.0f,  // knob 11
    -6.0f,  // knob 12
    -7.0f,  // knob 13
    -8.0f,  // knob 14
    -9.0f,  // knob 15
    -10.0f, // knob 16
    -11.0f, // knob 17
    -12.0f, // knob 18
    -13.0f, // knob 19
    -14.0f, // knob 20
    -15.0f, // knob 21
    -16.0f, // knob 22
    -17.0f, // knob 23
    -18.0f, // knob 24
    -19.0f, // knob 25
    -20.0f, // knob 26
    -21.0f, // knob 27
    -22.0f, // knob 28
    -23.0f, // knob 29
    -24.0f, // knob 30
    -25.0f, // knob 31
    -26.0f, // knob 32
    -27.0f, // knob 33
    -28.0f, // knob 34
    -29.0f, // knob 35
    -30.0f, // knob 36
    -31.0f, // knob 37
    -32.0f, // knob 38
    -33.0f, // knob 39
    -34.0f, // knob 40
    -35.0f, // knob 41
    -36.0f, // knob 42
    -37.0f, // knob 43
    -38.0f, // knob 44
    -39.0f, // knob 45
    -40.0f, // knob 46
    -41.0f, // knob 47
    -42.0f, // knob 48
    -43.0f, // knob 49
    -44.0f, // knob 50
    -45.0f, // knob 51
    -46.0f, // knob 52
    -47.0f, // knob 53
    -48.0f, // knob 54
    -49.0f, // knob 55
    -50.0f, // knob 56
    -51.0f, // knob 57
    -52.0f, // knob 58
    -53.0f, // knob 59
    -54.0f, // knob 60
    -56.0f, // knob 61 (⚠️ SKIPS -55)
    -58.0f, // knob 62 (⚠️ SKIPS -57)
    -100.0f // knob 63 (mute/-inf)
};

// ALSA raw value → dB float
static float val_to_db(int val) {
    if (val <= 0) return -100.0f; // Mute
    if (val > 65536) val = 65536;
    int amp = (63 * (65536 - val)) / 65536;
    if (amp < 0) amp = 0;
    if (amp > 63) amp = 63;
    return HARDWARE_DB_TABLE[amp];
}

// dB float → ALSA raw value
static int db_to_val(float target_db) {
    if (target_db > 6.0f) target_db = 6.0f;
    if (target_db < -58.0f) return 0; // Mute
    
    int best_amp = 63;
    float min_diff = 1000.0f;
    for (int amp = 0; amp < 63; amp++) {
        float db = HARDWARE_DB_TABLE[amp];
        float diff = std::abs(db - target_db);
        if (diff < min_diff) {
            min_diff = diff;
            best_amp = amp;
        }
    }
    int val = (65536 * (63 - best_amp)) / 63;
    if (val < 0) val = 0;
    if (val > 65536) val = 65536;
    return val;
}

// Value to dB string (ALSA raw value → dB display)
static std::string val_to_db_str(int val) {
    // Handle mute/zero case
    if (val <= 0) return "-inf";
    
    // Clamp to valid range
    if (val > 65536) val = 65536;
    
    // Step 1: Convert ALSA raw value to hardware knob position (0-63)
    // CRITICAL: Use exact Rust formula to avoid integer division rounding errors
    // amp = (63 * (65536 - val)) / 65536  [CORRECT]
    // amp = 63 - (val * 63 / 65536)       [WRONG - off by 1]
    int amp = (63 * (65536 - val)) / 65536;
    
    // Clamp amp to valid range [0, 63]
    if (amp < 0) amp = 0;
    if (amp > 63) amp = 63;
    
    // Step 2: Lookup dB value in hardware table
    float db = HARDWARE_DB_TABLE[amp];
    
    // Handle mute case
    if (db <= -65.0f) return "-inf";
    
    // Format as string with sign and 2 decimal places
    std::stringstream ss;
    ss << std::showpos << std::fixed << std::setprecision(2) << db;
    return ss.str();
}

// dB string to Value (dB display → ALSA raw value)
static int db_str_to_val(const std::string& db_str) {
    try {
        std::string s = db_str;
        s.erase(std::remove(s.begin(), s.end(), '+'), s.end()); 
        if (s == "-inf") return 0;
        float target_db = std::stof(s);
        return db_to_val(target_db);
    } catch (...) {
        return 0;
    }
}

} // namespace TotalMixer
