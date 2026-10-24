# The creature's mind

## Summary

The creature's mind is what makes it choose what to do: 40 desires that grow from their sources and fade, two
decision trees per desire (how good a thing is as what a plan is for, and how good it is to act on), learning from
the player's strokes and slaps,
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

#### The counts by object type

`CreatureMental::SaveMind` 0x4E79D0 writes, for each of 45 object types (0x2D, 0x4E7F57), three words: mind +0x183A8
+ 4t (0x4E7F5C), +0x1845C + 4t and +0x18510 + 4t; the reader fn_004E84A0 reads the first two from version 0x1D (29)
and the third from 0x1F (31) (0x4E8B88..0x4E8BCD). The first is `HeldObjectActionCounts`, how many times the creature
played with a thing of the type it held up (`ConstructSubActionsForHoldObject`, 0x4A2CBF / 0x4A2D5A; the first gift of
a kind from the hand reads and sets it too, see [grab-hold-and-throw.md](../hand/grab-hold-and-throw.md)); the third
is the not-food count (+0x18510). openblack keeps them as `creature_mind_model::ObjectTypeCounts::heldPlays`, `eaten`
and `notFood`, read and written through `MindFileData::learningCounts`.

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

#### The examples a mind file saves

- `DecisionTreeAgenda::SaveExamples` 0x4E92D0 writes the tree's example count (+0x10), then per example its context's
  (episode +8) lesson kind (+8), desire (+0xC) and action (+0x10) with `fn_00712160`, the context's belief (+0x14)
  through `CreatureBelief::Save` 0x4E8D90 (its type, its place +0x24 / +0x28, the attribute count and values), then
  the feedback (episode +0xC, `fn_007121E0`).
