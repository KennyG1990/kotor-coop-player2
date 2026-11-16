/* Copyright (c) 2026 The reone project contributors */

#include "reone/game/gui/modmenu.h"

#include <algorithm>

#include "reone/game/game.h"
#include "reone/gui/control/button.h"
#include "reone/gui/control/label.h"
#include "reone/gui/control/listbox.h"
#include "reone/gui/gui.h"
#include "reone/gui/guis.h"
#include "reone/resource/exception/notfound.h"

using namespace reone::gui;

namespace reone {

namespace game {

ModMenuScreen::ModMenuScreen(Game &game, ServicesView &services, ModMenuModel &model) :
    GameGUI(game, services),
    _model(model) {
    _resRef = guiResRef("optionsmain");
}

void ModMenuScreen::init() {
    // A private copy, as the Co-op page takes: the Options screen shares the
    // cached optionsmain, and this screen relabels and rewires its buttons.
    _gui = _presentation.guis.getPrivateCopy(_resRef, [this](IGUI &gui) { preload(gui); });
    if (!_gui) {
        throw resource::ResourceNotFoundException("GUI not found: " + _resRef);
    }
    _gui->setEventListener(*this);
    onGUILoaded();
}

void ModMenuScreen::onGUILoaded() {
    loadBackground(BackgroundType::Menu);
    centerRootInCanvas(isTSL() ? 800 : 640, isTSL() ? 600 : 480);

    std::vector<std::shared_ptr<Button>> stack;
    for (const char *tag : {"BTN_GAMEPLAY", "BTN_FEEDBACK", "BTN_AUTOPAUSE", "BTN_GRAPHICS", "BTN_SOUND"}) {
        if (auto button = findControl<Button>(tag)) {
            stack.push_back(button);
        }
    }
    std::sort(stack.begin(), stack.end(), [](const auto &a, const auto &b) { return a->extent().top < b->extent().top; });
    for (size_t i = 0; i < stack.size() && i < ModMenuModel::kRows; ++i) {
        _controls.buttons[i] = stack[i];
        _controls.buttons[i]->setOnClick([this, i]() {
            _model.pick(i);
        });
    }
    _controls.BTN_BACK = findControl<Button>("BTN_BACK");
    _controls.LBL_TITLE = findControl<Label>("LBL_TITLE");
    _controls.LB_DESC = findControl<ListBox>("LB_DESC");
    if (_controls.BTN_BACK) {
        _controls.BTN_BACK->setOnClick([this]() { close(); });
    }
    if (_controls.LB_DESC) {
        _controls.LB_DESC->setProtoMatchContent(true);
        if (auto proto = _controls.LB_DESC->protoItemOrNull()) {
            proto->setTextPaddingLeft(8);
        }
    }
}

void ModMenuScreen::open() {
    _open = true;
    _shownAny = false;
    _model.noteOpened();
    refresh();
}

void ModMenuScreen::close() {
    _open = false;
}

void ModMenuScreen::refresh() {
    if (_shownAny && _shownVersion == _model.version()) {
        return;
    }
    _shownAny = true;
    _shownVersion = _model.version();
    const auto &spec = _model.spec();
    if (_controls.LBL_TITLE) {
        _controls.LBL_TITLE->setTextMessage(spec.title);
    }
    for (size_t i = 0; i < ModMenuModel::kRows; ++i) {
        auto &button = _controls.buttons[i];
        if (!button) {
            continue;
        }
        bool shown = i < spec.rows.size();
        button->setVisible(shown);
        button->setDisabled(!shown);
        if (shown) {
            button->setTextMessage(spec.rows[i]);
        }
    }
    if (_controls.BTN_BACK) {
        _controls.BTN_BACK->setTextMessage(spec.back);
    }
    _shownText = spec.text;
    if (_controls.LB_DESC) {
        _controls.LB_DESC->clearItems();
        _controls.LB_DESC->addTextLinesAsItems(spec.text);
    }
}

bool ModMenuScreen::handle(const input::Event &event) {
    if (!_open) {
        return false;
    }
    if (event.type == input::EventType::KeyDown && event.key.code == input::KeyCode::Escape) {
        close();
        return true;
    }
    GameGUI::handle(event);
    return true; // the screen is modal while open
}

void ModMenuScreen::update(float dt) {
    if (!_open) {
        return;
    }
    if (!_model.active()) {
        close(); // the program took its screen away
        return;
    }
    refresh();
    GameGUI::update(dt);
}

void ModMenuScreen::render() {
    if (!_open) {
        return;
    }
    GameGUI::render();
}

} // namespace game

} // namespace reone
