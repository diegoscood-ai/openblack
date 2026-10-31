# AI-Decompiled: what this side is working on

Kept up to date by the maintainer's agent. Before starting work in an area, check that it is not listed under
"In progress here". The areas under "Offered to raffclar's agents" are not touched on this side; their issues are
filed on this repository with the `agent-task` label.

The source of truth is the maintainer's local repository; `AI-Decompiled` is updated when the maintainer publishes.
Pull requests are reviewed here, imported locally, verified with the full fidelity checks (they need the original
game's data) and land in `AI-Decompiled` with the next publish, after which they are closed.

Since 2026-10-20 the goal is to recreate the original as fast as possible, on raffclar's architecture; the two
trees are not merged, and his master is no longer brought in lane by lane. A piece of his is taken when it closes one
of our open rows or systems, saves us work on one, or fixes a difference from the original: by cherry-pick with his
authorship (`-x`), or as his files refitted onto this tree, with "Ported from raffclar/openblack PR #N" in the commit,
and it is measured against the original like any of our commits. Where his version and ours differ, the one that
matches the original wins (ours when that can't be told); each choice is listed for review. The dashboard checks
every pull request merged into his master against this tree (at his `e1cc0b8f` of 2026-10-22: 132 in, 59 partial,
570 pending).

## In progress here (do not start these)

- **Rendering:** the prints, static shadows, river channels and snow baked into the block textures and their
  tiles, as the original (#222).
- **Creature:** nothing in its code or mind tonight. Kept on this side: the lean onto the slope (#35), the
  falling-spell film (#23).
- **Villagers and towns:** footpaths (#74), the trees' tug and replanting (#87).
- **Physics:** the dead list's deferral (#8), the vortex between lands (#58).
- **Animals:** the detours round obstacles (#82), a new animal's first clip, the birds' included (#12).
- **Hand:** the glow while it holds a miracle and its flash on a recognised gesture (#122), waiting on the hand's
  second skinned pass.

Only the clean-up is worked on tonight; the other items stay on this side.

## Done since the last update

- joint275 (9f95b4854, FULL + G3): a creature's walk is cut short by the arrival ring's inside, not its outside, so it
  walks onto its point, as the original (fid, cycle, story, hands, 141 scenes move from Land 1 turn 62); what moves is
  drawn between turns as the original, with smooth drawing as an off-by-default debug switch (20 shots, G3).
- joint276 (245a1ec0a): Land 2's rival god's mind takes its turn, picks its aims and runs its chosen actions with the
  hand (fetching food, wood and villagers from other towns), as the original; fid, cycle, story and hands identical,
  the moves only in Land 2's quest runs from turn 1400.
- joint277 (5af8ddd13): the saves' writers and readers for the computer players, the influence rings and other
  parts (no caller yet); the hand's straight pick-up from a field or fish farm and the pile's grab time, as the
  original; quest testbeds and timelines for Lands 1-4 with their closing docs (59%); the work in progress recorded
  on its wiki pages. Build, 10 test groups, fid, cycle, story and hands identical.
- joint278 (c607b3526): a hand demo feeds its recorded moves to the gestures, so a demo's circle casts; a script's
  search for a shield finds one within its own radius; the miracle reactions' switch and shield bearings; Land 2 quest
  testbeds. Build, 8 test groups, fid, cycle, story, hands and 22 scenes identical (2 new scenes).
- joint279 (0e3318ff4, docs and tests): the quests' natives read against the original, the wiki's pages and the
  creature, hand, physics, gesture, animal and miracle progress rows brought up to date with the code (87 duplicate
  rows merged); the exe byte-identical to joint278's.
- joint280 (cf35d73f3): the creature's route planning as the original (longest route 2*d+5000 with no time limit,
  a failed search refusing, a new point handed to the follower on its way, a fresh follower from standing, the search
  turns) and a script's move carrying a walk on; the wiki's pages and every progress row checked against the code
  (score 58%, 249+87 duplicate rows merged). fid, cycle and hands identical; the story and 11 scenes move, each named
  (4 of them from joint278's reaction switch).
- joint281 (51d0e0b26): an arena's fight reaction spreads from the script's place; a frozen creature is drawn in the
  original's frozen tint; CALL and CALL_NEAR pick the newest creature; raffclar's status table classified to #1009.
  fid, cycle, story, hands and 30 scenes identical but the two freeze shots. G3 moved by joint280's routes (named).
- joint282 (b61c2db71): about thirty quest natives as the original (GET_TARGET_RELATIVE_POS, SET_POSITION, Flying and
  Speed, a mind file's tattoos and the creature fizzing in, an animal's script move, alignment, growing-up stage and
  the swap of minds, the field of view and the camera test), the creatures searching a route sharing 32 search turns
  (raffclar's, the story and the day land view move from it, named); Land 2's quest rows closed and the natives' rows
  counted per use (60%). fid, cycle, hands and 152 scenes identical.
- joint283 (1851f3ee2): more quest natives as the original (special effects at a place or object, developer function
  11, the static hold, a script's release, miracles' from points, the shared look-up refusing dying and deleted things,
  reaction followers' join stamps and save), Khazar's challenges closed against a hand-played run, every duplicated
  progress row merged and Land 2's citations made stable (61%). fid, cycle, story, hands, G3 and 155 scenes identical.
- joint284 (a1dcea1ba): object placements as the original (an abode's entrance points from its map-position matrix,
  the builders' ring, the draw-list sphere), raffclar's leash refits (#990, #989), a town's or flock's speed set on
  each member and a creature eating alive as the original, a citadel heart's snow fading, miracles sparing what a
  script holds, lessons limiting the hand, a town's new miracle above its chest. Cycle (Land 2 t1, last bits) and the
  story move by the entrance points, 3 scenes by #989 and eating; each named.
- joint285 (2c9ca39fb): the last stub natives as the original (screen texts and their colour and fade from raffclar,
  SET_ANIMATION_MODIFY from raffclar, the clipping window, CALL_FLYING, GET_DEAD_LIVING, LEARN_EVERYTHING and others),
  the hand's pick and feel through an object's placement, a script's move of a static object leaving its placement,
  a joint action's partner sizing the size spells. Everything identical; 3 scenes joint284 moved and did not run
  (eating alive, the entrance points) promoted.
- joint286 (4c4083d0b): raffclar's fight refit (#994: the blow's force, the computer's wait, the computer attacking a
  casting opponent and drawing its lot only when it can), the lightning bolt without our straight-down guard, a script
  reading a spell's or seed's strength, indestructible members and the spells and foresters respecting them. Four
  fight scenes move by #994's third commit, named; the rest identical.
- joint287 (95a5e2253): raffclar's particle draw order (#952) refitted: a willow wisp draws its point in the ball before
  its speed, the sphere-surface tracer draws three angles, a cast's velocity starts from its parent. 51 miracle scenes, the
  cycle and the story move (Land 2 turn 2, random), each commit's moves named.
- joint288 (e1df7b7a7, FULL): the creature walks along its route's own heading and corners, steps off following
  the route's turns a turn behind its follower, and faces its opponent outright in a fight, with the original's exact
  angle wraps; a creature's static action draws for its mirror on every start try; what stands where it is placed is
  drawn with its own placement; the challenge record natives read against the original (12 quest rows). 11,194 tests;
  fid, cycle, story, hands, the day land view and 169 scenes move at Land 1 t62 (heading) and t70 (the mirror draw).
- joint289 (570907760): every advisor line's recording pinned per land against the original's rule, the Consciences window
  forcing a spirit's voice (debug), the Pied Piper quest run on a newborn creature (the original stalls a creature of
  size 1.6 or more in the crèche ring), the growth constant, a forced stomp-and-eat for a script, and three natives'
  stale marks closed. Everything identical.
- joint290 (cbc47d085): CREATE puts storms, puzzles, dispensers and vortices on the land as the original (Land 2's way-in
  vortex no longer makes arriving villagers 77 m underground, so 30 of them no longer die), GET_POSITION at and near the
  goal with its altitude, the puzzle delete, IsObject, the flock and dance deletion, raffclar's mode-1 ghost, and a
  scene forcing a creature's stomp-and-eat. fid, cycle and hands move at Land 1 t28 (the vortex's y), the story on Land 2.
- joint291 (db3728207, FULL, new base): a thing marked indestructible survives a killing blow, and a creature spits
  such a meal out whole; a challenge picture is of the frame already on screen; Land 2 quest rows closed from two
  hand-played runs (Worship Site, Workshop, Khazar's fireball and shield), the Pied Piper's good ending; every area's
  pages brought up to date. 11,227 tests; every state, shot, hand demo and the story identical.
- joint292 (5c2ca428f, FULL): the land's far texture band is blended with its tile once, as the original (the
  pale veil over Land 2 is gone; the land views by day and night renewed); a worship chant readout; script property
  natives for towns, flocks, dances, scale, height and age. 11,243 tests; every state and scene identical.
- joint293 (037f18598, FULL, 54 commits): from the user's Land 1 hand test and the breadth plan: worship deaths
  counted; reward chests tapped; a refused pick-up no longer sticks in the hand; demo icons follow the demo's cursor;
  the food demo's tap; the creature takes what the leash taps, with the target marker (raffclar refit); villagers
  impressed by magic trees, things looked at, a held villager and a fight won; tribal power copied per player; a
  creature dead for good; a creature takes up a fire's reaction; rows the hand test contradicted lowered; lost quest
  logs rerun into refs/story/evidence; the dashboard counts rows proven in play. 11,295 tests; 14 scenes moved, each
  attributed (6 by the testbed now casting both held miracles).
- joint294 (0bd580ace, FULL, 54 commits): the player's fight input, the arena's ring, the marks and fight miracles
  (raffclar refits); the creature's walk (route headings, braking stops, step offs) and its pose blends (turns, morphs,
  layered animations); features and animated scenery solid to thrown things; animals flee fires; the creature learns
  skills by watching; the hand stands on a tugged thing and leans with it; a demo's press is taken again as the
  original's playback; worship icons draw their charge. 11,384 tests; 226 scenes moved, attributed by set.
- joint295 (7c4ec96b4): what a creature holds is drawn in its hand at the original's hold, out of the map, with
  its hold pose; the worship site's dance; the hand holds a sleeping creature, a toy football, sleep eyes; the hand's
  quest actions proven through real input. Land 1 is now played through to Land 2 by the user. 38 scenes moved.
- joint296 (aa7b20e35): Impress Village won by the hand's pours alone; a story run from Land 1 through Land 2
  into Land 3 (two testbed shortcuts left), its rows closed; Load Defaults and the kept help record; held villagers
  calling the other sex confirmed as ours. Everything identical.
- joint297 (016e7e887): quest timelines for Lands 3-5; 73 of their rows closed; a creature watching the hand
  behind it turns once and ends on time (the Land 5 curse no longer stalls); the first land read from Map.txt
  (raffclar). 16 creature scenes moved by the watch fix.
- joint298 (46b99f42b): about 215 story rows closed from the evidence runs; 47 hand rows proven through real
  input; The Wolves Are Possessed played to both endings (the take-town cheat no longer crashes). 63%. Identical.
- joint299 (72a550adc, FULL, new base): the hand is a mirrored right hand from the start, as the original, and
  the leash collar on it is lit in its own white; the held scaffold's ghost building; the opened chest's blue beam;
  villager names; Land 4's gold quests and the Defending Ogres played; the builder disciple's hammer and an eaten
  globe's inner model fixed; about 100 story rows closed. 64%. Every reference renewed (the mirror moves the hand).
- joint300 (712bb6bcd, docs only): whether the player clicked an object proven through real input on a land's
  own script; story rows it and the Land 1 evidence run unblock closed. No game change.
- joint301 (225ceb458, docs only): 468 story rows now name the system row that finishes them; Land 1 silver
  quest rows closed from their evidence runs. No game change.
- joint302 (225ceb458, docs only): The Treacherous Path played: 18 rows done, 15 partial (the page from 6% to 47%).
- joint303 (81bb7ea48, docs only): I Have a Surprise for You played to its villages won: 34 rows done, 19 partial (0% to 57%).
- joint304 (76f6e573a, docs only): the advisors' messages told apart and their winning/losing lists closed; the minigames' and portals' undetermined rows and the temple LOAD_MAP turn settled from the original.
- joint305 (7ae8f257b, docs and comments only): the cut scene walks and clips settled (a creature's forced clip already as the original's; no shipped script walks a creature along a track), 7 story rows closed; the advisors' idle clock and booming voice settled.
- joint306 (f3d5e26e4): a challenge's reminder as pure rules with tests, the marker tap closed through real input; the reward chest's tap sounds proven; Land 1's Ogre, Pied Piper and temple rows closed from their evidence runs. Identical.
- joint307 (273335e8e): the script camera along three of the game's tracks checked bit for bit against the original (the cut scenes' recorded path closed); 3 story rows closed. Identical.
- joint308 (a09d0b430): a creature a script makes die faints, lies, is carried home and comes round; a script's play of an empty clip never starts (raffclar refit); 15 story rows closed. Identical.
- joint274 (c9429ba41): a resource put down goes into any store of its type within reach (a workshop takes wood), with
  mid-walk piles, the citadel's heart as a wood store and the unchecked subtraction, as the original; the scripts' EQ
  and NEQ compare only integers, booleans and floats. One scene moved (a creature's wood cast by a workshop).
- joint273 (3f5f47763): a creature's clear-area search blocks only on what the map gives collide data, picks the
  nearest by the original's distance, and holds its middle in map coordinates, as the original; quest runs for Land 1
  to 4 and three new testbed commands. All identical.
- joint272 (d945dea27): a computer player's hand keeps what it holds out of the map cells until it lets go (Khazar's
  workshop scaffold now becomes its site), as the original; the wiki reviewed page by page and six facts from the
  openblack project wiki added. All identical.
- joint271 (81e52fe25): on the aggression leash, or at a point it can't stand on, a creature holding something
  walks to eight times its height from the point and throws the thing there, as the original. All identical.
- joint270 (6530fe4bc): two quest-run comments name the quests, not the original's script files. Comments only.
- joint269 (49c67c063): Land 2's Worship Site played by a quest run, its beats closed from the log. Testbed and docs only.
- joint268 (a9f57c8de): the dialogue strip's fade pinned to the original's frame by frame; Land 1's silver rows that
  waited on it closed; the scroll notice that never plays moved to not-in-the-original. Tests and docs only. 57%.
- joint267 (72da88029): Land 2's story work: a town with no building, plan or people is deleted; a town changing
  hands leaves its old owner as a deleted town does; THING_VALID as the original; STOP_SCRIPT's name list; the
  creature's developer functions 8/9; a spot visual's target object; the scroll's instant save. All identical.
- joint266 (460456377): screen texts are placed with the original's half pixel (43 scene shots, 4 hand-demo shots
  and the temple shots move; every state identical); Land 1's silver-scroll rows closed by their quest checks.
- joint265 (8a774e6df): Choose Your Creature, The Lost Brother and The Sculptor played through by a quest run to the
  creature's choice, their rows closed from its log; testbed only otherwise.
- joint264 (d216e6709): the quest runner (a native trace switch, quest= with arguments, aimed presses) and Choose
  Your Creature's first beats closed by it; Land 1's silver scrolls as story scenes; the leash carries a held thing
  to a point and puts it down; a discard by the put-down lays a thing down; a rock's script scale; haptics rows set
  apart (device). All identical.
- joint263 (1058a01ae): a worship site's altar is built with its site, and an unbuilt site's spell icons draw
  nothing, as the original (the cycle and story move from Land 2 turn 2; closes #175); the flocks are written.
- joint262 (f01285c76, FULL): after turning on the spot the creature walks on in the same turn, and it turns on the
  spot however little, as the original (fid, cycle, story, hands and 40 scenes move, all attributed); the camera
  bookmarks' markers, pulse, follow, flight, clearing and Backspace, as the original.
- joint261 (a8a2dae02): the dances, worship sites (with their totems, signs and spell icons) and villagers in dance,
  dead and target states are loaded and written as the original saves them. All identical.
- joint260 (0be669dd8): a worship site's altar is filed in the map cells, as the original; the arenas are loaded and
  written; a scene of an artefact's glow; the save progress page lists each row once. All identical.
- joint259 (23bffd8e1): the hand takes hold of a tied leash's rope and pulls it, its fingers closing over the rope;
  a script's arena starts its fight reaction as it is made (the story run moves from turn 485, as the original).
- joint258 (2777eb9c8): a villager not available for a reaction takes no miracle's reaction either (8 miracle
  scenes move, as the original); the players' records and the in-play saves; rocks, fires and villagers are loaded and
  written as the original saves them; the save library compiles its records' copies and moves once.
- joint257 (d7a6ddc04): a ghost dissolves through its mask's holes (the store's take, the gate stones); the temple
  map's markers are shaded as the room's models; a sick creature throws the original's drops; an artefact's glow is
  as strong as its worth and a destroyed temple's glow follows the heartbeat; a quick tap on a tree under the hand is a
  tap and the hand lets go of a held tree on release; a puzzle totem's press clicks the land; the neutral player's
  type is the original's; the save's shared moving and living parts. Every state identical but the expected movers,
  renewed after the second pass.
- joint256 (aeb92e3e6): a tree keeps its angle within a turn and sways in the original's wind slot; the
  SuperVillagers' temporary shadows are made newest at the head; every belief sprite is queued with no limit, as the
  original; the save writes the rivers, mists, forests, trees, big forests, climates with their storms and the
  things of the weather, and a load brings the climates' storms and the weather things back; wiki errata from the
  names harvest. Every state identical; one scene shot and one hand shot renewed.
- joint255 (9b331b618): the save writer: the world file built from the entities (with the objects that aren't
  entities, such as forests and footpath nodes), the game object's head and tail, and a slot written in the game's
  order with its info, pictures and creature minds; the frozen Land 1 autosave writes back byte for byte. Nothing
  calls it yet; every check identical.
- joint254 (22e86e239): what flies is stopped by a pile that isn't a storage pit's, by a building only while it
  stands (the village centre always) and by a standing feature, as the original; a moving camera keeps the gesture
  while following a creature or watching a fight; a worship site's miracle sends motes of light from its icon. Four
  scenes moved by the collision rules and renewed; 182 more scenes now checked at every joint; every other check
  identical.
- joint253 (219905d55): the land takes an object's footprint only for the objects the original gives it (gates,
  caves, fields, worship sites, the citadel, a house made unbuilt); an animal given to the altar counts one
  sacrifice; a held villager not available, a skeleton or a footballer takes no work; a land-coloured particle mesh
  off the map takes the full light; the library's read texts come back on load; the square wave and the hidden
  phone box scene (from raffclar); openblack's debug tools leave the progress count. Land views and 8 scene shots
  renewed; every state identical.
- joint252 (8b4d764c1, new base): each land gives its players their colour index, as the original (a scene on
  the second land shows it); an animal's clip plays by the ground its speed covers; the land grip works while
  holding something and starts at the press, over the sky too; villager table and carried-prop tests. Fidelity,
  cycle, story, hands and the land views identical; animal scenes' shots and hand.top_pitch renewed.
- joint251 (e7e83293e): a held seed found destroyed leaves its type as the last seed type however the hand learns
  it (the earlier fix only covered the turn's check). Two debug scenes show the caught rock's flying reaction ending
  and the repeat after a spent lightning. Every check identical. 129 scenes that sat in no check list now do, with
  their references renewed; the scene batch refuses an unknown id.
- joint250 (925bd2e78): a reaction spreads over as many cells as its initiator's power gives (#51, first rule);
  the hand past the border takes the worship sites' spare chanting; a pot or pile that goes takes its reactions at
  once; a fire's crackle measures the camera's map distance. Every check identical but tornado_village (promoted).
- joint249 (0d930de04): a store makes its food pile first, then its five wood piles, at its points through its
  whole placement, and a land script's store takes its food and wood once it is made, as the original; the hand over
  any pile shows the store's whole total (tested). The loaders' header no longer needs the morph headers (the town
  tests build again). Two showing scenes. Land 1 moves at turn 1 (the piles' places); every reference renewed after a
  second pass agreed. #86 corrected: a store's piles are not made on demand.
- joint248 (0076d0747): villagers stop to look at a scaffold put up for a building, and watch a town taken over
  without a centre (a town going neutral too); a town a scaffold founds takes its forests from its wood store; flocks
  are the neutral player's, and the flock miracle's the caster's; a caught thing ends its flying reactions; a seed
  its spell deletes records its type; the facing-camera point as the original; the creature spec through the cache.
  Two new scenes. Fidelity, cycle, story and hands identical; three scenes move by the take-over reaction.
- joint247 (dd9eaba6b): a land script's positions are read in map units, as the original: 563 Land 1 objects
  move onto their quantised point, and the footpaths, Land 2 and the story follow. Every scene's state identical; the
  land, hand, story and land-view references renewed after a second pass agreed. A blast test (smoke on land, steam
  on water).
- joint246 (fc0c265d3): SQUARE_ROOT takes the root of any value; GET_SPELL_ICON_IN_TEMPLE looks up any miracle and
  adds the icon to the scripts' table; a script's in-world value points in the world only when it is 1; an advisor
  with a blend that is not a number is drawn near the screen; the hand's spec file read through the cache; a sprite's
  draw mode as a pure rule. Every check identical, the land views too.
- joint245 (4120e0d12): the land script reader reads a line as the original does; the town turn moves its
  player's alignment by the town's desires (pinned to oracle runs); the command line's graphics interface and window
  mode as pure rules; the resource caches and the script machine's auto-start tested. Every check identical, the
  story too.
- joint244 (83bb3baa7): the install folder's choice and the draw's submesh rules as tested functions; the sky,
  the moon, the sun and the hand's last rows tested; a won town's wonder gives its power to the new owner. Every
  check identical, the land views too.
- joint243 (4526cb206): the footpath files, 16-bit images, mesh and animation packs and the tables' names read
  as the game reads them (tests); an abode info is the first matching row, none when none does; CREATE_FIELD makes
  a field of no town with the Celtic row; the vulture and planned features, never made by the original, not ported.
  Every check identical.
- joint242 (5b64642ad): the hand driven through its real frame in a world of the game's own systems (picking,
  holding, pouring, the tug, throwing, the orbs); a new game starts the story only with its program read and on
  Land 1; SET_A_TOWNS_INFLUENCE_MULTIPLIER is no land script command; CREATE_TOWN gives an unknown player's town to
  the neutral player; CREATE_NEW_ANIMAL keeps a negative age; a town field of no town makes nothing. Every check
  identical.
- joint241 (490b5fe87): STOP_ALL_SCRIPTS_EXCLUDING splits its list on spaces, commas and tabs, as the original;
  the blast's rubble and rings tested in the world; the land scripts' designed scenery, the rival gods' natives and
  the reward chest tested. Every check identical.
- joint240 (542bd960b), tests only: the land as the game builds it (countries, corner and midpoint altitudes, the
  shore), the rivers and lakes as water, and the fireflies, each in the world against the original. Only test_bgfx ran.
- joint239 (09ca6bb9b): Firestorm parked with online play; the fan-made Island Wars and Death Comes To Those That
  Wait and openblack's own Construct land leave the progress pages (not in the original); thrown things through trees
  and the bonfire and a struck storage pit tested in the world. Only test_bgfx ran.
- joint238 (a2b1656d9), tests only: things dropped into the sea played in the world (seven water rows); the world
  tests keep the map of what is in each cell. Only test_bgfx and test_villagers ran.
- joint237 (a4ff845b1), tests only: villagers' deaths, hunger, homes and missionaries, and scaffolds, their sites
  and a founded town, each played in the world against the original. Only test_villagers ran.
- joint236 (e9d002477), tests only: more of the villagers' tools and loads in the world (the log a flying villager
  drops, the wood taken with the chopping clip, nothing held at home): 17 rows of tools in all. Only test_villagers ran.
- joint235 (94b2f21af), tests only: a villager world of the game's own villager services, its tools and loads
  at work checked against the original (nine rows). Only test_villagers ran.
- joint234 (09f42d356), tests only: the world tests' land is dry land; the toys, and a land script's town and its
  store, played turn by turn against the original. Every target built; only test_bgfx ran.
- joint233 (9f311bc96), tests only: phase three's world multi-tests: the shipped challenge program run in the
  real VM (its start order on Land 1 confirmed against a capture of the original's own), picking up and letting go,
  throwing, and a building broken by a blow, each with a synthetic twin. Only their test groups ran.
- joint232 (96b2b55ba), tests only: the first world multi-test. The rock split scene played turn by turn in a world
  of the game's own systems (test/support/WorldHarness.h), nine of its rows checked at once against the original, each
  with game data and with a synthetic twin. Only its test group ran.
- joint231 (44617451c): the creature's second phase pins its object actions, alignment, fights, held villagers,
  eating sounds, fizzle, lessons, town dances and species rows to the original. Every check identical.
- joint230 (6aa2fa831): the ray behind a point against a body's faces checked against the original's own and fixed
  to its order; a body on the land is not in the water. Every check identical, the physics scenes too.
- joint229 (db5454949), docs only: the creature's done rows cite the tests that check them (with a test 789, without
  18). The test campaign's first phase now covers every family.
- joint228 (886c8efe5): the story's spell, advisor, camera, game flow and sound natives and the rest of the vortex
  pinned to the original (tests). Every check identical.
- joint227 (f4ae1f3f4), on its own with every scene: the contact point, the ray against a body's faces, the touch
  of the ground and the faces' set-up checked against the original's own functions in the exe oracle and fixed to its
  order; a settle on the land no longer turns the faces again; 13 scenes and the story move by last bits; Land 1, the
  cycle and the hands identical.
- joint226 (e73fdd6b9): the land script's field, feature, forest, animated static and worship site commands and
  the vortex's pull and crossing rules pinned to the original, with game-data tests. Every check identical.
- joint225 (be165d9ae), on its own with every scene: the rigid body's set-up, forces and step checked against the
  original's own functions run in the exe oracle (golden vectors), and fixed to its order of float operations
  throughout; 22 scenes of thrown and resting things and the story move by last bits; Land 1, the cycle and the hands
  identical.
- joint224 (16c7cbc0c): the source of every number the day's tests pin, marked on the wiki and in xref notes; a
  gesture's small turn at the original's exact tolerance; over a worship icon the hand aims a fixed distance short; a
  worship charge pulses only past its interval; a film's corners at the original's depth; the challenge program and
  its natives tested. Every check identical.
- joint223 (11030b0b5, published f34ccc181), on its own with every scene: a contact's damped push is multiplied in
  the original's order, with no guard on the length (21 scenes of thrown and resting things and the story move from
  last-bit rotations; Land 1, the cycle and the hands identical). Five stale shot references from joints 214-215
  renewed.
- joint222 (08061e620): the test campaign's second phase pins the hand's picking, holding, throwing, sounds and
  animations, the camera, the interface and the audio to the original, keeping one copy of each rule. Every check
  identical.
- joint221 (d953399b4), tests only: the land script's reader and object commands, and every shipped island,
  playground and demo script's contents pinned to the game's own scripts; 269 story rows cite them. Only the two test
  groups ran, as the change can reach nothing else.
- joint220 (d3727cade, published 8c2640681): the test campaign's second phase pins the gestures, the sky, the
  video, the villagers' carried objects and 14 more town-area rows (data tests included) to the original. Every check
  identical, the land views too.
- joint219 (df152eb6b, published 08714e231): the test campaign's second phase starts: 25 rows of the town areas and
  the rigid body's rules pinned to the original; three physics constants as the original's exact floats (the spin cap
  moves physics.throw_many from turn 28, two ulps on bodies at the cap); a fading firefly's alpha truncated as the
  original; ten unused hand rules removed and one wired; a lump of poo held on the palm.
- joint218 (84ec0bad8, published 8c1504c3a): the test campaign's first phase: about 1460 done rows of every area
  but the creature now name the tests that already check them (the story's from 139 to 994); a building a script holds
  stays when destroyed; the hand's state each frame as a pure function with tests. Every check identical.
- joint217 (92e526802, published 556429d80): a villager becomes a physics object only when it can be reached, its
  throw taking the in-hand mark off first; BELIEF_FOR_PLAYER answers any thing through its own player; a forest's
  planting as pure functions with tests. Every check identical.
- joint216 (57668d7c5, published 351d6465a): over a store's pile the hand shows all the store holds, not that
  pile's own amount, as the original; a villager drowns for the scripts only in its drowning state (test); why a
  check run's answer 3 keeps a creature (docs). Every check identical.
- joint215 (a72e0a8a5, published c7b3793c5): a scroll or sign is drawn with its soft sprite towards the eye
  (compared with the original from a shot of the user's game; moves the land views, two hand demos and eight scenes,
  state identical); a pot keeps its own flag for its reaction; a double click onto an arena watches the fight and its
  point is read again on the land; the screen clears to opaque black; a collision sound's key as a pure function
  (closes #144, #24).
- joint214 (d6a708c61, published 9305de240): setting the help level sends the advisors home; lingering over a
  fight's arena watches it; a scroll is clicked through a sphere half a metre wider than it; the falling spell's film
  ends at base alpha 0x50; a collision sound's level as a pure function; three debug controls (closes #190, #191, #192).
  Every check identical.
- joint213 (fcbb78681, published 4a8331961), FULL, on its own: an animal takes its first clip as it is made, so
  each bird's coin is drawn at its creation as the original's (949 of 949 load draws matched against the original);
  every reference renewed; closes #12.
- joint212 (a85ef0f40, published 7b856c54b): the night's voices try the name of the profile played; how many
  flames a fire wants is a pure function with its test, and a size or age that is not a number answers as the
  original; a SuperVillager's shadow is cast with its own matrix and clip. Every check identical.
- joint211 (5dde882d8, published c9b4af8c2): a thing that hits a building gives the blow its own player (two
  building scenes move in the neutral player's alignment); a civic building just built draws its villagers to dance or
  watch; a dead tree that hits a town is its own player's; a shield's fall panics a villager as the original; the
  teleport's and the mourning's reactions end as any reaction does; the player's camera tells the help where the
  screen's middle falls (Zoom Out shows); the unwired mesh detail by distance, fourteen developer-tool rows and two
  unused script natives move to Not in the original.
- joint210 (86264d34d, published 85f1eac92), FULL: a town wants a wonder by its belief in its player (the guidance's
  town-desire remark then draws for it: five scenes move in the local random only); a rock that hits a town is its
  own player's; a toadstool is poisonous to stores and villagers; two players' flocks never merge; a static keeps the
  built percentage a script sets; the fire's reaction ends as any reaction does; a held miracle's spin rate, a
  script's camera set where it is already, a replayed click's point and a step's sound on deep lying snow as the
  original's.
- joint209 (3506edcd6, published 6748f56b2): the snow on an object read by its mesh's own position and normal (moves
  the throw-in-snow shot); the projection's near plane is the frame's one near clip from the eye drawn the frame
  before (moves one hand-on-creature shot); the world climate places its storm by its own radius; a tornado's dust
  takes the snow lying deep enough; a creature's dance chant at the creature; a scripted throw's tumble with the
  original's float; a building being built adds influence by its percent built; a new pit takes its food first.
- joint208 (b94191de6, published 7d57adad7): villagers take effects as the original's classes answer (a heal unless
  dead, others only where reachable), run from a burning thing in the hand, a shield's interest read from their own
  info row, the sea report by the hand that last let go; a town its people left empties; a wanderer looks for a flock
  to merge with; an artefact whose object goes is let go with its dance and glow; food from a sped-up pile speeds the
  villager up; a Villagers debug window; raffclar's pull requests recounted (54% in our tree); check runs read a
  pinned real date (OPENBLACK_CLOCK_AT), since a full moon at night made them differ. Every check identical.
- joint207 (2f17178d3, published 97e6096a9), docs only: the wiki's stale code references, repeated notes and empty
  dated remarks cleaned across 239 pages; the refactor's progress counts and the port audit recounted against the
  tree; the progress dashboard regrouped (the creature and save and load as families of their own, the online
  rows parked out of the score, the story land by land, done with and without a test).
- joint206 (604d4cc2a, published 7b02b06bf): the whole wiki and every progress page checked against the code (rows
  up or down with their evidence, stale names and links fixed, Pending only what is open; the challenge scripts'
  page regenerated); a creature's throw is not settled on the land before it flies; the hand's own distance in the
  creature and temple states (moves the temple shot); the left-handed box's side, Escape on the menu's first page
  and GAME_THING_FIELD_OF_VIEW's sphere as the original's. This joint is the new base.
- joint205 (41f6d7c50, published 7e601ebdb): a loaded game's world goes much further (footpaths, citadel hearts,
  abodes, creches and graveyards, the towns' desire flags, rivers, mists, climates, the lying snow, the creation
  counter), a read-only save is refused as the original refuses it, and a test loads the original's own reference
  saves against their files and land scripts (all of it off unless the debug bar loads a slot); a dispenser counts
  only while built, its globe stays while they touch; a creature notices a magic tree's reaction; an animal's step
  off the land is taken; an input recording's mouse reaches the debug bar; about 35 rows pinned with tests. Every
  check identical.
- joint204 (1fad9c916, published 859f61558): the hand's distance zoomer and its locked select as the original's (moves
  the hand in fidelity from turn 2, the hand demos, vis, the temple and most scenes with the hand in view); a sunk
  animal dies through its own dying and teaches the creature; two villager shield timings; a creature's put-down is
  not from the hand; four animal fixes (a calmed pack, a villager gone home, the hunt's order, a pot over its
  maximum); an armed seed's hum from the gesture's point; a shield's last attacker; a loaded game's towns, pits and
  piles (off unless asked); about 10 rows pinned with tests.
- joint203 (f1d2eb9bf, published bbf85a284): the hand stops while a box has the mouse, as the original (moves the hand
  at turn 1 of the land runs and the land shots by day and night); a tree, dead tree or fence let go over a building
  site goes into it; a temple room of pictures eases its camera onto a close look (moves the challenge room's shot);
  an animal killed while a storm carries it starts dying only when let go; the death listeners the original lacks
  removed; six fire rules taken out as pure functions; a loaded game's big forests; about 20 rows pinned with tests.
- joint202 (6e7b63e6d, published 3264421c5): the statistics graphs drawn at whole-unit places, a mouse button bound to
  an action still acting in the temple, the land's tooltips over a thing the hand can pick up, as the original's; a
  fire taking a crackle slot stops the fire it held; a mobile static in the way of what flies only by its kind; an
  animal landing dead dies through its own SetDying; seven animal fixes (breeding, switching reactions, the way and
  the turn while watching, which move Land 2 from turn 20 and one creature scene; the turn counter, a lone predator's
  lair); forest ids follow the original's counter; a saved game's land, scripts, clock, forests and trees load from
  the debug bar (off unless asked); about 25 rows pinned with tests.
- joint201 (4f15742eb, published 6c78c69be): every saved game now reads to its last byte and writes back the same
  (the game object's lists and tail), and a save service keeps a profile's slots with safe writes (nothing in play
  uses it yet); a villager turns to face a fire before it acts (moves three lightning scenes); a villager going to
  worship walks round a fire; the bonfire and fragments as the original's rocks (the hand, fireflies, the creature's
  blow); a map script's dispenser joins the nearest town; a wonder of no tribe drapes over the land; a scaffold's join
  sound, a pot's 32-bit amount, a sunk pile not drawn; a grazer passes over cells under buildings (moves Land 2 from
  turn 50); the totem's slide, a handful over a building site and the held object under a box as the original's; an
  Animals debug window.
- joint200 (1734ab43a, published 7c0c9db7c): the saved game's records read and written further (animals, creatures,
  worship sites, dances, particles and the destruction meshes: every object a save's players reach); an effect on a
  feature or flowers counts as their own kind for alignment (moves the lightning arcs scene); a burnt building
  destroyed through its own destruction; a dead tree's info row; the camera's help icons, easing and paired keys and a
  watched fight's keys as the original's; three script and miracle fixes (the cast time, the charging clear, the cost
  to create); about 60 progress rows pinned with tests.
- joint199 (9fc46b2ab, published 81ff6984e): a bird's next leg starts from the height its last leg aimed at, as the
  original's (moves Land 1 from turn 5 and five bird scenes; references renewed on two agreeing passes).
- joint198 (70cae0658, published bcad14c9e): the saved game's format read as the original writes it (the stream and
  its sums, SaveGame.inf, the pictures, the script machine's state written, the records up to the scaffolds, and a
  save tool; nothing in play reads or writes it yet); four miracle fixes (moving the bird and wolf scenes); a tree
  counted once by its forest, drowning, fireflies and a burnt tree removed through its own deletion (moving four
  forest scenes); 132 progress rows pinned to the original with tests.
- joint197 (75ef59603, published 55ab9b6e7): the player's camera starts again as a new one when a script's camera
  control ends, so its easing restarts as the original's (moves Land 1 from turn 9; references renewed on two
  agreeing passes); raffclar's start-up split into four functions (his #881, ten services moved, identical); the
  options and statistics as the original's (the volume times 1/127, the choices taken as the box is left, the tab
  of the page shown, the players counted); a fire credited to the object's own player.
