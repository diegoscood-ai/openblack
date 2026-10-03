# openblack internals (local branch)

## Build, test, run

- `C:\Users\diewgarc\dev\openblack\build_openblack.bat configure|build` (from bash:
  `cmd //c "C:\\Users\\diewgarc\\dev\\openblack\\build_openblack.bat build"`). **After adding .cpp files, `configure`**
  (the sources are collected with GLOB).
- Tests in `cmake-build-presets\ninja-multi-vcpkg\bin\Release`: `test_camera` (11 recordings of the original),
  `test_set_camera_pos`, `test_game_initialize`, `test_load_scene`, `test_mobile_wall_hug`, `test_fixed`,
  `test_zoomer`, `test_lh_matrix`, `test_land_normal`… (all the `test_*.exe`). They must all pass.
- Portable copy: `B&W\BnW_openblack\openblack.exe` + `Jugar.bat`.
- Without `-W/-H`, the window takes 85 % of the usable desktop and is centred; larger sizes are reduced.
- openblack starts **paused**: the scripts and the pathfinding do not run until it is unpaused.

## Map of the hand code

| File | Contents |
|---|---|
| `HandSystem.cpp` | Press state machine, animation, public interface |
| `HandPlacement.cpp` | Hand geometry, `Place`, object under the cursor, `ResolveCursorPoint` (ORHP) |
| `HandHolding.cpp` | Picking up, poses, spring, dropping, throwing, thrown objects |
| `HandResources.cpp` | Piles, pots, taking in batches, putting down, stores |
| `HandTrees.cpp` | Uprooting, roots, replanting, dead trees |
| `HandEffects.cpp` | Dust when grabbing, particles when picking up (grain, wood, fish sparkles) |
| `HandFish.cpp` | Splash when grabbing the water, fishing in the fish farms |
| `HandDebugHooks.cpp` | All the test environment variables |
| `Common/Zoomer` | Zoomer and Zoomer3d from LH3DLib ([engine-math.md](engine-math.md#zoomer-lh3dlib)) |
| `3D/ObjectMatrix` | `lh_matrix`: LHMatrix constructors ([engine-math.md](engine-math.md#lh-matrices)) |
| `3D/LandNormal` | `land_normal`: LH3DIsland::GetNormal ([engine-math.md](engine-math.md#terrain-normal)) |
| `ECS/StoragePitStore` | Store logic |
| `ECS/StaticGrounding` | Mod: settle static objects |
| `Graphics/TextureMipmaps` | Mod: mip chain on the CPU |
| `Archetypes/PotArchetype` | Create pots/piles, `SetSize`, `UpdateSizes` |

## Render

- Instance matrix (mat4 per object): the `w` of the rotation columns are reused:
  - `[0][3]` = 1 − opacity (`components::Alpha`; with the table 0xC387C8, in the queue if its mesh has the flag 0x200 and
    otherwise immediately in the main view);
  - `[1][3]` = V texture offset (`components::UvScroll`);
  - `[2][3]` = `components::MeshTint` (1e6 and above; see mod-library.md). The footprint shader only uses xyz of those columns.
- `MorphWithTerrain` (the original: `LH3DObject::UpdateMelting` 0x8168F0; objects of 3D type 1 = morphable, see
  `dev\documentacion\morph\morph_notes.txt`; the common API is `land_morph`, see
  [rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph)): each vertex rises `GetAltitude(xz del vértice) − GetAltitude(xz del origen)`,
  so the object's own height (a pile or a field sinking) is preserved. vs_object computes the exact GetAltitude
  (the 4 unfiltered corners of the cell, its `split` diagonal and the flattening next to the sea) with the RG32F height
  map (altitude, split; the 17×17 cells of each block). Before, it was bilinear and misaligned by half a cell (errors of
  8-50 units at the map edges: gaps under fields and buildings). The objects that fade out (`Alpha`) stay
  stuck to the terrain (in the original they go through the same `Draw`).
- The buildings that do **not** follow the terrain (houses of all tribes, mill, dispenser, totem, wonders of
  tribes 1, 2, 5 and 6) are sunk when created (`Abode::CallVirtualFunctionsForCreation` 0x403270, in
  `AbodeArchetype::Create`): down to the lowest ground under the 4 xz corners of their mesh's box
  (`GetAltitudeFondation` 0x63ABC0, never above the origin), at most `max(0,2·radio 2D, 0,8)` (radius =
  scale × the largest half-extent in x or z, 0x638180). It replaces the script's altitude. Only with an island loaded
  (`UnloadedIsland` throws in `GetHeightAt`). The temple flattens the terrain around it
  (0x882730: flat up to 35 units, blend up to 70; done in `CitadelArchetype::FlattenLandUnderTemple`).
  **Deviation:** openblack does it on load (it creates the temple already built); the original, when turning the plan
  into a temple (`CitadelHeart::Create` → 0x4675A0 → 0x882730; `dev\documentacion\mapa\flecos_citadel.md`). **Pending:**
  the entrance (`Entrance.l3d`) follows the terrain.
- **Town centre totem** (`components::TotemStatue`, `CreateTotemStatue` in AbodeArchetype.cpp; notes in
  `dev\documentacion\totem\totem_notes.txt`): `TownCentre::CreateTotemIfNecessary` 0x743DA0 → `TotemStatue::Create`
  0x737CC0. Two static meshes without sinking: the tribe's pedestal (`InfoConstants.totemStatue[tribu].plinth`,
  BuildingPlayerIconPlinth*) at special point 6 of the centre (`GetTotemPos` 0x743F20, with the centre's matrix and
  raised like its morph), with its Y angle and scale; on top (+2.729, 0x999A9C) the icon: the player's creature
  (BuildingPlayerIcon<Especie>) or, without a creature, the hand (BuildingSpellHand, the only one there is now). They rise
  `8 × fracción de culto` (Draw 0x738960; no worship yet: 0). The beliefs (GBelief::DrawBelief 0x438800) go at
  `y del punto 6 + alto de la malla del icono × escala` (Object::GetHeight 0x638120). Pending: facing the worship
  site (AddToPlayer 0x738130) and the creature icon.
- The instance buffer grows with a margin and is uploaded with `bgfx::copy` (with `makeRef` and a `resize` freed memory
  was read: artefacts when creating and destroying meshes every frame).
- `L3DSubMesh` keeps positions and indices on the CPU (`GetCollisionPositions/Indices`) for picking and measurements;
  `L3DMesh::RayIntersect` does the triangle test.
- Textures: if a material asks for a non-existent texture and the mesh carries an embedded skin, that one is used (mods).
- **Trap**: ninja does not recompile the variants that only `#include` their base when the base changes
  (`vs_object.sc`, `vs_static_shadow.sc`…): `vs_object_instanced*.sc`, `vs_object_hm_instanced*.sc` and
  `vs_static_shadow_instanced_static.sc` (7 files) have to be touched (`touch`). With old shaders objects
  disappear (all of them, or the trees). `dev\verify_head.bat` already does it before building (2026-10-01).

- `RenderContext::entityInstances`: entity → (mesh, instance index, `morphWithTerrain`,
  `receivesDynamicShadow`), to draw a specific entity (reflections, shadow on objects).
- `LandIslandInterface::GetUnflattenedHeightAt`: `GetAltitude` without the sea flattening (search for the fish farms).

- **Trap: Vulkan uniform buffer.** On every call bgfx copies the whole vertex shader uniform block
  into a per-frame buffer of 128 B × 65535 = 8 MB, without checking it in Release. `vs_object` with `u_model[128]` is
  ~8 KB per call: with ~1000 calls it overflowed and crashed in `ScratchBufferVK::write` (Kapa's Land1). Meshes without
  bones use the `*_static` variants (`BGFX_CONFIG_MAX_BONES 1`, `Renderer::StaticVariant`); when adding object
  shaders, create their variant too.
  Meshes with up to 32 bones (villagers 22, almost all animals) use the `*B32` variants
  (`Renderer::BonesVariant32`): each villager or animal with a pose is its own draw, and those outside the view
  are not drawn (`SphereInView` in the instance loop). In addition bgfx is patched (vcpkg overlay
  `vcpkg-overlay-ports/bgfx`, `raise-vulkan-limits.patch`, enabled in `CMakePresets.json` with
  `VCPKG_OVERLAY_PORTS`): the descriptor set pool goes from 1024 to 8192 per frame in flight (with ~2000-2700 draws
  it ran out and crashed in `getDescriptorSet` inside the driver: Greek, Tibetan, Demon, Kapa's Land1, Ultimate Sandbox)
  and the uniform buffer from 128 to 512 B per draw (32 MB). Hooks: `OPENBLACK_DRAW_STATS=1` (draws per frame in the
  log) and `OPENBLACK_TEST_MAP_CYCLE="<frames>:<guion>,<guion>..."` (loads the next script every N frames, like the
  "Load Island" menu; paths relative to Scripts, e.g. `Playgrounds/TwoGods.txt`).
- **Trap: `bgfx::makeRef` on local data.** bgfx reads them later; use `bgfx::copy` unless the buffer lives
  until after the next `bgfx::frame()` (three cases in `LandIsland::LoadFromFile`, already fixed).

## Debugging a crash

- `Common/CrashHandler`: an unhandled exception or `std::terminate` write the stack to stderr and to
  `openblack_crash.txt` (working directory). With names and lines only if the `.pdb` is next to it: build
  `RelWithDebInfo` with `C:\Users\diewgarc\dev\herramientas\build_rwdi.bat` (output in `bin\RelWithDebInfo`).
- Unexplained crash (30-09-2026): a RelWithDebInfo build crashed when loading Land1 with 0xC0000005 in
  `btCollisionWorld::updateSingleAabb` (`stepSimulation`, from `Game::Update`). The cause is unknown and so is whether it
  is already fixed; if it reappears, look for a `btCollisionObject` freed without `removeCollisionObject`.
- `OPENBLACK_FLUSH_LOG=1`: the log is written line by line (the last lines before a crash are not lost).
- Scripts: `LHScriptX::Script` skips the line it does not understand (`ScriptError`, `LexerException`) and logs it
  ("line skipped"); the skirmish maps contain typos (double quote, empty argument, stray words) that the
  original tolerates. An integer is valid where a decimal is expected and vice versa; integers are saturated.
  `CREATE_BASE_WITH_ANGLE` does not exist yet (it is skipped). The gods' `.lnd` files store in `blockSize` the size of all
  the blocks together.

## Mods

Library in `src/Mods/` ([mod-library.md](mod-library.md)). The mods write `EngineConfig` switches
(`msaa`, `textureMipmaps`, `anisotropicFiltering`, `livingWater`, `groundStaticObjects`) that the engine reads.
- `world.ground-statics`: lowers each static object until its lowest vertex touches the ground (remembers by how much in
  `MobileStatic::groundedDrop`). Known limitation: a rock resting on a tip still looks like it is floating; it
  will be solved with physics.

## Debug environment variables

`OPENBLACK_PROFILE=<s>` (profiler summary in the log), `OPENBLACK_CAMERA_FLY="ox,oy,oz,fx,fy,fz"` (the flight
takes a few thousand frames: use `-n 8000 --screenshot-frame 7900`; with `--mod game.skip-intro=off` the Land 1 intro
fades to black at around 90 s of play (`SetAviSequence` / `ObjectDelete` in the log) and at 7900 the shots come out
black or half faded, so there use `-n 6000 --screenshot-frame 5900`, with the flight already still, and check that
the shot is not black; the clouds with `OPENBLACK_CLOUD_SEED` are born the same but advance with the real milliseconds,
so two shots cannot be compared pixel by pixel in the sky), `OPENBLACK_DUMP_COAST_ALPHA=1` (or `=<fichero>.png`: dumps the coastal alpha texture, x to the right and z
downwards from the island's first block, which is printed in the log; compare with `documentacion\agua\sea_coast_alpha.py`, which
starts at block 0), `OPENBLACK_DUMP_BLOCK_TEXTURE=1` (or `=<fichero>.png`: the RGBA block texture of the whole
island, colour and coastal alpha, same orientation; compare with `documentacion\agua\re\cmp_block_dump.py`),
`OPENBLACK_PRINT_ALTITUDE="x,z"` (game height, of the drawn mesh and unflattened,
physical terrain and nearby objects), `OPENBLACK_MARK_LOWEST=1` (marks the lowest vertex of the nearby rocks),
`OPENBLACK_SEA_TRACE=1` (every 500 frames, the sea rows: first row, n, 1/z and its step, smooth top row,
sea frame and drift; see [rendering.md](rendering.md#sea-skyraw--skyaraw)),
`OPENBLACK_DUMP_STATIC_GAPS=1`,
`OPENBLACK_DUMP_LAND_AVOID=1` (or `=<fichero>.png`: the creature's `LandAvoid` mask when loading the landscape, one
pixel per cell, green 0, blue 6, red 1, grey 2; see [water.md](water.md#the-creatures-landavoid-mask)), `OPENBLACK_HAND_TRACE=1`, `OPENBLACK_HAND_TEST_ROCK="x,z"`
(+ `_FOOD`, `_NO_BOULDER`), `OPENBLACK_HAND_TEST_TREE="x,z[,dead][,roots][,store]"`,
`OPENBLACK_TEST_TREE_GROWTH="x,z"` (two saplings there, one in a forest and another without a forest: only the first grows),
`OPENBLACK_TREE_TRACE=1` (each growth step, the trees a forest plants, the bending of crowns and the glow),
`OPENBLACK_TEST_REPLANT="x,z,grados"` (drops there a tree tilted by those degrees and says whether it is replanted, falls with physics
or stays dead),
`OPENBLACK_HAND_TEST_STORE_TAKE="madera,comida"`, `OPENBLACK_HAND_ANIM=<nodo>`, `OPENBLACK_NO_PICKUP_PSYS=1`,
`OPENBLACK_HAND_TEST_HOLD=<escala>` (the hand starts holding a rock),
`OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"` (the hand holds a rock 0, a pot of 300 food 1 or of wood 2,
the first villager 3, the first tree 4 or the first animal 5, and drops it gently there after those seconds of game time: over the sea it must fall with physics, see
[physics.md](physics.md#water-in-impacts-and-when-dropping)), `OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (`villager|animal|tree|pot|rock`: creates that object at that height, 2 by default, above the point and puts it into physics with no velocity; the log gives the cell, the density, the radius and `GET_LAND_HEIGHT` there and on the reference land, and the drowning villager's counter every 100 turns; with `OPENBLACK_PHYSICS_TRACE=1` it can be seen sinking, see [water.md](water.md#sinking-drowning-and-being-deleted); with `OPENBLACK_TEST_CUT=1` the object also carries `components::CutByPlane` and its part under the water is drawn cut in 0xFF303070, see [rendering-objects.md](rendering-objects.md#cutting-by-the-water-plane-drawcutbyplane)), `OPENBLACK_TEST_SHARK=1` ([water.md](water.md#sharks-class-whale); the sharks part of `FollowUs` in Land 1: two `SharkArchetype` in `CONVERT_CAMERA_FOCUS(221)` and `(230)` with `WALK_PATH` along tracks 21 and 20 of `camera.edt`; `="pista,cámara[,adelante[,desde[,hasta]]]"` a single one; see [camera-tracks.md](camera-tracks.md)), `OPENBLACK_WALK_PATH_TRACE=1` (every turn of every `WALK_PATH`: sample, segment, t, focus point and position set), `OPENBLACK_TEST_JC_SPECIAL="6[,modo[,fotogramas[,ms]]]"` (the missionaries' boat, [water.md](water.md#the-missionaries-boat-petitnavire), `PLAY_JC_SPECIAL(6)`, those frames after having a landscape; mode 1 starts on the crossing; `ms` advances the boat in one go in 33 ms steps for shots at a given moment, e.g. `6,0,7880,7200` with `-n 8000 --screenshot-frame 7900` and `OPENBLACK_CAMERA_FLY="1892,14,3176,1866,6,3161"` gives the splash; `OPENBLACK_BOAT_TRACE=1` writes mode, time, hull and sprites every 500 ms), `OPENBLACK_TEST_BUILT_PERCENTAGE="p"` (the Land 1 ArkDryDock where `TheMissionaries` creates it, with `BUILT_PERCENTAGE` = p; camera `1905,22,3180,1881,8,3154`), `OPENBLACK_TEST_FISH_PUZZLE="x,z[,dentro]"` (the script's `PuzzleGame` 14 at (x, 0, z), processed once: bait, net of 7 floats and the 2 shoals; when the net closes the next turn writes `PuzzleGame 14 played`; the shoals only count with the camera closer than 300; in Land 4 `2497.9,3628.35`; with `dentro` = 1 all the fish start at the bait and the net closes after 500 ms; with `OPENBLACK_HAND_TRACE=1` it writes `Fish puzzle: inside N/30` and `net closed`, see [water.md](water.md#fish-puzzle)), `OPENBLACK_TEST_SPLASH="x,z"` (one hand
splash per second), `OPENBLACK_AUDIO_TRACE=1` (each start of `Audio/SamplePlay` with mode, owner, channel and gain, each channel stopped and why, each sample that ends and each 3D cut by distance; the `AudioManager` emitters no longer exist since phase B5
of the audio),
`OPENBLACK_HAND_TEST_FISH=1` (splash and
fishing in the first shoal with the action held for 3 s; with `OPENBLACK_HAND_TRACE=1` it writes `Fish trace`), `OPENBLACK_START_PAUSED=1` (starts paused like the old openblack; by default the game runs from the first frame, like the original: turns and
scripts from the first frame), `OPENBLACK_TEST_FADE="r,g,b,segundos"` (`SET_FADE`), `OPENBLACK_TEST_VIEW_VILLAGER="n[,distancia[,ángulo]]"` (the camera looks at villager n from that distance and side; `dev\herramientas\shot_villager.sh` launches it from a copy in `dev\hdp_run` so as not to lock the exe of the other sessions; the villagers walk, so from far away they can leave the frame; beware: it sets the view only once, when there is a landscape, and the ~5 s intro of Land 1 then moves the camera, so in a late capture the view is no longer on the villager: it is not useful for BEFORE/AFTER captures, use `OPENBLACK_CAMERA_FLY`, which holds it every turn), `OPENBLACK_TEST_WIDESCREEN=1`, `OPENBLACK_TEST_CHIMNEY=all` (all the chimneys give off smoke even if nobody is at home; [rendering-objects.md](rendering-objects.md#chimney-smoke-lh3dsmoke)), `OPENBLACK_CLOUD_SEED=<n>` (calls `game_random::crt::Srand(n)` once before the first sky: it seeds the CRT `rand()` stream that the whole game shares, not one of the clouds' own; (inferred) the sky only comes out the same if the previous CRT draws are the same; there is no longer a time seed: the original's only `srand(time)`, 0x577721 in fn_005776E0, only runs when saving creature.lhp), `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (target of the sky alignment: −1 evil, 0 neutral, 1 good; instead of the debug slider), `OPENBLACK_LOG_ISOK=1` (one `isok:` line in the log for each script tree, pot or mobile object that `IsOkToCreateAtPos` does not allow to be created, with what blocks it; see [map-loading.md](map-loading.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)), `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` (changes the HD-Tweaks mod at that frame, like the menu; `dev\herramientas\shot_hand.sh` for the hand), `OPENBLACK_SCENERY_TRACE=1` (the fixed scenery of Land 3/4, `ECS/DesignedScenery`: creation, land change and each ring of the waterfall with its V; see [water.md](water.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)), `OPENBLACK_SOUND_TAG_TRACE=1` (`Audio/SoundTags`: each tag created, deleted, released or delayed, and every 50 turns the channel of each one), `OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` / `_LANTERN` / `_NO_WIDESCREEN` (camera on a villager or on a lantern, forced clip, without the widescreen filter: [audio.md](audio.md#test-hooks)), `OPENBLACK_ATMOS_TRACE=<n>` (every n turns the lines of the original `GSoundMap::Dump` — `Sound Map Calc Update X=%d Z=%d %s Count=%d`, `Sound Map At Hand ...`, `Sound Radius=%3.3f Distance=%3.3f DistanceAboveLand=%3.3f` — and of `ProcessAtmosBanks` — `%s Vol=%3.3f Sent=%d Step=%d` per bank —, plus an "(openblack)" line with the 14 volumes and their cells; and each loop start and release of the ambience), `OPENBLACK_LANTERN_SOUND_TRACE=1` (writes `Lantern sound:` in the log: start, cut and release of each lantern's loop with its distance, and every 50 turns the number of lanterns, whether it is night and the distance of the nearest one; test it with `OPENBLACK_TIME_OF_DAY=22` and the camera within 5 units of the tip of a lantern). `OPENBLACK_PSYS_SOUND_TRACE=1`: trace of the PSys sounds (start with action, size, surface, distance and sample from spells.sad; "too far" if the camera is farther than the sample's maxDist; loop release or cut when the atom dies; deletion), see [magic.md](particles.md#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp). Influence ([magic.md](magic.md#influence-m1i-srcecsinfluence)): `OPENBLACK_TEST_INFLUENCE="x,z[;x,z...]"` writes in the log, on turns 2 and 100, what `GET_INFLUENCE(0, 0, pos)` would give at each point (with the raw value, whether there is an anti ring, and the radii of citadels and towns); `OPENBLACK_TEST_INFLUENCE_RING="x,z,radio[,anti[,jugador]]"` creates on turn 1 a ring like `INFLUENCE_POSITION`; `OPENBLACK_INFLUENCE_EVERYWHERE=1` is the original's "GatheringFlag" flag (influence 1 everywhere). Time and weather ([magic.md](day-night-weather.md#weather-and-climate-m6a-srcecsweather)): `OPENBLACK_TEST_WEATHER="x,z,radio[,lluvia[,fundido[,temperatura[,sheetMin,sheetMax[,forkMin,forkMax]]]]]"` (the sheet lightning and forked lightning intervals of the descriptor +0x30..+0x3C; with them `GWeather::Update` starts the flash fn_00837290, which the miracle storm never requests: fn_006D5730 leaves them at 0) registers on turn 1 a static storm made like that of the storm miracle (inner `max(radio, 60)`, outer `max(2,5·radio, interior + 20, 80)`, corrected by the storm miracle lane: the three `fcomp; test ah, 0x41; je` of fn_006D5730 keep the value only if it is larger, rain 100, cloudiness 80, 20 degrees, almost infinite lifetime, fade 1 s) and writes on turns 2 and 30 what `GClimate::ComputeWeather` returns at the centre, inside, at the two radii and outside (temperature, rain, snow, cloudiness, wind in bytes and in m/s, `GetMaxRainingOrSnowing` and `GetTemp`); the grid is 40 m, so points in the same cell give the same value. `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` writes the same at those points. `OPENBLACK_WEATHER_TRACE=1` writes each game day the climates (temperature and target, wind, rain desire, dry and raining days, storms), every 50 turns the storms, and once per second what the rain draws (`Rain: N baldosas`). Worship and one-shot miracles ([magic.md](magic.md#worship-where-miracles-come-from-m7-srcworship-ecssystemsimplementationsvillagerworship)), all on turn 1 except where stated: `OPENBLACK_TEST_WORSHIP_SITE="<TRIBU>[,<SEMILLA>...]"` creates that tribe's worship site at the player's citadel, like `CREATE_WORSHIP_SITE`, with an icon for each seed (the scripts do not give the human player any built town centre, so its citadel would never have its own site); `OPENBLACK_TEST_WORSHIP_PLAYER="<n>"` makes these hooks act as player n instead of the human; `OPENBLACK_TEST_TOWN_SPELL="<ciudad>,<MAGIA>[;...]"` is `SET_MAGIC_IN_OBJECT(ciudad, magia, 1)` (the town stores the magic and its owner enables it); `OPENBLACK_TEST_MANA="<cánticos>"` is `GAME_SET_MANA` at that player's first worship site; `OPENBLACK_TEST_WORSHIP="<ciudad>,<fracción>"` is `Town::SetWorshipPercentage` (what dragging the totem does); `OPENBLACK_TEST_TAP_ICON="<SEMILLA>[,turno...]"` taps the player's icon for that seed on those turns (by default turn 5) and writes the magic, what is needed, the result, the store and whether it stays charging; `OPENBLACK_TEST_TAP="x,z,turno"` taps the nearest tappable object to that point (an icon, a town centre icon or a one-shot ball); `OPENBLACK_TEST_DISPENSER="<ABODE>,x,z,<MAGIA>[,segundos]"` creates a dispenser like the Land1 challenge script (`GiveSpellDispenserReward`); `OPENBLACK_TEST_FIREFLY_REWARD="x,z[,n]"` rolls the fireflies reward n times there. `OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"` puts the camera there every turn (in Land 1 the script takes the camera with START_CAMERA_CONTROL and its intro waits for MOVE_GAME_THING; without `OPENBLACK_CAMERA_LOCK`, `OPENBLACK_CAMERA_FLY` also holds the camera at its end point every turn; it is used by the mod [test.miracle-dispensers](mod-library.md#testmiracle-dispensers)). `OPENBLACK_WORSHIP_TRACE=1` writes each worship site when it is created (player, tribe, slot, position and angle), its icons, and every turn its chant count (`icons`, `N` dancers, `C` capacity, `k` intensity, tension, battery and maximum, available and damage per dancer), as well as the charging of the icons, the dispensers' orbs and the villagers who go to worship. Physics: see [physics.md](physics.md#test-hooks).

Miracles ([magic.md](magic.md#spell-core-m1-srcmagiccore-srcmagicspells-srcecseffects)), once on the
first turn with the map loaded: `OPENBLACK_TEST_SPELL="<magia>,x,z[,radio[,duración[,jugador[,curl]]]]"` casts like
`SPELL_AT_POS` (the creator is the neutral player, which replenishes the chants; with `jugador` 0..7 that player casts,
which does not replenish, and -1 is the neutral one; radius 10 and the duration of `timerWhenPlayerCasting` if not given
(-2 also requests it); "from" 30 m above the point; `curl` is the script's, PSysProcessInfo +0x34, from which the shields'
spins come) and writes what the class check (vt 0x30) would answer there; `OPENBLACK_TEST_SEED="<semilla>[,pu]"` puts a
charged seed in the hand, like `OneOffSpellSeed::CreateSpellIntoHand`; `OPENBLACK_TEST_ONESHOT="<semilla>,x,z[,pu[,tap]]"` creates
a one-shot ball on the ground (with `tap`, it taps it and it goes to the hand). `<magia>` is a MAGIC_TYPE number or the name
from info.dat (`FIRE`, `HEAL`, `STORM_PU2`...); `<semilla>` a SPELL_SEED_TYPE number or its name (`FIRE`, `HEAL`,
`STORM`...). `OPENBLACK_SPELL_TRACE=1` writes every turn `Spell trace` per spell (chants, safety level,
strength, cost of the turn, age, closed, PSys and atoms), each applied event and the heal target searches.
With it (or with `OPENBLACK_HAND_TRACE=1`) `Pot::AddResourceToPos` writes `Pot trace` (which pile or store each
amount goes into, at what distance and with what radius, and the new piles with their 2D radius) and each grain of food or wood
`SpellResource event` (units, chants paid, dry land, strength), and `Grain trace` gives every turn the elevation of the
stream's hand (t, height, tilt and whether it is fixed); see
[miracles.md](miracles.md#food-and-wood-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource).

Heal (M4, [miracles.md](miracles.md#heal-m4-m4h-magicspellsspellhealcpp-psysruleshealcpp)):
`OPENBLACK_TEST_HURT_VILLAGERS="x,z,radio,vida[,envenenado[,turno[,curar[,repetir]]]]"` sets to `vida` (0..1) all the
villagers within `radio` of (x, z), poisons them with a 1, and with `curar` 1 (HEAL) or 2 (HEAL_PU_ONE) casts the
miracle right there like `SPELL_AT_POS`. It happens on turn `turno` (1 by default) and repeats every `repetir` turns (0 = only
once; useful so that there is a lit chakra in the capture). Then it writes each change: `Heal test: turn +N villager
E life a -> b, poisoned p, glow (specular) r,g,b` (the chakra's glow; `none` when there is no chakra).

Casting from the hand and gestures (M2, [magic.md](magic.md#casting-from-the-hand-gestures-and-hand-effects-m2-srcmagicgestures-srcmagichand-handspellseedcpp)):
- `OPENBLACK_TEST_CAST="press@t0,release@t1[,press@t2,release@t3...][,shot@t]"` presses and releases the action (the right
  button) at those seconds from when the map exists, added to the real mouse. With a seed in the hand
  (`OPENBLACK_TEST_SEED`) it goes through the real path: arms and casts on release (HAND_GESTURE), casts on press
  (HAND_POSITION) or casts every turn while held (IN_HAND). The hand is wherever `OPENBLACK_MOUSE_AT` says.
  `shot@t` (at the end) requests a capture at that moment to `OPENBLACK_TEST_SHOT_PATH`, for captures at a game time and
  not at a frame (the frame rate varies).
- `OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`: during the first press the hand travels along that line over the terrain (the
  food, wood and water sprinkle only releases when it is moved).
- `OPENBLACK_TEST_THROW_VEL="vx,vy,vz"`: the hand velocity (ThrowVelocity of the interface state) that the
  spell receives when cast (the fireball leaves with it).
- `OPENBLACK_TEST_GESTURE="<GESTO>[,tamañoPx[,cx,cy[,t]]][;<GESTO>...]"` or `=<fichero.txt>` ("x y" pixels per line):
  draws the gesture's first template (GESTURE_TYPE name or number; a leading `-` mirrors it) `tamañoPx`
  wide (200) centred on the window fraction (cx, cy) (the centre), t seconds of game time after the map exists
  (0.5), as mouse messages every 28 ms. When it finishes it writes which gestures the buffer matches. The camera must not move
  (an `OPENBLACK_CAMERA_FLY` flight clears the buffer).
- `OPENBLACK_GESTURE_TRACE=1`: recognised gestures (template, mirror, segment), circles (position and size), help
  events and casts from the hand (also with `OPENBLACK_SPELL_TRACE=1`).

Fire and lightning bolt (M5, [magic.md](magic.md#fire-m5-srcecsfire) and [miracles.md](miracles.md#fireball-and-lightning-m5-magicobjectsmagicfireball-psysrulesfireballlightning)):
`OPENBLACK_TEST_FIRE="x,z,T[,clase[,turno]]"` sets the object with fire data nearest to (x, z) to temperature T
(`SetTemperature`) or, if T ≤ 1, sets it on fire with that speed (`SetOnFire`); the class can be `any`, `tree`, `abode`,
`villager` or `field`, and the turn (counted from the first one with the map loaded) allows waiting for the camera to arrive.
`OPENBLACK_FIRE_TRACE=1` writes the new fires, the objects that are consumed, the deleted fires, the reactions and
the villagers who flee or put out fires, and every 20 turns what each burning object draws (`Fire: graphic of fire ...`:
flames, scale and alpha of the first one, steam and smoke), which serves to check the flames without a capture. The
lightning bolt and the fireball are cast with `OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,x,z,radio,duración"` and `"FIREBALL,x,z"`; with a long
duration (for example 300 s) the lightning keeps striking until the camera arrives. `OPENBLACK_SPELL_TRACE=1` writes each
type 3 event of the tips with their numbers and, per turn, `Lightning: N targets ... M of K forks struck` with the origin, the
heading of the cone and the scale of the forks. `OPENBLACK_PSYS_CHAIN_TRACE=1` writes per frame the PSys ribbons
(how many, with how many joints, their texture and where they go from and to), which is the way to tell "the ribbon is not
drawn" apart from "in that frame there were none" (the lightning flickers: a fork is only visible on the turn it strikes).

`OPENBLACK_ZSORTER_TRACE=1` writes once per second (every 60 drawn frames) a `ZSorter trace:` line with
what the frame's single transparent queue holds (`graphics::zsorter`, `Renderer::DrawPass`): the total, the
entries lost to the 0x800 cap (`NewZObject` 0x83F31C), how many there are of each class (models, faded, clouds,
rain tiles, boat sprites, sprites, meshes and chains of the `Sorted` effects, `Queued` effects, mists,
smoke, sprites, hand) and the key (squared
distance) of the first and the last; see
[rendering-objects.md](rendering-objects.md#the-single-transparent-queue-lh3dzsorter).

`OPENBLACK_SHADOW_TRACE=1` writes once per second the projected shadows (`graphics::shadow_list`): for each
`ShadowInfo` (`shadow <n> caster <entidad> light <tipo> alpha <a> fade <f> box (...) kMin <k> t' <t> max n <n>
points <p>`, or why it is not drawn), the objects that receive one (`shadow receiver: instance <i> mesh <m> shadows <n>`)
and in which view it is drawn on them (`shadow on object: ... view <v>`: 4 = Main, immediately; 5 = MainBlended, inside their
queue entry). `OPENBLACK_DUMP_SHADOWS=<carpeta>` saves every 300 frames each texture ×8 as PNG
(`shadow_<fotograma>_<n>_<entidad>.png`). See [rendering.md](rendering.md#projected-shadows-shadowinfo).

`OPENBLACK_ORB_TRACE=1` writes, **every drawn frame** and from `Renderer::DrawScene` (right after sorting the
list from back to front), two kinds of line in the log with the `graphics` logger:

- `Orb trace: surface (<textura>) path <p> sorted <k>/<n> origin (x, y, z)` for each `ZR_SurfRevol` surface (the
  dispenser disc, the teleport pool): the path of its effect (0 `Sorted`: drawn immediately, before
  the whole queue, `k` = −1; 1 `Queued`: `k` is the position of its effect's entry) and the effect's origin.
- `Orb trace: orb <entidad> phase <p> frame <f> packed[1][3] <v> uv (u, v) alpha <a> sorted <k>/<n> key <d> sortPoint (x, y, z) inView <b>`
  for each `components::OneOffSpellSeed` (the bubble of a one-shot ball): the phase and the frame of its 4×4 sheet
  (`OneOffSpellSeed::UpdateFrame` 0x72A570), the packed value it carries to the shader in `[1][3]`
  (`frame_anim::PackUvOffset`) with the UV it represents, the alpha (1 − `[0][3]`), whether it entered the sorted list and at what
  position with what key (−1 = it did not enter: the list leaves out instances with alpha 0), the sort point of
  `OneOffSpellSeed::Draw` 0x518E90 and the result of the view volume test. If the entity has no instance
  this frame it writes `Orb trace: orb <entidad> has no instance this frame`.

It serves two purposes: checking that the dispenser disc is drawn **before** the bubble and
following the bubble through the 15 → 0 wrap-around of its sheet. The scene is
`OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826,2670,10,2"` with
`OPENBLACK_CAMERA_LOCK="1816,52,2656,1826,37,2670"` (script in `dev\documentacion\unify\scripts\shot_orb.sh`, with `RUN_DIR` = a private copy of the exe). It is about 2
lines per frame, so it is advisable to limit the frames with `-n`.

Teleport ([miracles.md](miracles.md#teleport-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)): `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,jugador[,modo]]"` plants two teleport stones like `SPELL_AT_POS` (B at x1,z1 and A at x0,z0; player 7 = neutral and free, 0 = PLAYER_ONE spends chants). `modo`: `walk` (default, the villager nearest to A walks towards B two turns earlier and A's reaction diverts it through the stones), `drop` (a second later the villager is dropped onto A, forced jump like `fn_005FC4F0`), `none` (only the stones). `OPENBLACK_TEST_TELEPORT_TURN=<n>` delays the start (the flight of a capture takes ~160 turns). `OPENBLACK_TELEPORT_TRACE=1` (or `OPENBLACK_SPELL_TRACE=1`) writes the stones, the distribution of the reaction, the jumps (from which stone to which, the saving in metres) and the spell's `PayFor` (a useful jump adds chants, a forced backwards one costs, R13). The discs use `ZR_SurfRevol` (`RendererSurfRevol.cpp`).

Forest ([miracles.md](miracles.md#forest-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)):
`OPENBLACK_TEST_MAGIC_TURN=<n>` makes `OPENBLACK_TEST_SPELL`, `_SEED` and `_ONESHOT` wait for game turn n (so
that the `OPENBLACK_CAMERA_FLY` camera is already there); `OPENBLACK_TEST_FOREST_SHOT="<turnos>,<ruta.png>[;<turnos>,<ruta>...]"`
requests a capture those turns after the forest seed touches the ground (the frame number of
`--screenshot-frame` varies with the drawing speed; this request replaces the command-line one if they coincide).
Example: `OPENBLACK_TEST_MAGIC_TURN=330 OPENBLACK_TEST_SPELL=NATURE,1790,2625
OPENBLACK_CAMERA_FLY=1772,52,2604,1790,36,2625 OPENBLACK_TEST_FOREST_SHOT="3,a.png;120,b.png"` with `-n 11000`.

Water ([miracles.md](miracles.md#water-m4a-magicspellsspellwater-psyscreatorsmist)): `OPENBLACK_TEST_SPELL=WATER,x,z,10,6` (or
`WATER_PU1`; the cloud appears 30 m above the point, the drops fall around the point) and
`OPENBLACK_TEST_WATER_SHOT="<turnos>,<ruta.png>[;...]"`: captures those game turns after the spell's first `Process`
(also while it is closing). With `OPENBLACK_SPELL_TRACE=1` each drop writes its point, its distance, the watered
objects and the last ring; each field, its crops, growth and food; a tree, the sapling it plants; a burning object,
reaction 34. With the hand: `OPENBLACK_TEST_SEED=WATER OPENBLACK_TEST_CAST="press@20,release@32"
OPENBLACK_TEST_CAST_PATH="x0,z0,x1,z1"`. Putting out a fire: `OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"` and the water
on turn 200 (`OPENBLACK_TEST_MAGIC_TURN=200`), camera `1810,36,2620,1818.6,31,2628.4`.

Flocks ([miracles.md](miracles.md#flocks-m4c-magicspellsspellflock-psysrulesflockcpp)): `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` or `GROUND_FLOCK,x,z` (24 / 25; from 30 m above the point, the neutral player, so they head towards +x alternating sides) and `OPENBLACK_TEST_FLOCK_SHOT="<turnos>,<ruta.png>[;...]"` (captures those game turns after the cast). With `OPENBLACK_SPELL_TRACE=1` each animal writes its departure and destination and, every 10 turns, each member its position, state and alpha. To make the camera follow them: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90" OPENBLACK_TEST_ANIMAL_SPECIES=20 OPENBLACK_TEST_VIEW_LOCK=1` (22 the wolves).

Shields ([miracles.md](miracles.md#shields-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)):
`OPENBLACK_TEST_SHIELD_SHOT="<turnos>,<ruta.png>[;...]"` requests a capture those game turns after the first
MapShield is created (like the forest one). `OPENBLACK_TEST_SHIELD_FRAMES="<turnos>,<n>,<prefijo>[@<lento>]"` takes n captures
in consecutive frames (`<prefijo>_<i>.png`, turn and fraction in the log; `@<lento>` lengthens the turn but **not** the
PSys interpolation, see [miracles.md](miracles.md#hooks-and-screenshots)). With `OPENBLACK_SPELL_TRACE=1` it writes
`SpellShield::InitWithPos` (radius,
anti rings, reaction, town, cost per turn), every turn the physical shield (`t`, growth curve, drawn scale and
that of the object, angle, height, dying and whether it has a body in the physics) and each physical hit (momentum and chants
paid). Example of the physical shield growing: `OPENBLACK_TEST_MAGIC_TURN=300
OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,1826.8,2641.4,40,-2,-1,2" OPENBLACK_CAMERA_FLY=1720,90,2560,1826.8,40,2641.4
OPENBLACK_TEST_SHIELD_SHOT="7,a.png;10,b.png;13,c.png;25,d.png"` with `-n 16000`. A rock against it:
`OPENBLACK_TEST_PHYSICS="1745,2641.4,25,12,6,0,1.0,1"` (12 m/s: it bounces).

Lightning explosion ([miracles.md](miracles.md#lightning-explosion-and-missing-psys-classes-m6b-psysrulesexplosionkeypointsorientforestcpp)):
`OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` (also `BEAM_EXPLOSION_PU1` and `_PU2`, which spread several explosions) and
`OPENBLACK_TEST_EXPLOSION_SHOT="<turnos>,<ruta.png>[;...]"`, which requests captures those game turns after the first
step of the first explosion (like the shield one). With `OPENBLACK_SPELL_TRACE=1` it writes `Explosion: started ...`
(centre, shield margin, search radius and spiral cells) with each target (distance, radius and class), each
destroyed object (ring, exploded and deleted) and what is not ported (the meshes in pieces), and the ground mark it leaves (`Explosion: ground mark`).
Example: `OPENBLACK_TEST_MAGIC_TURN=300 OPENBLACK_TEST_SPELL="BEAM_EXPLOSION_PU2,1790,2600"
OPENBLACK_CAMERA_FLY=1700,140,2480,1790,40,2600 OPENBLACK_TEST_EXPLOSION_SHOT="12,a.png;40,b.png;80,c.png"` with
`-n 16000`.

Storm ([miracles.md](miracles.md#storm-electric-storm-and-tornado-m6-storm-magicspellsspellstormandtornado-psysrulesstorm-ecsweatherlightningflashstormclouds)):
`OPENBLACK_TEST_SPELL="STORM,x,z,60"` (or `STORM_PU1` with lightning, `STORM_PU2` with a tornado; the radius is the spell's,
clamped to 20..1000) casts like `SPELL_AT_POS` (from 30 m above: without a heading, so the storm has no wind
of its own). `OPENBLACK_TEST_STORM_SHOT="<turnos>,<ruta.png>[;...]"` requests a capture those game turns after the
first turn of the first storm; `OPENBLACK_TEST_STORM_STRIKE_SHOT="<n>,<ruta.png>[;...]"` requests it on the turn of
lightning strike no. n from the clouds (the strikes last 1 or 2 turns). `OPENBLACK_TEST_STORM_PILE="x,z,cantidad[,wood]"` puts a
pile of food (or of wood) there when the first storm is cast, for the tornado to pick up.
`OPENBLACK_TEST_STORM_CLOUDS="x,z,radio[,nubes[,negrura[,elevación]]]"` registers on the first frame with land a
storm with clouds (`GWeather::DrawClouds`: inner = radius, outer = 3 × radius, 8 clouds, blackness 0.5, elevation 160
by default; rain 100, fade 1 s). `OPENBLACK_STORM_TRACE=1` writes the registration of the miracle's storm (radii,
rain, cloudiness, wind, fade), each lightning strike (from which cloud, the next one, its lifetime), each thing the tornado picks up (what
it is, how high it rises) and, every 10 turns of each storm spell, its age, chants, the weather at its centre
(`ComputeWeather`), its atoms, the objects it carries and the flash at that point. Tornado example:
`OPENBLACK_TEST_MAGIC_TURN=200 OPENBLACK_TEST_SPELL="STORM_PU2,1826.8,2641.4,60"
OPENBLACK_TEST_STORM_PILE="1830,2650,400" OPENBLACK_CAMERA_FLY=1775,60,2595,1830,45,2650
OPENBLACK_TEST_STORM_SHOT="95,a.png;120,b.png"` with `-n 12000`.

Useful points in Land1: `1464,2016` is **open sea** (altitude 0, water cell that is not drawn), not the beach;
shore at altitude 1 at `1485,2015`; dry land at altitude 44 at `1788.4,2710`; dry sand `1478,2129`; village store
`1826.8,2641.4` (camera `1818,75,2612,1824,44,2636`); tree next to the store `1818.6,2628.4` (the ground is at 29.4 m:
camera `1810,36,2620,1818.6,31,2628.4`). The `OPENBLACK_CAMERA_FLY` flight takes about 4000 frames, which with the map
loaded are about 150-180 game turns: to see something short-lived, it is advisable to cast it with the turn parameter.

## Tests and test data

- `test_food_wood` (M3): the HandStateGrain spline and its loop, the cost of each grain, the pile sounds,
  `GetProportionRaised`, the emission of SF_Food (18 grains/s with the hand still, 1 per 2.5 m when moving it) and the reading of
  ARRAY with floats; with `OPENBLACK_GAME_PATH` the real food and wood rows.
- `test_spell_forest` (M4b): the forest spiral, the target scale, how many trees it wants, the cost and
  GetMaxObjectsToCreate; with `OPENBLACK_GAME_PATH` the real NATURE row and the `magicTreeTypes`.
- `test_shield` (M6, [miracles.md](miracles.md#shields-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)):
  the clamping and the cost of the radius, the ProcessShield curves, the sphere helpers (inside, crossing, intersection,
  bounce), the DefensiveSphere registration with a real effect and `DoAnyShieldDeflections`, and the frame of the PSys
  hierarchies (the marked ancestors, with their scale); with `OPENBLACK_GAME_PATH`, the real rows of both shields.
- `test_influence`: the ring curve, the town and citadel radius, the anti rings and those that follow an object.
- `test_lightning` (M5, [miracles.md](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-psysruleslightningcpp)): that
  `UR_Lightning`, `UR_LightningStrike`, `ParticleChainCreator` and `ParticleLightMapCreator` are registered, the
  properties they read and the per-segment UV of the ribbon; with `OPENBLACK_GAME_PATH`, the real `SF_LightningBolt*` and
  `SF_LightningStrike` and the size of the light map `.raw`.
- `test_spell_chants` (the chant economy of [magic.md](magic.md), without a world): a player's lightning bolt and the
  neutral player's, the sustained shield and the tribal power, the edge cases of the strength and the alignment distribution;
  with `OPENBLACK_GAME_PATH` also the real rows of the lightning bolt and the shield.
- `test_worship` (the worship economy of [magic.md](magic.md), without a world except a registry): the capacity and
  battery of the site by dancers, its end-of-turn count (dance intensity, battery and damage per
  dancer), the tension, the failure of the maintenance reserve, `UseChants`, the excess of `AddToChantStore`,
  how many villagers a town requests, `SigmoidThreshold` and the fireflies' probabilities.
- `test_magic_tables` (miracle tables, [magic.md](magic.md)): with `OPENBLACK_GAME_PATH=<instalación>` it also
  checks the real `Scripts\info.dat`; without it that test is skipped.
- `test_spell_sounds` (PSys sound, [particles.md](particles.md#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)): likewise, with `OPENBLACK_GAME_PATH` it also reads
  `Data\SoundAction.h`, `spells.sad` and `SF_TeleportVortex`.
- `test_weather` (time and weather, [day-night-weather.md](day-night-weather.md#weather-and-climate-m6a-srcecsweather)), without a world: the
  byte arithmetic, the date and the seasons, the fade and the `CalcAtmos` of a storm, the 40 m grid and the falloff with
  height, the deletion over two turns, `KILL_STORMS_IN_AREA`, the sum of the climates, the temperature oscillation, the
  storm that creates a climate with desire 1 and the bytes of `CREATE_WEATHER_STORM`.
- `test_water_queries` (water queries and `LandAvoid`) needs the original data: it is **skipped** if
  `OPENBLACK_TEST_GAME_PATH=<carpeta del juego>` is not defined. It loads `Scripts\Land1.txt` once for the whole suite
  (beware: the magic tests use `OPENBLACK_GAME_PATH`, this one `OPENBLACK_TEST_GAME_PATH`).
- The test terrain is generated by `lndtool` in `test/mock/CMakeLists.txt`. A bug that erased the previous points
  of the same block was fixed; the cell under the `test_set_camera_pos` camera is now flat.

## Commits with several sessions

The protocol is in `C:\Users\diewgarc\dev\BUILD_PROTOCOL.md` (build lock, turn in `commit_queue.txt`).
To commit only your hunks of a file that other sessions also touch, do **not** use
`git apply --unidiff-zero`: it places the insertions according to the working tree's lines and not HEAD's, and it broke
HEAD twice (f9c0b08c, fixed in 6fde22cf; 09fc3b05, fixed in 6a0b1a4b). Use
`dev\herramientas\commits\hunks2.py list|build <base> <archivo> <picks>` and `dev\herramientas\commits\commit_build.py <config.json>`, which assemble the commit
in a temporary index on top of HEAD.

## Mouse tests

`C:\Users\diewgarc\dev\drive*.ps1` move the real mouse (SetCursorPos, mouse_event): **do not launch them if the user is
using the PC**. To verify without the mouse: captures with `OPENBLACK_CAMERA_FLY` and the test hooks.
