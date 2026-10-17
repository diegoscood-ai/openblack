# Reading this wiki

## Summary

This wiki collects what was found out about how the original Black & White (2001) works, read from the frozen
v1.42 unofficial-patch `runblack.exe` and from the original data, and how openblack rebuilds it. Start with the area
you are interested in below. Every page has the same four sections: **Summary** (what the system is and what
openblack does with it today), **The original** (the behaviour, formulas and addresses), **openblack** (status,
files, 🎯 chosen differences and the page's test hooks) and **Pending** (what is unknown or not ported, each item
with what would settle it). A topic lives on one page; the others link to it. Why openblack
aims at the same behaviour and not at the same code is on [A reconstruction, not a decompilation](reconstruction.md).

## The original

### Pages by area

**Engine**

- [The engine loop: turns and frames](../engine/loop.md): the original loop call by call, the interface's packets,
  threads, proving two runs are the same game, game statistics, and openblack's loop.
- [The game clock](../engine/game-clock.md): game time, turn length, speed and pause.
- [Random numbers and streams](../engine/random.md): the generator, the four streams, seeds, and the creature's
  draws.
- [Floating point](../engine/floating-point.md): the x87 at 24-bit precision, the double constants, reproducing the
  results bit for bit.
- [Coordinates, distances and angles](../engine/coordinates-and-angles.md): MapCoords, the GUtils distances and
  angles.
- [Matrices and the Zoomer](../engine/matrices-and-zoomer.md): the LH matrices and the Zoomer interpolator.
- [Object size and map cells](../engine/object-size-and-map-cells.md): how big an object is, and the per-cell
  object lists.

**Formats**

- [info.dat](../formats/info-dat.md): the layout of the tables several systems read.
- [Land and script files](../formats/land-and-script-files.md): LND, `camera.edt`, land-script lines, `.dnc`.
- [Media files and data packs](../formats/media-files.md): `.sad`, `.sas`, the MP2 music segments, `AllAnims.anm`,
  the installation's modified packs.

**Scripts**

- [The land script reader](../scripts/map-script-reader.md): LHScriptX, towns and players, creation commands,
  flocks, fish farms, built percentage, simulation data.
- [Land script objects](../scripts/map-script-objects.md): lanterns, bonfires, dead trees, gates, reward chests,
  deleting by script.
- [CHL commands that move things](../scripts/script-commands.md): MOVE_GAME_THING, SET_POSITION, script states,
  DELETE, ScriptManage slots, the virtual machine.
- [Land changes, skipping the tutorial and saving](../scripts/land-change-and-saving.md): LOAD_MAP, the SkipBox,
  writing land-script lines.
- [The Land 1 intro](../scripts/land1-intro.md): the flow, dialogue texts, CALL look-ups and the JC specials.

**World**

- [Terrain](../world/terrain.md), [Sea, coast and rivers](../world/sea-coast-and-rivers.md),
  [Water](../world/water.md), [Sea life and fixed scenery](../world/sea-life-and-scenery.md).
- [Trees and forests](../world/trees.md), [Objects](../world/objects.md), [Buildings](../world/buildings.md),
  [Plans and building sites](../world/building-sites.md), [Fire](../world/fire.md), [Vortexes](../world/vortex.md).
- [Day and night](../world/day-and-night.md), [Sky](../world/sky.md), [Weather and climate](../world/weather.md),
  [Haze and mist](../world/haze-and-mist.md).

**Beings**

- Villagers: [data and states](../beings/villager-states.md), [deciding what to do](../beings/villager-decisions.md),
  [jobs](../beings/villager-jobs.md), [home, food, sleep, age and births](../beings/villager-life.md),
  [reactions](../beings/villager-reactions.md), [picked up, dropped, hurt and dead](../beings/villager-hand-and-death.md),
  [speed and scripts](../beings/villager-speed-and-scripts.md).
- [Towns](../beings/towns.md), [Animals: species and the turn](../beings/animals.md),
  [Animals: reactions, flocks and bodies](../beings/animal-life.md), [Dances](../beings/dances.md).
