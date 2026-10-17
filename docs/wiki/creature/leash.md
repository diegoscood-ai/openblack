# The leash

## Summary

The leash ties the player's creature to the hand, to a thing in the world, or lets it go. Its type (the compassion
leash GOOD, the aggression leash EVIL, or the learning leash ROPE) is picked at the temple's posts and forces a desire
on the creature for as long as it is worn: the **timed dominant desire**. A right double click with the leash in the
hand ties it to the thing under the hand; a single tap sends the creature to act on a thing or a place; a needs sign
of a worship site makes it feed or fish there. Nine script natives and the L, V and B keys drive the same state.

openblack ports the leash's rules (`creature_leash`, `LeashSystem`), the timed dominant desire, the hand's tie and taps,
the nine natives and the keys. Still different: the keys act at once instead of through packets, the needs sign is
rules only, and the clear's per-player gate has no counterpart yet. The rope itself, its drawing and the walk back are
in [The leash's rope](leash-rope.md#the-leashs-rope); the temple's posts in
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
  leash off and does not make it dominant beforehand). Each end calls `MakeDesireLeastDominant(d, info, 1.3)`
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
  then the player's creature's moods) from `GLeashStatus::SetOn(on)` (0x5E7001, before the lengths) and from the type
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
    stored back (the static starts at 1, so 0x94 first, then 0x95, in turn). An object: `fn_005E6BD0(creature,
    object)` (the tie: its own sound turn [0xBF362C], +0x24, +0x34, `SetOn(1)`, the tied lengths, `MimicPlayer`, the
    town and needs-sign branches; the sound is 2D for the local player, at the object for another),
    `FinishActionUnsuccessfully("pulled by leash", 1, 1)`, `ForceActOnObject(object)` 0x4C5FF0, Creature +0x380 = 2.
  - **0x5D** (0x63D4B6): the sender's interface status's leash works (+0x18), a creature and an object:
    `ForceActOnObject(object)`, +0x380 = 2. No test of the leash being on or of the learning leash.
  - **0x5E** (0x63D57E): a creature, `GetInterfaceStatusLeashOn` ≠ 0 and works: `ForceActOnMapCoord(point)`
    0x4C6540, +0x380 = 2.
