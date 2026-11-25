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

#include "reone/game/gui/ingame/abilities.h"

#include "reone/game/d20/feat.h"
#include "reone/game/d20/feats.h"
#include "reone/game/d20/forcepowers.h"
#include "reone/game/d20/spell.h"
#include "reone/game/d20/spells.h"
#include "reone/game/di/services.h"
#include "reone/game/game.h"
#include "reone/game/gui/ingame.h"
#include "reone/game/object/creature.h"
#include "reone/game/party.h"
#include "reone/gui/control/button.h"
#include "reone/gui/control/label.h"
#include "reone/gui/control/listbox.h"
#include "reone/resource/2da.h"
#include "reone/resource/di/services.h"
#include "reone/resource/provider/2das.h"
#include "reone/resource/provider/textures.h"
#include "reone/resource/strings.h"

using namespace reone::audio;

using namespace reone::graphics;
using namespace reone::gui;
using namespace reone::resource;

namespace reone {

namespace game {

static constexpr int kStrRefSkillRank = 1579;
static constexpr int kStrRefBonus = 32129;
static constexpr int kStrRefTotalRank = 41904;

void AbilitiesMenu::onGUILoaded() {
    loadBackground(BackgroundType::Menu);
    bindControls();
    tintK2InGameFooter();
    tintK2InGameHeader();

    // The Skills / Powers / Feats tabs (design note k1hud_parity_2, follow-up): live
    // on both layouts; the selected tab is lit as K2's filter buttons are.
    for (auto &[button, tab] : std::initializer_list<std::pair<std::shared_ptr<Button>, Tab>> {
             {_controls.BTN_SKILLS, Tab::Skills}, {_controls.BTN_POWERS, Tab::Powers}, {_controls.BTN_FEATS, Tab::Feats}}) {
        if (!button) {
            continue;
        }
        button->setDisabled(false);
        Tab target = tab;
        button->setOnClick([this, target]() {
            selectTab(target);
        });
    }

    _controls.LBL_SKILLRANK->setTextMessage(_services.resource.strings.getText(kStrRefSkillRank));
    _controls.LBL_BONUS->setTextMessage(_services.resource.strings.getText(kStrRefBonus));
    _controls.LBL_TOTAL->setTextMessage(_services.resource.strings.getText(kStrRefTotalRank));
    _controls.LBL_RANKVAL->setTextMessage("");
    _controls.LBL_BONUSVAL->setTextMessage("");
    _controls.LBL_TOTALVAL->setTextMessage("");
    _controls.LBL_NAME->setTextMessage("");

    _controls.LB_DESC->setProtoMatchContent(true);
    if (_controls.LBL_ABILITIES) {
        _defaultAbilitiesTitle = _controls.LBL_ABILITIES->text().text;
    }
    if (isTSL()) {
        fillK2SectionStrip(_controls.LBL_BAR1, _controls.LBL_BAR2);
        _controls.LB_ABILITY->setTintBorderFill(true);
        _controls.LB_DESC->setTintBorderFill(true);
        _controls.LB_DESC_FEATS->setTintBorderFill(true);
        _controls.LBL_INFOBG->setTintBorderFill(true);
        _controls.LBL_NAME->setTintBorderFill(true);
        useK2ShellTitle(_controls.LBL_ABILITIES);
        enableK2ButtonBodyFill(_controls.BTN_EXIT);
        if (_inGameMenu) {
            _controls.BTN_CHANGE1 = _inGameMenu->getBtnChange2();
            _controls.BTN_CHANGE2 = _inGameMenu->getBtnChange3();
        }
        if (auto protoItem = _controls.LB_ABILITY->protoItemOrNull()) {
            enableK2ButtonBodyFill(*protoItem);
            protoItem->setBorderFill("uibit_fill_2wt");
            protoItem->setHilightFill("uibit_fill_2wt");
            protoItem->setTintBorderFill(true);
        }
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
    _controls.LB_ABILITY->setOnItemClick([this](const std::string &item) {
        onListItemClick(item);
    });
    _controls.BTN_EXIT->setOnClick([this]() {
        if (_itemBacking) {
            _itemBacking->resetInspectedSubject();
        }
        _game.openInGame();
    });

    loadSkills();
    selectTab(Tab::Skills);
}

void AbilitiesMenu::onListItemClick(const std::string &item) {
    if (item.empty()) {
        return;
    }
    if (item[0] == 'p' || item[0] == 'f') {
        auto it = std::find_if(_entries.begin(), _entries.end(), [&item](const Entry &e) { return e.tag == item; });
        if (it == _entries.end()) {
            return;
        }
        _controls.LBL_NAME->setTextMessage(it->name);
        auto &desc = (isTSL() && _controls.LB_DESC_FEATS) ? _controls.LB_DESC_FEATS : _controls.LB_DESC;
        desc->clearItems();
        desc->addTextLinesAsItems(it->description);
        return;
    }
    {
        auto skill = static_cast<SkillType>(stoi(item));
        auto maybeSkillInfo = _skills.find(skill);
        if (maybeSkillInfo == _skills.end())
            return;

        std::shared_ptr<Creature> subject(subjectCreature());
        if (!subject) {
            return;
        }
        int rank = subject->attributes().getSkillRank(skill);
        int total = subject->getSkillValue(skill);
        int bonus = total - rank;
        _controls.LBL_RANKVAL->setTextMessage(std::to_string(rank));
        _controls.LBL_BONUSVAL->setTextMessage(bonus >= 0 ? "+" + std::to_string(bonus) : std::to_string(bonus));
        _controls.LBL_TOTALVAL->setTextMessage(std::to_string(total));
        _controls.LBL_NAME->setTextMessage(maybeSkillInfo->second.name);

        _controls.LB_DESC->clearItems();
        _controls.LB_DESC->addTextLinesAsItems(maybeSkillInfo->second.description);
    }
}

std::vector<AbilitiesMenu::Entry> AbilitiesMenu::entriesFor(Tab tab, const CreatureAttributes &attributes,
                                                            const ISpells &spells, const IFeats &feats) {
    std::vector<Entry> result;
    if (tab == Tab::Powers) {
        for (SpellType type : attributes.spells()) {
            auto spell = spells.get(type);
            if (!spell || spell->userType != kForcePowerUserType) {
                continue;
            }
            result.push_back({"p" + std::to_string(static_cast<int>(type)), spell->name, spell->description, spell->icon});
        }
    } else if (tab == Tab::Feats) {
        for (FeatType type : attributes.feats()) {
            auto feat = feats.get(type);
            if (!feat || feat->name.empty()) {
                continue;
            }
            result.push_back({"f" + std::to_string(static_cast<int>(type)), feat->name, feat->description, feat->icon});
        }
    }
    return result;
}

void AbilitiesMenu::showSkillInfo(bool show) {
    // The rank / bonus / total panel belongs to skills.
    for (auto &label : {_controls.LBL_INFOBG, _controls.LBL_SKILLRANK, _controls.LBL_RANKVAL, _controls.LBL_BONUS,
                        _controls.LBL_BONUSVAL, _controls.LBL_TOTAL, _controls.LBL_TOTALVAL, _controls.LBL_BAR5}) {
        if (label) {
            label->setVisible(show);
        }
    }
    if (isTSL() && _controls.LB_DESC_FEATS) {
        // K2 authors a taller description for feats and powers.
        _controls.LB_DESC_FEATS->setVisible(!show);
        _controls.LB_DESC->setVisible(show);
    }
}

void AbilitiesMenu::selectTab(Tab tab) {
    _tab = tab;
    for (auto &[button, each] : std::initializer_list<std::pair<std::shared_ptr<Button>, Tab>> {
             {_controls.BTN_SKILLS, Tab::Skills}, {_controls.BTN_POWERS, Tab::Powers}, {_controls.BTN_FEATS, Tab::Feats}}) {
        if (!button) {
            continue;
        }
        bool selected = each == tab;
        if (isTSL()) {
            updateK2FilterButton(button, selected);
        } else {
            button->setSelected(selected);
            button->setTextColor(selected ? _hilightColor : _baseColor);
        }
    }
    showSkillInfo(tab == Tab::Skills);
    _controls.LBL_NAME->setTextMessage("");
    _controls.LB_DESC->clearItems();
    if (_controls.LB_DESC_FEATS) {
        _controls.LB_DESC_FEATS->clearItems();
    }
    refreshList();
}

void AbilitiesMenu::refreshList() {
    if (_tab == Tab::Skills) {
        _controls.LB_ABILITY->clearItems();
        for (auto &skill : _skills) {
            ListBox::Item item;
            item.tag = std::to_string(static_cast<int>(skill.second.skill));
            item.text = skill.second.name;
            item.iconFrame = itemFrameTexture(1);
            item.iconTexture = skill.second.icon;
            _controls.LB_ABILITY->addItem(std::move(item));
        }
        return;
    }
    auto subject = subjectCreature();
    _entries = subject ? entriesFor(_tab, subject->attributes(), _services.game.spells, _services.game.feats)
                       : std::vector<Entry>();
    _controls.LB_ABILITY->clearItems();
    for (const auto &entry : _entries) {
        ListBox::Item item;
        item.tag = entry.tag;
        item.text = entry.name;
        item.iconFrame = itemFrameTexture(1);
        item.iconTexture = entry.icon;
        _controls.LB_ABILITY->addItem(std::move(item));
    }
}

void AbilitiesMenu::loadSkills() {
    std::shared_ptr<TwoDA> skills(_services.resource.twoDas.get("skills"));
    for (int row = 0; row < skills->getRowCount(); ++row) {
        auto skill = static_cast<SkillType>(row);

        SkillInfo skillInfo;
        skillInfo.skill = skill;
        skillInfo.name = _services.resource.strings.getText(skills->getInt(row, "name"));
        skillInfo.description = _services.resource.strings.getText(skills->getInt(row, "description"));
        skillInfo.icon = _services.resource.textures.get(skills->getString(row, "icon"), TextureUsage::GUI);

        _skills.insert(std::make_pair(skill, std::move(skillInfo)));
    }

}

std::shared_ptr<Creature> AbilitiesMenu::subjectCreature() const {
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

void AbilitiesMenu::refreshControls() {
    _subjectView = _itemBacking ? _itemBacking->readEquipment(-1).subject : MenuSubjectView {};
    auto subject = subjectCreature();
    if (_controls.LBL_ABILITIES) {
        std::string baseTitle = _defaultAbilitiesTitle.empty() ? "Abilities" : _defaultAbilitiesTitle;
        std::string display = !_subjectView.subjectName.empty()
                                  ? _subjectView.subjectName
                                  : (subject ? subject->name() : std::string());
        _controls.LBL_ABILITIES->setTextMessage(
            (_subjectView.isGuestSubject && !display.empty()) ? (display + " - " + baseTitle) : baseTitle);
    }
    if (_controls.LBL_PORTRAIT) {
        _controls.LBL_PORTRAIT->setBorderFill(subject ? subject->portrait() : nullptr);
    }
    refreshPortraits();
    // The inspected subject may have changed since the screen was built.
    refreshList();
}

void AbilitiesMenu::refreshPortraits() {
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

} // namespace game

} // namespace reone
