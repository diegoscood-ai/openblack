# Changelog

What this fork of openblack changes. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Every change was checked against the build before it, with a fixed time step:

- the full test suite;
- a per-turn hash of the world state and the full random number trace, over Land 1 and a Land 1 → Land 2 → Land 1
  cycle;
- pixel comparisons of the land view by day and by night, and of the temple;
- scripted hand demos;
- the testbed's scenarios the change can reach.

A change that alters the game on purpose is its own commit, measured and described, and the references are remade
after it. How to run the checks is in [docs/wiki/project/testing.md](docs/wiki/project/testing.md).

## [Unreleased] — the original's behaviour, area by area

Everything below was measured against the build before it, as described above. Pieces taken from raffclar's tree
that are not connected to the game yet are marked *(not wired yet)*.

### Documentation

- **The wiki rebuilt by game system** in [docs/wiki](docs/wiki/project/reading-the-wiki.md): one page per subject,
  in fifteen sections (engine, rendering, physics, world, beings, creature, magic, hand, camera, interface, audio,
  video, scripts, file formats, project), each page with the same four parts: Summary, The original, openblack and
  Pending. A coverage check proves that every fact of the old pages is kept; the few dropped on purpose are listed
  with their reason in [DROPPED.tsv](docs/wiki/project/DROPPED.tsv).
- **One viewer page,** [docs/index.html](docs/index.html), reads the Markdown pages directly, with a contents list
  ([docs/SUMMARY.md](docs/SUMMARY.md)), search and a live progress dashboard. `docs/open_wiki.bat` (or
  `python docs/serve.py`) serves it locally. On GitHub the same pages read as plain Markdown.
- **Status marks with emoji:** ✅ faithful, ⚠️ approximate, 🔍 inferred, 🧪 verify, 📼 1.20 only and 🎯 chosen
  difference; the word is always kept.
- **A [progress dashboard](docs/wiki/project/dashboard.md):** the original's features by family, raffclar's work in
  our tree and the code clean-up's goals, drawn as bars, and the milestones; the repository's front page shows its
  summary.
- raffclar's progress notes moved to [docs/wiki/project/progress](docs/wiki/project/progress/README.md) and the
  refactor's documentation to `docs/wiki/project/`. Code comments cite `docs/wiki/<section>/<page>.md#<anchor>`.
- The wiki's tools are in [docs/tools](docs/tools/README.md): a link checker, the coverage check, the status marks
  and the progress counts.
- **Removed:** the generated HTML copy of the wiki (`docs/wiki-html`) and its generator; the viewer page replaces
  them.
- The wiki pages and the progress rows were brought up to date with the code and re-measured against the original,
  area by area. A row is done, chosen, partial with its share, inferred or pending; behaviour the original does not
  have gets no row, and what was found about it is on
  [Not in the original](docs/wiki/project/not-in-the-original.md).
