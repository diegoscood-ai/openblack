# Black & White 1 notes for openblack (local branch `local/hand-hbn`)

Guide to what was discovered while rebuilding the original behaviour from `runblack.exe` (unofficial v1.42 on top of
v1.20, W120 layout) and from the original data, and to how openblack does it. Everything stated here is
verified in the executable or measured, except where marked **(inferred)** or **(approximate)**.

**Local-only** project: nothing is published (no push, no PRs, no forks).

- [Pages by area](#pages-by-area)
- [Where do I look for…?](#where-do-i-look-for)
- [How the pages are written](#how-the-pages-are-written)
- [Philosophy](#philosophy)
- [Local external references](#local-external-references)

## Pages by area

**Tools and engine basics**

| Page | Contents |
|---|---|
| [tooling.md](tooling.md) | Disassembly, symbols, openblack tools, data formats, LND and BWLandEditor maps |
| [engine-math.md](engine-math.md) | Coordinates (MapCoords 16.16, cells, InBounds, spiral: `ecs::map_coords`), GUtils distances and sigmoids (`gutils`), object size (2D radius, height and overrides: `ecs::object`), game clock (turn, fraction, dt, pause: `game_clock`), terrain height, LH matrices, Zoomer |
| [engine-loop.md](engine-loop.md) | Game turns and frames: the original loop and turn call by call, the interface's packets, random streams between turn and draw, openblack's loop, threads plan, state hash and fixed clock for replay tests |
| [openblack-internals.md](openblack-internals.md) | Where everything is in the code, building, tests, test hooks, debugging crashes, pitfalls (Vulkan, makeRef), commits with several sessions |

**The hand and objects**

| Page | Contents |
|---|---|
| [hand-and-interface.md](hand-and-interface.md) | Hand: placement, states, grabbing, throwing, object under the cursor |
| [objects-and-resources.md](objects-and-resources.md) | Piles and pots, picking up in batches, store, static objects and rocks, fields, sounds (pick-up, LHAudio and QMixer, channels, ambience) |
| [trees.md](trees.md) | Trees and forests: uprooting and the tug, pick-up rules, dropping and replanting, wood and GTreeInfo, API for villager jobs, searches, growth, drawing, tree fire, sacrifice |
| [map-loading.md](map-loading.md) | Map loading and script functions: CHL CREATE, map fog, herds and animals, simulation data, fish farms, `BUILT_PERCENTAGE`, script objects (street lamps, bonfires, dead trees, gates, planned citadel), `IsOkToCreateAtPos`, towns and citadel |
| [physics.md](physics.md) | Original physics: thrown objects, collisions, damage, floating, dropping from the hand, buildings and rocks that break |
| [buildings.md](buildings.md) | Buildings and towns, the building side: resources held by objects (`ecs::object_resources`), the town's temporary pots (`ecs::town_stores`), life and damage, plans and building sites (pending) |

**Living beings**

| Page | Contents |
|---|---|
| [animation.md](animation.md) | Villagers and animals: ANM clips, which clip per state, speed, size, creation index, clip sounds, objects in the hand, drawing between turns |
| [villagers.md](villagers.md) | Villagers: original fields, +0xE0 flags, creation, state changes (SetTopState / SetCurrentAndDestinationState), speed jumps, assumptions |
| [animals.md](animals.md) | Animals: complete original AI (herbivores, predators and hunting, birds, reactions, flocks, age, hand, physics, death, villagers as prey), clips per species, remaining differences |

**World, time and graphics**

| Page | Contents |
|---|---|
| [day-night-weather.md](day-night-weather.md) | Day and night clock (visual and script time, cycle, scripts), night lights, weather; game time and weather (LH3DAtmos, GClimate, storms, rain) |
| [rendering.md](rendering.md) | World rendering: D3D states, terrain and small bump, sea and coast, light table, haze, camera, shadows on the terrain, sky and clouds, rivers, fade, fonts and text, map fog, land haze and light (`graphics::haze`, `land_light`, light and shadow stamps) |
| [rendering-objects.md](rendering-objects.md) | Model rendering: L3D materials, model lighting, textures, sprites, splats, reflections in the sea, clipping by the water plane, fish shoals, object and hand shadows, LOD, chimney smoke; the shared systems: transparent queue (`graphics::zsorter`), camera-facing objects (`graphics::billboard`), animated textures (`graphics::frame_anim`), ground-hugging meshes (`land_morph`), render modes (`graphics::render_modes`) |
| [parity.md](parity.md) | Graphics engine parity table: each stage of the original and its status in openblack |
| [original-frame.md](original-frame.md) | Map of the original frame (draw order, render modes, states, levels of detail) |
| [audio.md](audio.md) | Audio engine (GAudio, LHaudio, QMixer, openblack layers), banks and formats (.sad, .sas, MP2 music), music (LHMusic, GameMusic), voices and texts, audio CHL, phase A done and phases B/C |
| [water.md](water.md) | Water in the game: water cells (SeaCells), queries (coast, river and drinking water) and the `LandAvoid` mask, water in the scripts, hits and falls into the water, sinking and drowning, rings, sharks, the fish puzzle, the missionaries' boat, waterfall and ark, water audio |
| [camera-tracks.md](camera-tracks.md) | `Data\camera.edt`: `Cam%d` cameras, `Track%d` tracks (`LH3DWay`), `WALK_PATH` of the MobileObjects (sharks) |
| [script-camera.md](script-camera.md) | Script camera: GCamera zoomers, CameraModeScript, arrival rule, camera CHL opcodes, FOV, releasing control |
| [intro.md](intro.md) | The Land 1 intro and the tutorial's script side: FollowUs step by step, dialogue texts (HelpText), the advisor spirits (HelpDude), CALL / CALL_NEAR, interaction levels, script highlights, timers and help events, the family in high detail (SuperVillager), JC specials |
| [video.md](video.md) | Bink videos (.bik): the five videos and when each one plays, `LHVideoPlayer`, the 16-bit copy (555/565), pacing, pause, widescreen, fade, ESC, the 3D world not being drawn, the audio of each video; `video::VideoPlayer` and the V3..V8 plan |

**Magic**

| Page | Contents |
|---|---|
| [magic.md](magic.md) | Magic core: info.dat tables, spell life cycle, chants, events and effects, casting rules, seeds and single-use miracles, casting from the hand and gestures, worship and prayer power, influence, alignment, reactions, life, fire model, order within the turn; audited assumptions |
| [miracles.md](miracles.md) | Each miracle: food and wood, water, heal, forest, flocks, fireball and lightning, shields, teleport, storm and tornado, lightning explosion; the creature's ones (pending) |
| [particles.md](particles.md) | Particle engine (PSys): particle types, class registry, PSys linked to the spell, hierarchies, creators (meshes, chains, light maps, fog), particle sound, index of rules |

**Mods**

| Page | Contents |
|---|---|
| [mod-library.md](mod-library.md) | Mod library: menu, `settings.cfg`, `--mod`, mod types, how to write one, catalogue |
| [mods.md](mods.md) | The modified `AllMeshes.g3d` of the installation and the HD-Tweaks mod |

**Coming up** (plan `C:\Users\diewgarc\dev\documentacion\equipo\WIKI_PLAN.md`): `villagers.md` (villager jobs).

## Where do I look for…?

| Topic | Page |
|---|---|
| An address or symbol of `runblack.exe`, the disassembly scripts | [tooling.md](tooling.md) |
| Terrain height, coordinates, matrices, object radius and height, the turn and the frame dt | [engine-math.md](engine-math.md) |
| Picking up, dropping, throwing, the cursor | [hand-and-interface.md](hand-and-interface.md) |
| Food and wood, store, fields | [objects-and-resources.md](objects-and-resources.md) |
| Trees and forests (tug, replanting, growth, fire, sacrifice) | [trees.md](trees.md) |
| What the map script (CHL) creates, fogs, herds, street lamps, towns | [map-loading.md](map-loading.md) |
| Hits, damage, buildings that break, dropping from the hand | [physics.md](physics.md) |
| Animations and speed of villagers and animals | [animation.md](animation.md) |
| Villager data and states | [villagers.md](villagers.md) |
| Animal behaviour | [animals.md](animals.md) |
| Time of day, lit windows, weather, storms, rain | [day-night-weather.md](day-night-weather.md) |
| How the world is drawn (terrain, sea, sky); whether it already matches the original | [rendering.md](rendering.md), [parity.md](parity.md) |
| How a model is drawn (materials, lighting, reflections, shadows, sprites, smoke) | [rendering-objects.md](rendering-objects.md) |
| Which API to use for a billboard, an animated texture, something stuck to the ground, a blend mode or the transparent order (do not hand-write it) | [rendering-objects.md](rendering-objects.md) (sections for each system), [rendering.md](rendering.md#haze-and-land-light-the-common-api) |
| Order of the original frame | [original-frame.md](original-frame.md) |
| Magic: spells and chants, casting from the hand, gestures, worship, influence, alignment, reactions, fire | [magic.md](magic.md) |
| A specific miracle (food, water, heal, forest, flocks, fireball, lightning, shields, teleport, storm, lightning explosion) | [miracles.md](miracles.md) |
| Particles: types, PSys classes, creators, particle sound, which rule is where | [particles.md](particles.md) |
| Which cell is water; nearest coast, river or drinking water; the creature's `LandAvoid` | [water.md](water.md#water-cells-seacells) |
| Sinking and drowning, sharks, the fish puzzle, the missionaries' boat, waterfall, what sounds in the water | [water.md](water.md) |
| Cameras and tracks of `camera.edt`, shark routes | [camera-tracks.md](camera-tracks.md) |
| How the script moves the camera (MOVE_CAMERA_*, HAS_CAMERA_ARRIVED, lenses) | [script-camera.md](script-camera.md) |
| The intro video or the spell-drop video, skipping it with ESC, the .bik files | [video.md](video.md) |
| The Land 1 intro, the advisors, the dialogue texts, the tutorial's script functions | [intro.md](intro.md) |
| Enabling or writing a mod | [mod-library.md](mod-library.md) |
| Test hooks (`OPENBLACK_*`), building, debugging | [openblack-internals.md](openblack-internals.md) and the «Test hooks» section of each page |

## How the pages are written

Each page starts with what it covers and an index. Each topic carries its status: **faithful** (verified in the original),
**(approximate)**, **(inferred)**, **mod/own** or **pending**. At the end: **Pending**, **Test hooks** and
**Sources** (reports in `C:\Users\diewgarc\dev\documentacion\…`). A topic lives on a single page; the others link to it.
Addresses and figures are never deleted when editing. Details in `C:\Users\diewgarc\dev\documentacion\equipo\WIKI_PLAN.md` §2.

## Philosophy

1. First everything original, verified in the executable: openblack is a replica, with no assumptions. Every constant or
   rule carries its address or its source data; what cannot be read is marked and stated.
2. Whatever departs from the original goes in as a **mod**, disabled by default, in the mod library (`src/Mods/`,
   the game's **Mods** menu, [mod-library.md](mod-library.md)). Not to be confused with the modified `AllMeshes.g3d` of the
   installation ([mods.md](mods.md)).
3. Verify before accepting something as good: build, tests and automatic screenshots (without the mouse if the user is using
   the PC).
4. Several sessions work at the same time: guidelines in `C:\Users\diewgarc\dev\TEAM_GUIDELINES.md`, build and
   commit protocol in `BUILD_PROTOCOL.md` and board in `TEAM_STATUS.md` (same folder).

## Local external references

- `C:\Users\diewgarc\dev\bw1-decomp`: matching decompilation from openblack/bw1-decomp; `config\BW1W120\symbols.txt`
  gives names to the addresses.
- `C:\Users\diewgarc\dev\decomp_pickup`: reconstructed pseudo-C++ of the hand, the interface and the objects
  (`hand.cpp`, `interface.cpp`, `objects.cpp`, `multi.cpp` and their `NOTES_*.md`).
- `C:\Users\diewgarc\dev\documentacion`: disassembly scripts, dumps and reports by topic.
- `C:\Users\diewgarc\dev\herramientas`: own tools (`lnd_*` scripts, screenshots, Real-ESRGAN).
