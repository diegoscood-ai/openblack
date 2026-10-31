# The leash

## Summary

The leash ties the player's creature to the hand, to a thing in the world, or lets it go. Its type (the compassion
leash GOOD, the aggression leash EVIL, or the learning leash ROPE) is picked at the temple's posts and forces a desire
on the creature for as long as it is worn: the **timed dominant desire**. A right double click with the leash in the
hand ties it to the thing under the hand; a single tap sends the creature to act on a thing or a place; a needs sign
of a worship site makes it feed or fish there. Nine script natives and the L, V and B keys drive the same state.

openblack ports the leash's rules (`creature_leash`, `LeashSystem`), the timed dominant desire, the hand's tie and taps,
the nine natives and the keys. Still different: the L key acts at once instead of through its packet (V and B send
the leash type's message, applied at the next turn, as the original), the needs sign is rules only, and the clear's per-player gate has
no counterpart yet. The rope itself, its drawing and the walk back are in [The leash's rope](leash-rope.md#the-leashs-rope); the temple's posts in
[The temple's leash posts](temple-leash-posts.md#the-temples-leash-posts).

## The original

### Leash and the hand

The hand holds only the player's own or an allied player's creature, in or out of the influence
(`Creature::ValidForLockedSelectProcess` 0x476E10 and the action press 0x5D13FB..0x5D1481). Let go within 450 ms of
camera time, the press was a click: packet 0x5F, the TOGGLE_LEASH path below. Strokes and slaps act at frame time on
the local machine (packet 0x27 only replays them elsewhere); how the creature was treated goes to its mind with packet
0x59, the mind's "player feedback" fn_004E06A0 (`FinishActionUnsuccessfully("player feedback")`, then a plan when
|v| > 0.01). The hand's side is in [The hand on a creature](../hand/states.md#the-hand-on-a-creature).

- **The hand's end** 📼 1.20 only: `GInterface::UpdateAllLeashes` 0x5D9130 steps the rope with
  `fn_00848970(collar, hand point, dt)`: the collar (the drawn neck, 3D +0x47F0 at the bone [+0x51D4]) moves the
  rope's +0 end and the hand point its +0xC end (0x848B31..0x848B7D). The hand point is `GetLeashStartPoint(…, 0)`
  0x5D8EA0: untied on the local interface, the translation of the render hand's first drawn bone (CHand +0x47F0
  +0x24..+0x2C, 0x5D90B9..0x5D90F3), not CHand +0x78; on another interface the status's +0xB0; tied, the object's own
  branches. The drag ropes (interface state 0x15) use CHand +0x78 (0x5D9253).

#### The timed dominant desire

What a leash, a town tied to and the script's `SET_CREATURE_ONLY_DESIRE` force on a creature. Fields: mind
(Creature +0x164) +0x700 the forced desire (0x28 = none), +0x704 its turns, +0x708 its whole seconds. The desire
numbers below are openblack's `creature_desires::Desire` order (each source's owning desire in info.dat's source table
agrees).

- **`CreatureMental::SetCheatDominantDesire(d, seconds, needsToo)` 0x4DCE20**, in order: +0x700 = d, +0x704 = 0,
  +0x708 = `ftol(seconds)`; `SetActivated(d, 1)`; for every other desire i of 0..39 a byte table at 0x4DCEF4
  (index i − 4, three targets at 0x4DCEE8) chooses: **never held down** 9 IdleWithPlayer, 15 RestoreHealth,
  16 BeFriends, 18 ManifestState, 23 Rest, 30 PlayWithPlayer, 35 HangAroundAtHome, 38 LookAround; **only with
  needsToo** 4 Hunger, 7 Poo, 8 Tiredness, 14 Water; every other `SuppressDesire(i, seconds)`. Then, if d is
  Compassion (1), `UnsuppressDesire(16)` and `MakeDesireFullyDominantAndChangeSource(16)`; last `UnsuppressDesire(d)`
  and `MakeDesireFullyDominantAndChangeSource(d)`.
- `SuppressDesire` 0x4DC260: turns = `ftol((1000 div [0xD01A38]) · seconds)` (integer turns a second, a float product),
  kept only when larger than the counter (+0xA8 + 4i). `UnsuppressDesire` 0x4DC2D0 zeroes one counter,
  `UnsuppressAllDesires` 0x4DC2F0 all forty.
- `MakeDesireFullyDominantAndChangeSource` 0x4DC9A0: `MakeDesireFullyDominant` 0x4DC920 (only if the desire is
  activated: its value = its maximum +0x468, every other desire's value = the species' floor, creature info +0x274,
  activated or not), then `MakeAllSourcesMaximum` 0x4DE780 (every source of the desire but types 9
  AngerFromBeingDamaged and 33 RestoreHealthFromLife: `SetSource(type, 1)`), then `SetActivated(d, 1)`. So under
  Compassion, BeFriends ends unsuppressed, activated, its sources (34, 35) full and its value **back at the floor**,
  set there by Compassion's own step.
- `SetSource` 0x4DE6C0 clamps to 0..1 and calls the type's setter, a member-function pointer from a per-type table at
  0xC8D478 (32 bytes a row, getter +0, setter +0x10) filled by the static initialiser 0x4DD430. Setters exist for
  types 5, 11, 14, 21, 22, 26, 27, 32, 33, 35, 36, 37, 39 and 55. The innate ones write a mind field their getter
  reads: 5 kindness +0x1AA14, 11 aggression +0x1AA18, 26 lethargy +0x1AA1C, 35 friendliness +0x1AA20,
  39 communicativeness +0x1AA24. Of the leash's desires' sources (Impress 0, 1; Compassion 2..5; Anger 6..11;
  BeFriends 34, 35) only 5, 11 and 35 have setters, all innate.
