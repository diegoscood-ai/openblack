# The beach temple puzzle

## Summary

Land 2's beach temple (`PuzzleGame` 3, the Hanoi puzzle that the silver scroll HanoiFlood makes): four rings of
different sizes stacked on the first of three bases in a row, to be moved by hand, one at a time and never onto a
smaller ring, until all four stand on a base other than the first two. The puzzle puts back what moves off its place,
settles a dropped ring onto the stack it lands near and tells the scripts when it is solved. The quest around it is on
[The Beach Temple Puzzle](../project/progress/story/silver_scrolls/the_beach_temple_puzzle.md); its progress rows on
[minigames](../project/progress/story/minigames.md#beach-temple-rings-land-2).

openblack ports the rules (`src/ECS/HanoiRules.h`, taken from raffclar's f3f1a91b8, `test/test_hanoi_rules.cpp`) and
the puzzle's pieces, turn and solution (`src/ECS/HanoiPuzzle.cpp`, refitted from his puzzle game system onto our
`src/ECS/PuzzleGames.cpp`).

## The original

The v1.42 exe's `PuzzleGame` (constructor 0x6D61D0, `Process` 0x6D7480, `IsComplete` 0x6D66E0) and `HanoiBlock`
(made by fn_006DB8A0, `IsCannotBePickedUp` 0x6DE440, `InterfaceSetInMagicHand` 0x6DE3F0, `CanBecomeAPhysicsObject`).

### The puzzle's grid

- The script's angle in radians becomes a game angle by `ftol(radians x 2048 x 0.15915494)` (0x3E22F983); the
  constructor keeps the game cosine and sine of it (the sine table, cosine at +512, no mask), each times the size and
  truncated (+0x390 and +0x394).
- fn_006D9290(across, along) is the place on the grid: x = sine x along + cosine x across + the puzzle's x, z = the
  puzzle's z + (cosine x along - sine x across), the altitude the puzzle's.
- At the start (sub_6DB570) both steps are doubled, the three bases are made at (0, 0), (0, 1) and (0, 2), and the four
  rings on the first base, the largest at the bottom, each raised by the height of the piece under it: 0 for a base,
  4, 3, 2 and 1 for the rings from the largest (mobile object infos 26 for a base, 27 to 30 for the rings).
- Land 2's only puzzle has the half turn (game angle 1024): its row goes 20 m a base towards -z from the puzzle's spot.

### The pieces

- A piece (fn_006DB8A0) keeps its rest place (+0x68), the piece on it (+0x74), the piece under it (+0x78) and its
  puzzle (+0x7C); it is indestructible (flag +0x25 bit 0x40), and a base is immovable (flag bit 0x1000).
- The hand may not take a base, a piece with another on it, a piece of no puzzle, or any piece while the puzzle's count
  of calm turns is below 5; and not a piece with the cannot-be-picked-up flag (bit 0x2000).
- Taken into the hand (`InterfaceSetInMagicHand`), a piece comes off the piece under it and loses its links; the largest
  ring says one of eight lines (text 5267 + LocalRand(8), through `GScript::SaySoundEffect`).
- A base never becomes a physics object, and a ring only while nothing is under it.
- The start (0x6DB570) switches off the shadow on the land's texture of the three smaller rings: `SetShadowOnTexture(0)`
  (3D object vt +0x80, 0x7F9880, which clears bit 0x1000) on rings 2 to 4 at 0x6DB7A1; the largest ring's 3D object gets
  bit 0x100 instead (0x6DB7B2, not read further). A piece made again later comes from the same maker and keeps its
  shadow (raffclar's c2a717f1e, his vanilla-bugs row 822).

### The puzzle's turn (`Process`, kind 3)

Once solved, nothing (`IsComplete` sets the puzzle's played mark). Else, each turn:

1. Each base off its grid place is ghosted (`GoolooGooloo`), deleted, and a new base is made on the place with no
   links, taken under script control (vt +0x440). A base that is gone is made again from its last links.
2. Each ring that is gone is made again. A ring off its rest place and not in a hand looks for a stack: in nine map
   cells spiralling out from its own cell, the first puzzle piece within 10 m of it that is not the ring, followed up
   to the top of its stack, a stack holding the ring not counting.
   - If the ring may rest on that top piece (on a base, or on a ring of the same size or larger), it goes onto it: its
     rest place is the top's, raised by the top's height, the links are set, the puzzle is marked begun, and it is made
     again at its new place.
   - Else, once a ring of this turn has gone onto a stack, it is made again at its place.
   - Else the turn is restless: the count goes down by one (to -5 at the lowest), and below 0 the ring is put back onto
     the stack found at its rest place (no size test) and made again there.
3. A calm turn (no restless ring) adds one to the count, up to 5. The count starts at 5.
4. A piece made again plays one of nine sounds of the editor bank from 413 (2D, no owner) and is taken under script
   control.

### The solution (`IsComplete`)

Each ring rests on the next larger one and the largest on neither the first nor the second base. The rings then get the
cannot-be-picked-up and immovable flags and the puzzle is played: `PLAYED` and `GET_OBJECT_STATE` read this test.

### The puzzle going

`ToBeDeleted` 0x6D6FF0 takes the puzzle off the game's list and clears the four rings' puzzle (the bases keep theirs).

## openblack

- `src/ECS/HanoiRules.h` and `.cpp`: the grid, the heights, the may-rest test, the solution test, the pick-up test and
  the count of calm turns, as pure functions; `test/test_hanoi_rules.cpp`.
- `src/ECS/HanoiPuzzle.cpp` (`ecs::hanoi_puzzle`): `CreatePuzzleGame` of type 3 keeps the place and the game angle
  (`hanoi::GameAngleOf`) and makes the pieces (`Start`): each a mobile object with a `components::HanoiBlock`
  (`ECS/Components/PuzzleGame.h`), indestructible, a base immovable. `ProcessPuzzleGame` runs the turn (`Process`) as
  above; a piece taken away leaves its ghost (`object_ghosts::Add`) and is deleted; one made again is put under script
  control and plays its editor-bank sound. `IsPuzzleGamePlayed` (`PLAYED`) and `PuzzleGameStatusOf`
  (`GET_OBJECT_STATE`, begun once a ring has gone onto a stack) read `TestSolution`, which sets the rings'
  cannot-be-picked-up and immovable flags. A puzzle that goes leaves its rings with no puzzle (`Forget`).
- The three smaller rings of the start carry `components::NoShadowOnTexture` (`hanoi::StartRingCastsShadowOnTexture`),
  which `abodes::CastsShadowOnTexture` reads, so neither the land's prints nor the projected shadows draw theirs; a
  ring made again has none. Test `HanoiPuzzleTest.ThePiecesAreMadeStackedOnTheFirstBase`,
  `ATurnSettlesARingLetGoOfNearABase`.
- Where openblack differs: a piece is off its place when its drawn position differs from the one the puzzle put it at
  (the original compares map coordinates, which are its positions); several puzzles take their turns oldest first (the
  original's list is newest first; the land has one); the pieces are made at angle 0 and size 1 (raffclar's choice;
  the original's arguments are not read).

- The hand's and physics' side: `object_flags::IsCannotBePickedUp` asks a piece's own refusal first
  (`hanoi_puzzle::RefusesPickUp`), then the flag, as the piece's own pick-up test does; the player's hand taking a piece
  (`HandSystem::PickUp`) unlinks it and the largest ring says its line through `audio::voices::Say`
  (`hanoi_puzzle::TakenInHand`); `PhysicsGameHooks::CanBecomePhysicsObject` gives a base no physics and a ring only
  while it rests on nothing (`hanoi_puzzle::CanBecomePhysicsObject`).

- A saved game's puzzle and its seven blocks are made again and written back as the original's load and save keep them
  (the stacks, the rest places, begun and played; the count of calm turns 5, and the rings' shadows on the land's
  texture, as the load does not run the start): [saved games, the puzzle games](../engine/saved-games.md#the-puzzle-games).
- Tests: `test/test_hanoi_puzzle.cpp` (the pieces made, a ring settled and one put back after the restless turns, a
  moved base put back, the hand's refusal, a taken piece unlinked, the physics test, the solution read by `PLAYED` and
  `GET_OBJECT_STATE`, the puzzle gone), in a small world with the real map cells.

## Pending

- A computer player's or a creature's hand taking a piece does not unlink it yet: only the player's hand calls
  `TakenInHand`.
- **Found difference (2026-11-14): the creature's pick-up test.** The original's creature asks a piece only what it
  asks any mobile object: `Object::CanBePickedUpByCreature` 0x4E3A20 (the script, town-artifact, villager and map
  tests), then 0x4E3960 (the leash, the reach, the weight), then `MobileObject::CanBePickedUp` (vt +0x628, 0x425B30:
  the cannot-be-picked-up flag, which the solve sets). The piece's own refusal (`HanoiBlock::IsCannotBePickedUp`
  0x6DE440, vt +0x180) is read only by the hand's and the interface's code (`xref.py vtcalls 0x180`: GInterface,
  `PlaceObjectInMagicHand`, no creature function). So before the solve a creature may take a base, a ring with
  another on it or a ring of a restless puzzle, as far as its reach and weight allow. openblack's creature test
  (`CreatureObjectActionSystem::PickUpTest`) reads `object_flags::IsCannotBePickedUp`, which adds the piece's own
  refusal (`hanoi_puzzle::RefusesPickUp`), so its creature is refused those pieces. To fix as its own measured commit:
  the creature's test reads the flag alone.
