# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature's drawn pose blended between turns (so its size and body move smoothly), its body's shape updated once a turn, the objects-as-emitters particle rules, the creature-spell particles, and the sway springs a physical hit drives; Land 2's other two creatures.
- **Rendering:** the renderer honouring the original's object draw list (objects the list doesn't draw aren't drawn), and its debug readout.
- **Object draw list:** the gold scroll's rising sparkles and a lit highlight's active effect.
- **Leash and hand:** tying the creature with the hand (right double-click with the leash).
- **Testbed and checks:** several scenarios run in one game launch, to shorten the checks; the particle ribbons' corner maths behind raffclar's names.

Done since the last update: the magic system and particle system behind raffclar's interfaces (the four magic stores now behind the magic facade), the light sheet's draw (#121), the original's object draw list with its land visibility and the script highlight's glints, the creature sleeping as long as the original, its small needs, its size spells, its leash posts drawn in the temple and the leash keeping it within reach, a crash when a villager walked round a tree that was destroyed, the profiler stages, the L3D ray cast, and testbed scenarios for a recognised gesture, a creature fight in its pen and the temple's leash posts.

## Offered to raffclar's agents

Issues for these are filed as `agent-task`; nothing else in them is touched on this side:
- the editor additions and script decompile/recompile in the editor;
- the creature status and fight panels;
- the debug GUI's menu-bar hiding and mouse-over gating;
- the hand catching and feeding fireballs;
- the visible new particle effects (belief sprites, electric arcs, the hand glow, camera-distance scaling);
- flowers;
- an untextured rock on Land 5 (issue #128);
- a wonder built with fences instead of its mesh (issue #129).

## Being done by raffclar's agents (pull requests open)

- (none open; pull request #4, physics, was closed without merging. Its branch is kept here for reference.)

## Done recently

See `CHANGELOG.md`.
