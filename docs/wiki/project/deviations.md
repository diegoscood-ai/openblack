# Deliberate deviations taken from raffclar's tree

## Summary

raffclar's tree keeps a list of where his openblack deliberately differs from vanilla (his `docs/deviations.md`). When
a piece of his is taken here, its deviations follow the project's working rules ("Taking pieces from raffclar"): a
presentation-only one may be taken as he wrote it; one that changes game logic keeps vanilla's behaviour by default,
with his fix beside it behind the one **Fix Vanilla Bugs** switch ([below](#kept-as-vanilla-his-fix-beside-it)). Our
own chosen differences (🎯) are listed, with their dates, on
[A reconstruction, not a decompilation](reconstruction.md#what-is-deliberately-different--chosen-differences): the high
meshes only at the top detail level (2026-10-15), the Maximum Detail entry setting that level (2026-10-20, and always,
also from the default level 4, 2026-10-21), openblack starting at that top level where the original starts at 4,
with every check run there (2026-10-21), the mesh pack's textures at 32-bit colour and their own sizes (2026-10-20),
the sun glare's depth test answering a frame or two late through his occlusion queries (2026-10-21), multisampling of
the back buffer as a debug switch only, off by default (2026-10-21), a Mods options tab only with
the mod loader and a read mods folder (2026-10-21), switched-on data mods replacing the game's files (2026-10-22),
everything that moves drawn smoothly between turns, also where the original draws it a turn at a time (2026-10-22,
issue #220) and, taken from his list, the helper icons' sizing on a wide screen (2026-10-20, below).

## Taken

| His row | What | Kind | Ported with |
|---|---|---|---|
| dev#60 (his numbering at #528) | The library's three buttons drawn in the 4:3 front-end space | Presentation | PR #528, the temple library |
| — | The light column's tint drawn unlit | Presentation | PR #250, light columns |
| dev#52 | The challenge room's close-look caption sized by the screen's 4:3 width (exact at 800x600); his sharp glyphs not ported yet | Presentation | PR #381, challenge room 3 |
| dev#41 | The front end's swirling backdrop behind the new player's box covers the screen's 4:3 box, centred, black at the sides (the original lays it over the whole screen, which it only ran at 4:3; at 4:3 it is the same) | Presentation | PR #352, the new player's box |
| dev#49 | The player profiles kept in one text file, `Profiles.txt`, in place of the original's registry keys (one per player, the current player named by a value beside them); the folder is `Profiles` in the program's working directory (his: his own user-data folder; his dev#65: the working folder), never the game's folder, and a run of the checks keeps none | Storage (nothing changes in play: the same names, symbols, creatures and settings are kept) | PR #352 / #377, the player profiles |
| dev#60 (his numbering at #525) | The Tech Stats' "Total Objects Created" counts every entity made since the registry's last reset; the original counts every allocation of its game-object heap since the land started (beliefs, agendas, footpaths and file objects too, [engine loop](../engine/loop.md)), which has no one-to-one equivalent here yet; the registry's count is also a debug readout (2026-10-21) | Presentation | PR #525, the Game Statistics box |
| dev#61 (his numbering at #525) | The creatures over the Creature tab's boxes placed and sized in the 800x600 layout space | Presentation | PR #525, the Game Statistics box (its render pass not yet ported) |
| dev#11 | The camera's helper icons by the hand sized from the width of a 4:3 screen as tall as the screen, so that a wide screen does not make them bigger (the original: the focal length across the whole width over 22); chosen by the user on 2026-10-20 | Presentation | his 9b8718f30 (ours 71f47680f), the helper icons by the hand ([player camera](../camera/player-camera.md#the-tricons-what-is-shown)) |

## Kept as vanilla, his fix beside it

These keep the original's behaviour by default. His fix runs only with the **Fix Vanilla Bugs** switch on
(`EngineConfig::fixVanillaBugs`, the debug bar's Debug menu, off by default; user, 2026-10-20): one switch for every
game-logic fix of his that is ported, so the checks, which leave it off, always see the original.

| His row | What | Ported with |
|---|---|---|
| vb#63 | Villagers who hide when a shield falls pop out at once (vanilla's state table); his tree lets them hide 15-30 s | PR #410 |
| — | A hand demo's click that finds a thing it cannot act on zeroes the click's point (vanilla), so the press falls back to the thing nearest the map's corner; his fix keeps the point (behind the switch) | PR #333/#361, the hand demos' click |

## Pending

- Every row is unverified against the frozen v1.42 exe until the fidelity audit of the imported code.