- joint196 (d30cc6984, published 6cd544a8b): the snow lying on the land baked into the blocks' textures, the temple
  heart and worship sites baking their footprint and shadow, a storm's 0.01 case (#222 part 6); the camera help
  hears only the player's own controls, and two camera input differences fixed (closes #216); the forest's ring
  step as the original's float; 80 progress rows pinned by tests on the original's values (miracles, interface,
  the hand). Game state identical in every check; the ground, temple and miracle-name shots renewed.
- joint195 (2c22f22c6, published 7e2c92a6b): the hand shows what the original shows over a desire flag, a built
  workshop, a worship icon, an altar, a temple, a worship site being built, a scaffold and a reward chest, and a
  miracle's name at its power when taken (closes #157); the old print passes' shaders removed (#222); the computer
  gods' totem rows (closes #210); the docs say C++23. Game state identical in every check.
- joint194 (2d27878d0, published b8613b7a1): the footprints, static shadows and river beds are baked into the
  land blocks' textures at their prints step, as the original (#222 part 5), and the old live print passes are
  removed. Game state identical everywhere; the shots that show the ground moved and their references were
  renewed. Found on the way: the temple's own shadow and footprint are not baked yet (fixed in the next ground joint).
- joint193 (93e6313c2, published f7a184c8c): the tattoo editor's and the story history's boxes fade in from nothing
  each time they are shown, a menu question's box fades in by itself and its answer gives the menu back whole, and
  a fragment's mesh goes with the fragment. All identical.
- joint192 (0fb2eeca8, published d85056cd3): the bake of a block's prints step in the game's order, with the trees'
  texture shadows (not wired yet, #222), and a game's generated meshes go with it, so the next game numbers its own
  from 0. All identical.
- joint191 (d27916df7, published fa704e6af): the scripts' bars put every dialog away when they come in (#201), the
  key names written as the original writes them (#200; its menu fade-in already matched), a new game reads the
  hand's mesh afresh as the original makes a new hand (the last game's good or evil shape no longer stays), and
  the menu's pages fade into one another as the original's boxes do; plus the bake's pure functions for #222. A
  full joint with no scene moved; the batch's scenes now each match their own single run again.
- joint190 (53143155e, published d5685646a): the far blocks show their low-resolution tile and the texture detail
  line blends it over the near ones, as the original (#71, B1); the temple pool's map keeps the block textures for
  now. A full joint: the game's state identical everywhere; the land shots by day and night, the temple shot, three
  hand shots and two fish scenes renewed after two agreeing passes.
- joint189 (f15e3ef8d, published cc95aeaf9): a dead tree's fire going out gives its wood reaction to the tree's own
  player (#219), PLAYED of a weather thing answers whether its storm is over (#218), and the villagers' reaction to
  a burning thing in the hand converts the walker's speed as the original (#209; the issue had it the other way
  round). The footpath walks' audit (#74): every live caller already walks the footpaths. State identical in
  every check and in the fire, weather and reaction scenes.
- joint188 (64eb2139a, published 47a1dc750): drawing between turns (#220, the user's chosen difference) for what
  is not the creature: villagers and animals wherever they move continuously, the animals' and birds' turn, a
  villager's carried tool and a disciple's marks, and a highlight's golden shower; what the logic places at once
  still jumps, and the View menu's "Stepped drawing (as the original)" draws them as the original. The game's
  state is identical in every check; 11 scene shots moved (mostly birds), and their references were renewed
  after two agreeing passes.
- joint187 (7f76575c8, published 7d54cdb88): his portability fixes (#129), the oracle tests' data folder (#651/#652)
  and sprites keeping texture ids (#857), and synthetic twins so the tests that need the original's data have
  in-memory counterparts the CI can run. A full joint: every test alone, and all 192 scenes kept their state
  (only shots already known to vary moved); every other check identical.
- joint186 (365683a28, published f37f1a7b7): his turn and frame step tables (#802, #810) filled with this side's
  order, and the land's alpha pass drawn only where prints are marked (part of #71), all identical; the spooky
  voices' random 2D pan as the original and a sound bank entry's own pan (#221 closed), which the checks cannot
  hear. Every check identical, the land, night and temple shots included.
- joint185 (dd50aade4, published e34cd1305): code clean-up, all identical: the saves, the banks' waves, the input
  replay and the test hooks' files go through the file system service, a player's own files are never read from a
  data mod, the last fields and functions whose meaning the original shows take descriptive names (several his),
  and the villager state table is renamed. The dashboard's clean-up goals are all closed: 7 reached, and in the other
  3 only what is required is left.
- joint184 (b2ba0006a, published b1242646a): code clean-up, all identical: no new expressions are left in the game's
  code (std::make_unique, private constructors taking a key) and no C arrays (std::array; main takes `char** argv`,
  byte-identical), counted from the sources by `docs/tools/cpp_counts.py`. The dashboard's clean-up bar: 7 of 10.
- joint183 (2d44f0cbb, published 5f27f9deb): code clean-up, all identical (tests, now with every data-backed test
  running, fidelity, cycle, the story and the hand demos identical): the Locator interfaces we share take his method
  names and signatures where they do the same work (time, land, audio, magic, dynamics, a villager's state change;
  six pairs stay ours, each with its reason, in the conventions page), the object draw list takes his names, the
  camera's unused idle-time stub goes (his #804), and the published docs were brought up to date with the tree.
- joint182 (81871454b, published 9be0e2c97): code clean-up, all identical (tests, fidelity, cycle and the story
  identical; the hand demos identical but for the trace's source lines, renewed): the `Game` singleton is gone (the
  debug windows and the editor reach the game through a host that keeps `Game`'s method names), the mutable statics
  are down to the eight a signal handler, a per-thread scope or the exit guard needs, the particle code takes his
  names (`openblack::particles`, `Particle*` files; the file-format component keeps `psys`), the camera zone's and
  the route follower's unclear fields are named, his #866 split of the game's run and his #791 per-process test
  temporary folders are in.
- joint181 (b6dc69611, published 35e8709e2): a stopped vcpkg install leaves no stamp behind, and a new build
  folder's second configure no longer installs again (build steps only); docs synced. Its full run was identical to
  joint180's, so it confirmed joint180's references, which had been made from a single pass; four scenes were
  renewed on two agreeing passes.
- joint180 (1780a2bce, published 31d34c4cc, the new base): the checks read the original `AllMeshes.g3d` and the
  user's mesh pack is a mod now, so 25 more of Land 1's trees stand; the trees' sway runs on the frame's whole game
  milliseconds and leans in the game's precision; smoke and mist ages take each frame's whole steps; a model's part
  sets its culling from its own material, so the Norse workshop's flat fence is drawn from both sides; the gesture
  icons at the bottom right of the screen, with the leashes, the bubble and the seed inside them; and the wiki records
  smooth drawing between turns as a chosen difference (#220), why the mesh pack decides which trees a land has, and
  the hand's shadow and wrist and the trees as seen in the original. Every reference was renewed; fidelity and cycle
  move from turn 1. Issues #189 and #215 closed.
- joint179 (a23813fa2, published with joint180): the creature's lesson actions get the original's agendas, with his
  #232: sleeping at home only with a temple and not too far from it, waving only at another creature, a fright on the
  spot only about what frightens it, a forced put-down, lying dead for good as a faint that stays down, pointing at a
  thing, holding a thing up to its player and sleeping by a thing, nine of them going by its height; what the
  original never does is removed (the leash tug, the old idle stand-ins, planning every desire at once); a creature
  the game makes starts at the first stage of growing up, another creature is believed by its drawn size, and an
  animal is believed alive only above a fifth of its life. Land 1 moves from turn 44; 11 creature scenes moved that
  are not explained yet. Issues #195 and #196 closed.
- joint178 (873c0cd2a, published ba18fc95e): the temple closed out as the original, with his #468's rules and pieces
  of his tree (the scrolls' control keys, the save game room's time played, the future room): its outside blended
  two meshes at a time, the main room's pool, doorway, door signs and influence map, the creature room's flames,
  mists and waterfall, the challenge pictures and the options room; the testbed plays Land 1's story as land
  timelines (the opening, the tiger stone, the tutorial, The Temple Is Finished), with an opt-in scripts part in the
  state hash; the land blocks repainted through their queue, with building marks and prints (part of #71); the
  worship walks on the footpaths (part of #74); the ambience mixer replays the original's scheduler; the creature
  interaction takes the camera and gives it back as the original; the hand's pointer pinned while it holds a village
  totem; and every wiki area brought up to date with the tree. Fidelity, cycle, the story and the hand demos were
  identical; 26 scenes moved (the land blocks, the temple, the worship walks) and were renewed with joint180. Issues
  #163, #165 and #198 closed.
- joint177 (80a6aca71, published 06e0fc890): the gestures closed out (the pass knows when the creature fights, a
  gesture shown on the land is laid as the original's, a camera shake keeps the path drawn, a still mouse sends its
  own messages, a template of more than 80 points is kept) and the films (an opaque film's frame waits for its next
  picture, Escape and the skips as the original, the pre-intro and the logos cut short only by Escape or a mouse
  button, the loading screen's version from the game settings); the falling-spell creature's own hair; and the
  did-you-know bubble drawn in front of the camera, where nothing of it showed. Five scenes moved, as predicted.
- joint176 (ff7ceb5d1, published 0ec495ead): the World pass: the sea and the missionaries' boat, fish farms a fire
  heats and a miracle wears down, the climate in the game's precision and the scripts' weather and wind natives, a
  player's resistance to wind, the gusty wind's noise, rain and snow standing still inside the temple, each thing's
  light from the land, the night lights, light columns as wide as their model, a worship site's footpaths from each
  town's pit, the ambience's fades in the original's order, the creature's hair tufts and its foot in the water
  scaring the fish; a land change keeps what the original keeps and only a new game resets it (the clock, the
  guidance, the hand's gestures, a film, the camera's lens and more); the leash wipes the gesture path; the hand's
  locked select and totems as the original; the player's camera rounded through the map's middle every frame; a fire
  put out impresses only the villagers near it, and a villager at home or hiding is only impressed by a reaction.
  Every reference was run and every move explained, then renewed: the story moves from Land 1's turn 483, the cycle
  on Land 2. Issues #166, #167, #181, #182 and #202 to #208 closed.
- joint175 (f511f9b71, published 49a2b6917): a built wonder raises its player's tribal power; teleport stones,
  seeds, totems and empty pots take no effect; a raised object passes over a way out of a land; no rain with the
  Weather setting off; a Norse gate's lamps drawn only with their gate; the snow drawn over each land block's four
  tiles as the original; picking a leash wipes the gesture path, and the power-up pass stops on all of the original's
  early outs; the camera's movement keys, its clear view over sloping land and the map's disc as the original (part
  of #201); the hand's slap and caress on the creature blend in and keep their own clock. Every reference was run:
  the state is identical everywhere. Issues #193 and #199 closed.
- joint174 (3e7358664, published 480b54a71): data mods: a switched-on mod's files replace the game's files of the
  same path, only with the mod loader. Identical everywhere.
- joint173 (14cea29a4, published 30b1c9e16): the Mods page: with the mod loader, the options list each mod with a
  switch, and the game and the loader agree by the SDK's version, a hash of its headers; nothing changes without the
  loader. Every family's progress was measured again against the code and the original (the Beings family in
  182e5a0aa, published 7699ce942). Issue #76 closed: fishermen are in, and in the original fishing takes no food out
  of a farm.
- joint172 (e34ede6a1, published c3b8cdcba): the raw input recorded and played back where the window's events enter
  (off by default); the villagers' overlay names each villager's top state; the footpath tab's Execute button says it
  is not wired; progress rows count a partial row as its share. Identical everywhere.
- joint170 and joint171 (ff94ef905, published b40eb5a3e, landed together): openblack starts at the top detail level
  (the user's choice); at the two lowest levels the sky's dome is the day pictures, unblended and tinted by the time
  of day; the creature looks at the sun only by day and at the moon only at night, files each thing by its class's
  kind, sees a thing move once it is away from where it was put, and on the way to a far point looks at the last
  moving thing it met; the fight music while the player directs the fight, the effects and music volumes kept for the
  computer, a scroll's click as it is activated; the camera kept inside a script's fence; multisampling as a View
  switch, off as the original draws; progress rows only for behaviour the original has. Every reference was renewed
  at the top detail level, where every check runs since. Issues #160, #161 and #162 closed.
- joint169 (68800eb81, published 3cd7ce29b): his #477 a creature casting at a place (a forest where a villager is, a
  town's shield at a built storage pit, wood by a workshop), his #544 the creature's emergencies (it looks at them
  before it decides, and puts out a burning thing with water) and his #354 the creature held by the hand (it stands
  still, turns to the player and shows what it wants; lessons, spells and fumbles are told), with the guide's and
  the Ogre's natives (a script sets a creature's desires, makes or takes away reactions, makes a creature from
  another); a town's desire flags, made with their town and flying for its desire; the need signs over a worship
  site's altar; what a challenge script creates (a chest dropped from the sky, a one-unit store, no highlight); a pot
  or a pile in the physics only when its row says so. Fidelity, cycle, the story, the temple shots and the hand
  demos were renewed.
- joint168 (0c09fccc9, published 128a66f64): two in ten new villagers become named villagers, as the original. It
  moves every reference; all were renewed.
- joint167 (b30d23942, published 6cf03120f): the hand's texts, with his #494, over the land, buildings and scrolls
  and with a gift (Pitch and Rotate, Zoom Out, a building's wood and progress, Give, Combine Scaffolds, Sacrifice at
  an altar, a town's and a pit's numbers each turn) and the hand demo's mouse; his #347 a creature told to look at a
  thing turns to it, keeps its head on it and comes to know it, and a script camera facing a creature takes its
  body's facing; heavy rain splashes rings on the land and the sea; animals leaving a script, a walk or a reaction
  let go of their circle; the hand's press over a thing it can do nothing with clicks the land. The hand demos were
  renewed. Issue #159 closed.
- joint166 (c5e4a4030, published b18e60085): a villager leaving script control lets go of the circle it hugged; a
  creed never becomes a physics object and hangs where the hand lets it go, and what flies goes through it; each
  player's hand keeps the last thing it let go of, and a creature can copy where it was let go. With it, the wiki
  caught up with joints 148 to 166 (c5402793a, published 2804e9aca, the new base).
- joint164 (add75ea86, published 1835ff1f4) and joint165 (1864b95f3, published c4f35733c): a shield's fall panics a
  villager about whatever it was watching, as the original; leaving the Options page on Maximum Detail always sets
  the top detail level, also from the default 4 (the user's choice).
- joint163 (93a387834, published 3a6709899): his #308 and #309, the worshippers as the original (disciples sent
  last, the site's food pot, altar rest and supply errands, the chant's cost, hiding at the door, no worshippers for
  a site still being built, a dancer leaving when the site is gone, a tired worshipper not going home), and a
  villager wandering round a script's place (SET_SCRIPT_STATE_POS, SET_SCRIPT_FLOAT); a script resizing a weather
  thing's storm; a walk left for a reaction or a script kept, and the script's walk keeping its circle.
- joint162 (d5b56dc26, published ce1a0ce02): what a challenge script creates settles on the land (poo, rocks, a
  football, a scaffold, an ark, mobile objects), a football and a lump of poo moving by their own physics rows; a
  villager in a script's crowd plays a crowd clip each time its clip is ready; the plain walks and leaving
  MOVE_TO_POS as the original. Identical everywhere.
- joint161 (6968dde6a, published ba479304f): faster builds (reproducible MSVC outputs, vcpkg installing only when
  what it installs from changes, five headers including less); a held leash's rope starts at the hand's wrist; his
  #518 a thing seen again stirs the creature's wish to impress, compassion, anger and play; his #228
  MAP_SCRIPT_FUNCTION carries out its land-script line; the golden shower drawn over things a highlight keeps (part
  of #63). Three leash scenes were renewed. Issue #152 closed.
- joint160 (94d03d1ec, published 678a049e2): a creature's throw leads a walking villager and aims where a creature
  will be; a drag keeps the camera hints of the frame before its press; IN_CREATURE_HAND as the original; a new dance
  gets a footpath from a town's storage pit, and a deleted fixed object's footpath link goes with it. Nine new camera
  scenes got their references. Issue #151 closed.
- joint159 (e8fef535c, published 4c493567f): the top detail level draws the villagers, animals and birds again, each
  film actor with its own high mesh, and the options' Maximum Detail sets that level (the user's choice); a thrown or
  nudged miracle bubble stays drawn round its miracle; the one-shot miracles a challenge script creates (Khazar's on
  Land 2) settle on the land; his #361 a hand demo's recorded click lands on the thing of its kind nearby, and his
  #333 IS_OF_TYPE; the disciple's glow and yellow symbol follow the villager; a creature's SCALE and SPEED for
  scripts; one debug switch, off by default, for the fixes of the original's game-logic bugs. Identical everywhere.
  Issues #145 to #148 closed, and #80 (the disciples) after it.
- joint158 (652f0896d, published d6ff3e78a): an animal a script creates is held still until the script needs it, so
  the shepherd quest's sheep no longer wander off. Land 1 moves from turn 9; fidelity, cycle, the story, the land
  shots and the hand demos were renewed. Issue #150 closed.
- joint157 (6bc97ff78, published 8115efef3): the state hash's creature part folds the leashes each creature knows.
  Only that part moved; fidelity, cycle, the story, 72 scenes and the hand demos were renewed.
- joint156 (cd2eabbb7, published 11b57a2de): a young creature's head, hands, feet and eyes are drawn bigger, and its
  eyes take the size it is drawn at, as the original. Land 1's transforms move from turn 10, the random draws
  unchanged; fidelity, cycle, the story, 69 scenes and a temple shot were renewed. Issue #156 closed.
- joint155 (79be6513f, published 2d66a47c9): a tap on the land sends a leashed creature there, marked and
  acknowledged with its sound, and a second tap makes it run (part of #154); the idle hand takes no action press
  while the game is paused; a dead tree crosses the vortex as the player's it is. Identical everywhere. Issue #158
  closed.
- joint154 (7094d3bb8, published 4ed161480): the camera's helper icons by the hand, from his pure rules (the pitch
  and rotate arrows, the zoom glass, the red cross out of the influence, faded in and out, sized from a 4:3 screen),
  in play and in the hand demos; the hand's texts for signposts and scrolls (Activate, Reminder), help bubbles
  (Scroll), the grip with both buttons (Zoom) and the leash (Detach Leash, Attach Leash, Bring Creature). 75 scenes,
  the land shots by day and night and the hand demos were renewed. Issue #155 closed.
- joint153 (6b2f3d01c, published fd5a276ba): his #468 the temple's rooms open as the game opens them, his #226 the
  help of its rooms, scrolls and places, and his #252 a help text's control shown as its key or mouse button; a
  scaffold a computer hand drops leaves the hand as the player's does; three unused shader varyings dropped. The
  temple shots moved.
- joint152 (4c99bf016, published 6604fd05e): a creature made from its mind file knows the leashes the file keeps
  (part of #153); the hand strokes and slaps a creature only once its body is free, and reads its pose as the hand's
  creature state begins; the creature interaction asks for the camera once, and no script camera hands it the view;
  five temple rooms show their reflection under their floors. A temple shot moved.
- joint151 (54e4a064d, published dad401a55): the sun drawn before the moon, after the dome; SET_SUN_DRAW,
  GET_OBJECT_FADE and the real-clock natives (GET_REAL_TIME, GET_REAL_DAY, GET_REAL_MONTH, GET_REAL_YEAR); another
  creature's dominant desire is the one its action shows now; the Statistics box's town figures read as they stand;
  the vortex's crossing file writes the angles each thing keeps. Identical everywhere.
- joint150 (f921941cc, published 180d1722f): only the creature's idle face draws its random pick, as the original.
  It moves Land 1 from turn 44; fidelity, cycle, the story, the land shots and the hand demos were renewed.
- joint149 (49274a3d5, published 56929fd5e): each player's influence power is worked out once the land's script has
  run, and lasts to the next land until it is worked out again; his #320 a temple's door lanterns hidden from a
  little after its explosion, and his #413 the hurt temple heart's wait and sound pinned to the original's numbers.
  After it, a milestones page and a simpler dashboard that measures raffclar's work against his current master
  (c2fb6f48d, published 8eb6fadd4).
- joint148 (f107fa43f, published 5dde484e3): his #349 the temple's depth fog on its rooms, walls and trophies, and
  its model light; the original's facts from eight older notes brought into the wiki. Only the temple shot moved.
  Before it, the dashboard's one measure of raffclar's work (c97de55f5, published 55cde0a02) and the wiki brought up
  to date with joints 132 to 147 (6f5dffa29, published 12af5b482).
- joint147 (6dcd1fdcc, in 7ca9f39a7): whose throw it is, as the original: what any creature lets go of, throws at a
  target or kicks is its own player's throw, with the creature as the thrower, and a computer player's hand drops for
  that player (stores, knocks, catches, toy play and remarks follow); a held scaffold sounds its phantom building; and
  a creature impresses the villagers who react to it by its size and what it last did. Every reference was run.
- joint146 (1177006b4): every creature makes the reaction the living things about it react to, so villagers inspect,
  respect or run from it and animals run from it, as the original; and the user's choice to keep the mesh pack's
  textures 32-bit, recorded. Every reference was run; the story moves from Land 1's turn 725.
- joint145 (45868e848, published 46120b530, the new base): his #294 the artefact's aura and the relaxation order (the
  football pitch first), the creature's creed glows and the falling creature's hand glows, the citadel heart's
  destruction drawn as the original's (shell, fade, the god's colour, newest first, hidden at the end), and the plan
  others see a creature hold. Only the temple scenes moved.
- joint144 (29aa6ec82): a villager reacts to a creature by its town's attitude (inspecting, respecting or running), and
  each town judges the creatures it met by what they last did, replacing the imported attitude; identical in the
  measured runs.
- joint143 (8689d0a1e): a thrown thing keeps the player who threw it, and a town keeps a record of each creature its
  people met; identical everywhere.
- joint142 (71a06bf90, published e51e94196): a creature's kick as the original's (kicked trees fly, stomped ones stand,
  homes lose pieces to two blows, a creak at the foot), whose harm each player's things keep, a felled tree as the
  neutral player's, how impressive things are for wonders and scaffolds, and script natives (MOVE_GAME_THING on storms
  and on things that are not alive, a climate's first rain).
- joint141 (844c13c40, published 8f039c1c8): his stored angles (a placed thing keeps its angles, a moved thing takes its
  body's), with our script angles and leans and a creature's script angle, and a building an effect destroys going
  inside the effect. The cycle moves on Land 2.
- joint140 (f6aabd1da, published 651092bd7): the original's floating point at eight sites (the hand's scale, the
  tornado, the fireball's direction, a body's spin, the arcs round circles), every object's reaction impressing the
  villagers, and the players of predators' and crushed things' reactions. Every reference was run.
- joint139 (a2fcf04c7, published 44bdf994a): the creature sleeps on the spot in a clear place or at home, and its mind's
  two trees as the original's (what a plan is for, what it is done to), with the worth of a thing to act on and of a
  gift.
- joint138 (f9851cecf, published 6f3261954): his #493 the buildings' sounds of the time of day and #525 the Game
  Statistics box (the creature's tallies, the help record, the lines of code), the computer players' personality
  files, the vortex's crossing file and MAKE_LAST_OBJECT_ARTIFACT, and x87 models of the runtime's maths. Every
  reference was run.
- joint137 (fbd9ba84e, published f3c654b2e): his #617 sleeping on the spot only on a leash, with no temple or far from
  home, and a worship site's altar as a temple part. Every reference was run; Land 1 moves from turn 44.
- joint136 (ce7e4eb7e, published 92ed7c3aa): the state hash folds every town's belief; every reference renewed once.
- joint135 (c6b95e5f5, published e1660fdbe): the creature's body timing as the original's (a loop's end, a start's loop
  and a faint in the same turn), what a creature must go round, the advisors' fight remark, the hand's give at the
  press, script angles, and the editor's miracle bubbles. Issues #18 and #100 closed.
- joint134 (6b6fa567f, published d54eadf47): his #420 second set of kind actions (a blow at the right foot, a rock
  broken in two, the planner's rock, toy and repair actions, the ball kicked about), #610 a creature's spell to amuse
  another creature, and #507 eating from a storage pit, a field or a tree; our exe fixes to the creature's attention
  (loneliness, lack of interaction, its body's time standing still in the hand) and its feedback lessons, the towns'
  counts read as they stand, every object's own resource, the scaffold in the hand, seven more computer-mind scores
  (84 of 86), more script natives, and the shipped pack's see-through textures.
- joint133 (9ff41c252, published 77e61f374): the did-you-know bubble drawn under the dialogue texts, particle sounds
  keyed on the player's alignment, the sacrifice's light, prayer number and tree ghost, a kept creature finding no spot
  outside its area, a change-house disciple moving in, GET_RESOURCE and REMOVE_RESOURCE, and the new progress
  dashboard; identical in the measured runs apart from the help shots. Issues #65, #75, #93 and #131 closed.
- joint132b (73562c6b6): issue fixes from the exe: the town attack sound and its warning, the sacrifice sounds, the
  hand's lightning going for creatures, what the vortex throws out not being eaten, a villager carrying what a script
  gives it, a lantern's light going with the lantern, and the creature's sounds with no camera. Issue #20 closed.
- joint131 (e0f5be33b, the new base): the creature's body and hands timed as the original's (the mind waits for a busy
  body, the hands act the turn after a walk-up, a request waits for the hands, the hand's gift and take), and a reward
  chest opens only for its own player. Issues #5, #57, #108, #118 and #119 closed.
- joint130 (acbf8635a): the players' alliances (kept across lands, an ally's influence, gifts and throws), more of the
  computer minds' scores (the creature's busy body, the heart's last blow), and his #396 creature fights, #521 beliefs,
  #611 thefts for a town and #533 drinking, with our exe fixes to the end of a gift to a store.
- joint129 (e07596e1d): his group 58 (leaving a land through the vortex: the ape's sub-type, the vortex-leave natives,
  the creature sparkling away, the way out pulling things in), a joint of its own: it moves Land 1 from turn 99, and
  every reference was run twice and renewed.
- joint128 (0a5f46b77): the computer players' minds (the decision tree as the original's, its weighing and 74 of its
  86 scores, the mind natives), the start of physics outside the hand, the workshop and breeder natives, and our exe
  fixes to the creature's watch of the hand; all identical in the measured runs.
- joint127 (af0b5a2de): a creature made by a script's CREATE (of no player, out of reactions until a script lets it),
  with its home where it is made and facing as the original's does; every reference moved with it and was renewed.
- joint126 (cfadad969): the story's natives (PLAYED_PERCENTAGE, KEY_DOWN, object state, targets, containers, poison,
  mist, the health total, carrying and creeds) and the computer players' scripted hands (move, pick up, drop, forced
  actions, town totals), so Land 2 plays on past its arrival; all identical in the measured runs.
- joint125 (bec419607): the creature's throws timed and launched as the original's, the town counts read live and the
  villagers' shuffle, the disciple native, his #478 and group 41 (the plan a creature carries out moves its alignment),
  and the story: object properties, the fourth land's puzzle totems (bell towers) and the story pages' statuses.
- joint124 (738c93836): his #425 #580 #508 (dances outside the temple, lost things, leaving home) and our exe fixes: the
  landed-lanes audits (the creature's hand watch, faints, food, dances), a town taken over, a town's room read live, and the
  story's fight natives with the fight queue's lean.
- joint123 (845a2f561): his #507 #610 #556/#617 #420 pieces and our exe fixes (the creature's fart, its last hit, the
  landed-lanes audit, the kind actions with something in hand, bringing things home, eating from fields), the story's
  scripted Fight; all measured identical.
- joint122 (9daa0bf01, published 05fd4f670): his #352/#377 the players' profiles (the new player's box, the Players
  page, a name for every creature) and #384 swapping creatures; our exe fixes to the creature's casting (its looks and
  faces) and the story's PLAYED of an animal. A new check follows the story from Land 1 to Land 2 through the vortex.
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

- None lane by lane since 2026-10-20 (above): the joints above name each piece of his that came in. Ported here
  before that date and parked until the recreation reaches their rows: the per-desire object test, #267, the
  shepherds, #417 and #529.

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
- the editor additions: the creature spawner window hosted by the editor, and placing, moving and inspecting
  creatures (issues #97 and #99);
- the testbed's miracle scenarios, globes and dispenser grid from his tree (issue #188).

The flowers (issue #124) and the hand catching and feeding fireballs (issues #114 to #116) were done on this side,
as were the other parts once offered here: script decompile and recompile in the editor, its Scripts tab and its
dispensers (#96, #98, #100), the creature status and fight panels (#111), and the belief sprites, electric arcs and
camera-distance scaling (#117 to #119); those issues are closed. The Land 5 rock (issue #128) was read on this side:
its skin is white in the game's data, so it is drawn as the original draws it. So were a wonder's fences (issue
#129): they are the wonder's own scaffold, drawn by how far it is built, as the original draws it.

## Being done by raffclar's agents (pull requests open)

- (none open.)

## Done recently

See `CHANGELOG.md`.
