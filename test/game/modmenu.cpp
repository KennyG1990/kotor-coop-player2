/* Copyright (c) 2026 The reone project contributors */

#include <gtest/gtest.h>

#include "reone/game/modmenu.h"

using namespace reone::game;

TEST(ModMenu, nothing_shows_until_a_program_sets_a_screen) {
    ModMenuModel model;
    EXPECT_FALSE(model.active());
    model.pick(0);
    EXPECT_TRUE(model.takePicks().empty());
}

TEST(ModMenu, a_screen_is_read_from_json_and_clicks_are_kept_for_the_program) {
    ModMenuModel model;
    model.set(ModMenuModel::parse(R"({"button":"Companions","title":"Companion settings","text":"Joules today: 12 of 200",
                                     "rows":["Joules a day: 200","Calls a minute: 6"],"back":"Done"})"));
    ASSERT_TRUE(model.active());
    EXPECT_EQ("Companions", model.spec().button);
    EXPECT_EQ("Done", model.spec().back);
    ASSERT_EQ(2u, model.spec().rows.size());
    unsigned version = model.version();
    model.pick(1);
    model.pick(0);
    model.pick(4); // no such row: not kept
    EXPECT_EQ((std::vector<int> {1, 0}), model.takePicks());
    EXPECT_TRUE(model.takePicks().empty());
    model.set(ModMenuModel::parse(R"({"button":"Companions","title":"T","rows":["x"]})"));
    EXPECT_GT(model.version(), version);
    EXPECT_EQ("Close", model.spec().back);
    model.clear();
    EXPECT_FALSE(model.active());
}

TEST(ModMenu, only_so_many_clicks_wait) {
    ModMenuModel model;
    model.set(ModMenuModel::parse(R"({"button":"B","title":"T","rows":["a"]})"));
    for (int i = 0; i < 100; ++i) {
        model.pick(0);
    }
    EXPECT_EQ(ModMenuModel::kPicksKept, model.takePicks().size());
}

TEST(ModMenu, what_is_not_a_screen_is_refused) {
    for (const char *bad : {"not json", "[]", R"({"title":"T","rows":["a"]})", R"({"button":"B","title":"T","rows":[]})",
                            R"({"button":"B","title":"T","rows":["1","2","3","4","5","6"]})",
                            R"({"button":"B","title":"T","rows":[""]})", R"({"button":"B","title":"T","rows":[3]})",
                            R"({"button":"A button label far too long to fit","title":"T","rows":["a"]})"}) {
        EXPECT_THROW(ModMenuModel::parse(bad), std::invalid_argument) << bad;
    }
}
