# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature learning miracles by watching, its reactions to miracles and what it
  thinks its god wants, the mind noticing animals and frightening things, Land 2's other two creatures, and the
  remaining particle rules.
- **Camera and rendering:** the sampler defaults, the mouse ray from the eye, the near plane, the mouse camera (grip,
  pan, edge drags, auto pitch, wheel, both buttons, clear view), the sun's glare, the draw list under raffclar's names.
- **Game logic:** the map interface, the fire and explosion systems, thunder, the remaining key bindings and the
  left-handed option, map-cell filing of the creature and of path walkers, the forest behaviour, the creature going
  with its player to the next land, and picking a tree anywhere in its forest.

Done since the last update: the landscape vortex's drawing, living things following reshaped land, snow, the sky
dome's tint, vertex blends at the creature's seams, the creature's shadow, the magic, particle, gesture, shield,
tornado, teleport and flock interfaces, and the rain, water ring, smoke, sway and field interfaces.

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
