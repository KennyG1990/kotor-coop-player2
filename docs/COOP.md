# Co-op

How co-op works, for developers. For players, the README's "Host and join by IP" section covers
hosting and joining.

## The model: host-authoritative

- **The host** runs the full single-player game, unchanged: scripts, AI, combat, dice,
  saves. Between simulation ticks it applies the commands its guests sent, after checking
  each against the creature that guest controls. After each tick it sends every guest a
  delta snapshot of the replicated state.
- **A guest** is a thin viewer. It loads the host's current module from its **own**
  install, runs no scripts and no simulation, binds its copy of each object to the host's
  object (`coopidmap.cpp`: stable keys from the module's own data, because object ids
  differ between processes), applies snapshots, and interpolates between them. What
  its player does becomes commands (move, attack, use, cast, equip, ...).
- Without a session, the game behaves exactly as single-player.

Because guests never simulate, a guest cannot change story state by itself, and the host
rejects any command that names an object or item that is not the guest's to use (every
refusal has a `CommandReject` reason).

The host's simulation runs at a fixed 30 ticks per second (see
[DETERMINISM.md](DETERMINISM.md)).

## Story Mode and Free Mode

| | Story Mode | Free Mode |
|---|---|---|
| players | up to 3: the host and 2 guests | up to 8: the host and 7 guests |
| a guest plays | one of the host's companions (asked for by tag, or any free one); a stand-in character when none is free | their own character, brought from their machine (`GuestCharacter`) |
| inventory | the party's shared inventory | each guest's own items, credits and crafting materials |
| saving | the host's save | the host's save, plus each guest's character on both machines |

`--coop-mode story|free` picks the mode. A dedicated host (`engine --dedicated`, see
[dedicated_host.md](dedicated_host.md)) runs Free Mode with no local player and takes
up to 8 guests.

Host options: `--coop-host-only-transitions` (no area-exit ready check: only the host's
exit moves the party), `--coop-host-only-trading` (only the host opens Story Mode stores),
`--coop-no-pause`, `--coop-pause-cooldown <s>`, `--coop-enemy-scaling <f>` (enemy hit
points and damage per extra player), `--coop-ask-join 0|1`, `--coop-password <text>`,
`--coop-party-seats <0-2>` (below).

### Party seats (this fork)

A Free Mode guest's own character stays outside the party table, so scripts that count
or check the party see the host's party alone. On the host's screen that leaves the
guest as a line under the minimap and a small portrait beside it. `--coop-party-seats <n>`
(0, the default, changes nothing) lets the first `n` guests to join hold a party seat
instead:

- the guest takes the next free party portrait on the HUD, with its health and Force
  bars, status marks and level-up marker; the line and the small portrait are dropped
  for it, and its name tag stays;
- the party selection screen offers one companion place fewer per seat taken;
- the story always wins a place: companions it forces into the party are counted
  first, and when the party is full the seated guest is listed under the minimap again
  until a portrait frees.

The guest is still a puppet outside the party table, nothing is sent to clients and the
protocol is unchanged. The rules that need no engine are in `game/partyseats.h`
(`test/game/partyseats.cpp`); the host's `netstatus` reports `partySeats` (seats taken,
the seated characters present, and what each portrait shows).

### A seat driven from outside (this fork)

A program beside the game can play a guest seat through the control port, with the same
commands a person's screens send. The commands below exist for that. Each does nothing
unless it is called; a person in a seat is unaffected, and none changes the protocol.

Guest (client) console:

- `coopguestoptions [class slot]`: what a new guest character of that class may choose,
  in the shape of `cooplevelupoptions`: four levels' worth of skill points, a first-level
  character's rank caps, the class's first-level feat, and the set the creation screens'
  Recommended buttons make.
- `coopnewguest <class slot> <name> <choice>`: quick creation with a chosen set, written
  as `cooplevelupchoose` takes it. The choice is held to the creation screens' rules
  (`applyCoopCreationChoice`) or refused with the reason. Without the third argument the
  command is what it was: the class's own character, with no skill ranks.
- `cooplevelupoptions`: what the controlled character may choose at its next level, as
  JSON (points, costs, caps, legal feats and powers, the Recommended set).
- `cooplevelupchoose <choice>`: send a chosen set. `validateCoopLevelUp` on the host is
  still the only judge; `coopresult <seq>` gives its answer.
- `coopgear`: what the character wears and carries, and where each item may go.
- `coopsheet`: what the character is and can do, as JSON in the game's own names: classes
  and levels, abilities, each skill's ranks, the value a check reads and whether it can be
  used untrained (skills.2da), the feats and powers held, vitality, alignment. Read-only.

Host console:

- `coopgive <player> <item>` and `coopbag`: hand a guest an item from the host's
  inventory; list what can be handed over (Free Mode).
- `cooptalkable <player> <1|0>`: let that guest be talked to by clicking their
  character. Off for every guest unless set. A click then counts as a request to talk,
  reported in `netstatus` (`talkable`, `talkRequests`).
