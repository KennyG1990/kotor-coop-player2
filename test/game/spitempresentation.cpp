/*
 * Copyright (c) 2026 The reone project contributors
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

#include "../fixtures/engine.h"
#include "../fixtures/itempresentation.h"
#include "reone/game/game.h"
#include "reone/game/gui/ingame/itembacking.h"
#include "reone/game/object/creature.h"
#include "reone/game/object/item.h"
#include "reone/game/party.h"
#include "reone/resource/2da.h"

namespace {

class SPItemPresentation : public SharedPresentation {};

TEST_F(SPItemPresentation, sp_backing_applies_the_real_equipment_presenters_selection) {
    TestEngine engine;
    engine.init();
    StubConsole console;
    Game game(GameID::KotOR, "", engine.options(), engine.services(), console);
    auto baseItems = std::shared_ptr<TwoDA>(TwoDA::Builder()
                                                .columns({"equipableslots", "itemclass"})
                                                .row({"2", "armor"})
                                                .row({"2", "armor"})
                                                .build());
    EXPECT_CALL(engine.resourceModule().twoDas(), get("baseitems"))
        .Times(AnyNumber())
        .WillRepeatedly(Return(baseItems));
    auto player = game.newCreature();
    game.party().setPlayer(player);
    ASSERT_TRUE(game.party().addMember(kNpcPlayer, player));
    auto record = Gff::Builder().field(Gff::Field::newInt("BaseItem", 0)).field(Gff::Field::newWord("StackSize", 2)).field(Gff::Field::newCExoLocString("LocalizedName", -1, "Saved armor")).field(Gff::Field::newCExoString("Tag", "same_tag")).build();
    auto item = game.newItem();
    item->deserialize(*record, SerializedIdentityContext::templateResource());
    player->addItem(item);
    auto secondRecord = Gff::Builder().field(Gff::Field::newInt("BaseItem", 1)).field(Gff::Field::newWord("StackSize", 3)).field(Gff::Field::newCExoLocString("LocalizedName", -1, "Other saved armor")).field(Gff::Field::newCExoString("Tag", "same_tag")).build();
    auto second = game.newItem();
    second->deserialize(*secondRecord, SerializedIdentityContext::templateResource());
    player->addItem(second);
    ScreenResources guis(options, scene, graphics, resources);
    auto backing = newEquipmentMenuBacking(game, engine.services());
    Equipment screen(GameID::KotOR, options, services(guis), resources.services().strings, backing, []() {});
    screen.init();
    screen.openItems();
    auto list = std::static_pointer_cast<ListBox>(guis.screens.at("equip")->findControl("LB_ITEMS"));
    ASSERT_EQ(3, list->getItemCount());
    EXPECT_EQ("Saved armor", list->getItemAt(1).text);
    list->setSelectedItemIndex(2);
    guis.screens.at("equip")->findControl("BTN_EQUIP")->handleClick(0, 0);
    auto equipped = player->getEquippedItem(InventorySlots::body);
    ASSERT_TRUE(equipped);
    EXPECT_NE(item, equipped);
    EXPECT_EQ(1, equipped->baseItemType());
    EXPECT_EQ(2, item->stackSize());
    EXPECT_EQ(2, second->stackSize());
    EXPECT_EQ(1, equipped->stackSize());
    EXPECT_EQ(player->id(), equipped->owner());
    screen.openItems();
    list->setSelectedItemIndex(0);
    guis.screens.at("equip")->findControl("BTN_EQUIP")->handleClick(0, 0);
    EXPECT_FALSE(player->getEquippedItem(InventorySlots::body));
    ASSERT_EQ(2u, player->items().size());
    EXPECT_EQ(2, item->stackSize());
    EXPECT_EQ(3, second->stackSize());
    EXPECT_FALSE(equipped->isRuntimeLive());
}

TEST_F(SPItemPresentation, inventory_and_equipment_presenters_drive_portrait_switch_and_item_transfer) {
    struct TransferStubBacking : public SuppliedItems {
        int selectedPortrait {0};
        std::vector<uint64_t> transferred;
        bool selectPortrait(int slot) override {
            selectedPortrait = slot;
            inventory.subject.isGuestSubject = true;
            inventory.subject.subjectName = "Dessa";
            inventory.subject.customTitlePrefix = "Dessa's Pack - ";
            inventory.subject.transferActionLabel = "Take to Party";
            equipment.subject = inventory.subject;
            return true;
        }
        bool transferItem(uint64_t handle) override {
            transferred.push_back(handle);
            return true;
        }
    };

    ScreenResources guis(options, scene, graphics, resources);
    auto backing = std::make_shared<TransferStubBacking>();
    auto portraitTex = std::make_shared<Texture>("po_pfha01", TextureType::TwoDim, Texture::Properties {});
    backing->inventory.subject.present = true;
    backing->inventory.subject.portraits[1] = portraitTex;
    backing->inventory.subject.portraitSelectable[1] = true;
    backing->inventory.subject.transferActionLabel = "Give to Dessa";
    backing->equipment.subject = backing->inventory.subject;
    MenuItemView transferableItem;
    transferableItem.handle = 42;
    transferableItem.name = "Medpac";
    transferableItem.description = "Heals wounds";
    transferableItem.stackSize = 2;
    transferableItem.transferable = true;
    backing->inventory.items = {transferableItem};

    InventoryMenu inv(GameID::KotOR, options, services(guis), backing, []() {});
    inv.init();
    inv.refreshPortraits();
    inv.refreshItems();

    auto invGui = guis.screens.at("inventory");
    auto useBtn = invGui->findControl("BTN_USEITEM");
    auto titleLbl = invGui->findControl("LBL_INV");
    auto change1Btn = invGui->findControl("BTN_CHANGE1");
    EXPECT_EQ("Party Inventory - All Items", titleLbl->text().text);
    EXPECT_FALSE(useBtn->isDisabled());
    EXPECT_EQ("Give to Dessa", useBtn->text().text);
    EXPECT_TRUE(change1Btn->isSelectable());

    useBtn->handleClick(0, 0);
    ASSERT_EQ(1u, backing->transferred.size());
    EXPECT_EQ(42u, backing->transferred[0]);

    change1Btn->handleClick(0, 0);
    EXPECT_EQ(1, backing->selectedPortrait);
    EXPECT_EQ("Dessa's Pack - All Items", titleLbl->text().text);
    EXPECT_EQ("Take to Party", useBtn->text().text);

    Equipment equip(GameID::KotOR, options, services(guis), resources.services().strings, backing, []() {});
    equip.init();
    equip.update();
    EXPECT_TRUE(guis.screens.at("equip")->findControl("BTN_CHANGE1")->isSelectable());
}

} // namespace
