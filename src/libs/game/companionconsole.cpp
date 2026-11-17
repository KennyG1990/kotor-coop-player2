/* Copyright (c) 2026 The reone project contributors */

// Console commands for a companion program that plays a seat from outside.
// Each does nothing unless called; none is a developer command.
//
//   modmenu         the program's own settings screen in the in-game Options
//   companionpazaak a real Pazaak game between the host and a seated guest,
//                   whose side the program plays; stakes move between the
//                   host's credits and that guest's own, and nowhere else

#include "reone/game/game.h"

#include <algorithm>
#include <charconv>

#include <boost/algorithm/string.hpp>
#include <boost/json.hpp>

#include "reone/game/coopsession.h"
#include "reone/game/gui/ingame.h"
#include "reone/game/gui/ingame/options.h"
#include "reone/game/object/creature.h"
#include "reone/game/outsidedialog.h"
#include "reone/system/logutil.h"

namespace reone {

namespace game {

namespace {

constexpr float kCompanionPazaakFallbackSeconds = 20.0f;

std::string cardText(const pazaak::CardDefinition &card) {
    switch (card.behavior()) {
    case pazaak::CardBehavior::FixedPositive:
        return "+" + std::to_string(card.magnitude());
    case pazaak::CardBehavior::FixedNegative:
        return "-" + std::to_string(card.magnitude());
    case pazaak::CardBehavior::SignSelectable:
        return "+-" + std::to_string(card.magnitude());
    default:
        return "special" + std::to_string(card.magnitude());
    }
}

int parseIndex(const std::string &text, const char *what) {
    int value = -1;
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc {} || result.ptr != text.data() + text.size() || value < 0) {
        throw std::runtime_error(std::string(what) + " must be a whole number, 0 or more");
    }
    return value;
}

std::string resultName(PazaakCompletedResult result) {
    switch (result) {
    case PazaakCompletedResult::PlayerWon:
        return "player_won";
    case PazaakCompletedResult::OpponentWon:
        return "companion_won";
    default:
        return "player_forfeited";
    }
}

} // namespace

void Game::initCompanionConsole() {
    _console.registerCommand("modmenu", "a settings screen a companion program puts into the in-game Options: modmenu set <base64 JSON {button,title,text,rows,back}> | off | state | open", [this](const ConsoleArgs &args) {
        std::string verb = args.size() > 1 ? boost::to_lower_copy(std::string(*args[1])) : "state";
        if (verb == "set") {
            if (args.size() != 3) {
                throw std::runtime_error("Usage: modmenu set <base64 JSON>");
            }
            try {
                _modMenu.set(ModMenuModel::parse(decodeBase64(std::string(*args[2]))));
            } catch (const std::invalid_argument &e) {
                throw std::runtime_error(e.what());
            }
            _console.printLine("modmenu set " + std::to_string(_modMenu.version()));
            return;
        }
        if (verb == "off") {
            _modMenu.clear();
            _console.printLine("modmenu off");
            return;
        }
        if (verb == "open") {
            // As its button does: the in-game Options, then the screen.
            if (!_modMenu.active()) {
                throw std::runtime_error("modmenu: no screen is set");
            }
            if (!_module || !_inGame || !_inGame->optionsMenu()) {
                throw std::runtime_error("modmenu: the in-game menu is not loaded");
            }
            if (_screen != Screen::InGameMenu || _inGame->activeTab() != InGameMenuTab::Options) {
                openInGameMenu(InGameMenuTab::Options);
            }
            _inGame->optionsMenu()->openModMenu();
        } else if (verb != "state") {
            throw std::runtime_error("Usage: modmenu set <base64 JSON> | off | state | open");
        }
        boost::json::object state;
        state["active"] = _modMenu.active();
        state["open"] = _inGame && _inGame->optionsMenu() && _inGame->optionsMenu()->modMenuOpen() &&
                        _screen == Screen::InGameMenu;
        state["opens"] = _modMenu.opens();
        state["version"] = _modMenu.version();
        boost::json::array picks;
        for (int pick : _modMenu.takePicks()) {
            picks.push_back(pick);
        }
        state["picks"] = std::move(picks);
        _console.printLine(boost::json::serialize(state));
    });

    _console.registerCommand("companionpazaak", "co-op Free Mode host: a Pazaak game with a seated guest whose side a companion program plays: companionpazaak start <guest id> [stake] [deck row] | play <hand index> [plus|minus|+1|+2|-1|-2] | end | stand | builtin | state", [this](const ConsoleArgs &args) {
        std::string verb = args.size() > 1 ? boost::to_lower_copy(std::string(*args[1])) : "state";
        if (verb == "start") {
            auto id = args.get<uint32_t>(2);
            if (args.size() < 3 || args.size() > 5 || !id) {
                throw std::runtime_error("Usage: companionpazaak start <guest id> [stake] [deck row]");
            }
            if (!_coop || !_coop->isHost() || !_coop->freeMode()) {
                throw std::runtime_error("companionpazaak: Free Mode host only");
            }
            if (!_module || _pazaakSession) {
                throw std::runtime_error(_pazaakSession ? "companionpazaak: a Pazaak game is already running" : "companionpazaak: no game is loaded");
            }
            auto purse = _coop->hostGuestPurse(*id);
            auto body = getObjectById<Creature>(*id);
            if (!purse || !body || !body->isRuntimeLive() || body->isDead()) {
                throw std::runtime_error("companionpazaak: that is not a seated guest's character");
            }
            int stake = args.size() > 3 ? parseIndex(std::string(*args[3]), "stake") : 0;
            int deck = args.size() > 4 ? parseIndex(std::string(*args[4]), "deck row") : 0;
            // Never more than the guest really holds: a game cannot make credits.
            int limit = std::min(stake, purse->second);
            bool borrowed = false;
            if (!playPazaak(deck, std::string(), limit, false, body)) {
                // The player has no side deck of their own yet: a friendly game
                // with borrowed cards, for nothing.
                PazaakSessionParams params;
                params.opponentDeck = deck;
                params.maximumWager = 0;
                params.opponentId = body->id();
                params.opponentName = body->name().empty() ? purse->first : body->name();
                params.collection = isTSL() ? PazaakSession::k2DefaultCollection() : PazaakSession::temporaryK1TestCollection();
                params.opponentSideDeck = isTSL() ? PazaakSession::temporaryK2OpponentSideDeck() : PazaakSession::temporaryK1OpponentSideDeck();
                if (!startPazaakFlow(std::move(params), nullptr, true)) {
                    throw std::runtime_error("companionpazaak: the game did not start");
                }
                borrowed = true;
                limit = 0;
            }
            _pazaakCompanionGuest = purse->first;
            _pazaakCompanionBorrowed = borrowed;
            _pazaakSession->setOpponentExternal(true, kCompanionPazaakFallbackSeconds);
            boost::json::object started;
            started["started"] = true;
            started["guest"] = purse->first;
            started["borrowedCards"] = borrowed;
            started["wagerLimit"] = limit;
            started["guestCredits"] = purse->second;
            started["playerCredits"] = _party.gold();
            _console.printLine(boost::json::serialize(started));
            return;
        }
        bool ours = _pazaakSession && !_pazaakCompanionGuest.empty();
        if (verb == "state") {
            boost::json::object state;
            state["active"] = ours;
            if (!_companionPazaakLast.empty()) {
                try {
                    state["last"] = boost::json::parse(_companionPazaakLast);
                } catch (const std::exception &) {
                }
            }
            if (ours) {
                auto &session = *_pazaakSession;
                state["guest"] = _pazaakCompanionGuest;
                state["borrowedCards"] = _pazaakCompanionBorrowed;
                state["screen"] = session.screen() == PazaakFlowScreen::Wager ? "wager" : session.screen() == PazaakFlowScreen::Setup ? "setup" : "board";
                state["wager"] = session.wager();
                state["fallbacks"] = session.opponentFallbacks();
                state["companionPlays"] = session.opponentExternal(); // false once handed to the game's own play
                state["waited"] = session.opponentWaited();
                if (session.screen() == PazaakFlowScreen::Board && session.match()) {
                    const auto &match = *session.match();
                    const auto &set = match.set();
                    auto projection = session.boardProjection();
                    auto side = [&](pazaak::Participant who) {
                        const auto &held = set.participant(who);
                        boost::json::object out;
                        out["total"] = held.total();
                        out["stood"] = held.finished();
                        boost::json::array board;
                        for (const auto &card : held.board()) {
                            board.push_back(card.value());
                        }
                        out["board"] = std::move(board);
                        return out;
                    };
                    boost::json::object player = side(pazaak::Participant::One);
                    boost::json::object companion = side(pazaak::Participant::Two);
                    player["sets"] = projection.playerSetWins;
                    companion["sets"] = projection.opponentSetWins;
                    // The companion's own hand: theirs to read, as the player reads theirs.
                    pazaak::LegalActions legal(match.legalActions(pazaak::Participant::Two));
                    const auto &hand = match.participant(pazaak::Participant::Two).hand();
                    boost::json::array cards;
                    for (size_t i = 0; i < hand.size(); ++i) {
                        boost::json::object card;
                        card["i"] = i;
                        card["card"] = cardText(hand[i].definition);
                        card["used"] = hand[i].used;
                        card["playable"] = std::find(legal.playableHandCards.begin(), legal.playableHandCards.end(), i) != legal.playableHandCards.end();
                        cards.push_back(std::move(card));
                    }
                    companion["hand"] = std::move(cards);
                    bool theirTurn = session.opponentExternal() && set.result() == pazaak::SetResult::InProgress &&
                                     set.activeParticipant() == pazaak::Participant::Two &&
                                     set.turnStage() == pazaak::TurnStage::AwaitingAction;
                    state["player"] = std::move(player);
                    state["companion"] = std::move(companion);
                    state["companionTurn"] = theirTurn;
                    state["canEndTurn"] = legal.canEndTurn;
                    state["canStand"] = legal.canStand;
                }
            }
            _console.printLine(boost::json::serialize(state));
            return;
        }
        if (verb == "builtin") {
            // The program cannot play on (out of calls or joules): the game's
            // own opponent plays the rest of the match, with no wait.
            if (!ours) {
                throw std::runtime_error("companionpazaak: no companion game is running");
            }
            _pazaakSession->setOpponentExternal(false);
            _console.printLine("companionpazaak builtin ok");
            return;
        }
        if (verb != "play" && verb != "end" && verb != "stand") {
            throw std::runtime_error("Usage: companionpazaak start <guest id> [stake] [deck row] | play <hand index> [plus|minus|+1|+2|-1|-2] | end | stand | state");
        }
        if (!ours || !_pazaakSession->opponentExternal() || _pazaakSession->screen() != PazaakFlowScreen::Board) {
            throw std::runtime_error("companionpazaak: no companion game is on the board");
        }
        pazaak::ActionError error;
        if (verb == "play") {
            if (args.size() < 3 || args.size() > 4) {
                throw std::runtime_error("Usage: companionpazaak play <hand index> [plus|minus|+1|+2|-1|-2]");
            }
            pazaak::PlayHandCardCommand command {pazaak::Participant::Two, static_cast<size_t>(parseIndex(std::string(*args[2]), "hand index")), std::nullopt, std::nullopt};
            if (args.size() == 4) {
                std::string how(*args[3]);
                if (how == "plus") {
                    command.sign = pazaak::CardSign::Positive;
                } else if (how == "minus") {
                    command.sign = pazaak::CardSign::Negative;
                } else if (how == "+1" || how == "+2" || how == "-1" || how == "-2") {
                    command.value = std::stoi(how);
                } else {
                    throw std::runtime_error("companionpazaak: plus, minus, +1, +2, -1 or -2");
                }
            }
            error = _pazaakSession->applyOpponentCommand(command);
        } else if (verb == "end") {
            error = _pazaakSession->applyOpponentCommand(pazaak::EndTurnCommand {pazaak::Participant::Two});
        } else {
            error = _pazaakSession->applyOpponentCommand(pazaak::StandCommand {pazaak::Participant::Two});
        }
        if (_pazaakBoard) {
            _pazaakBoard->refresh();
        }
        if (error == pazaak::ActionError::None) {
            completePazaakIfReady();
        }
        _console.printLine("companionpazaak " + verb + " " + (error == pazaak::ActionError::None ? std::string("ok") : "error" + std::to_string(static_cast<int>(error))));
    });
}

void Game::settleCompanionPazaak(PazaakCompletedResult result, int wager) {
    // Between the two purses only: what one side loses the other wins, and
    // never more than the loser really holds.
    int moved = 0;
    if (!_pazaakCompanionBorrowed && wager > 0 && _coop) {
        if (result == PazaakCompletedResult::PlayerWon) {
            moved = -_coop->hostMoveGuestCredits(_pazaakCompanionGuest, -wager);
            _party.giveGold(moved);
        } else {
            moved = std::min(wager, _party.gold());
            _party.takeGold(moved);
            _coop->hostMoveGuestCredits(_pazaakCompanionGuest, moved);
        }
    }
    boost::json::object last;
    last["guest"] = _pazaakCompanionGuest;
    last["result"] = resultName(result);
    last["wager"] = wager;
    last["moved"] = moved;
    last["borrowedCards"] = _pazaakCompanionBorrowed;
    last["fallbacks"] = _pazaakSession ? _pazaakSession->opponentFallbacks() : 0;
    _companionPazaakLast = boost::json::serialize(last);
    _console.printLine("companionpazaak finished " + _companionPazaakLast);
    info("Co-op: companion Pazaak " + _companionPazaakLast);
}

} // namespace game

} // namespace reone
