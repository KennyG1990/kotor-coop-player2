/* Copyright (c) 2026 The reone project contributors */

#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "reone/net/protocol.h"

namespace reone {

namespace game {

class CreatureAttributes;
class CreatureClass;
class Creature;
class Game;
class IFeats;
class ISpells;

/**
 * A co-op client's level-up for its companion (co-op design notes item 5):
 * the choices of one level, as the client's level-up screens made them.
 * The host checks every choice against its own rules and applies the level
 * only when all of them hold.
 */
struct CoopLevelUpChoice {
    int clazz {-1};              // ClassType the level goes into
    int ability {-1};            // Ability raised by one (levels divisible by four), -1 for none
    std::map<int, int> skills;   // SkillType -> ranks added
    std::vector<int> feats;      // FeatType picks, in order
    std::vector<int> powers;     // SpellType picks, in order

    bool operator==(const CoopLevelUpChoice &o) const {
        return clazz == o.clazz && ability == o.ability && skills == o.skills && feats == o.feats && powers == o.powers;
    }
};

/** The choices as the command's text field: "c<class>|a<ability>|s<skill>:<ranks>,...|f<feat>,...|p<power>,...". */
std::string encodeCoopLevelUp(const CoopLevelUpChoice &choice);

/** False for anything that is not exactly that form (bounded counts, numbers only). */
bool decodeCoopLevelUp(const std::string &text, CoopLevelUpChoice &choice);

/** What a level-up screen changed: before is the member's sheet, after the screens' result. */
CoopLevelUpChoice coopLevelUpDiff(const CreatureAttributes &before, const CreatureAttributes &after, int clazz);

/**
 * Host: check a client's level-up for its companion. The companion must have
 * the XP for the next level, and that level may not pass the host's own
 * (a companion never out-levels the host). The class is the one the level
 * goes into, the ability point comes only every fourth level, skill ranks
 * are paid from the class's points within the rank caps, and every feat and
 * power is a level-up candidate by the game's own rules, in order.
 */
net::CommandReject validateCoopLevelUp(Game &game, const Creature &companion, int hostLevel,
                                       const CoopLevelUpChoice &choice, CreatureAttributes &result);

/** Host: apply a validated level, as the level-up screen's finish does. */
void applyCoopLevelUp(Game &game, Creature &companion, const CreatureAttributes &result);

/**
 * What a character may choose at its next level (fork addition). A guest seat
 * driven from outside can read these and pick for itself instead of taking the
 * Recommended set. They are the numbers the level-up screens work from and
 * validateCoopLevelUp checks; the host still judges the choice it is sent.
 */
struct CoopLevelUpOptions {
    struct Skill {
        int id {0};
        int rank {0};            // ranks held before the level
        int cost {1};            // points a rank costs: 1 for a class skill, 2 otherwise
        int cap {0};             // the highest rank allowed at the new level
        bool classSkill {false};
    };
    struct Pick {
        int id {0};
        std::string name;
        std::string description;
    };

    int clazz {-1};              // ClassType the level goes into
    std::string className;
    int level {0};               // the level being taken
    bool abilityDue {false};     // one ability point, on every fourth level
    std::array<int, 6> abilities {};
    int skillPoints {0};         // before an ability point; raising Intelligence can add one
    std::vector<Skill> skills;
    int featChoices {0};
    std::vector<Pick> feats;     // every feat that is a candidate now
    int powerChoices {0};
    std::vector<Pick> powers;    // every power that is a candidate now
    CoopLevelUpChoice recommended; // the game's own picks, as Recommended makes them
};

/** The next level's options in a class, from attributes before the level. */
CoopLevelUpOptions coopLevelUpOptions(const IFeats &feats, const ISpells &spells, const CreatureClass &clazz,
                                      const CreatureAttributes &attributes, bool darkSide);

/** text cut to at most limit bytes without splitting a UTF-8 character. */
std::string coopCutText(const std::string &text, size_t limit);

/** The options as one line of JSON, for the console. Descriptions are cut to 300 characters. */
std::string coopLevelUpOptionsJson(const CoopLevelUpOptions &options);

/**
 * What a new character may choose at creation (fork addition), in the shape of the
 * level-up options: four levels' worth of skill points, the first level's rank caps and
 * the class's first-level feats, as the custom creation screens allow. attributes are
 * the class's own at level 1 (its abilities and granted feats). The recommended choice
 * is the one the creation screens' Recommended buttons make.
 */
CoopLevelUpOptions coopCreationOptions(const IFeats &feats, const CreatureClass &clazz, const CreatureAttributes &attributes);

/**
 * Apply a creation choice to the class's level-1 attributes by those same rules.
 * Returns why it is refused, or an empty string once attributes hold the choice.
 * Nothing is changed when it is refused.
 */
std::string applyCoopCreationChoice(const IFeats &feats, const CreatureClass &clazz, const CoopLevelUpChoice &choice,
                                    CreatureAttributes &attributes);

/**
 * What a character is and can do, in the game's own names (fork addition), as one
 * line of JSON: classes and levels, abilities, skills, the feats and powers it holds,
 * vitality and alignment. For a guest seat driven from outside, which must act by its
 * own sheet. Read-only; descriptions are cut to 200 characters.
 */
struct CoopOwnSheetState {
    std::array<int, 8> skillValues {}; // ranks with the ability and what is worn, as a check reads them
    std::array<bool, 8> skillUntrained {true, true, true, true, true, true, true, true}; // usable with no ranks (skills.2da)
    int hitPoints {0};
    int maxHitPoints {0};
    int force {0};
    int maxForce {0};
    int alignment {50}; // 0 dark to 100 light
    int xp {0};
};
std::string coopOwnSheetJson(const IFeats &feats, const ISpells &spells, const CreatureAttributes &attributes,
                             const CoopOwnSheetState &state);

/**
 * Host: the sheet of the creature a client controls: what its character,
 * equipment and level-up screens show. levelUpAllowed carries the host's cap.
 */
net::CharacterSheet coopCharacterSheet(const Creature &creature, bool levelUpAllowed);

/** Client: bring the local copy's attributes, XP and gear to the host's sheet. */
void applyCoopCharacterSheet(Game &game, Creature &creature, const net::CharacterSheet &sheet);

} // namespace game

} // namespace reone