- [Routes and footpaths](../beings/routes.md), [Walking round obstacles](../beings/wall-hugging.md),
  [Skeletal animation](../beings/animation.md).

**The creature**

- [The creature](../creature/overview.md) (start here), [body](../creature/body.md), [mind](../creature/mind.md),
  [plans and needs](../creature/plans-and-needs.md), [mimicry](../creature/mimicry.md),
  [pose and animation](../creature/pose-and-animation.md), [drawing](../creature/drawing.md).
- [The leash](../creature/leash.md), [the leash's rope](../creature/leash-rope.md),
  [the temple's leash posts](../creature/temple-leash-posts.md), [casting miracles](../creature/casting.md),
  [fights](../creature/fights.md), [hits and catches](../creature/hits-and-catch.md),
  [under a script](../creature/script-control.md), [the player's creature](../creature/players-creature.md),
  [the tattoo editor](../creature/tattoos.md).

**Magic**

- [The spell core](../magic/spell-core.md), [Gestures](../magic/gestures.md),
  [Casting from the hand](../magic/hand-casting.md), [Worship](../magic/worship.md),
  [Influence and alignment](../magic/influence-and-alignment.md),
  [The temple and the citadel](../magic/temple-and-citadel.md), [The village totem](../magic/village-totem.md).
- The miracles: [food, wood, water and heal](../magic/food-wood-water-heal.md), [forest](../magic/forest.md),
  [flocks](../magic/flocks.md), [fireball and lightning](../magic/fireball-and-lightning.md),
  [lightning explosion](../magic/lightning-explosion.md), [shields](../magic/shields.md),
  [teleport](../magic/teleport.md), [storm and tornado](../magic/storm-and-tornado.md),
  [the falling spell](../magic/falling-spell.md).

**Physics and particles**

- [Physics](../physics/physics.md), [Physics impacts](../physics/physics-impacts.md).
- [Particles: the PSys core](../physics/particles.md), [Particle creators](../physics/particle-creators.md),
  [Particle rules](../physics/particle-rules.md).

**Hand, camera and interface**

- The hand: [states](../hand/states.md), [picking](../hand/picking.md),
  [grabbing, holding and throwing](../hand/grab-hold-and-throw.md),
  [animation and placement](../hand/animation-and-placement.md).
- [The game camera](../camera/game-camera.md), [The player camera](../camera/player-camera.md),
  [The script camera and camera tracks](../camera/script-camera.md).
- [Advisors and help](../interface/advisors-and-help.md), [Screen, text and read-outs](../interface/screen-and-readouts.md),
  [Script highlights](../interface/script-highlights.md#script-highlights), [Tooltips](../interface/tooltips.md),
  [The temple interior](../interface/temple-interior.md).

**Rendering**

- [The frame and its draw stages](../rendering/frame.md), [Visible blocks and level of detail](../rendering/visible-blocks-and-lod.md),
  [The object draw list](../rendering/object-draw-list.md).
- [Meshes and level of detail](../rendering/meshes-and-lod.md), [Model lighting and colour](../rendering/model-lighting.md),
  [Textures](../rendering/textures.md), [Transparency and render modes](../rendering/transparency-and-render-modes.md),
  [Shadows and reflections](../rendering/shadows-and-reflections.md).

**Audio and video**

- [The audio engine](../audio/engine.md), [Banks and channels](../audio/banks-and-channels.md),
  [Music](../audio/music.md), [Voices and texts](../audio/voices.md), [Script audio](../audio/script-audio.md),
  [World sounds](../audio/world-sounds.md), [Magic and weather sounds](../audio/magic-and-weather-sounds.md),
  [Creature and interface sounds](../audio/creature-and-interface-sounds.md).
- [Bink video](../video/bink.md), [Film playback](../video/playback.md).

**The project**

- This page; [Research tools](research-tools.md); [Testing and verifying a change](testing.md);
  [Testbed scenarios](scenarios.md); [Debugging openblack](debugging.md); [openblack internals](internals.md).
- [Refactor conventions](refactor-conventions.md) and the port decisions per area:
  [engine and world](port-decisions-engine-world.md), [beings](port-decisions-beings.md),
  [the creature](port-decisions-creature.md), [magic](port-decisions-magic.md),
  [hand, camera and interface](port-decisions-hand-camera-interface.md),
  [audio, video and scripts](port-decisions-audio-video-scripts.md).
- [The minimal mod loader](mod-loader.md); [Progress](progress/README.md).

### Progress: what our tree does, row by row

The [progress notes](progress/INDEX.md) break each part of the game into everything the original does, one row per
behaviour, and mark each row done, partial, todo or n/a for this tree, with the place in our code. They were written
by raffclar ([raffclar/openblack](https://github.com/raffclar/openblack)) and re-marked for this tree. The wiki
pages remain the source of truth for how the original behaves; a topic page's openblack section names the progress
rows it closes. Bugs of the original found so far are in [progress/vanilla_bugs](progress/vanilla_bugs/README.md).
The progress tables are regenerated by their tool; no page quotes their counts, which go stale.

### Where do I look for…?

| Topic | Page |
|---|---|
| An address or symbol of `runblack.exe`, xref, the oracle, the disassembly scripts | [Research tools](research-tools.md) |
| Coordinates and cells, distances and angles | [Coordinates, distances and angles](../engine/coordinates-and-angles.md) |
| Terrain height and normal | [Terrain](../world/terrain.md) |
| Matrices, the Zoomer | [Matrices and the Zoomer](../engine/matrices-and-zoomer.md) |
| An object's radius and height, the objects in a cell | [Object size and map cells](../engine/object-size-and-map-cells.md) |
| The turn and the frame dt, the game speed | [The game clock](../engine/game-clock.md), [The engine loop](../engine/loop.md) |
| Random draws and their streams | [Random numbers and streams](../engine/random.md) |
| Picking up, dropping, throwing, the cursor | [Grabbing, holding and throwing](../hand/grab-hold-and-throw.md), [Picking](../hand/picking.md) |
| Food and wood, store, fields, piles and pots | [Objects](../world/objects.md), [Buildings](../world/buildings.md) |
| Trees and forests (tug, replanting, growth, fire, sacrifice) | [Trees and forests](../world/trees.md) |
| What the land script (CHL) creates, fogs, herds, street lamps, towns | [The land script reader](../scripts/map-script-reader.md), [Land script objects](../scripts/map-script-objects.md) |
| Hits, damage, buildings that break, dropping from the hand | [Physics](../physics/physics.md), [Physics impacts](../physics/physics-impacts.md) |
| Vortexes, objects carried by the tornado | [Vortexes](../world/vortex.md) |
| Animations and speed of villagers and animals | [Skeletal animation](../beings/animation.md), [Villagers: speed and scripts](../beings/villager-speed-and-scripts.md) |
| Villager data and states | [Villagers: data and states](../beings/villager-states.md) |
| Dances: the `.dnc` files, a dance's groups and clock, the scripts' dance commands | [Dances](../beings/dances.md), [Land and script files](../formats/land-and-script-files.md#the-dnc-format) |
| Animal behaviour | [Animals: species and the turn](../beings/animals.md) |
| The creature: info.dat tables, mind files, its random draws, stand-in constants | [The creature](../creature/overview.md), [info.dat](../formats/info-dat.md#the-creature-tables-in-infodat), [Random streams](../engine/random.md#random-streams) |
| Time of day, lit windows, weather, storms, rain | [Day and night](../world/day-and-night.md), [Weather and climate](../world/weather.md) |
| How the world is drawn (terrain, sea, sky); whether it already matches the original | [The frame and its draw stages](../rendering/frame.md), [Sky](../world/sky.md) |
| How a model is drawn (materials, lighting, reflections, shadows, sprites, smoke) | [Meshes and level of detail](../rendering/meshes-and-lod.md), [Model lighting and colour](../rendering/model-lighting.md), [Shadows and reflections](../rendering/shadows-and-reflections.md) |
| Which API to use for a billboard, an animated texture, something stuck to the ground, a blend mode or the transparent order (do not hand-write it) | [Meshes and level of detail](../rendering/meshes-and-lod.md), [Transparency and render modes](../rendering/transparency-and-render-modes.md), [Haze and land light: the common API](../world/haze-and-mist.md#haze-and-land-light-the-common-api) |
| Order of the original frame | [The frame and its draw stages](../rendering/frame.md) |
| Object draw list: which objects the original draws each frame, when the list is rebuilt, who sets the rebuild count | [The object draw list](../rendering/object-draw-list.md#the-object-draw-list-stage-4m) |
| Magic: spells and chants, casting from the hand, gestures, worship, influence, alignment, reactions | [The spell core](../magic/spell-core.md) and the magic pages above |
| A specific miracle | the miracle pages under **Magic** above |
| Particles: types, PSys classes, creators, particle sound, which rule is where | [Particles: the PSys core](../physics/particles.md) |
| Which cell is water; nearest coast, river or drinking water; the creature's `LandAvoid` | [Water](../world/water.md#water-cells-seacells) |
| Sinking and drowning, sharks, the fish puzzle, the missionaries' boat, waterfall, what sounds in the water | [Water](../world/water.md), [Sea life and fixed scenery](../world/sea-life-and-scenery.md) |
| Cameras and tracks of `camera.edt`, shark routes | [The script camera and camera tracks](../camera/script-camera.md), [Land and script files](../formats/land-and-script-files.md#the-file) |
| How the script moves the camera (MOVE_CAMERA_*, HAS_CAMERA_ARRIVED, lenses) | [The script camera and camera tracks](../camera/script-camera.md) |
| The intro video or the spell-drop video, skipping it with ESC, the `.bik` files | [Film playback](../video/playback.md), [Bink video](../video/bink.md) |
| The Land 1 intro, the advisors, the dialogue texts, the tutorial's script functions | [The Land 1 intro](../scripts/land1-intro.md), [Advisors and help](../interface/advisors-and-help.md) |
| The installation's modified meshes and textures | [Media files and data packs](../formats/media-files.md) |
| Test hooks (`OPENBLACK_*`), building, debugging | [Debugging openblack](debugging.md), [openblack internals](internals.md) and the "Test hooks" section of each page |

### How to read the pages

- Everything stated is verified in the executable or measured, unless it is marked:
  - **🔍 inferred**: deduced from the code or the data, but not read directly;
  - **⚠️ approximate**: openblack comes close to the original but does not match it exactly;
  - ✅ faithful: openblack's code was checked to give the original's result;
  - **📼 1.20 only**: a rule read with Lionhead's 1.20 symbols that the v1.42 patch did not touch, or a 1.20-only rule kept as
    a note;
  - **🧪 verify with xref/oracle**: a claim not settled yet, with the command that settles it, listed in Pending.
- Addresses, original names and formulas appear only under "The original", in code spans. Every constant or rule
  carries the address it was read at, or its source data. Addresses and figures are never deleted when editing.
- Dates are kept only on a decision of the user ("user, 2026-10-15"), not on readings.
- Every page ends with a **Pending** section: what is not ported yet, what is still unverified, and open questions;
  a difference from the original that is found becomes a measured fix, never a standing note.
- A topic lives on a single page; the others link to it with one sentence. Links between pages are relative
  (`../magic/shields.md#the-object`).

#### Which exe is the original

"The original" is the frozen v1.42 unofficial-patch `runblack.exe` (sha1 `1e73ba9a96c3…`, the copy in
`dev\_gamedata\frozen` and every copy of the game here), not Lionhead's 1.20 (sha1 `bccccbd3a08f…`, bw1-decomp's
reference, not on the machine). bw1-decomp is built from 1.20 and its BW1W120 symbols name the functions; where the
patch changed code, the exe's bytes win and Lionhead's rule is a note (user, 2026-10-15). On top of v1.42 that exe carries two community patches, and it stays the original with them (user, 2026-10-17): the **Infinite Drawing Distance** patch (NOPs at 0x823810 and 0x823B43; 100000 in place of 700, 5 and 1 at [0xC37200], [0xC37EC8] and [0xC3813C]) and the **Creature Growth Patch** (a NOP at 0x4EF5E9). The plain published v1.42 is sha1 `9c9aea96…`; where it differs, the page says so in a note. The patch's readme lists:
the low-spec detection fixed (textures and landscape), addons activating, windowed mode, skipping the tutorial on a
fresh install (25 NOPs in the skip-tutorial requester: the SkipBox always shows, see
[Skipping the tutorial](../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)),
every secret creature unlocked (the secret-creature test `fn_00711D90` returns 1 and is NOP-filled, so
`IS_CREATURE_AVAILABLE` is true for every type: `GScript::CreatureAvailable` 0x6F5170 maps the types 3, 6, 14, 15,
16 to indices 0..4 through the byte table 0x6F51F4, pushes 1 for the rest at 0x6F51CA, and calls `fn_005551D0`
0x5551D0, which asks `fn_00711D90(index)`; in v1.42 its bytes are `B8 01 00 00 00 C3` and 42 `NOP`s, 0x711D90..0x711DBF
[V]), and resolutions above 1080p. The detail levels and the drawn model's level of detail
may also be the patch's ([Meshes and level of detail](../rendering/meshes-and-lod.md)).

**Precedence when sources disagree**: the bytes of the v1.42 exe win; then bw1-decomp (its C, and its symbols as the
xref index serves them); then the newest measured finding (an oracle run, a check, a dated reading of the exe); then
the newest text. A claim the text alone cannot settle is marked **🧪 verify with xref/oracle** and goes to Pending.

### Status marks

Every fact about the original carries how sure we are of it. The viewer shows the marks as coloured pills; on GitHub
they read as text.

| Mark | Meaning |
|---|---|
| ✅ faithful | openblack reproduces it exactly, checked against the original |
| ⚠️ approximate | close to the original, but not exact yet |
| 🔍 inferred | deduced from the code or the data, not yet confirmed |
| 🧪 verify | an open question, with the xref or oracle command that would settle it |
| 📼 1.20 only | a rule of Lionhead's 1.20 that the v1.42 exe changed; kept as a note, not followed |
| 🎯 chosen difference | different from the original on purpose, with the date it was chosen |

### How the code is written

openblack is a replica: first everything original, verified in the executable, with no guesses. The code is modern
C++20 on EnTT: state lives in ECS components and Locator services, assets load through the resource caches, and
comments describe behaviour in plain English, with no decompiled names or addresses (those belong in this wiki). The
conventions are in [Refactor conventions](refactor-conventions.md), and how a change is verified (tests, the
fidelity run, pixel-identical screenshots) in [Testing and verifying a change](testing.md).

### External references

- [openblack/bw1-decomp](https://github.com/openblack/bw1-decomp): matching decompilation of `runblack.exe` (from
  Lionhead's 1.20); `config/BW1W120/symbols.txt` gives names to the addresses.
- [raffclar/openblack](https://github.com/raffclar/openblack): the tree whose architecture openblack's code follows
  ([Refactor conventions](refactor-conventions.md)).

## openblack

The wiki is written in `docs/wiki/` of the repository, one folder per area as above; images are in `docs/wiki/img/`
and keep their names. The viewer opens this page first. Code cites a page section by its anchor, so a heading that
code or another page links to keeps its anchor (an old anchor stays as an alias when a heading is reworded).

openblack differs: `IsCreatureAvailable` leaves the type on the script stack and pushes false, where v1.42 pops it and
pushes true (`src/CHLApi.cpp:4941-4947`), a fix row.

### Test hooks

None of its own: every debug environment variable and test hook is listed in [Debugging openblack](debugging.md),
and each page's own in its "Test hooks" section.

## Pending

- The progress folder's links ([progress/vanilla_bugs](progress/vanilla_bugs/README.md) and the per-domain files)
  resolve once the folder has moved whole under `project/progress/`.
- openblack differs: `IS_CREATURE_AVAILABLE` should pop the type and push true for every creature
  (`src/CHLApi.cpp:4941-4947`), a fix row; see [The creature](../creature/overview.md).
