/*
 * Fork addition (KennyG1990/kotor-coop-player2). GPL-3.0-or-later, as the rest
 * of this engine.
 */

#include "reone/game/outsidedialog.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <boost/json.hpp>

namespace reone {

namespace game {

namespace {

using resource::Dialog;

// The engine substitutes <tokens> in dialog text. Text from outside is shown as written.
std::string plainText(const boost::json::value *value, size_t limit, const char *what) {
    if (!value || !value->is_string()) {
        throw std::invalid_argument(std::string(what) + " is missing");
    }
    std::string text(value->as_string().c_str());
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '<' || c == '>') {
            continue;
        }
        out += (c == '\n' || c == '\r' || c == '\t') ? ' ' : c;
    }
    if (out.size() > limit) {
        size_t end = limit;
        while (end > 0 && (static_cast<unsigned char>(out[end]) & 0xC0) == 0x80) {
            --end; // never end inside a character
        }
        out.resize(end);
    }
    return out;
}

std::string optionalText(const boost::json::object &object, const char *key, size_t limit) {
    auto found = object.if_contains(key);
    if (!found || found->is_null()) {
        return "";
    }
    return plainText(found, limit, key);
}

std::string checkName(const boost::json::object &object, const char *key) {
    auto name = optionalText(object, key, 32);
    if (!name.empty() && !isConversationCheckName(name)) {
        throw std::invalid_argument(std::string(key) + " is not one of the game's conversation checks");
    }
    return name;
}

Dialog::EntryReplyLink link(int index, const std::string &active = "") {
    Dialog::EntryReplyLink result;
    result.index = index;
    result.active = active;
    return result;
}

// A silent line has no voice to time it: long enough to read, never shorter than a glance.
int readingSeconds(const std::string &text) {
    return std::clamp(static_cast<int>(std::lround(1.5 + text.size() / 16.0)), 2, 14);
}

} // namespace

