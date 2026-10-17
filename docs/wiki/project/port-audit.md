# Port audit: raffclar's target build

## Summary

openblack's work follows raffclar's tree (see [Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)).
His `master` at `0222f9f5` is the fixed target (user, 2026-10-11): every piece of that build was audited against our
tree and written down as a **port row**, one thing his build has or does that ours had to take, rename, measure or
match. There are **161 rows**. A row is **closed** when its work is in our tree and verified (same behaviour bit for
bit, or a measured change); it is **open** while any part of it is left. Sub-rows found later and fix rows (fidelity
differences, vanilla bugs, GitHub issues) are not counted here.

His later commits are not audited until this build is reached, except a newer piece that closes one of these rows or
fixes a fidelity difference.

The [progress dashboard](dashboard.md) draws these counts as bars.

## Rows by area

Counted at the joint of 2026-10-16, after the last recount.

<!-- port-audit:table -->
| Area | Rows | Closed | Open |
|---|---:|---:|---:|
| Creature, magic and camera | 47 | 30 | 17 |
| Renderer and interface | 42 | 36 | 6 |
| Villagers, scripts, hand and audio | 45 | 28 | 17 |
| Testbed, physics and animals | 24 | 9 | 15 |
| Coordination | 3 | 2 | 1 |
| **All** | **161** | **105** | **56** |
<!-- /port-audit:table -->

One more row of the renderer, triple buffering against stalls, is parked and not counted.

## Open rows

A row that has an open issue on the fork links to it.

### Creature, magic and camera

- **R24** Shield, teleport and flock components: the testbed proofs are left.
- **R25** The tornado's and the teleport's creature branches: the testbed proofs (a tornado on a creature, a teleport between two points).
- **N2-R-2** The help profile counts the world-camera controls: the camera help events split, events 25 to 33, the flight to the temple.
- **N-R-3** The creature's fire agenda: the remaining steps and their measure.
- **N-R-7** Things carried home fizz out and in, drawn in two passes: coded and reviewed, not in yet.
- **N-R-8** The temple pen's carried-home path: needs N-R-7.
- **N-R-10** The creature carried over to the next land, saved to `Scripts\CreatureMind` with a one-time backup.
- **N-R-13** Leash orders: the double tap, the nod and the marker.
- **R22** Miracle flags in a fight, and effects applied in a fight.
- **R29** The Compassion and Freeze rules and the rest of the spell maths.
- **N-R-6** Mind refinements: urges, remembered plans, fainting from needs, throwing over, pick-up and stomp height limits. Not started.
- **N-R-14** The three temple leashes hang and tumble outside the temple. Not started.
- **N-R-16** Seeds of the four best-learnt miracles hover on the cave plinths: needs the temple's cave-seed feed.
- **N-R-15** A hungry creature fishes at a farm's shoal, or takes fish home or to a pit. Not started.
- **N-R-1** Fight arenas, the fight buttons, gestures, natives and camera: needs N-C-16.
- **N-R-20** Thrown things strike the magic shield: needs N-C-1.
- **N2-R-1** The camera-zone force field: its rules are in, the wiring is left.

### Renderer and interface

- **N2-M-10** The tip bubble: the remaining steps, eight behaviour fixes and the scenario's references.
- **M22** Editor additions: the Creatures tab, the inspector's creature sections, hosting the creature tools.
- **N-M-6** Villager detail by view depth: the level-of-detail wiring is parked, since the original never takes that path.
- **M17** Zoom to places: the code is in but not wired; needs N2-R-2.
- **N-M-9** A body sunk under the sea is drawn in no pass: needs N-C-1.
- **M10** The renderer and shader merge: the footprints, the environment map, the fizz, the pupils, the moon, the seeds, the hand glow and the rest.

### Villagers, scripts, hand and audio

- **N-A-3** The picking service: the camera's use of it, the grip lock, the pick at draw time, the land hiding an object, the hand's feel.
- **N2-A-3** A script state other than walking ends the walk: held for its first part.
- **N-A-11** Fireflies made by the land script, caught for a one-shot miracle: making them and catching them.
- **N-A-6** Influence past the border and the mana path: the worship sites' chants, the hum, the mana path's draw and sparks.
- **N-A-8** Dances: the dance shapes and the remaining steps and fixes.
- **N2-A-7** Dancers' clips keep time across clip changes: needs N-A-8.
- **N-A-7** One worship site per tribe: the rename, the icon overflow, the dance loop.
- **N-A-10** The hand knocks on a building and its people come out: blocked by the disciples.
- **N-C-15** Script natives: the rest of the natives that are not implemented yet.
- **N-C-11** Towns: town artefacts and the homeless.
- **N-C-16** Walks in whole map units, circles nearest first: the last three steps.
- **N-C-12** The story's temples start under construction, with music on finishing. Not started.
- **N-A-1** The hand grab: his grab system, the press and release input, the measured rules.
- **N-A-9** The village totem: the second totem's see-through and its tooltip.
- **N-A-12** Temple destruction: the heart's fade, the shell and plasma beams drawn, and a struck worship site passing the blow on.
- **N-C-22** The football as a town toy: the ball, its creation by script, the villager's pick-up, its removal.
- **Game speed keys** Alt+1 and Alt+2 halve and double the game speed with the original's clamps. Not started.

### Testbed, physics and animals

- **N-C-1** The physics core.
- **N-C-20** Resource caches: videos and advisor models.
- **N-C-3** Living physics rules: thrown people and animals, creature paths, animals fleeing, their lairs, snow dust.
- **N-C-4** Object rules: roots, rock wear, felling, spills: needs N-C-1.
- **N-C-6** Rewards: the chests' physics, after N-C-1.
- **N-C-7** Forests: the growth measure, scenic forests and lairs.
- **N-C-8** Birds, flocks and eating: pits, fields and trees, and two fixes.
- **N-C-19** His Locator slots: the remaining slots, each with its system.
- **N2-C-2** Four more Locator slots: their tracking row.
- **N-C-18** Lazy loading and paced uploads to the graphics card.
- **Testbed video option** His `--play-video` launch option. Not started.
- **N-B-5** His missing tests: the remaining passes.
- **N2-B-1** His 29 new tests.
- **N-B-6** The injected world seams, so each system is tested on fakes: with each row.
- **N-B-3** His testbed scenario files: nature, fishing for a town, script highlights and others.

### Coordination

- **N2-C-3** The orphan tests and the game start-up hunks that go with their rows.
