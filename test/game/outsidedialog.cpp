/*
 * Fork addition (KennyG1990/kotor-coop-player2). GPL-3.0-or-later.
 */

#include <gtest/gtest.h>

#include "reone/game/outsidedialog.h"

using namespace reone::game;
using reone::resource::Dialog;

namespace {

const char *kExchange = R"({
  "line": "You pulled me out of that wreck. I have not forgotten.",
  "playerTag": "player",
  "options": [
    {"label": "Ask about their past", "line": "You never talk about where you came from. What happened before the ship?"},
    {"label": "[Persuade] Come with me to the lower city", "line": "I need you down there. Trust me on this.", "check": "k_con_persmed"},
    {"label": "[Force Persuade] You will hand me the datapad", "line": "You want to give me the datapad.", "showIf": "k_con_fperslow"},
    {"label": "Say something else", "line": "", "end": true},
    {"label": "That is all for now.", "line": "That is all for now.", "end": true}
  ]
})";

} // namespace

TEST(OutsideDialog, an_exchange_becomes_an_ordinary_dialog) {
    auto spec = parseOutsideDialog(kExchange);
    ASSERT_EQ(5u, spec.options.size());
    auto built = buildOutsideDialog(spec, "p2talk");
    const auto &dialog = *built.dialog;

    EXPECT_EQ("p2talk", dialog.resRef);
    ASSERT_EQ(1u, dialog.startEntries.size());
    EXPECT_EQ(0, dialog.startEntries[0].index);
    // The owner's line offers every option, in order, as the reply list rows.
    const auto &line = dialog.entries[0];
    EXPECT_EQ("You pulled me out of that wreck. I have not forgotten.", line.text);
    EXPECT_TRUE(line.speaker.empty()); // the owner of the conversation speaks
    ASSERT_EQ(5u, line.replies.size());
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(i, line.replies[i].index);
        EXPECT_EQ(spec.options[i].label, dialog.replies[i].text);
    }
    EXPECT_EQ(OutsideDialogNode::Kind::Line, built.nodes[0].kind);
    EXPECT_EQ(OutsideDialogNode::Kind::Waiting, built.nodes[1].kind);
    EXPECT_EQ(built.nodes.size(), dialog.entries.size());
}

TEST(OutsideDialog, picking_an_option_makes_the_players_character_say_the_full_line) {
    auto built = buildOutsideDialog(parseOutsideDialog(kExchange), "p2talk");
    const auto &dialog = *built.dialog;
    ASSERT_EQ(1u, dialog.replies[0].entries.size());
    int said = dialog.replies[0].entries[0].index;
    EXPECT_EQ("You never talk about where you came from. What happened before the ship?", dialog.entries[said].text);
    EXPECT_EQ("player", dialog.entries[said].speaker);
    EXPECT_EQ(OutsideDialogNode::Kind::Said, built.nodes[said].kind);
    EXPECT_EQ(0, built.nodes[said].option);
    EXPECT_FALSE(built.nodes[said].checked);
    // Then the screen is held: an empty reply, which the screen takes by itself, leads to the waiting node.
    ASSERT_EQ(1u, dialog.entries[said].replies.size());
    const auto &carryOn = dialog.replies[dialog.entries[said].replies[0].index];
    EXPECT_TRUE(carryOn.text.empty());
    ASSERT_EQ(1u, carryOn.entries.size());
    EXPECT_EQ(1, carryOn.entries[0].index);
    EXPECT_EQ("...", dialog.entries[1].text);
    EXPECT_EQ(45, dialog.entries[1].delay);
    EXPECT_TRUE(dialog.entries[1].replies.empty()); // nothing follows: the next exchange is opened from outside
}

TEST(OutsideDialog, a_check_is_the_games_own_script_with_the_passed_branch_first) {
    auto built = buildOutsideDialog(parseOutsideDialog(kExchange), "p2talk");
    const auto &dialog = *built.dialog;
    const auto &persuade = dialog.replies[1];
    ASSERT_EQ(2u, persuade.entries.size());
    EXPECT_EQ("k_con_persmed", persuade.entries[0].active); // the game decides
    EXPECT_TRUE(persuade.entries[1].active.empty());        // what plays when it does not pass
    int passed = persuade.entries[0].index;
    int failed = persuade.entries[1].index;
    EXPECT_NE(passed, failed);
    EXPECT_EQ(dialog.entries[passed].text, dialog.entries[failed].text); // the same words either way
    EXPECT_TRUE(built.nodes[passed].checked);
    EXPECT_TRUE(built.nodes[passed].passed);
    EXPECT_TRUE(built.nodes[failed].checked);
    EXPECT_FALSE(built.nodes[failed].passed);
    EXPECT_EQ(1, built.nodes[failed].option);
}

