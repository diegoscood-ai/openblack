# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature's reactions to miracles and what it thinks its god wants, the mind
  reacting to predators and fire, Land 2's other two creatures, the lightning strike's particles, and the remaining
  particle rules.
- **Camera and rendering:** the cloud mist frames, the street lanterns' flicker, the snow and rain placed at start-up,
  and the rest of the draw list under raffclar's names.
- **Game logic:** picking a tree by its drawn pixels (the leaves' clear parts don't count), town aggression, villagers
  walking round a fire, fire attacking a town, and belief from miracles.

Done since the last update: the mouse ray from the eye and the mouse camera, the creature fight camera, the file
dialog and debug file browser, the map interface, fire and explosions, thunder, the remaining key bindings, map-cell
filing of the creature and of path walkers, the forest behaviour, the creature going with its player to the next land
(its mind and physique saved at the land change), the creature learning by watching, its perceived desires and the
temple scroll, animals as things it looks at, and the LOAD_CREATURE, SET_CREATURE_NAME and CREATURE_AUTOSCALE script
commands.

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
