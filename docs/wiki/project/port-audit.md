# Port audit: raffclar's target build

## Summary

openblack's work follows raffclar's tree (see [Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)).
His `master` at `0222f9f5` was the fixed target (user, 2026-10-11): every piece of that build was audited against our
tree and written down as a **port row**, one thing his build has or does that ours had to take, rename, measure or
match. There are **161 rows**. A row is **closed** when its work is in our tree and verified (same behaviour bit for
bit, or a measured change); it is **open** while any part of it is left. Sub-rows found later and fix rows (fidelity
differences, vanilla bugs, GitHub issues) are not counted here.

Since 2026-10-20 the fixed target no longer leads the work (the user's scope change): the goal is to recreate the
original as fast as possible, and only a piece of his that closes one of our open rows or systems, saves us work on
one, or fixes a fidelity difference is taken, measured against the exe
([Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)).

This audit is kept as history. The dashboard now measures raffclar's work against his current `master`, pull
request by pull request, in [raffclar's work in our tree](raffclar-status.md); these rows are part of its evidence.

## Rows by area

Counted at the joint of 2026-10-16 and recounted on 2026-10-24 (joint206) against the tree: N-R-7 (things carried
home fizz out and in), N-R-10 (the creature carried over to the next land), N-M-9 (a sunk body drawn in no pass) and
the game speed keys have closed since. Every open row below says what is in and what is left at joint206.

<!-- port-audit:table -->
| Area | Rows | Closed | Open |
|---|---:|---:|---:|
| Creature, magic and camera | 47 | 32 | 15 |
| Renderer and interface | 42 | 37 | 5 |
| Villagers, scripts, hand and audio | 45 | 29 | 16 |
| Testbed, physics and animals | 24 | 9 | 15 |
| Coordination | 3 | 2 | 1 |
| **All** | **161** | **109** | **52** |
<!-- /port-audit:table -->

One more row of the renderer, triple buffering against stalls, is parked and not counted.

## Open rows

A row that has an open issue on the fork links to it.

### Creature, magic and camera

- **R24** Shield, teleport and flock components: in under his names, with their testbed scenarios
  (`miracles.shield_*`, `miracles.flock_*`); the scenarios' reference runs are left.
- **R25** The tornado's and the teleport's creature branches: a tornado faints a creature where it lies (in); the
  creature's walk through the teleport stones is left.
- **N2-R-2** The help profile counts the world-camera controls: turns, tilts, zooms, double clicks and drags are
  counted with the original's input mask (events 25 to 33); the flight to the temple (event 32) is not reported yet.
- **N-R-3** The creature's fire agenda: the remaining steps and their measure. Row 238 `PutOutFireWithMagicWater` and
  `Target::Burning` are in, and the pure `creature_fire` rules are wired to the fire's reaction (2026-10-30). Left: row
  37 `PutOutFire` (the douse), `SetFireToObject` with `Target::Unburnt` and `StartFire`.
- **N-R-8** The temple pen's carried-home path: a knocked-out creature is carried to its home or the temple's pen
  point (ours, as the original's sub-action 112); whether his also pens it on arrival is not checked.
- **N-R-13** Leash orders: the double tap, the nod and the marker. A second tap's run and wait, the mark at the point
  of a land order and the acknowledging sound are in (ours, from the exe); the creature's acknowledging gesture, the
  mark at a thing and the land's other orders are not ported yet
  ([The leash](../creature/leash.md#pending)).
- **R22** Miracle flags in a fight, and effects applied in a fight: in a fight a creature takes no harmless effect
  from another and its life does not change (in); the fight's own part (blocking, reeling, fight health, knock-out)
  and the fight's miracle flags are left.
- **R29** The Compassion and Freeze rules and the rest of the spell maths: the spells' particle maths and the freeze's
  stop are in; the hold on the other desires and the freeze's sheen and shadow are left.
- **N-R-6** Mind refinements: a remembered plan's lessons and fainting from needs are in; urges, throwing over, and
  the pick-up and stomp height limits are not started.
- **N-R-14** The three temple leashes: the posts and their collars are drawn and turn outside the temple, as the
  original (ours, from the exe); his hang-and-tumble model is not taken.
- **N-R-16** Seeds of the four best-learnt miracles hover in the creature cave: not in our tree, and not yet read in
  the original (the medals on the plinths are in).
- **N-R-15** A hungry creature fishes at a farm's shoal, or takes fish home or to a pit. Not started.
- **N-R-1** Fight arenas, the fight buttons, gestures, natives and camera: the arenas and the watching camera are in
  part; the arena's marking, reuse and spectators, the fight icons and the hand's clicks in a fight are left (needs
  N-C-16).
- **N-R-20** Thrown things strike the physical shield, bounce off and cost it power (ours); a very fast rock passes
  its shell, and his physics core (N-C-1) is not in.
- **N2-R-1** The camera-zone force field: the fence keeps the camera inside with its shake and sound (in); the force
  field drawn and its pulse, and the push out of exclusion domes, are left.

### Renderer and interface

- **N2-M-10** The tip bubble: the steps, the eight behaviour fixes and its scenarios (`help.tip_bubble`,
  `help.tip_bubble_edge`) are in; the scenarios' reference runs are left.
- **M22** Editor additions: creatures in the palette and the Scripts tab are in; the Creatures tab, the inspector's
  creature sections and hosting the creature tools are left.
- **N-M-6** Villager detail by view depth: closed, not in the original. This exe draws every model's high mesh at any
  depth, so raffclar's unwired mesh detail was removed
  ([not in the original](not-in-the-original.md#renderinglevel_of_detail)); the high meshes are drawn only at the
  top detail level, a chosen difference.
- **M17** Zoom to places: the temple and realm keys fly the camera there (in); the flight's 0.4 s, the temple's
  rounding, the camera mode switch and the camera help's report are left (N2-R-2).
- **M10** The renderer and shader merge: the footprints, the fizz, the advisors' pupils and the static shadows are
  drawn; the environment map, the moon extras, the hand glow, the frustum and the upload pacer are left (mesh detail is not
  in the original, N-M-6).

### Villagers, scripts, hand and audio

- **N-A-3** The picking service: the camera's use and the hand's feel are in; the pick at draw time
  (`PickUnderCursor`), with the grip lock and the land hiding an object, is left (the hand still picks along its own
  ray).
- **N2-A-3** A script state other than walking ends the walk: held; ours keeps the walk, which goes on only in the
  walking states.
- **N-A-11** Fireflies made by the land script: catching one for a one-shot miracle is in; the script's
  `CREATE_FIRE_FLY` is left.
- **N-A-6** Influence past the border and the mana path: the worship sites' chants, the hum, the mana path's draw and
  sparks.
- **N-A-8** Dances: the dance shapes from the dance files, the worship dances (a ring stands in), the moves between
  groups, the lights and the camera.
- **N2-A-7** Dancers' clips keep time across clip changes: needs N-A-8.
- **N-A-7** One worship site per tribe: the rename, the icon overflow, the dance loop (its slot is declared, not
  served).
- **N-A-10** The hand knocks on a building and its people come out: in; a disciple held near another house does not
  start the read-out yet.
- **N-C-15** Script natives: the rest not implemented yet (60 stubs of 464).
- **N-C-11** Towns: the artefacts and the homeless are in; left: an artefact going with its dance and glow, its trip
  through a vortex, and the check against v1.42.
- **N-C-16** Walks in whole map units, circles nearest first: the last three steps.
- **N-C-12** The story's temples start under construction: Land 1's is built by its villagers from the plan (ours,
  from the exe); Land 2's 30%-built start is not seen in play, and the music on finishing is not read.
- **N-A-1** The hand grab: his slot, interface and pure rules are in, served by our hand; his grab system and world,
  the press and release input, and the rules that stay ours are left.
- **N-A-9** The village totem: the see-through second totem and its tooltip are in; the grey share floating over it
  for five seconds is left.
- **N-A-12** Temple destruction: the heart's fade and its shell are drawn (ours, from the exe); the plasma beams and a
  struck worship site passing the blow on are left.
- **N-C-22** The football as a town toy: the ball's physics body and a script's football are in; the pitch making its
  ball, a villager fetching it and its removal are left.

### Testbed, physics and animals

- **N-C-1** The physics core: ours behind his interface; his turn rules are in, his point-cloud bodies, materials
  and pair rules are not taken.
- **N-C-20** Resource caches: videos and advisor models.
- **N-C-3** Living physics rules: thrown people and animals and the snow dust are in; animals fleeing what flies,
  their lairs and creature paths are left.
- **N-C-4** Object rules: rocks wear and uprooted trees show their roots in ours; his roots, felling and rock wear
  rules need N-C-1.
- **N-C-6** Rewards: the chests' opening is in; the chests' physics, after N-C-1.
- **N-C-7** Forests: the growth measure, scenic forests and lairs.
- **N-C-8** Birds, flocks and eating: eating from pits, fields and trees is in; the rest of a meal's end (fish farms
  eaten in place, what an eaten thing leaves) is left.
- **N-C-19** His Locator slots: all declared; eight are not served by a system yet (creature carry-over, creature
  pen, fireflies, fish farms, hand demos, the inspector, script highlights, worship sites).
- **N2-C-2** Four more Locator slots: intro, help profile and tip bubble are served; the hand demos' slot is not yet.
- **N-C-18** Lazy loading and paced uploads: his upload pacer is in but not called; his background load queue is not
  ported.
- **Testbed video option** His `--play-video` launch option. Not started.
- **N-B-5** His missing tests: 97 of his 330 test files are not in by name, some covered under our names.
- **N2-B-1** His 29 new tests: 21 are in; test_tip_bubble, test_villager_eyes, test_camera_help_events,
  test_force_field, test_walker_placement, test_clip_sound_player, test_clip_sounds and test_sound_tags are left.
- **N-B-6** The injected world seams: five of his ten are in (building damage, dynamics, physics hooks, script
  objects, temple destruction); the firefly, hand grab, script highlight, village totem and worship site seams are
  left.
- **N-B-3** His testbed scenario files: nine of his eleven are in; Nature and the script highlights, and fishing for a
  town, are left.

### Coordination

- **N2-C-3** The orphan tests and the start-up hunks: test_creature_mode and the intro's update are in; the hand-demo
  wiring waits for its slot (N2-C-2).
