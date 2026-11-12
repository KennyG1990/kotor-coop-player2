/* Copyright (c) 2026 The reone project contributors */

#include <gtest/gtest.h>

#include "reone/game/partyseats.h"

using namespace reone::game;

namespace {

PartyPortraitSlot member(uint32_t id) {
    return PartyPortraitSlot {id, false};
}

PartyPortraitSlot seat(uint32_t id) {
    return PartyPortraitSlot {id, true};
}

PartyPortraitSlot empty() {
    return PartyPortraitSlot {};
}

} // namespace

// Without seats the portraits are the party, as they always were.
TEST(PartySeats, portraits_are_the_party_when_nobody_is_seated) {
    auto slots = partyPortraitSlots({10, 11, 12}, {});
    EXPECT_EQ(member(10), slots[0]);
    EXPECT_EQ(member(11), slots[1]);
    EXPECT_EQ(member(12), slots[2]);

    slots = partyPortraitSlots({10}, {});
    EXPECT_EQ(member(10), slots[0]);
    EXPECT_EQ(empty(), slots[1]);
    EXPECT_EQ(empty(), slots[2]);
}

TEST(PartySeats, a_seated_guest_takes_the_first_free_portrait) {
    auto slots = partyPortraitSlots({10}, {50});
    EXPECT_EQ(member(10), slots[0]);
    EXPECT_EQ(seat(50), slots[1]);
    EXPECT_EQ(empty(), slots[2]);

    slots = partyPortraitSlots({10, 11}, {50});
    EXPECT_EQ(member(11), slots[1]);
    EXPECT_EQ(seat(50), slots[2]);
}

TEST(PartySeats, two_seated_guests_fill_both_companion_portraits_in_order) {
    auto slots = partyPortraitSlots({10}, {50, 51});
    EXPECT_EQ(seat(50), slots[1]);
    EXPECT_EQ(seat(51), slots[2]);
}

// The story put two companions in the party: it keeps both portraits and the
// guest is left to the list under the minimap.
TEST(PartySeats, a_full_party_leaves_no_portrait_for_a_guest) {
    auto slots = partyPortraitSlots({10, 11, 12}, {50});
    EXPECT_EQ(member(10), slots[0]);
    EXPECT_EQ(member(11), slots[1]);
    EXPECT_EQ(member(12), slots[2]);
}

TEST(PartySeats, a_guest_never_takes_the_leaders_portrait) {
    auto slots = partyPortraitSlots({}, {50});
    EXPECT_EQ(empty(), slots[0]);
    EXPECT_EQ(seat(50), slots[1]);
}

TEST(PartySeats, a_guest_away_or_listed_twice_is_shown_once) {
    auto slots = partyPortraitSlots({10}, {0, 50, 50});
    EXPECT_EQ(seat(50), slots[1]);
    EXPECT_EQ(empty(), slots[2]);

    // A creature that is already a party member is not also a seat.
    slots = partyPortraitSlots({10, 50}, {50});
    EXPECT_EQ(member(50), slots[1]);
    EXPECT_EQ(empty(), slots[2]);
}

TEST(PartySeats, more_members_than_portraits_are_cut_off_not_overrun) {
    auto slots = partyPortraitSlots({10, 11, 12, 13}, {50});
    EXPECT_EQ(member(12), slots[2]);
}

TEST(PartySeats, each_seat_takes_one_companion_place_on_the_selection_screen) {
    EXPECT_EQ(2, partySelectionPlaces(2, 0, 0));
    EXPECT_EQ(1, partySelectionPlaces(2, 1, 0));
    EXPECT_EQ(0, partySelectionPlaces(2, 2, 0));
    EXPECT_EQ(0, partySelectionPlaces(2, 5, 0));
}

// A companion the story forces into the party always has a place.
TEST(PartySeats, forced_companions_come_before_seats) {
    EXPECT_EQ(1, partySelectionPlaces(2, 1, 1));
    EXPECT_EQ(1, partySelectionPlaces(2, 2, 1));
    EXPECT_EQ(2, partySelectionPlaces(2, 1, 2));
    EXPECT_EQ(2, partySelectionPlaces(2, 2, 2));
}

TEST(PartySeats, odd_inputs_never_give_a_negative_count) {
    EXPECT_EQ(2, partySelectionPlaces(2, -3, -1));
    EXPECT_EQ(0, partySelectionPlaces(-1, 1, 0));
    EXPECT_EQ(2, partySelectionPlaces(2, 1, 9));
}

TEST(PartySeats, the_option_is_kept_between_zero_and_two) {
    EXPECT_EQ(0, clampPartySeats(-1));
    EXPECT_EQ(0, clampPartySeats(0));
    EXPECT_EQ(1, clampPartySeats(1));
    EXPECT_EQ(2, clampPartySeats(2));
    EXPECT_EQ(2, clampPartySeats(7));
}
