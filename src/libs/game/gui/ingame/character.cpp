/*
 * Copyright (c) 2020-2023 The reone project contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "reone/game/gui/ingame/character.h"

#include "reone/game/coopsession.h"
#include "reone/game/d20/classes.h"
#include "reone/game/di/services.h"
#include "reone/game/game.h"
#include "reone/game/gui/ingame.h"
#include "reone/game/party.h"
#include "reone/game/types.h"
#include "reone/graphics/di/services.h"
#include "reone/gui/sceneinitializer.h"
#include "reone/resource/provider/models.h"
#include "reone/resource/strings.h"
#include "reone/scene/di/services.h"
#include "reone/scene/graphs.h"
#include "reone/scene/node/emitter.h"
#include "reone/scene/node/light.h"
#include "reone/scene/types.h"

using namespace reone::audio;

using namespace reone::graphics;
using namespace reone::gui;
using namespace reone::resource;
using namespace reone::scene;

namespace reone {

namespace game {

void CharacterMenu::onGUILoaded() {
    loadBackground(BackgroundType::Menu);
    bindControls();

    if (_controls.LBL_GOOD1)
        _lblGood.push_back(_controls.LBL_GOOD1);
    if (_controls.LBL_GOOD2)
        _lblGood.push_back(_controls.LBL_GOOD2);
    if (_controls.LBL_GOOD3)
        _lblGood.push_back(_controls.LBL_GOOD3);
    if (_controls.LBL_GOOD4)
        _lblGood.push_back(_controls.LBL_GOOD4);
    if (_controls.LBL_GOOD5)
        _lblGood.push_back(_controls.LBL_GOOD5);
    if (_controls.LBL_GOOD6)
        _lblGood.push_back(_controls.LBL_GOOD6);
    if (_controls.LBL_GOOD7)
        _lblGood.push_back(_controls.LBL_GOOD7);
    if (_controls.LBL_GOOD8)
        _lblGood.push_back(_controls.LBL_GOOD8);
    if (_controls.LBL_GOOD9)
        _lblGood.push_back(_controls.LBL_GOOD9);
    if (_controls.LBL_GOOD10)
        _lblGood.push_back(_controls.LBL_GOOD10);

    if (_controls.LBL_BAR1)
        _lblBar.push_back(_controls.LBL_BAR1);
    if (_controls.LBL_BAR2)
        _lblBar.push_back(_controls.LBL_BAR2);
    if (_controls.LBL_BAR3)
        _lblBar.push_back(_controls.LBL_BAR3);
    if (_controls.LBL_BAR4)
        _lblBar.push_back(_controls.LBL_BAR4);
    if (_controls.LBL_BAR5)
        _lblBar.push_back(_controls.LBL_BAR5);
    if (_controls.LBL_BAR6)
        _lblBar.push_back(_controls.LBL_BAR6);

    if (_controls.LBL_TITLE) {
        _defaultTitleText = _controls.LBL_TITLE->text().text;
    }

    if (isTSL()) {
        _controls.LBL_STATSBORDER->setTintBorderFill(true);
        _controls.LBL_XP_BACK->setTintBorderFill(true);
        useK2ShellTitle(_controls.LBL_TITLE);
        enableK2ButtonBodyFill(_controls.BTN_EXIT);
        _controls.BTN_CHANGE1 = _inGameMenu.getBtnChange2();
        _controls.BTN_CHANGE2 = _inGameMenu.getBtnChange3();
    } else {
        if (_controls.BTN_CHANGE1) {
            _controls.BTN_CHANGE1->setSelectable(false);
            _controls.BTN_CHANGE1->setOnClick([this]() {
                if (_itemBacking && _itemBacking->selectPortrait(1)) {
                    refreshControls();
                }
            });
        }
        if (_controls.BTN_CHANGE2) {
            _controls.BTN_CHANGE2->setSelectable(false);
            _controls.BTN_CHANGE2->setOnClick([this]() {
                if (_itemBacking && _itemBacking->selectPortrait(2)) {
                    refreshControls();
                }
            });
        }
    }

    // Auto: the level-up screen's steps with every Recommended pick.
    _controls.BTN_AUTO->setOnClick([this]() {
        if (_subjectView.isGuestSubject) {
            return;
        }
        _game.levelUpThroughScreen(_game.party().getLeader(), true);
    });
    _controls.BTN_EXIT->setOnClick([this]() {
        if (_itemBacking) {
            _itemBacking->resetInspectedSubject();
        }
        _game.openInGame();
    });
    _controls.BTN_LEVELUP->setOnClick([this]() {
        if (_subjectView.isGuestSubject) {
            return;
        }
        _game.openLevelUp();
    });

    if (!isTSL()) {
        // _controls.btnCharLeft->setVisible(false);
        // _controls.btnCharRight->setVisible(false);

        for (auto &control : _lblGood) {
            control->setVisible(false);
        }

        _controls.LBL_MORE->setVisible(false);
        _scripts = _controls.BTN_SCRIPTS;
    } else if (_controls.BTN_EXIT) {
        // character_p has no Scripts button: one in BTN_EXIT's K2 style, on
        // the footer row right of it (as equip_p's Equip sits).
        auto button = std::static_pointer_cast<Button>(std::shared_ptr<Control>(_gui->newControl(ControlType::Button, "BTN_SCRIPTS")));
        button->setBorder(_controls.BTN_EXIT->border());
        button->setHilight(_controls.BTN_EXIT->hilight());
        button->setText(_controls.BTN_EXIT->text());
        auto extent = k2ScriptsButtonExtent();
        button->setAuthoredExtent(extent);
        button->setExtent(extent);
        _gui->addControlToFront(button, IGUI::ControlCoordinates::Authored);
        enableK2ButtonBodyFill(button);
        _scripts = std::move(button);
    }
    if (_scripts) {
        _scripts->setDisabled(false);
        _scripts->setOnClick([this]() {
            // Co-op: the party AI runs on the host; a client's button is off.
            if (auto *coop = _game.coop(); coop && coop->isClient()) {
                return;
            }
            _game.party().setAIStyle(nextPartyAIStyle(_game.party().aiStyle()));
            refreshPartyAIStyle();
        });
        refreshPartyAIStyle();
    }
    if (_controls.SLD_ALIGN) {
        // The alignment bar only shows the value; both games make it read-only.
        _controls.SLD_ALIGN->setSelectable(false);
    }
    // K2-only: force mastery has no K1 data.
    hideK2OnlyControls({"LBL_FORCEMASTERY"});
}

int CharacterMenu::partyAIStyleStrRef(int style) {
    switch (style) {
    case 1:
        return 1097;
    case 2:
        return 1098;
    default:
        return 1096;
    }
}

std::shared_ptr<Creature> CharacterMenu::subjectCreature() const {
    if (_itemBacking) {
        uint32_t subjId = _itemBacking->activeSubjectId();
        if (subjId != 0) {
            if (auto subj = _game.getObjectById<Creature>(subjId); subj && subj->isRuntimeLive()) {
                return subj;
            }
        }
    }
    return _game.party().getLeader();
}

void CharacterMenu::refreshPartyAIStyle() {
    if (!_scripts) {
        return;
    }
    // "Scripts: Defensive" (TLK 1067 and the style's name).
    auto &strings = _services.resource.strings;
    _scripts->setTextMessage(strings.getText(1067) + ": " + strings.getText(partyAIStyleStrRef(_game.party().aiStyle())));
    auto *coop = _game.coop();
    _scripts->setDisabled((coop && coop->isClient()) || _subjectView.isGuestSubject);
}

void CharacterMenu::update(float dt) {
    std::shared_ptr<Creature> leader(_game.party().getLeader());
    bool canLevelLeader = !_subjectView.isGuestSubject && leader && leader->isLevelUpPending();
    _controls.BTN_LEVELUP->setVisible(canLevelLeader);
    _controls.BTN_AUTO->setVisible(canLevelLeader);
    GameGUI::update(dt);
}

static std::string toStringOrEmptyIfZero(int value) {
    return value != 0 ? std::to_string(value) : "";
}

static std::string describeAbilityModifier(int value) {
    return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
}

void CharacterMenu::refreshControls() {
    _subjectView = _itemBacking ? _itemBacking->readEquipment(-1).subject : MenuSubjectView {};
    refreshPartyAIStyle();
    std::shared_ptr<Creature> subject(subjectCreature());
    if (!subject) {
        return;
    }
    if (_controls.LBL_TITLE && !_defaultTitleText.empty()) {
        _controls.LBL_TITLE->setTextMessage(
            _subjectView.isGuestSubject && !_subjectView.subjectName.empty()
                ? _subjectView.subjectName + " - " + _defaultTitleText
                : _defaultTitleText);
    }
    if (_controls.LBL_NAME) {
        std::string display = !_subjectView.subjectName.empty() ? _subjectView.subjectName : subject->name();
        if (!display.empty()) {
            _controls.LBL_NAME->setTextMessage(
                _subjectView.isGuestSubject ? display + " - Character" : display);
        }
    }
    CreatureAttributes &attributes = subject->attributes();

    if (!isTSL()) {
        _controls.LBL_CLASS1->setTextMessage(describeClass(attributes.getClassByPosition(1)));
        _controls.LBL_CLASS2->setTextMessage(describeClass(attributes.getClassByPosition(2)));
        _controls.LBL_LEVEL1->setTextMessage(toStringOrEmptyIfZero(attributes.getLevelByPosition(1)));
        _controls.LBL_LEVEL2->setTextMessage(toStringOrEmptyIfZero(attributes.getLevelByPosition(2)));
        // A K2 prestige class (the K2 systems merge's third class slot)
        // shares the second line: "Jedi Sentinel / Jedi Watchman", "8/3".
        if (attributes.getClassByPosition(3) != ClassType::Invalid) {
            _controls.LBL_CLASS2->setTextMessage(describeClass(attributes.getClassByPosition(2)) + " / " +
                                                 describeClass(attributes.getClassByPosition(3)));
            _controls.LBL_LEVEL2->setTextMessage(std::to_string(attributes.getLevelByPosition(2)) + "/" +
                                                 std::to_string(attributes.getLevelByPosition(3)));
        }
    }

    _controls.LBL_VITALITY_STAT->setTextMessage(str(boost::format("%d/%d") % subject->currentHitPoints() % subject->maxHitPoints()));
    _controls.LBL_DEFENSE_STAT->setTextMessage(std::to_string(subject->getDefense()));
    _controls.LBL_FORCE_STAT->setTextMessage(subject->maxForcePoints() > 0
                                                 ? str(boost::format("%d/%d") % subject->currentForce() % subject->maxForcePoints())
                                                 : std::string());

    _controls.LBL_STR->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Strength)));
    _controls.LBL_STR_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Strength)));
    _controls.LBL_DEX->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Dexterity)));
    _controls.LBL_DEX_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Dexterity)));
    _controls.LBL_CON->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Constitution)));
    _controls.LBL_CON_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Constitution)));
    _controls.LBL_INT->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Intelligence)));
    _controls.LBL_INT_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Intelligence)));
    _controls.LBL_WIS->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Wisdom)));
    _controls.LBL_WIS_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Wisdom)));
    _controls.LBL_CHA->setTextMessage(std::to_string(subject->getEffectiveAbilityScore(Ability::Charisma)));
    _controls.LBL_CHA_MOD->setTextMessage(describeAbilityModifier(subject->getEffectiveAbilityModifier(Ability::Charisma)));

    SavingThrows savingThrows(attributes.getAggregateSavingThrows());
    _controls.LBL_FORTITUDE_STAT->setTextMessage(std::to_string(savingThrows.fortitude));
    _controls.LBL_REFLEX_STAT->setTextMessage(std::to_string(savingThrows.reflex));
    _controls.LBL_WILL_STAT->setTextMessage(std::to_string(savingThrows.will));

    if (_controls.SLD_ALIGN) {
        // Light at the top, as the LBL_LIGHT / LBL_DARK labels read.
        _controls.SLD_ALIGN->setValue(subject->goodEvil() / 100.0f);
    }

    _controls.LBL_EXPERIENCE_STAT->setTextMessage(std::to_string(subject->xp()));
    _controls.LBL_NEEDED_XP->setTextMessage(std::to_string(subject->getNeededXP()));

    refreshPortraits();
    refresh3D();
}

std::string CharacterMenu::describeClass(ClassType clazz) const {
    if (clazz == ClassType::Invalid)
        return "";

    return _services.game.classes.get(clazz)->name();
}

void CharacterMenu::refreshPortraits() {
    if (isTSL())
        return;

    if (_itemBacking && _subjectView.present) {
        if (_controls.BTN_CHANGE1) {
            bool ok = _subjectView.portraitSelectable[1] && _subjectView.portraits[1] != nullptr;
            _controls.BTN_CHANGE1->setBorderFill(_subjectView.portraits[1]);
            _controls.BTN_CHANGE1->setHilightFill(_subjectView.portraits[1]);
            _controls.BTN_CHANGE1->setSelectable(ok);
        }
        if (_controls.BTN_CHANGE2) {
            bool ok = _subjectView.portraitSelectable[2] && _subjectView.portraits[2] != nullptr;
            _controls.BTN_CHANGE2->setBorderFill(_subjectView.portraits[2]);
            _controls.BTN_CHANGE2->setHilightFill(_subjectView.portraits[2]);
            _controls.BTN_CHANGE2->setSelectable(ok);
        }
        return;
    }

    Party &party = _game.party();
    std::shared_ptr<Creature> partyMember1(party.getMember(1));
    std::shared_ptr<Creature> partyMember2(party.getMember(2));

    _controls.BTN_CHANGE1->setBorderFill(partyMember1 ? partyMember1->portrait() : nullptr);
    _controls.BTN_CHANGE1->setHilightFill(partyMember1 ? partyMember1->portrait() : nullptr);

    _controls.BTN_CHANGE2->setBorderFill(partyMember2 ? partyMember2->portrait() : nullptr);
    _controls.BTN_CHANGE2->setHilightFill(partyMember2 ? partyMember2->portrait() : nullptr);
}

void CharacterMenu::refresh3D() {
    auto &sceneGraph = _services.scene.graphs.get(kSceneCharacter);
    float aspect = _controls.LBL_3DCHAR->extent().width / static_cast<float>(_controls.LBL_3DCHAR->extent().height);

    SceneInitializer(sceneGraph)
        .aspect(aspect)
        .depth(kDefaultClipPlaneNear, 10.0f)
        .modelSupplier(bind(&CharacterMenu::getSceneModel, this, std::placeholders::_1))
        .modelOffset(glm::vec2(0.0f, 1.7f))
        .cameraFromModelNode("camerahook")
        .invoke();

    _controls.LBL_3DCHAR->setSceneName(kSceneCharacter);
}

static void softenAlignmentLights(SceneNode &node, float factor);

std::shared_ptr<ModelSceneNode> CharacterMenu::getSceneModel(ISceneGraph &sceneGraph) const {
    auto subject = subjectCreature();
    if (!subject) {
        return nullptr;
    }

    std::shared_ptr<Creature> character =
        _game.newPresentationCreature(sceneGraph.name());
    character->setFacing(-glm::half_pi<float>());
    character->setAppearance(subject->appearance());

    for (auto &item : subject->equipment()) {
        switch (item.first) {
        case InventorySlots::body: {
            auto previewItem = _game.newPresentationItem();
            previewItem->clone(*item.second);
            character->equip(item.first, previewItem);
            break;
        }
        default:
            break;
        }
    }

    character->loadAppearance();
    character->updateModelAnimation();

    // Both games stand the character in charrec_light: mist, streams and
    // sparkles posed for the alignment by the model's own zero-length
    // animations. (charmain_light is a plain blue mist neither game shows here.)
    auto sceneModel = sceneGraph.newModel(*_services.resource.models.get("charrec_light"), ModelUsage::GUI);
    sceneModel->attach("charrec_light", *character->sceneNode());
    sceneModel->playAnimation(alignmentAnimation(subject->goodEvil()), nullptr,
                              AnimationProperties::fromFlags(AnimationFlags::loop));
    int goodEvil = std::clamp(subject->goodEvil(), 0, 100);
    if (goodEvil < 50) {
        softenAlignmentLights(*sceneModel, 1.0f - 0.8f * (50 - goodEvil) / 50.0f);
    }

    return sceneModel;
}

// The dark side's fire lights the character far brighter than the light and
// neutral backdrops do. Scale this scene's lights down toward the dark end.
static void softenAlignmentLights(SceneNode &node, float factor) {
    if (node.type() == SceneNodeType::Light) {
        auto &light = static_cast<LightSceneNode &>(node);
        light.setMultiplier(light.multiplier() * factor);
    } else if (node.type() == SceneNodeType::Emitter) {
        static_cast<EmitterSceneNode &>(node).setAlphaScale(0.5f + 0.5f * factor);
    }
    for (auto &child : node.children()) {
        softenAlignmentLights(*child, factor);
    }
}

std::string CharacterMenu::alignmentAnimation(int goodEvil) {
    int band = std::clamp(goodEvil, 0, 100) / 5;
    if (band == 0) {
        return "evil";
    }
    if (band == 20) {
        return "good";
    }
    return "align" + std::to_string(band);
}

} // namespace game

} // namespace reone
