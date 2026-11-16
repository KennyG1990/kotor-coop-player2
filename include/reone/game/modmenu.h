/* Copyright (c) 2026 The reone project contributors */

#pragma once

#include <deque>
#include <string>
#include <vector>

namespace reone {

namespace game {

/**
 * A settings screen an outside program (a companion program) puts into the
 * in-game Options: the label of its Options button, a title, a block of text
 * and up to five rows to click. Nothing shows until a program sets one, and
 * nothing changes in the game when a row is clicked: the click is only kept
 * for the program to read (modmenu state), which then sets the screen again.
 */
struct ModMenuSpec {
    std::string button; // the Options button's label
    std::string title;
    std::string text;
    std::vector<std::string> rows; // top to bottom, at most ModMenuModel::kRows
    std::string back {"Close"};
};

class ModMenuModel {
public:
    static constexpr size_t kRows = 5;
    static constexpr size_t kPicksKept = 32;

    /// From JSON {"button","title","text","rows":[...],"back"}. Throws
    /// std::invalid_argument with the reason when it is not one.
    static ModMenuSpec parse(const std::string &json);

    void set(ModMenuSpec spec);
    void clear();
    bool active() const { return _active; }
    const ModMenuSpec &spec() const { return _spec; }
    /// Goes up each time the screen is set, so that an open screen redraws.
    unsigned version() const { return _version; }

    /// A row was clicked (0 is the top one). Kept until taken; the oldest go
    /// once kPicksKept are waiting.
    void pick(size_t row);
    std::vector<int> takePicks();

    void noteOpened() { ++_opens; }
    unsigned opens() const { return _opens; }

private:
    ModMenuSpec _spec;
    bool _active {false};
    unsigned _version {0};
    unsigned _opens {0};
    std::deque<int> _picks;
};

} // namespace game

} // namespace reone
