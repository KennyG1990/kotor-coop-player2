/* Copyright (c) 2026 The reone project contributors */

// Story Mode stand-ins: who a guest plays (coopcast.h) and the automatic
// level-up the copies and "Auto Level Up" share (autolevel.h).

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../fixtures/resource.h"

#include "reone/game/autolevel.h"
#include "reone/game/coopcast.h"
#include "reone/game/coopprogress.h"
#include "reone/game/coopstandin.h"
#include "reone/game/d20/class.h"
#include "reone/game/d20/classes.h"
#include "reone/game/d20/feats.h"
#include "reone/game/d20/spells.h"
#include "reone/resource/2da.h"

using namespace reone;
using namespace reone::game;
using namespace reone::resource;
using namespace testing;

namespace {

constexpr int kBastila = 0;
constexpr int kCanderous = 1;
constexpr int kCarth = 2;
constexpr int kHK47 = 3;
constexpr int kJolee = 4;
constexpr int kJuhani = 5;
constexpr int kMission = 6;
constexpr int kT3M4 = 7;
constexpr int kZaalbar = 8;

CoopCastInput input(const std::string &module) {
    CoopCastInput result;
    result.module = module;
    return result;
}

bool skipped(const CoopCastChoice &choice, const std::string &name, const std::string &why) {
    for (const auto &[who, reason] : choice.skipped) {
        if (who == name && reason.find(why) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(CoopCast, the_endar_spire_has_no_companion_to_copy_so_shepard_turns_up_as_a_republic_soldier) {
    auto choice = chooseCoopCast(input("end_m01aa"));
    EXPECT_EQ(CoopCastKind::Generic, choice.kind);
    EXPECT_EQ(kCoopShepardTemplate, choice.templ);
    EXPECT_EQ("Republic soldier", choice.name);
    EXPECT_EQ(ClassType::Soldier, choice.clazz);
    EXPECT_NE(std::string::npos, choice.reason.find("John Shepard"));
    EXPECT_EQ(0, choice.tier);
}

TEST(CoopCast, shepard_is_one_identity_and_only_the_outfit_changes_by_area) {
    EXPECT_EQ("John Shepard", coopShepardName());
    EXPECT_EQ(170, coopShepard().appearance);
    EXPECT_EQ(24, coopShepard().portrait);
    EXPECT_EQ(Gender::Male, coopShepard().gender);
    const auto *endar = coopGenericFor("end_m01ab");
    const auto *dantooine = coopGenericFor("danm13");
    const auto *lowerCity = coopGenericFor("tar_m03aa");
    const auto *upperCity = coopGenericFor("tar_m02aa");
    ASSERT_TRUE(endar && dantooine && lowerCity && upperCity);
    EXPECT_STREQ("Jedi apprentice", dantooine->label);
    EXPECT_EQ(ClassType::JediGuardian, dantooine->clazz);
    EXPECT_STREQ("swoop gang member", lowerCity->label) << "the longest prefix wins";
    EXPECT_STREQ("Taris mercenary", upperCity->label);
    EXPECT_NE(std::string(endar->kit[0].second), std::string(dantooine->kit[0].second));
    EXPECT_EQ(nullptr, coopGenericFor("stunt_00"));
}

TEST(CoopCast, taris_copies_the_first_companion_who_fits_preferring_a_role_the_party_lacks) {
    auto in = input("tar_m02aa");
    in.partyNpcs = {kCarth};
    in.partyRoles = {CoopPartyRole::Tank}; // a soldier PC and Carth
    auto choice = chooseCoopCast(in);
    ASSERT_EQ(CoopCastKind::Copy, choice.kind);
    EXPECT_EQ(kMission, choice.npc) << "Mission is first and a scout";
    EXPECT_EQ("p_mission", choice.templ);
    EXPECT_EQ(CoopPartyRole::Scout, choice.role);
    EXPECT_TRUE(skipped(choice, "Carth", "in the party"));

    // With a scout already along, the first who brings what is missing.
    in.partyRoles = {CoopPartyRole::Scout};
    EXPECT_EQ(kCanderous, chooseCoopCast(in).npc) << "Zaalbar is a scout too; Canderous a tank";

    // Every role covered: the first who fits.
    in.partyRoles = {CoopPartyRole::Tank, CoopPartyRole::Scout, CoopPartyRole::Caster};
    EXPECT_EQ(kMission, chooseCoopCast(in).npc);
}

TEST(CoopCast, a_copy_is_never_in_the_party_another_guests_or_present_in_the_module) {
    auto in = input("tar_m04aa");
    in.partyNpcs = {kMission};
    in.takenNpcs = {kZaalbar};
    in.presentNpcs = {kCanderous};
    auto choice = chooseCoopCast(in);
    ASSERT_EQ(CoopCastKind::Copy, choice.kind);
    EXPECT_EQ(kT3M4, choice.npc);
    EXPECT_TRUE(skipped(choice, "Mission", "in the party"));
    EXPECT_TRUE(skipped(choice, "Zaalbar", "another guest"));
    EXPECT_TRUE(skipped(choice, "Canderous", "in this module"));

    // Nobody left: the character of the place.
    in.presentNpcs = {kCanderous, kT3M4, kCarth};
    choice = chooseCoopCast(in);
    EXPECT_EQ(CoopCastKind::Generic, choice.kind);
    EXPECT_EQ("swoop gang member", choice.name);
    EXPECT_NE(std::string::npos, choice.reason.find("no companion fits Taris"));
}

TEST(CoopCast, story_clashes_keep_companions_out_and_globals_lift_them) {
    // Bastila is never copied on Taris (a captive until rescued) nor on the Leviathan.
    EXPECT_FALSE(coopCopyExclusion(input("tar_m02aa"), kBastila).empty());
    EXPECT_FALSE(coopCopyExclusion(input("lev_m40aa"), kBastila).empty());
    EXPECT_FALSE(coopCopyExclusion(input("lev_m40aa"), kCarth).empty());
    EXPECT_FALSE(coopCopyExclusion(input("end_m01aa"), kCarth).empty());
    EXPECT_TRUE(coopCopyExclusion(input("danm13"), kBastila).empty());

    // Juhani waits in the grove until DAN_JUHANI_DONE; Zaalbar is a prisoner while kas_ZaalbarPris.
    std::map<std::string, bool> globals;
    auto in = input("danm14aa");
    in.globalBoolean = [&globals](const std::string &name) { return globals[name]; };
    EXPECT_NE(std::string::npos, coopCopyExclusion(in, kJuhani).find("grove"));
    globals["DAN_JUHANI_DONE"] = true;
    EXPECT_TRUE(coopCopyExclusion(in, kJuhani).empty());

    in.module = "kas_m22aa";
    EXPECT_TRUE(coopCopyExclusion(in, kZaalbar).empty());
    globals["kas_ZaalbarPris"] = true;
    EXPECT_NE(std::string::npos, coopCopyExclusion(in, kZaalbar).find("prisoner"));
    EXPECT_FALSE(coopCopyExclusion(in, kJolee).empty()) << "Jolee is in the Shadowlands until G_JoleeJoined";
    globals["G_JoleeJoined"] = true;
    EXPECT_TRUE(coopCopyExclusion(in, kJolee).empty());

    // HK-47 is for sale on Tatooine, so not copied there; Korriban takes him.
    EXPECT_FALSE(coopCopyExclusion(input("tat_m17aa"), kHK47).empty());
    auto korriban = chooseCoopCast(input("korr_m33aa"));
    EXPECT_EQ(kCanderous, korriban.npc);
    EXPECT_EQ(2, korriban.tier);
}

TEST(CoopCast, modules_the_tables_do_not_know_copy_the_solo_character_as_before) {
    auto choice = chooseCoopCast(input("001ebo"));
    EXPECT_EQ(CoopCastKind::Clone, choice.kind);
    EXPECT_NE(std::string::npos, choice.reason.find("001ebo"));
    EXPECT_EQ(CoopCastKind::Clone, chooseCoopCast(input("")).kind);
}

TEST(CoopCast, the_choice_is_deterministic) {
    auto in = input("manm26aa");
    in.partyNpcs = {kCarth, kBastila};
    in.partyRoles = {CoopPartyRole::Tank, CoopPartyRole::Caster};
    auto first = chooseCoopCast(in);
    for (int i = 0; i < 5; ++i) {
        auto again = chooseCoopCast(in);
        EXPECT_EQ(first.npc, again.npc);
        EXPECT_EQ(first.reason, again.reason);
    }
    EXPECT_EQ(kMission, first.npc);
}

TEST(CoopCast, every_roster_companion_and_table_entry_is_consistent) {
    EXPECT_EQ(9u, coopRoster().size());
    for (const auto &planet : coopPlanets()) {
        for (int npc : planet.companions) {
            EXPECT_TRUE(coopRosterEntry(npc)) << planet.prefix;
        }
        EXPECT_GE(planet.tier, 0);
        EXPECT_LE(planet.tier, 3);
    }
    for (const auto &line : coopGearLines()) {
        EXPECT_EQ(4u, line.tiers.size()) << line.label;
    }
    for (const auto &entry : coopRoster()) {
        EXPECT_TRUE(isCoopTemplateName(entry.templ));
    }
    EXPECT_TRUE(isCoopTemplateName(kCoopShepardTemplate));
}

// Gear tiers

TEST(CoopGear, a_template_item_below_the_planets_tier_gives_way_to_the_same_kind_of_item) {
    // HK-47's blaster rifle on Taris, Dantooine, Korriban, the Star Forge.
    EXPECT_EQ("", coopTieredItem(77, "g_w_blstrrfl001", 0));
    EXPECT_EQ("g_w_blstrrfl002", coopTieredItem(77, "g_w_blstrrfl001", 1));
    EXPECT_EQ("g_w_blstrrfl003", coopTieredItem(77, "g_w_blstrrfl001", 2));
    EXPECT_EQ("g_w_blstrrfl004", coopTieredItem(77, "G_W_BLSTRRFL001", 3));
    // Already as good, or better: kept.
    EXPECT_EQ("", coopTieredItem(77, "g_w_blstrrfl003", 2));
    EXPECT_EQ("", coopTieredItem(77, "g_w_blstrrfl004", 1));
    // Items the table does not list (Carth's own blaster, a lightsaber): kept.
    EXPECT_EQ("", coopTieredItem(12, "g_w_blstrpstl010", 3));
    EXPECT_EQ("", coopTieredItem(8, "g_w_lghtsbr01", 3));
    // A tier past the table's end takes its last item; a repeated entry is no swap.
    EXPECT_EQ("g_w_blstrrfl004", coopTieredItem(77, "g_w_blstrrfl001", 9));
    EXPECT_EQ("", coopTieredItem(19, "g_w_bowcstr003", 2));
    EXPECT_EQ("g_w_bowcstr002", coopTieredItem(19, "g_w_bowcstr003", 3));
    // The base type decides, not the name.
    EXPECT_EQ("", coopTieredItem(12, "g_w_blstrrfl001", 3));
}

// The template field and the name tag

TEST(CoopStandInTemplate, the_template_field_carries_the_template_and_refuses_junk) {
    auto resref = coopStandInResRef(6, 0, 7, "p_carth");
    EXPECT_EQ("@standin:6:0:7:p_carth", resref);
    int appearance = 0, gender = 0;
    uint32_t source = 0;
    std::string templ = "x";
    ASSERT_TRUE(parseCoopStandInResRef(resref, appearance, gender, source, &templ));
    EXPECT_EQ("p_carth", templ);
    ASSERT_TRUE(parseCoopStandInResRef(coopStandInResRef(6, 0, 7), appearance, gender, source, &templ));
    EXPECT_EQ("", templ);
    EXPECT_EQ("@standin:6:0:7", coopStandInResRef(6, 0, 7, "Bad Name"));
    for (std::string junk : {"@standin:6:0:7:", "@standin:6:0:7:P_CARTH", "@standin:6:0:7:a:b", "@standin:6:0:7:abcdefghijklmnopq"}) {
        EXPECT_FALSE(parseCoopStandInResRef(junk, appearance, gender, source, &templ)) << junk;
    }
    EXPECT_EQ("HK-47 (John)", coopStandInLabel("HK-47", "John"));
    EXPECT_EQ("John", coopStandInLabel("John", "John"));
    EXPECT_EQ("John", coopStandInLabel("", "John"));
}

// Auto-level

namespace {

std::shared_ptr<TwoDA> makeTable(const std::vector<std::string> &columns, const std::vector<std::vector<std::string>> &rows) {
    TwoDA::Builder builder;
    builder.columns(columns);
    for (const auto &row : rows) {
        builder.row(row);
    }
    return std::shared_ptr<TwoDA>(builder.build());
}

// A soldier (row 0, primaryabil STR, 2 skill points, a feat every level) and
// a Jedi Consular (row 4, primaryabil WIS, powers from classpowergain).
std::shared_ptr<TwoDA> makeClasses() {
    const std::vector<std::string> columns = {
        "name", "description", "hitdie", "skillpointbase", "str", "dex", "con", "int", "wis", "cha", "primaryabil",
        "skillstable", "savingthrowtable", "attackbonustable", "featstable", "featgain", "spellgaintable", "forcedie", "spellcaster"};
    std::vector<std::vector<std::string>> rows(9, std::vector<std::string>(columns.size()));
    rows[0] = {"1", "2", "10", "2", "16", "14", "14", "10", "12", "10", "STR", "sol", "save", "attack", "sol", "sol", "", "0", "0"};
    rows[4] = {"1", "2", "6", "2", "10", "16", "10", "14", "14", "12", "WIS", "jcn", "save", "attack", "jcn", "jcn", "jcn", "8", "1"};
    // No primaryabil: the ability the defaults favour most (DEX).
    rows[1] = {"1", "2", "8", "6", "12", "16", "12", "14", "12", "10", "", "sol", "save", "attack", "sol", "sol", "", "0", "0"};
    return makeTable(columns, rows);
}

class AutoLevel : public Test {
protected:
    NiceMock<MockStrings> strings;
    NiceMock<MockTwoDAs> twoDas;
    std::unique_ptr<Classes> classes;

    void SetUp() override {
        ON_CALL(twoDas, get("classes")).WillByDefault(Return(makeClasses()));
        // Skills: demolitions then awareness recommended for the soldier (both class skills),
        // computer use third (cross-class: two points a rank).
        ON_CALL(twoDas, get("skills")).WillByDefault(Return(makeTable(
            {"label", "sol_class", "sol_reco", "jcn_class", "jcn_reco"},
            {{"computeruse", "0", "3", "1", "1"},
             {"demolitions", "1", "1", "0", ""},
             {"stealth", "0", "", "0", ""},
             {"awareness", "1", "2", "1", "2"}})));
        // Feats: the soldier recommends f2 then f0 (f0 needs f2), grants f3 at class level 3.
        ON_CALL(twoDas, get("feat")).WillByDefault(Return(makeTable(
            {"label", "sol_list", "sol_granted", "sol_recom", "jcn_list", "jcn_granted", "jcn_recom"},
            {{"f0", "0", "-1", "2", "0", "-1", ""},
             {"f1", "3", "1", "", "4", "-1", ""},
             {"f2", "1", "-1", "1", "0", "-1", "1"},
             {"f3", "3", "3", "", "4", "", ""}})));
        ON_CALL(twoDas, get("featgain")).WillByDefault(Return(makeTable(
            {"label", "sol_reg", "jcn_reg"}, {{"1", "1", "1"}, {"2", "1", "0"}, {"3", "1", "1"}, {"4", "1", "0"}, {"5", "1", "0"}})));
        ON_CALL(twoDas, get("classpowergain")).WillByDefault(Return(makeTable(
            {"label", "jcn"}, {{"1", "2"}, {"2", "1"}, {"3", "1"}, {"4", "1"}, {"5", "2"}})));
        ON_CALL(twoDas, get("save")).WillByDefault(Return(makeTable({"level", "fortsave", "refsave", "willsave"}, {{"1", "0", "0", "0"}})));
        ON_CALL(twoDas, get("attack")).WillByDefault(Return(makeTable({"bab"}, {{"0"}})));
        classes = std::make_unique<Classes>(strings, twoDas);
    }
};

class FakeFeats : public IFeats {
public:
    std::set<FeatType> selectable;
    std::map<FeatType, FeatType> requires;
    std::map<FeatType, std::shared_ptr<Feat>> named;

    void init() override {}
    std::shared_ptr<Feat> get(FeatType type) const override {
        auto it = named.find(type);
        return it != named.end() ? it->second : nullptr;
    }
    int getLevelUpChoiceCount(const CreatureAttributes &attributes, const CreatureClass &clazz) const override {
        return clazz.getFeatGain(attributes.getClassLevel(clazz.type()) + 1);
    }
    bool isLevelUpCandidate(FeatType type, const CreatureAttributes &attributes, const CreatureClass &) const override {
        if (attributes.hasFeat(type) || selectable.count(type) == 0) {
            return false;
        }
        auto it = requires.find(type);
        return it == requires.end() || attributes.hasFeat(it->second);
    }
    std::vector<FeatType> getLevelUpCandidates(const CreatureAttributes &attributes, const CreatureClass &clazz) const override {
        std::vector<FeatType> result;
        for (auto type : selectable) {
            if (isLevelUpCandidate(type, attributes, clazz)) {
                result.push_back(type);
            }
        }
        return result;
    }
    std::vector<FeatDisplayEntry> getLevelUpDisplayEntries(const CreatureAttributes &, const CreatureClass &) const override { return {}; }
};

class FakeSpells : public ISpells {
public:
    std::map<SpellType, std::shared_ptr<Spell>> spells;

    std::shared_ptr<Spell> get(SpellType type) const override {
        auto it = spells.find(type);
        return it != spells.end() ? it->second : nullptr;
    }
    bool isLevelUpCandidate(SpellType, const CreatureAttributes &, const CreatureClass &, const std::set<SpellType> &) const override { return false; }
    std::vector<SpellType> getLevelUpCandidates(const CreatureAttributes &attributes, const CreatureClass &, const std::set<SpellType> &chosen) const override {
        std::vector<SpellType> result;
        for (const auto &[type, spell] : spells) {
            if (!chosen.count(type) && !attributes.hasSpell(type)) {
                result.push_back(type);
            }
        }
        return result;
    }
    std::vector<SpellDisplayEntry> getLevelUpDisplayEntries(const CreatureAttributes &, const CreatureClass &, const std::set<SpellType> &) const override { return {}; }
    void add(int id, int dark, int light) {
        auto spell = std::make_shared<Spell>();
        spell->darkRecommendation = dark;
        spell->lightRecommendation = light;
        spells[static_cast<SpellType>(id)] = spell;
    }
};

} // namespace

TEST_F(AutoLevel, the_ability_point_goes_to_the_primary_ability_else_the_favoured_one) {
    auto soldier = classes->get(ClassType::Soldier);
    auto consular = classes->get(ClassType::JediConsular);
    auto scout = classes->get(ClassType::Scout);
    ASSERT_TRUE(soldier && consular && scout);
    EXPECT_EQ(Ability::Strength, autoLevelAbility(*soldier));
    EXPECT_EQ(Ability::Wisdom, autoLevelAbility(*consular));
    EXPECT_EQ(Ability::Dexterity, autoLevelAbility(*scout));
}

TEST_F(AutoLevel, a_soldier_level_takes_the_recommended_skills_and_feats_in_order) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    feats.selectable = {static_cast<FeatType>(0), static_cast<FeatType>(2)};
    feats.requires[static_cast<FeatType>(0)] = static_cast<FeatType>(2);
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 1);
    attributes.setAbilityScore(Ability::Intelligence, 12); // +1: (2 + 1) / 2 = 1 point

    auto picks = autoLevelPicks(feats, spells, *soldier, attributes, false);
    EXPECT_EQ(ClassType::Soldier, picks.clazz);
    EXPECT_EQ(-1, picks.ability) << "level 2 is no fourth level";
    EXPECT_EQ(1, picks.skillPoints);
    EXPECT_THAT(picks.skills, ElementsAre(Pair(SkillType::Demolitions, 1)));
    EXPECT_EQ(1, picks.featChoices);
    EXPECT_THAT(picks.feats, ElementsAre(static_cast<FeatType>(2)));
    EXPECT_TRUE(picks.powers.empty());

    auto after = applyAutoLevelPicks(*soldier, attributes, picks);
    EXPECT_EQ(2, after.getAggregateLevel());
    EXPECT_EQ(1, after.getSkillRank(SkillType::Demolitions));
    EXPECT_TRUE(after.hasFeat(static_cast<FeatType>(2)));

    // The next level: f0 (its prerequisite now owned), the class's level-3 grant, more skill.
    auto third = autoLevelPicks(feats, spells, *soldier, after, false);
    EXPECT_THAT(third.feats, ElementsAre(static_cast<FeatType>(0)));
    auto level3 = applyAutoLevelPicks(*soldier, after, third);
    EXPECT_EQ(3, level3.getAggregateLevel());
    EXPECT_TRUE(level3.hasFeat(static_cast<FeatType>(3))) << "feat.2da sol_granted at class level 3";
    EXPECT_EQ(2, level3.getSkillRank(SkillType::Demolitions));

    // Same input, same level.
    auto again = autoLevelPicks(feats, spells, *soldier, attributes, false);
    EXPECT_EQ(picks.skills, again.skills);
    EXPECT_EQ(picks.feats, again.feats);
}

TEST_F(AutoLevel, skills_stop_at_the_rank_cap_and_cross_class_costs_two) {
    auto soldier = classes->get(ClassType::Soldier);
    FakeFeats feats;
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 1);
    attributes.setAbilityScore(Ability::Intelligence, 20); // +5: (2 + 5) / 2 = 3 points
    attributes.setSkillRank(SkillType::Demolitions, 5);    // the cap at level 2
    attributes.setSkillRank(SkillType::Awareness, 4);

    auto picks = autoLevelPicks(feats, spells, *soldier, attributes, false);
    EXPECT_EQ(3, picks.skillPoints);
    // Demolitions is full; awareness takes one to 5; computer use (cross-class, cap 2) takes one for two points.
    EXPECT_THAT(picks.skills, ElementsAre(Pair(SkillType::ComputerUse, 1), Pair(SkillType::Awareness, 1)));
    EXPECT_EQ(0, picks.skillPointsLeft);
}

TEST_F(AutoLevel, every_fourth_level_raises_the_primary_ability) {
    auto soldier = classes->get(ClassType::Soldier);
    FakeFeats feats;
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 3);
    attributes.setAbilityScore(Ability::Strength, 16);
    auto picks = autoLevelPicks(feats, spells, *soldier, attributes, false);
    EXPECT_EQ(static_cast<int>(Ability::Strength), picks.ability);
    EXPECT_EQ(17, applyAutoLevelPicks(*soldier, attributes, picks).getAbilityScore(Ability::Strength));
}