- **Each turn**, `Creature::ProcessState` 0x473295..0x4732F0 (after the leash's confinement): if +0x700 is not 0x28,
  `UnsuppressDesire(+0x700)`, ++(+0x704), and when `(+0x704) div (1000 div [0xD01A38]) > (+0x708)` (unsigned),
  `UnsuppressAllDesires` and +0x700 = 0x28. With 10 turns a second and S seconds, it ends on the step that brings the
  count to (S + 1) · 10.
- **The clear** fn_004DCF20 (from `UpdateMoodsFromLeash` for ROPE, `SET_CREATURE_ONLY_DESIRE_OFF` and fn_004F5754):
  nothing if the creature has a player and `fn_0064A9C0(player)` → +0x12C → +0x1C is **not** 2 (`cmp [eax+0x1C], 2;
  jne` to the `ret`); otherwise, with no player or with that value 2, `UnsuppressAllDesires` if one was forced, and
  +0x700 = 0x28.
- **The clear's gate is the player's leash type.** `fn_0064A9C0` 0x64A9C0 returns the first non-null of the 18
  interface pointers at player +0x14, its +0x39C: the same walk and the same field as
  `GPlayer::GetNextInterfaceStatus` 0x64AAC0, so it is the player's first `GInterfaceStatus`. Its +0x12C is the
  `GLeashStatus` (as `UpdateLeash` reads it from `GetInterfaceStatusLeashOn`) and +0x1C the leash type, 2 = ROPE. So
  the clear acts only for a creature with no player or whose player's leash is the learning leash
  (`owner == NULL || FirstInterfaceStatus(owner)->LeashStatus->type == 2`, whether a leash is on or not). Called from
  `UpdateMoodsFromLeash` for ROPE, the type was just set to 2 (by `SetOn`'s caller or `SetType`), so that call always
  clears.
- **The mood and need spells**: their start and end handlers, 0x4F5980..0x4F5E5F (past the label fn_004F5754, which is
  why that label shows nine calls). Each start: `SetActivated(d, 1)`, `MakeDesireFullyDominantAndChangeSource(d)`, then
  `SetCheatDominantDesire(d, [0x8D1598], 1)`; Compassion and Anger first `FinishActionUnsuccessfully("nice spell
  started")` unless the byte at +0x25 has 0x4 set, Scratch `"made itchy"` likewise. [0x8D1598] is a float in `.rdata`,
  **20000.0** (0x469C4000), read by these nine calls only: so the spells' 20000 s is the seconds they pass, always with
  needsToo 1, for Compassion 1 (0x4F59D6), Anger 2 (0x4F5AF6), Hunger 4 (0x4F5BF3), Fear 5 (0x4F5C53), Tiredness 8
  (0x4F5CB3), Illness 25 (0x4F5D13), Water 14 (0x4F5D73) and Scratch 21 (0x4F5DE7; and 0x4F5E1A, which first takes the
  leash off and does not make it dominant beforehand). Compassion (sub_4F5980) and Anger (sub_4F5AA0) then, last,
  change a leash that is on: `GetInterfaceStatusLeashOn` 0x4CF060 (the first interface status of the creature's player,
  vt+0x1C, whose `GLeashStatus` +0x14 is set; none, nothing) and the type setter fn_005E6AE0 on that status's
  `GLeashStatus` (+0x12C) with GOOD 3 (0x4F5A04) or EVIL 1 (0x4F5B24) and the creature's player. The setter writes +0x1C
  and calls fn_005E6B00(type, player), which, for a player, sets the temple's pick (player +0xA48 → +0x30, fn_004648E0),
  runs `UpdateMoodsFromLeash` 0x4CEFC0 on the player's creature (+0xA4C: GOOD `SetCheatDominantDesire(1, 36000, 0)`,
  EVIL `(2, 36000, 0)`, ROPE the clear, only while a leash is on) and wipes the gesture path when the player is the
  local one (0x5E6B59..0x5E6B81). Nothing tests that the creature knows the leash, and the compassionate / angry spell
  refusal is the 0x65 packet's own (0x63DCE1..0x63DD05), not the setter's. As the leash's mood comes after the spell's
  `SetCheatDominantDesire`, the leash's 36000 s, needsToo 0 dominance is the one that stays. Each end calls `MakeDesireLeastDominant(d, info, 1.3)`
  0x4DC9F0; only Compassion (0x4F5A50), Anger (0x4F5B70) and Scratch (0x4F5E30) then call the clear fn_004DCF20. The
  ends of Hunger, Fear, Tiredness, Illness and Water (0x4F5C00, 0x4F5C60, 0x4F5CC0, 0x4F5D20, 0x4F5D80) do not, so
  their dominance stays until its 20000 s run out or another call replaces it. Nice and Nasty also save the alignment
  and, on a leash, make it GOOD (0x4F5A02) or EVIL (0x4F5B22); the clear at the end runs only with no player, or with
  the player's first interface status leash of type 2, the rope.
- **Callers**: `UpdateLeash` every turn, Compassion (GOOD, 0x4CE5F9) or Anger (EVIL, 0x4CE617, and on a creature it is
  tied to, 0x4CE673) for 36000 s, Impress for 120 s for a town (0x4CE574); the tie fn_005E6BD0 0x5E6DD9 (Impress
  120 s); `UpdateMoodsFromLeash` 0x4CEFC0 (GOOD Compassion, EVIL Anger, 36000 s; ROPE the clear); the script's
  `SET_CREATURE_ONLY_DESIRE` 0x6F476D (needsToo 1). Every leash call passes needsToo 0. Because the leash calls it
  every turn, the count restarts every turn and a leash's 36000 s never runs out while it is on, and every turn the
  other desires' values go back to the floor.
- **The other callers** of `SetCheatDominantDesire` ([V] byte scan, each 📼 1.20 only; the arguments pushed needsToo, seconds,
  desire): 0x474ED6 (the action row's desire, 30 s, needs 1); 0x47D163 in `fn_0047CFB0` from `ProcessState` 0x47358C
  (15 RestoreHealth, 10 s, needs 1, while life is below 0.4); the feedback 0x47B3F0 → 0x47E060, which first forces a
  plan (+0x1C14 = 1, agenda 0x4FF5C0) and then goes by the float's sign: 0 → 9 IdleWithPlayer 15 s (0x47E733), above
  0 → 30 PlayWithPlayer 180 s (0x47EBCD), below 0 → `GameRand(100)` > 50 ? 27 Sadness : 5 Fear, 180 s (0x47EEF8),
  keeping action 0x509130 at mind +0x21D0; 0x4EA57A in `fn_004EA540`, mimicry's stage 2 (the table's desire +0x24,
  120 s, needs 0; it also unsuppresses 33, sets +0x1C3C = 1 and, with +0x21FC above 0x28, clears +0x1C38 and suppresses
  33 for 40 s; see [Mimicry](mimicry.md#mimicry-and-the-town-hooks)); 0x507113 (the magic info's desire, 30 s, needs 1);
  0x654F5D in `fn_00653150`, the computer player's (its arguments, needs 0). The Hunger calls at 0x4F7DE5 and 0x4F8174
  use the plain form instead: `UnsuppressDesire(4)`, then `MakeDesireFullyDominant` 0x4DC920, with no sources filled
  and no activation.
- **The plan slots** 📼 1.20 only: the `LHStack` at mind +0x718 + 0x30·d that `SuppressDesire` 0x4DC2B9 empties is desire d's
  plan slot, a 0x30-byte plan laid out as the current plan +0xF48 (`SetToZero` 0x4F12B0 clears +0x0C..+0x2C;
  21 callers [V]). The planner rebuilds two slots a turn (0x4F0560, [0xBE026C] = 2 in the file), each by 0x4F0620 (its
  priority at +0x2C), and 0x4F0500 switches plans when the best slot's priority beats the current plan's +0xF74 by its
  own value.
- **When the leash's moods are taken.** `UpdateMoodsFromLeash` is called through fn_005E6B00 (the heart's leash pick,
  then the player's creature's moods, then the local player's gesture path wiped) from `GLeashStatus::SetOn(on)`
  (0x5E7001, before the lengths) and from the type
  setter 0x5E6AE0 (stores +0x1C, then 0x5E6B00; packet 0x65 below). The tie fn_005E6BD0 calls `SetOn(creature, 1)`
  (0x5E6D1A) every time, already on or not, then the tied lengths, `MimicPlayer`, the town-desire flags and, for a
  `TownCentre` (RTTI) on a leash that is not EVIL, `SetCheatDominantDesire(0 Impress, 120, 0)` (0x5E6DD9; the
  `FindDominantDesire` just before it is not used). `SetOn(off)` (0x5E7062..0x5E7091) clears with no gate unless the
  type is ROPE.
- **`UpdateLeash`'s order each turn** (0x4CE320, only while a leash is on): tied (+0x24), the kept point is the
  object's; then a `ShowNeedsVisuals` tie ([below](#the-leash-tied-to-a-needs-sign)); then the town: the object as a
  `TownCentre`, replaced by the `TotemStatue`'s +0x7C when it is one (the totem's town centre: its constructor 0x737B20
  stores its first argument there at 0x737B78), and, when that is set, the leash type is not 1 (EVIL) and
  `FindDominantDesire(0x28)` is not 0, its `GetTown` (vt +0x48): with the creature's player, `GetBeliefInPlayer(n)`
  (n = player +0xB5) stored as a float, `GetMaxBeliefMeNotIncluded(n)` × 0.5 compared with it (`fcomp`, `test ah, 1`):
  Impress 120 s when half the largest other belief is at least it (ordered; an unordered compare does not), else only
  when the town's player is not the creature's; with no player, when the town's player is not the creature's (none).
  Not tied: the kept point is the hand's. Then the radius = the full length, then the type's own desire (GOOD
  Compassion, EVIL Anger, 36000 s) after the town's, so on the compassion leash Compassion takes over the town's
  Impress every turn and only a ROPE tie keeps it; then, EVIL and tied to a creature (`CastCreature`) whose
  `FindDominantDesire(0x28)` is not 2 (Anger) and `GetDistanceInMetres(other, creature)` below 8 × the leashed
  creature's `GetHeight` (`fcomp`, `test ah, 0x41`: strictly below, ordered), Anger 36000 s on the other; last the
  attitude step.
- **`CreatureDesires::FindDominantDesire(except)` 0x4DC5B0**: over the 40 desires, the activated ones but `except`
  (0x28 leaves none out), the first whose value is strictly above the best so far, the best starting at 0.0; it
  returns 0 when none is above 0, the same as Impress. Held-down desires count.
- **Where the creature's turn runs.** `Living::ProcessLiving` 0x5EC810 calls `ProcessState` (Creature vtable +0x620 =
  0x472DC0, 0x5EC8CF) for every creature every turn, after `ProcessReaction`. `ProcessState` returns before
  `UpdateLeash` only when the hand's locked select holds it (Flags +0x24 & 0x10, 0x472F77); nothing there tests a
  fight. The keep-alive (0x473295) runs after the leash's confinement and before `UpdateDesires` (0x47364D).

#### Tying the leash with the hand

Read in the 1.20 exe (the scan of every call into the `SetPacket` overloads 0x550900..0x551800).

- **The gesture.** A double click of the Action (right) button while the leash is in the hand. `CMouse::ProcessButtons`
  0x61A150 tests the ControlMap's second binding (+0x618, mouse 5, the Action button) with `fn_00470DE0` (0x61A39F) and
  sends interface message 6, the double click (`fn_0061A510` → `fn_005D9DD0(6)`), except while the temple interior is
  up (g_game +0x205A28 == 1). Message 6's handler 0x5D9930 (entry 6 of the table 0xD186B8, stride 0x38) sets
  `GInterface` +0x39 |= 0x08 and |= 0x02 and +0x40 |= 1 when the low byte of +0x38 is 0 or has bit 1. The idle action
  states 0 and 1 share the process 0x5D5190, which after `ResetActionState` takes +0x39 & 0x08 as the tie (0x5D4210 at
  0x5D51DC) before the grab press (0x5D1320) and the action press (0x5D1330): the double click is taken in place of an
  action press. So a right double click in an idle hand is always eaten (0x5D4210 always ends with +0x38 = 0 and
  `ResetActionState`): leash or not, holding something or not, the press never reaches the grab or action paths. The
  window's WM_LBUTTONDBLCLK sets `SetButtons(0x10)` (0x7DC280) and WM_RBUTTONDBLCLK `SetButtons(0x20)` (0x7DC3D2),
  ORed into LHMouse +0x14C = [0xE85350] by `SetButtons` 0x7E4B80, and fn_0046F5F0 clears the bit as it reads it
  (0x46F647 / 0x46F623) 📼 1.20 only.
- **The decision, `GInterface` 0x5D4210**, in order:
  - the target is the action collide object (+0x400), else `FindObjectNearMapCoord` 0x5D39E0 at the action point
    +0x3F0 (the nearest within 5); none: nothing;
  - the interface's player's creature (`GPlayer` +0xA4C), whose player must be the interface's; none: nothing;
  - the first of the player's interface statuses (`GetNextInterfaceStatus` 0x64AAC0) whose `GLeashStatus` (+0x12C) is
    tied (+0x24): on the tied object or on the creature, packet **0x5C with object 0** (untie), otherwise nothing;
  - not tied: the target must not be the creature, `ValidAsInterfaceTarget` (vt +0x6F0) must be 1 and
    `GetInterfaceStatusLeashOn(creature)` 0x4CF060 must be this interface's status (+0x39C), the leash in this hand;
    then a `OneOffSpellSeed` (RTTI) is tapped (`SendTap` 0x5D38A0), and anything whose `ValidAsInterfaceLeashTarget`
    (vt +0x6F4) is not 0 gets packet **0x5C with its index** (`fn_00550C80`, 0x5D430A);
  - always at the end +0x38 = 0 and `ResetActionState`, and it returns 1.
- **Which things.** `ValidAsInterfaceTarget` is 1 for every `Object` (0x402840) but `ScriptHighlight` (0x709800 → 0).
  `ValidAsInterfaceLeashTarget` is 1 (0x402850) but `LeashObj` (the temple's posts, 0x464850) and `SpellIcon`,
  `TownSpellIcon`, `TownCentreSpellIcon`, `WorshipSpellIcon` (0x55D410). So the leash ties to anything in the world
  (trees, buildings, villagers, animals, a village centre, a totem, a needs sign, another creature, a rock) but a
  temple post, a spell icon, a script highlight and its own creature; a double click on a one-off seed taps it.
- **The single tap with the leash in the hand.** `GInterface::Tap` 0x5D3930(object): first, unless the leash is on
  (+0x14), untied (+0x24 = 0) and the object `IsReward` (vt +0x3F4), `fn_005D36D0(object)` (keeps the tapped thing at
  `BaseInfo` +0x45C and the turn at +0x468; fn_005D3700 forgets it after 15 s, [0x8C2C40]; read by
  `GAME_THING_CLICKED` 0x70AF38 📼 1.20 only; 0x5D393C..0x5D3967: it runs with the leash off, tied, or the object not a reward;
  no object skips it and the leash path); then with the leash on and untied, `ValidAsInterfaceTarget` == 1 and
  `ValidAsInterfaceLeashTarget`, 0x5D4180 sends **packet 0x5D** (the object) when the creature's leash is in this hand
  (`GetInterfaceStatusLeashOn` == +0x39C) and its current sub-action is not 0x5D (93 "TransportedByTeleport",
  0x4FCC79, unreachable without the teleport; mind +0xFB4 the agenda index, +0xFD8 its sub-action, 0x8F when the
  agenda count +0xFBC is 0); sent: +0x38 = 0, `ResetActionState`, return 1; otherwise `SendTap` 0x5D38A0. On the
  land, 0x5D3D10(point) returns 1 on every path. First the origin (0x5D3D16..0x5D3D32): the first two words of the
  point compared with 0 as integers (so -0.0 there is not the origin) and the third as a float (`fcomp`,
  `test ah, 0x40`: -0.0 counts, and so does a NaN, unordered setting C3); at the origin it jumps to 0x5D3DC0 and
  returns 1 having done nothing: no packet, no land tap, +0x38 left as it is. Elsewhere the leash on and untied, a
  creature, and the agenda's current sub-action not 0x5D (read without the count test) send **packet 0x5E** (the
  point, 0x550CB0), then +0x38 = 0 (no `ResetActionState`) and return 1; there is no in-this-hand test. Otherwise the
  land tap (0x6DCB90, and the point kept at 0xD4EEE8..0xD4EEF0, 0x5D3D9E..0x5D3DBA). 0x5D5608 is
  `SetPacket(0x5E, point)` and then `ResetActionState`, +0x38 = 0, +0x40 = 0.
- **The packets** (`GPacket::ProcessPacket`, the type byte − 6 indexes the jump table 0x63DDCC):
  - **0x5C** (0x63D416): the object from the packet (0x63DF70) cast to `Object`, the sender's player's creature
    (+0xA4C); needs Creature +0x1110 (the learning leash known), `GetInterfaceStatusLeashOn` ≠ 0 and its leash works
    (`GLeashStatus` +0x18). No object (none sent, or gone): `SetOn(creature, 0)`, then `fn_005E6EA0(creature)` on the
    sender's status: +0x24 = 0, `SetOn(creature, 1)` (whose on path tests only Creature +0x1110, else it turns the
    leash off, 0x5E6F91; it returns nothing, and the sound below plays whatever it did), and for the creature's player
    being the local one and the local player's own status (`MyInterfaceStatus`) sound 0x94 + t, t = ([0xBF3630] == 0)
    stored back (the static starts at 1, so 0x94 first, then 0x95, in turn) (confirmed: oracle). An object: `fn_005E6BD0(creature,
    object)` (the tie: its own sound turn [0xBF362C], +0x24, +0x34, `SetOn(1)`, the tied lengths, `MimicPlayer`, the
    town and needs-sign branches; the sound is 2D for the local player, at the object for another),
    `FinishActionUnsuccessfully("pulled by leash", 1, 1)`, `ForceActOnObject(object)` 0x4C5FF0, Creature +0x380 = 2.
  - **0x5D** (0x63D4B6): the sender's interface status's leash works (+0x18), a creature and an object:
    `ForceActOnObject(object)`, +0x380 = 2. No test of the leash being on or of the learning leash.
  - **0x5E** (0x63D57E): a creature, `GetInterfaceStatusLeashOn` ≠ 0 and works (`GLeashStatus` +0x18):
    `ForceActOnMapCoord(point)` 0x4C6540, then +0x380 = 2 (0x63D5B9) whatever it did, refusals included. Read whole
    below ([Sent to a point of the land](#sent-to-a-point-of-the-land)).

#### Pulling a tied leash

Read in the v1.42 exe; the release and the stretch test run in the oracle (`tools/oracle`, 62 and 3000 cases, bit
for bit against a single precision model).

- **The rope's pick.** `UpdateInterfaceCollide` 0x5D5A70, only with the action flags +0x38 at 0 and the leash tied
  (`[[+0x39C] +0x12C] +0x24`), runs fn_00848F90 on the tied rope (GInterface +0x198) with the mouse (+0x428, +0x42C)
  and [0x92AB44] = 2.0: from the rope's +0 end (the collar) through its 40 masses to its +0xC end (what it is tied
  to), each stretch between two points that `ProjectPoint` shows and whose depths are both above 1.0 (`test ah, 0x41`)
  is tested by fn_00848D70 with each end's width `max(ftol(screen width × rope +0x668 × 2.0 / depth), 2)` pixels.
  fn_00848D70 is a box round the stretch (the wider width out), then each end's round (t = 0 or 1), then t = the
  cosine between the stretch and the line from its first end to the mouse (the lengths' product at least 0.1, t in
  0..1) and a hit when `(width at t)² ≥ |m − a|² − t²·|b − a|²`. The hit's depth is the camera z of the point
  `(b − a)·t + a`.
- **What the rope takes the pick from** (0x5D5B83..0x5D5CF9): with an object under the hand, the rope wins when the
  object's depth +0x404 is strictly above the rope's and the object is no creature (vt +0x34); the object is then
  dropped (+0x400 = 0). With none, the bubble against the land (+0x40C against +0x3FC): when the land is nearer the
  bubble is dropped and the rope wins against the land's depth; otherwise against the bubble's, which it drops. A win
  sets the leash collide +0x410 = 1 and its depth +0x414.
- **The press.** `ActionPressed` 0x5D1330 takes the leash collide after the holding press and the bubble, before any
  object: 0x5D4440 lays two more ropes from the render hand's point CHand +0x78 (fn_00848600): +0x19C from the drawn
  collar (3D +0x5178 at the leash bone) with the tied rope's look (+0x668..+0x678) and its lengths ×1.5
  (+0x65C..+0x664), and +0x1A0 from the tied thing (its map place, its altitude over the land) with the same look and
  the lengths 5, 2 and 100; then action state 21. `UpdateAllLeashes` 0x5D9130 steps both every frame of state 21
  (0x5D9234..0x5D9302) from the collar and the tied thing to CHand +0x78, and `DrawAllLeashes` draws them.
- **The release** (state 21's process 0x5D5500): untied meanwhile, `ResetActionState` and nothing sent. Let go
  (+0x39 & 0x40): L = |CHand +0x78 − the tied thing's (x, land + altitude, z)|, in single precision as (dz² + dy²) + dx²,
  unless twice the thing's `Get2DRadius` (vt +0x64) is at least as long (`fcomp`, `test ah, 1`: an unordered pair keeps
  the distance); packet **0x3E** with L (0x5D55DE), then, unless the action point +0x3F0 is the origin, packet 0x5E
  with it (0x5D5608): the land under the hand. Then `ResetActionState`, +0x38 = 0, +0x40 = 0.
- **0x3E** (`ProcessPacket` 0x63C420): the sender interface's `GLeashStatus` +0x2C = L and +0x28 = L × 0.5, the full
  and slack lengths, with no other test. The turn keeps the creature within +0x2C of the tied thing.

#### Sent to a point of the land

Read with the disassembler (`Creature::ForceActOnMapCoord` 0x4C6540..0x4C721F, 1.20 symbols on the v1.42 exe;
not in the decomp). `phys` is creature +0x160, the mind +0x164; the point is the packet's `MapCoords`; "force(d, a)" is
`ForceActivityAndForceAction` 0x4C4450 (desire d, no activity belief, action a, the belief about itself, no
instrument, 1, k) with k = 0 when the mind already obeys (+0x1C14) and the current sub-action is 8, else 1.

- **The gates, in order** (each returns): the mind's active flag +0x20CE4 = 0; mind +0x1C38 = 0 is written; script
  control (Flags byte +0x25 & 4) unless [0xC64150]; `IsLeashWorking` 0x4CF0A0; the hand's locked select (Flags +0x24
  & 0x10); `GetLife` (vt +0x11C) ≤ 0 or unordered (`test ah, 0x41`); exhaustion phys+0x30 not below 1.0 (`test ah, 1`,
  so an unordered value passes).
- **Holding something** (phys+0x28, the object in its hand, 0x4C65E5): mind +0x1C78 = the point, force(24 ObeyPlayer,
  1 `MOVE_TO_POS`), +0x1C14 = 1, the sub-action agenda cleared (0x4FF5C0), +0x11B8 = 1, the target marker (below).
  With the leash status's type +0x1C = 1 (the aggression leash) or the point failing `IsDestinationValid` 0x483850:
  sub-action 0x73 (`SubArgumentPointAndFloat`, the point and `GetHeight` vt+0x42C × 8.0) with
  `LookWhileGoingTowardsPoint`, then the main sub-action 0x38 `ThrowAtPos` (`SubArgumentPoint`); the first is 0x73
  `MoveToThrowAtPos` (its first step 0x502660 walks to the point backed off 8 × its height along the line to the
  creature when nearer than that, to the point when further than 8 × its height, else nothing; its second 0x502810 is
  done once it no longer moves). The step's walk goes through 0x47A880 (0, point, `GetHeight` × 3, L): L is the
  entry's float (8 × the height when the order was given) as the nearest it may stop, and the far reach raised to
  L + 0.01; nearer than L the point is P − L·(P − C)/d (C the creature, d their ground distance), further than
  8 × its height P itself; the move's answers 0 and 2 fail the action, 1 tries again the next turn, 3 and 4 end the
  step. ThrowAtPos then throws at the order's point, at the land's altitude there plus the map coordinate's own
  height. openblack: `creature_leash::WalkToThrowAt` and `LeashSystem::ActOnPoint`'s `CarryOtherwise`, the throw
  once it stops (`CreatureLeash::throwAt`), at the ground's height; open point: at a distance of 0 the original
  divides by it, openblack throws from where it stands. Else sub-action 8 `MoveToPos` (the point, `GetHeight`, with no cap) and sub-action 1
  `DISCARD` with 0x61 (put it down, clip 97). The force's last argument is 1, and the marker is placed at the point
  tapped before the sounds below. No other test: no run when sent again, no wait, no help text, and the turns since
  it was made to act are not reset.