- The [chosen differences](docs/wiki/project/reconstruction.md#what-is-deliberately-different--chosen-differences)
  are listed with their dates; new pages hold the
  [deviations taken from raffclar's tree](docs/wiki/project/deviations.md) and the project's
  [milestones](docs/wiki/project/milestones.md).

### The creature

- Its body goes with it to the next land with its fatness, needs and age. Its radius, life and shown fatness, and the
  size spells aiming for its own limits, follow the original.
- The miracles that reach it (heal, harm, burning), what its spells cost it, and a miracle seed the hand applies to a
  creature cast on it, as the original.
- In its pen: its size, gait and animation speed, its growth while it moves, and the temple heart's leash posts. It is
  drawn between its last two turn poses, so it grows smoothly.
- The leash: the temple's leash posts drawn for the leashes it knows, the leash keeping it within the rope's reach,
  tied with the hand, shaken off by a scribble, and its walk back forced, as the original.
- It sleeps as long as the original (the day's length).
- It casts the miracles it knows, when told, by itself and in its fights, and stops casting when the hand grips it.
- Its mind: desires planned in passes, two a turn, with the plan decided as a pass ends; the plan priority, its cap
  and its ties; a goal-seeking desire with no goal has no plan; anger acts on its goal; the least dominant desire; a
  plan kept while its desire cannot be planned; compassion for a town's hurt villagers (the town's desires, its
  half-minute timer, the members as candidates). Land 1's creature first adopts a plan on the original's turn.
- Under a script's control (`SET_FOCUS` until `RELEASE_FROM_SCRIPT`), and with no plan choosing nothing of its own,
  as the original.
- Its learnt usefulness is capped; its feedback after a stroke or slap is judged on the newest thing it carried out;
  a young creature gets its coin after a stroke or slap.
- Picking up birds, animals, villagers and mobile things by the original's tests and reach; a meal's energy, end and
  judgement; eating from piles by portions; desires satisfied only by a carried-out plan.
- The mood and need spells make their desire dominant; freezing, mood or itch stop what it was doing; a creature
  spell's start sound comes from the spells' bank, and a spell whose miracle is gone wears off.
- Its look: mirrored poses read from its species' tables, eyes blinking on the game's local random stream, eyelids
  and motes, its turn on the spot as the original's spins, its lean onto the slope, its sway when struck, its fizz in
  and out of sight, its hair gone once frozen or fizzed, its dust as it walks, and its per-turn sound queue.
- It catches thrown things and is struck by a thrown object. A creature removed by the spawner or `OBJECT_DELETE`
  goes through its full removal.
- The tattoo editor in the Creature Cave, opened as the original does, with the creature's copy posed by the fire.
- The creature and fight panels drawn as the original (the watched fight, the held creature, under the fade).
- Its actions on things: pick-ups by each kind of thing's own rule; throwing a thing into a store, at another thing
  or at the camera, or showing it to its player; rocks, toys, repairs and a ball kicked about; helping a town;
  bringing a thing, food or a field's wheat home; eating from a storage pit, a field or a tree; drinking at the land's
  water points, else at the nearest coast; stealing for its temple; taking a living thing to its temple's altar; a
  magic mushroom making it high; the fart, the poo desire's other action (an evil creature's fart makes the fainting
  reaction, which nothing takes up yet).
- Its casting: the attack miracles and the fireball, the shields (a town's shield at its storage pit), a forest at a
  villager's place, water on its own fire or on a burning thing, and a miracle to impress a town's villagers or to
  amuse another creature; a try that fizzles is told to the player. It looks at its emergencies before it decides.
- What it notices: it watches the hand, looks other creatures over and watches things thrown through the air,
  remembers the things it came across, and forms its beliefs about other creatures and buildings. Once ignored (its
  loneliness and lack of interaction grow with the turns) it asks for its player's attention.
- It copies the deeds it sees, near enough and while free: gifts and resources given by the hand, things put down
  near its temple or stolen, toys let go, disciples put down, rocks tapped, houses built from scaffolds, and a
  miracle once it knows it. The plan it carries out moves its alignment each turn, and its feedback teaches what a
  plan was for and what it was done to, over its last five actions.
- Friends and fights: two friendly creatures act together, side by side or at a town; it smiles at a friend, waves
  at another creature, and dances with a town's villagers, outside its temple and around a thing. A fight is asked
  first and may be refused, a creature pulled away calls it off with no winner, and the advisors remark how the
  player's creature's fight went.
- Its help: held by the hand it stands still, turns to the player and shows what it wants, and the good advisor tells
  of its lessons, body, wants, spells and fumbles, and when it has lost the thing it looked for.
- Growing up: a stage switches its desires at once and lets a kept creature go, and a creature the game makes starts
  at the first stage. A young creature's head, hands, feet and eyes are drawn bigger, as the original, so its eyes no
  longer stand out of its head.
- Each creature has a name of its own, shown in the fight panel and on the Creature Cave's scroll; a script can swap
  two creatures' minds, each body keeping its own name, size and alignment.
- Its sleep: on the spot only on a leash, with no temple or away from home, in a clear place; at home only with a
  temple and not too far from it, unless forced. It looks at the sun only by day and at the moon only at night.
- Its look: the creed glows, the falling spell's hand glows and its copy's own hair, and tufts that an animation shows
  or hides; a foot set down in the water scares the fish.
- The leash: a tap on the land sends a leashed creature there, marked and acknowledged with its sound, and a second
  tap makes it run; the V and B keys step the leash as the original; a held leash's rope starts at the hand's wrist.
- A creature from a mind file (`CREATE_CREATURE_FROM_FILE`, a computer god's) is made as `LOAD_CREATURE` makes its
  own and knows the leashes its file keeps; a newly made creature faces angle 0 and has a home from the start. The
  land's map of regions, made once a game, gives the exploring actions their places.
- **Removed**, as the original has none of them: openblack's own idle choices for a creature with no plan, with their
  debug switch (it now stands as it is until its planner gives it another plan), the leash tug and planning every
  desire at once.

### Villagers, animals and towns

- Villagers walk round what is in their way on the original's circles, aim and re-aim their walk as the original's
  wall hug, follow the footpath rules, and walk, run or sprint by the original's whole speed limits; their speeds come
  from their town's belief and tribal power.
- They flee from and watch miracles, walk round fires, flee from or point at what the hand throws, look up at a flying
  thing, and end a reaction when the hand picks them up.
- Wounded, poisoned, looking, downed, landed and stopped villagers follow the original's rules. A new villager plays
  its first clip, and a walking villager's clip plays by its whole speed in whole milliseconds.
- Every villager, animal and creature made draws its age as the original, and villagers are scaled for their age
  last. The posed villagers and animals are drawn from one bone palette, and the villagers' eyes are unlit.
- Animals flee as the original, take their reaction turns and landscape circles, and let go of a circle when what made
  it leaves the map; grazers search the original's number of cells.
- Script flocks of birds: the leaders fly their legs about the flock's home, doves about a good god's temple and bats
  about an evil one's. Sharks show their fins above the water; the fish farms have their shoals and puzzle.
- Towns: their belief and awe, the believers they gain floating up as sparkles, a town's injured count, aggression,
  population read-outs, and dances with their shapes.
- A crash is fixed when a villager walked round a tree that was then destroyed.
- Disciples: a villager held in the hand takes up the work of what it is held near; traders carry food or wood to
  their trade town and craftsmen wood to the workshop, missionaries preach to another town, and breeders call
  villagers of the other sex; a farmer, builder, breeder or craftsman put by another town's thing moves into it;
  their glow and symbol, yellow in or under the hand.
- Villagers of an age meet, kiss and part; two in ten new villagers become named villagers; a villager given food or
  wood carries it. They run from, watch or hide from a predator, hide or panic when a shield falls, and inspect,
  respect or run from a creature by their town's attitude. An object's reaction, a scaffold, a creature and a fire
  put out impress the villagers who react to them, as the original.
- Towns: a town taken over plays its sound, leaves its worship site, celebrates its new owner and gives it its spell
  icons; desire flags fly over a town's storage pit for its wants; each town remembers the creatures its people met
  and judges them by what they last did; an attacked town plays the attack sound and its warning; a town's desires
  read its buildings as they stand.
- Artefacts: a rock, another static or a dead tree put down gently by a town's building or a worship site becomes its
  artefact, which the villagers dance round, with its worth, belief, glow, symbol and aura; a wonder's power counts
  how impressive the things round it are, and a built wonder raises its player's tribal power.
- Worship: the hand sacrifices a villager, an animal or a tree at a worship site's altar, with the original's sounds,
  light, red prayer power and the tree's fading ghost; hungry worshippers eat from the site's food pot, those beyond
  the dance hide at its door, and they walk to it on the footpaths; the altar raises its three need signs.
- Footpaths: the walk to a building takes its footpaths, a footpath link follows its building, and a new dance and a
  worship site have footpaths laid to them from a town's storage pit.
- Animals: the ground flock miracle's wolves frighten the living round them, a predator does not hunt a villager made
  a skeleton, and animals run from a creature.

### The hand

- A tree is picked by its drawn pixels; the throw is released from the hand's drawn pose; the held-miracle release.
- The hand over the sky (its start point, its first ease out from the camera, its action point), its land as the
  original's shifted land line, its pose blend, its pose at the screen's edge and its frames cut to whole
  milliseconds.
- What the hand can take, as the original's list; the scoop locking onto piles; things given to a store dissolving
  as the original's ghost; the hand put away bending no trees.
- The village totem gripped and slid by the hand, facing its site and wearing the creature's species icon.
- The hand's tooltips for pits, workshops, seeds, orbs, spells and totems; the night hand light wherever the hand is.
- Held to a creature, the hand wiggles, caresses or slaps it, larger on a big creature, by the original's clocks and
  once its body has finished what it was doing; every third hard slap in a row bruises it; a press while holding
  something gives it to the creature.
- The camera's helper icons by the hand: the pitch and rotate arrows, the zoom glass and the red cross out of the
  influence, faded in and out at the hand's point.
- More of the original's tooltips: Activate and Reminder over signposts and scrolls, Scroll over a help bubble, Zoom,
  Pitch, Rotate and Zoom Out over the land, Give, Sacrifice and Combine Scaffolds with a thing held, Attach Leash,
  Detach Leash and Bring Creature with the leash, a building's wood and progress, a desire flag's Need and, over a
  worship site's altar, the prayer power it stores.
- Scaffolds picked up off their site, joined to another scaffold, taken by a storage pit as wood, and put down gently
  keeping only what their building needs. A puzzle totem is dragged in whole steps and a totem held outside the
  influence is dropped; the idle hand takes no press while the game is paused, and a press over a thing it can do
  nothing with clicks the land.
- Hand demos: a recorded click lands on the thing of its kind nearby, and the demo's mouse is drawn by the camera's
  icons with its buttons lit.

### Magic and particles

- Exact particle key-point curves; objects as particle emitters; the gesture light sheet and its draw; the gestures'
  land points (the interface's land line, the samples, the shield's size and the trail); no gestures inside the
  temple.
- Lightning arcs over what a bolt strikes; a stray lightning effect removed; the storm in the hand flashing.
- A tornado makes a caught creature faint and passes over what is not available.
- The forest miracle's camera take; a player's miracles reported as deeds to the watching creatures; a creature
  miracle cast at anything but a creature fails.
- Gestures: the gesture icons at the bottom right of the screen, with the leash's and the bubble's models and the
  seed's graphic; a gesture shown on the land laid as the original; the path kept drawn through a camera shake, and
  wiped when a leash is picked, put on or taken off.
- Reactions: one is offered again each turn, the newest reaction of an object is the one it started, and its
  reactions shut down newest first. The alignment's seven steps are cut as the original, and each player's influence
  power is worked out once the land's script has run and kept from one land to the next.
- A bolt from the hand that finds a creature in sight strikes only creatures; the tornado's funnel and the fireball's
  direction take the original's floating-point maths, and the gusty wind its lattice noise; teleport stones, seeds,
  totems and empty pots take no effect; a highlighted object gets its golden shower.

### World, physics and buildings

- A building breaks in the original's order, a half-built building is broken again by a blow, and the temple's heart
  is destroyed over 22 s, passing its blows on.
- The physics' removal, end of turn, hit record and a hard impact's effect in the original's order; objects at rest
  placed on the map's grid; fragments coming to rest and hanging objects.
- Trees drawn unlit, the trees' bend after the hand, the tree tug as the original's spring, and the replanted tree's
  mimicry; a burning tree's alpha reference; a split rock's fire passed on.
- Land 1's creature gate opens, its stones laid by the hand. Reward chests, a fallen chest's dust, a dead body's
  smoke, the football and the stores' rules.
- The vortex between lands: the way out pulls things in and takes what is thrown into it, and the creature sparkles
  away through it; the way into the next land throws out what crossed (villagers, animals, globes, artefacts,
  scaffolds, pieces of buildings as rocks), its people flailing as they fly.
- Weather: heavy rain splashes rings on the land and the sea, and a block shows its streaks only over a rain strength
  of 5; no rain or falling snow with the Weather detail setting off, nor inside the temple; the snow drawn over each
  block's four tiles; the climate's temperature and raining days in the original's precision, and a storm spot tried
  again only where there is no land block; the scripts move, resize and steer a storm's weather thing, which goes when
  its storm is over unless a script holds it.
- Trees: a kicked tree flies off and a knocked-down one stays standing, since no creature fells a tree; a felled tree
  is the neutral player's; a thrown tree that ends dead tells the villagers there is wood; the sway runs on the
  frame's whole game milliseconds.
- Physics: placed and moved things keep their angles; a creed hangs where the hand lets it go and what flies passes
  through it; a football can become a physics object, with its own info and material, and a pot or a pile only when
  its row says so; what a challenge script creates settles on the land; the log a thrown villager drops spins.
- Reward chests tapped open by their player give what they hold, a scaffold chest a scaffold to its town's workshop; a
  creature's blow breaks a rock in two and its kick breaks pieces off a home; a fish farm is heated by fire and worn
  down by miracles; worship sites, building sites, dead trees, rocks, big forests and fish farms give their resources
  as the original's; the graves are drawn as the burials make them, and the creatures follow the land a vortex or the
  temple levels.

### Rendering and sky

- The original's per-frame object draw list (block cull, level of detail, rebuild rule, land visibility), which the
  renderer honours; blended models drawn in its order; the high meshes at the top detail level.
- The moon (its reflection before its glow, its depth, its phase), the sun's glare hidden by the land, and the sky's
  dome, sun and moon as entities.
- The advisor trails; the temple's door lanterns; the fireflies' blue; reflections in the sea for held and flying
  bodies, creatures and the boat.
- The revolved surfaces under the original's less-or-equal depth test; empty submeshes and unused ice maps skipped;
  a faster island block texture.
- At the top detail level the villagers, animals and birds are drawn again (none were), each actor with its own high
  mesh.
- The sun is drawn before the moon, after the dome, and the dome at the two lowest detail levels is the day pictures,
  unblended and tinted by the time of day.
- Lighting: each thing takes the land's light its own way, a model off the map the full light and a totem building
  its cell's light; the land's light and shadow stamps go only into the blocks drawn; a changed land block is
  repainted through the block texture queue, and its footprints and static shadows at its prints step.
- Light columns over the village centres and the temples, as wide as their model; the missionaries' boat's people in
  the hull's colour; a Norse gate's lamps only with their gate; a pack texture with transparent texels keeps its
  alpha; smoke and mist ages in whole steps; a thrown or nudged miracle bubble stays drawn round its miracle.

### Camera

- The camera flights, the camera zones, the pan stop, the double click flying to the object under the pointer, and
  the land under a pixel as the island's own ray.
- While the hand interacts with a creature the camera looks at it from in front, taking the view from Creature Mode,
  a miracle's camera path or a script's dual camera and giving it back after.
- The player's camera is kept inside a script's fence, with its shake and out-of-bounds sound, and within the map's
  disc; the movement keys go as far as the zoom's step; a camera sent to face a creature takes the way its body faces.

### Interface, video and audio

- The start-up logos, the pre-intro film, the tips and loading screens (with a switch to skip them for tests); no
  dialogue drawn over a film; the dialogue's skip key and click as the original; a text box taking the hand's input.
- The advisors: their faces, eyes, pointing arm, spacing, lip sync and gestures from their cue labels, and their
  rainbow trail and sparks; the help bubble's layout; the help tooltips reset on a new land and kept through a pause.
- The game's keys for speed, the temple, the realm and villager names; the C key held back under the script's
  interface level; the game speed words; the game pausing while minimised; fixed-point text rounded as the original.
- Sounds: the advisors' animation sounds, the fight music, the heartbeat, the buildings' own sounds, the
  missionaries' song with its karaoke box, the ambience loops fading in, and dead things' voices.
- Player profiles, kept in a `Profiles` folder of the working directory in place of the original's registry: a first
  start asks the new player's name over the swirling backdrop, and the Players page lists every profile, plays the one
  picked, creates and deletes players, restarts, and keeps the options with the profile.
- The Game Statistics box, opened from the main menu's Statistics tab, with the players' counters and histories.
- The Help key's answers; the advisors' remarks on an attacked town, a wrecked building and the player's creature
  attacking a town; a help text's control shown as its bound key or mouse button; the did-you-know bubble drawn in
  the world, under the dialogue texts and in front of the camera.
- The fight panel in the screen's top left corner, with each fighter's health and stamina bars, and one bar shared
  with the creature panel; the creature panel hidden only under a script's wide screen with an owner.
- Films: Escape does not skip one once the game is over or in the playground, and each new game decides again whether
  its films can be skipped; only Escape or a mouse button cuts a start-up logo short; the loading screen's version
  comes from the game settings.
- More sounds: the buildings' ambience by the time of day, a particle sound keyed by its player's alignment, a held
  scaffold's sounds, a scroll's click and the fight music while the player directs the fight; the effects and music
  volumes are kept between games, as the original keeps them.

### The temple

- The challenge room: its scroll lists the player's challenge records, its walls show the challenges' pictures with
  their tooltips and close look, and a challenge chosen fades to white to be replayed; the temple map marks the
  challenges not done yet.
- The library's tips and story history; the Creature Cave's scrolls, naming each miracle by its own learnt text and
  telling only the stronger of anger and compassion, and a script saying whether the cave shows the player's
  creature; the save game room's time played and counts; the future room's words; scrolls that picture the keys of
  the controls they name.
- The rooms open as the game opens them, a closed room's door says so, and each room, scroll and place starts its
  help; the main room's pool map takes the players' colours and shows their influence circles; going in puts the
  world's dialogue away, and walking out gives it back, swinging the doorway with its sound.
- Its look: depth fog on the rooms, walls and trophies, reflections under the rooms' floors, the pool lit in the
  original's water colour, the creature room's waterfall, flames and mists, and the outside blended two meshes at a
  time.
- A destroyed citadel heart fades out into its god's colour under its shell, and the heart and its door lanterns are
  drawn only until a little after its explosion.

### Scripts

- Scripts' "until" and "when" handlers run as the original, and the script VM's end of step counts as the original,
  so Land 1's creature is released on the original's turn.
- More natives: a thing's health, being hit, a marker type, `SET_POSITION` refusing held, flying or out-of-map things,
  `POPULATE_CONTAINER`, `BELIEF_FOR_PLAYER`, `GET_TOWN_WITH_ID`, `IS_ACTIVE`, `GET_OBJECT_CLICKED`, `FLOCK_MEMBER`,
  `CREATURE_SET_AGENDA_PRIORITY`, `TOGGLE_COMPUTER_PLAYER`, `CREATE_FLOWERS`, pile health for `GET_PROPERTY`, and
  `OBJECT_DELETE` by its mode. A script's `CREATE` puts its point on the map's fixed point first.
- Many more natives, among them `SET_SKELETON`, `LINK_FOOTPATH`, `SET_DISCIPLE`, `KEY_DOWN`, `PLAYED_PERCENTAGE`,
  `GET_TARGET_OBJECT`, `IN_CREATURE_HAND`, `IS_OF_TYPE`, `SET_TARGET`, `CALL_NEAR_IN_STATE`, `GET_RESOURCE`,
  `REMOVE_RESOURCE`, `SET_SUN_DRAW`, `MAKE_LAST_OBJECT_ARTIFACT` and `MAP_SCRIPT_FUNCTION`; the container, poison,
  mist, wind, vortex-leave and real-time clock natives; and properties such as a thing's player, height and health,
  and a creature's scale and speed.
- The creature natives: its training, its desires, its confinement, its fights, rest and home, the thing it looks or
  points at, and a creature made from another. A challenge script's `CREATE` makes creatures, chests dropped from the
  sky, magic piles and things that settle on the land.
- The natives that take control take it of what they are given, so on Land 1 the welcome crowd leaves the tiger stone
  and the temple builders go to work; the hit record forgets what is no longer available.
- A land change keeps what the original keeps (the help and the guidance, the camera's lens, the scripts' switches,
  the day and night clock, the hand's gestures and last miracle, a film playing), and a new game resets them; the
  scripts' restart, at a new game or a playground's start, resets the help and the scripts' switches, stops the fade,
  sends both advisors home and ends a lesson.

### Rival gods

- Each computer mind starts on the land, sizing its creature and teaching it its miracles; the scripts set its
  weights, suppressions and attitudes and load its personality files, and it lasts the whole game.
- A script takes a computer player's hand, which moves, picks a thing up, carries it and drops it gently, and what it
  holds is drawn where the hand was; a script switches a computer mind on or off, or pauses it.
- Players can be allies, as the scripts set them: outside their own influence a player has their allies', and may
  give an ally's creature things and take hold of it as of their own.

### From raffclar's tree

- His physics, picking, building damage, sky, vortex, intro, help, profile, magic world, dialogue, villager life and
  advisor services; his camera track files, `WalkPath` and shark systems; his hand grab and click rules, village totem
  interfaces, fire rules, physics materials, hand demo and clip sound caches; the creature's forced plans and his town
  compassion module *(some not wired yet)*. His remaining Locator slots are declared ahead.
- Pieces of over a hundred more of his pull requests, refitted onto our systems and measured like our own commits:
  much of the creature's mind and actions, the disciples, artefacts, town desire flags and take-over, the temple's
  library, challenge room and help, the Help key, the profiles and the new player's box, the Game Statistics box, the
  vortex between lands and the camera's helper icons. Each such commit says what it took from his tree, and
  [raffclar-status.md](docs/wiki/project/raffclar-status.md) gives the state of each of his pull requests.

### Mods

- With the mod loader library there and its mods folder read, the in-game options get a sixth tab, Mods, with each
  mod's name, version and author and a switch, and a switched-on mod's files replace the game's files of the same
  path (data mods). The game and the loader agree by the mod SDK's version, a hash of its headers; with another
  version or no library the game starts unmodded, with one log line saying why. See
  [the minimal mod loader](docs/wiki/project/mod-loader.md).

### Chosen differences

Differences from the original chosen on purpose, each recorded with its date in
[the table](docs/wiki/project/reconstruction.md#what-is-deliberately-different--chosen-differences):

- openblack starts at detail level 6, the top one, where the original starts at 4, and every check runs there
  (2026-10-21); the options box's Maximum Detail entry sets level 6 as the Options page is left (2026-10-20), also
  from the default level (2026-10-21); the villagers', animals' and birds' high meshes only at level 6 (2026-10-15).
- The mesh pack's textures keep 32-bit colour and their own sizes (2026-10-20).
- The sun glare's test of what is drawn in front of it answers a frame or two late, through occlusion queries
  (2026-10-21).
- Four-sample multisampling, as a debug switch only, in the View menu and off by default (2026-10-21).
- The Mods page and data mods, only with the mod loader (2026-10-21 and 2026-10-22).
- The camera's helper icons by the hand sized from a 4:3 screen as tall as the screen, so that a wide screen does not
  make them bigger, taken from raffclar's list (2026-10-20).
- Everything that moves drawn smoothly between turns, also where the original draws it a turn at a time (2026-10-22;
  recorded, not coded yet).

### Testing and debugging

- A testbed: a flat land with scenarios (miracles, weather, the leashed creature, fights, birds, eating, a land change
  on a copy of the game folder) and checks that run several scenarios or hand demos in one game launch.
- A per-entity state trace for the checks; trace switches that never change what the game does; a VSync switch.
- The debug bar shows only with the game's menu, with the system pointer hidden; the editor's Scripts tab shares one
  Scripts panel; the debug land loader brings the player's creature as a real land change does.
- The audio device is owned with RAII, and the spirit meshes load through the resource cache.
- The debug bar: the renderer's debug text on F11, since F1 is the game's Help key; a frame counter in the screen's
  top-right corner while playing, which no check draws; a Fix Vanilla Bugs switch, off by default, for the imported
  fixes of the original's game-logic bugs; an input recorder (`--record-input`, `--replay-input`); a Start button for
  a script in the script viewer.
- The state hash takes in a town's belief, the leashes each creature knows and, when asked, the scripts; testbed
  timelines play scenario commands on a real land, among them Land 1's opening and tutorial, Choose Your Creature's
  tiger stone and The Temple Is Finished.

### Build

- Reproducible MSVC outputs, and vcpkg's install step runs only when what it installs from changed (a stopped install
  leaves no stamp behind).

## raffclar's architecture, the temple and the creature

The fork now follows the architecture of raffclar's openblack tree: his layout, names, Locator slots and
`<X>SystemInterface` / `Implementations/<X>System` pairs, and his test layout. Where both trees did the same job, his
version was taken only when it was better or behaved the same bit for bit; otherwise ours stays, behind his names. The
patterns are written down in [docs/wiki/project/refactor-conventions.md](docs/wiki/project/refactor-conventions.md).

### Added

- **Our reverse-engineering tools** in `tools/re/` (xref, the oracle, precheck, hashdiff), each with its usage docs. They read your own copy of the game; nothing from the game is in the repository.

- **The temple interior** from raffclar's tree: its rooms, scrolls and signs, with the world paused while you are
  inside. The door is entered with the hand, and the temple's outside follows the player's alignment.
- **The player's creature in Land 1.** It is loaded from the player's profile, and runs on raffclar's creature systems:
  body, mind, movement, fights, hair, skin and sounds. The hand can hold it, the land scripts' creature commands work,
  it lives at its temple's pen, it is drawn smaller in the pen as in the original, and it moves smoothly between turns.
- **The storage pit as in the original.** The hand gives what it holds to the store under it. Wood joins the pit's
  total, fences count as wood, animals are taken as food, and the worship site keeps what it is given.
- **The editor (F2) and creature mode** from raffclar's tree.
- **Debug windows:** the creature spawner, camera paths, weather, gestures, key bindings, the magic tabs and a
  vortices window.
- **raffclar's progress notes** in [docs/wiki/project/progress](docs/wiki/project/progress/README.md): every feature of the original game, row
  by row, with its status, plus the original's own bugs. His pull request and issue templates are in `.github`.
- **The creature learns and remembers.** It learns miracles by watching them, knows what it thinks its god wants
  (the temple scroll shows it), looks at animals, and runs from what frightens it. When the story moves on to the next
  land, its mind and physique are saved and the next land brings it back, as in the original. Before the first save,
  a one-time `.bak` copy of the creature file is kept.
- **More script commands:** `LOAD_CREATURE`, `SET_CREATURE_NAME`, `CREATURE_AUTOSCALE` and
  `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING`. `LOAD_MAP` changes land at once, inside the script, as the original does.
- **The landscape vortex is drawn**, and living things and piles follow the ground when a vortex or the temple reshapes
  it.
- **Snow** lying on the land and falling, from raffclar's tree.
- **The fight camera** circles two fighting creatures by the original's rules.
- **Thunder** at a lightning strike.
- **A file dialog and file browser** in the debug menu, from raffclar's tree.

### Changed

- Names follow raffclar's tree, for example `map_coords`, `gutils`, `zsort`, `Alignment`, and the `time`,
  `particleSystem` and `villageLightSystem` services.
- The tests are in the top-level `test/` folder, in his folders and with his test macros.
- Dialogs, the text database and the creature's cave screen use raffclar's code.
- The land light palette is a resource, and the sky and render frame services are behavioural.
- Making a texture or a mesh mid-game no longer stalls the frame.
- The mouse ray starts at the camera's eye, as in the original, and the mouse camera (grip, pan, edge drags, pitch,
  wheel) follows the original.
- The map, fire, explosion, forest, weather, rain, smoke and field systems sit behind raffclar's interfaces.
- The clouds, the street lanterns, the snow and the rain make the original's visual random draws, in its order. The
  lanterns flicker only in the dark, and the snow and rain are placed once at start-up.

### Fixed

- The camera no longer clips things up to twice its near distance, so the consciences are no longer cut off close to
  the camera.
- The "Did you know?" signs open their bubble when tapped.
- The temple door can be entered, the temple window no longer crashes, and the miracle dispensers make their miracles.
- The creature's hair, eyes and whiskers follow its drawn size, and the temple no longer covers its hair.
- The worship site wears its temple's texture.
- The creature's eyes are drawn, and the pen's leash holds the creature at its drawn size.
- The rain is no longer placed again every time a land loads.
- A creature loaded from its file keeps the rates at which its desires fade.

### Removed

- **The Bullet physics library.** The game's own physics moves its bodies, and the hand, the camera and the cursor
  find the land and the objects under a pixel with the original's own rays, so nothing asked Bullet anything any
  more. Bullet, its CMake and vcpkg entries and its land and mesh shapes are gone, with no change to the game.

## Code base refactor

The code follows the project's conventions, summarised in [docs/wiki/project/refactor-conventions.md](docs/wiki/project/refactor-conventions.md). The
simulation and the visuals are unchanged, except where noted under Removed.

### Changed

- **No mutable globals for game logic.** About 400 globals and statics moved into Locator services or into components
  on the entity they belong to. Tests inject fakes through the services instead of using test-only setters.
- **Assets load once**, through the resource caches, instead of being read from disk in about 35 places.
- **Modern C++:** `std::array`, `std::unique_ptr`, `enum class`, `constexpr` constants and `[[nodiscard]]`.
- **Comments** are plain English that describe behaviour. Names and addresses from the original game's binary moved to
  the research wiki, so no research was lost, and identifiers were renamed after what they do.
- **Formatting:** the whole code base passes the CI's clang-format 17 check.
- **Faster builds:** precompiled headers and 13 grouped test executables. A clean build takes about 7 minutes instead
  of 31.
- **Our own audio decoders** replace dr_wav and dr_mp3, with the same samples on every sound and music segment of the
  game.
- `glwtool`'s JSON keys use the field names from raffclar's tree. JSON written with the old keys must be renamed.

### Added

- **openblack's own Bink video decoder.** The game's films play, frame-identical to the original's Bink library,
  without FFmpeg. Its tables and coefficient order follow FFmpeg's Bink decoder (Konstantin Shishkov, Peter Ross;
  LGPL-2.1-or-later, used under GPL-3).
- **Debug windows** for audio (play any sound or music, silence the game's sounds or music) and video (play any film,
  turn the films off).
- **The script VM's debugger** from raffclar's tree: breakpoints, stepping, and editing variables.
- **The research wiki, curated,** with a Pending section on every page (now [docs/wiki](docs/wiki/project/reading-the-wiki.md),
  read through the viewer page `docs/index.html`, which replaced the HTML copy).
- **The refactor's documentation** (now in [docs/wiki/project](docs/wiki/project/refactor-conventions.md)).
- `OPENBLACK_IGNORE_REAL_INPUT=1`, for test runs: the real mouse is ignored.

### Fixed

- MS-ADPCM sounds end at their real length instead of playing the padding of their last block.
- The hand's grip flag starts as false instead of uninitialised.
- A broken house's scaffold was drawn as a huge dome. The same bug is in upstream openblack.

### Removed

- **The mod loader and the built-in mods**, with their Lua, sol2 and JSON dependencies and the `--mod`, `--msaa`,
  `--mipmaps`, `--anisotropic` and `--enhanced-graphics` options. A minimal, versioned `ModLoader.dll` remains; the mod
  SDK's design is in [docs/wiki/project/mod-loader.md](docs/wiki/project/mod-loader.md).
- Because the intro-skipping mod is gone, the game plays the intro and asks the tutorial question, as the original
  does.
- FFmpeg and drlibs.

## Known issues

- Tree foliage sway can differ between runs while assets load, so the start menu screenshot is not pixel-stable. The
  land view is.
- The first night screenshot after a fresh build can differ from later ones.
- The shader build does not track `#include`d shader files: after changing `vs_object.sc`, delete the generated shaders
  to rebuild the instanced ones.