TEST_F(AutoLevel, a_consular_gains_the_classpowergain_count_in_the_alignment_order) {
    auto consular = classes->get(ClassType::JediConsular);
    ASSERT_TRUE(consular);
    FakeFeats feats;
    FakeSpells spells;
    spells.add(10, 0, 1); // light first
    spells.add(9, 1, 0);  // dark first
    spells.add(23, 2, 2);
    spells.add(5, 0, 0);  // unranked: never picked
    CreatureAttributes attributes;
    attributes.addClassLevels(consular.get(), 4);
    auto light = autoLevelPicks(feats, spells, *consular, attributes, false);
    EXPECT_EQ(2, light.powerChoices) << "classpowergain jcn level 5";
    EXPECT_THAT(light.powers, ElementsAre(static_cast<SpellType>(10), static_cast<SpellType>(23)));
    auto dark = autoLevelPicks(feats, spells, *consular, attributes, true);
    EXPECT_THAT(dark.powers, ElementsAre(static_cast<SpellType>(9), static_cast<SpellType>(23)));
    EXPECT_EQ(0, light.featChoices) << "featgain jcn_reg level 5 is 0";
    auto after = applyAutoLevelPicks(*consular, attributes, light);
    EXPECT_TRUE(after.hasSpell(static_cast<SpellType>(10)));
    EXPECT_TRUE(after.hasSpell(static_cast<SpellType>(23)));
    EXPECT_EQ(5, after.getClassLevel(ClassType::JediConsular));
}