TEST(OutsideDialog, an_option_can_be_offered_only_when_the_games_script_allows_it) {
    auto built = buildOutsideDialog(parseOutsideDialog(kExchange), "p2talk");
    const auto &line = built.dialog->entries[0];
    EXPECT_TRUE(line.replies[0].active.empty());
    EXPECT_EQ("k_con_fperslow", line.replies[2].active);
}

TEST(OutsideDialog, an_ending_option_ends_after_its_line_and_an_empty_line_stands_only_a_moment) {
    auto built = buildOutsideDialog(parseOutsideDialog(kExchange), "p2talk");
    const auto &dialog = *built.dialog;
    int typed = dialog.replies[3].entries[0].index;
    EXPECT_TRUE(dialog.entries[typed].replies.empty());
    EXPECT_EQ(1, dialog.entries[typed].delay);
    EXPECT_TRUE(built.nodes[typed].end);
    EXPECT_EQ(3, built.nodes[typed].option);
    int bye = dialog.replies[4].entries[0].index;
    EXPECT_TRUE(dialog.entries[bye].replies.empty());
    EXPECT_GE(dialog.entries[bye].delay, 2);
}

TEST(OutsideDialog, a_silent_line_is_given_time_to_be_read) {
    OutsideDialogSpec spec;
    spec.line = std::string(400, 'a');
    spec.options.push_back({"Go on", "Go on.", "", "", false});
    auto built = buildOutsideDialog(spec, "p2talk");
    EXPECT_EQ(14, built.dialog->entries[0].delay); // capped
    spec.line = "Yes.";
    EXPECT_EQ(2, buildOutsideDialog(spec, "p2talk").dialog->entries[0].delay);
}

TEST(OutsideDialog, text_from_outside_cannot_use_the_engines_tokens_or_break_the_line) {
    auto spec = parseOutsideDialog(R"({"line": "Hello <FullName>,\nwell <CUSTOM12> met.", "options": [{"label": "<b>Hi</b>", "line": "Hi."}]})");
    EXPECT_EQ("Hello FullName, well CUSTOM12 met.", spec.line);
    EXPECT_EQ("bHi/b", spec.options[0].label);
}

TEST(OutsideDialog, only_the_games_conversation_checks_can_be_named) {
    EXPECT_TRUE(isConversationCheckName("k_con_persmed"));
    EXPECT_TRUE(isConversationCheckName("k_con_fpershigh"));
    EXPECT_FALSE(isConversationCheckName("k_pman_persuad_f"));
    EXPECT_FALSE(isConversationCheckName("k_con_"));
    EXPECT_FALSE(isConversationCheckName("k_con_Persmed"));
    EXPECT_FALSE(isConversationCheckName("k_con_a_name_that_is_long"));
    EXPECT_FALSE(isConversationCheckName("k_con_pers;quit"));
    EXPECT_THROW(parseOutsideDialog(R"({"line": "x", "options": [{"label": "a", "line": "b", "check": "end_game"}]})"), std::invalid_argument);
}

TEST(OutsideDialog, a_broken_request_is_refused_with_a_reason) {
    for (const char *bad : {"", "[]", "{}", R"({"line": ""})", R"({"line": "x"})", R"({"line": "x", "options": []})",
                            R"({"line": "x", "options": [{"line": "no label"}]})", R"({"line": "x", "options": ["a"]})",
                            R"({"line": 4, "options": [{"label": "a"}]})"}) {
        EXPECT_THROW(parseOutsideDialog(bad), std::invalid_argument) << bad;
    }
    std::string many = R"({"line": "x", "options": [)";
    for (int i = 0; i < 9; ++i) {
        many += std::string(i ? "," : "") + R"({"label": "a", "line": "b"})";
    }
    EXPECT_THROW(parseOutsideDialog(many + "]}"), std::invalid_argument);
}

TEST(OutsideDialog, long_text_is_cut_without_splitting_a_character) {
    std::string line(kOutsideDialogMaxLine - 1, 'a');
    line += "\xC3\xA9\xC3\xA9"; // two two-byte characters across the limit
    auto spec = parseOutsideDialog(std::string(R"({"line": ")") + line + R"(", "options": [{"label": "a", "line": "b"}]})");
    EXPECT_EQ(kOutsideDialogMaxLine - 1, spec.line.size());
}

TEST(OutsideDialog, base64_carries_the_request_through_the_console) {
    EXPECT_EQ("{\"line\": \"x\"}", decodeBase64("eyJsaW5lIjogIngifQ=="));
    EXPECT_EQ("", decodeBase64(""));
    EXPECT_EQ("\xC3\xA9", decodeBase64("w6k="));
    EXPECT_THROW(decodeBase64("not base64!"), std::invalid_argument);
}