- **An empty hand** (0x4C69A9): the point is taken as a 3D point (x · 10/65536, altitude, z · 10/65536); not
  `IsDestinationValid`, `SpiralCheckForValidPoint` 0x4839D0 and, still not valid, `SendCandidateHelpScript(0x28
  FailureExplanation, 3)` and return. A spiral point becomes the order's point (×6553.6, `ftol`); the tests below
  still read the point tapped. Then: not `IsFighting` 0x47B1C0 and +0x10AC ≠ 0 (not named) returns; mind +0x18C54 = 0
  (0x4C6B11); `EndAnyAnimsRapidly` unless k = 0; when `GetInterfaceStatusLeashOn` is `MyInterfaceStatus` (the
  creature's player is the local one) `PlaySoundEffect` 0x429E30 with options bank +4 = [[0xCD3B20]+0x3AC] (the
  InGame bank), 2D (+8 = 0), owner +0x20 = 0, sample +0x24 = 1 (G_AcknowledgeCommand) (0x4C6B65..0x4C6B9F); then
  `PlayASoundEffect(55 Acknowledge, mode 0, not generic)` 0x4C6BB1 on the 3D body (the species bank, through the
  creature sounds' gate, no game draw) and `StartConcurrentAction(200)`. The holding branch plays the same three at
  its end (0x4C67E3..0x4C6840, the 55 at 0x4C6834). A field's `ForceActOnObject` plays its own as well (0x4C6283).
  Then the first that holds:
  1. nearer than 5.0 to the point and phys+0x28 set: force(24, 67 `PUT_DOWN`), +0x1B4 = 1; reached only if the hand
     took something since the first read;
  2. `MapCoords::IsWater` 0x6035B0 of the point: phys+0x28 and Play (3) activated: force(3, 42 `THROW_IN_THE_SEA`)
     on what it holds (again only if it took something); else Water (14) activated and thirst phys+0x34 > 0:
     +0x1C0 = 1, force(14, 55 `DRINK_FROM_THE_SEA`), mind +0xFE4 = the point; else force(14, 44
     `LOOK_AT_REFLECTION`), mind +0xFE4 = the point;
  3. `FindType(point, 18 FIELD)`: the target marker, then `ForceActOnObject(field)` 0x4C5FF0 and return;
  4. a player (vt +0x1C) with a citadel (+0xA48), exhaustion > 0.1 (`test ah, 0x41`), plan action mind +0xF60 ≠ 17 and
     home (+0x1200) to the point below 12.0: force(8 Tiredness, 17 `SLEEP`);
  5. else (0x4C6FBA): with +0x1C14 already set, +0x19C = 1 (the Run prompt); mind +0x1C78 = the point tapped;
     force(24 ObeyPlayer, 1 `MOVE_TO_POS`); +0x1C14 = 1; the agenda cleared; +0x11B8 = 1; sub-action 8 `MoveToPos`
     (the 3D point, min(`GetHeight`, 8.0): above 8.0 it is 8.0, `test ah, 0x41`) with `LookWhileGoingTowardsPoint`.
     With +0x1C14 already set, also `SetRequiredSpeed`(species +0x258, the run) and sub-action 24 `Wait` of
     ftol((`GameFloatRand(3.0)` + 2.0) × (1000 / ms a turn)) turns (0x4C70FC..0x4C71A4): the run is set first, then
     the draw (`GameFloatRand` 0x6DE530, the synced stream, one draw, after the force and the move are queued and
     before the help message), + 2.0 (`fadd`, 24-bit), the turns a second an unsigned `div` of 1000 by [0xD01A38]
     (`GGameInfo` +0x40), `fimul`, `__ftol`; queued with `AddSubAction` after `MoveToPos`, so it runs once the walk
     is over, with no look function. `Wait` (row 24, named "Wait" at 0xBE633C, filled in at 0x4F9DB1..0x4F9E20) has
     no start step and one step, 0x503500: while the 3D body `IsPerformingBodyAction` and is not `IsActuallyDead` it
     returns 0 without counting; else the agenda entry's count (+0x38) is decremented (`dec`, wrapping) and the step
     is over (2) once it is ≤ 0 (`jg`), so a count of n ends at its n-th counted call. Then, a plan belief and
     mind +0x1C78 farther than 40.0 from the creature: `SendCandidateHelpScript(0x29 CurrentDesire, plan desire
     +0xF50, the plan's object, +0x1C78)`.
  Paths 1 and 2 set +0x1C14 = 1 and +0x11B8 = 1 and send `CurrentDesire` when the plan is valid (no distance test);
  path 4 sets both and sends nothing.
- **The target marker** `fn_004F47E0(point)` 0x4F47E0: first `fn_004F4890` 0x4F4890 (the marker +0x11E4, when set,
  `GParticleContainer::CloseDown` 0x63E370; +0x11E8 = 0, +0x11F0 = 0), then `CreateSpotVisualWithSpecifiedDuration`
  0x63E580 (the order's point copy: the point tapped, or the spiral point that replaced it; type 12 CREATURE_TARGET,
  magnitude 1.0, 20 turns, owner the creature) kept at +0x11E4 (0x4F4820), `GetCreatureTargetFromObject` 0x4F4860 of
  the point into +0x11D8..+0x11E0 and +0x11F0 = 1. Every path calls it except the refusals and the empty hand's
  can't-get-there: the holding branch at 0x4C6683, the water and put-down paths at 0x4C6C6B / 0x4C6EB5, the field at
  0x4C6EEA (before `ForceActOnObject`), the sleep at 0x4C6FA0 and the walk at 0x4C7208 (its last step, after the
  wait's draw). The other callers of `fn_004F4890` are `Creature::StopWhatIAmDoing` (0x47596F), so the mark closes
  when the creature stops what it was doing, and fn_004F4700 (0x4F4707): the mark at a thing, below.
- **The target's bookmark.** The creature's `Initialise` makes it a `BookmarkGraphic` (+0x11EC, `Create(1, the
  creature's type)` 0x43A7D0: a sprite of Misc0, frame 0x38, colour 0xFFFFFF00). Each frame `PSysGlobal::DrawLoop`
  0x68F655 hands the local player's creature to sub_4F4CC0 0x4F4CC0, which, while +0x11F0 and the graphic are set,
  draws it at the target: the thing's point worked out again (`GetCreatureTargetFromPos` 0x4F4780: the thing's place,
  2 m up, its `GetHeight` on top), else the kept point +0x11D8. `BookmarkGraphic::Draw` 0x43A8D0 draws the frame
  sized by the bookmarks' pulse, then, for a creature's graphic, the creature type's frame (fn_0081F340) turned by
  −π/2 at 0.7 of the pulse the other way round. The thing goes from the target in `CreatureReceiveSpell::Process`
  once unavailable, and in `CreatureMental::RemoveBeliefsAboutDeletedObjects` 0x4D8945.
  In numbers ([the bookmarks' markers](../camera/player-camera.md#player-camera-the-bookmarks-markers)): the ring
  (frame 0x38) is a bookmark's marker, half width max(PulseWidth, 0.0001) and stretch PulseHeight / PulseWidth; the
  footprint's frame is `fn_0081F340(type)` & 0x3F, the first dword of the 16-byte rows at [0xC38230] (the creatures'
  land prints' table: cell, then the print's scale at +0xC), 0, 2, 4, 4, 4, 4, 3, 5, 6, 1, 1, 2, 0, 0, 0, 2, 0, 7 for
  types 0..17 (the type is the creature info's +0x1F4, its row in the tables, the ape's first) and type 0's past 17,
  with no first-of-April smiley; its angle 0xBFC90FDB, half width max(PulseHeight × 0.7, 0.0001) (0.7 the float
  0x3F333333 at [0x8AB238], 0.0001 the float at [0x8BF518]) and stretch (PulseWidth × 0.7) / PulseHeight, each step a
  float (24-bit FPU); the sprite's half height is stretch × half width. At the pulse of 100 ms (width 0x3F9355D2,
  height 0x3F49AC4A) the oracle gives the footprint's size 0x3F0D2BCD and stretch 0x3F82EAD1 (half height
  0x3F106377). Both pictures are drawn in one Z object, keyed by the sprite's point (`AddDrawing` 0x43A880), the ring
  first. `PSysGlobal::DrawLoop` hands over the local player's creature (`g_game` + 0xA64 + 0xA60 × `PlayerIndex`)
  only while [0xC029F4] is set; no instruction writes it directly (`xref.py writers`).