// Self-directed level-ups (fork addition): what a guest seat may choose.

TEST_F(AutoLevel, level_up_options_give_the_numbers_the_host_will_check) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    feats.selectable = {static_cast<FeatType>(0), static_cast<FeatType>(2), static_cast<FeatType>(5)};
    feats.requires[static_cast<FeatType>(0)] = static_cast<FeatType>(2);
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 1);
    attributes.setAbilityScore(Ability::Intelligence, 12);
    attributes.setSkillRank(SkillType::Demolitions, 1);

    auto options = coopLevelUpOptions(feats, spells, *soldier, attributes, false);
    EXPECT_EQ(static_cast<int>(ClassType::Soldier), options.clazz);
    EXPECT_EQ(2, options.level);
    EXPECT_FALSE(options.abilityDue) << "level 2 is no fourth level";
    EXPECT_EQ(12, options.abilities[static_cast<int>(Ability::Intelligence)]);
    EXPECT_EQ(1, options.skillPoints) << "(2 + 1) / 2";
    ASSERT_EQ(8u, options.skills.size());
    for (const auto &skill : options.skills) {
        bool classSkill = soldier->isClassSkill(static_cast<SkillType>(skill.id));
        EXPECT_EQ(classSkill, skill.classSkill) << skill.id;
        EXPECT_EQ(classSkill ? 1 : 2, skill.cost) << skill.id;
        EXPECT_EQ(classSkill ? 5 : 2, skill.cap) << skill.id << ": level + 3, half for cross-class";
    }
    EXPECT_EQ(1, options.skills[static_cast<int>(SkillType::Demolitions)].rank);
    EXPECT_EQ(1, options.featChoices);
    // Feat 0 needs feat 2 first, so it is not a candidate yet.
    std::vector<int> candidates;
    for (const auto &pick : options.feats) {
        candidates.push_back(pick.id);
    }
    EXPECT_THAT(candidates, UnorderedElementsAre(2, 5));
    EXPECT_EQ(0, options.powerChoices);
    EXPECT_TRUE(options.powers.empty());

    // Recommended is what the game's own auto-level takes.
    auto picks = autoLevelPicks(feats, spells, *soldier, attributes, false);
    ASSERT_EQ(picks.feats.size(), options.recommended.feats.size());
    EXPECT_EQ(static_cast<int>(picks.feats[0]), options.recommended.feats[0]);
    EXPECT_EQ(picks.ability, options.recommended.ability);
    EXPECT_EQ(options.clazz, options.recommended.clazz);
}

