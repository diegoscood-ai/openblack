# Changelog

What this fork of openblack changes. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Every change was checked against the build before it, with a fixed time step:

- the full test suite;
- a per-turn hash of the world state and the full random number trace, over Land 1 and a Land 1 → Land 2 → Land 1
  cycle;
- pixel comparisons of the land view by day and by night, and of the temple;
- scripted hand demos.

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
- **A [progress dashboard](docs/wiki/project/dashboard.md):** the features row by row, the port of raffclar's target
  build and the refactor's counts.
- raffclar's progress notes moved to [docs/wiki/project/progress](docs/wiki/project/progress/README.md) and the
  refactor's documentation to `docs/wiki/project/`. Code comments cite `docs/wiki/<section>/<page>.md#<anchor>`.
- The wiki's tools are in [docs/tools](docs/tools/README.md): a link checker, the coverage check, the status marks
  and the progress counts.
- **Removed:** the generated HTML copy of the wiki (`docs/wiki-html`) and its generator; the viewer page replaces
  them.

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
  as the original (a debug switch keeps the old idle policy).
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

### The hand

- A tree is picked by its drawn pixels; the throw is released from the hand's drawn pose; the held-miracle release.
- The hand over the sky (its start point, its first ease out from the camera, its action point), its land as the
  original's shifted land line, its pose blend, its pose at the screen's edge and its frames cut to whole
  milliseconds.
- What the hand can take, as the original's list; the scoop locking onto piles; things given to a store dissolving
  as the original's ghost; the hand put away bending no trees.
- The village totem gripped and slid by the hand, facing its site and wearing the creature's species icon.
- The hand's tooltips for pits, workshops, seeds, orbs, spells and totems; the night hand light wherever the hand is.

### Magic and particles

- Exact particle key-point curves; objects as particle emitters; the gesture light sheet and its draw; the gestures'
  land points (the interface's land line, the samples, the shield's size and the trail); no gestures inside the
  temple.
- Lightning arcs over what a bolt strikes; a stray lightning effect removed; the storm in the hand flashing.
- A tornado makes a caught creature faint and passes over what is not available.
- The forest miracle's camera take; a player's miracles reported as deeds to the watching creatures; a creature
  miracle cast at anything but a creature fails.

### World, physics and buildings

- A building breaks in the original's order, a half-built building is broken again by a blow, and the temple's heart
  is destroyed over 22 s, passing its blows on.
- The physics' removal, end of turn, hit record and a hard impact's effect in the original's order; objects at rest
  placed on the map's grid; fragments coming to rest and hanging objects.
- Trees drawn unlit, the trees' bend after the hand, the tree tug as the original's spring, and the replanted tree's
  mimicry; a burning tree's alpha reference; a split rock's fire passed on.
- Land 1's creature gate opens, its stones laid by the hand. Reward chests, a fallen chest's dust, a dead body's
  smoke, the football and the stores' rules.

### Rendering and sky

- The original's per-frame object draw list (block cull, level of detail, rebuild rule, land visibility), which the
  renderer honours; blended models drawn in its order; the high meshes at the top detail level.
- The moon (its reflection before its glow, its depth, its phase), the sun's glare hidden by the land, and the sky's
  dome, sun and moon as entities.
- The advisor trails; the temple's door lanterns; the fireflies' blue; reflections in the sea for held and flying
  bodies, creatures and the boat.
- The revolved surfaces under the original's less-or-equal depth test; empty submeshes and unused ice maps skipped;
  a faster island block texture.

### Camera

- The camera flights, the camera zones, the pan stop, the double click flying to the object under the pointer, and
  the land under a pixel as the island's own ray.

### Interface, video and audio

- The start-up logos, the pre-intro film, the tips and loading screens (with a switch to skip them for tests); no
  dialogue drawn over a film; the dialogue's skip key and click as the original; a text box taking the hand's input.
- The advisors: their faces, eyes, pointing arm, spacing, lip sync and gestures from their cue labels, and their
  rainbow trail and sparks; the help bubble's layout; the help tooltips reset on a new land and kept through a pause.
- The game's keys for speed, the temple, the realm and villager names; the C key held back under the script's
  interface level; the game speed words; the game pausing while minimised; fixed-point text rounded as the original.
- Sounds: the advisors' animation sounds, the fight music, the heartbeat, the buildings' own sounds, the
  missionaries' song with its karaoke box, the ambience loops fading in, and dead things' voices.

### Scripts

- Scripts' "until" and "when" handlers run as the original, and the script VM's end of step counts as the original,
  so Land 1's creature is released on the original's turn.
- More natives: a thing's health, being hit, a marker type, `SET_POSITION` refusing held, flying or out-of-map things,
  `POPULATE_CONTAINER`, `BELIEF_FOR_PLAYER`, `GET_TOWN_WITH_ID`, `IS_ACTIVE`, `GET_OBJECT_CLICKED`, `FLOCK_MEMBER`,
  `CREATURE_SET_AGENDA_PRIORITY`, `TOGGLE_COMPUTER_PLAYER`, `CREATE_FLOWERS`, pile health for `GET_PROPERTY`, and
  `OBJECT_DELETE` by its mode. A script's `CREATE` puts its point on the map's fixed point first.

### From raffclar's tree

- His physics, picking, building damage, sky, vortex, intro, help, profile, magic world, dialogue, villager life and
  advisor services; his camera track files, `WalkPath` and shark systems; his hand grab and click rules, village totem
  interfaces, fire rules, physics materials, hand demo and clip sound caches; the creature's forced plans and his town
  compassion module *(some not wired yet)*. His remaining Locator slots are declared ahead.

### Testing and debugging

- A testbed: a flat land with scenarios (miracles, weather, the leashed creature, fights, birds, eating, a land change
  on a copy of the game folder) and checks that run several scenarios or hand demos in one game launch.
- A per-entity state trace for the checks; trace switches that never change what the game does; a VSync switch.
- The debug bar shows only with the game's menu, with the system pointer hidden; the editor's Scripts tab shares one
  Scripts panel; the debug land loader brings the player's creature as a real land change does.
- The audio device is owned with RAII, and the spirit meshes load through the resource cache.

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
