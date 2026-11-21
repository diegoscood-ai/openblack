# The lion maze and the Theseus mazes

## Summary

Puzzle game type 12 is a grid of 7 by 7 squares walled with short fences, a palm at each corner and a street lantern
half way along each side. A wolf (the engine calls it the lion) and a sheep stand on their squares; four bells round the
grid each send the wolf sliding one way until a wall stops it, and a fifth, at a corner, starts the maze again. The
wolf turns to its way and then walks to its new square at 4 m/s; the puzzle is solved when it has walked onto the
sheep's square, and both go in a sparkle. Land 5's `LionPuzzle` script makes it (Stanley the wolf).

## The original

- **Its making** (the puzzle's constructor sub_6D61D0, case 12 and 13): sub_6DAD50, then the five bells.
- **sub_6DAD50** (types 10 to 13):
  - the steps +0x390 / +0x394 (ftol(cosine × size), ftol(sine × size) of the game angle +0x38C) are divided by 5, so a
    grid unit is a fifth of a cell (2 m at size 1); +0x44 and +0x46 (words) are 0;
  - the walls: for x and y from 0 to the size (fn_006DB520: 7 for types 10, 12 and 18, 8 for 11 and 13), first a wall
    across x when y is not the size and fn_006DACD0(x − 1, y, 1) (+0xFC + 36x + 4y), then a wall across y when x is
    not the size and fn_006DACD0(x, y − 1, 0) (+0x240 + 36x + 4y), each Flags |= 0x2000 (byte +0x25 |= 0x20: never
    picked up);
  - a wall (fn_006DB020(x, y, way)): `MobileStatic::CreateWithXYZAngles` (fn_00608840) of GMobileStaticInfo row 0 (the
    short Celtic fence) at the grid place (2x + 1, 2y) across y or (2x, 2y + 1) across x (fn_006D9290(across, along) =
    x + sine × along + cosine × across, z + cosine × along − sine × across), angles (0, a, 0) and the puzzle's size,
    a = float(float(+0x38C) × π) × (1 / 512) (asm 0x6DB046..0x6DB064: π [0x8C36A0], 2^−9 [0x93AAF0]), + π/2 across x
    ([0x8C7B48]); then Flags |= 0x4000 (indestructible). With the game angle's 2048 to the circle, a wall turns four
    times the puzzle's angle; Land 5's puzzle has angle 0;
  - four palms (`Tree::Create(place, GTreeInfo row 8, forest 0, 1.0, 0, 1.0)`) at (0, 0), (14, 14), (14, 0), (0, 14),
    each held by the scripts (vt +0x440 (1)); four street lanterns (`GStreetLantern::Create`, GMobileStaticInfo row 7)
    at (6, 0), (14, 6), (6, 14), (0, 6);
  - then `ResetToStartConfig`.
- **The map** (`PuzzleGame::IsBlockInDirectionSimple` 0x6DAB00: type 12's at 0x93A9E8, read with the oracle): 8 rows of
  9 bytes, row 7 − y, column x; 0xAF a wall on the square's +y side, '|' on its +x side, 0xAC both. fn_006DACD0(x, y,
  way): 1 off the map's 0..7, else way 0 tests 0xAF or 0xAC, way 1 '|' or 0xAC. The step test fn_006DAC60(x, y, way):
  +y (x, y, 0), +x (x, y, 1), −y (x, y − 1, 0), −x (x − 1, y, 1). The oracle's answer for every square and way is
  pinned in `LionMazeRules.TheWallsStopTheStepsTheOriginalsStop`.
- **The bells** (0x6D640B..0x6D65D0): `PuzzleTotem::Create` (fn_006DA550) of GAbodeInfo row 146 (the bell tower), no
  town, size 0.5, at (7, 17) angle 0, (17, 7) 4.5, (7, −3) 3.0, (−3, 7) 1.5 and (−3, −3) 2.4; each highest +0xE4 = −1
  (only tapped), height 0, Flags |= 0x1000 (unmovable) and held by the scripts. Bells 0 to 3 send the wolf +y, +x, −y,
  −x; bell 4 starts again.
- **`ResetToStartConfig`** 0x6DB160 (types 10 to 13): the sheep (+0x3F8), the villager (+0x3FC, types 10 and 11) and
  the wolf (+0x3F4), when available, melt away (`GoolooGooloo`) and go (vt +0xC); the squares from the type's tables
  (type 12: the sheep (4, 4), the wolf (1, 3)); the sheep a `Cow` of GAnimalInfo row 24 (`PuzzleSheep`'s vtable), the
  wolf a `Lion` of row 25 (`PuzzleLion`'s), both of age 20, `SetScale(2.0)` and Flags |= 0x2000; then the wolf's and
  the sheep's `CallVirtualFunctionsForCreation` at their squares (the wolf first) and Flags |= 0x4000; a 10 turns
  sparkle (spot visual 12, magnitude 2.0) over the sheep, then over the wolf.
- **The pieces**: `PuzzleLion` is `PieceWolf`'s vtable (the wolf's model and clips, no needs, not frightening) with
  `DecideWhatToDo` 0x6DB100 = 1 (nothing) and `CanBecomeAPhysicsObject` 0x5615D0 = 0; `PuzzleSheep` is
  `PieceSheep`'s with the same two.
- **The turn** (`PuzzleGame::Process` 0x6D7480, case 12, after the solved flag and `IsComplete`):
  - every turn, all types first: a tree +0xC4..+0xD0 or lantern +0xD4..+0xE0 no longer available is let go of;
  - fn_006D71D0: each wall (x and y 0 to 8, across x first): `SetTemperature(0, 0)`; not available, made again (no
    0x2000); off its place (its MapCoords against fn_006D9290's), melted away, deleted and made again with 0x2000;
  - no wolf or no sheep: nothing more. Either unavailable: `ResetToStartConfig`. Either in state 1 (MOVE_TO_POS):
    straight to the end;
  - the sheep off its square: `MoveMapObject` (vt +0x55C) onto it;
  - bells 0 to 3 in turn: one no longer available is made again (fn_006D7400: at its x and z, altitude 0, row from the
    old one, angle 0, size 1.0, its highest and height copied, unmovable, held by the scripts); the first whose height
    is not 0 is rung: a 3 turns sparkle (12, 2.0) at it, the script sound bank's (GAudio +0x3BC) sample 0x5C at its
    point, 3D, not tracked, owned by the bell's slot (+0x398 + 4i), begun +0x584 = 1; then, while fn_006DAC60 says
    no wall, the wolf's square steps that way, its way +0x40 = 0x200, 0, 0x600 or 0x400 and +0x44 = 0;
  - the wolf `SetTowardsAngle(+0x40)` (vt +0x868, `Animal::SetTowardsAngle` 0x418560); when its game angle (+0x5C) is
    +0x40: at stage 0 `SetSpeedInMetresPerSecond(4.0)` (0x60C110: `SetSpeed(ConvertMetersToWholeDistance(4 / 10))`)
    and `SetupMoveToPos(its square, DECIDE_WHAT_TO_DO)` (0x5F2830), stage 1; at stage 1 `AreWeThere(0)` (0x60AD40),
    stage 2;
  - bell 4's height not 0: `ResetToStartConfig`;
  - the end: every bell's height 0.
- **The solution** (`IsComplete` 0x6D66E0 → 0x6D6BA7): a wolf and a sheep; the wolf not in state 1; stage 2;
  `AreWeThere(0)`; its square the sheep's. Then a 3 turns sparkle (12, 2.0) over the sheep, the sheep and the wolf
  deleted (vt +0xC (0)) and their slots emptied, solved +0x3C = 1: a later test finds no wolf and says no (the PLAYED
  native and the status call `IsComplete` too).
- **The status** (`GetPuzzleGameStatus` 0x6DC070): not solved → 5 (not begun) or, with +0x584, 4 (in progress).
- **A bell's tap** ([the puzzle totem](../magic/puzzle-totem.md#the-hand-and-the-tap)): its height −1.

## openblack

- `ecs::lion_maze_game` (`src/ECS/LionMaze.h`, 2026-10-31), its rules `ecs::lion_maze` (`src/ECS/LionMazeRules.h`: the
  map, `WallAt`, `Blocked`, `Slide`, the grid's spots, `WallAngle`, the bells) and the maze `components::LionMaze` on
  the PuzzleGame: `CreatePuzzleGame` starts it for type 12 (`Tilt1`, `IsLionMaze`), `IsPuzzleGamePlayed` asks its test
  and `ProcessPuzzleGame` its turn, in the original's order. The grid's steps are `hanoi::StepsOf`, divided by 5, and
  `hanoi::GridPlace`.
- The walls are `MobileStaticArchetype::CreateWithXYZAngles`, the palms `TreeArchetype`, the lanterns
  `StreetLanternArchetype`, the bells `PuzzleTotemArchetype` totems only tapped. A wall's, the sheep's and a bell's
  place are kept as they were put: openblack keeps a thing's place as a world point, which does not round-trip to the
  same MapCoords. A bell destroyed before it is made again gives its own highest and height (−1 and 0) instead of
  the old object's.
- The pieces: `AnimalArchetype::ConstructPiece` and `FinishCreation` (the class constructor, then the creation's end),
  `components::PuzzlePiece` (`src/ECS/Components/PuzzlePiece.h`): such an animal decides nothing, has no needs and
  never becomes a physics object (`DynamicsSystem::CanBecomeAPhysicsObject`). The wolf's turn and walk are
  `animal_ai::TurnTowards`, `GameAngle`, `SetSpeedInMetresPerSecond`, `MoveTo` and `AreWeThere` (`src/ECS/AnimalApi.cpp`).
  The pieces' own reactions and the creature's fear are the animal AI's, not changed here.
- A bell's sound owner is an `audio::NewObjectId` number of its slot, taken the first time it rings, as the slot's
  address is the original's owner.
- Tests `LionMazeRules.*` (`test/test_lion_maze_rules.cpp`) and `LionMazeWorld.*` (`test/test_lion_maze.cpp`, on a
  flat land, synthetic and game data). Readout: the debug overlay's Lion Maze line (the wolf's and the sheep's squares,
  the wolf's walk, begun or not).
- Checked in play: the quest run a2st_3 (`quest.stanley_the_wolf`, Land 5, the pinned pointer, 2026-11-03, refs/story/evidence/a2st_stanley_bells.log: the eight bells pressed at lines 97259, 104328, 111004, 118587, 125438, 132006, 139689 and 146228, then `PLAYED` of the puzzle true at line 148391, turn 3251); the bells were pressed through the testbed's pinned pointer, not the
  window's input path ([../project/progress/story/minigames.md](../project/progress/story/minigames.md), "A wolf stands in a walled grid").

## The Theseus mazes

Types 10 and 11 (Land 4's `ThesiusPuzzleLand4` makes 10, then 11 once 10 is played) share the lion maze's start: the
same walls of their own maps, the palms, the lanterns and `ResetToStartConfig`, with a villager in place of the sheep
and no bells. The player clicks the grid's ground: the villager steps a square towards the click (or waits, clicking his
own square), then the wolf takes up to two steps after him. The wolf on him starts the maze again; the villager past the
grid's last row or column is the solution.

### The original

- **The kinds** (fn_006DB520, the start tables 0x6DAB60..0x6DAC20, `IsBlockInDirectionSimple`): type 10 has 7
  squares, its map at 0x93A940, the villager at (2, 3) and the wolf at (2, 5); type 11 8 squares, 0x93A994, (2, 7) and
  (1, 6). The oracle's step test for both is pinned in `LionMazeRules.TheTheseusMazesWallsStopTheStepsTheOriginalsStop`.
- **The villager** (`ResetToStartConfig` 0x6DB160, types 10 and 11): an `Animal` of GAnimalInfo row 26 (+0x48B8,
  `PuzzleVillager`'s vtable: `PieceVillager`'s with `DecideWhatToDo` 0x6DB0F0 = 1 and `CanBecomeAPhysicsObject`
  0x561680 = 0), age 20, at its age's size (no `SetScale`), Flags |= 0x2000 (+0x3FC); the wolf as the lion maze's. The
  creation's end: the villager, then the wolf, each then Flags |= 0x4000; a 10 turns sparkle over the villager, then
  over the wolf.
- **The click** (`ClickOnLand` 0x5D3D10, the press on the land that is not the untied leash's act): `AppliedMapPos` =
  the hand's point (0x5D3D9F..0x5D3DBA). `GlobalGameLists::Process` clears it after every puzzle's turn (0x591455..
  0x591464), so a click counts for the turn after it only.
- **The turn** (fn_006D8B00, from `PuzzleGame::Process` for types 10 and 11):
  - the marker (+0x30) no longer available goes; the walls kept (fn_006D71D0); the villager (+0x3FC) or the wolf
    (+0x3F4) no longer available: `ResetToStartConfig`, nothing more;
  - while either is in state 1 (MOVE_TO_POS) or either walk (+0x44 the wolf's, +0x46 the villager's) is not 2: on to
    the walks;
  - else the marker goes; the villager's MapCoords equal to the wolf's (`MapCoords::operator==` 0x605660):
    `ResetToStartConfig`, nothing more;
  - no click (`AppliedMapPos`'s x and z cells 0): nothing more; a click west or south of the place, or east or north of
    the grid's far corner (fn_006D9290(2s + 2, 2s + 2)), compared unsigned: nothing more;
  - the click's square (0x6D8D0F..0x6D8D8D): across = ftol((cosine × dx − sine × dz) / (size² × 171793456.0)), along =
    ftol((sine × dx + cosine × dz) / (that)), each a float at 24 bits, then (n / 2) × 2 + 1; a `CreateSpotVisual` of
    visual 12, magnitude 1.0, no time, at it is the marker;
  - the step (0x6D8DB9..0x6D8E63): the click's offset from the villager's square in half squares; none: wait; along y
    with nothing across, +y or −y; along x with nothing along, +x or −x; on a slant nothing; a step a wall stops (fn_006DAC60)
    nothing. Nothing: no more this turn;
  - a step moves the villager's square; his way +0x42 = ftol(−512.0 [0xD4EEE0] + 1024, 512, 0 or 1536 for +y, +x, −y,
    −x, brought into 0..2048) and +0x46 = 0. A wait leaves him;
  - the wolf, twice: begun +0x584 = 1; the first of +x, −x, +y, −y that brings it nearer the villager on that axis
    with no wall; else it stops. Its ways' code (+x 3, −x 18, +y 9, −y 1, each once) picks its way (the byte table
    0x6D9270 into 0x6D9248): none for 0; −512 + 512 (3), 256 (4), 1024 (9), 768 (12), 1536 (18), 1792 (19), 1280
    (27), any other 0; +0x44 = 0;
  - the walks (0x6D9108..): the wolf, then the villager, each `SetTowardsAngle` of its way, and once it faces it, at
    stage 0 `SetSpeedInMetresPerSecond(4.0)` and `SetupMoveToPos(its square, DECIDE_WHAT_TO_DO)`, stage 1, and at
    stage 1 `AreWeThere(0)`, stage 2.
- **The solution** (`IsComplete` 0x6D66E0, types 10 and 11): the villager's x or z square the size. A 3 turns sparkle
  (12, 2.0) over the villager; the villager and the wolf deleted (vt +0xC (0)), their slots emptied; the marker, when
  available, deleted; solved +0x3C = 1, yes. Nothing tests it again in play: the puzzle's turn stops once it is
  solved (0x6D74C3), `GGame::ProcessTurn` runs the puzzles' turn (0x54E651) before the scripts' (0x54E693) so the
  solving step is made a turn before the script's PLAYED sees it, and the only script that reads the two mazes
  (`ThesiusPuzzleLand4`) stops asking at its first yes (it deletes the first maze, :129669, and goes on from the second,
  :129683); no script asks their state (`GetPuzzleGameStatus`, the other caller).
- The chess puzzle's part of the click (sub_6DCB90) is for type 19, which no land makes.

### openblack

- `lion_maze_game` takes both (`src/ECS/LionMaze.h`, 2026-10-31): `components::LionMaze` keeps its `lion_maze::Kind`
  (`k_TheseusOne`, `k_TheseusTwo`, `k_LionMaze`: squares, map, starts), and the villager, his square, way and walk and
  the marker. The rules: `ClickedSpot`, `ClickedStep`, `StepAngle`, `ChaseOf`, `ChaseAngle`, `Escaped`
  (`src/ECS/LionMazeRules.h`).
- The click: `HandSystem::TapLand` keeps the hand's point (`SetPuzzleAppliedMapPos`, `src/ECS/PuzzleGames.h`) when the
  press is not the leash's act; `ProcessPuzzleGamesTurn` clears it after the puzzles' turn.
- The solution as the original's; as nothing tests it again after it (above), its sparkle is made only while the villager
  is there, and the second test the original never makes is not modelled.
- Tests `LionMazeRules.TheTheseusMazesWallsStopTheStepsTheOriginalsStop`, `AClickAsksTheVillagerForAStep`,
  `AClickLandsOnASquaresMiddle`, `TheWolfChasesTheVillagerTwoSteps`, `TheVillagerCanGetOutOfBothTheseusMazes`, and
  `LionMazeWorld.ATheseusMazeIsMadeWithItsVillagerAndWolf`, `AClickMovesTheVillagerAndTheWolfChasesHim`,
  `TheWolfCatchesTheVillagerOrHeGetsOut`. Readout: the debug overlay's Theseus Maze line.

## Pending

- Not checked in play: the Theseus mazes (Land 4's `ThesiusPuzzleLand4`) have no quest run yet.
- Type 13 (the second lion maze) is not made: no shipped script creates it.
