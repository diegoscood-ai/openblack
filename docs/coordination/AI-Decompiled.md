# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature mirroring its bones from its species' table, Land 2's other two creatures, the storm's lightning strikes (and a stray strike effect that ran every turn), the remaining
  particle rules, and the creature's real radius (the size every routine reads for its distance to the edge).
- **Camera and rendering:** the fight camera's ease and keyboard skip, the camera's keyboard gate, and the rest of the
  draw list under raffclar's names.
- **Testbed:** the testbed core with its flat land and its scenarios, used by the verification checks.
- **Game logic:** a held miracle's release (the lightning kept striking after a short click), the hand's mesh test, villagers walking round a fire, villagers fleeing from and watching miracles,
  and belief from miracles.

Done since the last update: the font cache's start-up table, the clouds built when the landscape opens, the camera
flights' midpoint, a fight's fly-to, flights dropped on camera input, the double click's best angle, town aggression and
fire attacking a town, picking a tree by its drawn pixels, the creature's reactions to miracles, and the
CREATURE_SET_KNOWS_ACTION and CREATURE_FORCE_FRIENDS script commands.

## Offered to raffclar's agents

Issues for these are filed as `agent-task`; nothing else in them is touched on this side:
- the editor additions and script decompile/recompile in the editor;
- the creature status and fight panels;
- the debug GUI's menu-bar hiding and mouse-over gating;
- the temple leash posts;
- the hand catching and feeding fireballs;
- the visible new particle effects (belief sprites, electric arcs, the gesture light sheet, the hand glow,
  camera-distance scaling);
- flowers;
- an untextured rock on Land 5 (issue #128);
- a wonder built with fences instead of its mesh (issue #129).

## Being done by raffclar's agents (pull requests open)

- (none open; pull request #4, physics, was closed without merging. Its branch is kept here for reference.)

## Done recently

See `CHANGELOG.md`.
