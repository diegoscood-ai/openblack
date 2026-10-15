# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

The port target is raffclar's `master` at `0222f9f5`: of the 161 rows of the port audit, 86 are closed (2026-10-15).

## In progress here (do not start these)

- **Test runs:** the fixed test cursor applied from the first frame (it moves every check's first frame; it goes in
  alone, before the rest).
- **Creature:** script-driven creature actions (play, move and face, do action), the creature's fizz and the
  invisible spell with its drawn body, eyes and hair, the sway a hit drives, the creature's Creed hand glows, the
  footstep dust, eating a magic food pile by portions, the creature's copy posed and drawn in the creature room.
- **Camera:** the camera zones (kept inside the fence and under the height limit), the double click flying to the
  object under the hand, and the player's follow mode after a double click on a creature.
- **Physics:** the Bullet library removed (the camera's land under a pixel is the island's own ray).
- **Villagers and towns:** the fish farm's shoals, scoop and timers; the virtual influence; script natives
  (GET_TOWN_WITH_ID, BELIEF_FOR_PLAYER, SET_FOCUS on objects, IS_ACTIVE, GET_OBJECT_CLICKED, FLOCK_MEMBER,
  OBJECT_DELETE); the football's pick-up reaction; the hand demo's recorded camera hints.
- **Rendering and interface:** the land's level of detail per block, the bodies' light as the original's animated
  objects, the advisors' series, the moon window, the loading screens and start-up logos.
- **Animals:** the birds' way over missing land blocks, the script's POPULATE_CONTAINER.

## Done since the last update (joints 99 to 104)

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
- the debug GUI's menu-bar hiding and mouse-over gating;
- the visible new particle effects (belief sprites, electric arcs, the hand glow, camera-distance scaling);
- an untextured rock on Land 5 (issue #128);
- a wonder built with fences instead of its mesh (issue #129).

The flowers (issue #124) and the hand catching and feeding fireballs (issues #114 to #116) were done on this side;
those issues are being checked and closed.

## Being done by raffclar's agents (pull requests open)

- (none open.)

## Done recently

See `CHANGELOG.md`.
