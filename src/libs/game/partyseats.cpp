/* Copyright (c) 2026 The reone project contributors */

#include "reone/game/partyseats.h"

#include <algorithm>

namespace reone {

namespace game {

std::array<PartyPortraitSlot, kPartyPortraitCount> partyPortraitSlots(
    const std::vector<uint32_t> &members,
    const std::vector<uint32_t> &seatedGuests) {
    std::array<PartyPortraitSlot, kPartyPortraitCount> slots;
    auto shown = [&slots](uint32_t id) {
        return std::any_of(slots.begin(), slots.end(), [id](const PartyPortraitSlot &slot) {
            return slot.creature == id;
        });
    };
    // Members keep the positions the party gives them, gaps included: the
    // HUD's portrait n is party member n.
    for (size_t i = 0; i < members.size() && i < slots.size(); ++i) {
        slots[i].creature = members[i];
    }
    for (uint32_t guest : seatedGuests) {
        if (guest == 0 || shown(guest)) {
            continue;
        }
        // The first portrait no member holds, never the leader's.
        int free = -1;
        for (int i = 1; i < kPartyPortraitCount; ++i) {
            if (slots[i].creature == 0) {
                free = i;
                break;
            }
        }
        if (free < 0) {
            break;
        }
        slots[free].creature = guest;
        slots[free].seat = true;
    }
    return slots;
}

int partySelectionPlaces(int maxFollowers, int seatsTaken, int forcedCompanions) {
    int limit = std::max(0, maxFollowers);
    int forced = std::clamp(forcedCompanions, 0, limit);
    int seats = std::clamp(seatsTaken, 0, limit - forced);
    return limit - seats;
}

int clampPartySeats(int requested) {
    return std::clamp(requested, 0, kMaxPartySeats);
}

} // namespace game

} // namespace reone