TEST_F(AutoLevel, level_up_options_offer_an_ability_point_only_on_a_fourth_level) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 3);
    EXPECT_TRUE(coopLevelUpOptions(feats, spells, *soldier, attributes, false).abilityDue);
    attributes.addClassLevels(soldier.get(), 1);
    EXPECT_FALSE(coopLevelUpOptions(feats, spells, *soldier, attributes, false).abilityDue);
}

TEST_F(AutoLevel, level_up_options_are_one_line_of_json_with_a_choice_the_decoder_accepts) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    feats.selectable = {static_cast<FeatType>(2)};
    FakeSpells spells;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 1);

    auto options = coopLevelUpOptions(feats, spells, *soldier, attributes, false);
    options.feats[0].name = "Power \"Attack\"";
    options.feats[0].description = std::string(900, 'x');
    std::string line = coopLevelUpOptionsJson(options);
    EXPECT_EQ(std::string::npos, line.find(static_cast<char>(10))) << "one line";
    for (const char *key : {"\"class\":", "\"level\":2", "\"ability\":", "\"skills\":", "\"points\":", "\"feats\":",
                            "\"powers\":", "\"candidates\":", "\"recommended\":", "\"name\":\"demolitions\""}) {
        EXPECT_NE(std::string::npos, line.find(key)) << key;
    }
    EXPECT_NE(std::string::npos, line.find("Power \\\"Attack\\\"")) << "names are escaped";
    EXPECT_EQ(std::string::npos, line.find(std::string(301, 'x'))) << "descriptions are cut";
    CoopLevelUpChoice back;
    EXPECT_TRUE(decodeCoopLevelUp(encodeCoopLevelUp(options.recommended), back));
    EXPECT_EQ(options.recommended, back);
}

