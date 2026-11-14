/*
 * Fork addition (KennyG1990/kotor-coop-player2). GPL-3.0-or-later, as the rest
 * of this engine.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "reone/resource/dialog.h"

namespace reone {

namespace game {

/**
 * A conversation driven from outside the game, shown on the game's own
 * conversation screen.
 *
 * A program beside the game (a companion that thinks elsewhere) supplies one
 * line for a character to say and the options the player may answer with. The
 * engine turns that into an ordinary dialog and runs it on the standard
 * screen: the same camera, the same reply list, the same rules.
 *
 * An option is a short label (what the reply list shows) and the full line the
 * player's character then says. An option may carry the name of one of the
 * game's own conversation checks:
 *   - check:  the game's script decides whether the attempt works, as it does
 *             for a [Persuade] reply (k_con_perseasy, k_con_persmed,
 *             k_con_pershigh). Nothing here re-implements the rule.
 *   - showIf: the option is offered only when the game's script says the
 *             player's character can attempt it, as for [Force Persuade]
 *             (k_con_fperslow, k_con_fpershigh).
 * After the player's line a waiting node holds the screen until the outside
 * program opens the next exchange, unless the option ends the conversation.
 *
 * Nothing in the engine starts such a conversation by itself.
 */
struct OutsideDialogOption {
    std::string label;   // shown in the reply list
    std::string line;    // what the player's character says; may be empty
    std::string check;   // a k_con_* condition script, or empty
    std::string showIf;  // a k_con_* condition script, or empty
    bool end {false};    // the conversation ends after the line
};

struct OutsideDialogSpec {
    std::string line;       // what the owner of the conversation says
    std::string playerTag;  // tag of the player's character, so their lines are theirs on screen; may be empty
    std::string waiting {"..."}; // shown while the outside program prepares the answer
    int holdSeconds {45};   // how long the waiting node holds the screen
    int readSeconds {0};    // how long the owner's line holds before the options; 0: by its length, as before
    float mouthSpeed {1.0f}; // how fast the owner's mouth moves on a line with no voice; 1: as the game plays it
    std::vector<OutsideDialogOption> options;
};

/** What a node of a built dialog is, by entry index. */
struct OutsideDialogNode {
    enum class Kind {
        None,
        Line,    // the owner's line; the options are offered after it
        Said,    // the player's character says the chosen option's full line
        Waiting  // the screen is held for the next exchange
    };
    Kind kind {Kind::None};
    int option {-1};      // which option led here (Said)
    bool checked {false}; // the option carried a check
    bool passed {false};  // and the game's script let it pass
    bool end {false};     // the conversation ends after this node
};

struct OutsideDialog {
    std::shared_ptr<resource::Dialog> dialog;
    std::vector<OutsideDialogNode> nodes; // by entry index
    float mouthSpeed {1.0f};              // as the outside program asked; 1 when it did not
};

/** Limits of what is accepted, so a broken sender cannot flood the screen. */
constexpr int kOutsideDialogMaxOptions = 8;
constexpr size_t kOutsideDialogMaxLine = 700;
constexpr size_t kOutsideDialogMaxLabel = 120;

/**
 * Parses the JSON an outside program sends:
 * {"line": "...", "playerTag": "...", "waiting": "...", "hold": 45, "read": 20, "mouth": 0.5,
 *  "options": [{"label": "...", "line": "...", "check": "k_con_persmed",
 *               "showIf": "k_con_fperslow", "end": false}, ...]}
 * Throws std::invalid_argument with a plain reason.
 */
OutsideDialogSpec parseOutsideDialog(const std::string &json);

/** Whether a name is one of the game's conversation condition scripts (k_con_*), by its form. */
bool isConversationCheckName(const std::string &name);

/** Builds the dialog. resRef is the name the engine reports while it runs. */
OutsideDialog buildOutsideDialog(const OutsideDialogSpec &spec, const std::string &resRef);

/** Decodes standard base64 (the console takes one word, so the JSON arrives encoded). Throws std::invalid_argument. */
std::string decodeBase64(const std::string &text);

const char *outsideDialogKindName(OutsideDialogNode::Kind kind);

} // namespace game

} // namespace reone
