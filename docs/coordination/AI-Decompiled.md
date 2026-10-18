# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

Since 2026-10-17 raffclar's newer master (after `0222f9f5`) is brought in lane by lane, as he proposed: each lane's
new files are taken, its hooks refitted onto this tree, and the commit says "Ported from raffclar/openblack PR #N"
(unverified against v1.42 until the fidelity audit). Conflicts where ours wins are listed for review. The dashboard
counts the share of his gain brought in. The 0222f9f5 port audit: 105 of 161 rows closed.

## In progress here (do not start these)

- **Creature:** the turn on the spot as the original's spins, catching what is thrown, the lean onto the slope, the
  eye blink on the local stream, the alignment sparkles, the falling-spell film, eating from fields, trees and stores.
- **Villagers and towns:** the walk round obstacles (the wall hug and its circles), footpaths, the temple's
  destruction, the first clip of a new living, the trees' tug and replanting, villagers fleeing from predators.
- **Physics:** the "in the physics" tag with hanging objects, fragments coming to rest, the dead list's deferral, the
  vortex's pull.
- **Rendering and interface:** the totem's gate and light, the draw order of the object list's tail.
- **Animals:** the detours round obstacles, the birds' first clip.

## Done since the last update

- joint121 (411e7efb0, published 14b9786b0): his #605 the desire to steal (a thing taken to the creature's temple),
  #616 the joint actions of two friends, #631 a creature throwing a ball into the sea, and #547 what an action is
  done to (the planner's objects: a holding creature examines what it holds).
- joint120 (64aa13849, published 04d03a448): his #524 odd actions and five creature natives, #543 the altar
  sacrifice, #567 creature script natives, and our exe fixes to six planner rows (built pits only, what can be
  examined or caught, breeders, totems).
- joint119 (bb336271e, published 6d6cccf55): his #469 creature casting (with #467), #432 reactions to creatures
  and flying things, #381 the challenge room (records, pictures on the walls, markers) and our cave copy-pick and
  tattoos (#349).
- joint118 (499940f7d, published da6682ae0): his #419 what the creature picks up and uses (things by kind, targets,
  mushrooms) and #517 the creature helping a town (14 actions); and our fix from the exe: getting high is only done
  to a magic mushroom (his row let a creature pick up and eat villagers or a ball).
- joint117 (905c06601, published f1edb5a86): his #229 training natives, #346 development stages, #363 the reward
  chest, #301 villager reproduction, #266 graveyard skeletons, #645/#646 computer players and creatures from mind
  files, and our group #74 footpaths from the exe (villagers walk the footpaths of their buildings). 39% of his newer
  work is in; the whole game stands at 47%.
- joint116 (b557b6d7b, published 6ae4dd5db): his #348 the hand with the creature (stroke and slap, the interaction
  camera, fight and status panels), #513 the creature's eleven attention actions, #488/#520 dances; and our
  reactions' per-turn re-offer from the exe (villagers now take up reactions they walk into).
- joint115 (5924534da, published dc4354776): his #353 creature help (the good advisor tells of your creature),
  #236 and #310 disciples' trades, craftsmen, missionaries and moving town on landing, an always-on frame counter,
  and our fix for a heal told by the debug tools.
- joint114 (b598f2254, published 74e05d11f): his #371 the world's statistics in the temple, #378 challenge
  pictures, #509 creature rest and home, #261 disciples in the hand; our fixes for script property 21 and the creature
  cave's attention. 19% of his newer work is in.
- joint113 (3cbdc1247, published 79f39035c): his #275 town desire flags, #410 villagers hiding or panicking when a
  shield falls, #250 light columns over towns and temples, #387 giving things to the creature, #528 the temple
  library, #401 the Help key and guidance remarks; with two of our own guidance fixes (the moon wait, the remarks
  gate). 14% of his newer work is in.
- joint112 (0422c84b0, published 18172f482): his #522, the creature's map of the land (regions, eight exploring
  actions), and his #278, artefacts (gently placed objects join a town; villagers dance round them; worth, belief
  and glow). The dashboard counts how much of his newer work is in (7%).

## Lanes of his being ported now

- Next: #524, #543, #605, #547, #567, #525 and our exe fixes; then joints of their own for the per-desire object
  test, the creature's two decision trees, #267, the shepherds, #417 and #529.

## Done before (joints 99 to 111)

- tools and notes (6d5bd1d35): our reverse-engineering tools published in tools/re/ (xref, oracle, precheck, hashdiff)
  with usage docs; the wiki states that the original is v1.42 with the Infinite Drawing Distance and Creature Growth
  community patches.
- wiki rebuilt (757301889): the research wiki now lives in docs/wiki/ by game system, read through one viewer page
  (docs/index.html, or docs/open_wiki.bat locally) with a progress dashboard; docs/progress moved to
  docs/wiki/project/progress/ and docs/bw1-notes and docs/wiki-html are gone.
- joint111: villagers walking round obstacles on the original's circles; animals leaving a circle whose thing left
  the map; the ambience loops fading in (they started silent); pause, dead voices, volcano and small sound rules;
  the temple's challenge list, room and picture click; the creature's motes, eyelids, falling-film copy, its body
  posed from the turn's layer times and the hand resting on it; the tree tug as the original's spring; the audio
  device owned with RAII; the spirit meshes through the cache.
- joint110: the creature's eyes blinking on the game's local random stream at its turn's end; its per-turn sound
  queue; the advisors' animation sounds, the fight music and the heartbeat; the windmill's hum and the creche's
  children; the missionaries' song with its karaoke box; animals' reaction turns and landscape circles; sharks'
  fins above the water; the replanted tree's mimicry and forest spot.
- joint109: the villagers' walk aimed and set up as the original's wall hug; the footpath walk rules; the
  temple's heart destroyed and passing its blows on; the creature's turn on the spot, lean onto the slope, catch of
  thrown things and scripted focus; lightning arcs over what a bolt strikes; hanging objects and fragments coming
  to rest; a new villager's first clip; the totem's gate and light; the object list's tail in the original's order.
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