// Self-directed creation (fork addition): a new character's skills and first feat, by the creation screens' rules.

TEST_F(AutoLevel, creation_options_are_the_creation_screens_numbers) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    feats.selectable = {static_cast<FeatType>(0), static_cast<FeatType>(2), static_cast<FeatType>(5)};
    feats.requires[static_cast<FeatType>(0)] = static_cast<FeatType>(2);
    const CreatureAttributes &start = soldier->defaultAttributes();

    auto options = coopCreationOptions(feats, *soldier, start);
    EXPECT_EQ(static_cast<int>(ClassType::Soldier), options.clazz);
    EXPECT_EQ(1, options.level);
    EXPECT_FALSE(options.abilityDue);
    EXPECT_EQ(4, options.skillPoints) << "four times a level's (2 + 0) / 2";
    ASSERT_EQ(8u, options.skills.size());
    for (const auto &skill : options.skills) {
        bool classSkill = soldier->isClassSkill(static_cast<SkillType>(skill.id));
        EXPECT_EQ(0, skill.rank);
        EXPECT_EQ(classSkill ? 1 : 2, skill.cost) << skill.id;
        EXPECT_EQ(classSkill ? 4 : 2, skill.cap) << skill.id << ": a first-level character's caps";
    }
    EXPECT_EQ(1, options.featChoices) << "the class's first-level gain";
    std::vector<int> candidates;
    for (const auto &pick : options.feats) {
        candidates.push_back(pick.id);
    }
    EXPECT_THAT(candidates, UnorderedElementsAre(2, 5));
    EXPECT_EQ(0, options.powerChoices);

    // Recommended is what the screens' Recommended buttons choose: the first recommended skill to its cap, the first recommended feat.
    EXPECT_EQ(-1, options.recommended.ability);
    EXPECT_EQ(4, options.recommended.skills[static_cast<int>(SkillType::Demolitions)]);
    EXPECT_EQ(1u, options.recommended.skills.size());
    ASSERT_EQ(1u, options.recommended.feats.size());
    EXPECT_EQ(2, options.recommended.feats[0]);
    EXPECT_NE(std::string::npos, coopLevelUpOptionsJson(options).find("\"level\":1"));

    // And the recommended choice is one the rules accept.
    CreatureAttributes made(start);
    EXPECT_EQ("", applyCoopCreationChoice(feats, *soldier, options.recommended, made));
    EXPECT_EQ(4, made.getSkillRank(SkillType::Demolitions));
}

