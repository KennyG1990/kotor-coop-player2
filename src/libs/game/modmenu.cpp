/* Copyright (c) 2026 The reone project contributors */

#include "reone/game/modmenu.h"

#include <stdexcept>

#include <boost/json.hpp>

namespace reone {

namespace game {

namespace {

constexpr size_t kButtonLength = 24;
constexpr size_t kTitleLength = 40;
constexpr size_t kTextLength = 2000;
constexpr size_t kRowLength = 64;
constexpr size_t kBackLength = 16;

std::string text(const boost::json::object &object, const char *key, size_t limit, bool required) {
    auto found = object.find(key);
    if (found == object.end() || found->value().is_null()) {
        if (required) {
            throw std::invalid_argument(std::string("modmenu: \"") + key + "\" is missing");
        }
        return std::string();
    }
    if (!found->value().is_string()) {
        throw std::invalid_argument(std::string("modmenu: \"") + key + "\" must be text");
    }
    std::string value(found->value().as_string().c_str());
    if (required && value.empty()) {
        throw std::invalid_argument(std::string("modmenu: \"") + key + "\" is empty");
    }
    if (value.size() > limit) {
        throw std::invalid_argument(std::string("modmenu: \"") + key + "\" is longer than " + std::to_string(limit));
    }
    return value;
}

} // namespace

ModMenuSpec ModMenuModel::parse(const std::string &json) {
    boost::json::value root;
    try {
        root = boost::json::parse(json);
    } catch (const std::exception &) {
        throw std::invalid_argument("modmenu: not JSON");
    }
    if (!root.is_object()) {
        throw std::invalid_argument("modmenu: not a JSON object");
    }
    const auto &object = root.as_object();
    ModMenuSpec spec;
    spec.button = text(object, "button", kButtonLength, true);
    spec.title = text(object, "title", kTitleLength, true);
    spec.text = text(object, "text", kTextLength, false);
    auto back = text(object, "back", kBackLength, false);
    if (!back.empty()) {
        spec.back = back;
    }
    auto rows = object.find("rows");
    if (rows == object.end() || !rows->value().is_array()) {
        throw std::invalid_argument("modmenu: \"rows\" must be a list");
    }
    const auto &list = rows->value().as_array();
    if (list.empty() || list.size() > kRows) {
        throw std::invalid_argument("modmenu: one to five rows");
    }
    for (const auto &row : list) {
        if (!row.is_string() || row.as_string().empty() || row.as_string().size() > kRowLength) {
            throw std::invalid_argument("modmenu: each row is text of 1 to " + std::to_string(kRowLength) + " characters");
        }
        spec.rows.emplace_back(row.as_string().c_str());
    }
    return spec;
}

void ModMenuModel::set(ModMenuSpec spec) {
    _spec = std::move(spec);
    _active = true;
    ++_version;
}

void ModMenuModel::clear() {
    _spec = ModMenuSpec();
    _active = false;
    _picks.clear();
    ++_version;
}

void ModMenuModel::pick(size_t row) {
    if (!_active || row >= _spec.rows.size()) {
        return;
    }
    _picks.push_back(static_cast<int>(row));
    while (_picks.size() > kPicksKept) {
        _picks.pop_front();
    }
}

std::vector<int> ModMenuModel::takePicks() {
    std::vector<int> taken(_picks.begin(), _picks.end());
    _picks.clear();
    return taken;
}

} // namespace game

} // namespace reone