bool isConversationCheckName(const std::string &name) {
    if (name.size() < 7 || name.size() > 16 || name.compare(0, 6, "k_con_") != 0) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

OutsideDialogSpec parseOutsideDialog(const std::string &json) {
    boost::json::value root;
    try {
        root = boost::json::parse(json);
    } catch (const std::exception &) {
        throw std::invalid_argument("that is not JSON");
    }
    if (!root.is_object()) {
        throw std::invalid_argument("the dialog must be a JSON object");
    }
    const auto &object = root.as_object();
    OutsideDialogSpec spec;
    spec.line = plainText(object.if_contains("line"), kOutsideDialogMaxLine, "line");
    if (spec.line.empty()) {
        throw std::invalid_argument("line is empty");
    }
    spec.playerTag = optionalText(object, "playerTag", 32);
    auto waiting = optionalText(object, "waiting", 60);
    if (!waiting.empty()) {
        spec.waiting = waiting;
    }
    if (auto hold = object.if_contains("hold"); hold && hold->is_number()) {
        spec.holdSeconds = std::clamp(static_cast<int>(hold->to_number<double>()), 5, 120);
    }
    auto options = object.if_contains("options");
    if (!options || !options->is_array() || options->as_array().empty()) {
        throw std::invalid_argument("options are missing");
    }
    if (static_cast<int>(options->as_array().size()) > kOutsideDialogMaxOptions) {
        throw std::invalid_argument("too many options");
    }
    for (const auto &value : options->as_array()) {
        if (!value.is_object()) {
            throw std::invalid_argument("an option is not an object");
        }
        const auto &item = value.as_object();
        OutsideDialogOption option;
        option.label = plainText(item.if_contains("label"), kOutsideDialogMaxLabel, "label");
        if (option.label.empty()) {
            throw std::invalid_argument("an option has no label");
        }
        option.line = optionalText(item, "line", kOutsideDialogMaxLine);
        option.check = checkName(item, "check");
        option.showIf = checkName(item, "showIf");
        if (auto end = item.if_contains("end"); end && end->is_bool()) {
            option.end = end->as_bool();
        }
        spec.options.push_back(std::move(option));
    }
    return spec;
}

OutsideDialog buildOutsideDialog(const OutsideDialogSpec &spec, const std::string &resRef) {
    OutsideDialog built;
    auto dialog = std::make_shared<Dialog>();
    dialog->resRef = resRef;
    dialog->skippable = true;

    // Entry 0: the owner's line. Entry 1: the waiting node. Then, per option, the player's
    // line: one entry, or two when the game's check decides (passed first, as the game's own
    // dialogs order them: the first entry whose condition holds is the one that plays).
    Dialog::EntryReply first;
    first.text = spec.line;
    first.delay = readingSeconds(spec.line);
    Dialog::EntryReply waiting;
    waiting.text = spec.waiting;
    waiting.delay = spec.holdSeconds;
    dialog->entries.push_back(std::move(first));
    dialog->entries.push_back(std::move(waiting));
    built.nodes.push_back({OutsideDialogNode::Kind::Line, -1, false, false, false});
    built.nodes.push_back({OutsideDialogNode::Kind::Waiting, -1, false, false, false});

    // Reply n (after the options): an empty reply, which the screen takes by itself, leading to the waiting node.
    int autoReply = static_cast<int>(spec.options.size());
    for (size_t i = 0; i < spec.options.size(); ++i) {
        const auto &option = spec.options[i];
        Dialog::EntryReply reply;
        reply.text = option.label;
        auto said = [&](bool checked, bool passed) {
            Dialog::EntryReply entry;
            entry.speaker = spec.playerTag;
            entry.text = option.line;
            // An empty line still stands for a moment, so that whoever opened the conversation can see it was chosen.
            entry.delay = option.line.empty() ? 1 : readingSeconds(option.line);
            if (!option.end) {
                entry.replies.push_back(link(autoReply));
            }
            dialog->entries.push_back(std::move(entry));
            built.nodes.push_back({OutsideDialogNode::Kind::Said, static_cast<int>(i), checked, passed, option.end});
            return static_cast<int>(dialog->entries.size()) - 1;
        };
        if (option.check.empty()) {
            reply.entries.push_back(link(said(false, false)));
        } else {
            reply.entries.push_back(link(said(true, true), option.check));
            reply.entries.push_back(link(said(true, false)));
        }
        dialog->replies.push_back(std::move(reply));
        dialog->entries[0].replies.push_back(link(static_cast<int>(i), option.showIf));
    }
    Dialog::EntryReply carryOn;
    carryOn.entries.push_back(link(1));
    dialog->replies.push_back(std::move(carryOn));

    dialog->startEntries.push_back(link(0));
    built.dialog = std::move(dialog);
    return built;
}

std::string decodeBase64(const std::string &text) {
    std::string out;
    int value = 0;
    int bits = -8;
    for (unsigned char c : text) {
        int digit;
        if (c >= 'A' && c <= 'Z') {
            digit = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            digit = c - 'a' + 26;
        } else if (c >= '0' && c <= '9') {
            digit = c - '0' + 52;
        } else if (c == '+' || c == '-') {
            digit = 62;
        } else if (c == '/' || c == '_') {
            digit = 63;
        } else if (c == '=') {
            break;
        } else {
            throw std::invalid_argument("that is not base64");
        }
        value = (value << 6) | digit;
        bits += 6;
        if (bits >= 0) {
            out += static_cast<char>((value >> bits) & 0xFF);
            bits -= 8;
        }
    }
    return out;
}

const char *outsideDialogKindName(OutsideDialogNode::Kind kind) {
    switch (kind) {
    case OutsideDialogNode::Kind::Line:
        return "line";
    case OutsideDialogNode::Kind::Said:
        return "said";
    case OutsideDialogNode::Kind::Waiting:
        return "waiting";
    default:
        return "none";
    }
}

} // namespace game

} // namespace reone
