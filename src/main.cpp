#include "alsa_core.hpp"
#include <iostream>

int main(int /*argc*/, char** /*argv*/) {
    try {
        std::cout << "Initializing AlsaCore..." << std::endl;
        // Try to find Fireface or generic card
        // Pass -1 to search, or specific index if needed.
        // For testing, we let it fail if no Fireface is found, or we can look for *any* card.
        // But the code logic defaults to searching Fireface if no index is given.
        TotalMixer::AlsaCore alsa; 
        
        std::cout << "Card Name: " << alsa.get_card_name() << std::endl;
        
        auto controls = alsa.list_all_controls();
        std::cout << "Found " << controls.size() << " controls." << std::endl;
        
        for (const auto& [name, idx] : controls) {
            std::cout << "Control: " << name << " (Idx: " << idx << ")" << std::endl;
            auto info = alsa.get_control_info(name, idx);
            if (info) {
                std::cout << "  Type: " << info->type << ", Min: " << info->min << ", Max: " << info->max << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