- Every example is made in two places. `fn_004E14C0` (its only callers are `LearnParticularLessonFromFeedback`
  0x4E11C0's four, 0x4E12DD..0x4E1440) puts a new episode {context, feedback} in the tree that `fn_004E1530` picks
  for the context's kind (kind 1: the first tree, kind 2: the second); `fn_004D42C0`, its add, has no other caller.
  `LearnParticularLessonFromFeedback` makes those four contexts with `fn_004E1CF0(kind, desire, belief, 0)`
  (0x4E12C5..0x4E12CC and 0x4E1323..0x4E132B with kind 1, 0x4E13C2..0x4E13C9 and 0x4E1420..0x4E1428 with kind 2):
  the action is always 0, and the belief is a copy (vt +0x34; `CreatureBelief::operator=` 0x4DA010 copies the place +0x24) of the creature's own,
  with the place it had then. The
  other is the starting lesson (below; kind 2, desire 4, action 0, `fn_004CE200`). `sub_4CBC20`, `sub_4CC990` and
  `sub_4CD800` also build kind 2 contexts and add them with `fn_004CE200`, but have no callers.
- So every example the game teaches is saved with the kind of its tree (1 in the first, 2 in the second) and action 0;
  only one read from a mind file keeps that file's own fields.

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

#### The two trees of a desire

Each desire has two trees, and neither is about the thing used [V]:

- **Where they are**: the `DecisionTreeCollection` at mind +0x2478 is two arrays of 40 agenda pointers, type 0
  (`DECISION_TREE_TYPE_ACTIVITY_OBJECT`, +0x2478 + 4d) and type 1 (`DECISION_TREE_TYPE_ACTION_OBJECT`, +0x2518 + 4d).
  Its constructor 0x4D7220 builds, for each desire, the type 0 agenda then the type 1 one
  (`DecisionTreeAgenda(type, desire)` 0x4D43F0); an agenda keeps its type at +0, its desire at +4, its examples at +0xC
  (count +0x10) and its tree at +0x14.
- **Type 0, the goal tree** (what a plan is for): `GetActivityObjectUsefulness` 0x4E9900 reads it (bw1-decomp
  `GetActivityObjectUtility`: `agendas[DECISION_TREE_TYPE_ACTIVITY_OBJECT]`). Its callers choose or judge a goal:
  `CalculateDesirabilityOfProposedActivity` 0x4C45FA, `AssessParticularObject` 0x4C51EA, `ShouldNearestTownBeAttacked`
  / `Helped` 0x4D3322..0x4D33A4, `UpdateSourcesFromBelief` 0x4DE822, `IsObjectSuitableForDesire` 0x4E37A0, the goal
  search fn_004F08D0 0x4F094D and fn_004F0980 0x4F0A17, `SetupReactToFire` 0x4F2FB1.
- **Type 1, the act-on tree** (what the plan's action is done to): `GetObjectToActOnUsefulness` 0x4E9A70 reads it
  (`[mind + 4d + 0x2518] + 0x14`, 0x4E9B67..0x4E9B78) and multiplies the tree's usefulness by
  `CalculateDistancePriority` (0x4E9B5E) and by the desire's object factor (the desire table's +0x24 member, 1 when
  there is none; 0.01 for Curiosity on an `Object` whose vt+0x350 says no and whose type, by vt+0x10C, is marked at
  mind +0x1D420, 0x4E9B02..0x4E9B52); for a creature kept within an area, beyond its radius it is 0, except for
  HangAroundAtHome (35) on a thing whose vt+0x35C says yes. Its callers judge what to act on: the action choice
  fn_004F1A10 0x4F1ABF, the act-on choice without a goal fn_004F10A0 0x4F112B, `FindBestObjectToActOn` of the town,
  flock, creature, forest and citadel beliefs (0x4DAD7F..0x4DAFE9), the two gates of
  `FindSomethingToDoOnObjectUsingObject` (0x4C540E, 0x4C5476), `HowInterestingIsObject` 0x4C3B44,
  `SetupReactToCreature` 0x4F2D30 and the sub-action sub_50A380 0x50A64D.
- **The mind file** keeps them by desire, type 0 then type 1: `DecisionTreeCollection::Save` 0x4E9150 writes 40, then
  for each desire the agenda at +0 and the one at +0xA0 (`DecisionTreeAgenda::Save` 0x4E9250: its type, its desire
  twice, its examples); the reader fn_004E9190 (from fn_004E84A0 0x4E8802) loads them in the same order by place
  (fn_004E9290 reads the type into +0 and the desire into +4, then grows the tree, fn_004D4340).
- **What teaches them**: every lesson goes through `LearnParticularLessonFromFeedback` 0x4E11C0 → fn_004E14C0, which
  takes its tree from fn_004E1530 by the lesson kind (context +8) and the desire (+0xC): kind 1 the desire's type 0
  agenda, kind 2 its type 1 agenda, kinds 0 and 3 (the lessons about the desire and about the action) and any kind
  above 3 (an unsigned compare) none (jump table 0x4E1560; run in the oracle on a fake collection for the kinds 0 to 4
  and the desires 0, 4 and 7). Kind 1: the feedback's lesson about what the plan was for (fn_004E0FC0),
  `UpdateHowNiceIsCreature` (±0.5, 0x4C4E48, 0x4C4EB7) and what the player shows on the aggression or compassion leash
  (`MimicPlayer`, ±1, 0x4E0EEA, 0x4E0F61). Kind 2: the feedback's lesson about what the plan was done to, and learning
  to eat what it holds (fn_004E06A0 0x4E085A, 1.0). Both kinds: copying the player (`DecideOnNewPlan` 0x4EA7B5, 0.5
  each) and the forgotten order (sub_4E1150). The starting lesson adds to the type 1 Hunger agenda directly (+0x2528).
  Three more initial lessons (sub_4CBC20 on type 1 Hunger, sub_4CC990 on type 0 Anger, sub_4CD800 on type 0 Impress)
  have no caller.
- **In the data**: in both `.erc` files of `Scripts\CreatureMind` the type 0 trees hold towns (0), creatures (8) and
  flocks (22), the things plans are for; the type 1 trees hold villagers (6), animals (7), abodes (3), fields (11) and
  the like, the things done to; Hunger's type 1 tree holds the starting lesson's enemy villagers at 0.8.

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

<a id="the-kind-of-belief-each-class-gives"></a>
#### The kind of belief each class gives

✅ [V: every vtable's slot +0x10C read with xref (`vtslots`), each override's body read, the info rows read from
`Scripts/info.dat`] `CreatureMental::AddBeliefAboutObject` 0x4D7BD0 files a belief in one of two lists by
`GetCreatureBeliefListType` (vt +0x110: 0 for Town 0x739200, Forest 0x539A90, Citadel 0x462A70, Flock 0x52F8B0 and
Creature 0x477DF0, 1 for the base 0x401990 and Spell 0x55CEB0); in the list, fn_004D7B00 asks `GetCreatureBeliefType`
(vt +0x10C, 0x4D7B0C) and keeps nothing of a type of 24 or more (`cmp eax, 0x18; jge`). The overrides:

| Type | Classes (vtables whose slot +0x10C holds it) |
|---|---|
| 0 | Town 0x7391F0 |
| 1 | Forest 0x539A80 (only Forest: BigForest keeps the base) |
| 2 | Citadel 0x462A60 |
| 3 | Abode 0x401740: Abode, Creche, Graveyard, PuzzleTotem, SpellDispenser, StoragePit, Totem, TownCentre, Wonder, Workshop |
| 4 | CitadelPart 0x464AA0: CitadelPart, CitadelHeart, CreaturePen, WorshipSite, WorshipTotem |
| 5 | Tree 0x55D950: Tree, MagicTree |
| 6 | Villager 0x55CA70: Villager, SpecialVillager |
| 7 | Animal 0x417840: every animal class (36 vtables) |
| 8 | Creature 0x473F70 |
| 10 | Spell 0x55CEC0: every spell class (13) |
| 11 | Field 0x527F20, FishFarm 0x52C4D0 |
| 15 | Feature 0x422140 (Feature, AnimatedStatic, Flowers, the chess pieces, WorshipSiteUpgrade), Football 0x531180 |
| 16 | Pot 0x55D580: Pot, PotStructure, PileFood, PileWood, PileResource, MagicFood, MagicWood, PuzzleGrain |
| 17 | SpellIcon 0x55D420: SpellIcon, TownSpellIcon, TownCentreSpellIcon, WorshipSpellIcon |
| 18 | SpellSeed 0x727F70, Mist 0x55EBA0 |
| 19 | PrayerIcon 0x670940 |
| 12 or 19 | MobileStatic 0x608B10: `IsRock` (vt +0x1F0) ? 12 : 19 (`neg; sbb; and 0xFFFFFFF9; add 0x13`). `IsRock` is 1 for Rock, Bonfire and Fragment (0x439730), 0 for DeadTree and FelledTree (0x5109D0) and for MobileStatic, MagicTeleport and the vortices (0x401B20) |
| 13, 14 or 19 | MobileObject 0x607430: its info's `MobileObjectType` (this +0x28, info +0x104): 4 → 14, 5 → 13, else 19. The classes: MobileObject, Ball, PBall, Arrow, Creed, FieldCrop, HanoiBlock, OneOffSpellSeed, Poo, PuzzleMobileObject, Reward, Scaffold, Whale |
| 21 | the base `GameThingWithPos` 0x405280, in 60 vtables, among them BigForest, GStreetLantern, GStreetLight, MagicShield, PhysicalShield, MapShield, ScriptHighlight, GBaseOnly (the singing stone's base, MOBILE_STATIC info 6), Fixed, Furniture, CitadelEntrance, HelpSpirit, LeashObj, MagicFireBall, InfluenceRing |
| 23 | TotemStatue 0x5610F0 |

The info rows' `MobileObjectType` (file offset 0xEC past each row's debug string; rows 0x104 apart in the file): the
mobile object table gives row n the kind n, but row 16 (N/A) 17 and row 22 (Eyes) 21; so only row 4 (Magic Food) is
type 14 and row 5 (Lump of Poo) type 13. The ball rows (Normal Ball, the ball and football tables) are kind 8, every
reward row -1, and a one-off seed is made with mobile object row 25 (`fn_0072A3A0` 0x72A3B6 passes
`GMobileObjectInfo::InfoList` + 0x1AF4 = row 25), kind 25: all three are 19.

<a id="the-attributes-a-belief-reads"></a>
#### The attributes a belief reads

✅ [V: each `UpdateValue` read with xref and run on the oracle with fake objects, 2026-10-22; script
`_scratch/Creador/tmp/fid/oracle_beliefs.py`] A belief's attributes are refreshed by `CreatureBelief::UpdateAttributes`
0x4D7E80 (from `CreatureBeliefList::AddBeliefAboutObject` 0x4D7CE0, which passes the believer's own player, its vt
+0x1C, at 0x4D7D04): each attribute's vt +0x28 `UpdateValue(thing, player)`, then the value is kept below its
`ValueRange` (vt +0x20).

- **Harder than me** (`AttributeIsHarderThanMe::UpdateValue` 0x4D40E0, range 2): 0 unless the thing is a creature
  (`CastCreature`, vt +0xA4: null for every other class), the player is given and has a creature (`GPlayer +0xA4C`);
  then 1 when the player's creature's `GetHeight` (vt +0x42C, `Creature::GetHeight` 0x477F50 = drawn size × 15) is
  below the thing's (`fcomp`, C0), so as tall gives 0 and a height that is not a number gives 1. Oracle: drawn sizes
  1 / 1 → 0, 1.0000001 / 1 → 1, 0.9999999 / 1 → 0, 2 / 0.5 → 1, 0.5 / 2 → 0, NaN / 1 → 1, no player → 0, a player
  without a creature → 0. Strength plays no part. openblack: `creature_tree::HarderThanMeValue` on
  `creature_pose::DrawnSize` × 15 of the thing and of the believer's player's creature
  (`PlayerSystemInterface::GetPrimaryCreature`), in `mind_detail::BeliefOf`; tests
  `CreatureDecisionTree.AnotherCreatureIsHarderOnlyWhenTallerThanThePlayersCreature`,
  `CreatureMindSystemTest.AnotherCreatureIsHarderWhenDrawnTallerThanTheBelieversPlayersCreature`.
- **A creature's height** (`AttributeCreatureHeight::UpdateValue` 0x4D3C70, range 4): the thing as a `Creature`
  (`__RTDynamicCast`), its `CreaturePhysical::GetUserSize` 0x4EF4F0 (the 3D body's +0x90, the drawn size) against
  0.25, 0.75 and 1.25 (0x8AB3D4, 0x8AB274, 0x8CF998): below 0.25 → 0, below 0.75 → 1, below 1.25 → 2, else 3; a size
  that is not a number fails the first test (C0) → 0. Oracle: 0 → 0, 0.2499999 → 0, 0.25 → 1, 0.7499999 → 1,
  0.75 → 2, 1.2499999 → 2, 1.25 → 3, 4 → 3, −1 → 0, NaN → 0. openblack: `creature_tree::CreatureHeightValue` of
  `creature_pose::DrawnSize`; test `CreatureDecisionTree.AnotherCreaturesHeightIsInFourBandsOfTheSizeItIsDrawnAt`.
- **Alive** (`AttributeLife::UpdateValue` 0x4D39A0, range 2): the thing as an `Object` (`__RTDynamicCast`; not an
  object → 0), its `GetLife` (vt +0x11C; `Object::GetLife` 0x402600 = +0x48 for the animals, abodes and the other
  objects) against 0.2 (0x8AB244, `test ah, 0x41`): 1 only above it; 0.2 itself and a life that is not a number give 0.
  The same function for villagers, animals and buildings. Oracle, on a cow: 0, 0.1, 0.2 → 0; 0.2000001, 0.5, 1 → 1;
  NaN → 0. openblack: `creature_tree::LifeValue` of `ecs::life::LifeOf` for villagers, animals and abodes; tests
  `CreatureDecisionTree.BuildingsAndVillagersShowTheirLifeAndWhetherTheyAreBuilt`,
  `CreatureAnimalFoodTest.WhatACreatureKnowsOfAnAnimal`.

<a id="the-last-moving-thing-seen"></a>
#### The last moving thing seen

✅ [V: `AddBeliefAboutObject` 0x4D7BD0..0x4D7C3B read, the writers and readers of mind +0x1D3C8..+0x1D3D8 found by
a scan of the code for the offsets] Before it files the belief, `CreatureMental::AddBeliefAboutObject` asks the thing
`IsMoving` (vt +0x174, 0x4D7BDC). When it moves, is not the mind's own creature (mind +0x20D18, 0x4D7BE6) and the
record's weight (+0x1D3D8, a float) is not above 0.5 (`fcomp [0x8AA3B4]` = 0.5, `test ah, 0x41; je`, so an
unordered compare keeps it too), it keeps the thing's MapCoords (+0x14, 12 bytes) at +0x1D3C8, then writes
`GetHeight` (vt +0x42C) over the MapCoords' altitude (+0x1D3D0), the game turn (`g_game` +0x205A40) at +0x1D3D4 and
0.5 at +0x1D3D8. This is the `CreatureLookState` at mind +0x1D3C0 (bw1-decomp `CreatureLook.h`: +0x8 `coords`, +0x14
`GameTurn`, +0x18 the weight).

- `IsMoving` by class (slot +0x174 of every vtable): `Object::IsMoving` 0x402710 (`Pos` x or z, +0x14 / +0x18, not
  the object's `coords` +0x2C / +0x30) in 132 vtables, every Object class; `GameThingWithPos` 0x4052B0 (0) in 51, the
  towns, forests, flocks, citadel, spells, mist, the planned buildings and the like; `Creature` 0x479E40
  (`LH3DCreature::IsMoving` 0x484300), `GCamera` 0x441B00, `FireFly` 0x52A1D0.
- The writers of `Object::coords` (+0x2C, 12 bytes from `Pos` +0x14), found by a scan of the whole code for a store
  through +0x2C beside a read of +0x14 (`mov [r+0x2C]`, `lea x, [r+0x2C]`, `add x, 0x2C`, then the stores):
  - the `Object` constructor 0x636592 (the made position; the default constructor 0x636476 zeroes it for the loader);
  - `Living::ProcessLiving` 0x5EC8B3, every living each turn before its reaction and state
    ([teleport.md](../magic/teleport.md#who-uses-a-stone)), and `Whale::Process` 0x775285 at the start of its turn;
  - `GScript::SetPosition` 0x6F8C45, after `MoveMapObject`, for an Object;
  - `Object::EndPhysics` 0x6375C7 (slot +0x790), when it has no physics object or the thing is in the physics (flag
    +0x24 & 0x40, IN_PHYSICS, cleared there): every class's end of physics reaches it (Fixed 0x52DF50, Pot 0x66DBD0,
    Villager 0x5F0A60, Animal 0x5F0D80, Scaffold 0x6E8780 and PuzzleGrain 0x561A40 call it; Rock, DeadTree, FelledTree,
    Fragment and Tree go through Fixed's), and `Tree::EndPhysics` 0x74BBF5 for the dead tree it makes.
  None of the moves writes it: `Object::MoveMapObject` 0x636A40, `MultiMapFixed::MoveMapObject` 0x52E4F0 and
  `SpellIcon::MoveMapObject` 0x7265D0 leave it, so a thing moved by them (a flag a storage pit moves, a building moved
  into place) goes on moving until one of the writers above.
- The only reader is fn_004D0E70, the second of the three looks on the way to a far point
  ([script-control.md](script-control.md#an-action-forced-by-a-script-creature_do_action), `LookWhileGoingTowardsPoint`):
  with no interesting thing (mind +0x1AA40), while the game turn less +0x1D3D4 (unsigned) is below
  `__ftol(2.0 × (1000 div` the ms per turn `))`, the look is the kept MapCoords (its altitude the height, over the
  ground there); otherwise it writes 0 to +0x1D3D8 and the look is the point. fn_004D0CD0 also tests +0x1D3D8 against
  1.0 (0x4D0D73), which nothing writes. The three writes of +0x1D3D8 are 0x4D7C3B (0.5), 0x4D0F18 (0) and none other.

<a id="the-plans-things-met-by-the-sweep"></a>
#### The plan's things met by the sweep

✅ [V: the writers and readers of mind +0x1AA60..+0x1AA6C found by a scan of the code for the offsets] Mind +0xF58 is
the plan's belief about the thing it acts on and +0xF5C the one about the thing it uses (the agenda's; 0x507C40 sets
+0xF5C). Through the sweep (mind +0x1AA38, cleared by fn_004D0390 0x4D0390, which zeroes +0x1AA60..+0x1AA70):

- `AddBeliefAboutObject` 0x4D7C45..0x4D7C6E: the thing is +0xF58's (belief +0x30) → +0x1AA60 = 1; +0xF5C's →
  +0x1AA68 = 1 (the thing was met).
- fn_004D0A20 0x4D0A55 / 0x4D0A83, the cell handed over (x, z): the belief's cell (its position's high words, belief
  +0x26 and +0x2A) is that cell → +0x1AA64 = 1 / +0x1AA6C = 1 (its cell was looked at).
- `Creature::StopWhatIAmDoing` 0x475C62..0x475C86 writes 0 to all four.
- The only reader is fn_004D2740, which fn_004D07A0 0x4D0807 calls as a new sweep starts (+0x1AA74 back to 0), when
  the plan's action row (`g_CreatureActionInfos`, 0x110 bytes a row, row mind +0xF60) has +0xC0 set, before the
  sweep's flags are cleared. For each of the two beliefs it calls fn_004D2780(belief, cell looked at, thing met): with
  a belief, its cell looked at and the thing not met, fn_004D2710 (true unless mind +0xFB8 is set and the body is not
  `IsMovingOrStanding` 0x4842E0) and the thing `IsObjectInMap` (vt +0x178), it finds the belief about the same thing
  in the mind's list (+0x222C, next +8) and copies the thing's `Pos` (+0x14) into that belief's position (+0x24).

So the plan's believed place of its thing is put right when the creature looked where it believed the thing was and
did not find it there. The believed position +0x24 is what the plan's distance and the look at an object's foot read
(`CalculateDistancePriority`, `GetObjectToActOnUsefulness`, `LookAtObjectArgumentBottom`).


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
| 0xEC | 0xFC | `HowNiceChange` | fn_00475450 0x475643 (from `ProcessSubAction`, as an action completes), into `UpdateHowNiceIsCreature` 0x4C4D50 on the creature of the mind's act-on belief (+0xF58, a dynamic cast to `Creature`) | how much nicer the creature finds the creature the action acted on (openblack `creatureNicenessChange`); with no thing, sub-action 0x6C made the act-on belief the creature's own, so it changes how nice it finds itself |
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
- The eating's lesson [V] (0x4E07D8..0x4E085A): the belief about what it holds (`GetBeliefAboutObject`), a plan
  {Hunger, action 11, no goal, that belief as what it acts on and uses, 1.0}, its context (source 61, none), and
  `LearnParticularLessonFromFeedback(context, 1.0, lesson 2, 1, ...)` 0x4E11C0. That is the feedback's act-on lesson
  (the jump table's case 2, 0x4E1353): skipped when the plan is not valid (no belief), when what it acts on is the goal
  or the creature itself; 1.0 to Hunger's tree type 1, then, for every other desire in order whose dependency on
  Hunger (`g_CreatureDesireDependency` +0x10 + 4 (44 x 4 + other)) is neither 0 nor NaN (`test ah, 0x40`),
  dependency x 1.0, with no clamp. In both `Scripts\info.dat` and `CreatureIsle\Scripts\info.dat` Hunger's row
  (file offset 0x632A4 + 4 x 0xA0) is all 0, so only Hunger learns it there.
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
- The lessons (fn_004E0FC0, from the second stage, only when the desire's initial info +0x38 is set) are
  `LearnParticularLessonFromFeedback` 0x4E11C0 called with the lesson types 0, 1, 2 and 3 in that order (the loop at
  0x4E0FF1..0x4E101A), each returning early when `CreaturePlan::IsValid` fails. The context's plan sits at +8: its
  desire at +0x10, the belief of what it is for (`ActivityObject`) at +0x14, of what it acts on at +0x18 (bw1-decomp
  `CreaturePlan.h`). Jump table 0x4E14B0 = {0x4E1288, 0x4E129F, 0x4E1353, 0x4E14A0} [V]:
  - 0, the desire (`UpdateDesireFromLesson` 0x4E15A0);
  - 1, **what the plan is for**: none without that belief (0x4E12A2); no test of the creature itself and no limit; a
    learning context of kind 1 (`fn_004E1CF0`) for the desire with f, then, for every other desire whose dependency
    (`g_CreatureDesireDependency` +0x10, a row of 44 floats per desire) is not 0 (nor NaN: `test ah, 0x40`), in
    ascending order, the dependency × f (`fn_004E14C0` adds the example and grows the tree);
  - 2, **what it acts on**: skipped when that belief is the one the plan is for (`cmp [ctx+0x14], ebx`, 0x4E1356: the
    same belief; a creature keeps one belief per thing, `CreatureBeliefList::AddBeliefAboutObject` 0x4D7CE0 returning
    the one `GetBeliefAboutObject` finds) and when it is the creature's own (0x4E135F..0x4E1368); for Poo (desire 7,
    0x4E1371) f is held to [-0.5, 0.5] first (below -0.5 it is -0.5, above 0.5 it is 0.5, 0x4E1376..0x4E13A2); then
    kind 2, spread as for 1 with the held f;
  - 3, the action's opinion (`fn_004E15E0`).
  There is no lesson on the instrument (+0x14 of the plan). Kind 1 goes to a desire's tree of type 0 at mind +0x2478
  (`DECISION_TREE_TYPE_ACTIVITY_OBJECT`), kind 2 to its tree of type 1 at +0x2518 (`ACTION_OBJECT`): `fn_004E1530`'s
  jump table 0x4E1560 = {none, +0, +0xA0, none}; the collection's constructor 0x4D7220 makes type 0 then type 1 per
  desire and `DecisionTreeCollection::Save` 0x4E9150 writes them in that order. The planner reads type 0 in
  `GetActivityObjectUsefulness` 0x4E9900 (0x4E9A3B) and type 1 in `GetObjectToActOnUsefulness` 0x4E9A70 (0x4E9B67).
  Each lesson that teaches also gives its line (`RespondToPlayerFeedback` 0x4E1670): lesson 1 names the thing it is
  for, lesson 2 the thing it acts on, with the same format (0xBDF9EC), at the unheld f. The oracle on 0x4E11C0 (the
  plan valid, the learning stubbed and its arguments captured) [V]: lesson 1, Poo, f 0.9, dependencies Play 0.5 and
  BuildHome -0.25 → Poo 0.9, Play 0x3EE66666 (0.45), BuildHome 0xBE666666 (-0.225); lesson 2, Poo, f 0.9 → 0.5, 0.25,
  -0.125; f -0.9 → -0.5; f 0.3, 0.5, -0.5 unchanged; 0x3F000001 → 0.5; Hunger 0.9 with Poo dependent 0.5 → 0.9 and
  0.45 (Poo's not held); act-on belief = goal belief, or its thing = the creature: nothing.
- The contexts: only the success path fn_00475450 finishes one (+0x48 = 1, +0x4C = the turn, +0x3C = 1 when
  `GetInterfaceStatusLookingAtCreature` 0x4775D0 finds a camera of the creature's player within 400 m and inside
  dot > cos(2/3 x 70 degrees)); a step's end in `ProcessSubAction` 0x4FF128 and the help callback 0x4CAA80 also set
  +0x3C. `PreviousContextStack::Push` 0x4F23D0 overwrites the newest in place while it is not finished.
- **Five contexts** ✅ [V]: the stack (`PreviousContextStack`, learning +0x2AD8 of the mind; bw1-decomp
  `CreatureContext contexts[0x5]`) is a ring of five, its next slot at +0x20 and its count at +0x24, both wrapped or
  capped at 5 in `Push`. With five held, `Push` first calls the stack's forget callback (+0x10, set by the
  `CreatureLearning` constructor 0x4E0290 to sub_4E1100) on the oldest, then writes the new one over it. openblack
  kept eight until 2026-10-18 (`creature_learning::k_MaxContexts`, now 5).
- **The forgotten context's lesson** ✅ [V] (raffclar's #560, refitted): sub_4E1100 takes the oldest context (through
  0x4E1E50 on learning +0x2AD8) and goes on when it was seen (+0x3C) or forced (+0x44), its lesson mark (+0x40) is 0,
  and its desire's initial-desire row has the flag at +0x3C of the exe's array (stride 0x1C0; info.dat's field 0x2C of
  the 0x1B0 row). sub_4E1150 then teaches only a forced one: the approval is the species' creature info +0x2A0
  (info.dat's `GCreatureInfo` 0x290, `orderApproval`, 0.3 in the shipped data), through
  `LearnParticularLessonFromFeedback` 0x4E11C0 twice, origin 2 (indirect, the player watching): type 1 (activity
  object, the context's +0x14) when there is one, and type 2 (action object, +0x18). Each lesson needs
  `CreaturePlan::IsValid`; both spread over the desires by `g_CreatureDesireDependency`; type 2 skips the creature
  itself, and for poo (desire 7) is held within ±0.5 (see the next item).
- **The lessons about a plan's things** ✅ [V] (2026-10-19): the context is `CreatureContext`, whose plan sits at +0x8
  (bw1-decomp `CreatureContext.h`, `CreaturePlan.h`), so its +0x10 is the plan's desire, **+0x14 the plan's
  `ActivityObject`** (what the plan is for), **+0x18 its `ObjectToActOn`** and +0x1C its `ObjectToUse`. The context's
  constructor 0x4E1B80 copies the plan field by field (0x4E1BAD..0x4E1BF0) and then replaces each of the three
  beliefs by its own copy (vt +0x34, `Copy`, 0x4E1BFF / 0x4E1C0E / 0x4E1C1D). `IsValid` 0x4F12E0 reads the same
  places (+0xC the goal when the desire looks for one, +0x10 the act-on, +0x14 the instrument when the action asks
  for one). `LearnParticularLessonFromFeedback` 0x4E11C0, the one function both the feedback (fn_004E0FC0, the lesson
  types 0 to 3 in turn, 0x4E0FF1..0x4E101A) and the forgotten order (sub_4E1150, types 1 and 2) teach through:
  type 1 (0x4E129F) takes +0x14 and makes a kind 1 context of it (0x4E12C8), so the goal tree; type 2 (0x4E1353)
  takes +0x18 and makes a kind 2 context (0x4E13C5), so the act-on tree, skipped when +0x14 is the same pointer
  (0x4E1356) or the belief's thing (+0x30) is the creature (0x4E1362), and for desire 7 held within ±0.5
  (0x4E1371..0x4E13A2). No type reads +0x1C: nothing is learnt about the thing used. The first skip never happens:
  the two pointers are the context's own copies, and every belief's `Copy` (0x4D9D60 .. 0x4DA9D0) allocates a new one
  (the base's 0x4D8B40 returns 0), so a plan done to its own goal teaches both trees.
- **What "forced" is**: +0x44 is the int `CopyPlanToPreviousContext` 0x4F1600 gives the context's constructor (stored
  at 0x4E1C42). `ForceActivityAndForceAction` 0x4C44B0 passes its own, and its read callers pass 1 (the leash and the
  stroke repeat in 0x4E06A0, fainting, the teleport, the catch, `ReduceLife`, the fight, the three mimicry copies,
  0x508C10, 0x509480, 0x50A1F0, 0x50A380); `ForceActOnObject` passes 1 and `ForceActivityAndDecideAction` 0; a few
  pass a register not traced (`SwapMinds`, `TrappedInEnclosedSpace`, 0x47A1D0, `DevFunction`, 0x5073E0). So every
  plan forced on the creature counts, not only the player's orders.
- **openblack:** `creature_learning::Remember` returns the context forgotten to make room;
  `CreatureMindSystem::RememberAction` teaches `ForgottenOrderApproval` (the context's `ordered`, the rules'
  `approvesOrders`) through `creature_learning::ThingLessons` (below), by `creature_learning::Spread`, after
  `PlanIsValid`. `ordered` is set for every plan through `ForceActivity`, the path of
  all our forced plans; the planner's own plans and the agenda's actions leave it off. Two vanilla bugs are kept: the
  lesson mark is set on a copy, so a slapped forced action still approves when forgotten; and a seen action passes the
  first test but teaches nothing.
- **Pending**: the int the planner's own plan (0x4CEC00) and the agenda's action (0x5062C0 passes 1) give; the
  register-passed callers above.
- The action rows' instrument flag (+0x44 of the code-built table 0x9D1678, stride 0x50): set for 37 rows (14 15 24
  28 30 31 32 33 35 36 68 69 73 83 135 171 232 233 234 235 241 245 247 249 252 262 280 281 283 285 291 303 306 307 308
  326 327).

#### The attention its god pays it

The mind scroll's "how much attention" line is a share of the creature's life, not a fade over time alone (v1.42
exe; bw1-decomp has the `CreatureAttitudeToPlayer` names only).

- **The counter** ✅ [V]: `fn_00477A00`, called once a turn from `Creature::ProcessState` (0x47338C, inside the
  [0xD00DE8] block, always 1: written only by the GGlobal constructor 0x590F00). When
  `GetInterfaceStatusLookingAtCreature` 0x4775D0 finds a camera: +0x18C48 += 1 (the looked-at streak), **+0x18C50 +=
  1 (the turns looked at)**, +0x18C4C = 0; else +0x18C4C += 1 and +0x18C48 = 0. Always +0x18C54 += 1. The same
  counters are listed under "Seconds alone" in [Script control](script-control.md). The turns lived are the body's
  physical +0x70, `++` at 0x472FC2 on the same path; both are skipped while the creature is in a locked select (Flags
  +0x24 & 0x10, the jump at 0x472F7B), with no early exit between them (0x472FC5..0x4732FA).
- **The looking test** ✅ [V], 0x4775D0: only for a creature with a player (vt +0x1C). It walks that player's
  interface statuses (`GPlayer::GetNextInterfaceStatus` 0x64AAC0, 18 slots at +0x14), the camera origin +0xB0 and
  focus +0xBC of each (the camera the interface sends at the turn's start), and the creature's 3D body's place
  (physical +0x58 → +0x78). At 24-bit precision:
  1. A = focus − origin, P = point − origin;
  2. A ≠ 0: L = sqrt((Az² + Ay²) + Ax²), r = 1/L, A ×= r (0x47767A..0x4776A4);
  3. P = 0: D = 0, straight to the dot; else D = sqrt((Pz² + Py²) + Px²) stored to a float (0x4776ED), q = 1/D,
     P ×= q;
  4. D < 400.0f (0x47770D, [0x8C7728]), else not looked at;
  5. dot = (Pz·Az + Py·Ay) + Px·Ax (0x47771E..0x47772A);
  6. c = fcos(round24(1.2217305f [0xBF34D4] × 0.6666667f [0x8AB4C8])), kept unrounded: 0.68624164065…, while
     float(c) = 0.6862416267 lies below it;
  7. looked at iff dot > c (`fcompp`, C0, 0x47773E..0x47774B). With several statuses the nearest D is kept; P = 0 or
     A = 0 gives dot 0, not looked at.
- **The scroll** ✅ [V]: `CreatureRoom::MakePersonalityScrollText` 0x788D20 (its text ids are 4 below the bw1-decomp
  header's) adds ATTITUDE_TO_PLAYER (0x4A9) only when (double)(u32 +0x18C50) / (i32 physical +0x70) > 0
  (0x788EA5..0x788F00), then HOW_MUCH_ATTENTION_PLAYER_HAS_GIVEN_ME (0x4AA) always. Its word comes from
  `TempleRoom::AddText` 0x79A910, case 0x4AA (0x79B560..0x79B616): share = physical +0x70 == 0 ? 0 : (u32 +0x18C50)
  / (i32 +0x70), rounded to 24 bits; × 5.0f ([0x8AB6E4]), rounded again; `__ftol` (cut toward zero); `cmp eax, 4;
  jbe`, unsigned, so anything above 4 or below 0 is 4; text 0x471 + level, HELP_TEXT_HOW_MUCH_01..05. The creature's
  export text `fn_0047B440` (0x47B8CD..0x47B91B) maps it the same way.
- **Saved with the mind** ✅ [V]: `SaveMind` 0x4E79D0 writes +0x18C50 just before the attitude +0x18C58
  (0x4E7D72..0x4E7D89); the reader fn_004E84A0 reads it back (0x4E88D6).
- **Its readers** (a scan of the whole .text for the displacement): `DisplayStatistics` (a debug dump), the export
  text, `SaveMind` and its reader, and the scroll. No decision reads it.
- **The turns not looked at, desire source 36** ✅ [V]: `AttentionFromLoneliness` 0x4DEA10 (the source table's
  getter for type 36) = the turns +0x18C4C cut to whole seconds by two unsigned divisions (1000 `div` the ms a turn
  [0xD01A38], then the counter `div` that), `fild` and × 0.016666668f ([0x8C5818], 0x3C888889, no writer), 0 below 0,
  1 above 1 (`test ah, 1` / `test ah, 0x41`). Its setter 0x4DEA80 (reached by `SetSource`, see
  [the leash's desires](leash.md)) writes +0x18C4C = `ftol`(1000 / ms × value × 60.0f). The other readers of
  +0x18C4C (the same scan): `IsPlayerIgnoringMe` 0x4DF0C0, `HowNeglectfulIsPlayer` 0x4C3F60 (no direct caller),
  sub_4F0030 and `DisplayStatistics`. Nothing saves it: `SaveMind` writes only +0x18C50 of the four.
- **The turns since it was sent to act, desire source 37** ✅ [V]: `AttentionFromLackOfInteraction` 0x4DEAD0 = the
  same whole seconds of +0x18C54 × 0.008333334f ([0x8D006C], 0x3C088889, no writer), within 0..1; its setter
  0x4DEB40. +0x18C54 counts every turn of `fn_00477A00` and is zeroed in three places (the scan of the whole .text):
  `ForceActOnObject` (0x4C629B, after its tests, the big-forest hand-over to `ForceActOnMapCoord` and the fish-farm
  meal, as it acknowledges the order), `ForceActOnMapCoord` (0x4C6B11), and the feedback's lessons 0x4E0FC0
  (0x4E106C, at the end, on both of its paths: also when the context's desire can't be taught, the jump at
  0x4E0FE2). Its other reader is `DisplayStatistics`. Neither the rest of the feedback 0x4E06A0 nor
  `UpdateAttitudeToPlayerFromFeedback` 0x4E1080 touches +0x18C4C or +0x18C54.
- **Every player's camera** ✅ [V]: every player has an interface: the game's start gives one to each player left
  without a human interface (bw1-decomp `Game.cpp`, `players[i].Init(PLAYER_TYPE_0, …)` → `GPlayer::Init`, which
  makes `interfaces[player_id]`). `Citadel::CreateCitadel` (0x4632EC..0x46330E) sets every interface status of the
  citadel's player looking at the citadel (fn_005DBB40: focus +0xBC = the citadel's place, its height the land's
  altitude there plus the citadel's +0x1C; origin +0xB0 = focus + (0, 20, 20)). So a computer player's creature is
  looked at by its own player's still camera above its citadel, never by the human player's. Whether anything else
  moves a computer player's camera is not read.

### A thing that the creature uses goes

`CreatureMental::RemoveBeliefsAboutDeletedObjects` 0x4D83C0 walks the creature's beliefs. For each one whose object is
deleted (the object's +0xA bit 1): when `IsBeliefCurrentlyUsedInAgenda` 0x4D7D90 says it is in use, a flag is set. In use
means the plan at mind +0xF48 is valid (`CreaturePlan::IsValid`), the sub-action agenda has steps (+0xFBC) with the
current one (+0xFB4) not past the last, the agenda's state (+0xFB8) below 3, and the belief is one of the plan's three
(+0xF54, +0xF58, +0xF5C) or the belief of a sub-action from the current one on (+0xFDC, 0x60 bytes each). Each per-desire
plan (+0x718, 0x30 bytes, 40 of them) that the belief's bit set names is zeroed. Then every sub-action from the current
one on: when its belief's object answers 0 to its vtable +0x2C, or its other thing (+0x20) is deleted, the sub-action
agenda is cleared (`ClearSubActionAgenda` 0x4FF5C0) and the flag is set. With the flag set: when the plan's desire (mind
+0xF50) is 5, Fear, it is made least dominant (`MakeDesireLeastDominant`, factor 1.3, 0x4D85C4..0x4D85D8); the help gets
type 0x28 with 8 (`SendCandidateHelpScript` 0x4C9FE0, 0x4D85DD..0x4D85ED); and the creature reconsiders
(`ReconsiderDecision` 0x4E1570) [V]. What the vtable +0x2C test is has not been read.

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
- **The examples' saved fields**: `creature_mind_model::Learn` keeps an example taught without a lesson kind of its
  own with its tree's (1 for the first tree, 2 for the second) and action 0, as the original saves it; the starting
  lesson brings kind 2, and a file's examples keep the file's fields. The place of the thing (the belief's +0x24 /
  +0x28) is still saved as 0: openblack's beliefs keep no place (a port row). Debug: the creature spawner's
  Decision trees node lists each example's saved fields.
- **The two trees** are `creature_mind_model::TreeKind`: `Goal` is a mind file's type 0, `ActOn` its type 1, and
  `FromFile` / `ToFile` keep them by type. The planner's things carry both usefulnesses
  (`creature_planner::ObjectCandidate::goalUsefulness` and `usefulness`): a goal is chosen and counted by the first, a
  thing acted on by the second. The debug window's Decision trees node names them "what a plan is for" and "what to
  act on". As the original's ([The two trees of a desire](#the-two-trees-of-a-desire)):
  - the goal tree is read for compassion's town (`CompassionTown`), for the goals of the desires that look for one
    (`PlanDesire`), for the desire a creature most wants about another creature (`mind_detail::DesireAboutCreature`,
    the original's `AssessParticularObject`), for the asked creature's answer to a friend's request and for whether
    to attack the nearest town (`CreatureMindFriends.cpp`, the original's `CalculateDesirabilityOfProposedActivity`
    and `ShouldNearestTownBeAttacked`), all of which read type 0 through `GetActivityObjectUsefulness`; it is taught
    by what the player shows on a leash and by two creatures leashed together (the original teaches both with the
    lesson kind 1), and by the feedback's and the forgotten order's lessons about what the plan was for (kind 1);
  - the act-on tree is read for what a plan is done to (`PlanWithoutGoal`, Anger's goal acted on, a town's members in
    `BestActOn`, a town member counting only as that), for the food it eats first (`mind_detail::FoodUsefulness`), for
    what it does with a thing it is given (`UseGift`), for the thing a stroke has it act on again and for whether
    another creature is of use to look over (`ObjectToActOnUsefulness`, the original's `GetObjectToActOnUsefulness`,
    which reads type 1); it is taught by
    the feedback's and the forgotten order's lessons about what the plan was done to, by learning to eat what it is
    given (kind 2, all three) and by the starting lesson;
  - the feedback and the forgotten order teach a plan's things through `creature_learning::ThingLessons`, as the
    original's lesson types 1 and 2 ([The player's feedback](#the-players-feedback), "The lessons about a plan's things"): the
    context's `aboutBelief` (what the plan was for, the original's activity object) in the goal tree, its `belief`
    (what it was done to, the act-on object) in the act-on tree unless that is the creature, held within ±0.5 for poo;
    nothing about what it used (`used`, the instrument). Until 2026-10-19 the forgotten order taught `belief` in the
    goal tree, neither taught what the plan was for, and both kept a lesson about what it used that never ran (its
    belief was never filled). Test
    `CreatureMindModel.APlanTeachesWhatItWasForToTheGoalTreeAndWhatItWasDoneToToTheActOnTree` and `CreatureLearning.FeedbackTeachesAboutWhatThePlanWasForThenWhatItWasDoneTo`;
  - `creature_mind_model::LessonKind` and `TreeOf` are the original's lesson kinds and the tree each teaches (the
    original's chooser, run in the original for the kinds 0 to 4: 1 the goal tree, 2 the act-on tree, the rest none);
    test `CreatureMindModel.ALessonAboutWhatThePlanWasForTeachesTheGoalTreeOneAboutWhatItWasDoneToTheActOnTree`.
- **The starting lesson** is `creature_mind_model::StartingLesson` (lesson kind 2) and `TeachStartingLesson`, on
  hunger's act-on tree and only while that tree has no example, from
  `CreatureMindSystem::SetUpLearning` and, for what a mind file does not hold, `TakeUpFile`.
- **Saving a mind as the creature itself**: `creature_spells::ValuesToSave`, `mind_detail::CurseValuesToSave` and
  `CreatureMindSystem::SaveMind` follow the before-values and the curse globals, but openblack differs: the cursed size
  is divided by 15 (`creature_morph::k_HeightAtSizeOne`), the original multiplies by 0x3D888889
  (`mind_detail::CurseValuesToSave`, `src/ECS/Systems/Implementations/CreatureMindLearning.cpp`), a fix row.
  `SaveMind` is called by `player_creature::ToMindFile` (`src/ECS/PlayerCreature.cpp`), which writes the player's
  creature to its file at a land change (`SaveMyCreature`, from `Game.cpp`) and as a creature is removed, and swaps
  two minds (`CreatureMindSwap.cpp`); the curse rule is reached only while the curse script runs. The
  `ScriptCreatureCurse` record and its restore at load are not ported (a port row).
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
- the eating's lesson for Hunger and, in proportion, every desire depending on it
  (`creature_learning::EatWhatItHoldsLessons`, the feedback's `Spread`), in `TreeKind::ActOn` (index 1, the original's
  type 1);
- the slap's give-up on the newest context (`creature_learning::PlanIsValid`, `creature_mind_tables::NeedsInstrument`);
- the stroke repeat forced at score 1.

**A plan that names nothing is a plan on the creature itself** ✅ [V] (`creature_learning::OnItself`). In the original
every plan holds a belief to act on, and an action done on the spot holds the creature's own belief: the planner gives
it (fn_004F10A0 at 0x4F118E), and the forced on-the-spot actions pass `GetBeliefAboutObject(itself)` (fainting
0x4F3B9D..0x4F3BAE, the hand pick-up 0x4F374C..0x4F375B, the kept next actions 0x509083..0x509094,
0x509143..0x509154, 0x509543..0x509557, and the others at 0x4F76CD, 0x4F77CD, 0x4F77FD, 0x47D0D7, 0x4C7299; the reactions' calls of
`ForceActivityAndForceAction` 0x4C4450 pass the belief about what they react to, `ScriptDoAction` 0x4F7021 the
script's target; the slap's kept fright 0x509010 passes the one it was kept with, not read here). openblack's on-the-spot plans name no thing, so its readers take "nothing" for "itself":

- `CreaturePlan::IsValid` 0x4F12E0 always needs the act-on belief (+0x10, 0x4F1305), which an on-the-spot plan has, so
  the slap gives up such a plan too: `PlanIsValid` no longer refuses a context with no thing (it did, so a slap after
  a sleep, a sit or a look left the desire as it was);
- the lesson about the thing acted on (`LearnParticularLessonFromFeedback` 0x4E11C0, lesson 2 at 0x4E1353) is skipped
  when that belief is the creature's own (its +0x30 against the mind's creature +0x20D18, 0x4E135F..0x4E1368): no
  lesson about itself, whether the context names it or nothing (openblack learnt one when it named itself, as the
  cut-scene look does);
- the stroke repeat needs a thing other than itself (0x4E0C5C), as before, now through the same test;
- the creature spawner's readouts (the planner, the plans table and the recent actions say "itself").

**The lessons about things** ✅ [V] (`creature_learning::ThingLessons`, in `LearnFromFeedback` after the desire lesson
and before the opinion, and in the forgotten order; [the lessons in the original](#the-players-feedback)): one about
the thing the plan was for (`Context::aboutBelief`, the plan's own copy of what it knew of it), whenever it knew it, at
the feedback, in the goal tree; then one about the thing it was done to (`Context::belief`), in the act-on tree,
skipped only when that is the creature itself, and for Poo at the feedback held to ±0.5 (`k_PooActOnFeedback`). A
plan done to the thing it was for teaches both, since each belief is the plan's own copy. Each spreads to the
dependent desires (`Spread`). Before, openblack had no lesson about the thing a plan was for, and held Poo's feedback
on a lesson about the thing used, whose belief was never filled; that lesson and `Context::usedBelief` are gone (the
original has none). Until 2026-10-19 openblack read what the plan was for at the feedback rather than from the plan,
and skipped the second lesson for a plan done to its own goal. The creature spawner's mind window has a closed
"Lessons about things" node: what each recent action's last feedback taught (`Context::taught`, debug only). Tests:
`CreatureLearning.FeedbackTeachesAboutWhatThePlanWasForThenWhatItWasDoneTo`, pinned to the oracle's numbers, and
`CreatureMindModel.APlanTeachesWhatItWasForToTheGoalTreeAndWhatItWasDoneToToTheActOnTree`.

The contexts finish only when carried out (`creature_learning::Finished`, seen by `LooksAt` on the game's camera), and
a new one takes the place of an unfinished newest one.

**The attention** ([The attention its god pays it](#the-attention-its-god-pays-it)): `CreatureMindSystem::ProcessTurn`
counts `CreatureMindState::turnsLookedAt` each turn the local player's creature is out of the hand and
`creature_attention::IsLookingAt` holds for `Locator::camera` (raffclar's `CreatureAttention`; `creature_learning::LooksAt`
forwards to it). The mind scroll reads `creature_attention::Share(turnsLookedAt, Needs::turns)` and
`HowMuch` at float precision (`gutils::x87`), and shows the attitude line only when the share is above 0.
`IsLookingAt` takes the looking test's steps in the original's order: the sums z first, the unit vectors by the
lengths' reciprocals, the distance as a float checked after, and the compare with the cosine in double (it stands for
the FPU's unrounded `fcos`; the floats on either side of it give the same answers). It is tested at the angle's
edge, at the range's last float and on two edge points where the plain glm order gives the other answer. The count is
the mind file's field before the attitude (`MindFileData::turnsLookedAt`), loaded with the mind and written back on
save. The creature debug window's Desires section shows "Looked at n of m turns, attention k of 4". It replaced an
invented five-minute fade of `secondsAlone`.

**Loneliness** (desire source 36): the same turn out of the hand counts `CreatureMindState::turnsNotLookedAt`, back to
none on a turn looked at (`creature_attention::NoteTurn`), and the source reads
`creature_attention::Loneliness(turnsNotLookedAt, MsPerTurn())`: the whole seconds (`WholeSeconds`, none where the
original's division would fail) times a sixtieth as a float, at most 1. It replaced the seconds since the last stroke
or slap, which the original's source does not read. The debug window's Desires section shows "Not looked at for n
turns, loneliness v". Only the local player's creature is ever looked at in openblack (see [Pending](#pending)).

**Lack of interaction** (desire source 37): the turn out of the hand also counts
`CreatureMindState::turnsSinceMadeToAct`, which `LeashSystem::ActOn` (the leash's act on a thing, openblack's
`ForceActOnObject`) sets back to none; the source reads `creature_attention::LackOfInteraction`, a hundred and
twentieth of the whole seconds as a float, at most 1. `LearnFromFeedback` sets it back to none too on the path of
the lessons (`FeedbackPath::Lessons`, the original's 0x4E0FC0), whether or not the desire can be taught; a stroke or
slap that ends before (too slight, eating what it holds, forgiven for running away, nothing to credit, too young)
leaves it, and nothing resets the turns not looked at (`secondsAlone`, which every feedback reset, is gone). The debug
window shows "Sent to act n turns ago, lack of interaction v".

### The creature help in openblack

Ported from raffclar's creature help (unverified against v1.42). The good advisor tells the player about the local
player's creature. The rules are pure functions in `creature_help` (`src/Creature/CreatureHelp.h`):

- 42 kinds of message, each with a help level, a first text (the message's parameter is added to it), how it is said
  (pointing at the creature, text only, unsaid, or a stage's lesson script), and whether it needs the creature in
  view, waits when it can't be told, and keeps only one waiting;
- `Decide`: dropped inside the temple or for another player's creature; below stage 13 or with the help off, only the
  stage messages pass; while the creature is out or asleep only forced ones; held while a script holds the dialogue,
  the hand holds the creature, a message was told this turn, a script controls it or it is in a lesson script (a held
  message waits if its kind stacks); told again only after 60 s, 5 s for what it wants.
- **The lesson the feedback taught** ✅ v1.42 (2026-10-21): after the four lessons, fn_004E0FC0 (skipped whole when
  the context's desire row +0x38 is 0) sends help type 4 (`LessonIncreaseOpinion`) for f > 0 or 5 for f ≤ 0 (`fcomp 0;
  test ah, 0x41`, 0x4E1024..0x4E103D) with the context's desire (+0x10) when any lesson returned 1, else type 0 or 1
  (`LessonIncrease/DecreaseSource`) with the context's source (+0x58); then mind +0x18C54 = 0.
  `LearnParticularLessonFromFeedback` 0x4E11C0 returns 1 only for lessons 1 (what it was for) and 2 (what it was done
  to): fn_004E14C0 returns the leaf the belief reaches in the desire's own tree once the example is added and the tree
  grown (fn_004D4BD0 → fn_004D50F0), and the lesson returns 1 when that leaf's +0xC is set (0x4E144B..0x4E1456); +0xC
  is the node's parent (0 for the root, `fn_004D4D30` at 0x4D4DB1; a child's from its third argument, `fn_004D4DF0` at
  0x4D4E3D, called by the split fn_004D6A50 with the node split). So the opinion message is told when either thing
  taught sits below its tree's root. openblack: `creature_help::OpinionChanged` of the goal and act-on leaves
  (`creature_tree::LeafOf`, the root at place 0), each only when that lesson ran; `FeedbackLesson`.

`CreatureHelpSystem` (`Locator::creatureHelpSystem`) keeps each creature's memory as a `CreatureHelpMemory`
component, not saved. Once a turn, after the creatures (`Game::GameLogicLoop`), it raises the first waiting message
again, tells of a body problem at most every 120 s (the species' thresholds in `GCreatureInfo`), what the creature
wants once the hand has hovered over it long enough, and one of 12 prompts on how to look after it every 120 s. A
message is said by starting the challenge script's `HelpForCreaturePointAtCreature` or `HelpForCreatureJustText`
with the text's number pushed, without stopping other help scripts.

The stage messages (12 to 25) start their lesson scripts (the tell functions 0x4CAC40..0x4CAE73), each setting the
in-lesson flag Creature +0x10C0; the first stage's (12, 0x4CAC30) does nothing. The last, fully grown (0x4CAE80),
starts "TheStorm" and sets the flag only on the first land (`g_game+0x205A08` = 1); on any other land it pushes
4095.0f (0x457FF000) twice and starts "HelpForCreatureMoveToNewDevelopmentPhase" (0x9DD1EC), leaving the flag
alone. openblack: `creature_help::StageScript`, `StagePushed` and `StageStartsLesson` (until 2026-10-18 the last
stage off the first land started nothing).

The kinds that need the creature in view have the table's condition (row +0x18) set to 0x4775D0, the same looking
test as the mind scroll's attention: `creature_help::LooksAt` is `creature_attention::IsLookingAt` (since 2026-10-18;
before it was its own float version, which differed right by the cone's edge).

The hand-over reading (Creature +0x1270 = over (fn_004777A0) × 0.1 + itself × 0.9, `ProcessState` 0x473B0E..0x473B38)
is taken every turn outside the script-controlled block 0x4735C7..0x473633 that holds the help's own turn (the waiting
message 0x4CA670 at 0x473621, `UpdateHelp` 0x4C9C90 at 0x473633): no jump of 0x473380..0x473B48 leaves past it.
openblack follows it (`FollowHandOver`) also for a creature a script controls or with no desires yet (since 2026-10-18;
before, the reading stood still then).

Telling what the creature wants (CurrentDesire, 41) and something interesting off screen (36) share the tell 0x4CAA80:
it takes the local player's creature's newest action context (mind +0x2AD8 → 0x4F25C0, the newest of the five) and sets
its +0x3C, "seen by the player", to 1 before it starts `HelpForCreatureJustText` with 624 + the parameter; then, out of
multiplayer, mind +0x1C2C = 1 (unnamed, not ported). A context is made with +0x3C = 0 (`CreatureContext`'s constructor,
0x4E1C3C) and its finish (fn_00475450, 0x4756DD..0x4756FE) only ever sets it, when the camera looks at the creature.
openblack: `creature_learning::ShownToPlayer` from the help's tell, and `Finished` keeps a seen context seen (since
2026-10-18; before, the tell marked nothing and the finish overwrote the mark).

Raised by: a skill or miracle seen (`SeeSkill`; `WatchMiracle`, so also a miracle seen cast in play), the creature
fainting (`Fainting`, before the faint is cleared), and the hand offering it something it can't pick up (MiscellaneousStacked
7, "too big to handle", text 4322; `HandSystem` giving, as `InterfaceValidToGiveObject`'s refusal in
[picking.md](../hand/picking.md)). The creature help option and SET_CREATURE_HELP switch it.

Kept as the original has them (vanilla, not deviations): the "very" body texts are only heard on fainting (the plain
one is tested first and always holds too); the mental problems are made but unsaid; "knocked out" is never heard (it
is raised only once the creature lies dying, which holds it back); "can't learn it yet" is never heard.

### The player's attention in openblack

Raffclar's creature-attention-actions lane (his PR #513, with the pieces of #354, #420, #432, #505 and #511 it stands
on), unverified against v1.42. Eleven actions with the player are plan actions (`creature_plan_actions::Build::GoToHand`
to `BeSillyWithPlayer`); their rules and the per-turn steps are pure, in `creature_player_attention`
(`CreaturePlayerAttention.h`), and their agendas in `CreaturePlayerAgendas.h`:

- **How interesting the player is** (`creature_desire_sources::PlayerInterest`): each turn the local player's left hand
  adds the distance it moved, up to 60, fading by 0.98 a turn, and what the hand holds adds its radius across the land
  over 30; more interest is taken at once, less halves the way to it each turn. Off the land the hand counts where it
  last was. No desire reads the value yet.
- **The hand's speed** (`creature_player_attention::HandSpeed`): each turn's movement of the hand, made a speed, is
  followed by six tenths. The hand moves while the speed is above 0, which lasts long after it stops: the original
  (`GInterfaceStatus::Process` → fn_005DBC60, 0x5DBD4E..0x5DBED6) takes the length with the FPU's own `fsqrt` over the
  sum of squares, whose range never lets a square go to 0, and stores it to +0x100, which `IsHandMoving` 0x4D2D20
  compares with 0; so the hand moves while any part of the speed is not 0, the smallest float included. Run in the
  oracle (moved (1,0,0) at 100 ms a turn, then still) the speed is 6.0, 0x40199999, ..., the bits 1 on the 115th
  still turn and 0 on the 116th. openblack compares the speed with 0 itself (until 2026-10-18 it took a float length,
  whose square went to 0 some 57 turns sooner).
- **The rules** (`Possible`): looking at the hand while the hand moves (no camera needed); going to the hand when the
  hand is further along the ground than an eighth of the camera's view width at the focus (tan 0.875 × the focus
  distance / 8); being cross with an attitude to the player below −0.1; following the player and showing the player a
  thing when the camera does not look at the creature (`creature_attention::IsLookingAt`); going to the middle of the
  screen when it does (vanilla bug kept, see
  [creature vanilla bugs](../project/progress/vanilla_bugs/creature.md)). The situation they read is filled by
  `SituatePlayer` (CreatureMindLearning.cpp) after `SeeCamera`, for the local player's creature only.
- **Looking at the hand** (`creature_mind::LookAtHand`, `Step::Kind::WatchHand`): one watch, curious, the head on the
  nearest hand, for whole seconds above those of 15 turns, five at most; it turns to the hand more than a quarter turn
  away and is not done while it turns. It replaced our turn to the camera with the look-at-me clip. Each turn, once
  the creature has a player, it takes one `GameRand(30)` (ours, from the exe, 2026-10-18): the sub-state
  `LookAtHand` (0x12 in the sub-state table 0xBE0B98, run at 0x502C70) passes `GameRand(30) + 10` seconds and a range
  of 40 to the shared turn-to-face helper 0x502E70 (no turn within 0.1 on the ground; a turn when more than pi/2 off,
  0x502F38) and ignores what the helper returns; its own end test follows (not moving, whole seconds above
  min(whole seconds of +0x58, 5.0), 0x502CE6..0x502D53).
  The order (2026-10-19): the stage first asks for the player (vt+0x1C, 0x502C79; none: done, 0x502C81, no draw),
  then reads the nearest hand's status (0x47D740, 0x502C8D) with no test for a missing one and copies its last point
  (+0xC8..+0xD0), then draws (0x502CBB), all before its own tests. Ours draws first under `Senses::hasPlayer` and ends
  the watch at once without a player. The point (ours, from the exe, 2026-10-19): the status's +0xC8..+0xD0 is the
  hand's last reported place. `SetToZero` 0x5DBA00 clears it with +0xA4 (0x5DBA42..0x5DBA5A); the hand's packet
  0x15, sent by fn_005D2250 with the hand's `CHand` +0x78 (0x5D2350), is stored at +0xA4 by fn_005DBFB0 (0x5DBFB7);
  every `GInterfaceStatus::Process` 0x5DC4E0 runs fn_005DBC60 first, which copies +0xA4 into +0xC8 with no test
  (0x5DBF1F..0x5DBF2C); `Load` 0x5DCDC0 reads both. No other `GInterfaceStatus` function writes it, so it keeps
  the hand's last place while no new one comes, and is (0, 0, 0) before the first. `GetNearestHandInterfaceStatus`
  0x47D740 picks the player's status nearest the creature by that same point (0x47D7A9..0x47D7BD). Ours keeps
  `CreatureMindState::lastHandPoint` (`creature_player_attention::LastHandPoint`, from `k_HandPointBeforeAny` =
  (0, 0, 0)), updated every turn from the hand nearest the creature, so a creature with a player keeps watching where
  the hand last was while the hand has no place on the land (the land's corner before it was ever placed). Until
  then a turn with a player but no hand point ended the watch.
- **Following the hand** (`Step::Kind::FollowHand`, `FollowHandTurn`): thirty seconds of turns from the turn after its
  start; standing, it walks to the hand until within its radius and 4; while its body is busy it walks again
  only when the hand is not ahead, by the game's straight angle difference (`IsDirectionClose`, vanilla bug kept). A
  hand off the map gives the agenda up.
- **Following the camera** (`Step::Kind::FollowCamera`, `creature_emote::FollowCameraTurn`): thirty seconds of turns,
  arriving within twice one more than its height, walking to two short of that; moving, it sets off again only when the
  focus is not within a sixth of a turn of the body's way (`creature_look::k_BodyViewParts`) nor in its own cell. The
  height is set for the four actions that follow the camera as their agenda is made (`FollowsTheCamera`).
- **A busy body** in both follows is the original's `LH3DCreature::IsPerformingBodyAction` 0x4842D0 (the 3D state
  +0x4994 not 0: walking, state 1, or any action), read by the hand's handler 0x505FA0 at 0x50602F and the camera's
  0x503C60 at 0x503D0E: while it is busy only the direction is weighed, and the distance ends the follow only once the
  body is free. openblack passes the mind's `Senses::bodyBusy` to both, the same test as the computer players'
  (`creature_body::IsPerformingAction`): walking or turning, playing an action, playing the animations on a thing (a
  pick-up, knock-down, throw, kick or point, `CreatureObjectActionSystemInterface::GetProgress`) or catching; a fighter's
  states never reach the mind, which leaves a fighter alone. Until 2026-10-18 the hand's follow read only an action
  played and the camera's only walking; until 2026-10-19 the actions on things were left out, so a step could start
  while the hands' animation still played, where the original's starts refuse while the body is busy (bw1-decomp
  `Creature3D.cpp`: `StartBlendedAction`, `StartTurningAction`, `StartFighting`, `ThrowAtPointGivenTime`,
  `StartPickUpFromHandAction`) and the blended pick-up or knock-down (state 0xE, 0x10), the throw (0xD), the kick (0x12)
  and the point (8, 9) keep the state until their clip ends (`StateSwitch`). Test
  `CreatureMindSystemTest.ANextStepWaitsWhileTheHandsStillActOnAThing`.
  The watch of the player (`Step::Kind::WatchPlayer`, `WatchPlayerTurn`) reads the same test: its sub-state 0x7C's
  turn handler is 0x508250 (the row written into the sub-state table at 0x4FE106); after its two "still waiting for
  orders" tests (+0x10E8, vt+0x1C; done, 2, when either fails) it calls `IsPerformingBodyAction` on the 3D creature
  (physical +0x160 → +0x58) at 0x50827A and, while that is true, only waits (returns 0 at 0x5084B9) before it weighs
  the hand or the camera. openblack passes `Senses::bodyBusy` there too (until 2026-10-19 only the body layer's
  action, `creature_layers::IsPlaying`). The watch is reached only while the creature waits for orders, which
  openblack has not yet (see Pending), so this changes nothing in play today.
- **The camera's follow off the map**: 0x503C60 makes the focus (the camera status +0xBC) a `MapCoords` (0x603160) and
  returns 1, the fail path (0x4FF18E → 0x475730), when it is not `InBounds` (0x6042C0, 0x503CB4), before it looks at
  the body; its reach is weighed with `GUtils::GetDistance` 0x74CCB0 → `ConvertWholeDistanceToMeters` 0x74DCC0 from
  the creature's map point (+0x14), as the hand's follow. openblack: `FollowCameraAction::Fail` gives the agenda up
  as the hand's follow does, and the distance is `gutils::GetDistanceInMetres` (until 2026-10-18 a float length and no
  map test). The origin test (the focus's map x, z and altitude all 0) is still taken on the float point.
- **Being silly**: four rounds of a turn to the camera and talk with fluttering eyelids (`Step::flutterEyelids`, a
  1250 ms blink until the next plan), the playful clip three times in four, a laugh and talk one time in two, and one
  of the ten faces for five seconds (`Step::Kind::Face`).
- **The thing thrown or shown**: throwing at the camera and showing the player a thing first take up a thing it may pick
  up (`Executor::use = Target::Pickable`, his #419's way of choosing it, see [the kind
  actions](plans-and-needs.md#the-kind-actions-in-openblack)), or the thing a script names. With its hand full it uses
  what it holds.
### A lost thing in openblack

`CreatureMindSystem::LoseGoneThings`, each turn just before the creature is free to choose (raffclar's PR #508, unverified
against v1.42): the things of its plan (while a step of the plan is still to come) and of its steps from the current
one (`creature_lost_thing`); one of them gone (no longer in the registry), the creature gives up its plan and agenda
and decides again, a frightened creature wants to flee least of all, and the advisor tells the player that it lost the
object it was looking for (FailureExplanation with `creature_help::k_LostThing`, 8, as the game's type 0x28 with 8).
What it is eating does not count: it goes as the meal ends. Testbed `mind.lost_thing`.

From raffclar's creature help 2 (unverified against v1.42):
- **Held by the hand.** Taking hold of the creature (`CreatureHandSystem::Grab` → `HandTookHold`) turns it to the
  player's eye. ✅ v1.42 (2026-10-21): `GoIntoStateOfFinishingAction` 0x477B20 (from
  `NetworkFriendlyStartLockedSelect`) takes the way from its drawn body (+0x78) to the nearest camera interface's
  point (+0xB0), each a float subtraction; when |dx| and |dz| are both 0.02 or less (a double, 0x477B99 / 0x477BAC)
  the heading is 0, else `heading_from_direction_vector` 0x7FAA50 stored as a float; then the body's velocity (+0x48C0)
  is zeroed and `EndAnyActionsThenTurnToHeading` 0x48F7B0 runs: with no body action (+0x4994 0) it is
  `StartTurningAction` at once, else `EndAnyActions(1, 1)` and the turn kept for after it (+0x523C = 1, the heading
  at +0x5240). openblack: `creature_locomotion::HeldTurnHeading` (from the drawn pose, `creature_pose::BetweenTurns`,
  else the Transform, to the camera's origin; `affine::GetYAngle`, our heading its minus) and
  `CreatureLocomotionSystemInterface::TurnToHeading` (stop, then a turn on the spot with no near-facing snap; a body
  playing an action ends its loop and the turn waits in `CreatureLocomotion::turnAfterAction`). Pinned to the
  oracle's 0x7FAA50 results: `CreatureLocomotion.AHeldCreatureTurnsTowardsTheEyeAsTheOriginal`. The vt+0x1C both
  tests call (0x477B28, 0x477B38) is `Creature::GetPlayer` 0x473F20 (the Creature vtable 0x8CC810 + 0x1C; it answers
  +0x1070, the creature's player): a creature of no player is neither turned nor marked (+0x3CC stays 0), and the
  second test, the same call, cannot fail after the first, so its heading-0 branch (0x477BCC) is never taken. Ours is
  the same: the hand takes hold of no creature that belongs to nobody (`creature_hand::MayHold`, before `Grab` calls
  `HandTookHold`). What the body ends first: `EndAnyActions(1, 1)` 0x48F710 calls `EndDanceAction` 0x4848F0,
  `EndFighting` 0x4845F0, `EndStaticAction` 0x4848C0 and `StopPointing` 0x484AE0 (state 8, pointing, to 9, its end),
  then `StopMoving` 0x484260. Ours: `TurnToHeading` ends a dance (`creature_layers::EndDance`: the moves stop and the
  finish plays) and a static action's loop (`EndLoop`: its end plays), and `HandTookHold` first calls
  `CreatureObjectActionSystemInterface::StopPointing` (a pointing that plays ends its hold; one still turning to its
  point goes on, as state 8 is not reached yet); the stop is the locomotion's. Tests:
  `CreatureLocomotionSystemTest.TurnToHeadingEndsADanceFirstAndTurnsOnceItIsOver`,
  `CreatureObjectActionSystemTest.StopPointingEndsAPointingThatPlaysAndLeavesOneUnderWay`.
  **A fighting creature.** None of the creature's own hold checks looks at a fight: `ValidForLockedSelectProcess`
  0x476E10 (vt+0x6CC) refuses species row +0x1F4 = 13, a creature under a script (+0x25 & 4), one in the first
  sub-actions (+0xFB4 ≤ 2) of the faint's forced plan (mind +0xF60 = 9), one of no player and one under spell 0;
  `IsCannotBePickedUp` 0x401A10 is the flag 0x2000 (scripts, files, puzzles), and fn_005DA950 tests
  `IsInteractable` (vt+0x190) and Flags & 0x10. So the hand takes hold of a fighting creature whenever the interface
  is not in its fight state (`ResetActionState` 0x5D29C0 sets state 0x10 only with a `CameraModeNew3` that `HasFight`
  (+0x44), g_game +0x250538 set, its player's creature `IsFighting` and an empty hand; see [Pending](#pending)).
  `EndFighting` 0x4845F0 then always clears the body's opponent (+0x528C = 0), and only in fight states 0x16, 0x17,
  0x18, 0x1A, 0x1B, 0x1D, 0x20, 0x21, 0x22 (start, stance, action, block start, block, block end, cast start, cast,
  cast end) tells a `CameraModeNew3` `EndFightSoon(1)`, takes (1 − fight health +0x4AA8) × `FightLifeLossScale`
  (0x8CF080, 0.25) off its life (vt+0x11C, vt+0x5B0) and asks for the finish (+0x5238 = 1); its opponent is not
  touched. openblack: `CreatureFightSystemInterface::EndOwnFight` (an empty default; `CallOff` ends both sides), in a
  duel and only in those states (`creature_fight::CameraLingersOnEnd`): the camera told, the life loss, its move
  finished; `CreatureFightTurnsTest.EndOwnFightEndsOnlyItsSideAndOnlyInTheMidstOfTheFight`. The cleared opponent
  outside those states has no counterpart in ours (our duel keeps its two sides). While held its mind stands still each turn after the turn's clocks; once its body is free (not
  waiting for the turn to end: ours, from the exe, see below) it plays the show action of the desire behind what it did lately that feedback would be credited
  to, else its active plan's (`creature_help::DesireShownOnHold`), and the help is told CurrentDesire, forced. The
  dominance cheat's step is skipped with the rest of the mind. Holds made by the testbed's commands do not call it.
- **Feedback lessons.** Too young to learn (or an action the tables don't know): Miscellaneous 4 stroked, 5 slapped.
  On the lessons path: the opinion lesson by desire when the lesson split what it knows of such things (what it was
  for reaches a leaf below the root of its goal tree, or what it was done to of its act-on tree, each when taught;
  `creature_help::OpinionChanged` of `creature_tree::LeafOf`, ours from the exe, see "The lesson the feedback taught"
  above), else the source lesson by the context's source (`creature_learning::PlanSource`, kept in
  `Context::source`). The too-young messages and the lesson messages are checked against v1.42 (2026-10-21).
- **Spells and fumbles.** A spell taking hold or wearing off (by its place in the game's spell table); a fumbled
  miracle (MessedUpAction 1).

### The things it has come across in openblack

- **What is kept:** only whether a thing is new to the creature, by the thing's kind, with the original's pool sizes
  (`creature_object_beliefs::Store`, `k_Capacity`, the oldest-place marker that a full kind's newest belief replaces,
  vb#25 kept). What it thinks of a thing is still read from the world when needed (`BeliefOf`), so the store has no
  attributes and no place, and no second belief system comes in. Ported from raffclar PR #359 (396845baa), unverified
  against v1.42; the beliefs359 plan's B1, brought in with the creature reactions lane.
- **The sweep:** each turn, after the exploration's visit, `CreatureMindSystem::SweepEyes` takes the next cells of the
  square about the creature (`creature_look::SweepCells`, a ninth a turn, at most nine, ring after ring from its own
  cell), passes over those it can't see (`creature_perceived_desires::CanSeePos` towards the cell's corner at the least
  x and z), and comes across every thing in the rest, then the thing it holds. It runs under a script's control too, as
  `Creature::Look` 0x4D0700 does, and while the hand holds the creature (his tree skips that; the original's guard is
  only the perception flag).
- **A new thing** leads it on to what it belongs to (`BelongsTo`: a villager's, abode's or field's town, an animal's
  flock); a tree's forest is not followed, as our forests are no entities.
- **The kinds** (`mind_detail::BeliefKindOf`) are the original's by class ([the kind of belief each class
  gives](#the-kind-of-belief-each-class-gives)): a big forest, a street lantern, a shield, a script's highlight and the
  singing stone's base are of no kind of their own (21), a spell icon its own (17); a static that can be moved is a rock
  (12) when it is of the rock class (`object_physics::IsRock`: a rock, a bonfire, a fragment), else a mobile
  object (19), as a dead tree, a magic teleport and a vortex are; a loose thing of the mobile objects' class (a mobile
  object, a ball, a one-shot globe, a reward chest, the whale) by its row's kind
  (`creature_object_beliefs::MobileObjectBeliefKind`): magic food 14, poo 13, any other 19. Since 2026-10-21; before
  that the lanterns, shields, highlights and spell icons were kept no belief about, a big forest was a forest and a
  rock, poo or magic food a mobile object.
- **Who reads it:** the reactions to another creature keep the turn it last looked the other over (`Belief::inspected`).
- **The last moving thing seen** ([the original](#the-last-moving-thing-seen)): `ComeAcross` keeps it first, before the
  thing's kind is asked, in `CreatureMindState::movingSeen` (`creature_object_beliefs::MovingSeen`,
  `KeepsMovingThing`): its place on the map's grid, `ecs::object::GetHeight`, the turn and the weight 0.5. The walk's
  second look (`creature_mind::LookWhileGoing`, `MovingSeenRecently`) takes it, at the ground there plus its height,
  and forgets it (weight 0) when it is two seconds old. What moves: a creature on its way or turning (its locomotion),
  a villager or an animal not where its turn started (`villager_teleport::IsMoving`), the whale not where its turn
  started (`Shark::turnStart`), never a town, a flock, a forest, a spell or mist, and any other thing when it is not
  where it was last put (`components::PlacedAt`, `ecs::MovedSincePlaced`): the registry puts every thing where its
  position is first given (the constructor's write), and SET_POSITION's move of an object and the end of a flight
  (`DynamicsSystem::EndPhysicsAsObject`, and the dead tree a falling tree becomes) put it again (`ecs::PlaceWhereItIs`).
  Since 2026-10-21.
- **A thing seen again** stirs desire sources, as the common `CreatureBelief::UpdateAttributes` 0x4D7E80 does after it
  refreshes the attributes. The known path of `CreatureBeliefList::AddBeliefAboutObject` 0x4D7CE0 calls the belief's
  vt +0x28 (a new belief does not); every belief class reaches 0x4D7E80 (the small, flock, context, citadel, forest
  and mobile-object beliefs through the vtable, the town 0x4D7FD4, villager 0x4D8084 and creature 0x4D8241 beliefs by a
  call) but the abode's 0x4D7F80, which keeps its first sight and stirs nothing. Then, in order:
  - a thing whose vt +0x1DC `IsSuitableForCreatureActivity` answers true (Town 0x739260, Forest 0x539AA0, Citadel
    0x462A90, Flock 0x52F8C0 and Creature 0x473FD0 return 1; the base 0x401AD0 returns 0), that is not the creature
    itself (belief +0x30 against the creature argument), when the game turn (`g_game` +0x205A40) less belief +0x3C is
    above 200 (`cmp ecx, 0xc8; jbe`, unsigned): +0x3C takes the turn and `CreatureMental::UpdateSourcesFromBelief`
    0x4DE800 adds 0.5 to each source of the three at 0xBDF554 (1, 3, 7: impress, compassion and anger from seeing
    things), whose desire (the source table, stride 0xA4, +0x10) finds the thing more than 0.1 useful
    (`GetActivityObjectUsefulness` 0x4E9900, `fcomp [0x8CFFBC]` = 0.1, `test ah, 0x41`);
  - a thing whose vt +0x3B4 `IsToy` answers true (a ball 0x4363B0; a static 0x609020 of a model 399 to 403) stirs
    source 12 (to play from watching the player) by 0.5, at every sighting;
  - a thing whose vt +0x394 answers true (can be stolen) stirs source 60 by 0.3 (not in ours yet: the steal lane).
  The belief constructor 0x4D7810 sets +0x3C to 0.
  Ours: `CreatureMindSystem::StirSourcesOnSighting`, called by `ComeAcross` for a thing it knows, with the pure rules in
  `creature_object_beliefs` (`SeeingAgainStirs`, `InListOfWholeThings`, `SightingStirsSources`, `UsefulEnoughToStir`,
  `Belief::sourcesTurn`) and `creature_desires::DesireOfSource`. The turn is `game_clock::Turn()`. The usefulness is
  `ActivityUsefulness`: the creature's ActOn tree for the desire, nothing outside the area it is kept within; the
  original's other gates of 0x4E9900 (a social desire's object with no associated belief, the leash's tied object, a
  friend for anger) are not in ours. The desire of a source is the first desire that grows from it (raffclar's
  `DesireOfSource`), as the source table's +0x10 is filled at run time (0 in the file image) and our info layout of it
  is 0x94 bytes, not 0xA4. Ported from raffclar PR #518 (5fc9122a8), behaviour from the exe; the beliefs359 plan's B2.
- The creature mind panel has a Beliefs readout (counts per kind, the sweep's ring and cells left, and the sources that
  sightings stir).

### Reactions to another creature, a flying thing and the hand in openblack

Ported from raffclar PR #432 (692fdd9cd) with the slice of his #417 (9bb3a2a64) creature reaction setups for the three
kinds, unverified against v1.42. His reaction system is not taken: the rules are in our `creature_reaction_rules`
(his `creature_reaction`) and run by our handler (`CreatureReactions.cpp`).

- **Who is free to react** (`IsAvailableForReaction`, the original `Creature::IsAvailableForReaction` 0x4F2820, xref
  note #495): available, reactions on (+0x1060, `CreatureReactionsOff`, set by `CREATURE_REACTION` 281), not under a
  script's control (+0x24 bit 0x400), not planning to fight nor its opponent (mind +0xF60 == 0x15, for itself and for
  +0x1BE8), not fighting, not out cold, and a mimic only above priority 150. Not modelled: the dance editor, +0x24 bits
  0x80 and 0x10, +0x1C14 with +0x380 (his reading: the leash's order, which our leash does not have, port audit
  N-R-13), +0x384, +0x10AC, +0x10B0, mind +0x1C10, +0x10F4. The same test gates the catch (vt+0x984 from 0x47C98E), so
  a scripted creature no longer catches either.
- **Another creature (6):** urgency 255 once due a look over: not its own reaction, the other is a creature and still
  there, it has no opponent and its plan does not act on that creature already, and it never looked it over or not for
  more than `GCreatureInfo::inspectCreatureSeconds` (+0x2CC) of turns. Weighing a new reaction notes the turn as it
  weighs it, as his does; the reaction it already follows is weighed again without. Taken up, it plans
  `InspectCreature` (go near at four heights, face 0.1 s, watch 5 s plus up to 10 s, curious) when the other is of some
  use to act on for curiosity (`ObjectToActOnUsefulness`: what it has learnt of acting on such things times
  `DistancePriority` times the desire's `ActOnFactor`, nothing outside its kept area but a temple part for hanging
  around at home; see [plans-and-needs.md](plans-and-needs.md#the-plans-priority)). Every creature makes this
  reaction as it is made (`CreatureArchetype::Create`, through `effects::reactions::CreateReactionBeforeFiling`: its
  player's, not started, spread over 55 m from where it stands before it is filed in its cell, so it does not reach
  itself), 2026-10-20.
- **A flying thing (9):** the table's urgency when its player is human, it is past 899 turns after the last flying
  thing it took up (`CreatureReaction::sawFlying`), not catching, the thing was thrown from a hand and is still really
  in the air (`ecs::ActuallyInTheAir`, shared with the villagers), and it sees it; another player's only on a draw of 0
  to 2 of 100, unless that player is allied to the creature's (`ReactToFlyingObjectPriority` 0x4F34DA,
  `creature_reaction_rules::IsOthersPlayer`), when no draw is made. Taken up, it notes the turn; still up, it points at it 2 s half the time, amazed, then watches it while
  it flies, two seconds and a draw of up to two more each turn; one already down is not kept.
- **The hand picking something up (16):** three times the table's urgency, cut to a byte, for its own player's hand
  holding something; taken up, it watches the hand (LookAtHand) for a minute and up to another, at most five seconds
  once still. Our hand makes no such reaction yet, so it is not reached.
- **LookAtHand (8)** is the original's single watch (`ConstructSubActionsForLookAtHand` 0x49BA10), from lane513.
- Testbed: `creature.inspect_creature` (shows nothing until the creature reaction is made) and
  `creature.look_at_flying`. The creature mind panel has a Reactions node with a "Reactions off" switch.

### Beliefs about other creatures and buildings in openblack

Ported from raffclar's creature beliefs rest lane, unverified against v1.42. `mind_detail::BeliefOf` gives another
creature the miracles it knows (`creature_tree::SpellKnowledgeValue`), the kind of thing it carries (the held thing's
type, `CarryingValue`, 29 for nothing) and the desire shown by the action it carries out now
(`DominantDesireValue` on the action table's `shownDesire`, memory 0xC4 above): the original reads the mind's current
action (+0xF60, 0x4D3C11), its plan's while one runs, the idle activity's own row during an idle activity, and 0 once a
plan has ended (`StopWhatIAmDoing` → `CreatureAgenda::Clear` 0x475A86). Ours: `mind_detail::HeldPlan`'s action, none
giving 0; it no longer falls back to the last plan's action. A villager or a building is alive above a fifth of its life (`LifeValue`), and a building
says whether it is built (`BeingBuiltValue`). The caps (40, 42, 44) are his reading. As an action completes, the
creature's attitude to the creature the plan acted on moves by `creatureNicenessChange`, to itself for an action on no
thing (the completion in `CreatureMindLearning.cpp`, through `mind.leash.attitudes`).

Not taken from his lane: choosing the thing to use from the beliefs, newest first, and testing the kept area against
where the creature believes a thing is (our beliefs keep no place); his inspector queries.

### Test hooks

- The testbed's `mind.go_to_hand`, `mind.point_at_hand`, `mind.follow_player`, `mind.go_to_middle_of_screen`,
  `mind.be_cross`, `mind.show_how_nice`, `mind.pray_to_player`, `mind.be_silly`, `mind.throw_at_camera` and
  `mind.show_player_object` give the actions through a script's CREATURE_DO_ACTION (the last two with a toy ball).
- The debug spawner reports sightings to `SeeMiracle`, the only path that does today.
- Debug > Consciences > Creature help (closed by default): the switch, the last 16 messages told, the player's
  creature's waiting messages and hand-over reading, and a button that raises any kind of message.
- Testbed `advisors.creature_help`: a grown-up tiger shown the fireball every six seconds, 38 times (nearly learnt at
  the 21st, learnt at the 27th, told again a minute later).
- `CreatureFrightening.OnlyFearListsRunningAwayFromAnObject` (skips without the game's data).

## Pending

- **The hand during a creature's fight** (2026-10-21): in the original, while its player watches its creature's
  fight (the interface's fight state 0x10, see "Held by the hand" above) an empty hand's presses go to the fight
  gestures (sub_5D4AC0), so it cannot take hold of the creature; openblack has no interface fight state (the fight
  gestures are not ported, [gestures.md](../magic/gestures.md)), so its hand can. To port with the fight gestures.
- **Creature help, the prompts' flags** [V]: the 12 prompts known are creature +0x198 + 4·i in the order of
  `creature_help::Prompt` (the told turns at +0x1C8 + 4·i): `Creature::UpdateHelp` walks them from 0x4C9EF8 with the
  condition table at 0xC82858 (16 bytes a row) and sends help type 0x1C (Tutorial) with the prompt's index. The leash's
  thing order (`Creature::ForceActOnObject`) sets +0x1C0, Drink, before it forces Hunger's action 0x9B (0x4C6134..0x4C615B;
  raffclar's vanilla bug 94: a fishing order marks the drink prompt, not a fishing one). The ground order
  (`ForceActOnMapCoord`) sets +0x19C Run (0x4C6FDB), +0x1B4 DropAtFeet (0x4C6C01) and +0x1C0 Drink (0x4C6DDC) on its
  paths. These wait for openblack's leash orders.
- **Creature help**: nothing raises the stage messages yet (which code raises them in the original is not found);
  the leash's ground and thing orders, which mark the prompts known and tell what it wants or why it can't get there,
  wait for the leash orders; `Decide` does not read each kind's help level (as in raffclar's port); where the original
  runs the help's turn is not settled (the creature help's source in the decompilation is empty, and only its
  constructors are known).
- **A lost thing**: the sub-actions' vtable +0x2C test (raffclar read it as "not there, as a dying villager") is not read and not ported; the per-desire plans
  are not zeroed (ours drops a kept plan whose thing is gone when it next plans).
- **The other players' cameras** ([The attention its god pays it](#the-attention-its-god-pays-it)): openblack keeps
  only the local player's camera, so a computer player's creature is never looked at; the original looks at it with
  its own player's still camera above its citadel (and learning's "seen" asks the same cameras). A port row: each
  player's interface camera, set as its citadel is made.
- **Sent to act** ([The attention its god pays it](#the-attention-its-god-pays-it)): the counter is zeroed wherever
  openblack's leash acts (`ActOn`), whose own tests are still `ForceActOnObject`'s queued fix
  ([the leash's Pending](leash.md#pending)); sent to a point of the land (`ForceActOnMapCoord`, 0x4C6B11) it is
  zeroed on the walk and, through `ActOn`, on a field; the orders not ported yet (the sea, sleep, what it holds)
  zero nothing ([Sent to a point of the land](leash.md#sent-to-a-point-of-the-land)).
- **The attention counters across a land change**: the original's `CreatureAttitudeToPlayer` constructor 0x4C8130
  (from the `CreatureMental` constructor, 0x4D23BE) zeroes +0x18C48..+0x18C54; whether a land change makes the mind
  again or keeps it is not read here. openblack keeps them with `CreatureMindState`.
- **Saving a mind**: openblack differs: the cursed size is divided by 15, the original multiplies by 0x3D888889
  (0x4E8F00); they differ by one ulp in about 63 % of heights (oracle on `Creature::SetHeight` 0x47DE80: 192 of 307)
  (`mind_detail::CurseValuesToSave`), a fix row. The `ScriptCreatureCurse` record (`Init`, `ResolveLoad` 0x6F61E0) is
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
- **The player's attention**: showing a thing puts it down plainly, not with the put-down clip; pointing at the hand
  points at the ground under it for the usual while (the point's height and time are not read); watching the player ends
  at once, as openblack has no fifteen-second wait for orders after a leash order to a place.
- **The innate personality and the previous actions**: openblack draws neither (see
  [Random streams](../engine/random.md#random-streams)).
- **The plan's things met by the sweep** ([the original](#the-plans-things-met-by-the-sweep)): not ported, as
  openblack keeps no believed place for a thing (the store has no position: every reader takes the thing's own
  position, which is what the original's belief holds right after this correction, and not before). The four flags
  have no reader without that place; they come with the beliefs' positions.
- **The last moving thing seen**: two writers of `Object::coords` are not matched: the save loader
  (`GameOSFile::LoadInstance`, which builds things with the zeroing constructor and reads the saved coords; openblack
  puts a loaded thing where it is loaded), and whether a thing a creature holds has its `Pos` moved with the hand in
  the original (openblack moves it, so the held thing it comes across moves); neither was traced. A store to +0x2C
  through a register that the scan's patterns miss (no read of +0x14 beside it) would not have been found.
- **The things it has come across**: only the sweep and the held thing come across things; the other "seen again"
  callers of `AddBeliefAboutObject` 0x4D7BD0 (agendas, reactions, catching, leash, mimicry) come with their lanes; the
  perception flag (mind +0x20CEC) has no writer in ours; a tree's forest is not followed.
- **Reactions to another creature, a flying thing and the hand**: the hand's pick-up reaction
  (`GInterface::PlaceObjectInMagicHand` 0x5DA7DD) is not made, so the last is not reached (every creature's own
  reaction at its making, `Creature::Initialise` 0x47448F, radius 55, is made since 2026-10-20); for a creature taking kind 6, which turns the
  original uses (ours: the creature's own of the table) is not determined; the watch of another creature does not yet
  drop to act on a desire it finds (his TODO).
  On its way: a flying thing's reaction leaves out only a creature of a computer player (player type 2), as the
  original; ours accepts only a human player's (type 1, `CreatureReactions.cpp`). They differ only for a player of
  the third type, whether such a player exists to be checked first.
- **Beliefs about other creatures and animals** ([in
  openblack](#beliefs-about-other-creatures-and-buildings-in-openblack)): on its way, measured identical (each alone on
  the light checks, together on the full set with the story): "harder than me" judged by the drawn height, for creatures
  only; another creature's height believed in four bands of its drawn size; an animal believed alive only above a fifth
  of its life (ours: above none).
- **The plays with a held thing**: on its way: the plays counted by object type as a creature holds a thing up to its
  player are kept in the learning's count that the mind file saves and loads (ours count them apart, in memory only),
  with the hold's own agenda ([plans and needs](plans-and-needs.md#pending)).
