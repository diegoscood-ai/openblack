# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

## In progress here (do not start these)

- **Creature, magic, particles:** the creature's body without its mind: its size in the pen, its per-turn pose
  (with the head, hands and feet scaled by its size), its feet on the ground, growth, fatness and alignment, its
  needs, and a creature casting a miracle; Land 2's other two creatures; the remaining particle rules (objects as
  emitters, the creature-spell particles).
- **Camera and rendering:** the creature's leash posts and the rest of the leash (lengths, walk-back, moods, tug,
  tying, the rope's speed, issue #135), and the miracles' camera takes.
- **Testbed:** more scenario fixtures (a temple with the creature's pen); the per-consumer draw hooks of the
  miracle effects (fragments, tribal power, shield domes, globes, and an empty hook for the hand glow); and the gesture light sheet (issues
  #120 and #121, taken back on this side).
- **Object draw list:** the original's per-frame object draw list (block cull, rebuild rule, on-screen test), which
  steps the script highlight's glints.
- **Game logic:** the hand's part in the leash and the posts, and the hand and pick-up issues.

Done since the last update: the testbed (issues #101-#104 and #106) with five example scenarios, a thrown
object leaving from the hand's drawn pose (#134), a thrown miracle bubble turning about its centre (#133), the
creature's fatness, needs and age carried to the next land, its radius and life, the miracles that reach it, the
forest miracle's camera take, the stray lightning-strike effect removed (it ran every turn), the held miracle's release
(a short click no longer leaves the spell running), the leash rope stepping while paused, villagers fleeing from and
watching miracles and walking round fires, the hand placed on the original's mesh hit, the fight camera's ease and
keyboard skip, the camera path driven per frame, and the game pausing while minimised.

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
