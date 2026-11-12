/* Copyright (c) 2026 The reone project contributors */

#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace reone {

namespace game {

/**
 * Party seats (fork addition; host option --coop-party-seats).
 *
 * A Free Mode guest's own character is a puppet outside the party table, so
 * scripts that count or check the party see the host's party alone. A party
 * seat keeps that, and only changes what the host is shown: the seated guest
 * takes a free party portrait on the HUD and one of the two companion places
 * on the party selection screen. The story always wins a place: when it
 * fills the party itself, the seated guest is listed under the minimap as
 * any other guest is, until a portrait frees.
 *
 * The parts here need no engine, so they can be tested alone.
 */

constexpr int kPartyPortraitCount = 3;
constexpr int kMaxPartySeats = 2;

struct PartyPortraitSlot {
    uint32_t creature {0}; // object id; 0 for an empty portrait
    bool seat {false};     // a seated guest rather than a party member

    bool operator==(const PartyPortraitSlot &other) const {
        return creature == other.creature && seat == other.seat;
    }
};

/**
 * Which creature each of the HUD's three party portraits shows: the party's
 * members in their own order (the leader first), then seated guests in the
 * portraits left over. Ids of 0 and guests already shown are skipped.
 */
std::array<PartyPortraitSlot, kPartyPortraitCount> partyPortraitSlots(
    const std::vector<uint32_t> &members,
    const std::vector<uint32_t> &seatedGuests);

/**
 * Companion places the party selection screen offers before it counts the
 * companions already in the party: the game's limit less the seats taken.
 * Companions the story forces into the party come first, so a seat never
 * takes a place one of them needs. Never negative.
 */
int partySelectionPlaces(int maxFollowers, int seatsTaken, int forcedCompanions);

/** The option's value as the session uses it: 0 to kMaxPartySeats. */
int clampPartySeats(int requested);

} // namespace game

} // namespace reone