TEST_F(AutoLevel, a_creation_choice_is_applied_only_within_the_creation_rules) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    feats.selectable = {static_cast<FeatType>(0), static_cast<FeatType>(2), static_cast<FeatType>(5)};
    feats.requires[static_cast<FeatType>(0)] = static_cast<FeatType>(2);
    const CreatureAttributes &start = soldier->defaultAttributes();

    CoopLevelUpChoice choice;
    choice.clazz = static_cast<int>(ClassType::Soldier);
    choice.skills = {{static_cast<int>(SkillType::Awareness), 2}, {static_cast<int>(SkillType::ComputerUse), 1}}; // 2 + 2 points
    choice.feats = {5};
    CreatureAttributes made(start);
    EXPECT_EQ("", applyCoopCreationChoice(feats, *soldier, choice, made));
    EXPECT_EQ(2, made.getSkillRank(SkillType::Awareness));
    EXPECT_EQ(1, made.getSkillRank(SkillType::ComputerUse));
    EXPECT_TRUE(made.hasFeat(static_cast<FeatType>(5)));
    EXPECT_EQ(1, made.getAggregateLevel()) << "still a first-level character";
    EXPECT_EQ(start.getAbilityScore(Ability::Strength), made.getAbilityScore(Ability::Strength));

    auto refused = [&](CoopLevelUpChoice bad, const CreatureAttributes &from) {
        CreatureAttributes attributes(from);
        auto why = applyCoopCreationChoice(feats, *soldier, bad, attributes);
        EXPECT_EQ(from.getSkillRank(SkillType::Awareness), attributes.getSkillRank(SkillType::Awareness)) << "a refusal changes nothing";
        EXPECT_EQ(from.feats(), attributes.feats()) << "a refusal changes nothing";
        return why;
    };
    auto with = [&](auto change) {
        CoopLevelUpChoice bad(choice);
        change(bad);
        return bad;
    };
    EXPECT_NE("", refused(with([](auto &c) { c.skills[static_cast<int>(SkillType::Demolitions)] = 1; }), start)) << "five points of four";
    EXPECT_NE("", refused(with([](auto &c) { c.feats = {0}; }), start)) << "a feat that needs another first";
    EXPECT_NE("", refused(with([](auto &c) { c.feats = {2, 5}; }), start)) << "two feats where one is chosen";
    EXPECT_NE("", refused(with([](auto &c) { c.feats = {1}; }), start)) << "a feat the class already gave";
    EXPECT_NE("", refused(with([](auto &c) { c.ability = 0; }), start)) << "no ability point at creation";
    EXPECT_NE("", refused(with([](auto &c) { c.powers = {1}; }), start)) << "no power at creation";
    EXPECT_NE("", refused(with([](auto &c) { c.clazz = static_cast<int>(ClassType::JediConsular); }), start)) << "another class";
    EXPECT_NE("", refused(with([](auto &c) { c.skills = {{99, 1}}; }), start)) << "not a skill";
    // With points to spare, a cap still holds: a bright soldier may not pass rank 2 in a cross-class skill.
    CreatureAttributes bright(start);
    bright.setAbilityScore(Ability::Intelligence, 18);
    EXPECT_EQ(12, coopCreationOptions(feats, *soldier, bright).skillPoints);
    EXPECT_NE("", refused(with([](auto &c) { c.skills = {{static_cast<int>(SkillType::ComputerUse), 3}}; }), bright));
    EXPECT_NE("", refused(with([](auto &c) { c.skills = {{static_cast<int>(SkillType::Awareness), 5}}; }), bright));
    CoopLevelUpChoice wide(choice);
    wide.skills = {{static_cast<int>(SkillType::Awareness), 4}, {static_cast<int>(SkillType::ComputerUse), 2}};
    CreatureAttributes widened(bright);
    EXPECT_EQ("", applyCoopCreationChoice(feats, *soldier, wide, widened));
    EXPECT_EQ(4, widened.getSkillRank(SkillType::Awareness));
}