- **Told to act on a thing** (`Creature::ForceActOnObject` 0x4C5FF0..0x4C6456, read with the disassembler on the
  v1.42 exe; not in the decomp; `xref.py show 0x4C5FF0`). Callers: packets 0x5C and 0x5D, which share the call at
  0x63D504 and then set +0x380 = 2 at 0x63D50B (0x5C jumps there from 0x63D4B4, 0x5D falls through at 0x63D501),
  packet 0x68 (handler 0x63D520, call 0x63D557, then creature +0x10E8 = ftol(1000 / ms a turn × 15.0 [0x8C2C40]) and
  `ForceToolTips(0xEA2, 1.0)`, with no +0x380; 0x69 at 0x63D5CE does the same after `ForceActOnMapCoord`), the
  field order of `ForceActOnMapCoord` (0x4C6EF2), `FightThisToObject` (0x4C5FE5), sub_4CECF0 (0x4CEE59),
  fn_004CEE80 (0x4CEFA5) and sub_50A380 (0x50A746, the kept action it can set below). In order:
  1. the gates, each returning: mind +0x20CE4 = 0; mind +0x1C38 = 0 is written; `IsLeashWorking`; Flags +0x24 & 0x10;
     +0x10AC ≠ 0 and not `IsFighting`; `GetLife` (vt +0x11C) ≤ 0 or unordered; exhaustion phys+0x30 not below 1.0.
     There is no script-control test (the point order has one). From the field order every gate has already passed;
  2. a `BigForest` (RTTI cast): `ForceActOnMapCoord(its MapCoords +0x14)` and return; a `FishFarm` with desire 4
     activated, `KnowsAction(4, 0)` and phys+0x1C below 1.0: +0x1C0 = 1, force(4, 0x9B) on its belief, mind +0xFE4 =
     its point, +0x1C14 = 1, with a valid plan `SendCandidateHelpScript(0x29, …)`, the mark at the farm, +0x11B8 = 1,
     and return, with no sound. A field (`Field` : `Abode` : `MultiMapFixed`) is neither;
  3. the object's vt +0xAC ≠ 0: mind +0x21D0..+0x21DC = {sub_50A380, 0, 0, 0} (the kept action; vt +0xAC not named);
  4. **the acknowledgement**, unconditional past the gates (0x4C6218..0x4C628F): when `GetInterfaceStatusLeashOn` is
     `MyInterfaceStatus`, `PlaySoundEffect` with the InGame bank ([[0xCD3B20]+0x3AC]), 2D (+8 = 0), owner +0x20 = 0,
     sample +0x24 = 1, as `ForceActOnMapCoord`'s; then `PlayASoundEffect(0x37 Acknowledge, 0, false)` on the 3D body
     phys+0x58 (`edi` is 0 on both paths) and `StartConcurrentAction(200)`;
  5. mind +0x18C54 = 0 (0x4C629B); `AddBeliefAboutObject(itself, object)`, `AddBeliefAboutAssociatedObject`; a plan
     zeroed; k = mind +0x1C14 (already obeying); the agenda cleared; `FinishActionUnsuccessfully(0, 0, 1)`;
     `EndAnyAnimsRapidly`; with something in its hand (phys+0x28) `FindSomethingToDoOnObjectUsingObject` (with the
     held thing's belief), else `FindSomethingToDoOnParticularObject` 0x4C52D0; either plan found is adopted
     (`ConstructCurrentPlanFromPlan(plan, 1)`). With its hands empty the object's `CanBePickedUpByCreature` (vt +0x258,
     on the belief's thing, 0x4C6369..0x4C6390) comes first: a thing it can pick up skips the search (`jne 0x4C63B8`)
     and goes straight to fn_004C7590(belief), as does a search that finds nothing. fn_004C7590 0x4C7590 asks
     vt +0x258 again and forces `ForceActivityAndForceAction(6 Curiosity, 0, action, belief, 0, 1, 0)` with action
     `((vt+0x258 != 0) ? 0x2C : 0) + 0x14` (`neg; sbb; and 0x2C; add 0x14`): 64 `HoldObject` for a thing it can pick
     up, 20 `ExamineByLooking` otherwise (info.dat's action rows, `Scripts\info.dat` 0x4BD40 + row × 0x100), then
     mind +0x1C14 = 1. So a tap on a cow, a villager or anything else a creature can lift, with the creature's hands
     empty, makes it pick the thing up and hold it, whatever its desires say; k set: `SetRequiredSpeed`(species +0x258,
     the run);
  6. with a valid plan: `SendCandidateHelpScript(0x29 CurrentDesire, plan desire +0xF50, the plan's object, mind
     +0x1C78, 0)` (no distance test), mind +0x1C14 = 1, the mark at the object (fn_004F4700) and
     `MimicPlayer(object)`.
  So a field order is heard twice: sample 1 and the creature's 55 as the order is taken, and sample 1 and 55 again
  as it is told to act on the field, the local player's sample before the creature's sound each time; another
  player's creature makes its 55 twice. The tie (0x5C) and the act on a thing (0x5D) acknowledge too, once.
- **The mark at a thing** fn_004F4700(object) 0x4F4700: fn_004F4890 (the mark before closed), then
  `CreateSpotVisualWithSpecifiedDuration(the object's MapCoords +0x14, 12 CREATURE_TARGET, 1.0, 20, the creature)`
  kept at +0x11E4, +0x11E8 = the object, `GetCreatureTargetFromPos` into +0x11D8 and +0x11F0 = 1.
- **`FinishActionUnsuccessfully`'s parts** that a pull away from an action touches: it stops the creature
  (`StopWhatIAmDoing`'s `fn_0048F710(1, 1)`), clears the creature's returning flag (+0x11B8, 0x475C4A), its led
  control (+0x380, 0x475BD2) and the mind's obeying (+0x1C14, 0x475773 and 0x475C1F), and gives up the plan (the
  agenda cleared at 0x475A86 and the plan's desire +0xF50 = 0x28 at 0x475A91); the rest of 0x47573E..0x475A7A includes
  `ModifyStrength([0xC6DAF0])` with 3D +0x528C, the sub-action's end call through 0xBE0C18 and the partner's stop.
- **The mind's obeying flag** (mind +0x1C14, the agenda's `CommandState.Commanded`): set after a forced plan by its
  caller, not by `ForceActivityAndForceAction` 0x4C4450 itself: `CommandActivityAndAction` 0x4C4580 (0x4C45AD),
  `ForceActOnObject` and fn_004C7590 above, `ForceActOnMapCoord`, and others (`gx.py grep CommandState.Commanded`,
  listed under [Pending](#pending)); also cleared by `CreatureAgenda::Clear` 0x4D3760;
  cleared by `FinishActionUnsuccessfully` 0x475773 and `StopWhatIAmDoing` 0x475C1F, and by `ConsiderDecision`
  0x4F0370 when the creature has no plan or no step left (bw1-decomp `CreaturePlan.cpp`, `ConsiderDecision`). Its readers:
  `ConsiderDecision`'s free choice runs only while it is clear (0x4F03C3; the kept action of
  [the needs sign](#the-leash-tied-to-a-needs-sign) and [casting.md](casting.md) read the same test); `ForceActOnMapCoord`
  0x4C6540 takes k and the Run prompt from it ([Sent to a point of the land](#sent-to-a-point-of-the-land));
  `LeashMakesCreatureStop` 0x4CEC00 counts the stopped desire and sends the failure help only while it is clear
  (`CreatureLeash.cpp:353`); the mimicry of another creature (`CreaturePlan.cpp:198`,
  [mimicry.md](mimicry.md)), which is inside bw1-decomp's `VERSION_BW1WCI` block (the confined creatures) and not in
  this exe's `ConsiderDecision` (`gx.py show 0x4F0370 --ghidra`: the plan test, the mimic state, then the gate on
  the flag, the reaction, `Fainted` and `ProcessStoredCommands`, the kept action and `DecideWhatToDo`); the reactions (`IsAvailableForReaction`, `CreatureReaction.cpp:89`, clear or a leash
  control; `ReactToCreaturePriority`, `CreatureReaction.cpp:182`).
- **Other callers of the tie** (not the hand): the script's `AttachObjectLeashToObject` 0x6F456B and the hand demo's
  `AttachLeashToObject` 0x653CC0 (registered by 0x64E720 at 0x64EF46; its tie call 0x653D6A), which call
  `fn_005E6BD0` directly. The tie itself does not tell the mind to act: only packet 0x5C's handler does.
- **Going inside the citadel takes the leash off.** `GGame::GoInsideCitadel` 0x553E10, when not already inside
  (g_game +0x205A28 != 1): if the player is human (`GetHumanNumberInPlayer` > 0), has a creature (player +0xA4C) and
  `Creature::GetInterfaceStatusLeashOn` 0x4CF060 (the first of the creature's player's interface statuses whose
  `GLeashStatus` +0x14 is set) is `MyInterfaceStatus`, it sends packet 0x5B (`SetPacket` at 0x553F5C), before it keeps
  the pause and pauses the game. The handler (0x63D3CE) tests again that the sender's creature exists and that its
  leash is on in the sender's status, then calls `GLeashStatus::SetOn(creature, 0)` (the off path, as
  `DETACH_OBJECT_LEASH`). In single player no turn runs while paused (`LocalTimerSaysDoATurn` 0x54C4A0 returns 0 with
  game +0x14 bit 2 set, 0x54C528), and the paused branch of `ProcessNetworkPackets` (0x54CC66..0x54CCFF) runs only
  `Temple::ProcessGameTurn` once every 100 ms, so the packet is applied at the first turn after leaving.

#### The leash tied to a needs sign

Read in the 1.20 exe.

- **Which sign.** A `WorshipTotem`'s constructor 0x7808B0 sets its +0x100 to its worship site and makes a
  `ShowNeeds(totem)` (0x719B60 → 0x719AB0), which makes three `ShowNeedsVisuals`, the `GShowNeedsInfo` rows 0, 1 and
  2 (0xD99738 + i × 0x114), at ShowNeeds +0x18 / +0x1C / +0x20, each owned (+0x60) by the ShowNeeds. Row 3 is the
  workshop's own sign (`Workshop::CallVirtualFunctionsForCreation` 0x779429), whose owner is not a `ShowNeeds`: it never
  qualifies. `fn_00719B90` (from `WorshipSite::Process` 0x77B2B1) sets each sign's desire (+0x5C) with `fn_00719E80`:
  row 0 the totem's `CalculateDesireForFood` (vt +0x420), row 1 `CalculateDesireForRest` (vt +0x424), row 2
  `CalculatePeopleHidingIndicator` (vt +0x428).
- **The fill, `fn_0071A200`.** The desire (+0x5C) against the info's +0x108 (`MaxNeedValue`), `fcompp` with `test ah,
  0x41`: the most strictly above the desire gives desire / most, otherwise (equal, below, unordered) most / most. At the
  game's 24-bit precision the quotient is a float.
- **At the tie** (`fn_005E6BD0`, 0x5E6DE0..0x5E6E87): the tied object cast to `ShowNeedsVisuals`, its +0x60 to
  `ShowNeeds`, that one's +0x14 to `WorshipTotem` with +0x100 set; then mind +0x21D0..+0x21DC = {0x50A760, 0, 0, 0} and
  +0x21E0 = `AddBeliefAboutObject(creature, sign)`. No fill test.
- **Every turn while tied** (`UpdateLeash` 0x4CE363..0x4CE46A): the same casts, then the fill against 0.6
  (`fcomp qword` [0x8CF7D8], `test ah, 0x41`: strictly above, ordered); the slot set again unless it already holds
  exactly {0x50A760, 0, 0, 0}, with a fresh belief.
- **The slot** is a member-function pointer (🔍 inferred from the layout) run once by the mind's free choice
  `fn_004F0370` (0x4F048F..0x4F04E4) when there is no forced plan (+0x1C14 = 0), Creature +0x94 = 0, +0x384 = 0,
  `fn_004C74E0` is false and no plan step is left: it is called with (+0x21E0, +0x21F0), then +0x1C14 = 1 and the slot
  is cleared to {0, 0, 0, −1}.
- **0x50A760**: action 0x12C (300, fish for the worship site) when `KnowsAction(NORMAL, 4 KNOWN_ABOUT_FISH)`, then
  action 0xF6 (246, food for the worshippers) when `KnowsAction(MAGIC, 14 MAGIC_TYPE_FOOD)`, which wins; with one,
  `ForceActivityAndForceAction(0x18 ObeyPlayer, none, action, the sign's belief, none, 1, 1)` and +0x1C14 = 1. The
  sign's row is never tested: tied to the rest or the hiding sign above 0.6 it feeds or fishes all the same.

### Leash natives

**Evidence.** The native table's "NONE" entry is at 0x00C0DB98, as `bw1-decomp/src/Black/ScriptFunctions.cpp`
says. bw1-decomp has the table entries (the same file's native table) and the symbols
(`config/BW1W120/symbols.txt`, the `GScript` natives) but no bodies; the bodies below were disassembled from
`runblack.exe` with capstone. Mac (BW1M110) names are given in brackets where they differ.

#### Common facts

- `GScript::ScriptErrorMessage` (0x6F62B0) is a bare `ret`: every script error message is a no-op in the shipped game.
- VM push types: 1 = Int, 6 = Boolean (openblack's `LHVMTypes.h` `DataType`). `POP` returns the raw 32-bit value; only
  TOGGLE_LEASH converts it (`fld` + `__ftol`).
- "Is a creature" is vtable +0x34; "its player" is vtable +0x1C (`GetPlayer`).
- The leash state is a `GLeashStatus` at `GPlayer::GetLeaderInterfaceStatus()` +0x12C. Fields: +0x14 leash on;
  +0x18 works; +0x1C type in use (a LEASH_TYPE); +0x24 object it is tied to; +0x34 turn it was tied; +0x38 interface
  status. So the leash state, its "works" flag included, is the player's, not the creature's; `SetOn` does not touch
  +0x18.
- **The fields as read** 📼 1.20 only: `Creature::GetInterfaceStatusLeashOn` 0x4CF060 returns the first of the player's
  interface statuses whose `GLeashStatus` has +0x14 set (`SetOn(creature, 1)` writes 1 at 0x5E6F9F, the off path 0 at
  0x5E7066, the init 0 at 0x5E6946); `Creature::GetObjectWhichLeashIsAttachedTo` 0x4CF120 the one with +0x24 set (the
  tie writes the object, then `SetOn(creature, 1)`; the off path 0x5E7069 and the untie 0x5E7193 clear it), so a tied
  leash is still on; +0x1C is 2 (ROPE) at init (0x5E6950), written by packet 0x65 (0x5E6AEA) whether the leash is on
  or not, and passed by `SetOn` to fn_005E6B00 (0x5E6FFB) → `UpdateMoodsFromLeash` 0x4CEFC0; +0x18 is read by
  `Creature::IsLeashWorking` 0x4CF0A0 (init 1). Creature +0x110C, +0x1110 and +0x1114 are the evil, rope and good
  leashes known (fn_004CF040 counts them); a version 27+ mind file sets +0x1108..+0x1114 at load (`fn_004E84A0`
  0x4E8A6F..0x4E8A96, `Archive >> int` 0x7123B0; version 24+ also +0x10C0 at 0x4E8A6A).
- **The unnamed functions** 📼 1.20 only: `GLeashStatus::SetOn(creature, on)` 0x5E6F70 puts the leash on needing only Creature
  +0x1110 (the rope known); without it, it runs the off path (+0x14 = +0x24 = 0). fn_005E6EA0 puts it on in the hand
  (untie, `SetOn(1)`, the sounds 0x94 / 0x95 in turn); fn_005E7140 is the toggle (tied → untie, with mind +0x20D1C = −1
  and +0x20D24 = 1 for a `TownDesireFlags`; else `SetOn(!on)`); fn_005E6BD0 the tie (creature, object); fn_005D06E0
  the toggle's checks. "Has a player" is `Creature::GetPlayer` 0x473F20 (+0x1070 not null). In 185 and 269 the
  creature is the argument pushed first when both are creatures (0x6F4515..0x6F452F).
  `ConvertScriptPlayerToGamePlayer` 0x6EB9A0: 0 is the player at g_game +0x205A5B, n is n − 1, and `GetPlayer` gives
  none from 8.
- There are 9 leash entries, not 8 (TOGGLE_LEASH is the ninth). Ids, names and argument/return counts match
  openblack's `CHLApi.cpp` bindings.

#### Script enum and the temple's selection

`bw1-decomp/include/chlasm/Enum.h`'s `LEASH_TYPE` (Lionhead header, "Jonty Barnes 2001-11-15", via Daniels118/chlasm):
`LEASH_TYPE_NONE = -1`, `FIRST = 0`, `EVIL = 1`, `ROPE = 2`, `GOOD = 3`. The original `LAST = 4` is commented out; the
decomp changed it to 3. The game data ships no CHL `Enum.h`.

The temple stores the type as a 0-based index in `TempleLeash::field_0x0` (bw1-decomp `CitadelHeart.h`), -1 for none:

- Setter `fn_004648E0` (= Mac `CitadelHeart::SetLeash(LEASH_TYPE)`): 1 → 0, 2 → 1, 3 → 2; anything else leaves it
  unchanged.
- Getter `fn_00464920`: returns -1 when `heart->leashes` (+0xE4) is null or the index is -1; otherwise 0 → 1, 1 → 2,
  anything else → 3.
- `LeashObj::InterfaceTap` (0x464490) sets the index to -1 when the post already selected is tapped.

#### Per native

| Id | Name | Function | Pops (first pop = last pushed) | Pushes | Behaviour |
|---|---|---|---|---|---|
| 185 | ATTACH_OBJECT_LEASH_TO_OBJECT | `AttachObjectLeashToObject` 0x6F4480 [Mac `AttachObjectLeachToObject`] | A, then B | — | Swaps A and B if only one is a creature, so either order works. Needs a creature with a player, then `fn_005E6BD0(creature, object)` on the leader's `GLeashStatus`: plays the sound, +0x24 = object, +0x34 = game turn, `SetOn(creature, 1)`, `UpdateLeashLengthWhenAttachedToObject`, then 0x4E0DE0 (labelled `MimicPlayer`), and the TownDesireFlags / TownCentre desire. |
| 186 | ATTACH_OBJECT_LEASH_TO_HAND | `AttachObjectLeashToHand` 0x6F4580 | creature | — | Needs a creature with a player, then `fn_005E6EA0(creature)`: +0x24 = 0, `SetOn(creature, 1)`, a sound for the local player. **No branches**: always unties and turns the leash on. |
| 187 | DETACH_OBJECT_LEASH | `DetachObjectLeash` 0x6F4610 | creature | — | Needs a creature with a player, then `GLeashStatus::SetOn(creature, 0)` (0x5E6F70) on the leader status. Off path: clears +0x14 and +0x24, unsuppresses desires unless +0x1C == 2, clears some creature fields, `StopImmersion(7)`. |
| 222 | IS_LEASHED | `IsLeashed` 0x6F4980 | object | Bool | false if missing, not a creature, or no player; otherwise `Creature::GetInterfaceStatusLeashOn(c) != 0` (0x4CF060): some interface status of the player has +0x14 set. |
| 249 | SET_LEASH_WORKS | `SetLeashWorks` 0x6F4E50 | creature, then value | — | `GLeashStatus+0x18 = raw popped bits` (float bits, unconverted) when it is a creature with a player. |
| 269 | IS_LEASHED_TO_OBJECT | `IsLeashedToObject` 0x6F4FC0 [Mac `ObjectLeashedToObject`] | A, then B | Bool | Same swap as 185. Pushes `GLeashStatus+0x24 == other`; does not test +0x14. Any failure pushes false. |
| 275 | GET_OBJECT_LEASH_TYPE | `GetObjectLeashType` 0x6F5460 | object | **Int** | See below. |
| 305 | SET_DRAW_LEASH | `SetDrawLeash` 0x708C80 | value | — | `GGame+0x250090` (GScript) `+0x78 = raw popped bits` (written at 0x708C9D); matches [The first land's intro](../scripts/land1-intro.md). Does not touch the leash state. |
| 354 | TOGGLE_LEASH | `ToggleLeash` 0x6F4430 [Mac `TogglePlayerLeash`] | player (float, `ftol`) | — | See below. |

**TOGGLE_LEASH.** `ConvertScriptPlayerToGamePlayer` (0x6EB9A0) → `GetPlayer` → `GetLeaderInterfaceStatus` →
`GetInterface` → `fn_005D06E0`, which takes `player->creature` (+0xA4C), needs creature vt+0x2C true and Creature
+0x1110 != 0, needs the leash off or on in this same interface status (+0x39C), then calls `fn_005E7140`: if tied
(+0x24) it unties (+0x24 = 0, town desire reset, `StartImmersion(7, 0x80000000)`), otherwise
`SetOn(creature, !+0x14)`, whose on path puts on the picked type (+0x1C).

#### GET_OBJECT_LEASH_TYPE

`GetObjectLeashType` 0x6F5460-0x6F54F0 does **not** report the creature's own leash. It does
`dynamic_cast<Creature>` → `GetPlayer()` → `player->citadel` (+0xA48, bw1-decomp `Player.h`) → `citadel->heart` (+0x30) →
`fn_00464920(heart)`. It reports the **type selected at that player's temple**, pushed as Int:

| Case | Pushes |
|---|---|
| Object not found, not a creature, no player, no citadel, or no heart | `0` |
| Heart exists but its TempleLeash is not created, or nothing is selected | **`-1` (`LEASH_TYPE_NONE`)** |
| Selected index 0 / 1 / 2 or more | `1` EVIL / `2` ROPE / `3` GOOD |

**No leash selected pushes -1.** 0 only means the lookup failed.

#### The leash keys

The original's L (action 26 of `GGame::DoAction` 0x63F940: the byte table 0x63FF58 sends it to case 11 of 0x63FF0C,
0x63FC0D) only tests that the local player has a creature (g_game player +0xA64) and sends packet 0x5F, the hand's
click on the creature (0x63FC2C): at the next turn its handler clears the leash picker's pending flag and goes through
`fn_005D06E0`'s tests, the same as the click and `TOGGLE_LEASH`.

V (action 31) and B (action 32) 📼 1.20 only each take the local interface's status (`MyInterface` +0x39C, its `GLeashStatus`
+0x12C) and its type +0x1C, step it, and send `SetPacket(0x65, type, 0)` only when creature +0x1108 + 4·type is 1, the
leash known (0x63FDDD..0x63FEEF): the temple's pick packet, applied at the next turn. The player is g_game +0x18 +
0xA60 × [g_game +0x205A59], its creature at +0xA64; no creature, no packet.

- **V**, the case at 0x63FE75: 1 → 2, 2 → 3, 3 → 1 (the packet sent at 0x63FEEF).
- **B**, the case at 0x63FDDD: 1 → 3, 2 → 1, 3 → 2 (the packet sent at 0x63FE63).
- A type outside 1..3 sends the action number itself (`[esp + 0x1C]` at 0x63FE0F / 0x63FEA7 is `DoAction`'s argument,
  after the prologue 0x63F940..0x63F95A and read at 0x63F96B for the jump table: 32 for B, 31 for V) [V], and only when
  the creature's +0x1108 + 4 × that number is 1 (0x63FE50 / 0x63FEE0). No data reaches it, since +0x1C starts at 2 and
  only 1..3 are written.

Packets 0xF / 0x10 are not the keys' path.

### The leashes a creature knows

The leashes known are the creature's, four ints from Creature +0x1108 (bw1-decomp `Creature.h`: `field_0x1108`,
`ScriptFlag0..2`): +0x110C evil, +0x1110 rope, +0x1114 good, by the leash type's number (+0x1108 + 4 × type); +0x1108,
type 0, is no leash's. Every writer of the four (the frozen exe disassembled for `[reg + 0x1108..0x1114]`):

- `Creature::SetToZero` 0x474311..0x474323: all four 0 as a creature is made;
- `DEV_FUNCTION(2)` +0x1110 = 1 (0x6FC1EA), `DEV_FUNCTION(3)` +0x1114 and +0x110C = 1 (0x6FC233, 0x6FC25C);
- the mind file: `CreatureMental::SaveMind(Archive&)` 0x4E79D0 writes all four, as they are, at 0x4E7EB6..0x4E7ED5;
  fn_004E84A0 reads them back over the creature's (0x4E8A6F..0x4E8A96, a version 27+ file). fn_004E84A0 is the mind
  load of `Creature::LoadFullyQualified` (0x4E82D0, reached from `Creature::Load` 0x4E7FF0, the `LOAD_MY_CREATURE`
  path fn_00552AC0 at 0x552B6C), of `CreatureMental::LoadMindIntoPreexistingCreature` (0x4E846F; its callers are
  `Creature::SwapMinds` 0x47C0D7 / 0x47C1AB and fn_0065BA40) and of `Creature::ResolveLoad` (0x4E6CE1);
- packet 0x4E's handler (`GPacket::ProcessPacket` 0x63DA5F..0x63DA7C, after the creature is made or loaded) and
  fn_0065BA40 (0x65BB9E..0x65BBBB, the computer player's creature loaded from `.\Scripts\CreatureMind\%s`, after
  `LoadMindIntoPreexistingCreature`): all four 1, then `MoveToDevelopmentPhase(13, 0)`;
- a saved game: `GGame::Save` packs +0x110C, +0x1110 and +0x1114 of the local player's creature into one byte
  (0x554205..0x554215) and `GameOSFile::LoadAllGame` puts them back (0x558B2A..0x558B58).

Readers: `GLeashStatus::SetOn` needs +0x1110 non-zero (0x5E6F91), fn_005D06E0 reads it (0x5D0705), `GPacket::ProcessPacket`
reads it at 0x63D444, fn_004CF040 counts the non-zero ones among the three, and the V / B keys need the picked
type's int to be exactly 1 (0x63FE50 / 0x63FEE0).

**From land to land.** The land change saves the creature with `SaveMind` (`GGame::ClearMap`, 0x552CE9; see
[The player's creature](players-creature.md)) and the next land's `LOAD_MY_CREATURE` reads the file back, so the
leashes it knew come with it. On Land 1 with the start-up answer 3, `LOAD_MY_CREATURE` at turn 7 reads the game
folder's first profile's `C4ba71b36.erc`, whose four ints are 1, 1, 1, 1 (read from the frozen copy), so the creature
knows the three leashes from turn 7, before `DEV_FUNCTION(2)` and `(3)` at turn 30.

**A skirmish.** `GGame::Init` sends packet 0x4E for the local player (0x54FDD1) when g_game +0x205A0C (the skirmish
flag, also read by `GComputerPlayer::InGameInit` at 0x6570E1) is set or the game is a multiplayer one (0x54FD24..
0x54FD35), the local player has a citadel (player +0xA48, 0x54FD52) and its leader interface status is the local one
(`GetLeaderInterfaceStatus` = `MyInterfaceStatus`, 0x54FD67..0x54FD77); its point is the cell of the citadel's creature home (`Citadel::GetCreatureHomePos` 0x463A30,
0x54FD98 / 0x54FDC0). The handler loads the creature named by the sender's profile ("Dummy" without one, 0x63D86B..
0x63D882) with `Creature::Load` (0x63DA4F), or in a multiplayer game with `CLANCREATECREATURE<nn>` start data makes a
new one, and then the four ints are 1 and the phase 13: the skirmish creature knows every leash, whatever its file
said.

### Sources

- bw1-decomp: `src/Black/ScriptFunctions.cpp` (the native table), `include/chlasm/Enum.h` (the leash types),
  `CitadelHeart.h` (`TempleLeash`), `Player.h`; every function body read in the exe with the xref index (the natives
  disassembled with capstone).

## openblack

**Done**: the timed dominant desire, the hand's tie and taps, the citadel's leash off, the nine natives and the keys
(L at once; V and B through the leash type's message at the next turn). **Partial**: the needs sign (rules only), the clear's gate. The port decisions taken from raffclar's
code are in [Port decisions: the creature](../project/port-decisions-creature.md).

### The leash and the hand in openblack

`LeashRules`, `LeashRope` (40 masses on springs at 200 Hz), `LeashOwnership` (one leashable creature per player),
`LeashKeys` (L, V, B, checked against openblack's `input::GameActionMap`, read once a frame by
`ecs::creature_loop::ProcessLeashKeys`; his shake tracker is left out) and `CreatureHandRules`. raffclar's rule
allowed any god's creature in the hand; openblack holds the player's own or an allied player's. The let-go click is the
leash key. openblack's `HandPoint` is the hand entity's Transform and its rope starts at the hand
(`LeashSystem`'s `Step(rope, hand-or-tied, collar)`), so its u coordinate, its corners' sides and its tension's end run
from the other end (Pending).

### The timed dominant desire in openblack

The API is raffclar's `creature_spell_mind` (`SetCheatDominant`, `StepCheat`, `ClearCheatDominance`,
`HeldDownByCheat`, `MakeFullyDominantOverOthers`, `CheatTurnsLeft`, `k_CheatSeconds` = the spells' 20000 s as the
default seconds), with the state, `Cheat`, in `CreatureMindState::dominantDesire`. Behind it are the rules read above,
`creature_desires::detail::SetDominant`, `StepDominant`, `ClearDominant`, `HeldDownByDominant` and
`DominantTurnsLeft`, over `MakeFullyDominantWithFullSources`, `Unsuppress` and `UnsuppressAll` (tests
`test_creature_dominant_desire` and `test_creature_spell_mind`).

- The leash calls them (`LeashSystem`): `TakeMoodOf` as the leash is put on, changed or tied (GOOD and EVIL make their
  desire dominant for `k_LeashDesireSeconds`, ROPE clears), every turn again for GOOD and EVIL, the town rule
  (`creature_leash::WantsToImpress`, `k_ImpressTownSeconds`, the tie's 120 s for a town centre abode), the anger of a
  tied creature, and the clear as any leash but ROPE comes off; every call passes the body's needs free, the species'
  `desireFloor` and the whole turns a second from `game_clock::MsPerTurn()`.
- The mind counts it with `StepCheat` once a turn in `CreatureMindSystem::ProcessTurn`, before its sources and desires
  update. `creature_desires::FindDominant` is `FindDominantDesire`.
- The leash's turn runs while the creature fights or is knocked out too; only its moving it (the hand's pull, the
  walks back) waits for the fight to end. The forced strength the planner once took (max(value, 36000)) is gone.
- `MakeFullyDominantOverOthers` stays raffclar's: the desire let go, then, if active, at its maximum and every other
  at the floor, with no sources filled and no activation, as the Hunger calls at 0x4F7DE5 and 0x4F8174 do.
- **The creature spells**: `creature_spells::Apply` activates the desire and asks for the dominance at the start
  (`SideEffects::dominant`, which `SpellCreature` runs through `creature_spell_mind::SetCheatDominant`, needs too),
  Itchy's hold takes the leash off and asks for Scratch every counted turn, the end runs the least-dominant rule, and
  Nice and Nasty turn a leash into GOOD or EVIL (`SideEffects::leashType`). `SpellCreature` runs that after the
  dominance, as the original: `LeashSystemInterface::SetType` (the change `ChangeType` makes, without its test that the
  creature knows the leash; the leash's mood while one is on) and `temple_leash::TakeLeashType` for the creature's
  owner (the temple's pick and, for the local player, the gesture path wiped: the tail of the leash packet's
  `ApplyLeashType`, which keeps its own refusal under these two spells and, as the original's, no test that the
  creature knows the leash). Before, it called `ChangeType` before the dominance, refused for a
  leash the creature did not know, with no pick and no wipe, and the spell's dominance was the one that stayed. Test
  `SpellCreatureTest.OnALeashNiceAndNastySetTheLeashKnownOrNotAfterTheirMoodWipingTheLocalPath`. The stop at the start (`FinishActionUnsuccessfully`: always for
  Freeze, unless a script controls the creature for Nice, Nasty and Itchy) is `SideEffects::abandon`, which
  `SpellCreature` runs through `CreatureMindSystemInterface::AbandonAction` (the action, the walk, the agenda and the
  plan given up, and a cast let go of). Still to port: the clear at the end; openblack has no creature without an
  owner and keeps the leash's type per creature, not per player, so its gate has no counterpart yet.
- openblack calls `SetCheatDominant` from the leash, the creature spells, the badly hurt emergency and the scripts'
  SET_CREATURE_ONLY_DESIRE (`CreatureMindSystem::SetOnlyDesire`, the body's needs held down too, for the script's
  seconds; its OFF sets the desire to 0 and lets the others go only for a creature of no player or whose player's
  leash in use is the rope: `creature_leash::OnlyDesireLetsGo` over the worn leash's type, or with none on `CreatureLeash::selected`, the leash
  last put on or picked, which starts as the rope as the original's +0x1C does (init 2 at 0x5E6950, kept by the off
  path; the native's clear is fn_004DCF20 from 0x6F47E9). Before 2026-10-18 openblack read only the worn leash, so a
  creature with no leash on kept its others held down), and mimicry's copy uses
  `MakeFullyDominant` (Pending). openblack plans on demand and has no plan slots, so the clear has nothing to act on
  there (Pending).

### Tying the leash in openblack

- **The rules** are `creature_leash` (`Creature/LeashTie.h`): `TieTargetKind`, `ValidAsTarget`, `ValidAsLeashTarget`,
  `DecideHandTie(HandTieCheck)` → Nothing / Tap / Tie / Untie in 0x5D4210's order, and `DecideLeashTapOnObject` /
  `DecideLeashTapOnLand` → NotLeash / ActOnObject / ActOnPoint / Nothing (the land at the origin, `IsTapOrigin`: the
  first two coordinates bit for bit, the third as a float). The packets are `game_packets::Type::LeashTie` (0x5C),
  `LeashActOnObject` (0x5D) and `LeashActOnPoint` (0x5E).
- **`ecs::leash_tie`** (`ECS/LeashHandTie.h`) holds the hand area's entry points, `OnHandDoubleClick(player, target)`
  and `OnHandTap(player, target)` (a bool) / `OnHandTap(player, point)` (a `LeashTap`), which take the kind from the
  components (`LeashPost`, `SpellIcon` for every icon kind, `ScriptHighlight`, `OneOffSpellSeed`), decide and push the
  packet, and the turn's handlers, registered after the temple leash's in `Game::ResetRegistryForNewLand`.
- **0x5C** needs `Knows(Rope)`, `IsLeashed` and `Works`, then `LeashSystem::ReturnToHand` (`TakeOff`, then on again in
  the holder's hand with no `WhyNot` and no refusal kept, only when the creature knows the learning leash as `SetOn`'s
  on path, untied, whether it works kept, the untying sounds 148 / 149 (confirmed: oracle) in their own turn for the local player whether
  or not it went back on) or `TieTo`, whose result is not looked at, then always `PullAwayFromAction` and `ActOn`.
  `TieTo` itself tells the mind nothing (it only adds the thing shown, for the lessons), as the original's
  `fn_005E6BD0`: so the script's tie (`creature_leash::script::AttachToThing`, `AttachObjectLeashToObject` 0x6F456B)
  never sends the creature to act on the thing, and 0x5C's tie sends it once, through `ActOn`. Before, `TieTo` added
  the thing to `leash.actOn` itself, so the script's tie sent the creature to act and 0x5C's added it twice
  (`LeashSystemTest.ToldToActOnAThingItsMindIsToldEveryTime`). The testbed's tie commands (`TieLeash`,
  `TieLeashToCreature`) tie as the hand does: the tie, the pull away and `ActOn`.
  `PullAwayFromAction` stands for `FinishActionUnsuccessfully("pulled by leash", 1, 1)` (0x63D4AE): it stops the
  creature, a walk braking along its route to a stop since 2026-10-29 (`BrakeToStop`; it stood at once before), ends
  the body's own action as `EndAnyActions(1, 1)`'s body part does (0x48F71B..0x48F730: the dance's end
  `fn_004848F0`, state 6 → 7 or, while it starts, +0x5238 = 1 to end after the start; the fight `fn_004845F0`; the
  static action's end `fn_004848C0`, 3 → 4 or, while it starts, +0x5238 = 1; the point `fn_00484AE0`, 8 → 9), each
  playing its end: `creature_layers::EndLoop` and `EndDance`, and `StopPointing`, since 2026-10-29, the plan that
  follows (`Replan`) letting that end play out (`MindHooks::bodyEndPlaysOut`; before, the act that followed cut the
  body's action short at once), clears
  `CreatureLeash::returning`, `Control::Led` and `obeying`, and gives up the plan (`planActive`, `planner.current`,
  the idle plan). `ActOn` adds to the mind's `leash.actOn` on every call, as `ForceActOnObject` acts on every packet:
  the mind takes the last entry and clears the list, and the fight system finds a creature already fighting on a
  repeat, so the repeats change nothing. `ActOn` also counts the creature's turns since it was sent to act from none
  (`CreatureMindState::turnsSinceMadeToAct`, desire source 37), where `ForceActOnObject` zeroes mind +0x18C54
  (0x4C629B) on its way to acting (the feedback's lessons zero it too); [the attention its god pays it](mind.md#the-attention-its-god-pays-it).
  Then, for every thing, `ActOn` calls `PullAwayFromAction` (the agenda cleared and
  `FinishActionUnsuccessfully(0, 0, 1)`, step 5, ahead of the pick-up test at 0x4C6369), so a thing it can't pick up,
  or one it is told of with its hands full, also stops what it was doing before its mind takes the thing up; before,
  only the hold below did, and the search kept the plan it had and its walk
  (`LeashSystemTest.ToldToActOnAThingItIsPulledAwayFromWhatItWasDoingFirst`). With its hands empty (no
  `CreatureHeldObject`) and a thing it can pick up
  (`CreatureObjectActionSystemInterface::CanPickUp`, the game's pick-up test), `ActOn` does not queue the thing: it
  forces the plan
  (`CreatureMindSystemInterface::ForceActivity`, Curiosity, `HoldObject` on the thing, as fn_004C7590) and, when the
  plan is carried out, marks the thing (`MarkTargetAt`). Before, the thing went to the mind's search like any other,
  which keeps the plan it has unless a better one is found, so a tap on the leash lesson's cow while the creature was
  already curious about it left it standing by the cow (`LeashSystemTest.ToldToActOnAThingItCanPickUpWithItsHandsEmptyItHoldsItUp`;
  debug: the creature spawner's leash panel, the Tap line of each thing and **Tap the nearest thing**).
  A thing queued for the mind is also kept as its player's order (`MindHooks::ordered`, the last one; a hold clears
  it). When the mind's next plan takes the thing up and chooses no plan (`creature_planner::Choose` finds none, as
  the original's searches return 0), it forces Curiosity and `creature_leash::OrderedThingAction(CanPickUp)` on the
  thing, `HoldObject` (64) for a thing it can pick up and `ExamineByLooking` (20) otherwise, as fn_004C7590 (whose
  test is the thing's own pick-up test, so a thing it can lift is held up even with its hands full), and marks the
  thing when the plan is carried out; a plan found but not adopted forces nothing, as the original's adopted search
  goes on without the fallback. A thing the mind is given to act on otherwise (the gift used as led, a desire forced
  on a thing) has no such fallback. Before, its desires finding nothing left it with nothing to do
  (`CreatureMindSystemTest.SentByTheLeashToAThingItsDesiresFindNothingForItLooksItOverOrHoldsItUp`,
  `LeashSystemTest.ToldToActOnAThingItsMindKnowsItAsItsPlayersOrder`, `LeashRules.AThingItIsSentToAndCuriousAboutIsHeldUpWhenItCanBePickedUpElseLookedOver`;
  debug: the leash panel's "Sent to act on" line). Neither `ForceActOnObject` nor its searches
  (`FindSomethingToDoOnParticularObject` 0x4C52D0, `FindSomethingToDoOnObjectUsingObject` 0x4C5390) nor
  `ForceActivityAndForceAction` 0x4C4450 test whether a script controls the creature (only the point order does, at
  its gates), so the mind's plan takes up the leash's order on a creature under a script's control too, as it plans a
  desire forced on a thing; any other thing waits until the script lets it go, as before. Before, the order waited
  too (`CreatureMindSystemTest.UnderAScriptsControlAThingTheLeashSentItToIsPlannedAllTheSame`).
  The mind's obeying flag is `CreatureMindState::commanded`, apart from `MindHooks::obeying` (which keeps the mind off
  the body for a walk and stands for the flag during the walks): set after the forced hold (`ActOn`, whether or not
  the plan could be made, as fn_004C7590), after the fallback (the same) and after a plan adopted for the leash's
  order (step 6); cleared by `PullAwayFromAction` (`FinishActionUnsuccessfully`), by `Abandon` (what it does stopped
  as a failure, which every forced plan, `ForceActivity`, does first, as `ForceActivityAndForceAction` runs
  `FinishActionUnsuccessfully`), by `StopWhatItWasDoing` (`StopWhatIAmDoing`) and, before the mind's free choice and
  at the start of the mind's plan, once its agenda has no step left (as `ConsiderDecision`). It is also set by what
  else the original forces with it: `Faint` 0x476FA0 (`CreatureMindSystem::Faint` and the faint from what the body
  lacks), being sick at a meal's end (`CommandActivityAndAction` 0x4C4580 from the eating step sub_500540, action
  50 Puke: `CreatureMindSystem::Puke`), the cut scene's look at the camera (`ProcessState` 0x472DC0, action 314), the
  stroke's do-it-again (`RespondToPlayerFeedback` 0x4E06A0), and every kept action `ConsiderDecision` runs in place
  of a choice, after which it sets the flag (0x4F04B5: what a slap or stroke left it to do, `DoAfterFeedback`; the
  fizzle shown, `ShowFizzle`; the high, `BecomeHigh`) (`CreatureMindSystemTest.SickFaintedOrAfterAStrokeItChoosesNothingForItselfUntilItIsDone`).
  Its readers: the mind's
  plan (`PlanCreature`) plans nothing while it is set, as `ConsiderDecision`'s free choice; a point order's walk takes
  it as `ForceActOnMapCoord`'s k: set, or obeying, the walk runs and waits there (`ActOnPoint`). Before, a walk sent
  while the creature held up or looked over a thing at the leash's order walked with no wait, and its mind planned
  every turn of the order (`CreatureMindSystemTest.CarryingOutItsPlayersOrderItChoosesNothingForItselfUntilThePlanIsOver`,
  `LeashSystemTest.CarryingOutItsPlayersOrderASentWalkRunsAndAPullAwayEndsTheOrder`; debug: the leash panel's
  "Player's order" line). The reactions read it too: `IsAvailableForReaction` 0x4F2820 takes none up while it is set
  and the leash control +0x380 is 0 (`CreatureReactionRules::Availability::orderedByPlayer` = commanded and
  `CreatureLeash::control` Idle), and `ReactToCreaturePriority` (bw1-decomp `CreatureReaction.cpp:182`) passes another
  creature over while it is set (`Facts::orderedByPlayer`); before, both were always false
  (`CreatureReactionHandlerTest.CarryingOutItsPlayersOrderItTakesNoReactionUnlessTheLeashLeadsOrSendsIt`). As the
  packet handlers set +0x380 = 2 after `ForceActOnObject` (0x5C after its call at 0x63D504, 0x5D after 0x63D557),
  `ActOn` leaves the
  creature `Control::Sent`, whatever came of it, with `CreatureLeash::sentToThing`: it is not walked back within the
  leash meanwhile (the walk back's `sentByLeash`), and the turn ends that only once its mind has taken the thing up
  (`MindHooks::ordered` and `actOn` empty) and the plan for it has no step left, as `StopWhatIAmDoing` clears +0x380 and
  closes the mark then; an order onto a field keeps it the same way. Before, `ActOn` left it `Control::Idle`, so it
  was walked back within the leash while it acted and the mark stayed until the next order
  (`LeashSystemTest.ToldToActOnAThingItIsSentUntilItsMindIsDoneWithTheThing`; debug: the leash panel's "Led:" line).
  The thing is searched afresh: `ForceActOnObject` clears a plan of its own (a local `CreaturePlan`, cleared after
  the beliefs, step 5) and the searches fill only that one (`FindSomethingToDoOnObjectUsingObject` 0x4C5390 clears it
  each try), so the plans the agenda keeps from its desires' passes (`PlanState.plans`) are neither weighed nor
  changed. `PlanCreature` with a thing to act on (the leash's, a desire forced on a thing, a gift used as led) now
  plans into a set of its own (`onThing`), chooses only from it, and leaves `planner.best` as it was; before, a plan
  another desire kept from an earlier pass could be chosen in its place, and the thing's plans replaced the kept ones
  (`CreatureMindSystemTest.ForAThingItIsSentToOnlyThePlansFoundForTheThingAreChosenFrom`). Still not the original's:
  which plan the search returns. The original tries the desires one at a time (`CreatureDesiresTried`, the activity
  `ConsiderActivitiesWhichDontRequireAnObject` picks, then the thing or its associated beliefs if suitable and useful,
  `FindBestActionFromObject`) and returns the first valid plan; openblack takes the most pressing of every eligible
  desire's plan on the thing. Not ported yet, as no quest check sees it (2026-10-28): in the Land 1 leash lesson the
  creature is held to one desire (`set MyCreature only desire`, Curiosity), so one desire is tried either way, and the
  cow, once the hand is within 10 of it, is held up without a search; in The Pied Piper the tie only starts the quest's
  own forced walk and catch (actions 67 and 64), which replace whatever the search gave.
- **0x5D** tests only that a worn leash works (`!IsLeashed || Works`): openblack keeps the flag on the worn leash, so
  with none on the leash counts as working, as every leash starts. `TieTo` keeps no refusal of its own for posts,
  icons, highlights or seeds: the script's tie has none.
- **0x5E** checks `IsLeashed` and `Works`, then `LeashSystem::ActOnPoint(creature, point)`
  ([Sent to a point of the land](#sent-to-a-point-of-the-land)). The decision is the pure
  `creature_leash::DecidePointOrder(PointOrderCheck)` in the exe's order: `Refused`, `CarryAndPutDown`,
  `CarryOtherwise`, `CantGetThere`, `DrinkFromSea`, `LookAtReflection`, `ActOnField`, `Sleep`, `WalkThere` (the two
  put-down and throw-in-the-sea paths that need the hand to take something between its two reads are folded into the
  holding branch). The facts: `ControlledByScript` (both script components, as the walk back), the worn leash's
  works flag, `HandSystemInterface::IsInLockedSelect`, `CreatureNeeds` life, exhaustion and dehydration (as thirst),
  `CreatureHeldObject`, the worn leash being `Evil`, `land_avoid::IsPosValid(point, 7.1)` and, failing it,
  `NearestValid(point, 7.1, 999.5)`, `sea_cells::IsWater`, `map_cells::FindType(cell, Field)`, Water `activated`,
  the player's temple, `creature_home::HomeOf` to the point and the plan action 17. Carried out:
  - every order but `Refused` and `CantGetThere` (`AcknowledgesOrder`) is acknowledged first: for the local
    player's creature the InGame sample 1 2D (`k_OrderAcknowledgedSample`, the leash's `PlaySound`), then
    `CreatureAudioSystemInterface::PlayNow(creature, Acknowledge)`, the species' bank through the gate
    (`IsHeard`: another player's creature only with `SET_CREATURE_SOUND`) and nothing without a camera, as
    `PlayASoundEffect`; no game stream is drawn (`AcknowledgeOrder`). `ActOn` stands for `ForceActOnObject` and
    acknowledges the same way every time it is called, so a field order is heard twice, as in the original, and the
    tie (0x5C) and the act on a thing (0x5D) once (`LeashSystemTest.SentOntoAFieldItAcknowledgesTheOrderAndAgainAsItIsToldToActOnTheField`,
    `ToldToActOnAThingItAcknowledgesTheOrderEveryTime`). After the order is carried out, the same orders get the mark
    (`ShowTargetMarker`): the mark before closed down (`CloseDownSpotVisual`), then
    `StartSpotVisual(CreatureTarget, the place walked to on the ground, 20, the creature, 1.0)` kept in
    `CreatureLeash::targetMarker`; it is closed down by `PullAwayFromAction` and when the walk the leash sent it on is
    over (both stand for `StopWhatIAmDoing`). The spot visual is a synced effect, so the synced stream moves where
    it runs. The target is kept as the original keeps it: the point 2 m above the place (`TargetAbove`,
    `CreatureLeash::targetPoint`) and `hasTarget`; a plan its mind takes on a thing the leash sent it to
    (`LeashSystem::MarkTargetAt`, from the mind's turn once the plan is adopted) marks the thing instead: the mark at
    the thing's place, `targetThing`, and the point over the thing's top (2 m, then `GetHeight`); the close clears the
    thing and the flag; each turn a mark that has gone is forgotten and a thing that has gone is let go of with the
    flag (as `CreatureReceiveSpell::Process` 0x4F48C0 does). The debug bar's leash panel shows the target (tests
    `LeashSystemTest.WhereTheLeashSendsItIsMarkedUntilItIsDone`, `AThingTheLeashSendsItToIsItsTargetUntilTheThingGoes`,
    `LeashTarget.*`);
  - **The target marker its player sees** (the original's creature bookmark graphic, above): while the local player's
    creature (`LeashSystemInterface::PlayersCreature`) has a target, every frame that draws the land draws a yellow
    ring of misc0.raw's cell 56 at its target, and inside it the species' footprint, its cell from the creatures' land
    prints' table by the species' row (`leash_marker::FootprintCell`), turned −π/2 (`Sprite::angle`), sized from the
    camera bookmarks' pulse of that frame (`CameraBookmarkSystemInterface::GetPulse`): the ring as a bookmark's marker,
    the footprint at 0.7 of the pulse the other way round (`leash_marker::RingHalfSize`, `FootprintHalfSize`). It
    stands at the thing it was sent to as the thing stands that frame (2 m up and its height on top,
    `leash_marker::Position`), else at the kept `targetPoint`. Both are one Z object of the main pass's sorted list,
    keyed at its point, the ring first (`Renderer::LeashMarkerSprites`, `ZObject::leashMarker`); none in the temple.
    The two sprites are made for the frame that draws them and are not entities (`LeashMarkerArchetype::Drawn`), so
    showing the marker leaves the game's entities, their numbering and the state hash as they were; nothing the game's
    turn reads changes. Refitted from raffclar's marker (his cc891c5bd): his own pulse clock, his footprint's size
    worked out from the ring's, his footprint turned through its Transform and his two entities made at each order
    gave way to the original's shared pulse, sizes and sprite angle, and to sprites made for the frame. Tests
    `LeashMarker.*` (`test/creature/test_leash_marker.cpp`, pinned to the oracle); debug: the leash panel's "Target
    marker" line; testbed `leash.tap_land_marker`;
  - `WalkThere`: the turns since it was sent to act to 0; `MoveTo(the place, Walk, 0, PointOrderArrival(height))`
    (min(height, 8), an unordered height kept), `Run` when the mind already obeys; a move that does not start stops
    it, as the walk back; then the walk back's `SendWalking` (`returning`, `returningTo` = the point tapped, the forced
    ObeyPlayer plan of row 1 about itself, `obeying`, and any wait given up, as the agenda is cleared); when the mind
    already obeyed, `CreatureLeash::waitAfterWalk` = `SecondOrderWaitTurns(game_random::GameFloatRand(3.0),
    MsPerTurn())` (the draw after the move, before the help); beyond 40 of the point, `CurrentDesire` (ObeyPlayer,
    itself, the point) to the help system. Once the walk is over (not moving, not fighting) the wait is counted
    (`CountWait`, one a turn, wrapping as `dec`) before the walk ends: it stays Sent, returning and obeying until the
    count is over, and the walk ends that turn. `PullAwayFromAction` gives the wait up;
  - `CarryAndPutDown`: it walks to within its height of the point, never running and with no wait, and once it stops
    it puts the thing down (`CreatureObjectActionSystemInterface::PutDown`, clip 97); the walk is over once its hands
    are free (`CreatureLeash::putDownAfterWalk`; a walk that can't start puts nothing down). Test
    `LeashSystemTest.SentToAPointHoldingSomethingItCarriesItThereAndPutsItDown`;
  - `CarryOtherwise` (holding something on the aggression leash, or sent where it can't stand): it walks to where it
    throws from, eight times its height from the point (`creature_leash::WalkToThrowAt`; nothing to walk when already
    there), and once it stops throws the thing at the point on the ground (`CreatureLeash::throwAt`,
    `CreatureObjectActionSystemInterface::Throw`); its turns since it was made to act go on and it tells the player
    nothing, as the put-down. Tests `LeashRules.ToThrowAtAPointItWalksToEightTimesItsHeightFromIt`,
    `LeashSystemTest.SentToAPointHoldingSomethingOnTheAggressionLeashItWalksToThrowItThere`;
  - `ActOnField`: `ActOn(creature, field)`;
  - `CantGetThere`: `FailureExplanation` with `k_Inaccessible` (3);
  - the others (the sea's drink and reflection, sleep) do nothing yet (Pending); `Refused` nothing.
  Whatever the order, `CreatureLeash::Control::Sent` stands for +0x380 = 2: the walk back is off while it holds
  (`WalkBackCheck::sentByLeash`), the tug counts it as led, and it ends, as `Led` does, the turn the creature is not
  moving and not fighting. The debug bar's leash panel has **Tap the land ahead**, the hand's tap 50 east of the
  creature (`OnHandTap(player, point)`, so the packet is applied at the next turn), and shows the Sent state. Tested
  in `test_leash_rules` (`SentToAPointOfTheLandItWalksThereUnlessSomethingElseComesFirst`,
  `SentToAPointItWalksToWithinItsHeightAtMostEight`), `test_leash_system` (`SentToAPoint…`) and
  `test_leash_hand_tie`.
- **What the hand area adds**: the Action button's double click, taken in the idle state in place of the action press;
  the target (the object under the hand, else the nearest within 5 of the action point); a call to `OnHandDoubleClick`
  (on `HandTie::Tap` its own tap; either way +0x38 = 0 and `ResetActionState`); on a thing, its own `fn_005D36D0` step
  first (with the leash off, tied, or the thing not a reward), then `OnHandTap(player, target)` before its own tap
  (true: +0x38 = 0, `ResetActionState` and no tap); on the land, `OnHandTap(player, point)` before its own land tap
  (`ActOnPoint`: +0x38 = 0, no `ResetActionState`, no tap; `Nothing`: no tap and +0x38 left; `NotLeash`: its own tap).
- **The hand's part.** `input::MouseButtonsState` latches the Action (right) button's double click from the window's
  event (SDL `clicks == 2` on its down, standing for the original's double-click bits; the down still sends its
  press), a box drops it, and `input::TakeActionDoubleClick` takes it once a frame, kept from the hand while the
  temple's interior is up (g_game +0x205A28) or a hand demo plays. The hand (`HandSystem::ActionDoubleClick`, then
  `Update`) takes it only when idle (`hand_leash::TakesDoubleClick`: not gripping the land, no bubble grip, tug, grab,
  locked select, armed release or seed apply; holding something is idle, as 0x5D1330's holding press is reached from
  0x5D5190), before the press's near-object search and branches, and drops that frame's press (+0x38 = 0; the idle
  state's `ResetActionState` leaves it idle). A right double click in an idle hand is always eaten, as in the
  original. The target (`hand_leash::TieTarget`) is the hovered thing, else the action collide object
  (`_cursorObject`, not a hand), else `FindObjectNearMapCoord` at the action point, unfiltered as 0x5D39E0 is: the
  decision refuses a post, an icon, a highlight or the creature.
- **The taps.** `HandTie::Tap` sends the tap packet alone (`SendTap`, 0x5D38A0: not remembered, not the leash's act).
  `HandSystem::Tap` is 0x5D3930: the memory of the last tap (`RememberTapped`, fn_005D36D0, moved out of `SendTap`)
  unless the leash is on, untied and the thing a reward (`hand_click::MarksThing`; openblack's `IsReward` is whether the thing is a reward chest,
  `ecs::components::Reward`, as in bw1-decomp only `Reward` overrides it), then `OnHandTap(player, object)`, then `SendTap`. Every tap site of the press goes through it (a thing
  that cannot go into the hand, a tap-only thing, the grab released within 225 ms) and so do the knock and scroll test
  hooks. A press that takes no branch with an empty hand and no object under or near it is `ClickOnLand` 0x5D3D10:
  `OnHandTap(player, point)` at the action point ((0, 0, 0) with none, the origin). Its three results end where the
  press already ends, so they need nothing more; `NotLeash`'s own land tap (0x6DCB90) is not ported. No standard run
  double clicks the Action button or holds the leash: every check is expected identical.
- **Inside the citadel**: `TempleInterior::Activate` calls `leash_tie::OnGoInsideCitadel` before the pause
  (`LeashOff`, 0x5B), and `ApplyLeashOff` takes the leash off with `LeashSystem::TakeOff`; the temple's own turn
  dispatches no packets, as the original's. Tested in `test_leash_hand_tie`; testbed `leash.off_inside_citadel`.

### Pulling a tied leash in openblack

- **The rules** are `Creature/LeashPull.h`: `PullLength` and `PullSlackLength`, `RopeSegmentHit` (fn_00848D70),
  `PickRope` (fn_00848F90's loop) and `RopeTakesThePick`, tested in `test_leash_pull` against the oracle's own
  results.
- **The hand** (`HandLeashPull.cpp`): each frame with no action in progress, `RopeTakesThePick` projects the player's
  tied rope from the collar (openblack's rope runs from what it is tied to) and weighs it against the object, the
  screen object or the land under the hand, with the depths the screen object's arbitration uses. A press on it takes
  `hand_press::Branch::LeashPull`, after the screen object and before every object; `StartLeashPull` lays the two
  ropes (`LeashSystem::StartPull`, kept on `CreatureLeash::Worn::pull`, stepped in `LeashSystem::Update` and drawn
  in place of the tied rope while the action state is 21,
  [Drawing the leash in openblack](leash-rope.md#drawing-the-leash-in-openblack)); the let-go sends
  `LeashPullLength` (0x3E) and `LeashActOnPoint` through `leash_tie::OnHandPullLetGo`, and `LeashSystem::SetPulledLength` sets the rope's full and slack lengths. The action
  state is 21 throughout.
- **The points on the screen** are `creature_leash::ProjectRopePoint`, `LH3DTech::ProjectPoint` 0x819390 (the
  int-pixel form with the clip depth out, `?ProjectPoint@LH3DTech@@SIIPAULHPoint@@PAH1PAM@Z`): X, Y and Z through
  the world-to-clipping matrix each summed x, z, y (`affine::ToClipForScreenTest`), Z below the near clip [0xE839E0]
  returns 0 (not shown), else x = ftol((X · (1 / Z) + 1) · half width), y = ftol((1 − Y · (1 / Z)) · half height),
  `__ftol` truncating towards zero, and the call returns 1 off the screen or 2 on it: the pick takes both as shown
  (`setne` at 0x848FC3 / 0x849009). openblack feeds it the camera's CPU matrices (`CameraFrame::clipMatrices`) and the
  pixel near clip (`Camera::GetPixelNearClip`). The hit's depth is `RopeHitDepth`: the point `(b − a)·t + a`, then
  ((z · W2C[8] + y · W2C[5]) + x · W2C[2]) + W2C[11] (0x84916F..0x849191). The tied thing's point is `TiedPointOf`:
  its map place, at the land's height there plus its altitude (`MapCoordsOf`, `GetHeightAt`), as 0x5D5500 and
  0x5D9297 read it. Pinned in the oracle: `ProjectPoint` in 400 cases, the whole pick 0x848F90 in 200 (102 hits, the
  hit point and depth bit for bit), the release in 62; `test_leash_pull` keeps a share of each.
- The two ropes are laid with the lengths they are given, where the original lays them with the lengths left from
  its last pull and gives them the new ones after; openblack's lay (`leash_rope::Create`) spaces the masses along the
  line whatever the lengths, as the original's fn_00848600 does, so the two give the same rope.
- **The hand's clip** on the rope or pulling it is Chold_fingers ([The hand's clip](../hand/animation-and-placement.md#the-hands-clip)).

### The needs sign in openblack

The rules only, `creature_leash` (`Creature/LeashNeedsSign.h`): `NeedsSignRow`, `NeedsSignQualifies`,
`NeedsSignFill`, `NeedsSignArmsAtTie`, `NeedsSignArmsThisTurn` (`k_NeedsSignWants` 0.6, the float fill compared as a
double) and `NeedsSignAction` (`k_FoodForWorshipAction` 246 over `k_FishForWorshipAction` 300). The altar's three
signs now exist, with their desires ([the altar's need signs](../magic/worship.md#the-altars-need-signs)); nothing
calls the leash's rules yet: the tie and the mind's kept action are parked (Pending).

#### What openblack does with the leash natives

All nine are ported (`Creature/LeashScript`, called from `CHLApi.cpp`). `creature_leash::script` takes the leash
service and the things named, so it is tested with a fake (`test/creature/test_leash_script.cpp`); the natives in
`CHLApi.cpp` pop as the table above and push the defaults when there is no leash service. Both are dormant on Land 1
and Land 2: no leash opcode runs there with the start-up answer 3 (the `SET_DRAW_LEASH` calls are in
`CreatureDevLeashIntro` and `AttachToHouse`, which are skipped). The version 27+ mind file's leashes known are applied
when a creature is made from its file ([The leashes known in openblack](#the-leashes-known-in-openblack)).

- **A thing**: an id of 0 or an entity no longer valid is none, silently (the original's messages are empty). "A
  creature" is an entity with the `Creature` component, looked up through the const registry, so no storage is made.
  The "with a player" part is not checked, but by 187: the leash service refuses a creature its player can't lead.
- **185 / 269**: the second pop is the creature and the first the thing; when only the first is a creature, they swap.
  185 ties the leash (`TieTo`, which puts the picked leash on first if none is worn); 269 compares the tie.
- **186**: `LeashSystem::PutInHand`, fn_005E6EA0's port: untied, then `SetOn(creature, 1)` whatever the creature
  wears. Knowing the learning leash, the leash its owner picked (`CreatureLeash::selected`, +0x1C; the one worn when
  one is) goes on in the owner's hand through `PutOnUnchecked`: its mood taken (`UpdateMoodsFromLeash` through
  fn_005E6B00), the rope laid afresh, the lengths taken in the hand (`CalculateLeashLengthFromCreature`), the
  owner's temple's pick and gesture path (`TakeLeashType`); a leash already held keeps its `control` (the on path writes no +0x380) and whether it
  works. Not knowing it, the off path: `TakeOff`, or `ReleaseUnleashed` with none on. Then one of the two untying
  sounds in turn for the local player (the same toggle as packet 0x5C's untie, which is `SetOn(0)` then fn_005E6EA0,
  0x63D478 / 0x63D48A). The only refusal kept is openblack's one leash a player: a creature that is not the one its
  owner leads, or one held by another player, is refused (logged, kept as the last refusal), since the original's
  status is the player's and openblack's leash is the creature's (Pending). Before, openblack untied a
  tied leash, put one not worn on through `Toggle` (with `WhyNot`) and left one already held as it was (raffclar's
  tied/Toggle logic). `TieTo` for 185 still goes through `PutOn`'s `WhyNot` (Pending). Tests
  `LeashSystemTest.TheScriptsLeashToTheHandPutsTheLeashOnAgainWhateverItWears`,
  `LeashScriptTest.AttachToHandPutsTheLeashInTheHandWhateverItWears`; the creature spawner's "Attach to hand (script)".
- **187**: `TakeOff` when a leash is worn. With none worn, `ReleaseUnleashed`, as `SetOn(creature, 0)`'s off path does
  whatever the creature wears: the confinement radius to 0 (+0x11B4 at 0x5E70AA), the leash's control back to none
  (+0x380 at 0x5E70A4; already none with no leash on in openblack), the pull `LH3DCreature` +0x48B0 to 0 (0x5E709E;
  never set, see [the rope](leash-rope.md)), the dominant desire let go unless the leash picked on the player's status (+0x1C) is the learning leash
  (2), and the gesture path of the hand wiped when the creature's owner plays at this machine (0x5E70D9..0x5E7129).
  The same `ReleaseUnleashed` runs for every creature taken out of the game, after its leash comes off
  (`creature_removal`'s `TakeOffLeash`), as `Creature::ToBeDeleted` calls `SetOn(creature, 0)` (0x47502F) whether or
  not a leash is on. Not ported, with no counterpart: mind +0x1C9C..+0x21BB cleared (0x5E70B0..0x5E70C6; per action, read
  by `GetActionPriority` 0x4C39A6 and `CanCreatureCurrentlyPerformAction` 0x4F1C14 while confined; a scan of the exe's
  displacements finds no other writer) and `StopImmersion(7)` (force feedback). A creature with no player is left alone,
  as the native's own test (`GetPlayer`, vt +0x1C, at 0x6F466F..0x6F4693): openblack's test is
  `creature_leash::script::HasPlayer`, any owner but `PlayerNames::_COUNT`. The neutral player is a player (a
  playground's neutral creatures take its slot, [players-creature.md](players-creature.md)), and `_COUNT` is the owner
  of a creature a script's `CREATE` makes, which `Creature::Create` gives no player (0x6F14B6). Before, openblack left
  the neutral player's creatures alone and let a script-made one go (raffclar #567 took the neutral player for no
  player). Every shipped call names the player's own creature (`MyCreature`, 20 calls in 7 files), so no quest moves.
  Test `LeashScriptTest.EveryPlayerTheNeutralOneIncludedLeadsACreatureButNoPlayerDoesNot`. **222**: `IsLeashed`.
  **249**: `SetWorks`, set when the popped bits are not zero. The switch is the player's leash status +0x18
  (`GetLeaderInterfaceStatus` +0x12C, 0x6F4ED7), not the worn leash, so openblack keeps it on
  `CreatureLeash::leashWorks` with no leash on and copies it to the next leash put on.
- **322 CONFINED_OBJECT** (`GScript::ConfineObject` 0x6F5980, five pops: the radius, z, y (dropped), x, the thing):
  for a creature (vt +0x34), the radius to +0x11B4 and the centre to +0x11A8 / +0x11AC as `ftol(m × 6553.6)`, altitude
  +0x11B0 = 0. `Creature::IsConfinedToArea` 0x47CCE0 reads it: radius above 0 and no leash on, or a leash that works.
  openblack writes `CreatureLeash::confinementCentre` (map-quantised, y 0) and `confinementRadius`, which the leash's
  keep-within reads the same way; the per-turn hand, tie and young-home areas overwrite it as they do the game's.
  **323 CLEAR_CONFINED_OBJECT** (0x6F5A60): the radius to 0. Both are silent for anything but a creature.
- **275**: as the original (raffclar's "None → 0" is right only for the failure cases): the leash picked at the
  temple of the creature's player (`temple_leash::PickOf`, read through `leash_posts::ScriptLeashType`): 0 when the
  thing is missing or not a creature or its player has no temple heart, -1 with nothing picked, else 1, 2, 3. The
  posts' tap picks at once on the tapping side and packet 0x65 again at the next turn, as the original; the debug
  window can set it too.
- **305**: `script_control::CameraControl::drawLeash` = the popped value as it is; the leash draw reads it.
- **354**: the script player (a float, truncated) through `magic::ScriptPlayerToGamePlayer`, then the leash key for
  that player (`PressKey`, which also refuses a player with no creature to lead). In the port a script player out of
  0..8 does nothing: that range is `ScriptPlayerToGamePlayer`'s own, and it matches the original's at 0, 8 and below 0
  (see the comparison in Pending). The original reaches its own creature checks by another path (the leader's interface status, then
  `fn_005D06E0`'s tests); the port's are `PressKey`'s (`PlayersCreature`, then `WhyNot` in `Carry`). Compared, they
  let the same through:
  - the player's creature (+0xA4C) is `PlayersCreature`, the player's primary creature, which is always the one they
    lead (owned and leashable), so `WhyNot`'s owner and leashable tests never refuse it;
  - the ROPE leash known (+0x1110) is `WhyNot`'s `DoesNotKnowLearningLeash`, which `Carry` asks for every command of
    the key, putting on, taking off and untying alike (the rope for the last two); `CommandFor`'s own rope test then
    only says why nothing is put on;
  - the leash off or on this interface status is `WhyNot`'s `HeldByAnother` (one interface status per player here);
  - `IsAvailable` (vt +0x2C) has no test of its own: `WhyNot` asks for a valid entity with a `Creature` [?: whether a
    creature the original no longer counts as available is still the primary one here is not read].
  `fn_005E7140`'s untie, take off and put on are `CommandFor`'s (the picked leash when known, else the rope, the same
  for every type the leash type's packet can set). A tie to a town's desire flag (`TownDesireFlag`) sets the mind's
  flag state and this untie resets it ([the mind's state](plans-and-needs.md#the-draws-finishing-and-the-state); `LeashSystem::TieTo`, `Carry`), and
  `StartImmersion` (force feedback) is not ported. Pinned by `test_leash_system.cpp`
  `LeashSystemTest.TheLeashKeyIsRefusedUnlessTheCreatureKnowsTheRopeLeash`.

#### The leash keys in openblack

L, V and B are read once a frame, after the temple's room keys: the first of the three that went down this frame
(down and changed, as the room keys are read) goes to `PressKey` for the local player, only when the player has a
creature they can lead. They are not read while the debug windows have the keyboard (the actions are not framed then)
nor while the world is paused in the citadel. The port's L calls `PressKey` at once instead of sending packet 0x5F
(Pending). V and B send packet 0x65 as the original's: `PressKey` steps the picked leash (`CommandFor`, `StepType`)
and, when the creature knows the leash it lands on, pushes `game_packets::Type::LeashType` with it for the player,
asking nothing else (no `WhyNot`: the original tests only that the player has a creature and the flag of the leash);
at the next turn `temple_leash::ApplyLeashType` changes the creature's leash (`SetType`, its mood while one is
on; whether the creature knows it is the keys' test only, as the original's handler has none), refuses it under
the compassionate or angry spell, sets the temple's pick and wipes the local player's gesture
path ([the temple's leash posts](temple-leash-posts.md)). Before, V and B changed the creature's leash at once through
`Carry` and `ChangeType`, with no pick, no wipe and the leash key's refusals. Test
`LeashSystemTest.TheLeashStepKeysSendTheStepAsTheLeashTypesPacket`; scenario `leash.keys` reads "sent" for them.

### The leashes known in openblack

A creature's leashes known are the `known` bits of its `CreatureLeash` component (`LeashSystem::Knows` / `SetKnown`),
one for each of the evil, rope and good leashes; a new creature knows none.

- **Saved.** `player_creature::ToMindFile` writes them into the mind file's `leashFlags` through
  `creature_mind_body::ToMindFile`, by the type's number, with the first type's flag kept as the loaded file had it.
  The land change (`Game::ChangeLand` → `SaveMyCreature`) saves the local player's creature so.
- **Loaded.** `creature_mind_body::FromMindFile` reads them back (`LeashesFromFlags`: evil, rope and good by the
  type's number, a flag that is not zero is known; none from a file older than version 27) into `Body::leashes`,
  carried by the load plan (`player_creature::LoadPlan::leashes`). `MakeFromMind`, the step shared by
  `LOAD_MY_CREATURE` and `LOAD_CREATURE` (and the computer player's creature), then calls `RestoreLeashes`, which
  sets each of the three known or not as the file says, over what the creature knew, as fn_004E84A0 does. So the
  leashes known come back on the next land, and on Land 1 the creature `LOAD_MY_CREATURE` loads at turn 7 knows what
  its file says from then on. `LOAD_CREATURE` and the computer player's creature then know all three
  (`SettleScriptLoaded`), as packet 0x4E's handler and fn_0065BA40 set them.
- **A skirmish.** openblack has no skirmish start; the debug menu's Load Island with "Take your creature along" on a
  playground stands for its packet 0x4E: the profile's creature, made at the first player's temple, is then told it
  knows the evil, rope and good leashes (`KnowAllLeashes`, whatever its file said) and set to stage 13
  (`debug::gui::LoadIsland`; `test_island_load`). Not ported: `GGame::Init`'s own send of the packet at a skirmish's
  start, and the packet's turn (the load happens as the land is loaded).
- Tested in `test_creature_mind_file_body` (`TheLeashFlagsAreTheEvilRopeAndGoodLeashesByTheirNumber`,
  `TheBodyKnowsTheLeashesOnlyOfAFileThatKeepsThem`) and `test_player_creature`
  (`ALoadPlanTakesTheLeashesTheFileKeeps`, `ALoadedCreatureKnowsTheLeashesItsFileSaysAndForgetsTheOthers`).

### Test hooks

- The creature spawner's leash section has "Tie with the hand" (closed by default): the decision for the thing last
  clicked and the nearest thing, and the tie and untie packets.
- Testbed scenario `leash.off_inside_citadel`.
- The leash panel's "Target marker" line (the local player's creature only): where its marker is drawn this frame,
  the ring's half size and the footprint's picture; testbed scenario `leash.tap_land_marker` sends a tiger to the land
  with a quick click and shows it.

## Pending

- **The leash's hand end**: openblack now steps a held rope from the drawn hand's wrist, the first bone, as the
  original ([The leash's hand end](leash-rope.md#the-leashs-hand-end)); which of the original's rope readers read the
  +0 end was not followed.
- **The timed dominant desire** ([The timed dominant desire](#the-timed-dominant-desire)):
  - `SuppressDesire` also empties the held-down desire's plan slot (mind +0x718 + 0x30·d, `SetToZero` 0x4F12B0): the
    per-desire slots of the original's time-sliced planner (0x4F0560, two desires a turn); queued as a fix with that
    planner;
  - the body-backed sources' setters (14 low energy, 0x4DE8F0 writes `1 − v` into the energy; 21, 22, 32, 36, 37):
    queued as a fix;
  - the other callers of the clear (the end of the Compassion, Anger and Scratch spells) are not wired; queued as a fix
    with the creature spells' mind (`SET_CREATURE_ONLY_DESIRE_OFF` is, through `creature_leash::OnlyDesireLetsGo`);
  - the other callers of `SetCheatDominantDesire` (feedback, mimicry, magic info, computer player): queued as fixes,
    one a caller; the low life one is ported with the creature's emergencies (badly hurt, RestoreHealth with the
    body's needs held down too: `CreatureMindEmergencies.cpp`, `creature_emergencies::IsHurt`, test
    `CreatureEmergencies.LifeBelowFourTenthsIsHurt`);
  - `MakeDesireLeastDominant` 0x4DC9F0 gives the species' floor where raffclar's `WearingOffItIsWantedLeast` expects
    the floor over 1.3; queued as a fix;
  - a creature whose desires are not made yet takes no mood from a leash: the original draws them in `Creature::Create`
    ([Random streams](../engine/random.md#random-streams)); queued with that fix;
  - two creatures leashed together: the original runs each creature's whole turn in the list's order; queued with the
    creature's turn inside `Living::ProcessLiving`;
  - the tie's `ShowNeedsVisuals` branch (mind +0x21D0 = 0x50A760, `fn_0071A200` > 0.6) waits on the needs signs;
  - with more than 1000 ms a turn the original's division would fail; no data reaches it.
- **Tying the leash with the hand** ([Tying the leash with the hand](#tying-the-leash-with-the-hand)):
  - the double-click bits are read; still open: whether the original's first reader (fn_0046F5F0, from
    fn_00470DE0 / fn_00471130) reads them once a frame, as openblack takes them; `fn_00470B30`'s timed +0x6520; which of
    openblack's hand states are the idle action states 0 and 1; the idle process 0x5D5190's other branches (g_game
    +0x14 & 4 at 0x5D5227, and +0x38 & 2);
  - 0x5D5608 sends 0x5E and then resets the action state and +0x40; openblack's `TapLand` keeps the action state, and
    which of openblack's paths this site is is not settled;
  - after 0x5E, `Control::Sent` for a walk or another order at a point ends the turn the creature stops moving (the
    original clears +0x380 in `StopWhatIAmDoing`, 0x475BD2, so a wait or a put-down after it are covered by the turn
    loop's own tests, and the end of the other orders' plans is not compared); queued as a fix;
  - `ForceActOnObject` 0x4C5FF0's own tests (`IsLeashWorking`, the fight, the life, the desire and `KnowsAction`):
    queued as a fix;
  - after the forced hold, the pull away first, the fallback and the order's flag (`LeashSystem::ActOn`, the mind's
    plan, `CreatureMindState::commanded`), still not ported, each its own fix:
    - the mind's obeying flag (mind +0x1C14, `CreatureMindState::commanded`): its writers whose callers openblack does
      not have yet, each with the row that ports the caller: `MoveByTeleport` 0x479F70 (from `MagicTeleport::DoTeleport`
      through vt+0xAF0: [locomotion.md](../project/progress/creature/locomotion.md), "Travelling by teleport");
      `TrappedInEnclosedSpace` 0x47D9A0 (from `DoMovingAction`, its kept action 0x50A7E0 too: locomotion.md, "Stuck or
      trapped in an enclosed space"); `GiveFoodToWorshippers` 0x50A760 (the needs sign's kept action: [The leash tied
      to a needs sign](#the-leash-tied-to-a-needs-sign), below); the fish farm's branch of `ForceActOnObject` (with
      the big forest's, queued with `ForceActOnObject`'s gates); `BeSad` 0x509130 (every slap's kept action, set at
      0x4E09B7 before the `GameRand(2)` that may put `BeFrightened` or `BeHappy` in its place, and by fn_0047EC10;
      the not-learning path's synced `GameRand(2)` at 0x4E0B67 (0x6DE510; the help request before it draws nothing)
    replaces it; openblack's slap leaves no kept action but the frightened one: [mind.md](mind.md), the feedback; not
    ported yet, as the Land 1 punishment lesson cannot see it (2026-10-28, `refs/chl/CreatureDevelopment.txt`
    6550..6681: it waits on the interaction magnitude, which the slap itself sets, then forces action 82 and plays 56
    or 55 on the creature by script, which take its body before the kept action's free choice comes (inferred from
    the script, not run); the creature is
    released only after the lesson's last check)) and
      `RepeatLastAction` 0x50A1F0 (the kept action after a stroke while it carries a thing to eat, with its forced
      hunger action 11, and fn_0047EC10's; openblack has neither branch); `ResolveLoad` 0x4E6AB0 (a saved creature's
      plan in progress: openblack does not save a creature's agenda). Never reached in this game:
      `SetupReactToTeleport` 0x4F3A60 (vt+0x9F0, no call site), `LeashForceCreatureToActOnHandObject` 0x4CECF0 (no
      caller), `LeashMakesCreatureMoveToHand` 0x4CE8E0 and through it `LeashForceCreatureToActOnAttachedObject`
      0x4CEE70 / `TellCreatureWhatToDoWhenAttachedToMe` 0x4CEE80 (only the tug's packet, which nothing sends:
      [leash-rope.md](leash-rope.md#the-leashs-tug)). The point orders and `ForceGoToMapCoords` 0x4C7220 (also the
      pen's walk from `ProcessState` 0x473274) keep openblack's `obeying`, which stands for the flag on a walk. Its
      readers openblack lacks: `ForceActOnObject`'s own k
      (`SetRequiredSpeed` of the run), the
      stored commands (`ProcessStoredCommands`) and the kept action (not in openblack), `LeashMakesCreatureStop`
      0x4CEC00 (the tug, which never runs); its save (`ResolveLoad`). The mimicry of another creature at
      `CreaturePlan.cpp:198` is inside bw1-decomp's `VERSION_BW1WCI` block, not in this exe's `ConsiderDecision`;
    - whether a young creature lifts the leash lesson's cow: openblack's `CanPickUp` runs the original's test in its
      order (`Object::CanBePickedUpByCreature` 0x4E3A20, then 0x4E3960: the leash not tied to it, the two
      `Get2DRadius` within the pick-up reach, the weight within `MaximumPickUpWeight` 0x4E3D30 = `GetWeight` × 0.8,
      `creature_pick_up::k_LiftShare`, then the thing's own `CanBePickedUp`, which reads `CannotBePickedUp`). The
      weight passes by far: the cow's info.dat row (`Scripts\info.dat` 0x8008, its `GObjectInfo` weight at +0x9C) weighs
      35.0, at most 59.04 at its largest age scale 1.19 (scale³ × weight); `LH3DCreature::GetMass` 0x47FA80 in the oracle
      at the start size 0.22 with thin-fat and weak-strong both −1 (the lightest build) gives 431.34271 (0x43D7ABDE),
      so it lifts 345.07, and the same formula in openblack (`physics::living::CreatureMass`, in the same order) gives
      the same; a creature lifts a cow at its largest from a size of about 0.122 with that build. The reach side
      (rO + rC against fn_00485DF0(0xE)) is the pick-up animations' hand points, ported and measured as the original
      ([animals.md](../beings/animals.md)), but its numbers for the lesson's species and size are not checked
      (they need the game's meshes). The lesson's cow starts unable to be picked up and is made pickable once the hand
      is within 10 of it, so a tap before that goes to the search and so to `ExamineByLooking`, as in the original;
  - `ForceActOnMapCoord` 0x4C6540 ([Sent to a point of the land](#sent-to-a-point-of-the-land)): the walk, the field,
    can't-get-there, the put-down and the throw are ported (above). What openblack does not do yet: the other orders
    (drinking from or looking into the sea, which also write mind +0xFE4; sleeping near home), each queued as a fix;
    the throw at a distance of 0, where the original divides by it and openblack throws from where it stands; the mind's active flag +0x20CE4 and
    +0x10AC (not named) are not kept, so they never refuse; `StartConcurrentAction(200)` after the
    acknowledgement and after `ActOn`'s; `EndAnyAnimsRapidly`; `ActOn` acknowledges past none of
    `ForceActOnObject`'s gates (queued with them above) and with no big-forest or fish-farm branch;
    `ForceActOnObject`'s own `CurrentDesire` help once a plan is found (not ported); the help
    prompts +0x19C, +0x1B4 and +0x1C0; mind
    +0x1C38 = 0; the first order's speed (the original leaves the required speed as it was, openblack walks); the
    wait's pause while the body performs a body action (openblack counts every turn the walk is over); whether the
    wait's first count falls on the turn the move ends or the next (openblack: the turn it is seen over, inferred);
    the spiral point kept back as the order's `MapCoords` (×6553.6) for the marker;
    `[0xC64150]` (written only by `Creature::OnClearMap`, 0) is taken as 0;
  - `PullAwayFromAction`'s stop: the original's tie ends the action as finished unsuccessfully with both its walk and
    its body flags set, so a walking creature brakes along its route to a stop, and its body ends its own action (a
    static action's loop plays its end, a dance its finish, a point held is let go), which the plan that follows lets
    play out. openblack stands the creature at once and the act's plan cuts the body off. Written, being measured: the
    creature walk series (the leash tie's pull-away brake and the body action end,
    [routes](../beings/routes.md#the-creatures-walk-on-the-follower)). Not checked after it: the end of a point the
    body holds (the pull-away's pointing end) against the original's;
  - a walk the leash sends it on (the walk to a tapped point, to put a thing down or to throw it) that is refused
    stands a walking creature at once; the original's failed move brakes it to a stop. Written, being measured, with
    the series above ([the walk back in openblack](leash-rope.md#the-walk-back-in-openblack));
  - `PullAwayFromAction`'s leftovers (`ModifyStrength([0xC6DAF0])` with 3D +0x528C, the sub-action's end call through
    0xBE0C18, the partner's stop and the rest of 0x47573E..0x475A7A): queued as a fix;
  - the tie's sound: 2D for the local player, at the object for another (`fn_005E6BD0`); queued as a fix;
  - the leash's "works" flag is the player's (`GLeashStatus` +0x18, untouched by `SetOn`); queued as a fix;
- **The leash tied to a needs sign** ([The leash tied to a needs sign](#the-leash-tied-to-a-needs-sign)): only the rules
  are ported. The worship totem's three signs and their desires are ported ([the altar's need
  signs](../magic/worship.md#the-altars-need-signs): three objects made right after the altar, so they take the three
  creation indices after its own). Parked: the tie on a sign (the casts to the sign's altar and its site at the tie and
  every turn, `creature_leash::NeedsSignArmsAtTie` / `NeedsSignArmsThisTurn`); the mind's kept action (mind
  +0x21D0..+0x21E0, run once at the free choice `fn_004F0370`, then cleared to {0, 0, 0, −1}); actions 246 and 300 in
  the creature's action set; and what `fn_004C74E0`, Creature +0x94 and +0x384 stand for in the free-choice test, and
  the second argument (+0x21F0) of the kept call (unused by 0x50A760). The fill is a float as the game's 24-bit FPU
  rounds the division; that its precision control is 24 bits here too is taken from the engine's setting, not read at
  this call.
- **The leash keys** ([The leash keys](#the-leash-keys)): the original's L only sends packet 0x5F (action 26,
  0x63FC0D); queued as a fix.
- **Leash natives, openblack's differences** ([What openblack does](#what-openblack-does-with-the-leash-natives)):
  - `ATTACH_OBJECT_LEASH_TO_OBJECT`: `SetOn` 0x5E6F70 refuses only a creature that does not know the rope leash, and
    then takes the leash off; openblack refuses more through `WhyNot`. Queued as a fix. `ATTACH_OBJECT_LEASH_TO_HAND`
    refuses only a creature its owner does not lead, or one another player holds: the original acts on the player's
    status, whichever of their creatures it names; that waits on one leash state per player (below);
  - `TOGGLE_LEASH`'s player: 0 is the player at g_game +0x205A5B (the neutral one, 7), n is n − 1, returned
    unsigned (0x6EB9A0); `GetPlayer` 0x5509B0 gives none from 8 (`cmp eax, 8; jb`, so a negative script player is
    none too). openblack's `ScriptPlayerToGamePlayer` gives the same: 0 and 8 the neutral player, 1..7 players one to
    seven, below 0 and from 9 none (`test_leash_script`). With none the original goes on with a null player (no
    script passes one); openblack does nothing. `IsAvailable` (vt +0x2C) is not compared [?];
  - one leash state per player: `IS_LEASHED_TO_OBJECT` acts on the player's; queued as a fix. `SET_LEASH_WORKS`'s
    switch is kept with no leash on (raffclar #567), but is still the creature's, not shared by the player's creatures;
  - the leashes known are one bit each: a flag of a mind file that is neither 0 nor 1 is known to openblack, as to
    `SetOn` and fn_004CF040, but the original's V and B keys need it to be 1 (0x63FE50 / 0x63FEE0); no writer of the
    original makes such a flag. The first type's flag (+0x1108) is not modelled, only kept in the file;
  - `Creature::SwapMinds` loads each mind into the other creature through fn_004E84A0, so the leashes known go with
    the minds; openblack's `TakeUpBody` (the body's side of a swap, called by `CreatureMindSystem::SwapMinds`)
    takes no leashes, so in openblack the leashes known stay with the bodies;
  - which creatures have no player (Creature +0x1070 null, `Creature::Initialise` 0x474397's argument) is not read.
- **Leash natives** ([Leash natives](#leash-natives)): no native body is in the decomp, everything is a reading of the
  x86 (the unnamed functions and the `GLeashStatus` fields are read in [The leash keys](#the-leash-keys) and
  [Common facts](#common-facts)). Left: no shipped CHL `Enum.h`; the PPC Mac bodies are not disassembled.
