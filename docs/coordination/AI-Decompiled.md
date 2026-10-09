# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature's reactions to miracles and what it thinks its god wants, the mind
  reacting to predators and fire, Land 2's other two creatures, the lightning strike's particles, the remaining
  particle rules, and the creature's real radius (the size every routine reads for its distance to the edge).
- **Camera and rendering:** the font cache's start-up table, the clouds built when the landscape opens, the fight
  camera's fly-to, the camera flights (their midpoint, cancelling on input, the double click's best angle), and the rest of the draw list under raffclar's names.
- **Game logic:** picking a tree by its drawn pixels (the leaves' clear parts don't count), town aggression, villagers
  walking round a fire, fire attacking a town, and belief from miracles.

Done since the last update: the cloud mist frames, the street lanterns' flicker only in the dark, the snow and rain
placed once at start-up, the creature running from what frightens it, and a creature loaded from its file keeping the
rates at which its desires fade.

## Offered to raffclar's agents

Issues for these are filed as `agent-task`; nothing else in them is touched on this side:
- the editor additions and script decompile/recompile in the editor;
- the testbed core with its flat land, and the testbed scenarios;
- the creature status and fight panels;
- the debug GUI's menu-bar hiding and mouse-over gating;
- the temple leash posts;
- the hand catching and feeding fireballs;
- the visible new particle effects (belief sprites, electric arcs, the gesture light sheet, the hand glow,
  camera-distance scaling);
- villagers fleeing from and watching miracles;
- flowers;
- an untextured rock on Land 5 (issue #128);
- a wonder built with fences instead of its mesh (issue #129).

## Being done by raffclar's agents (pull requests open)

- (none open; pull request #4, physics, was closed without merging. Its branch is kept here for reference.)

## Done recently

See `CHANGELOG.md`.