// Self-knowledge (fork addition): what a character is and can do, in the game's own names.

TEST_F(AutoLevel, own_sheet_names_what_the_character_holds_and_only_that) {
    auto soldier = classes->get(ClassType::Soldier);
    ASSERT_TRUE(soldier);
    FakeFeats feats;
    auto twoWeapon = std::make_shared<Feat>();
    twoWeapon->name = "Two-Weapon \"Fighting\"";
    twoWeapon->description = std::string(900, 'x');
    feats.named[static_cast<FeatType>(3)] = twoWeapon;
    auto notHeld = std::make_shared<Feat>();
    notHeld->name = "Sniper Shot";
    feats.named[static_cast<FeatType>(9)] = notHeld;
    feats.named[static_cast<FeatType>(4)] = std::make_shared<Feat>(); // no name: never shown to a player
    FakeSpells spells;
    auto push = std::make_shared<Spell>();
    push->name = "Force Push";
    push->description = "Throws a target back.";
    spells.spells[static_cast<SpellType>(23)] = push;
    CreatureAttributes attributes;
    attributes.addClassLevels(soldier.get(), 3);
    attributes.setAbilityScore(Ability::Intelligence, 14);
    attributes.setSkillRank(SkillType::ComputerUse, 6);
    attributes.addFeat(static_cast<FeatType>(3));
    attributes.addFeat(static_cast<FeatType>(4));
    attributes.addSpell(static_cast<SpellType>(23));
    CoopOwnSheetState state;
    state.skillValues[0] = 8;
    state.skillUntrained[1] = false;
    state.hitPoints = 21;
    state.maxHitPoints = 30;

    std::string line = coopOwnSheetJson(feats, spells, attributes, state);
    EXPECT_EQ(std::string::npos, line.find(static_cast<char>(10))) << "one line";
    for (const char *key : {"\"level\":3", "\"name\":\"intelligence\",\"score\":14,\"modifier\":2",
                            "\"name\":\"computeruse\",\"rank\":6,\"value\":8,\"untrained\":true",
                            "\"name\":\"demolitions\",\"rank\":0,\"value\":0,\"untrained\":false",
                            "Two-Weapon \\\"Fighting\\\"", "\"name\":\"Force Push\"", "\"hp\":21", "\"maxHp\":30"}) {
        EXPECT_NE(std::string::npos, line.find(key)) << key;
    }
    EXPECT_EQ(std::string::npos, line.find("Sniper Shot")) << "a feat it does not hold is not listed";
    EXPECT_EQ(std::string::npos, line.find("alignment")) << "a guest's copy is not sent its alignment, so none is printed";
    EXPECT_EQ(std::string::npos, line.find("\"id\":4,\"name\":\"\"")) << "a feat without a name is left out";
    EXPECT_EQ(std::string::npos, line.find(std::string(201, 'x'))) << "descriptions are cut";
}

TEST(CoopCutText, never_ends_inside_a_character) {
    EXPECT_EQ("abc", coopCutText("abc", 300));
    EXPECT_EQ("abcd", coopCutText("abcdef", 4));
    // U+00E9 is two bytes: a cut that would land between them backs off one.
    std::string accented = std::string("ab") + static_cast<char>(0xC3) + static_cast<char>(0xA9) + "z";
    EXPECT_EQ("ab", coopCutText(accented, 3));
    EXPECT_EQ(accented.substr(0, 4), coopCutText(accented, 4));
}