- `talkopen <object id> <base64 JSON>`, `talkstate`, `talkclose`: a conversation
  supplied from outside, run on the game's own conversation screen
  (`game/outsidedialog.h`, `test/game/outsidedialog.cpp`). One exchange is a line for the
  character to say and the replies the player may pick: a short label each, and the full
  line the player's character then speaks. A reply may name one of the game's own
  conversation checks: `check` (the game's script decides, as for a [Persuade] reply:
  `k_con_perseasy`, `k_con_persmed`, `k_con_pershigh`) or `showIf` (offered only when the
  script allows, as for [Force Persuade]: `k_con_fperslow`, `k_con_fpershigh`). The rule is
  the game's; nothing re-implements it. Such a conversation is the host's own, like a
  workbench's talk: it is not mirrored to guests and nobody is held by it.
- `conversationstats`: what those checks read from the player's character (Persuade,
  level, Charisma, Affect Mind, Dominate Mind, alignment).

## Joining by IP:port

Co-op uses [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets)
over UDP (`net/gnstransport.cpp`). The host listens on UDP **27015** by default
(`--host` or `--host <port>`). A guest joins with `--join <ip>:<port>`, or the Co-op screen
in the menus; a bare IP or host name gets port 27015. Players outside the host's network
need that UDP port forwarded to the host. No account and no server are involved.

An optional join-code service (`tools/coopserver`, `--coop-server host[:port]`) lets a
host hand out a short code instead of an address; builds use it only when it is
configured.

When a guest connects, the host is asked to Accept or Decline (unless `--coop-ask-join 0`
or headless).

## Protocol

`include/reone/net/protocol.h` defines every message; `src/libs/net/protocol.cpp` encodes
and decodes them, with hard size limits (a client message is at most 2 KiB, except a
guest's character, 16 KiB) and every enum checked on decode.

**Versioning.** `kProtocolVersion` (now **6**) goes in `Hello`. The host also compares the
build and fingerprints of both players' game files, and rejects a mismatch with a reason
the guest sees (`ProtocolMismatch`, `BuildMismatch`, `FilesMismatchK1`, `FilesMismatchK2`,
`ModeMismatch`, `SessionFull`, `Declined`, `Kicked`, `BadPassword`, ...). In practice the
host and every guest must run the same release. Bump the version whenever a message's
encoding changes; the comment beside `kProtocolVersion` lists what each version added.

**Messages** (by type byte):

| ids | messages |
|---|---|
| 1-11 | `Hello`, `Welcome`, `Reject`, `SlotRequest`, `SlotAssign`, `ModuleInfo`, `Snapshot`, `Command`, `CommandResult`, `Bye`, `Notice` |
| 12-15 | `ContainerView`, `ReadyCheck`, `SnapshotAck` (unreliable: the newest snapshot a guest holds, the base for deltas), `Events` (one tick's visual-effect and projectile events) |
| 20-27 | `SharedInventory`, `CharacterSheet`, `Bark`, `GuestCharacter`, `StoreView`, `GuestRoster`, `Offer`, `BenchView` |
| 28-29 | `MiniGameControl` (a guest's minigame input), `MiniGameState` |
| 100-103 | `ChatSend`, `ChatMessage`, `PingSend`, `PingMarker` |

`Command` carries a `CommandType` (32 types: movement, combat, items, feats, powers,
skills, ready check, pause, containers, equipment, level-up, stores, offers, Pazaak,
conversation replies, workbench and minigame commands). The host answers with a
`CommandResult` that is either accepted or one `CommandReject` reason.

[protocol_coverage.md](dev/protocol_coverage.md) lists how every message, command and
refusal is round-trip tested and fuzzed.

## What guests can and cannot do

Guests can move, fight, use feats, Force powers, skills and items, loot containers,
equip, level up (checked by the host: a companion never out-levels the host), trade,
take part in the area-exit ready check, pause (if the host allows it), use workbenches
and labs ([guest_workbench.md](guest_workbench.md)), drive swoop races and turrets the
host hands over ([guest_minigames.md](guest_minigames.md)), and chat.

The host leads conversations: guests watch, and pick replies only in a terminal session
the host hands to them. Module minigames belong to the host unless handed over. Some host
options (above) take transitions, trading or pausing away from guests.

## Chat and pings

Guests and the host can type chat lines and drop ping markers on points or objects. The
host checks and relays them (at most 5 chat lines per 5 seconds and 3 pings per 4
seconds per guest; the host itself is never limited), can mute players by name, and
guests keep a chat log; pings fade after 6 seconds. The parts
that need no engine are in `game/coopchat.h`; the HUD side is `gui/hudchat.cpp` and
`coopchatui.cpp`.

## Testing co-op

`test/game/coopnet.cpp` and the other `test/game/coop*.cpp` tests run a host and guests in
one process over the loopback transport, with synthetic data. `test/net/` covers the wire
format. The engine has `--coop-sim-lag`, `--coop-sim-loss` and `--coop-sim-jitter` for
testing under bad networks, and `--coop-record <file>` writes the game and co-op state
once a second for bug reports. See [TESTING.md](TESTING.md).
