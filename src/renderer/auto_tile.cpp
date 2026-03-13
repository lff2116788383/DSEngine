#include "auto_tile.h"

void AutoTileRule::AddRule(const Rule& rule) {
    rules_.push_back(rule);
}

int AutoTileRule::GetTileIndex(const Connection neighbors[8]) const {
    for (const auto& rule : rules_) {
        bool match = true;
        for (int i = 0; i < 8; ++i) {
            if (rule.neighbors[i] == Connection::Ignore) continue;
            
            // If rule says Connect (1), neighbor must be connected
            // If rule says None (0), neighbor must be disconnected
            if (rule.neighbors[i] != neighbors[i]) {
                match = false;
                break;
            }
        }
        if (match) return rule.tile_index;
    }
    return -1; // No matching rule found
}
