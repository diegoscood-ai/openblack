# openblack internals

## Summary

Where things are in openblack's code: the map of the hand code, how the renderer packs each object's data into its
instance and draws its passes, the traps found so far, and every identifier that was renamed (from the decompiled
names to plain ones, and back to raffclar's names when his tree became the base). How to build, test and debug is in
[Debugging openblack](debugging.md); openblack's debug controls, one line each: [Not in the original](not-in-the-original.md#openblacks-debug-controls-kept-exempt-from-removal).

## The original

Nothing on this page is about the original: it maps openblack's own code. The original's names that the code
kept are listed under [Renamed identifiers](#renamed-identifiers).

## openblack

> **Code rules.** State lives in ECS components or Locator services, never in globals; assets and files load through
> the resource caches; resources are owned with RAII and standard types (`std::unique_ptr`, `std::optional`,
> `std::span`, `std::array`); unit tests live in `test/` and inject fakes rather than checking systems through the
> locator; comments describe behaviour in plain English, with no decompiled names or addresses (those belong in this
> wiki). See [the conventions](refactor-conventions.md) and [how a change is verified](testing.md).

### Third-party libraries replaced by native code

openblack used to lean on outside libraries for physics, video and audio. They are all gone: the code that replaced
them is openblack's own, and each replacement was checked to give the same result as the library before it (the
same game state, frames and samples) before the library was removed.

| Library | What it did | What does it now | Commit |
|---|---|---|---|
| **Bullet** (physics) | Rigid bodies, and the land and object under a pixel (the hand, the camera and the cursor ray) | The game's own physics moves the bodies; the land and objects under a pixel come from the original's own rays over the island | `fafd05625` (joint108) |
| **FFmpeg** (video) | Decoding the game's Bink (`.bik`) films | openblack's own Bink decoder, frame-identical to the original's Bink library. Its tables and coefficient order follow FFmpeg's Bink decoder (Konstantin Shishkov, Peter Ross; LGPL-2.1-or-later, used under GPL-3) | `ffd248787` |
| **dr_libs** (`dr_wav`, `dr_mp3`) (audio) | Decoding WAV/PCM, ADPCM and MPEG audio in the `.sad` banks | openblack's own decoders (`audio::codec`): WAV/PCM, MS-ADPCM, MPEG layer II, and layer III for mods, with the same samples on every sound and music segment | `4e109add5`, `2f0a913a9` |
| **Lua, sol2, JSON** (mods) | The old mod loader and its built-in mods | Removed with the loader; a minimal versioned `ModLoader.dll` remains ([The minimal mod loader](mod-loader.md)) | `c590b1c16` |

Do not add these libraries back. New media formats get native decoders, tested with synthetic data. The one
exception planned is a script language for the code mods that are to come later, Lua or another
([The minimal mod loader](mod-loader.md)).

### Map of the hand code

The `Hand*.cpp` files are in `src/ECS/Systems/Implementations`.

| File | Contents |
|---|---|
| `HandSystem.cpp` | Press state machine, animation, public interface |
| `HandPlacement.cpp` | Hand geometry, `Place`, object under the cursor, `ResolveCursorPoint` (ORHP) |
| `HandFrame.cpp` | The hand's matrix and the empty hand's up, the end of the placement |
| `HandHolding.cpp` | Picking up, poses, spring, releasing (the physics' part is in `src/ECS/Physics/FromHand.cpp`) |
| `HandPhysics.cpp` | What thrown trees, pots and wood do when they hit a store or come to rest; the hand's hooks of `ecs::physics::from_hand` |
| `HandApplyToObject.cpp` | The held object applied to the object under the hand (a villager dropped into a teleport stone, a tree given to a store); a spell seed or a teleport stone picked up into the magic hand |
| `HandGiving.cpp` | Giving what the hand holds to a creature: the offer, the Give packet, letting go into the creature's hand |
| `HandSpellSeed.cpp` | A spell seed in the hand: the action pressed while holding, the apply to a point or an object, the failed apply, the forced drop |
| `HandGrain.cpp` | The hand raised and tilted while a food, wood or water sprinkle pours from it |
| `HandResources.cpp` | Piles, pots, taking in batches, putting down, stores |
| `HandTrees.cpp` | Uprooting, roots, replanting, dead trees |
| `HandEffects.cpp` | Dust when grabbing, particles when picking up (grain, wood, fish sparkles) |
| `HandFish.cpp` | Splash when grabbing the water, fishing in the fish farms |
| `HandNearObject.cpp` | The object near the action's point when the click hit none; a hand demo's click found again on the recorded kind of thing |
| `HandClicked.cpp` | The last thing tapped or clicked, which `GAME_THING_CLICKED` and `POSITION_CLICKED` read |
| `HandLeashTie.cpp` | The hand's part of tying the leash: the double click, and the tap with the leash in the hand |
| `HandLeashPull.cpp` | The hand pulling a tied leash's rope: the rope found under the mouse, the rope taking the press, and the pull ([pulling a tied leash](../creature/leash.md#pulling-a-tied-leash)) |
| `HandMorph.cpp` | The hand's good and evil morph of its texture and vertices |
| `HandToolTips.cpp` | The hand's tooltips, one function per interface state |
| `HandTurn.cpp` | The hand's packets, applied at the start of the next turn, and its per-turn pass |
| `HandDebugHooks.cpp` | The test environment variables run once the landscape exists (`OPENBLACK_HAND_TEST_*`, `OPENBLACK_CAMERA_FLY`, the hand demo…); the other systems keep theirs in their own `*DebugHooks.cpp` (animals, fire, influence, the sea, villagers, weather, gestures, magic and its spells, worship) |
| `Common/Zoomer` | Zoomer and Zoomer3 from LH3DLib ([zoomer (LH3DLib)](../engine/matrices-and-zoomer.md#zoomer-lh3dlib)) |
| `3D/ObjectMatrix` | `affine`: LHMatrix constructors ([LH matrices](../engine/matrices-and-zoomer.md#lh-matrices)) |
| `3D/LandNormal` | `land_normal`: LH3DIsland::GetNormal ([terrain normal](../world/terrain.md#terrain-normal)) |
| `ECS/StoragePitStore` | Store logic |
| `Archetypes/PotArchetype` | Create pots/piles, `SetSize`, `UpdateSizes` |

### Render

- Instance matrix (mat4 per object): the `w` of the rotation columns are reused:
  - `[0][3]` = 1 − opacity (`components::Alpha`; with the table 0xC387C8, in the queue if its mesh has the flag 0x200 and
    otherwise immediately in the main view);
  - `[1][3]` = the U and V texture offsets, packed (`components::UvScroll`, `frame_anim::PackUvOffset`);
  - `[2][3]` = where the instance's bones start in the frame's bone palette, for the posed villagers and animals drawn
    from it (`bone_palette::SetFirstBone`; see below); no other object shader reads it. It held
    `components::MeshTint` until that went with the mods (`acca40a1e`). The footprint shader only uses xyz of those
    columns.
- `MorphWithTerrain` (the original: `LH3DObject::UpdateMelting` 0x8168F0; objects of 3D type 1 = morphable; the common API is `land_morph`, see
  [meshes stuck to the ground (land_morph)](../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph)): each vertex rises `GetAltitude(vertex xz) − GetAltitude(origin xz)`,
  so the object's own height (a pile or a field sinking) is preserved. vs_object computes the exact GetAltitude
  (the 4 unfiltered corners of the cell, its `split` diagonal and the flattening next to the sea) with the RG32F height
  map (altitude, split; the 17×17 cells of each block). Before, it was bilinear and misaligned by half a cell (errors of
  8-50 units at the map edges: gaps under fields and buildings). The objects that fade out (`Alpha`) stay
  stuck to the terrain (in the original they go through the same `Draw`).
- The buildings that do **not** follow the terrain (houses of all tribes, mill, dispenser, totem, wonders of
  tribes 1, 2, 5 and 6) are sunk when created (`Abode::CallVirtualFunctionsForCreation` 0x403270, in
  `AbodeArchetype::Create`): down to the lowest ground under the 4 xz corners of their mesh's box
  (`GetAltitudeFondation` 0x63ABC0, never above the origin), at most `max(0.2·2D radius, 0.8)` (confirmed: xref constants; 0.2 at 0x8AA3AC, -0.8 at 0x8AA3A8, 0x403281..0x40329D; radius =
  scale × the largest half-extent in x or z, 0x638180). It replaces the script's altitude. Only with an island loaded
  (`UnloadedIsland` throws in `GetHeightAt`). The temple flattens the terrain around it
  (0x882730: flat up to 35 units, blend up to 70; done in `CitadelArchetype::FlattenLandUnderTemple`).
  **Deviation:** openblack does it on load (it creates the temple already built); the original, when turning the plan
  into a temple (`CitadelHeart::Create` → 0x4675A0 → 0x882730).
- **Town centre totem** (`components::VillageTotem`, `VillageTotemArchetype::Create`): `TownCentre::CreateTotemIfNecessary` 0x743DA0 → `TotemStatue::Create`
  0x737CC0. Two static meshes without sinking: the tribe's pedestal (`InfoConstants.totemStatue[tribe].plinth`,
  BuildingPlayerIconPlinth*) at special point 6 of the centre (`GetTotemPos` 0x743F20, with the centre's matrix and
  raised like its morph), with its Y angle and scale, turned side on to the town's worship site when it has one
  (`VillageTotemSystem::AddToPlayer`, `village_totem::FacingSite`); on top (+2.729, 0x999A9C) the icon
  (`village_totem::IconMesh`): the player's creature (BuildingPlayerIcon<Species>) or, without a creature, the hand
  (BuildingSpellHand). They rise `8 × worship fraction` (Draw 0x738960, `village_totem::RiseOf`; see
  [the village totem in openblack](../magic/village-totem.md#openblack)). The beliefs (GBelief::DrawBelief 0x438800) go at
  `y of point 6 + height of the icon's mesh × scale` (Object::GetHeight 0x638120).
- The instance buffer grows with a margin and is uploaded with `bgfx::copy` (with `makeRef` and a `resize` freed memory
  was read: artefacts when creating and destroying meshes every frame).
- `L3DSubMesh` keeps positions and indices on the CPU (`GetCollisionPositions/Indices`) for picking and measurements;
  `L3DMesh::RayIntersect` does the triangle test.
- Textures: if a material asks for a non-existent texture and the mesh carries an embedded skin, that one is used (the user's modified pack relies on it, see [embedded textures](../formats/media-files.md#embedded-textures)).
- **Trap, fixed**: ninja did not recompile the variants that only `#include` their base when the base changed
  (`vs_object.sc`…), and with old shaders objects disappeared (all of them, or the trees).
  Since `54e47f263` each shader depends on every file it reaches through its `#include` lines
  (`openblack_shader_include_depends` in `src/CMakeLists.txt`), so an edit to a base rebuilds its variants and no
  file has to be touched.

- `RenderContext::entityInstances`: entity → (mesh, instance index, `morphWithTerrain`,
  `receivesDynamicShadow`), to draw a specific entity (reflections, shadow on objects).
- `LandIslandInterface::GetUnflattenedHeightAt`: `GetAltitude` without the sea flattening (search for the fish farms).

- **Trap: Vulkan uniform buffer.** On every call bgfx copies the whole vertex shader uniform block
  into a per-frame buffer of 128 B × 65535 = 8 MB, without checking it in Release. `vs_object` with `u_model[128]` is
  ~8 KB per call: with ~1000 calls it overflowed and crashed in `ScratchBufferVK::write` (Kapa's Land1). Meshes without
  bones use the static variants (`vs_object_static_instanced`, `vs_object_hm_static_instanced`, …;
  `BGFX_CONFIG_MAX_BONES 1`, `Renderer::StaticVariant`); when adding object shaders, create their variant too.
  Meshes with up to 32 bones (villagers 22, almost all animals) use the few-bones variants
  (`vs_object_few_bones_instanced`, `vs_object_hm_few_bones_instanced`; the shadow programs keep `…Static`/`…B32`;
  `Renderer::FewBonesVariant`): each villager or animal with a pose is its own draw, and those outside the view
  are not drawn (`SphereInView` in the instance loop). In the main view, where it leaves the same pixels, the posed
  instances of a mesh are drawn together instead, in one instanced draw that reads each instance's bones from the
  frame's bone palette texture (`Graphics/BonePalette`, `vs_object_palette_instanced`, `Renderer::PaletteVariant`).
  The hand, the creatures, a queued, blended or land-morphing mesh, a mesh with fewer than two instances in view or
  with a shadow or a read-out drawn over one of them, and every mesh where the GPU cannot fetch RGBA32F textures in
  the vertex shader, are still drawn one by one. In addition bgfx is patched (vcpkg overlay
  `vcpkg-overlay-ports/bgfx`, `raise-vulkan-limits.patch`, enabled in `CMakePresets.json` with
  `VCPKG_OVERLAY_PORTS`, and by `CMakeLists.txt` wherever vcpkg is used without the presets): the descriptor set pool
  goes from 1024 to 8192 per frame in flight (with ~2000-2700 draws
  it ran out and crashed in `getDescriptorSet` inside the driver: Greek, Tibetan, Demon, Kapa's Land1, Ultimate Sandbox)
  and the uniform buffer from 128 to 512 B per draw (32 MB). Hooks: `OPENBLACK_DRAW_STATS=1` (draws per frame in the
  log) and `OPENBLACK_TEST_MAP_CYCLE="<frames>:<script>,<script>..."` (loads the next script every N frames, like the
  "Load Island" menu; paths relative to Scripts, e.g. `Playgrounds/TwoGods.txt`).
- **Sampler defaults** (`Graphics/ShaderSamplers`, `Graphics/SamplerDefaults`, `ShaderProgram::Submit`): when the
  shaders load, each program reads the samplers its two shaders declare from their SPIR-V build (the compiler keeps
  only the samplers a shader reads), with their stages and kinds. Every draw goes through `ShaderProgram::Submit`,
  which binds a 1×1 white texture of the sampler's kind (2D, 2D array, cube, 3D) to each stage nothing was set at
  since the bindings were last dropped (a submit that drops them, `ShaderManager::DiscardBindings`, the end of the
  frame). An unbound sampler reads zero under Direct3D 11 and whatever the descriptor last held under Vulkan, which
  can hang the GPU. Measured at the port: no draw's colour depends on a sampler it leaves unbound. The
  ones left unbound are read only where the result is thrown away: `s_diffuse` of an untextured primitive
  (`fs_object`, `fs_object_temple`: the material colour replaces it),
  `s_snowDepth`, `s_snow` and `s_snowAlpha` without snow (`u_snow.x` 0, `v_snow` 0; always so for `WorldTriangles`),
  and the blend sources 2 and 7 to 9 without a source (`u_vertexBlend.x` 0). A sampler
  that is set with an invalid handle counts as set and gets no default.
- **Trap: `bgfx::makeRef` on local data.** bgfx reads them later; use `bgfx::copy` unless the buffer lives
  until after the next `bgfx::frame()` (three cases in `LandIsland::LoadFromFile`, already fixed).

### Known traps

- **A native's argument count in the runtime binding is what the game's function pops, not what the CHL syntax
  takes.** openblack's script machine drops the arguments a bound native leaves on the stack. The game's machine does
  not: what the function does not pop stays there. Two cases:
  - `GET_ACTION_TEXT_FOR_OBJECT`: the land script pushes the object, and the game's function (`GScript::GetActionTextForObject`
    0x6FA430) only pushes 828, so the object stays under it. openblack binds it with no argument to keep that, while the
    CHL language table gives it its one parameter (`test_lhvm_native_stack`, `test_chl_language`).
  - Any other native found to differ follows the same rule.

- **Two registry-context types with the same name share one slot.** The services that keep one struct per module in an
  entity registry's context (`Locator::scriptState`, `worldEffects`, `debugHooks`, `audioState`, and the particle
  system's modules) look the struct up by type. EnTT identifies a type by hashing its name as the compiler spells it
  (`ENTT_PRETTY_FUNCTION`), so two different structs that spell the same name get the same id: the second module to ask
  for its state is handed the first one's, reinterpreted as its own type, and nothing warns. What makes a name the same:
  - two structs called e.g. `State` in **anonymous namespaces at file scope**, with no enclosing named namespace, in two
    source files: **the same id**, so they collide;
  - the same two inside **different named namespaces** (`openblack::help::input_prompt` and `openblack::help::tooltips`):
    different ids, which is safe, because the enclosing namespace is part of the name.
  So a module's state struct is safe as long as no other one shares both its name and its enclosing named namespace.
  Checked with MSVC 14.44 and the EnTT the project builds against, with a three-file test program.

### Renamed identifiers

Many of our names were copied from the original game, or kept a decompiler's placeholder. They were renamed to say
what they do. The wiki now uses the new names for our code and keeps the original game's names for the original. This
table lists only the old names that appear in this wiki or in older notes, so that older material can still be followed.
Paths are under `src/`.

| our old name | new name | file |
|---|---|---|
| `game_random::LHRand`, `guidance::LHRand` | `SeededRandom` | `Common/GameRandom.h`, `Audio/Services/Guidance.h` |
| `render_modes::Mode::AlphaTexturedAlphaNz`, `SmoothAlphaNz`, `TexturedAlphaNz`, `AlphaTexturedAlphaAdditiveNz`, `TexturedChromaAlphaNz` | the same with `NoZWrite` for `Nz` | `Graphics/RenderModes.h` |
| `render_modes::Mode::ChromaJustZ` | `ChromaDepthOnly` | `Graphics/RenderModes.h` |
| `sample_play::ClearInfoList`, `AudioSystemRand` | `ClearChannelOwners`, `AlternatingRandom` | `Audio/Engine/SamplePlay.h` |
| `DllRand` (the struct; the test suite keeps the name) | `LibraryRandomState` | `Audio/Engine/SamplePlay.cpp` |
| `MusicEngine::GetInfo`, `GetMasterInfo` | `FindChannelOfBank`, `GetMainChannel` | `Audio/Engine/MusicEngine.h` |
| `MusicEngine::SetMasterVolume`, `sample_play::SetMasterVolume`, `sample_play::MasterVolume` | `SetMainVolume`, `SetMainVolume`, `MainVolume` | `Audio/Engine/MusicEngine.h`, `Audio/Engine/SamplePlay.h` |
| `audio::SetSampleMasterVolume`, `SampleMasterVolume` | `SetSampleMainVolume`, `SampleMainVolume` | `Audio/Audio.h` |
| `EngineConfig::audioMusicMasterVolume`, `audioSampleMasterVolume` (the original's registry values keep their names) | `audioMusicMainVolume`, `audioSampleMainVolume` | `EngineConfig.h` |
| tests `MusicEngineTest.MasterVolume`, `SamplePlayTest.MasterVolume` | `MusicEngineTest.MainVolume`, `SamplePlayTest.MainVolume` | `test/audio/test_music_engine.cpp`, `test/audio/test_sample_play.cpp` |
| `PhysOb` (`ECS/Physics/PhysOb.h`, `.cpp`), test files `test_physob_pose.cpp`, `test_physob_water.cpp`, suites `PhysObPose`, `PhysObWaterTest` | `PhysicsBody` (`PhysicsBody.h`, `.cpp`), `test_physics_body_pose.cpp`, `test_physics_body_water.cpp`, `PhysicsBodyPose`, `PhysicsBodyWaterTest` | `ECS/Physics/`, `test/` |
| `ecs::physics::PhysicsBody` (`ECS/Physics/PhysicsBody.h`, `.cpp`); the test files and suites keep its name | `physics::Body` (`Body.h`, `.cpp`), raffclar's name | `Physics/` |
| `physics::psys_objects` (`ECS/Physics/PSysObjects.h`, `.cpp`), the component `PSysCarried` (`ECS/Components/PSysCarried.h`), `test_psys_objects.cpp` and its suites `PSysObjects`, `PSysObjectsTest` | `particle_carried_objects` (`ParticleCarriedObjects.h`, `.cpp`), `CarriedByParticleSystem` (`CarriedByParticleSystem.h`), `test_particle_carried_objects.cpp`, `ParticleCarriedObjects`, `ParticleCarriedObjectsTest` | `ECS/Physics/`, `ECS/Components/`, `test/` |
| `CarriedByParticleSystem` (`ECS/Components/CarriedByParticleSystem.h`); only the tornado takes objects | `CarriedByTornado` (`CarriedByTornado.h`), raffclar's name | `ECS/Components/` |
| directories `Audio/GAudio/`, `Audio/LH/` | `Audio/Game/`, `Audio/Engine/` | `Audio/` |
| directory `PSys/` (the `psys` namespace keeps its name) | `Particles/` | `Particles/` |
| `advisor::Four1`, `Analyse`, `CalcKey`, the `dude` parameters | `Fft`, `MagnitudeSpectrum`, `UpdateKey`, `Settings` (raffclar's names) | `Audio/LipSync.h` (`audio::lip_sync`) |
| `spooky::SoundExCode`, `PerformSoundexComparison`, `TrySoundex`, `PlaySpooky` | `SoundexDigit`, `SoundsAlike`, `FindSoundAlikeName`, `PlaySpookyVoice` | `Audio/Services/SpookyVoices.h` |
| `GameMusic::ProcessAudioGameTurn`, `RestartMusicThing`, `IsMusicThingFinished` | `UpdateTurn`, `RestartThingMusic`, `IsThingMusicFinished` | `Audio/GameMusic.h` |
| `CalculateRadiusPointAndDistance` | `UpdateReceiver` | `Audio/SoundMap.cpp` |
| `HelpSpritesPlayNow`, `HelpSpiritSay`, `HelpSpritesCheckMoonPhase`, `HelpSpritesAlignmentProcess` | `CanPlaySpiritRemark`, `SpiritSay`, `RemarkOnMoonPhase`, `UpdateAlignmentRemarks` | `Audio/Services/Guidance.h` |
| `HelpSpritesTownBeingAttacked`, `HelpSpritesLowOnFood`, `HelpSpritesLowOnPeople`, `HelpSpritesVillagerUnhappy`, `HelpSpritesGeneralBad`, `HelpSpritesDestroyBuilding` (and the other `HelpSprites*`) | `WarnTownUnderAttack`, `WarnLowOnFood`, `WarnLowOnPeople`, `WarnVillagersUnhappy`, `RemarkGeneralBad`, `RemarkBuildingDestroyed` (the others are `Warn*` or `Remark*`) | `Audio/Services/Guidance.h` |
| `ProcessTownDesireSFX`, `ProcessHeartBeatSFX`, `ResourceDropSFX`, `TownAttackSFX` | `UpdateTownDesireRemarks`, `UpdateHeartBeat`, `PlayResourceDropRemark`, `PlayTownAttackRemark` | `Audio/Services/Guidance.h` |
| `StartRaiseTotemSFX`, `EndRaiseTotemSFX`, `MakeDiscipleSFX`, `BeliefSFX`, `BeliefSample`, `DeathInVillageSFX`, `GetRandomSampleBasedOnValue` | `StartTotemRaiseSound`, `EndTotemRaiseSound`, `PlayDiscipleRemark`, `PlayBeliefRemark`, `PlayBeliefSample`, `PlayVillageDeathRemark`, `RandomSampleForValue` | `Audio/Services/Guidance.h` |
| `audio::SamplePlayAnimEffect` | `PlayAnimationEffect` | `Audio/Audio.h` |
| `flag10` (play options, sound tags and the `PlaySoundEffect*` parameters) | `extra3DFlag` | `Audio/Engine/SamplePlay.h`, `Audio/Services/SoundTags.cpp` |
| `audio::Get3DSoundPos`, `tags::Get3DSoundPos` | `OwnerSoundPoint`, `TagSoundPoint` | `Audio/Game/AudioSystem.h`, `Audio/Services/SoundTags.h` |
| `ecs::audio_queries::GAudioAlignment`, test `AudioLaws.GAudioAlignment` | `AudioAlignmentValue`, `AudioLaws.AlignmentCurve` | `ECS/AudioQueries.h` |
| `PSysSound`, `PSysSoundPosition` | `ParticleSound`, `ParticleSoundPosition` | `Audio/Services/SpellSounds.h` |
| `SetSetSent` | `MarkMessageSetSent` | `Help/HelpMessageSets.h` |
| `HelpDude`, `HelpDudeControl`, `HelpDude::Update1` | `AdvisorSpirit`, `AdvisorSpiritController`, `AdvisorSpirit::UpdateMotion` | `Help/Spirits.h` |
| `ConvertScriptSpiritToHelpSpirit` | `HelpSpiritOf` | `Help/ScriptSpirits.h` |
| `ActualEndGameSpeed` | `RestoreGameSpeed`, since gone: raffclar's `ScriptControlSystem` keeps which task has the game speed and gives it back (`EndGameSpeed`, `TaskStopped`), and `script_control::EndGameSpeed` and `OnTaskStopped` set the speed back to 1 | `ECS/Systems/Implementations/ScriptControlSystem.h`, `Help/ScriptControl.cpp` |
| `CAnim`, `CFrame`, `Help/CAnim.{h,cpp}` | `SpiritAnimClip`, `SpiritAnimKey`, `Help/SpiritAnimClip.{h,cpp}` | `Help/SpiritAnimClip.h` |
| `help::kmicon`, `KMIcon`, `Help/KMIcon.{h,cpp}` | `help::input_prompt`, `InputPromptIcon`, `Help/InputPromptIcon.{h,cpp}` | `Help/InputPromptIcon.h` |
| `hand_fx::RemoveAllPermBands`, `DoRemoveFromHandVisual`, `SetPULevel` | `RemoveAllPermanentBands`, `RemoveHandSpellVisuals`, `SetPowerUpLevel` | `Magic/Hand/HandMagicFX.h` |
| `DoPreCastThings`, `DoPostCastThings` | `PrepareCast`, `FinishCast` | `Magic/Core/SpellSeed.cpp` |
| `DrawBurst` parameters `a5`, `a6` | `phase`, `shape` | `Magic/Objects/FallingSpell.h` |
| `magic::GetMagicTypeFromPULevel`, `GetMagicInfoFromPULevel` | `MagicTypeForPowerUpLevel`, `MagicInfoForPowerUpLevel` | `Magic/MagicTables.h` |
| `players::ProportionOfWorldPopulationWhoBelieveInMe` | `BelieverFraction` | `Magic/Core/Players.h` |
| `magic::ProcessPSysGameLoopEnd` | `ProcessSpellParticlesEndOfLoop` | `Magic/MagicLoop.h` |
| `teleport::AnyMultiMapFixedNear` | `AnyMultiCellStaticNear` | `Magic/Objects/MagicTeleport.h` |
| `worship::player::IsAtLeastOneSpellBeginToBeCharged`, `IsSpellBeginToBeCharged` | `AnySpellCharging`, `IsThatSpellCharging` | `Worship/PlayerSpellIcons.h` |
| `icon::ActualInterfaceTap`, `UpdateGraphicsWithPULevels` | `HandleValidatedTap`, `UpdatePowerUpGraphics` | `Worship/WorshipSpellIcon.h` |
| `SetPULevel`, `UpdateGraphicWithPULevels` (town centre) | `SetTownCentrePowerUpLevel`, `UpdateTownCentrePowerUpGraphics` | `Worship/TownCentreSpellIcon.cpp` |
| `Timer::MSeconds`, `SetSpeedUpFactor`, `TimerSaysDoATurn` | `ElapsedMs`, `SetSpeed`, `IsTurnScheduled` | `GameClock.h` |
| `game_packets::ProcessOneSuperpacket` | `DispatchQueuedPackets` | `Input/GamePackets.h` |
| `FloodAnalyse` | `FloodFillReachable` | `3D/LandAvoid.cpp` |
| `Time2SkyType` | `SkyType` | `3D/DayNightClock.h` |
| `billboard::VolBlend` | `VolumeBlendBasis` | `3D/Billboard.h` |
| `PSysFrameAdvance`, `PSysFrameLerp`, `PSysFrameIndex` | `ParticleFrameAdvance`, `ParticleFrameLerp`, `ParticleFrameIndex` | `3D/FrameAnim.h` |
| `DrawPSysSprites` | `DrawParticleSprites` | `Graphics/Renderer.h` |
| `Graphics/RendererPSys.cpp`, `Graphics/RendererSurfRevol.cpp` | `Graphics/RendererParticles.cpp`, `Graphics/RendererRevolvedSurface.cpp` | (files) |
| `AsssignTownFeature` | `AssignTownFeatures` | `ECS/Town/TownFeatures.h` |
| `CalculeLairPos` | `CalculateLairPosition` | `ECS/AnimalAIDetail.h` |
| `SendFootpathsAroundObsticle`, `StopGoingRoundObsticle` | `RerouteFootpathsAroundObstacle`, `StopReroutingAroundObstacle` | `ECS/Footpaths.h` |
| `CheckSatisfySuppyWorship` | `CheckSatisfySupplyWorship` | `ECS/Villager/VillagerSatisfy.h` |
| `DeleteDependancys` (fields, fish farms, scaffolds, graveyard, wonders, workshops, villager, town centre) and its tests | `DeleteDependants` | `ECS/Fields.h` and the other seven headers |
| `WomanSpecial` | `UpdatePregnancy` | `ECS/Villager/VillagerAge.h` |
| `alignment::CrudeSet`, `CrudeUpdate` | `SetClamped`, `AddClamped` | `ECS/Effects/Alignment.h` |
| `pot_resource::JustAddResource` | `AddToPotDirect` | `ECS/PotResource.h` |
| `object_resources::JustGetResource` | `PotAmount` | `ECS/ObjectResources.h` |
| `object::ObjectGet2DRadius`, `ObjectGetHeight` | `FootprintRadius`, `ObjectHeight` | `ECS/ObjectMetrics.h` |
| `abodes::GetNewEp` | `GetEntrancePoint` | `ECS/Abodes.h` |
| `workshops::IsCloseToEP` | `IsNearSpecialPoint` | `ECS/Town/Workshops.h` |
| `FireEffect::Tc`, `Tmax` | `CombustionThreshold`, `MaxBurnTemperature` | `ECS/Fire/FireEffect.h` |
| `living::GetYXZYaw` | `YawFromRotation` | `Physics/LivingRules.h` |
| `map_cells::IsMultiMapFixedClass`, `fire::traits::IsMultiMapFixed` | `IsMultiCellStaticClass`, `IsMultiCellStatic` | `ECS/MapCells.h`, `ECS/Fire/FireObjectTraits.h` |
| `SmokyStuff`, `smoky_stuff`, `SmokyStuffCell`, `ECS/SmokyStuff.{h,cpp}` | `DisappearSmoke`, `disappear_smoke`, `DisappearSmokeCell`, `ECS/DisappearSmoke.{h,cpp}` | `ECS/DisappearSmoke.h`, `3D/FrameAnim.h` |
| `petit_navire`, `PetitNavire`, `ECS/PetitNavire.{h,cpp}` | `missionary_boat`, `MissionaryBoat`, `ECS/MissionaryBoat.{h,cpp}` | `ECS/MissionaryBoat.h` |
| `script_highlight::dyk_read` | `did_you_know_read` | `ECS/ScriptHighlight.h` |
| `thing_flags`, `ECS/ThingFlags.h` | `object_flags`, `ECS/ObjectFlags.h` | `ECS/ObjectFlags.h` |
| `PSysRand` (the lightning helper) | `RandomBelow` | `Particles/Rules/Lightning.cpp` |
| `DrawOffsetLT`, `SetRefPos` | `HandDrawOffset`, `SetReference` | `Particles/ParticleEffect.h` |
| `VSNoise1To1` | `SignedValueNoise` | `Particles/Noise.h` |
| `Creator::Kind::GJMesh`, `gj_mesh` | `MeshPiece`, `mesh_pieces` | `Particles/ParticleEffect.h`, `Particles/Rules/ExplodeObject.h` |
| directory `RPlan/`, `RPlan.{h,cpp}`, `RPHolder.{h,cpp}`, `RPFollow.{h,cpp}` | `RoutePlanner/`, `RoutePlan.{h,cpp}`, `ObstacleGrid.{h,cpp}`, `RouteFollower.{h,cpp}` (`Point2D.h` keeps its name) | `RoutePlanner/` |
| namespace `rplan` | `route_planner` | `RoutePlanner/*.h` |
| `RPAvoid`, `RPHolder` | `RouteObstacle`, `ObstacleGrid` | `RoutePlanner/ObstacleGrid.h` |
| `RPlan`, `RPFollow` | `RoutePlan`, `RouteFollower` | `RoutePlanner/RoutePlan.h`, `RoutePlanner/RouteFollower.h` |
| test suites `RPlanTest`, `RPFollowTest` | `RoutePlanTest`, `RouteFollowerTest` | `test/test_rplan.cpp`, `test/test_rplan_follow.cpp` |
| `Point2D::GetNormSq`, `GetRange` | `LengthSquared`, `DistanceTo` | `RoutePlanner/Point2D.h` |
| `RPAvoid::PointIsTotallyInside` | `RouteObstacle::ContainsCircle` | `RoutePlanner/ObstacleGrid.h` |
| `RPHolder::InitialiseSystem`, `SquareDoesNotContain`, `GetSidePointOfStartObject` | `ObstacleGrid::InstallCallbacks`, `IsNotInSquare`, `StartObstacleSidePoint` | `RoutePlanner/ObstacleGrid.h` |
| `RPlan::GameTurnUpdate` | `RoutePlan::StepSearch` | `RoutePlanner/RoutePlan.h` |
| `Lh3dColour.h`, `test_lh3d_colour.cpp` | `ArgbColour.h`, `test_argb_colour.cpp` | `Graphics/ArgbColour.h`, `test/test_argb_colour.cpp` |
| namespace `lh3d_colour` | `argb_colour` | `Graphics/ArgbColour.h` |
| test suite `Lh3dColour` | `ArgbColour` | `test/test_argb_colour.cpp` |
| shader include `lh3d_colour.sh` (`LH3D_COLOUR_SH`) | `argb_colour.sh` (`ARGB_COLOUR_SH`) | `assets/shaders/argb_colour.sh` |
| GLSL `Lh3dMulShr8`, `Lh3dAddSat`, `Lh3dMul255`, `Lh3dUnpackRgb24` | `MultiplyShift8`, `AddSaturated`, `MultiplyDiv255`, `UnpackRgb24` | `assets/shaders/argb_colour.sh`, `vs_object.sc` |
| tests `MulShr8_4…`, `AddSat`, `MulShr8KeepA`, `ScaleShr8…`, `Mul255…` | the names of the functions they test | `test/test_argb_colour.cpp` |
| `MulShr8_4`, `AddSat_4` | `MultiplyArgbShift8`, `AddArgbSaturated` | `Graphics/ArgbColour.h` |
| `MulShr8_3KeepA`, `ScaleShr8_3KeepA`, `AddSat_3KeepA` | `MultiplyRgbShift8KeepAlpha`, `ScaleRgbShift8KeepAlpha`, `AddRgbSaturatedKeepAlpha` | `Graphics/ArgbColour.h` |
| `Mul255`, `Mul255_4`, `Mul255_3KeepA`, `Mul255_3OpaqueA` | `MultiplyBytes`, `MultiplyArgb`, `MultiplyRgbKeepAlpha`, `MultiplyRgbOpaque` | `Graphics/ArgbColour.h` |
| namespace `gutils` | `game_math` | `ECS/GameAngle.h`, `ECS/GameDistance.h`, `ECS/FastExp.h` |
| files `ECS/GUtilsAngle.{h,cpp}`, `ECS/GUtilsDistance.{h,cpp}`, `ECS/GUtilsExp.h` | `ECS/GameAngle.{h,cpp}`, `ECS/GameDistance.{h,cpp}`, `ECS/FastExp.h` | `ECS/` |
| `LHArcTan`, `GetLHPointFromAngle` | `AngleFromDelta`, `OffsetFromAngle` | `ECS/GameAngle.h` |
| `ConvertGameAngleToScawenAngle`, `ConvertScawenAngleToGameAngle`, `k_ScawenOffset` | `GameAngleToPhysicsRadians`, `PhysicsRadiansToGameAngle`, `k_PhysicsAngleOffset` | `ECS/GameAngle.h` |
| `StepFromAngle8` | `StepFromAngleCoarse` | `ECS/GameAngle.h` |
| `Exp24` | `ExpSinglePrecision` | `ECS/FastExp.h` |
| `ToFixedGUtils`, `ToFixedTenthTimes65536`, `JustMapXZ` | `MetresToFixedExact`, `MetresToFixedForHandLookup`, `CellStep` | `ECS/MapCoords.h` |
| test files `test_gutils_angle.cpp`, `test_gutils_distance.cpp`, `test_lh_angles.cpp` | `test_game_angle.cpp`, `test_game_distance.cpp`, `test_game_angles.cpp` | `test/` |
| test suites `GUtilsAngle`, `GUtilsDistance`, `LhAngles` | `GameAngle`, `GameDistance`, `GameAngles` | `test/test_game_angle.cpp`, `test/test_game_distance.cpp`, `test/test_game_angles.cpp` |
| tests `GUtilsAngle.LHArcTan`, `MapCoords.ToFixedGUtilsIsAnotherRounding` | `GameAngle.ArcTanTableMatches`, `MapCoords.ToFixedRoundsDifferently` | `test/test_game_angle.cpp`, `test/test_map_coords.cpp` |
| namespace `lh_matrix` | `affine` | `3D/ObjectMatrix.h`, `3D/AffineMatrix.h` |
| `Mat43`, files `3D/LhMatrix43.{h,cpp}` | `AffineMatrix`, `3D/AffineMatrix.{h,cpp}` | `3D/AffineMatrix.h` |
| `YXZ`, `AngleXYZ`, `GetYXZ`, `lh_matrix::SetPosition` | `RotationYXZ`, `RotationXYZ`, `DecomposeYXZ`, `PlacementMatrix` | `3D/ObjectMatrix.h` |
| `MulPre`, `SetInverse`, `ChangeFovScales` | `MultiplyReversed`, `Inverse`, `LensScalesFromFov` | `3D/AffineMatrix.h` |
| `PointThroughWForScreenTest`, `PointThroughWForBoxTest`, `PointThroughWForShadowBlocks`, `BoneRootThroughW` | `ToClipForScreenTest`, `ToClipForBoxTest`, `ToClipForShadowBlocks`, `BoneRootToClip` | `3D/AffineMatrix.h` |
| `CameraFrame::lh` | `clipMatrices` | `3D/Billboard.h` |
| test files `test_lh_matrix.cpp`, `test_lh_matrix43.cpp` | `test_game_matrix.cpp`, `test_affine_matrix.cpp` | `test/` |
| test suites `TestLhMatrix`, `LhMatrix43` (and the tests `TestLhMatrix.YXZ`, `TestLhMatrix.AngleXYZ`, `LhMatrix43.ChangeFovScales`) | `GameMatrix`, `AffineMatrix` (`GameMatrix.RotationYXZ`, `GameMatrix.RotationXYZ`, `AffineMatrix.LensScalesFromFov`) | `test/test_game_matrix.cpp`, `test/test_affine_matrix.cpp` |
| the file-local `Ftol` copies in `3D/LandMorph.cpp`, `Graphics/ShadowMath.cpp`, `Graphics/Renderer.cpp`, `Help/HelpTextDisplay.cpp`, `Help/SpiritsRuntime.cpp`, `ECS/Town/TownQueries.cpp`, `Magic/Objects/FallingSpell.cpp`, `RoutePlanner/ObstacleGrid.cpp` | one shared `TruncateToInt` (`map_coords::FtoL` and the `Ftol` in `ECS/TownDesire.cpp` are kept: they behave differently) | `Common/TruncateToInt.h` |

Several old names are also the original game's names (`HelpDude`, `CAnim`, `DeleteDependancys`, `PSysRand`,
`DrawOffsetLT`, `Time2SkyType`, `LHArcTan`, `ConvertGameAngleToScawenAngle`, `JustMapXZ`, `MulPre`, `SetInverse`,
`GetYXZ`, `SetPosition`...). Where a page talks about the original, it still uses them.

#### Back to raffclar's names (2026-10-07)

raffclar's tree is now the architecture base ([Following raffclar's tree](refactor-conventions.md#following-raffclars-tree)). Step 2 of the
merge plan took these names from his tree. The rows above that they undo are kept as history.

| our name before | his name now | file |
|---|---|---|
| namespace `ecs::map_coords`, files `ECS/MapCoords.{h,cpp}` | `map_coords`, `3D/MapCoords.{h,cpp}` | `3D/MapCoords.h` |
| `CellStep`, `MetresToFixedExact` | `JustMapXZ`, `ToFixedGUtils` | `3D/MapCoords.h` |
| namespace `game_math`, files `ECS/GameAngle.{h,cpp}`, `ECS/GameDistance.{h,cpp}` | `gutils`, `Common/GUtilsAngle.{h,cpp}`, `Common/GUtilsDistance.{h,cpp}` | `Common/` |
| `GameAngleToPhysicsRadians`, `PhysicsRadiansToGameAngle`, `k_PhysicsAngleOffset` | `ConvertGameAngleToScawenAngle`, `ConvertScawenAngleToGameAngle`, `k_ScawenOffset` | `Common/GUtilsAngle.h` |
| `AngleFromDelta`, `OffsetFromAngle`, `GetAngleDirection`, `GetXFromAngle`, `GetZFromAngle` | `LHArcTan`, `GetPointFromAngle`, `GetAngleSign`, `GetXByAngle`, `GetZByAngle` | `Common/GUtilsAngle.h` |
| namespace `graphics::zsorter`, files `Graphics/ZSorter.{h,cpp}` | `graphics::zsort`, `Graphics/ZSort.{h,cpp}` | `Graphics/ZSort.h` |
| `components::PlayerAlignment`, file `ECS/Components/PlayerAlignment.h` | `components::Alignment`, `ECS/Components/Alignment.h` | `ECS/Components/Alignment.h` |
| file `ECS/Components/TownInfluence.h`, `CitadelInfluence::value` | `ECS/Components/Influence.h`, `CitadelInfluence::reach` | `ECS/Components/Influence.h` |
| test files `test_game_angle.cpp`, `test_game_distance.cpp`, `test_alignment_system.cpp`; suites `GameAngle`, `GameDistance`, `AlignmentSystemTest`; test `GameAngle.ArcTanTableMatches` | `test_gutils_angle.cpp`, `test_gutils_distance.cpp`, `test_alignment.cpp`; `GUtilsAngle`, `GUtilsDistance`, `Alignment`; `GUtilsAngle.LHArcTan` | `test/` |

#### The particle family under raffclar's names (2026-10-23)

His tree calls the particle code `openblack::particles` and keeps `psys` for the particle-file component only.

| our name before | new name | file |
|---|---|---|
| namespace `openblack::psys` (the game's particle code, with `psys::manager`, `psys::explode_object`...) | `openblack::particles` (`particles::manager`...); `components/psys` keeps `openblack::psys` | `Particles/` |
| namespace `game_random::psys` | `game_random::particles` | `Common/GameRandom.h` |
| files `Particles/PSys.{h,cpp}`, `PSysFile`, `PSysManager`, `PSysManagerState`, `PSysRegistry`, `PSysWaterRings` | `ParticleEffect.{h,cpp}` (his), `ParticleSpellFile`, `ParticleManager`, `ParticleManagerState`, `ParticleRegistry`, `ParticleWaterRings` | `Particles/` |
| test files `test_psys_creator_hooks`, `_draw_path`, `_file_loader`, `_forest_camera`, `_noise`, `_sorted_queue`, `_water` | `test_particle_*` | `test/` |
| `Spell::psys`, `SpellIcon::psys`, `SpellSeed::psysPower` | `effect`, `effect`, `power` (his) | `ECS/Components/` |
| `PSysFileLoader`, `PSysFileManager`, `GetPSysFiles`, `_psysFiles`, `PSysFileDirectory` | `ParticleFileLoader`, `ParticleFileManager`, `GetParticleFiles`, `_particleFiles`, `ParticleFileDirectory` (his) | `Resources/` |
| the renderer's `psysAtoms`, `psysAtomDrawDescs`, `psysAtomIndex`, `_psysMeshes`, `framePSys`, `psysItem`... | `particleAtoms`, `particleAtomDrawDescs`, `particleAtomIndex`, `_particleMeshes`, `frameParticles`, `particleItem`... | `ECS/Systems/`, `Graphics/` |
| `InitWithoutPSys`, `PsysSoundTrace`, test suites `PSysNoise`, `PSysWater`, `PSysRegistry`, tests `GameRandom.PSys*` | `InitWithoutEffect`, `ParticleSoundTrace`, `ParticleNoise`, `ParticleWater`, `ParticleRegistry`, `GameRandom.Particle*` | `Particles/`, `Debug/`, `test/` |

`SetPSysCloseDown` keeps its name: it is a class of the particle files, and his.

### Test hooks

The debug variables are listed in [Debug environment variables](debugging.md#debug-environment-variables).

## Pending

- Sampler defaults: the four default textures are created before every other texture, so every later texture handle
  moves up by four; nothing is known to depend on handle numbers, but the vis shots are the check.
- The temple entrance (`Entrance.l3d`) follows the terrain.
