#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <iomanip>

namespace TotalMixer {

struct CalibrationPoint {
    int val;
    float db;
};

// 58 Point Empirical Table from Python code
static const std::vector<CalibrationPoint> CALIBRATION_POINTS = {
    {0, -100.0f}, {1040, -58.0f}, {2080, -57.0f}, {3120, -56.0f}, {4161, -55.0f},
    {5201, -54.0f}, {6241, -53.0f}, {7281, -52.0f}, {8322, -51.0f}, {9362, -50.0f},
    {10402, -49.0f}, {11442, -48.0f}, {12483, -47.0f}, {13523, -46.0f}, {14563, -45.0f},
    {15603, -44.0f}, {16644, -43.0f}, {17684, -42.0f}, {18724, -41.0f}, {19764, -40.0f},
    {20805, -39.0f}, {21845, -38.0f}, {22885, -37.0f}, {23925, -36.0f}, {24966, -35.0f},
    {26006, -34.0f}, {27046, -33.0f}, {28086, -32.0f}, {29127, -31.0f}, {30167, -30.0f},
    {31207, -29.0f}, {32247, -28.0f}, {33288, -27.0f}, {34328, -26.0f}, {35368, -25.0f},
    {36408, -24.0f}, {37449, -23.0f}, {38489, -22.0f}, {39529, -21.0f}, {40569, -20.0f},
    {41610, -19.0f}, {42650, -18.0f}, {43690, -17.0f}, {44730, -16.0f}, {45771, -15.0f},
    {46811, -14.0f}, {47851, -13.0f}, {48891, -12.0f}, {49932, -11.0f}, {50972, -10.0f},
    {52012, -9.0f}, {53052, -8.0f}, {54093, -7.0f}, {55133, -6.0f}, {56173, -5.0f},
    {57213, -4.0f}, {58254, -3.0f}, {59294, 0.0f}, {65536, 6.0f}
};

// Linear interpolation helper
static float lerp(float x, float x0, float x1, float y0, float y1) {
    if (x1 == x0) return y0;
    return y0 + (x - x0) * (y1 - y0) / (x1 - x0);
}

// Value to dB string
static std::string val_to_db_str(int val) {
    if (val <= 0) return "-inf";
    
    // Find interval
    size_t i = 0;
    while (i < CALIBRATION_POINTS.size() - 1 && CALIBRATION_POINTS[i+1].val < val) {
        i++;
    }
    
    float db = -100.0f;
    if (i < CALIBRATION_POINTS.size() - 1) {
        db = lerp((float)val, 
                  (float)CALIBRATION_POINTS[i].val, (float)CALIBRATION_POINTS[i+1].val, 
                  CALIBRATION_POINTS[i].db, CALIBRATION_POINTS[i+1].db);
    } else {
        db = CALIBRATION_POINTS.back().db;
    }

    if (db <= -65.0f) return "-inf";
    
    std::stringstream ss;
    ss << std::showpos << std::fixed << std::setprecision(2) << db;
    return ss.str();
}

// dB string to Value (approximate reverse)
static int db_str_to_val(const std::string& db_str) {
    try {
    std::string s = db_str;
    s.erase(std::remove(s.begin(), s.end(), '+'), s.end()); 
    if (s == "-inf") return 0;
        
        float db = std::stof(s);
        
        // Find interval in reverse
        size_t i = 0;
        while (i < CALIBRATION_POINTS.size() - 1 && CALIBRATION_POINTS[i+1].db < db) {
            i++;
        }
        
        float val = 0.0f;
        if (i < CALIBRATION_POINTS.size() - 1) {
            val = lerp(db, 
                       CALIBRATION_POINTS[i].db, CALIBRATION_POINTS[i+1].db, 
                       (float)CALIBRATION_POINTS[i].val, (float)CALIBRATION_POINTS[i+1].val);
        } else {
            val = (float)CALIBRATION_POINTS.back().val;
        }
        return (int)val;
    } catch (...) {
        return 0;
    }
}

} // namespace TotalMixer
