# The creature's mind

## Summary

The creature's mind is what makes it choose what to do: 40 desires that grow from their sources and fade, two
decision trees per desire (what to act on, what to use it with), learning from the player's strokes and slaps,
learning skills and miracles by watching them, the things it looks at and the things that frighten it. The mind is
kept in a mind file (`.erc`), which a profile's creature is loaded from and saved to.

openblack reads and writes the mind file byte for byte, and has the desires, the decision trees, the learning, the
watching, the look and the fear test as pure functions with tests. The planner that turns desires into plans is on
[Plans and needs](plans-and-needs.md). Not yet ported: the sightings reported by the game (only the debug spawner
reports one), the learning meter, the messages, and the original's candidates for the look and for fear, which come
from the creature's beliefs about the scanned map cells rather than from the whole world.

## The original

### The mind and its file

#### The mind file's format

- The names in a mind file are enciphered with MSVC `rand`'s LCG (214013 / 2531011, `>> 16 & 0x7FFF`) seeded with
  0x913 for each name.
- The two `.erc` files in `Scripts\CreatureMind`, `C4ba71b36.erc` (a Mandrill, row 14) and `Cacab45d4.erc` (a Tiger,
  row 2), are version 33. They are not templates: they are two profiles' creatures, the files `LOAD_MY_CREATURE` reads
  ([The player's creature](players-creature.md#the-players-creature)).
- `Profiles\<name>\creature.lhp` is not a mind file (both profiles' files are refused as an unsupported version): it is
  an export the game writes and never reads back.

#### The body's fields

Checked in the game's writer 0x4E8E70 and reader 0x4E8FF0:

- physical +0x70 is unnamed (it reads like a turn number);
- then +0x08 the age, +0x0C the strength, +0x14 the fatness, +0x18 the previous fatness, +0x1C the energy, +0x20
  (from version 6), the u16 flags, and six needs from +0x2C (the amount of poo, the exhaustion, then four more);
- from version 22, an int from +0x50 and the size.
- The names Age, Strength, Fatness, PreviousFatness, Energy, AmountOfPoo and Exhaustion are the format strings of
  `Creature::OutputCreatureInformation` (0x4785A0 is inside it).
- The fatness and previous fatness were first named by bw_creature_editor.py, a creature-editor tool shared on the
  openblack Discord (credited in `MindFile.h`), which walks the same sections as openblack's reader up to the body.

Where that tool differs from the original, which openblack follows:

- a belief's two numbers are the object's x/z position (`UpdateAttributes` 0x4D7E80), not object ids;
- the 42-entry table holds (count, last turn) per spell (`ConsiderLearningAction` 0x4E2443, 0x4E2499);
- the physique file's first number is the species row, not a version (it equals the mind file's species in every
  copy checked).

#### Saving a mind

- `SaveMind` 0x4E7820 and its writers reseed nothing: none of `srand`'s four callers (0x577727, 0x87AF37, 0x8861F7,
  0x88BB89 [V]) lies in them.
- The writer `fn_004E79D0` takes the spells' before-values (`fn_004F5E60` / `F00` / `FA0` on +0x370): the size, the
  strength and the alignment the creature had before any spell of that kind began (before big, else before small;
  before strong, else before weak; before nice, else before nasty).
- For the local player's creature while the land's `CreatureCurse` script runs (0x6EB760) it takes instead the script's
  globals `OriginalSizeOfMyCreature` × the float 0x3D888889 ([0x8C9D38], 0.0666666701; in `CreaturePhysical::SaveBody`
  0x4E8E70 at 0x4E8F00, which `SaveMind` calls at 0x4E7E94) [V], `OriginalStrengthOfMyCreature` and
  `OriginalAlignmentOfMyCreature` (0x6EB790). The four names sit in the exe near the "Save Mind" and `CreatureMentalSaveAndLoad.cpp` strings
  (0x7DFF00..0x7E0010), and `Scripts\Quests\challenge.chl` holds them as script names. The exe has no
  `OriginalFatnessOfMyCreature`, though the script keeps one.
- The game also keeps the cursed creature's values natively, as `ScriptCreatureCurse {height, strength, alignment}` in
  `GGame::script_creature_curse` (bw1-decomp `Game.h:224`). `GGame::Save` calls `ScriptCreatureCurse::Init` on the
  local player's creature (0x6F6190; `GGame::Save` 0x5541FC is its only caller) and writes the record last
  (`GameOSFile::WriteIt`, 0x556F80); `GGame::Load` reads it back (`ReadIt`, 0x557070) and the load's resolve hands it
  to `ScriptCreatureCurse::ResolveLoad` with that creature (0x6F61E0), called from `GameOSFile::ResolveAllLoads` (0x561D50) [V]. `Init` keeps the
  creature's height (`GetHeight`, vt+0x42C = 0x477F50), strength (physical +0xC) and alignment (+8), or 0, 0, 0 without
  a creature. `ResolveLoad`, while the CreatureCurse script runs (`GScript::IsScriptRunning` 0x6F61FC) and the kept
  height is above 0, puts them back (`Creature::SetHeight` 0x47DE80, `CreaturePhysical::SetStrength` at 0x6F6231,
  `GAlignment::CrudeSet` at 0x6F6240) after the mind file is read: the record does not go through the mind file.

#### The starting lesson

📼 1.20 only `Creature::Initialise` 0x4744B0 calls `fn_004CB3C0` (its only caller; the source file named in it is
CreatureInitialLesson.cpp):

- It switches on the creature info's +0x1F4 into five empty cases (jump table 0x4CB3F4), so no species has a lesson of
  its own.
- It builds one villager belief (`CreatureBeliefAboutVillager` ctor 0x4DB110) with allegiance 1 (`ALLEGIANCE_ENEMY`),
  origin 0, animate 0, player 0, harder-than-me 0, creature type 0, type 6 (a villager), sex 0, job 0, life 1 and on
  fire 1 (not on fire), a context of the lesson kind 2 for desire 4, Hunger (`fn_004E1CF0`, 0x4CB660..0x4CB668), and an
  episode with the feedback [0xBDEA94] = 0.8 (0x4CB688; the 10th entry of the feedback scale at 0xBDEA70).
- Only when the hunger act-on tree (Creature +0x164, the mind, → +0x2528) has no example yet (+0x10 = 0, 0x4CB6B4) is
  it added (`fn_004CE200`) and the tree grown again (`fn_004D4340`).
- One example makes the tree a single leaf of the step 0.6, the first within a quarter of 0.8, so a new creature finds
  every thing it may eat worth 0.6 (usefulness 0.125 against 0.1 for nothing known) until other examples split the
  tree.

#### Usefulness from a tree

A thing's usefulness to the planner comes from its tree's utility
(`DecisionTreeCollection::ConvertUtilityToUsefulness` 0x4D7670) [V]: below 0, or a value that compares with nothing,
gives 0, and it is capped at 0.125, so everything learnt to be worth more than about 0.028 is as useful as anything
else. The mind's sigmoids are `Creature::SigmoidThreshold` and `GUtils::SigmoidThreshold` over the game's 41 exact
floats ([GUtils distances](../engine/coordinates-and-angles.md#gutils-distances)).

#### The innate personality

The `CreatureMental` constructor 0x4D22F0 builds, after the desires and their sources, the innate personality
(`fn_004E2E70`, this = mind +0x1AA14, from the creature info): nine fields, kindness +0x1AA14 (info +0x350),
aggression +0x1AA18, lethargy +0x1AA1C, friendliness +0x1AA20, communicativeness +0x1AA24, an unnamed +0x1AA28 and
three unnamed +0x1AA2C..+0x1AA34. Each is the info's value jittered and clamped to [0, 1] when [0xD00DEC] is set (it
always is: [A creature with no plan](plans-and-needs.md#a-creature-with-no-plan)), with the spread 0.1 ([0x8AB22C]) for
the first five, 0.3 ([0x8AB23C]) for the sixth and 0.4 ([0x8C7A44]) for the last three, by a macro that draws again at
every test. Then `fn_004F1FF0` (the `CreaturePreviousActions` constructor, mind +0x1CE98) gives each of the 328
actions a `GameRand(120)`. How many draws each part makes and where they fall:
[Where the creation draws fall](../engine/random.md#where-the-creation-draws-fall).

### Learning by watching

`ConsiderLearningAction` and the magic scroll, read in the disassembly (bw1-decomp has no bodies).

#### Considering a sighting

**`CreatureMental::ConsiderLearningAction(type, index)`** 0x4E2380, type 0 a skill, 1 a miracle. The mind's fields:
+0x20CE4 mind active, +0x1A9FC `CreatureActionsKnownAbout` (two lists, skills and miracles), +0x17D38 + 4·i a
miracle's sightings, +0x17DE0 + 4·i its last turn, +0x20D18 the creature. The turn is the game's (`g_game`+0x205A40).

1. With the mind not active it returns 0 at once (0x4E2388), for both types. The flag is cleared when the freeze spell
   starts (0x4F5449, after `FinishActionUnsuccessfully("frozen by spell")`) and set again at the thaw (0x4F54F6).
2. **The prerequisite**: a static table at 0xBDFAE0 (initialised data, nothing writes it), 8-byte entries
   `(type, index)` at `[type·42 + index]`, type 2 for none. When there is one and `KnowsAction(type, index)` 0x4E2890
   is false it returns 0: no message, nothing written. The miracle rows: 2 and 3 need miracle 1; 5 and 6 need 4; 8
   needs 7; 9 needs 8; 11 needs 10; 17 needs 16; 18 needs 17; 40 and 41 need skill 0 (building). Every other row is
   none. (The skill rows 0..17 are none; 18..41 read (0, 0), but there are only six skills.)
3. **A miracle**:
   - with the creature's phase (+0x1268) below the row's (0xCA9CD0 + 0x70·i): message 0xB, return 0, nothing counted;
   - when `now − last > 50` (unsigned, `ja`) or `last == 0`, the count goes up by 1, and by 2 more with the leash on
     and of type 2 (the rope; leash +0x12C → +0x1C): 1 or 3. A last turn of 0 is the "never seen" mark;
   - `last = now` always, also when the sighting did not count;
   - `AddActionLearnt(creature, 1, i, notify 0)` 0x4E28C0: the miracle is known about from this sighting on;
   - `needed = NumTimesMagicActionMustBeSeenBeforeItIsLearnt(i)` 0x4F8CA0, `fild(timesToSee)` times
     `fmul dword[CreatureInfo + 0x37C]`, 0 for i ≥ 42, not rounded. The game thread runs the x87 at 24 bits
     ([The game's FPU](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)), so it is the float product:
     15 × 1.2 is 18 exactly;
   - `needed <= count`: message 7 and result 1, and the meter (`fn_0047CDA0(1.0)`) while `count − needed < 3`
     (the 3.0 at 0x8C2C50); return 1;
   - otherwise `share = count / needed` (float); `share >= 0.75` (0x8AB274) gives message 9; the meter
     `fn_0047CDA0(share)`; return 0.
4. **A skill**: phase against 0xCAAF40 (too young: message 0xA); the first-seen turn (0 again the mark) at +0x17D20,
   the count at +0x17D08; the whole seconds since the first sighting, `(turn − first) div (1000 div [0xD01A38])`, at or
   above the row's watching time (0xCAAF38) give the meter at 1.0 (only if not yet known), then
   `AddActionLearnt(…, notify 1)`; otherwise, not known, message 8 and the meter at 0.5.

`fn_004C9FE0(code, index, 0, 0, 0)` shows a learning message, only for the local player's creature (🔍 inferred from
its player check at 0x4CA036; codes 6 and 7 learnt, 8 and 9 nearly, 0xA and 0xB too young).

#### The meter

📼 1.20 only `fn_0047CDA0(share)` does nothing unless the game turn − Creature +0x1124 > 100 (`cmp ecx, 0x64; jbe`); then the
turn is kept, and a 0x98-byte floating text of share × 100 in the wide format "%.0f%%" (0x9D0668) is made
(`fn_00827A50`, list [0xEB9A74], white, 1.0) at the head (`fn_004813B0`, y + 1.0), sized `GetHeight` × 0.1.
`GLandscape::Draw` steps and draws the list (0x5E629B → `fn_00827B90`): each text lives 6000 ms, rises 0.003 × size a
ms, grows by 1 + t/6000 and fades near its end. No debug gate is on this path.

#### Who reports a sighting

The six calls ([V] byte scan, each 📼 1.20 only):

- `CreatureBeliefAboutVillager::UpdateAttributes` 0x4D8158 (phase ≥ 3, not Flags +0x25 & 4, a switch on mind +0xF50,
  then the skill of the villager's state from the state table 0xDB9F58 + 0x114·state, state 0x17 taking +0x8D and 0x21
  beyond [0x8CF300] becoming 0xA3; skipped for 6 and for a known skill). So the reach is the creature's beliefs about
  villagers, the objects it looked at, not a fixed radius.
- 0x4EA6ED `MimicPlayer` (labelled `DecideOnNewPlan`; magic).
- 0x4F2945 through 0x4F2930, from `SetupReactToHandPuttingStuffInStoragePit` (skill 3), `SetupReactToNiceMagic` /
  `NastyMagic` (magic) and `SetupReactToHandUsingTotem` (skill 2).
- 0x4F2963 in 0x4F2950, which also keeps an action at mind +0x21D0 and has no static caller [V].
- 0x50613D (skill 0, after an item is added to Creature +0x120C).
- 0x50A4EC (magic).

#### No miracle known at the start

The `CreatureMental` constructor 0x4D23D6 builds `CreatureActionsKnownAbout` through fn_004E21F0 with both lists
empty. Field +0x54 of `CreatureMagicActionKnownAboutEntry` (1 in 24 rows of `Scripts/info.dat`: 0, 1, 4, 10, 13..30,
34, 35) is read only by the computer players' creature code: every reference to 0xCA9CD4 in the code is in fn 0x4E1F70
(counts the flagged miracles not known about), called only from fn_0065BCB0 and from the thunks 0x47C940 / 0x47C950
(no caller, no pointer), and in the helpers 0x4E29E0..0x4E2C80 ("the first flagged miracle not known about"), called
only from 0x659192..0x65A994 in the `PlayerComputerCreature` area.

#### The magic scroll

`CreatureRoom::MakeMagicScrollText` 0x789070: for i = 1..41 (never 0), only when `KnowsAction(1, i)` and the row's
text id (+0 of the 0x11C magic row) is not 0, the percent is `__ftol(min(1, count / needed) · 100)`, in float steps,
cut short. A miracle known about at 0 % is listed.

### What a creature looks at

#### The scan

`Creature::Look` 0x4D0700 → fn_004D07A0 scans up to 9 map cells a turn in a spiral (`CanSeePos` 0x477440), and
fn_004D0A20 hands every object in each cell (it skips those with flag `+0xA & 1`, UNAVAILABLE) to
`AddBeliefAboutObject` 0x4D7BD0 and to fn_004D1D70 (`CreatureInterestingThingToLookAt::ConsiderNewObjectToLookAt`).
That skips the creature itself, tests `CanSeePos` and fn_004774F0(pos, 6), and weighs the object by the virtual
`GetHowMuchCreatureWantsToLookAtMe` (vt+0x3E8) × the distance factor (1 within half the range, else 1 − d/range,
range = √(fn_004EFC70) × 10) × the boredom term (1 − min(watched, 20)/20) × the per-kind weight at mind+0x1AA2C.

#### How much each class is looked at

vt+0x3E8 by class (bodies 0x4D1AF0..0x4D1B80, read through every vtable): the Dove class 0.9 (the vtables of Dove,
Crow, Swallow, Pigeon, Seagull, Bat, SpellDove, SpellBat and Vulture), Citadel 0.9, Creature 0.85, Animal 0.7 (the
vtables of Lion, Tiger, Leopard, Wolf, SpellWolf, Cow, Sheep, Horse, Pig, Tortoise, Goat, Zebra and the Piece/Puzzle
ones), Villager 0.5, Abode 0.4, Tree 0.3, Fixed 0.25, MobileStatic 0.9 for a toy, else 0.4.

#### Which animal is which class

- The animal factory fn_00419E00 jumps on info+0x1F4 (27 cases, table 0x419FD0, and above 26 it returns no object,
  0x419E0C); each case's create function sets its class's vtable, and the cases line up with `AnimalInfo` 0..26.
  Traced through the factory: 0-4, 6 and 8-10 the Animal class, the flying ones (11-16, 20, 21) the Dove class, 22-26
  (SpellWolf and the Piece ones) Animal. Cases 5 (Goat), 7 (Zebra) and 17-19 (Vulture, CitadelDove, CitadelBat)
  create nothing. So every bird the factory makes is looked at as much as a dove, not only the miracle's doves.
- [L/V] `GameOSFile::LoadInstance` 0x55AB4E..0x55B05D (the constructor calls of the save loader,
  `GameOSFile::LoadInstance` (0x55ACAA, 0x55AD10, 0x55B05D), pick the class by the saved type) builds Goat (vt
  0x8AF4E0), Zebra (0x8B1964), Vulture (`Vulture` ctor 0x55EAD0, vt 0x8BB7F8), Bat (0x55EA30), Dove, Crow, SpellDove
  and SpellBat by the saved class; vt+0x3E8 is `Animal` 0x4D1B20 (0.7, [0x8AB238]) for Goat and Zebra and `Dove`
  0x4D1B10 (0.9, [0x8C5844]) for Vulture, Bat, Crow, SpellDove and SpellBat. The `AnimalInfo` comes from the save's
  data, so the class goes with the object; nothing ships such a save.
- The Puzzle classes (PuzzleHorse, PuzzleCow, PuzzleTortoise, PuzzlePig; Animal at vt+0x3E8) are made only by the
  chess puzzle (`PuzzlePig::AddToBoxPositionForChessGame`, 0x6DD1BB..0x6DD384) with the Horse, Cow, Tortoise and Pig
  entries of the animal info table (0xC4E95C, 0xC4E690, 0xC4E0F8, 0xC4EC28: entries 9, 8, 6, 10 of the 0x2CC-byte
  table at 0xC4D030). So no object of the original is known to carry 27-30; whatever it carries, a Puzzle animal is
  of the Animal class (0.7).
- CitadelDove (18) and CitadelBat (19) have no create function and no vtable named for them, so their class is not
  known.

#### The look point

📼 1.20 only `fn_004D1BB0` gives the thing's MapCoords x, z (+0x14 / +0x18) and `GetHeight` (vt+0x42C, 0x4D1BD6); its consumer
`fn_004D1870` (mind +0x1AA40 when set, else Creature +0x1214) ← `fn_004D03B0` 0x4D05F1 (from `ProcessState` 0x473593)
makes y = `LH3DIsland::GetAltitude`(MapCoords) 0x4D0602 + the height, x and z the fixed point / 6553.6 ([0x8AA3A4]),
for `fn_00482C90`. vt+0x42C: Villager, Abode, Tree, Animal, CitadelPart, CitadelHeart, Feature, Fixed and MobileStatic
take `Object::GetHeight` 0x638120 (mesh +0x28 × scale +0x50 × 2, 0 without a mesh), the Creature 0x477F50 (3D +0x90 ×
15), the Citadel `GameThingWithPos::GetHeight` 0x405500 (0); the slots read from the vtables.

#### The head's turn to its target

📼 1.20 only `LH3DCreature::UpdateLook` 0x482D90 normalises the direction from the eyes to the target and gives a sideways turn
only when both its x and its z are more than 1e-4 off 0 (`fabs` + `fcomp` against the double 1e-4 at 0x8C79D8,
0x482F54 and 0x482F67, each skipping the yaw on "not greater"). So a target straight north, south, east or west of the
eyes gives no sideways turn, only up and down.

### What frightens a creature, and the fire it believes

What a creature does about a fire (the ReactToFire row, the town-fire initiative) is on
[Fire: the creature and a fire](../world/fire.md#the-creature-and-a-fire); this section is the belief and the fear.

#### What can frighten

**`CanBeFrighteningToCreature`** is vt+0x238 (read through every class vtable). It answers 1 for `Creature` 0x4740B0,
`Spell` 0x55CED0 (and every Spell subclass), `Bat` 0x41EF20, `SpellBat` 0x41F000, `Vulture` 0x41F0D0, and `Lion`
0x41FC70, which Tiger, Leopard, Wolf and SpellWolf share (the same vtable entry). Every other class answers 0,
PieceLion/PuzzleLion and PieceWolf/PuzzleWolf (0x4200E0, 0x421D90), SpellIcon and Fixed included. The animals' part is
`AnimalSystem::IsFrighteningToCreature` ([API for other systems](../beings/animal-life.md#api-for-other-systems)).

#### The desires' object tests

- It is the Fear desire's object test. The per-desire table at 0xC87850 has one 0x28-byte entry per desire, filled at
  0x4DB5F0. Entry 5 (`CREATURE_DESIRE_FEAR`) +0x14 is 0x4DD0E0, a `jmp [eax+0x238]` thunk. This table is not
  `g_CreatureDesireDependency`, which is at 0xC85CD0, 40 rows of 0xB0 bytes, a 0x10 header and 40 floats each: how much
  one desire goes with or against each other, read by `SuppressDesiresAfterDecidingOnActivity`.
- `CreatureMental::IsObjectSuitableForDesire` 0x4E3730 calls entry[d]+0x14 on the object (0x4E37C5), after
  `IsSuitableForCreatureAction` vt+0x21C. The other entries: Impress → vt+0x244, Compassion → 0x4C33D0 (vt+0x23C
  `CanBeHelpedByCreature`; the Villager override is vt+0x628), Anger → vt+0x234 `CanBeAttackedByCreature`, Play →
  vt+0x240.
- 📼 1.20 only Its gates, in order: it refuses when `fn_004D8E10(belief)` holds (belief +0x34 set and the turn − +0x38 within
  60 s of turns for an object with +0x24 & 2, else 10 s; past that it clears +0x34); when `fn_0047CD20(belief+0x24)`
  holds (the kept radius +0x11B4 above 0, with a leash status the leash working, and the object farther than the radius
  from the kept point +0x11A8); for an object with +0xA & 2; for Hunger (4) with an object whose belief's
  `fn_004E9900(…, 1)` is above 0.1; when vt+0x21C is false; then the desire's member function from the table 0xC87864 +
  0x28·d (written once by 0x4DB5F0): 0 Impress vt+0x244, 1 Compassion +0x23C, 2 Anger +0x234, 3 Play +0x240, 5 Fear
  +0x238, 6 Curiosity +0x248, 7 Poo +0x24C, 8 Tiredness and 23 Rest +0x254, 13 BringStuffHome +0x258, 16 BeFriends,
  31, 32 and 37 +0x250, 34 GetHigh +0x2A4; no other desire has one.
- Its eight callers (0x4C53FE, 0x4C5466 `FindSomethingToDoOnObjectUsingObject`; 0x4DAD43..0x4DAFAD
  `FindBestObjectToActOn` of Town, Flock, Forest and Citadel; 0x4F0FCA, 0x4F10EC on the mind's lists +0x2218) all take
  a belief [V], so a spell must have one to be offered.

#### Fear in the data

In `Scripts\info.dat` (the desire → action rows after the 328 actions), Fear (5) lists `RunAwayFromObject` (16),
`BeFrightenedOnTheSpot` (174) and `RunHome` (170), and no other desire lists 16. So, for this data, "the object of
RunAwayFromObject must frighten" is the original's per-desire rule exactly.

#### The Fear desire's numbers

Row 5 of `CreatureInitialDesireInfo` (`CREATURE_DESIRE_FEAR`, openblack's `Desire::Fear`; the name in the row is "Fear"), the same in
`Scripts\info.dat` (row at file offset 0x65414) and in `CreatureIsle\Scripts\info.dat` (0x6CFE0):

- sources 17, 18, 19 (`FEAR_FROM_DARKNESS`, `FEAR_FROM_BEING_DAMAGED`, `FEAR_FROM_SEEING_SCAREY_MAGIC`), then 61
  (none) five times; learnable 1;
- cap +0x4C (openblack's `field0x3c`) = 0x3F6B851F = 0.92;
- decay range +0x50 / +0x54 (`field0x40` / `field0x44`) = 0x3F7FBE77 = 0.999 / 0x3F800000 = 1.0; the range is
  1.0 − 0.999 = 0x3A831200 (0.00099998713);
- weight +0x78 (`field0x68`) = 1.0.

So a creature's Fear decay is drawn once, when the creature is made, as `0.999 + GameFloatRand(0.00099998713)` on the
synchronised stream (the 6th of the 40 draws of `Initialise`, see
[The desires at creation](../engine/random.md#the-desires-at-creation)): from 0.999 up to 1.0, the largest draws
rounding to 1.0 exactly (no fading at all). A mind file keeps it; feedback moves it in steps of 0.001 × feedback within
[0.999, 1.0]. Each turn without a positive increment, Fear is multiplied by it and held in [0, 0.92].

#### The OnFire belief

`ATTRIBUTE_TYPE_ABODE_ON_FIRE` = 22. `AttributeOnFire` is built only in
`CreatureBeliefAboutVillager::CreateAttributes` (0x4D9C3E) and `CreatureBeliefAboutAbode::CreateAttributes`
(0x4D9D05). Its `UpdateValue` 0x4D4020 asks `IsOnFire` vt+0x298 (Object → 0x637CC0: a FireEffect at +0x44 with T ≥ Tc,
[The heat model](../world/fire.md#the-heat-model)), and **0 is on fire, 1 is not**: its `ValueNames` at 0xBDE9D4 are
"IsOnFire", "IsNotOnFire", and its `ValueRange` is 2.

In the decision-tree attribute array (`CreateAttributeArray` 0x4D52B0), slot 22 "OnFire?" is built with the
`AttributeCreatureDominantDesire` vtable 0x8CFB3C. It looks like the original's own slip, and matters only if
openblack's tree learning ever builds attributes from that array.

### The creature action table

`g_CreatureActionInfos` (`CreatureActionInfo`, 328 rows) sits at 0xC6C490, rows of 0x110 bytes; each row's info.dat
data starts 0x10 into it (0xC6C4A0, after the `GBaseInfo` part), so a memory offset is the file offset + 0x10. Read
from the exe's readers [V]:

| file | memory | bw1-decomp | read by | what it is |
|---|---|---|---|---|
| 0x98 | 0xA8 | `PlanIndex` | `UpdateDesiresAfterPerformingAction` 0x4DC521 (`IsActivated`, the desire's value), `UpdateActionPriority` 0x4C3A03, `GetActionPriorityModificationFromWhenLastDone` 0x4F0CEB, fn_00474E45, fn_004F8850 | the action's desire, as the planner and the learning use it (openblack `desire`) |
| 0xB4 | 0xC4 | `Desire` | `GScript::DesireIs` 0x6FA31D, `AttributeCreatureDominantDesire::UpdateValue` 0x4D3C21, both on the current action's row | the desire the action counts as for scripts and for the dominant-desire attribute |
| 0xD0 | 0xE0 | `LearnWindow` | `CalculateLearningPriority` 0x4E0669 | the learning window (openblack `learningWindowSeconds`) |
| 0xE0 | 0xF0 | (unnamed, `field_0xf0`) | the feedback handler 0x4E07B1 only | a stroke while it holds something teaches it to eat (openblack `eatWhenStroked`); set on 153 rows |
| 0xF0 | 0x100 | `Learnable` | `CalculateLearningPriority` 0x4E0611 | the action can be learnt from (openblack `learnable`) |

Readers through a row pointer, rather than the table's address, are not ruled out [?]. Two of bw1-decomp's names do
not fit the exe: memory 0xA8, which it calls `PlanIndex`, is read as a `CREATURE_DESIRES` by `IsActivated` and indexes
the desire values in `UpdateDesiresAfterPerformingAction`, so it is the action's desire; and memory 0xF0, which it
leaves unnamed (`field_0xf0`), has the feedback handler's eating check as its one reader, so it is the stroke's eating
flag.

### The player's feedback

The handler of a stroke or slap, the unnamed 0x4E06A0 (from the packets at 0x63D36B / 0x63D6B2 and the hand demo at
0x6540AF / 0x65419F) [V], in order:

- `FinishActionUnsuccessfully("player feedback", 1, 1)` first, for every feedback: the action, the walk, the agenda
  and the current plan end (its desire +0xF50 = 0x28); no learning context is touched. Then, only with Creature
  +0x1064 set (1 from `Creature::Initialise` 0x4743F4, never cleared):
- |f| <= 0.01: a forced plan {IdleAroundWithPlayer 9, LookAtCamera 82, itself}, every score 1.0
  (`ForceActivityAndForceAction`), and `ForceToolTips(0xEA2, 1.0)` (text 3746, HELP_TEXT_TOOLTIP_48 "Bring creature").
  LookAtCamera's one sub-action 0x42 turns the body to the camera when it is more than a right angle off and holds up
  to 5 s.
- The newest context (Nth(0) of mind +0x2AD8) decides two things: a stroke while the creature holds something, in an
  action whose row allows it, and from the first stage of growing up, makes Hunger fully dominant and forces the eating
  of what it holds; a stroke on RunAwayFromPlayer makes that desire least dominant, empties source 0x2B and raises its
  threshold by 0.2, and the current priority goes to 0.
- f is pushed out to at least ±info +0x2A4; a slap clears mind +0x1C38 (mimicking) and the state at +0x21D0.
- The best context (`GetBestContextToLearnFrom` 0x4E0410): from the newest to the oldest, a strictly higher
  `CalculateLearningPriority` 0x4E0600, which is 0 for an action whose row's flag is off, 1.0 for a context not
  finished (+0x48), 0 for a finished one the player did not see finish (+0x3C), else 1 − age / T (T the row's window).
  Priority 0: nothing more. A stroke then counts for the mimicry and holds back Sadness for 90 s. From the second stage
  the lessons (0x4E0FC0); before it, sources 9 and 0x12 for a slap, 0xC, 0x26 and 2 for a stroke, then the help
  request `SendCandidateHelpScript` fn_004C9FE0 (MISCELLANEOUS_STACKED, 5 SLAP_BUT_NOT_LEARNING or 4
  STROKE_BUT_NOT_LEARNING) and `GameRand(2)`: on 0 the mind keeps a next action (+0x21D0): for a slap Fear forced with
  `GameRand(3)` (1/3 RunAwayFromPlayer, 2/3 BeFrightenedOnTheSpot), for a stroke ManifestState CommunicateState.
- A slap (0x4E0BA4, every path) gives up the newest context's plan when `IsValid` 0x4F12E0 (desire < 40; its goal
  belief when the desire looks for one; the act-on belief; the action; the instrument when the action's row +0x44 asks
  for one): +0x1C28 = 0, FinishActionUnsuccessfully, `MakeDesireLeastDominant`(its desire, 1.3, the species' floor),
  every desire let go of, the current priority 0.
- Then creature +0x10E8 = 0 and the stroke repeat (0x4E0C5C): a stroke, the best context done to a thing other than
  itself, not mimicking: plan(its desire, its action, row 276 done as 11, scores 1.0), the thing of that kind most
  useful to act on (fn_004F10A0, strictly above 0, the same one allowed; itself with 0.1 for an action done to
  nothing), `CanCreatureCurrentlyPerformAction`, the associated object (its town, citadel or forest; the nearest town
  for a goal-searching desire), the instrument, `IsValid`, forced (+0x1C14 = 1).
- The contexts: only the success path fn_00475450 finishes one (+0x48 = 1, +0x4C = the turn, +0x3C = 1 when
  `GetInterfaceStatusLookingAtCreature` 0x4775D0 finds a camera of the creature's player within 400 m and inside
  dot > cos(2/3 x 70 degrees)); a step's end in `ProcessSubAction` 0x4FF128 and the help callback 0x4CAA80 also set
  +0x3C. `PreviousContextStack::Push` 0x4F23D0 overwrites the newest in place while it is not finished.
- The action rows' instrument flag (+0x44 of the code-built table 0x9D1678, stride 0x50): set for 37 rows (14 15 24
  28 30 31 32 33 35 36 68 69 73 83 135 171 232 233 234 235 241 245 247 249 252 262 280 281 283 285 291 303 306 307 308
  326 327).

### Sources

- The v1.42 `runblack.exe` through the xref index and the disassembly; bw1-decomp for names (it has no bodies for the
  mind's functions).
- `Scripts\info.dat` and `CreatureIsle\Scripts\info.dat` (the frozen copy), `Scripts\CreatureMind\*.erc`.
- bw_creature_editor.py, the creature-editor tool shared on the openblack Discord.

## openblack

Status: the mind file, the desires, the decision trees, learning by feedback and by watching, the look and the fear
test are ported as pure functions; the game does not yet report sightings, and the look and fear candidates come from
the world, not the creature's beliefs. How raffclar's creature mind code was taken or replaced, row by row:
[Port decisions: creature](../project/port-decisions-creature.md).

### The mind file and the mind's parts

- **The mind file** (`components/creaturemind`, `MindFile`): the format, read and written from bytes; both `.erc`
  files read and write back byte for byte. The land change's save (`creature_mind_body::ToMindFile`) writes the fatness
  at that field on its strength.
- **The mind** (`CreatureDesires`, `CreatureDecisionTree`, `CreatureLearning`, `CreatureWatching`, `CreaturePlanner`,
  `CreatureMindModel`, `CreatureMindTables`, `CreatureMindFileBody`): the desires, the two trees per desire, learning
  from strokes and slaps credited to recent actions, learning by watching, and the planner. The usefulness is
  `creature_tree::UsefulnessFromUtility` (`k_MostUsefulness`); `Sigmoid` is `gutils::CreatureSigmoidThreshold` and
  `SigmoidStep` `gutils::SigmoidThreshold`.
- **Idle mind and actions** (`CreatureIdleMind`, `CreaturePlanActions`, `CreatureFeedback`, `CreatureReach`,
  `CreatureObjectActions`): the agenda a creature works through once a turn; which of the table's actions it can
  carry out; strokes and slaps; reaching by blending four animations; picking up, eating, tossing and throwing things.
- **The starting lesson** is `creature_mind_model::StartingLesson` and `TeachStartingLesson`, from
  `CreatureMindSystem::SetUpLearning` and, for what a mind file does not hold, `TakeUpFile`.
- **Saving a mind as the creature itself**: `creature_spells::ValuesToSave`, `mind_detail::CurseValuesToSave` and
  `CreatureMindSystem::SaveMind` follow the before-values and the curse globals, but openblack differs: the cursed size
  is divided by 15, the original multiplies by 0x3D888889
  (`src/ECS/Systems/Implementations/CreatureMindLearning.cpp:328`), a fix row. Nothing calls `SaveMind`, so it is
  dormant. The `ScriptCreatureCurse` record and its restore at load are not ported (a port row).
- The action table's fields keep openblack's names (`desire`, `learningWindowSeconds`, `eatWhenStroked`,
  `learnable`).

### Learning by watching in openblack

`src/Creature/CreatureWatching`, `CreatureMindSystem::SeeSkill` / `SeeMiracle`, the cave's snapshot and scroll facts:

- The miracle known about is `Knowledge::miraclesKnown`; the prerequisite table is
  `creature_watching::MiraclePrerequisite`; `TimesNeeded` is the float product, rounded once (exact for any count below
  2^29).
- The messages and the meter are returned as data (`Progress::event`, `Progress::meter`) that nothing shows yet;
  `SeeSkill` returns no event and no meter.
- A paused mind (the freeze spell sets it) skips both kinds of sighting.
- The scroll lists every miracle from 1 known about, with the percent cut short. The text-id test of the scroll is not
  ported: the scroll names each miracle by its place.
- The "I've learnt" thought of the mind's log is openblack's: it comes once, at the sighting that reaches the times
  needed.
- Nothing in the game reports a sighting yet: only the debug spawner does, so the game's runs are unchanged. Of the six
  original callers, only the two miracle reactions are planned.
- No miracle is known at the start, as in the original.

### What a creature looks at in openblack

- `creature_look` (`LookRange`, `DistanceFactor`, `Priority`, `LookAbout`) and `creature_look::InterestOf` hold the
  scan's weights and the classes' values.
- `mind_detail::LookInterestOf` (`LookInterestOf`, taking the class with the object) gives Dove for `animal_ai::IsDoveClass` (`IsFlyingSpecies` and the Vulture, the same
  test the script types and the heal rule use), Animal for the rest; `AnimalInfo` 5, 7 and 17 get the class of the same
  name, 27-30 the Animal class (0.7), CitadelDove and CitadelBat Animal (0.7) as a placeholder.
- The candidates (`mind_detail::GatherCandidates`, through the const registry): creatures, villagers, abodes, trees,
  temples and every animal. An animal is looked at at its `Transform` position. Every entity of these kinds is offered,
  with no test of where it is: one in the god's hand or the creature's, or one dying or dead, included. The original
  offers only what is linked into the scanned cells and skips the UNAVAILABLE ones.
- The look point: openblack's heights (villager 1.8, abode 4, tree 6, temple 30, a creature 15 × size, an animal 0)
  are its own; `ecs::object::GetHeight` already ports the original's object height and the creature's override.
- The head's turn: `creature_layers::AnglesTowards` gives no sideways turn when either part is within 1e-4 of 0, as
  the original (it used to test the flat length).

### Fear and the fire belief in openblack

- `Target::Frightening` (`creature_plan_actions`) is RunAwayFromObject's target; `mind_detail::Accepts` takes another
  creature, a `Spell` or an animal `IsFrighteningToCreature` says yes to. `mind_detail::Gather` offers it no spell: how
  a spell reaches the Fear test in the original is not known, so only creatures and animals are its candidates. The
  test `CreatureFrightening.OnlyFearListsRunningAwayFromAnObject` pins the data side; it skips without the game's data.
  Before this, openblack ran from any villager, animal or creature.
- The Fear decay follows the original for a fresh mind: `creature_creation::SetupFor` reads the cap and the range,
  `creature_creation::Make` draws the decay as the creature is made (`CreatureArchetype::Create`), as
  `GameFloatRand(b − a) + a`, stored by `creature_desires::Create`, applied and clamped to [0, cap] in `UpdateDesires`
  (`CreatureDesires.cpp:194-195`), moved by feedback as the original (`CreatureLearning.cpp:60-61`, `k_DecayStep`
  0.001). The 40 draws are made inside the creature's creation, after the strength and fatness, each followed by its
  increase time's jitter, as in the original
  ([Random streams](../engine/random.md#random-streams)).
- A mind taken up from a file keeps its drawn decays (fixed): `FromFile` keeps the decays of the desires it is given
  to stand in for what the file lacks; before the fix those stand-ins sat at the middle of the range, so every desire
  of a creature taken up from a file faded at its range's middle (Fear 0x3F7FDF3C, 0.9995). `TakeUpFile` puts the
  drawn decays back, and its stand-ins are the desires drawn as the creature was made. The weights come from the table
  either way.
- The floor is openblack's constant 0 (`CreatureDesires.cpp:195`), not the species' `desireFloor`; every row of both
  info.dat files holds 0, so the result is the same with the game's data.
- The OnFire belief is `mind_detail::OnFireValue(ecs::fire::IsOnFire(thing))` for villagers and abodes (it used to be
  always 0, which in the original's values is "on fire").
- `IsObjectSuitableForDesire`'s gates: openblack has no failed-belief memory and keeps only Compassion's town members
  to the confinement, measured from the creature.

### The player's feedback in openblack

`CreatureMindSystem::ReceiveFeedback` stops first, forces LookAtCamera for a slight touch (the idle mind's
`LookAtCamera` step, as in
[Looking at and pointing at the camera](script-control.md#looking-at-and-pointing-at-the-camera)), then
`LearnFromFeedback`:

- the eating and the running away on the newest context, nothing more without a context worth crediting
  (`creature_learning::BestContext`, the newest of equals);
- the young creature's plain stroke or slap; after it, one time in two (`GameRand(2)`) it keeps something to do next
  (`CreatureMindState::afterFeedback`, the one slot it shares with a fizzled miracle): done once it is free to choose,
  the slap's fear (`GameRand(3)`: RunAwayFromPlayer one time in three, else BeFrightenedOnTheSpot, forced) or the
  stroke's happy action under ManifestState;
- the slap's give-up on the newest context (`creature_learning::PlanIsValid`, `creature_mind_tables::NeedsInstrument`);
- the stroke repeat forced at score 1.

The contexts finish only when carried out (`creature_learning::Finished`, seen by `LooksAt` on the game's camera), and
a new one takes the place of an unfinished newest one.

### Test hooks

- The debug spawner reports sightings to `SeeMiracle`, the only path that does today.
- `CreatureFrightening.OnlyFearListsRunningAwayFromAnObject` (skips without the game's data).

## Pending

- **Saving a mind**: openblack differs: the cursed size is divided by 15, the original multiplies by 0x3D888889
  (0x4E8F00); they differ by one ulp in about 63 % of heights (oracle on `Creature::SetHeight` 0x47DE80: 192 of 307)
  (`CreatureMindLearning.cpp:328`), a fix row. The `ScriptCreatureCurse` record (`Init`, `ResolveLoad` 0x6F61E0) is
  not ported (a port row).
- **Learning by watching**:
  - the turn: the original compares the game's turn (`g_game`+0x205A40), openblack the mind's own; it changes with the
    commit that first reports sightings from the game;
  - the meter (`fn_0047CDA0`) and the skill events: queued as a fix; the messages' texts wait on the creature help;
  - the sightings from the game (the six callers of `ConsiderLearningAction`, read): queued as a fix;
  - the species' multiplier is a 17-row table equal to `info.dat`'s column (checked), not read from the info;
  - the mind file's known list: that it is `ActionsKnownAbout` is 🔍 inferred (`SaveMind` 0x4E7820's list writer not
    read);
  - the scroll's text id: which magic rows have text id 0 is not read; openblack's scroll lists every known-about row
    from 1.
- **What a creature looks at**:
  - the look point: ground altitude at the quantised x, z plus `GetHeight` vt+0x42C for every kind (`fn_004D1BB0`,
    0x4D05F1); queued as a fix; which object stands for "the temple" there is not read;
  - the candidates: every object in the scanned cells, Fixed 0.25, MobileStatic 0.4 (toys 0.9), behind
    fn_004774F0(pos, 6), linked into the cells (fn_004D0A20) and not UNAVAILABLE (+0xA & 1); queued as a fix. Whether a
    held object stays linked into its cell is not traced;
  - the look class of CitadelDove (18) and CitadelBat (19): no class or vtable exists in the symbols; they are never
    made, so openblack's 0.7 is reached by nothing.
- **What frightens a creature**:
  - the other desires' object tests (the member table 0xC87864 + 0x28·d is read; each class's answer for its slot is
    not), so openblack's target per action stands;
  - `IsObjectSuitableForDesire`'s failed-belief gate (`fn_004D8E10`) and kept-area gate (`fn_0047CD20`): queued as
    fixes;
  - a spell as a Fear candidate: every caller of 0x4E3730 takes a belief, so a spell needs one; which of the 129
    `AddBeliefAboutObject` 0x4D7BD0 sites gives a spell is still open: by name the candidates are
    ShowFriendDestructiveSpell (0x4A70FC, 0x4A7181), ShowFriendCreationSpell (0x4A77C4, 0x4A7C9C), StealSpell
    (0x4B90A1), `MimicPlayer` 0x4E0E36, `CastSpellOnObject` 0x4F7B5D and `ForceActionCastSpellOnObject` 0x4F8883; the
    object type at each is not read;
  - CitadelBat (19): no class is known, so whether it frightens is unknown; it is never made;
  - slot 22 of `CreateAttributeArray` built with the dominant-desire vtable (see the OnFire belief above).
- **The player's feedback**: the help request after the young creature's plain feedback (openblack has no creature
  help yet), and the kept slot's other writers (the needs sign, the leash, a one-off spell seed, the cast in waiting)
  and its clearing when its belief is deleted; the tooltip of a slight touch; the stroke repeat's creature flag
  +0x10E8, its associated-object belief and its `CanCreatureCurrentlyPerformAction`; the other ways a context is marked
  seen (a step's end, the help text).
- **The innate personality and the previous actions**: openblack draws neither (see
  [Random streams](../engine/random.md#random-streams)).
</content>
