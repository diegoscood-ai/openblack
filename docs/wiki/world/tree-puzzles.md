# Tree puzzles

## Summary

Two puzzle games (types 1 and 2) are a grid of three by three spots, each with a tree whose kind shows the spot's
state. Pulling a tree off its spot changes that spot and the four beside it, each planted again with a small tree that
grows a twentieth of its size a turn. The puzzle is solved when every tree is fully grown and every spot shows the
same state; the trees then burn. Land 2's `TreePuzzleOne` makes type 1 (two states, turned 30 degrees) and Land 3's
`TreePuzzleTwo` type 2 (three states).

## The original

- **The puzzle's grid.** The puzzle's constructor (sub_6D61D0) keeps its place +0x14..+0x1C (the script's x, z and
  altitude), its type +0x48 and the two steps +0x390 / +0x394 = ftol(cosine × size) and ftol(sine × size) of its
  angle. A spot's place (fn_006D9290 (across, along)): x = sine × along + cosine × across + x, z = z + (cosine × along −
  sine × across), at the puzzle's altitude (`hanoi::GridPlace`, the beach temple puzzle's [grid](beach-temple-puzzle.md)).
- **The start** (sub_6D94A0, from the constructor for types 1 and 2):
  - the counts +0x4C (nine signed bytes, the rows across first) are 0; a tree on each spot (fn_006D93E0), held by
    the scripts (vt +0x440 (1));
  - seven pulls (fn_006D9450) of random spots, `GameRand(3)` twice each: the first draw is the spot along, the second
    the spot across;
  - a street lantern (`GStreetLantern::Create`, GMobileStaticInfo row 7) at the grid places (−1, −1), (−1, 3), (3, −1)
    and (3, 3);
  - a flock of the neutral player (`Flock(place (1, 1), g_defaultFlockInfo, neutral, 0xABA52)`, domain radius +0x50 =
    25, flock distance +0x52 = 24), and over each spot a mist (fn_00606380 → `Mist::Create`: size 1.0, colour
    0x143B658B, shrink 4.0) and one bat (`Animal::CreateFromType` at the middle spot, row 16, age 20, in the flock)
    put in DECIDE_WHAT_TO_DO (`Living::SetTopState` vt +0x8E8 (43)).
- **A tree** (fn_006D93E0): `Tree::Create(place, GTreeInfo row, forest 0, full size, angle 0, size 0.1)`. The row is
  the count's remainder by the states (2 for type 1, 3 for type 2, fn_006D9360): 0 → 3 (conifer), 1 → 5 (oak), 2 → 1
  (birch), anything else → 0 (beech); the full size (fn_006D93A0) is 1.0 for a remainder of 0, 1 or 2, else 0.0. A count
  past 127 wraps negative and its remainder is negative.
- **A pull** (fn_006D9450 (across, along)): fn_006D92F0 on (across + 1, along), (across − 1, along), (across, along + 1),
  (across, along − 1), then (across, along). A spot off the grid is left alone; one on it gets its count + 1, its old
  tree a ghost (`GoolooGooloo`) and `ToBeDeleted(0)` (vt +0xC), and a new tree, held by the scripts.
- **The turn** (`PuzzleGame::Process` 0x6D7480, types 1 and 2, after the solved flag +0x3C and `IsComplete`): spot by
  spot, a mist or a bat no longer available (vt +0x2C) is let go of; then a tree that is available and stands at its
  spot's place (its MapCoords against fn_006D9290's) grows `Tree::Grow(0.05, true, false)` while its scale (vt +0x120)
  is below its target +0x64; any other tree in a slot (gone, taken or moved) marks the puzzle begun (+0x584 = 1) and is
  a pull of its spot.
- **The solution** (`IsComplete` 0x6D66E0, types 1 and 2): no if a slot holds no tree, if a tree's scale is below its
  target, or if a spot's remainder differs from the first spot's. Solved: each tree `Tree::SetLife(0.5)` (vt +0x5B0,
  0x74A1E0) and `Tree::SetOnFire(1.0)` (vt +0x870, 0x74C140), its slot emptied; each mist `ToBeDeleted(0)`; each bat
  `SetTopState(13)` (SET_DYING); +0x3C = 1. As the slots are empty, a later test answers no: the PLAYED native and
  the status (both call `IsComplete`) say solved only in the turn of the solution (vanilla bug, raffclar's row 739).
- **The status** (`GetPuzzleGameStatus` 0x6DC070): not solved → 5 (not begun) or, with +0x584, 4 (in progress).
- **Its deletion** (`PuzzleGame::ToBeDeleted` 0x6D6FF0) leaves the trees, lanterns, mist and bats in the world.

## openblack

- `ecs::tree_puzzle_game` (`src/ECS/TreePuzzle.h`, 2026-10-31), the rules `ecs::tree_puzzle` (`src/ECS/TreePuzzleRules.h`,
  raffclar's) and the grid `components::TreePuzzle` on the PuzzleGame: `CreatePuzzleGame` starts it for types 1 and 2,
  `IsPuzzleGamePlayed` asks its test and `ProcessPuzzleGame` its turn, as the original's order. The grid's steps are
  the beach temple puzzle's `hanoi::StepsOf` and `GridPlace`.
- The trees are `TreeArchetype` trees held by the scripts (`script_held::SetControlledByScript`), grown with
  `GrowTree`, replaced through `object_ghosts::Add` and `ToBeDeleted`; the lanterns `StreetLanternArchetype`, the mist
  `MistArchetype`, the flock and bats `AnimalSystemInterface::CreateScriptFlock` and `CreateBird`, their states
  `animal_ai::SetState`. The solution takes `life::SetLife` and `FireSystemInterface::SetOnFire`.
- A tree is on its spot while it is available and its MapCoords (`object::MapCoordsOf`, from its point) equal, x, z and
  altitude, the ones it had as it was planted: openblack keeps a thing's place as its world point, so the spot's place
  is the MapCoords the planted tree's point gives.
- Tests `TreePuzzleRules.*` (`test/test_tree_puzzle_rules.cpp`, raffclar's) and `TreePuzzleWorld.*`
  (`test/test_tree_puzzle.cpp`). Readout: the debug overlay's Tree Puzzle line (each spot's state, begun or not).

## Pending

- Not checked in play: Land 2's story has not yet been run as far as the tree puzzle.
