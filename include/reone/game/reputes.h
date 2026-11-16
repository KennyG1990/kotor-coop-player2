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

#pragma once

#include "types.h"

#include <set>
#include <utility>

namespace reone {

namespace resource {

class ITwoDAs;
class Gff;
class TwoDA;

}

namespace game {

class Creature;

class IReputes {
public:
    struct FactionDefinition {
        std::string name;
        uint32_t parentId {std::numeric_limits<uint32_t>::max()};
        bool global {true};
    };

    struct State {
        std::vector<FactionDefinition> factions;
        std::vector<std::string> labels;
        std::vector<std::vector<int>> values;
    };

    /** The friendliest reputation a faction can have toward another. */
    static constexpr int kMaxReputation = 100;

    virtual ~IReputes() = default;

    virtual State baseState() const = 0;
    virtual State state() const = 0;
    virtual std::optional<State> parse(const resource::Gff &gff) const = 0;
    virtual void replace(State state) = 0;

    // Reputation is directed: it is the source faction's disposition toward the
    // target faction, and the reverse relationship is independent of it.
    virtual int getReputation(Faction sourceFaction, Faction targetFaction) const = 0;
    virtual void adjustReputation(Faction sourceFaction, Faction targetFaction, int adjustment) = 0;

    virtual bool getIsEnemy(const Creature &source, const Creature &target) const = 0;
    virtual bool getIsEnemy(Faction sourceFaction, Faction targetFaction) const = 0;
    virtual bool getIsFriend(const Creature &source, const Creature &target) const = 0;
    virtual bool getIsNeutral(const Creature &source, const Creature &target) const = 0;

    /** Co-op duels: two creatures who are enemies to each other alone, whatever their factions. Nothing sets one
     *  but the coopduel console command; none exist otherwise, and none are saved. */
    virtual void setDuel(uint32_t first, uint32_t second, bool on) {}
    virtual bool inDuel(uint32_t first, uint32_t second) const { return false; }
    virtual void clearDuels() {}
};

class Reputes : public IReputes, boost::noncopyable {
public:
    Reputes(resource::ITwoDAs &twoDas) :
        _twoDas(twoDas) {
    }

    void init();

    State baseState() const override;
    State state() const override;
    std::optional<State> parse(const resource::Gff &gff) const override;
    void replace(State state) override;

    const std::vector<FactionDefinition> &factions() const { return _factions; }

    int getReputation(Faction sourceFaction, Faction targetFaction) const override;
    void adjustReputation(Faction sourceFaction, Faction targetFaction, int adjustment) override;

    bool getIsEnemy(const Creature &source, const Creature &target) const override;
    bool getIsEnemy(Faction sourceFaction, Faction targetFaction) const override;
    bool getIsFriend(const Creature &source, const Creature &target) const override;
    bool getIsNeutral(const Creature &source, const Creature &target) const override;

    void setDuel(uint32_t first, uint32_t second, bool on) override;
    bool inDuel(uint32_t first, uint32_t second) const override;
    void clearDuels() override { _duels.clear(); }

private:
    resource::ITwoDAs &_twoDas;
    std::vector<FactionDefinition> _factions;
    std::vector<std::string> _factionLabels;
    std::vector<std::vector<int>> _factionValues;
    std::set<std::pair<uint32_t, uint32_t>> _duels; // smaller id first

    void loadBase(
        const resource::TwoDA &repute,
        size_t factionCount,
        std::vector<std::vector<int>> &values) const;
};

} // namespace game

} // namespace reone
