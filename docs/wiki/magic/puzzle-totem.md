# The puzzle totem

## Summary

A puzzle totem is a totem building of no town that rises out of the ground and sinks back into it, in whole steps.
Scripts make them with `CREATE` of a totem (script type 34): the fourth land's five bell towers (the Totem Puzzle)
and the two African totems of the undead village. Scripts read and set its height (`HEIGHT`) and its highest step
(`MAX_HEIGHT`), the creature pulls it up or down (RaiseTotemPole / LowerTotemPole) and the player's hand drags it.
The totem puzzle games lay out their own, which are only tapped. It is not [the village totem](village-totem.md),
the statue on a town centre (the original's `TotemStatue`, script type 40).

openblack makes it (`CREATE` of a totem), keeps its height and its highest step, which the scripts read and set,
and stands it as high as its height says. The rest is listed under [Pending](#pending).

## The original

The addresses below are those of the v1.42 exe, read with the 1.20 symbols. The class is `PuzzleTotem`, a `Totem`,
which is an `Abode`.

### CREATE of a totem

- The CHL create worker fn_006F11A0 switches on the type less 1 through the table 0x6F1A70 (read from the exe): entry
  33 (type 34, `TOTEM`) is 0x6F187C. It calls fn_006DA550 (`PuzzleTotem`'s create, puzzlegame.cpp) with the map
  coordinates (altitude 0: only types 1 and 8 keep the script's altitude, 0x6F1208..0x6F1212), the abode info of
  the sub-type (`GAbodeInfo::AbodeInfos` + sub-type × 0x1C8, not range-checked), no town, the angle, the scale, 1.0
  and 0 (built, not under construction). Type 36 (the spell dispenser) is the same call to `SpellDispenser::Create`.
- fn_006DA550: `Base` new of 0xFC bytes, which comes zeroed (fn_00436870's `rep stosd`); the `Totem` constructor
  0x737390 (the `Abode` constructor: built 1.0; then +0xC4..+0xCC zeroed and the position +0x14..+0x1C copied to
  +0xD0..+0xD8, the rest coordinates); Flags |= 0x4000 (indestructible, byte +0x25 bit 0x40); the `PuzzleTotem`
  vtable 0x8EDB84; +0xF0 = 0; +0xE4 = 100 (the highest step, `PUZZLE_TOTEM_DEFAULT_MAX_HEIGHT`); then the creation
  virtual (vt +0x658, `Totem::CallVirtualFunctionsForCreation` 0x737490: the abode's, then the mesh's flags +4 |=
  0x4000). The height +0xE8 is left 0.
- No town: `Totem::MakeFunctional` 0x7377D0 gives a totem to its town (`Town::SetTotem`) only when it has one, and
  `Totem::DoesTotemBelongToATownWhichIsVeryImpressedIndeed` 0x4E3E60 (has a town) is false, so the creature's own
  plans never choose one; the script's `CREATURE_DO_ACTION` does.
- Its script type afterwards is 2 (an abode, `GetScriptObjectType`), not 34.
- The info rows the scripts use, read from the frozen `Scripts/info.dat`: sub-type 146 "Bell Tower" (abode type 20,
  `ABODE_TOTEM`; abode number 6; no tribe; mesh 345 (`ObjectBellTower`); not physically damaged; life 1.0) and
  sub-type 22 "African Totem" (abode type 20; number 6; tribe 1; mesh 389 (`ObjectSkullTotem`); physically damaged;
  life 1.0).

### Its height

Fields: +0xE4 the highest step (int), +0xE8 the height (int), +0xEC the height before a totem puzzle game's move,
+0xF0 how many reads still blend from it, +0xC4 the hand's coordinates, +0xD8 the rest altitude, +0xF4 / +0xF8 the
drag's offset and the altitude last sent.

- `GetWorshipPercentage` (vt +0x92C, 0x6DAA90), the share it stands at: a highest of −1 → 1.0; with +0xF0 = c not 0,
  c − 1 is stored and the share is ((5 − (c − 1)) × height + before × (c − 1)) × 0.2 / highest (the read writes);
  else height / highest (`fild` / `fidiv`).
- `SetWorshipPercentage` (vt +0x930, 0x6DA680): a highest of −1 → height = −1; else height = `ftol`(highest × share),
  at most the highest.
- Only `PuzzleGame::Process` writes the blend (0x6D7E7E..0x6D7E9E: before = height, the new height, then +0xF0 = 5
  for all but one totem). A scan of the whole exe for writes to +0xEC / +0xF0 finds no other writer of a puzzle
  totem's, so a script's totem never blends.
- Where it stands (`Totem::Draw` 0x51ABA0, at every draw of a built totem; an unbuilt one is drawn as a building
  site): `InitTotemPosFromWorshipPercentage` 0x737760 sets the altitude +0x1C = rest + (share × travel − travel)
  (fn_00737730), and the object is drawn at the land under it plus that altitude. The travel is fn_00737790:
  ((1 − 0.35) × the mesh's half height (+0x28) × `GetScale`) × 2, each step a float. So at height 0 it is sunk by its
  whole travel, 0.65 of its height. While the local hand holds it, it is drawn at the hand's coordinates instead.

### The scripts' properties

- `GET_PROPERTY HEIGHT` (19) 0x70E567: a `PuzzleTotem` (`__RTDynamicCast`) gives its height as a float, before the
  totem statue's and every other thing's.
- `SET_PROPERTY HEIGHT` 0x70EA93: a `PuzzleTotem` → sub_6DA6D0: the height = the highest when the value is not below
  it (`fcomp`, `test ah, 0x41`: a NaN too), else `ftol`(value). There is no lowest.
- `GET_PROPERTY MAX_HEIGHT` (20): vt +0x14C, 0x5618A0 for a puzzle totem (its highest as a float), 0.0 for every
  other class.
- `SET_PROPERTY MAX_HEIGHT` 0x70EB48: vt +0x148 = `PuzzleTotem::SetMaxHeight` 0x6DA710: the highest = `ftol`(value),
  then vt +0x144 (the height as a float), which for a puzzle totem is `GameThingWithPos::SetHeight` 0x4019B0, a bare
  `ret 4`: the height is not brought under the new highest.

### The hand and the tap

- Tapping: `InterfaceValidToTap` 0x6DA5F0 only when the highest is −1 (the totem puzzle games set it, 0x6D6542..);
  `InterfaceTap` 0x6DA610 then sets the height −1 and plays the sample at `GGlobal` +0x3AC (not positioned).
- Taking hold: `ValidForLockedSelectProcess` 0x6DA5D0 is `IsFunctional` alone (no player test). The press
  (`GInterface` 0x5D1330 step 4) also needs it not `IsCannotBePickedUp` (Flags 0x2000, vt 0x180) and in the
  influence (vt 0x714 = 1); `StartTapOrLockedSelect` 0x5D1A00 takes the locked select when it cannot be tapped.
- A bell tower the script made unpickupable (`SET_ID_PICKUPABLE` false) is never held: the press goes on to
  `ClickOnLand`, and the idle release (fn_005D3700) remembers it for `GAME_THING_CLICKED`
  ([the tap memory](../hand/picking.md#tap-memory)).
- The grip (vt +0x6D4 0x6DA8E0, and vt +0x6D0 0x6DA8A0, the same body): the hand's coordinates = its position, the
  offset +0xF4 and the last sent +0xF8 = their altitude less the rest. No creature desire and no reaction (the plain
  `Totem`'s 0x737570 has both).
- The drag (vt +0x6DC, 0x6DA970), while the hand holds it (Flags 0x10): offset += the hand's move / 1.5 ([0xC09864]);
  below −travel it is −travel, above 0 it is 0; step = `ftol`((travel − |offset|) / travel × highest), at most the
  highest; the hand's altitude = rest − travel × (1 − step / highest), each step a float. When that altitude differs
  from the last sent, it is stored and packet 0x28 goes with the hand's coordinates. The end (vt +0x6E8, 0x6DA920)
  sends packet 0x28 once more.
- Packet 0x28 (`GPacket::ProcessPacket`, 0x63D1B8): the totem's position = the packet's coordinates, then fn_007376A0:
  share = (travel − |altitude − rest|) / travel, below 0 → 0, above 1 → 1, and `SetWorshipPercentage`(share).
- Run through the oracle (the drag, the packet and the setter of the exe, on a fake totem), that round trip leaves
  the height one step below the step the drag chose in 26 of 174 drags (for example a travel of 29.4, highest 100: the
  drag's step 99, altitude −0.294, height 98).

### The creature

- `CREATURE_RAISE_TOTEM_POLE` (56) and `CREATURE_LOWER_TOTEM_POLE` (57): `ConstructSubActionsForRaiseTotemPole`
  0x4A1BD0: go within 12, face it (0.1), individual action 0x34, then the main sub-action `RAISE_TOTEM` (0x31) on it.
- Its handler 0x504FC0: a `TotemStatue` first (the village totem's path); else a `PuzzleTotem` (0x505082): share =
  vt +0x92C; when 1 − share < 0.02 ([0x8CF178]) the share is set to 1.0, the creature's +0x10D4 = 0 and the action
  is done (2); else the share + 0.02 is set and it goes on (0). Lowering (0x505140) is the same towards 0. Neither
  class: 1.

### Land 4's scripts

- `Land4Puzzle`: five `CREATE_WITH_ANGLE_AND_SCALE` of type 34, sub-type 146, scale 1, at 340, 10, 50, 80 and 100
  degrees; `HEIGHT` = `MAX_HEIGHT` for each; `SET_ID_PICKUPABLE` false. `SeeWhichTowerClickedOn` waits for
  `GAME_THING_CLICKED` of each.
- `SetupUndeadVillage`: two of sub-type 22 at scale 2.233 (352.1 and 36 degrees), `SET_INDESTRUCTABLE`, `HEIGHT` = 0.
  `LookAfterTotem`: the creature within 50 while `HEIGHT` > 90 marks it; marked and back at 0, `CREATURE_DO_ACTION`
  56 on it, `PLAYED`, then until `HEIGHT` = `MAX_HEIGHT`. At every pass, while 0 < `HEIGHT` <= its last and it is not
  held (`IS_LOCKED_INTERACTION`), `HEIGHT` = `HEIGHT` − 0.00005: one whole step down, by the truncation.
  `LookAfterTotems` waits until both stand at their highest. So the player must first drag a totem up by hand.

## openblack

- `CreateScriptObject` (`src/CHLApi.cpp`) makes type 34 with `PuzzleTotemArchetype::Create`
  (`src/ECS/Archetypes/PuzzleTotemArchetype.cpp`): `AbodeArchetype::Create` of the info with no town
  (`nearestTownWhenNone` false; the land script's abodes still join the nearest), `Indestructible`, and the
  `components::PuzzleTotem` (highest 100, height 0, the rest altitude above the land). A sub-type past the abode table
  is refused as the other types' are.
- `ecs::puzzle_totem` (`src/ECS/PuzzleTotem.cpp`) holds the rules above as pure functions: `Travel`, `Share` (with the
  blend), `SetShare`, the `HEIGHT` / `MAX_HEIGHT` properties and `AltitudeAt`. `PuzzleTotemArchetype::Place` stands
  the totem at the land under it plus `AltitudeAt` whenever its height changes; the original does it at every draw.
- `GET_PROPERTY` / `SET_PROPERTY` `HEIGHT` and `MAX_HEIGHT` (`src/CHLApi.cpp`) take a puzzle totem first, before the
  totem statue and the creature, with `puzzle_totem::HeightProperty` / `SetHeightProperty` / `MaxHeightProperty` /
  `SetMaxHeightProperty`; a set places it again.
- The creature's RaiseTotemPole / LowerTotemPole (a script's `CREATURE_DO_ACTION` 56 / 57) on a puzzle totem: the
  step reads the totem's own share (`mind_town_help::TotemShareOf`, `puzzle_totem::ShareAsRead`) and sets it
  (`mind_town_help::Apply`: `SetShare`, then `Place`), by the same 0.02 a turn (`creature_town_help::MoveTotemTurn`)
  as the village totem's town share. The share read for the step does not count a read off a totem puzzle game's
  blend, and the placing counts one, where the original's step reads twice; no totem of openblack blends.
- The hand (`ecs::puzzle_totem_hand`, `src/ECS/PuzzleTotemHand.cpp`): the press takes a puzzle totem as it takes a
  village totem (`HandSystem`'s totem branch), by its own test (`IsFunctional`, not unpickupable, in the influence,
  not held already); the start, the hand's TOTEM state (interface state 14, the hand's hold and its side grip), the
  slide (`village_totem::HandSlide` with the totem's height, then `puzzle_totem::Drag`), the altitude packet
  (`game_packets::Type::PuzzleTotemPosition`, 0x28, sent at each new step and at the end; its handler
  `ApplyPosition`) and the end. `IS_LOCKED_INTERACTION` is true while the hand holds it. An unpickupable totem is not
  held: the release remembers it as the thing clicked (`HandClicked`). `abodes::InterfaceValidToTap` is false for a
  puzzle totem that is not tap-only, so it is never knocked on as a house.
- The slide's stop at the land reads the original's point: the hand stands at the totem's own place (the state's
  Enter puts it at the held object's MapCoords, and nothing moves it), so the land under the hand is the land at the
  totem's place, which `puzzle_totem_hand::Slide` reads (2026-10-24). Only a slide down is held to the land: a sunk
  totem whose grip is under the land is dragged up by the mouse's own slide, as the original's
  ([the village totem's update](village-totem.md#the-locked-select-on-a-village-totem-hand-state-5);
  `VillageTotemRules.AHandBelowTheLandSlidesUpFreelyAndIsBroughtUpToTheLandGoingDown`).
- The drag is worked as the original's, step by step in floats, and so is the round trip through the packet:
  `PuzzleTotemRules.TheHandsDragGivesTheOriginalsAltitudeAndHeight` pins 11 drags run on the exe (the oracle), and
  `...TheDragCanLeaveItAStepBelowTheStepTheHandChose` the step lost.
- Tests: `PuzzleTotemRules.*` and `PuzzleTotemPlaceTest.*` in `test/test_village_totem.cpp`.
- Testbed (`src/Debug/TestbedScenarioPuzzleTotem.cpp`): `story.bell_towers` (the Totem Puzzle's five towers, made,
  raised and made unpickupable through the natives, then the middle one clicked) and `story.undead_totem_drag` (the
  African totem made sunk and dragged up by the hand). The commands `ScriptPuzzleTotem` (CREATE, HEIGHT and
  SET_ID_PICKUPABLE through the natives) and `PuzzleTotemHeights` (the readout: every puzzle totem's HEIGHT and
  MAX_HEIGHT as a script reads them, and whether the hand holds it) are the debug controls. No check reaches a puzzle
  totem otherwise: no land before the fourth makes one.

## Pending

- 🔍 The original's press on an unpickupable totem goes on to `ClickOnLand` (0x5D3D10), not read; openblack's press
  does nothing there, and the release remembers the totem as the original's does.
- The tap of a tap-only totem (its sound) and the totem puzzle games' own totems.
- 🔍 What the mesh flag 0x4000 the creation sets does, and whether the abode's did-you-know sign and street lantern
  (`CreateAbodeSurroundingObjects`) belong to a totem of no town.
