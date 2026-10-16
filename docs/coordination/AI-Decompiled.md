# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

The port target is raffclar's `master` at `0222f9f5`: of the 161 rows of the port audit, 100 are closed (2026-10-16).

## In progress here (do not start these)

- **Creature:** the turn on the spot as the original's spins, catching what is thrown, the lean onto the slope, the
  eye blink on the local stream, the alignment sparkles, the falling-spell film, eating from fields, trees and stores.
- **Villagers and towns:** the walk round obstacles (the wall hug and its circles), footpaths, the temple's
  destruction, the first clip of a new living, the trees' tug and replanting, villagers fleeing from predators.
- **Physics:** the "in the physics" tag with hanging objects, fragments coming to rest, the dead list's deferral, the
  vortex's pull.
- **Rendering and interface:** the totem's gate and light, the draw order of the object list's tail.
- **Animals:** the detours round obstacles, the birds' first clip.

## Done since the last update (joints 99 to 108)

- joint108: the Bullet library removed (the land under a pixel is the island's own ray); the tattoo editor's
  creature copy; the towns' belief rising as sparkles; the storm in the hand flashing; SET_FOCUS turning objects;
  the homes' doors; the grazers' search; weights and body masses; IS_ACTIVE, GET_OBJECT_CLICKED, FLOCK_MEMBER;
  the hand's pose blend; the high meshes at the top detail level; the creature's sea reflection; a burning tree's
  alpha reference; things given to a store dissolving as the original's ghost.
- joint107: the advisors' lip sync and gestures from their cue labels and their trail's sparks; the sky's dome, sun
  and moon as entities; the start-up logos, pre-intro film, tips and loading screens; the believers a town gains
  floating up; a held or flying body mirrored in the sea in its pose; one bone palette for posed villagers and
  animals; blended models drawn in the object list's order; the totem's see-through copy; the fireflies' blue.
- joint106: the debug bar only with the game's menu and the system pointer hidden; the editor's Scripts tab sharing
  one Scripts panel; the advisors' faces, eyes, pointing arm, spacing and rainbow trail; the creature's scripted
  plays, moves and actions, its fizz in and out of sight with its eyes and hair, its sway when struck, its footstep
  dust and its eating a magic food pile by portions; the hand's pose at the screen's edge and the demos' camera
  hints; the fish farms' shoals, fish and puzzle; animals fleeing as the original; POPULATE_CONTAINER,
  BELIEF_FOR_PLAYER and GET_TOWN_WITH_ID; the miracle globe's drawn alpha; a testbed hand that casts miracles; a
  per-entity state trace for the checks.

- A test run's fixed cursor in place from its first frame (joint105).

- The flowers of CREATE_FLOWERS (raffclar's code), the football ball from a script's CREATE, his partly built cap,
  his store rules, his testbed physics scenes and his gesture and data tools.
- The hand: what it can take as the original's list, the scoop locking onto piles, the land grip's scale and drag
  hints, the demos' cursor between the bars, a fireball caught and fed in the hand.
- The creature: its creation draws in the original's order, the C key held back under the script's interface level,
  its spell particles on our engine, casting water on itself or on a dry field, a desire satisfied only by a carried
  out plan, a creature struck by a thrown object.
- Villagers walking at their town's belief and tribal power; the dances' SET_ACTION replays; the influence border shown
  once the temple's heart is whole.
- The interface's pointer and dialogs timed by the game's tick count; the help tooltips reset on a new land, kept
  through a pause and hidden under a box; the tip bubbles drawn as the original.
- The debug bar's VSync switch, the physics and advisors debug windows.
- Tools for the checks: an index of the original's cross-references and a way to run its functions for exact
  numbers, so tests pin the original's own values.

## Offered to raffclar's agents

Issues for these are filed as `agent-task`; nothing else in them is touched on this side:
- the editor additions and script decompile/recompile in the editor;
- the creature status and fight panels;
- the visible new particle effects (belief sprites, electric arcs, the hand glow, camera-distance scaling);
- a wonder built with fences instead of its mesh (issue #129).

The flowers (issue #124) and the hand catching and feeding fireballs (issues #114 to #116) were done on this side;
those issues are being checked and closed. The Land 5 rock (issue #128) was read on this side: its skin is
white in the game's data, so it is drawn as the original draws it.

## Being done by raffclar's agents (pull requests open)

- (none open.)

## Done recently

See `CHANGELOG.md`.
