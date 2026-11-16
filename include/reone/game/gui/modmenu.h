/* Copyright (c) 2026 The reone project contributors */

#pragma once

#include "../gui.h"
#include "../modmenu.h"

namespace reone {

namespace gui {
class Button;
class Label;
class ListBox;
} // namespace gui

namespace game {

/**
 * The screen of an outside program's settings (ModMenuModel), opened from its
 * button in the in-game Options. It has the Options page's own look: a title,
 * five buttons, the description box and Close. A click on a row is kept for
 * the program to read; the screen redraws whenever the program sets it again.
 */
class ModMenuScreen : public GameGUI {
public:
    ModMenuScreen(Game &game, ServicesView &services, ModMenuModel &model);

    void init() override;

    void open();
    void close();
    bool isOpen() const { return _open; }

    bool handle(const input::Event &event) override;
    void update(float dt) override;
    void render() override;

    /** Harness: the text the screen shows now. */
    const std::string &shownText() const { return _shownText; }

private:
    struct Controls {
        std::shared_ptr<gui::Button> buttons[ModMenuModel::kRows]; // top to bottom
        std::shared_ptr<gui::Button> BTN_BACK;
        std::shared_ptr<gui::Label> LBL_TITLE;
        std::shared_ptr<gui::ListBox> LB_DESC;
    };

    ModMenuModel &_model;
    Controls _controls;
    bool _open {false};
    unsigned _shownVersion {0};
    bool _shownAny {false};
    std::string _shownText;

    void onGUILoaded() override;
    void refresh();
};

} // namespace game

} // namespace reone
