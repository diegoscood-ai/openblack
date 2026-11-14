# The Japanese totem puzzle

## Summary

Puzzle game type 8 is an arc of six Japanese totems round the puzzle's place, each of five heights (0 to 4). Raising
or lowering one totem moves some of the others by a fixed table, every height counted round in fives; a totem at its
highest has a sparkle over it, and the puzzle is solved when all six stand at their highest. Land 4's
`JapaneseTotemPuzzle` script makes it north-west of the Japanese village, inside a ring of influence of radius 30, and
gives a spiritual shield miracle dispenser once it is played.

## The original

- **Its making** (fn_006DA350, from the puzzle's constructor sub_6D61D0 for types 6 to 9, and from
  `PuzzleGame::ResetToStartConfig`): the six kept heights +0x3CC..+0x3E0 are 0. For types 8 and 9, the totems
  +0x398..+0x3AC go round an arc, the step +0x0, +0x400, .. +0x1400 (below 0x1800):
  - the place: the puzzle's place +0x14..+0x1C (the script's x and z, altitude 0: the CHL create drops it for a puzzle
    game), x + `ftol`(SIN[step / 6] × size +0x388, doubled) and z + `ftol`(SIN[step / 6 + 512] × size, doubled), the
    index the unsigned multiplication by 0xAAAAAAAB keeps the high word of, shifted right by 2 (entries 0, 170, 341, 512,
    682, 853 of `LHSinTable`); at size 1, 20 m from the place (the first along +z, the fourth along +x);
  - `PuzzleTotem::Create` (fn_006DA550) of `GAbodeInfo` row 54 (the Japanese totem, +0x6030), no town, angle 0, scale
    [0xC09860] (1.0, a constant: no writer), built 1.0; then its highest +0xE4 = 4, its height +0xE8 = the kept height
    (0), and the flag byte +0x25 |= 0x10 (Flags 0x1000, unmovable: `SET_ID_MOVEABLE`'s flag);
  - its angle: `SetYAngle` (vt +0x524, `Object::SetYAngle` 0x639260) of `fpatan`(dz, dx) − π/2 ([0x8C78D8],
    1.5707964), dx and dz the totem's place less the puzzle's in `GetLHPoint` metres, as float subtractions; the
    arctangent comes out extended and is rounded once, to float, by the subtraction after it.
  - Types 6 and 7 instead stand their six totems on the puzzle's grid (fn_006D9290 (k, 0)) at angle 0.
- **The turn** (`PuzzleGame::Process` 0x6D7480, types 6 to 9, after the solved flag +0x3C and `IsComplete`):
  - totem by totem, in order: one that is no longer available (vt +0x2C) is made again (fn_006D7400): `PuzzleTotem::
    Create` at its MapCoords' x and z with altitude 0, of its own info +0x28, no town, angle 0, scale 1.0; its highest
    and height copied from the old one, +0x25 |= 0x10 and `SetControlledByScript(1)` (vt +0x440); it is not turned;
  - the first totem whose height differs from its kept height is the move. Before it, a totem whose blend +0xF0 is not
    0 is put at its height (`Totem::InitTotemPosFromWorshipPercentage` 0x737760). No move: nothing more.
  - The move: begun +0x584 = 1; move = height − kept + 5; the moved totem's kept height = (kept + move) % 5; each other
    totem's = (table[moved][other] × move + kept) % 5, the table of fn_006DA520 (types 7 and 9: 0x93A8B0; the others,
    type 8 included: 0x93A820, read from the exe with the oracle):

    | moved | 0 | 1 | 2 | 3 | 4 | 5 |
    |---|---|---|---|---|---|---|
    | 0 | 0 | 4 | 0 | 0 | 0 | 2 |
    | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
    | 2 | 1 | 1 | 0 | 2 | 0 | 0 |
    | 3 | 0 | 0 | 1 | 0 | 0 | 0 |
    | 4 | 0 | 0 | 0 | 0 | 0 | 1 |
    | 5 | 0 | 3 | 0 | 1 | 0 | 0 |

  - Then every totem: +0xEC (the height before) = its height, its height = its kept height, the blend +0xF0 = 5 for all
    but the moved one, and `InitTotemPosFromWorshipPercentage` (the [puzzle totem](../magic/puzzle-totem.md)'s blend
    counts that read off).
  - The sparkles +0xE4..+0xF8: each one is deleted (vt +0xC (0)) and emptied; for a totem whose kept height is 4, a new
    `GParticleContainer::CreateSpotVisual` (0x63E540) at its arc place (worked out again as at the making), spot visual
    12 (the creature's target), magnitude 5.0, for its own life.
- **The solution** (`IsComplete` 0x6D66E0, types 6 to 9): every kept height 4. Then each sparkle is deleted, a
  `CreateSpotVisualWithSpecifiedDuration` of visual 12, magnitude 5.0, 3 turns at each arc place, solved +0x3C = 1,
  and the answer is yes. `IsComplete` has no early answer for a solved puzzle, and the kept heights stay at 4, so each
  later call (the script's PLAYED, the status) answers yes and makes the six sparkles of three turns again; the turn
  no longer runs once solved.
- **The status** (`GetPuzzleGameStatus` 0x6DC070): not solved → 5 (not begun) or, with +0x584, 4 (in progress).
- The land click's puzzle part (sub_6DCB90, `AppliedMapPos`) is not this puzzle's: its flag [0xD4EF10] is set only by
  sub_6DD7D0, which the constructor runs for type 19 (`PUZZLE_GAME_CHESS`; type 18, the lions and sheep, starts with
  sub_6DBA80). No shipped script makes either.

## openblack

- `ecs::totem_puzzle_game` (`src/ECS/TotemPuzzle.h`, 2026-10-31), its rules `ecs::totem_puzzle`
  (`src/ECS/TotemPuzzleRules.h`: `ArcIndex`, `ArcPlace`, `FacingFromMiddle`, the table `k_Moves`, `AfterMove`,
  `Solved`) and the arc `components::TotemPuzzle` on the PuzzleGame: `CreatePuzzleGame` starts it for type 8 (`Totem3`,
  `IsTotemArcPuzzle`), `IsPuzzleGamePlayed` asks its test and `ProcessPuzzleGame` its turn, in the original's order.
- The totems are `PuzzleTotemArchetype` totems made at no angle and turned with `placed_turn::SetAngles`, as the
  script's ANGLE turns a thing; unmovable through `object_flags::SetMoveable(false)`; a remade one held by the scripts
  (`script_held::SetControlledByScript`). The sparkles are `ParticleSystemInterface::StartSpotVisual`
  (`SpotVisualType::CreatureTarget`), deleted with `ToBeDeleted`.
- The arctangent of the facing is taken in double and rounded to float once, by the subtraction of the quarter turn,
  as the codebase takes the FPU's extended sine and cosine (`Common/GameAngle.h`).
- A totem that has gone is made again at the arc place the puzzle kept (the totem never moves off it; openblack keeps
  a thing's place as a world point, which does not round-trip to the same MapCoords), with the highest step and height
  the old one has, or, once it is destroyed, the ones the puzzle last saw.
- Tests `TotemPuzzleRules.*` (`test/test_totem_puzzle_rules.cpp`) and `TotemPuzzleWorld.*`
  (`test/test_totem_puzzle.cpp`, on a flat land, synthetic and game data). Readout: the debug overlay's Totem Puzzle
  line (each totem's kept height, begun or not).

## Pending

- Not checked in play: Land 4's story has not yet been run as far as the Japanese village.
- Types 6, 7 and 9 (the grid layouts and the second table) are not made: no shipped script creates them.