- **`FinishActionUnsuccessfully`'s parts** that a pull away from an action touches: it stops the creature
  (`StopWhatIAmDoing`'s `fn_0048F710(1, 1)`), clears the creature's returning flag (+0x11B8, 0x475C4A), its led
  control (+0x380, 0x475BD2) and the mind's obeying (+0x1C14, 0x475773 and 0x475C1F), and gives up the plan (the
  agenda cleared at 0x475A86 and the plan's desire +0xF50 = 0x28 at 0x475A91); the rest of 0x47573E..0x475A7A includes
  `ModifyStrength([0xC6DAF0])` with 3D +0x528C, the sub-action's end call through 0xBE0C18 and the partner's stop.
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

**Evidence.** The native table's "NONE" entry is at 0x00C0DB98, as `bw1-decomp/src/Black/ScriptFunctions.cpp:29-30`
says. bw1-decomp has the table entries (`ScriptFunctions.cpp:232-401`) and the symbols
(`config/BW1W120/symbols.txt:15400-15423, 15711`) but no bodies; the bodies below were disassembled from
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

`bw1-decomp/include/chlasm/Enum.h:556-565` (Lionhead header, "Jonty Barnes 2001-11-15", via Daniels118/chlasm):
`LEASH_TYPE_NONE = -1`, `FIRST = 0`, `EVIL = 1`, `ROPE = 2`, `GOOD = 3`. The original `LAST = 4` is commented out; the
decomp changed it to 3. The game data ships no CHL `Enum.h`.

The temple stores the type as a 0-based index in `TempleLeash::field_0x0` (`CitadelHeart.h:77-80`), -1 for none:

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
`dynamic_cast<Creature>` → `GetPlayer()` → `player->citadel` (+0xA48, `Player.h:95`) → `citadel->heart` (+0x30) →
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

### Sources

- bw1-decomp: `src/Black/ScriptFunctions.cpp` (the native table), `include/chlasm/Enum.h` (the leash types),
  `CitadelHeart.h` (`TempleLeash`), `Player.h`; every function body read in the exe with the xref index (the natives
  disassembled with capstone).

## openblack

**Done**: the timed dominant desire, the hand's tie and taps, the citadel's leash off, the nine natives and the keys
(acting at once). **Partial**: the needs sign (rules only), the clear's gate. The port decisions taken from raffclar's
code are in [Port decisions: the creature](../project/port-decisions-creature.md).

### The leash and the hand in openblack

`LeashRules`, `LeashRope` (40 masses on springs at 200 Hz), `LeashOwnership` (one leashable creature per player),
`LeashKeys` (L, V, B, checked against openblack's `input::GameActionMap`, read once a frame by
`ecs::creature_loop::ProcessLeashKeys`; his shake tracker is left out) and `CreatureHandRules`. raffclar's rule
allowed any god's creature in the hand; openblack has no alliances, so only the player's own. The let-go click is the
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
  Nice and Nasty turn a leash into GOOD or EVIL. The stop at the start (`FinishActionUnsuccessfully`: always for
  Freeze, unless a script controls the creature for Nice, Nasty and Itchy) is `SideEffects::abandon`, which
  `SpellCreature` runs through `CreatureMindSystemInterface::AbandonAction` (the action, the walk, the agenda and the
  plan given up, and a cast let go of). Still to port: the clear at the end; openblack has no creature without an
  owner and keeps the leash's type per creature, not per player, so its gate has no counterpart yet.
- openblack calls `SetCheatDominant` only from the leash and the creature spells, and mimicry's copy uses
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
  on path, untied, whether it works kept, the untying sounds 148 / 149 in their own turn for the local player whether
  or not it went back on) or `TieTo`, whose result is not looked at, then always `PullAwayFromAction` and `ActOn`.
  `PullAwayFromAction` stands for `FinishActionUnsuccessfully`: it stops the creature, clears
  `CreatureLeash::returning`, `Control::Led` and `obeying`, and gives up the plan (`planActive`, `planner.current`,
  the idle plan). `ActOn` adds to the mind's `leash.actOn` on every call, as `ForceActOnObject` acts on every packet:
  the mind takes the last entry and clears the list, and the fight system finds a creature already fighting on a
  repeat, so the repeats change nothing.
- **0x5D** tests only that a worn leash works (`!IsLeashed || Works`): openblack keeps the flag on the worn leash, so
  with none on the leash counts as working, as every leash starts. **0x5E** checks `IsLeashed` and `Works` and does
  nothing yet. `TieTo` keeps no refusal of its own for posts, icons, highlights or seeds: the script's tie has none.
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
  unless the leash is on, untied and the thing a reward (`hand_leash::RemembersTap`; openblack has no rewards, so it
  always runs), then `OnHandTap(player, object)`, then `SendTap`. Every tap site of the press goes through it (a thing
  that cannot go into the hand, a tap-only thing, the grab released within 225 ms) and so do the knock and scroll test
  hooks. A press that takes no branch with an empty hand and no object under or near it is `ClickOnLand` 0x5D3D10:
  `OnHandTap(player, point)` at the action point ((0, 0, 0) with none, the origin). Its three results end where the
  press already ends, so they need nothing more; `NotLeash`'s own land tap (0x6DCB90) is not ported. No standard run
  double clicks the Action button or holds the leash: every check is expected identical.
- **Inside the citadel**: `TempleInterior::Activate` calls `leash_tie::OnGoInsideCitadel` before the pause
  (`LeashOff`, 0x5B), and `ApplyLeashOff` takes the leash off with `LeashSystem::TakeOff`; the temple's own turn
  dispatches no packets, as the original's. Tested in `test_leash_hand_tie`; testbed `leash.off_inside_citadel`.

### The needs sign in openblack

The rules only, `creature_leash` (`Creature/LeashNeedsSign.h`): `NeedsSignRow`, `NeedsSignQualifies`,
`NeedsSignFill`, `NeedsSignArmsAtTie`, `NeedsSignArmsThisTurn` (`k_NeedsSignWants` 0.6, the float fill compared as a
double) and `NeedsSignAction` (`k_FoodForWorshipAction` 246 over `k_FishForWorshipAction` 300). Nothing calls them:
the totem's signs and the mind's kept action are parked (Pending).

#### What openblack does with the leash natives

All nine are ported (`Creature/LeashScript`, called from `CHLApi.cpp`). `creature_leash::script` takes the leash
service and the things named, so it is tested with a fake (`test/creature/test_leash_script.cpp`); the natives in
`CHLApi.cpp` pop as the table above and push the defaults when there is no leash service. Both are dormant on Land 1
and Land 2: no leash opcode runs there with the start-up answer 3 (the `SET_DRAW_LEASH` calls are in
`CreatureDevLeashIntro` and `AttachToHouse`, which are skipped). The version 27+ mind file's leashes known are parsed
by `MindFile.cpp` as `leashFlags` and not applied (Pending).

- **A thing**: an id of 0 or an entity no longer valid is none, silently (the original's messages are empty). "A
  creature" is an entity with the `Creature` component, looked up through the const registry, so no storage is made.
  The "with a player" part is not checked: the leash service refuses a creature its player can't lead.
- **185 / 269**: the second pop is the creature and the first the thing; when only the first is a creature, they swap.
  185 ties the leash (`TieTo`, which puts the picked leash on first if none is worn); 269 compares the tie.
- **186**: a tied leash is untied back to the hand; a leash not worn is put on (`Toggle`); one already held stays
  (raffclar's tied/Toggle logic; the original always unties and turns the leash on). Both end in the same state only
  when the leash service lets the leash on: `Toggle` (and `TieTo` for 185, through `PutOn`) goes through `WhyNot`, and
  refuses, logging it and keeping it as the last refusal, a creature that is not the one its player leads or that does
  not know the leash. The script's command then does nothing here (Pending).
- **187**: `TakeOff`. **222**: `IsLeashed`. **249**: `SetWorks`, set when the popped bits are not zero.
- **275**: as the original (raffclar's "None → 0" is right only for the failure cases): the leash picked at the
  temple of the creature's player (`temple_leash::PickOf`, read through `leash_posts::ScriptLeashType`): 0 when the
  thing is missing or not a creature or its player has no temple heart, -1 with nothing picked, else 1, 2, 3. The
  posts' tap picks at once on the tapping side and packet 0x65 again at the next turn, as the original; the debug
  window can set it too.
- **305**: `script_control::CameraControl::drawLeash` = the popped value as it is; the leash draw reads it.
- **354**: the script player (a float, truncated) through `magic::ScriptPlayerToGamePlayer`, then the leash key for
  that player (`PressKey`, which also refuses a player with no creature to lead). In the port a script player out of
  0..8 does nothing: that range is `ScriptPlayerToGamePlayer`'s own; the two are still to be compared at 0 and 8
  (Pending). The original reaches its own creature checks by another path (the leader's interface status, then
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
  for every type the leash type's packet can set). openblack has no town desire flags, so the untie's mind reset is
  not made, and `StartImmersion` (force feedback) is not ported. Pinned by `test_leash_system.cpp`
  `LeashSystemTest.TheLeashKeyIsRefusedUnlessTheCreatureKnowsTheRopeLeash`.

#### The leash keys in openblack

L, V and B are read once a frame, after the temple's room keys: the first of the three that went down this frame
(down and changed, as the room keys are read) goes to `PressKey` for the local player, only when the player has a
creature they can lead. They are not read while the debug windows have the keyboard (the actions are not framed then)
nor while the world is paused in the citadel. The port's L calls `PressKey` at once instead of sending packet 0x5F,
and its V and B change the creature's leash at once with no packet (Pending).

### Test hooks

- The creature spawner's leash section has "Tie with the hand" (closed by default): the decision for the thing last
  clicked and the nearest thing, and the tie and untie packets.
- Testbed scenario `leash.off_inside_citadel`.

## Pending

- **The leash's hand end**: the original's rope runs from the collar (its +0 end, 0x848B31..0x848B7D) to the render
  hand's first drawn bone (`GetLeashStartPoint` 0x5D8EA0, CHand +0x47F0 +0x24..+0x2C), not to CHand +0x78; which of the
  original's rope readers read the +0 end was not followed. Queued as a fix ([Leash and the hand](#leash-and-the-hand)).
- **The timed dominant desire** ([The timed dominant desire](#the-timed-dominant-desire)):
  - `SuppressDesire` also empties the held-down desire's plan slot (mind +0x718 + 0x30·d, `SetToZero` 0x4F12B0): the
    per-desire slots of the original's time-sliced planner (0x4F0560, two desires a turn); queued as a fix with that
    planner;
  - the body-backed sources' setters (14 low energy, 0x4DE8F0 writes `1 − v` into the energy; 21, 22, 32, 36, 37):
    queued as a fix;
  - the other callers of the clear (the end of the Compassion, Anger and Scratch spells, `SET_CREATURE_ONLY_DESIRE_OFF`)
    are not wired; queued as a fix with the creature spells' mind;
  - the other callers of `SetCheatDominantDesire` (feedback, low life, mimicry, magic info, computer player): queued as
    fixes, one a caller;
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
  - creature +0x380 = 2 after the three packets waits on the forced actions;
  - `ForceActOnMapCoord` 0x4C6540 (packet 0x5E) and `ForceActOnObject` 0x4C5FF0's own tests (`IsLeashWorking`, the
    fight, the life, the desire and `KnowsAction`): queued as fixes;
  - `PullAwayFromAction`'s leftovers (`ModifyStrength([0xC6DAF0])` with 3D +0x528C, the sub-action's end call through
    0xBE0C18, the partner's stop and the rest of 0x47573E..0x475A7A): queued as a fix;
  - the tie's sound: 2D for the local player, at the object for another (`fn_005E6BD0`); queued as a fix;
  - the leash's "works" flag is the player's (`GLeashStatus` +0x18, untouched by `SetOn`); queued as a fix;
  - the script's `TieTo` also tells the mind to act; the original's `fn_005E6BD0` does not (only packet 0x5C's handler);
    queued as a fix.
- **The leash tied to a needs sign** ([The leash tied to a needs sign](#the-leash-tied-to-a-needs-sign)): only the rules
  are ported. Parked: the worship totem's `ShowNeeds` and its three signs with their desires (they would add three
  objects a totem; whether they take creation indices before or after the totem's own decides the state hash's
  change, to measure); the mind's kept action (mind +0x21D0..+0x21E0, run once at the free choice `fn_004F0370`, then
  cleared to {0, 0, 0, −1}); actions 246 and 300 in the creature's action set; and what `fn_004C74E0`, Creature +0x94
  and +0x384 stand for in the free-choice test, and the second argument (+0x21F0) of the kept call (unused by
  0x50A760). The fill is a float as the game's 24-bit FPU rounds the division; that its precision control is 24 bits
  here too is taken from the engine's setting, not read at this call.
- **The leash keys** ([The leash keys](#the-leash-keys)): the original's L only sends packet 0x5F (action 26,
  0x63FC0D), V and B packet 0x65 (actions 31 / 32, 0x63FE75 / 0x63FDDD); queued as fixes.
- **Leash natives, openblack's differences** ([What openblack does](#what-openblack-does-with-the-leash-natives)):
  - `ATTACH_OBJECT_LEASH_TO_HAND` / `_TO_OBJECT`: `SetOn` 0x5E6F70 refuses only a creature that does not know the rope
    leash, and then takes the leash off; openblack refuses more through `WhyNot`. Queued as a fix;
  - `TOGGLE_LEASH`'s player: 0 is the player at g_game +0x205A5B, n is n − 1, `GetPlayer` gives none from 8
    (0x6EB9A0); whether openblack's `ScriptPlayerToGamePlayer` gives the same at 0 and 8 is to be compared.
    `IsAvailable` (vt +0x2C) is not compared [?];
  - one leash state per player: `IS_LEASHED_TO_OBJECT` and `SET_LEASH_WORKS` act on the player's; queued as a fix;
  - the mind file's leashes known (`leashFlags`) are parsed and not applied;
  - the toggle's untie skips the town desire flags' reset (mind +0x20D1C = -1, +0x20D24 = 1), which needs the town
    desire flags, and `StartImmersion(7)`;
  - which creatures have no player (Creature +0x1070 null, `Creature::Initialise` 0x474397's argument) is not read.
- **Leash natives** ([Leash natives](#leash-natives)): no native body is in the decomp, everything is a reading of the
  x86 (the unnamed functions and the `GLeashStatus` fields are read in [The leash keys](#the-leash-keys) and
  [Common facts](#common-facts)). Left: no shipped CHL `Enum.h`; the PPC Mac bodies are not disassembled.
