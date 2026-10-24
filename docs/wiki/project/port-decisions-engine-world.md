# Port decisions: engine and world

## Summary

Each decision taken while porting raffclar's tree in the engine, the renderer, the sky and weather, the physics, the
particles and the world systems (trees, forests, fire, vortexes): what was taken from his tree, under which of his
names and slots, and what stays ours because it is the original's behaviour bit for bit. The rule behind every entry
is the one in the project's rules: where both trees do the same job, his version is taken only once it is shown to
behave as the original; otherwise ours stays, behind his names and interfaces. The conventions these decisions follow
are on [Refactor conventions](refactor-conventions.md).

## The original

This page is about openblack's code. The original's behaviour that each decision keeps is described on the page of
its system: [Physics](../physics/physics.md), [Particles: the PSys core](../physics/particles.md),
[Particle rules](../physics/particle-rules.md), [Weather and climate](../world/weather.md),
[Sky](../world/sky.md), [Trees and forests](../world/trees.md), [Fire](../world/fire.md) and
[Vortexes](../world/vortex.md).

## openblack

The decisions, grouped by area. Each one names his file, slot or interface and says what of ours is kept under it.

### Engine, time and files

#### An animation's data size and play time

the clip header's data-size field is a size in bytes, not a time, and
takes his names (`ANMHeader::animationDataSize`, `L3DAnim::GetDataSize`); anmtool and the mesh viewer show it as
bytes. His clip-timing fix (his waits read the data size, 56036 for the couple's kiss, where the play time is
5133 ms) fixes a bug ours never had: our waits have always read the play time. What is left of that fix is
identical, and is taken: his `villager_clips::HasPlayed` and `ClipPlayed` (the original's unsigned "turns × 100 ≥
play time × times", which our signed form gave for every real clip), the play time getter's name and type
(`L3DAnim::GetPlayTime`, unsigned; the callers that work in signed numbers convert it as they did before), and
`GetBoneMatrices` wrapping on the play time instead of the data size (only the mesh viewer calls it, at time 0, to
count the bones). His clip test
(`test_villager_clips`) comes with them.

#### Gui, the files taken from his tree

`Dialog`, `TextDatabase` and `CreatureCaveScreen` are his text. `Canvas`,
`DialogPainter` and `Controls` are his text with our deltas put back, each needed for the same pixels:
- `Canvas`: his view and `Blend`/`SetBlend`, plus our coverage batches and the `InterfaceText` program that draws
  `graphics::GameFont`'s one-channel atlas. His textured `DrawShape` overload is taken as he wrote it, the plain one
  calling it with no texture, as his does; the swirling backdrop behind the new player's box draws through it
  (2026-10-18), and our did-you-know bubble is not drawn on the canvas, since the original draws it in the world;
- `DialogPainter`: his `DrawPointer(Canvas&)`, plus `enum class Edges` with `HasEdge`, `MidTextSize()` and
  `SmallTextSize()` (one bigger when the help system asks for bigger text), and `DrawString` through
  `graphics::GameFont`;
- `Controls`: the message box's fit rule, the label and edit box sizes through the text size functions,
  `IsCaretShown` as a tested pure function, `std::accumulate` for a list's height, and our `Zoomer` calls;
- `CreatureCaveScreen`: with no creature the cave's window is not drawn at all, as in the original's empty cave.

#### The time system

The time system (`Locator::time`, `TimeSystemInterface`) takes his names where ours gives the same values:
`GetTurn`, `GetTurnFraction`, `SetPaused`, `GetSpeed`, `UpdateFrame` and `GetFrameGameTime` (in
`std::chrono::milliseconds`), and his `k_TurnDuration`, asserted equal to `game_clock::k_SchedulerMsPerTurn`. The
free `game_clock::` functions keep their names. Not taken, since they differ: his `StartGameClock`, `IsTurnDue`,
`Start`, `Update`, `GetElapsedTime`, `GetFrameRealTime`, `CameraStep`, `GameTimer`, his `std::function` tick source
and his constructor.
- The interface is in his `ECS/Systems/TimeSystemInterface.h` and, with `TimeSystem`, in `openblack::ecs::systems`.
  `game_clock::TickSource` moves into that header, so it needs neither `GameClock.h` nor `Locator.h`. No alias is
  left in `openblack`.

#### A file format his tree has as a component is taken as a component

A file format his tree has as a component is taken as a component, once a test shows it reads the game's files
to the same data as our reader: `components/psys` (the spell files, the enum headers and the light map bitmaps) is
his text, and `test_particle_file_parity` keeps our former reader as the reference over synthetic files and, with
the game installed, over every file of the kind. Our loaders keep their cache slots and call the component. Where our
code adds something to his type (the spell file's name for the logs), our type derives from his rather than copying
it, and an alias keeps the name the callers use until the interface port renames them. A function of ours whose name
the component takes for a type is renamed (`SoundActionValue` → `SoundActionByName`).

#### Tables that are the same data

Tables that are the same data take his text (`Particles/ParticleTypes`: his 150 rows are ours, with a name
column and his `particles::` namespace).

#### Tutorial skip and profiles

his `tutorialSkipSystem` (`TutorialSkipSystemInterface`, his `TutorialSkipSystem`
holding a `TutorialSkip`) keeps the three flags the scripts read, which were the map script globals'; it is emplaced
with them in `InitializeGameState` and reset just before them, so it lives exactly as long. His
`playerProfileSystem` (`PlayerProfileSystemInterface`) fronts our profile's creature file: `CurrentProfileHasCreature`
calls `player_creature::ProfileHasCreature`, and `GetProfileCount` counts the profiles, which a first start reads
to ask for a new player, since his `PlayerProfiles` came in behind the same slot
([player profiles](port-decisions-hand-camera-interface.md#player-profiles)). Our SkipBox, its answer as an integer
and its showing at every new game stay; the answer is turned into the flags by `SkipFor(ClampChoice(answer))`, the
same three comparisons for every integer. His `AskNewGameChoice` (asking only a returning player) is not taken.

#### What a player keeps from land to land

his `KeepForNextLand` and `TakeUpKept` on the player service, called
where he calls them (the start of `Game::PrepareNewLand`, and `PlayerArchetype::Create`). Behind them is what we
keep today: the local player's `InterfaceAlignment` and `InfluenceCrossing`, the hand's `VirtualInfluence` (since
SET_VIRTUAL_INFLUENCE switches it, 2026-10-16) and every player's alliances (2026-10-19); his other kept fields (the
alignment on the player's entity, `Player::damageFrom`, `windResistance` and the miracles) come with the rows that
port those fields. Our alignment stays in the service until its own move onto the player's entity.

### Renderer

#### Creature shadow

his `CreatureShadow` names (`Alpha`, `LightPoint`, the fade and light constants) over our
`shadow_math` and `shadow_list`; the creature's entry takes the hand's half rows. His GPU cells and
`creature_shadow.sh` are not taken.

#### Golden scenarios from his tree

Golden scenarios from his tree (his emulations of the original code, such as `test/audio/scenarios`) are taken
as data, with the scripts that made them (not built). His test is pointed at our code: through a pure function (a
step extracted unchanged from a service, as `atmos_banks::StepBankVolume`) or through the Locator with fakes. Where
our result differs, the game is not changed to fit: the test is `DISABLED_` with a comment that names the
difference, and the difference goes to the behaviour items (and the wiki's Pending).

#### Screenshots

his `Graphics/ScreenshotCapture` (`PlanScreenshot`, `UncapturedScreenshotPath`,
`ScreenshotFailures`, `WriteScreenshotPng`, `OffscreenScreenshots`), the renderer's `TakeScreenshotFailure` and the
windowing's `IsMinimised`, as he wrote them, with his `test_screenshot_capture`. Only the passes drawn aside differ:
`OffscreenScreenshots::k_ScreenPasses` lists our passes that draw to the window (`Main` to `Cursor`, and
`OpaqueAlpha`), not his. A window that is shown takes the screen route, `bgfx::requestScreenShot` as before, and the
PNG gets the same pixels; it is now written aside and renamed once whole. His inspector's use of the failures waits
for the inspector.

#### Cloud and village light tests

his `test_clouds` and `test_village_lights` cases run on our `Clouds`,
`night_lights` and `frame_anim` functions under his test names. `Clouds::Move` is our `Update`'s step for one cloud,
moved out unchanged so that his drift case can call it. His `Strength`, `VillageStrength`, `Place` and `Threshold`
cases are left out: ours works them out inside `night_lights::StampLight`, and its cell weights are floats.

#### Land drawing

- `LandNormal` is his text.
- `BlockTexture` takes his names (`k_OpenSeaFlag`, `k_MapTexels`, `coneWeights`) and keeps our code: the rebuild of
  a box of cells for the land morph, the altitude bits, the bounds-checked `Materials` and the coast alpha in
  `CoastAlpha`.
- `LandLightTable` takes his shape: the palette is a `LandLightPalette` resource (`weather/palette`, loaded through
  `LandLightPaletteLoader` into `GetLandLightPalettes()`), `Build` takes it, and the accessors take his names
  (`GetLandColour`, `GetWarmColour`, `GetMoonColour`). Our maths stay, because our sky type runs the original's way
  (0 by day, 2 at night) and his the other: the time column `(2 - T) * 15`, the overcast cap in floats, the storm haze
  and the lightning's whole steps of 256. Our colours stay 0xAARRGGBB, with `ToColour` for 0 to 1.
- There is no global table. The renderer hands each table it builds to `renderFrameSystem`
  (`SetLandLightTable`), and the game reads the last one through `land_light::CurrentTable()`, at the same moment
  in the frame as before.
- `renderFrameSystem` and `skyFrameSystem` are behavioural services, with no `GetState()`. Their state is private to
  the implementation. The `land_light::`, `model_light::`, `mists::` and `sky_type::` free functions stay as thin
  wrappers over them. `SkyFrameSystemInterface::GetCurrentSkyType` keeps our scale.

#### Render helpers

his `sun::`, `moon::`, `ground_blobs::`, `tree_brightness::`, `HandLight`, `hand_water_glow::`
(now in `src/Graphics`), `land_colour_stamps::`, `lightning::`, `FrameLandLight`, `mists::` (`3D/Mists.h`) and
`detail_level::` names hold our maths. Where his numbers differ, ours stay:
- the moon's hour angle is `t * pi / 12`, not his precomputed constant;
- the water glow looks no further than the island's own last cell (his: always 511);
- the stamps' cell weights are worked out in floats (his in doubles);
- the lightning's dip runs from 0.2 to 0.5 seconds (his from 0.1);
- the trees are fully bright when the view or the light has no direction.

His number is taken for the hand light's strength (2026-10-10): the land colour's mean is rounded down to an
integer and the strength is (120 − mean) × 1/15, as the original
([Day and night](../world/day-and-night.md), night lights); ours kept the mean's fraction.

The old names that other code calls (`frame_anim::Mist*`, `billboard::MistShrunkSize`, `guidance::MoonPhase`)
forward to his. Not taken: his `Combine` stamp enum, `ThunderSound` and `FrameLandLightInputs` (our land light's
inputs are the renderer's).

#### Graphics infrastructure

his `UniformTable`, the dynamic `VertexBuffer` with `Bind(first, count)`, the
`FrameBuffer` multisample and wrapping flags (bgfx's caps are asked only for a multisampled one), and the L3D blend
spans, copy and `Edit*` accessors are his text. Of these, our draws use the `UniformTable` (every shader program's
uniforms, since 2026-10-08) and the L3D blend spans (the creature's vertex blends, M08); the rest is not used yet.

#### Uniform handles

a mesh draw sets its uniforms through handles cached once per program (his `FindUniform`,
the handle setters and the renderer's `MeshUniformsOf`). The list is ours (`Graphics/MeshUniforms.h`), since our
draw sets other uniforms than his. The handle setters keep our `flags` (sampler flags) and `num` (array elements)
parameters, and a uniform the program lacks is still set by name, for its one warning. Every other draw sets its
uniforms by name, as before.

#### Shader and program names

where both trees have the same shader, it takes his file and program names, with our
shader code behind them.
- The object programs' bone-budget variants are his: `vs_object_static_instanced`, `vs_object_few_bones_instanced`,
  his height-map static pair, and our height-map few-bones variant under a name in his pattern; the programs
  `ObjectStaticInstanced` and `ObjectFewBonesInstanced`, and his `Graphics/BoneBudget.h`. Which variant a mesh draws
  with stays ours: a mesh without bones takes the static one, a boned mesh with few enough bones the few-bones one.
- The bone palette is his: `Graphics/BonePalette.h` (`bone_palette::Append`, `RowsFor`, `BoneAt`, `SetFirstBone`,
  `FirstBone`, `k_Width`), the shader `vs_object_palette_instanced` with `USE_BONE_PALETTE` and `s_bonePalette` at
  stage 2, the program `ObjectPaletteInstanced` and his `test_bone_palette`. Two things differ from his, so that a
  palette draw is the uniform draw bit for bit: a bone is four texels, its matrix's columns as the uniform holds them
  (his three rows rebuild the last row as (0, 0, 0, 1)), and the first bone rides in the w of the instance's third
  column, since our first column's w is the fade (his puts it there). The shader clamps the bone index to the bones
  of the program the mesh would have used (`u_bonePalette.x`), as that program does. His second path for the animals
  (`vs_object_posed_instanced`, `u_bones`, `s_bones`) is not taken: our posed draw treats villagers and animals
  alike, so both go through the one palette. Which posed meshes are drawn through it is ours: only where the
  reordered draws are bound to give the same pixels (`Renderer::DrawPass`, `bone_palette::DrawsInAnyOrder`), see
  [Skinned and rigid sub-meshes](../rendering/meshes-and-lod.md#skinned-and-rigid-sub-meshes).
- The world quads' fragment shader and program are his `fs_world_textured` and `WorldTextured`, with our alpha test
  kept inside, since his file has none. The test also reads which alpha it compares (`u_alphaTest.y`, the
  `render_modes::AlphaSource` of `PrimitiveAlpha`): mode 15 tests the texture's alpha times the vertex alpha. The
  advisors' trail uses it with ALPHAREF 5, which is his rule; his separate `fs_advisor_quad` program is not taken,
  since a new row would be one more program and ours rounds the alpha to a byte as `fs_object` does.
- Our object shadow programs (`ObjectShadowInstanced` and the other `*Shadow*` rows) and the `vs_static_shadow_*`
  files keep our names. His static-shadow bake names are never taken: the bake itself is parked, because his shadow
  projection is not ours.
- A new shader row goes only at the end of `k_Shaders`, and a renamed one is renamed in place. bgfx sorts the draws of
  a non-sequential view by program index, so moving or inserting a row would change the draw order.

#### Dormant renderer pieces of his, not wired yet

each is his text, compiled and tested but not called, so
nothing drawn changes.
- `Graphics/UploadPacer` and its test, as he wrote them: the pacer that holds a loading thread's buffer and texture
  uploads to what a frame has room for. Nothing calls it: his calls from the textures, the buffers and his
  background load queue wait for that queue (his `Resources/LoadQueue`), which is not ported.
- `Graphics/ViewFrustum` (`view_frustum`) and its test: the four sides of a camera's view from its view-projection
  matrix, and whether a sphere, or its reflection in the sea, is inside them. Nothing culls by it yet.
- `Graphics/MeshDetail` (`mesh_detail`) and its test: which of its meshes a model is drawn as by its depth into the
  view, its size and importance, with the fade-out and leaving out. The model detail setting is
  `detail_level::ModelDetail`, a table of its own (`k_ModelDetail`) beside our `Level` settings rather than a field
  of them, so that the levels' initialisers stay as they are. Nothing chooses meshes by it yet: the wiring and his
  far villager smudges change what is drawn and are measured on their own.
- `Graphics/SeaReflection.h` (`sea_reflection::ReflectsVillager`) and its test: the sea reflects a villager only
  while it moves in the physics, or while it is held in a hand that is itself reflected. Wired for what the hand
  holds, villager or not, in our `DrawObjectReflections` (the physics part is already ours, through the physics
  list): the hand's reflection drawn this frame is `handReflected`. His villager draw in the reflection is not taken.
- The moon's extras of his `Graphics/Moon` beside ours: `Offset`, `MonthFraction` with his `k_NewMoonDay`,
  `k_MonthsPerDay` and `k_SecondsPerDay`, `ScriptPercentage`, `DateAtFraction`, `Angles` and `SeaGlow`, with his
  tests merged into our `test_moon`. Ours stays where both trees do the job: `Place` keeps its own angle (rounded
  twice; his `Offset` rounds once, so `Place` does not go through it), `Phase` keeps rounding the months to a float,
  and `SeaGlow` is built on our `Basis` and `MakeGlow`. So his test that the phase is made from the full-precision
  fraction (many months on) is left out: ours misses it by about 1e-4, and which one the original is has not been
  read. His Moon debug window reads `Offset`, `Angles`, `MonthFraction` and `DateAtFraction`, and
  GET_MOON_PERCENTAGE pushes `ScriptPercentage` ([the moon as last shown](#the-moon-as-last-shown)); nothing calls
  `SeaGlow` yet: the sea glow is drawn and measured on its own.
- His static-shadow projection (`ObjectShadows`), taken as `Graphics/StaticShadowProjection` and removed with the
  live static shadow pass once the static shadows were baked as the original's prints step does (#222): nothing read
  it then, and the bake projects through the original's own sun point (`block_texture::ProjectShadow`).
- His creature spells' looks in our `fs_object`, behind `USE_CREATURE_SPELL_LOOK`, which only `fs_object_creature`
  defines, for the four `*SpellLook` programs, which went in at the end of `k_Shaders` so that the programs before
  them keep their handles:
  his three samplers (`s_iceEnvironment` at 10, `s_iceEnvironmentAlpha` at 5, `s_staticAlpha` at 15, all free in
  the object programs), his `u_creatureSpellLook`, the fizz's depth-only pass through the static's alpha, the
  fizzing body's alpha and the frozen sheen, facet by facet. How frozen comes from the uniform's w, since our
  fifth instance column is taken, and the sea's plane is ours (`SeaPlaneDiscard`). Every other object program
  compiles to what it did and binds no more textures. The fizzing creature's two-pass draw was turned on as its own
  behaviour commit (2026-10-15); the ice's two textures are not bound yet, so those stages take the white default.
- His `USE_ENVIRONMENT` variant on our temple pair: `vs_object_temple_environment_instanced` and
  `fs_object_temple_environment` set it over `vs_object_temple_instanced` and `fs_object_temple`, which keep our
  temple light: the environment map's coordinates from where the normal points across the view (his rule), and
  the map added on the second stage (`s_environment` at 5), saturating, after our temple light. The program
  `ObjectTempleEnvironmentInstanced` went in at the end of `k_Shaders`; nothing fetches or submits it until the cave's
  trophies are drawn with it, measured on their own. His non-instanced `ObjectEnvironment`, for the cave's seeds,
  and his environment-alone mode wait for the seeds, which are blocked.
- His temple depth fog and the temple's light (PR #349, aea838240 and 95e8d0d75), on our temple renderer. His
  `TempleFog` is not taken as a second type: its arithmetic joins our `temple_fade` (`Step`, `ShareOfStep`,
  `EndColour`, `AddedAt`, `Once`, `AtVertex`), our ranges stay (the creature room's 40 / 150 only once that room has
  been drawn, not the room the player is in) and so does our oracle-pinned float step. His `GetFog` on the temple's
  interface is not added: the renderer takes the fog from `TempleLight::fade` and the light's colour, so no fake
  changes. His `haze.sh` mode (`u_haze.w` = 2) and `u_fogAt` do not fit our temple, which has its own programs:
  the per-vertex fog is `temple_fog.sh` in `vs_object_temple_instanced`, and the once-a-submesh fog (first vertex,
  or a belt's place) is worked out on the CPU in whole bytes and handed to the same uniforms (`u_templeLight`,
  `u_glow`, with `u_templeFog.w` = 2). His `FrameLight(..., templeOrigin)` is not taken, since ours never runs inside
  the temple: `model_light::InsideTemple` under a `ScopedLight` for the whole temple pass, the value read in the exe.
- His advisors' pupils as a variant of our object program, since our advisors' bodies are drawn with
  `ObjectInstanced`: `vs_object_advisor_instanced` sets `USE_ADVISOR_PUPILS` over `vs_object_instanced`, and
  `vs_object` then takes his `u_advisorPupil0`, `u_advisorPupil1` and `u_advisorPupilCentre` and his rule moving
  the pupils' texture over the eyes' vertices. The program `ObjectAdvisorInstanced` went in at the end of
  `k_Shaders`. The advisors' bodies are drawn with it: his `Pupils()` (each eye's scales across and down, from the
  face) feeds `u_advisorPupil0` and `u_advisorPupil1` with the eyes' first bones, and the .hd's pupil centre feeds
  `u_advisorPupilCentre`, through the mesh draw's uniform table, on every primitive. In the renderer's variant
  tables its rigid and few-bone parts take `ObjectStaticInstanced` and `ObjectFewBonesInstanced`, as before.

#### The globes' draw

His `Renderer::DrawGlobes` is in his file, `Graphics/RendererMiracles.cpp`, but it reads a
frame list of draws (`graphics::globes::GlobeDraw`, `Graphics/Globes.h`) instead of his globe components, and the
list stays empty: our bubble (`OneOffSpellSeedArchetype`, `magic::one_off`), the seed inside it and the bands
(`worship::seed_graphic`) stay entities, drawn and queued with the models, with our facing, band maths, glint clock
and mode-12 material. In the world view only, right after the tribes' names, each draw of the list would be one Z
object of our single queue keyed at its sort point as the models are (`ZObject::globe`, which `IsModel` counts; the
drain passes over it until a feed exists). His Reflection draw, his bgfx-depth sort of a translucent view, his
bubble moved towards the camera, his phials' pulse and environment map and his alignment-0 flock seed are not
taken. Of his `Magic/MiracleVisuals` only the globe's glint and the rings are in our tree (`StepGlint`,
`GlintUvOffset`, `RingCount`, `RingAlpha`, `RingTurn`), unread by the game and pinned against ours by
`test_miracle_visuals`: the glint cell is `frame_anim::OneOffFrame`'s, the rings are the band levels
(`seed_graphic::BandLevels`, pu + 1), as faint (`BandAlpha`, (60 x alpha) >> 8) and turned the same way
(`RingTurn` is `BandRotation` transposed). His `k_GlobeAlpha` was 150, the orb's tint byte; the original multiplies
it with white first, so the globe is drawn at 149 and its rings at 34, as ours are. `k_GlobeAlpha` is now that drawn
149 (rings 34), so a draw that takes it as the globe's alpha draws the original's. His bubble moved towards the camera
by its radius is not taken: the original moves only the sort point and restores it (the one-off spell seed's `Draw`,
[seeds and one-off miracles](../magic/spell-core.md#seeds-and-one-off-miracles)), as ours does. Feeding the list
means taking the entities' meshes off in the same, measured commit, or every globe is drawn twice.

#### The exploded pieces' draw

His `Renderer::DrawParticleFragment` is the draw of a model's exploded pieces (our
`particles::Creator::Kind::MeshPiece` atoms, his fragments), in `Graphics/RendererParticles.cpp`, but with our data and
signature: it takes our `world_triangles::Frame` and the atom tag, has no depth argument (the original gives the
pieces no Z object), and its body is the `world_triangles::Submit` both call sites made before. The pieces stay
built on the CPU (`mesh_pieces`), lit with the integer land light and drawn at once with each primitive's material,
in the world view only. His fragment shader, translucent-pass submit, sorting, two-sided culling and blend state are
not taken, since each of them would change the pixels against the wiki; the drain's `Fragment` case stays empty.

#### The hand's glow and bands

His `Renderer::DrawHandGlow` is in `Graphics/RendererMiracles.cpp`, but it takes the
pass to draw into and a `std::optional<graphics::HandGlowDraw>` (`Graphics/HandGlow.h`: alpha, the flowing
texture's cell, the player's colour) instead of reading his `HandMiracleFx` component; the pure
`graphics::HandGlowOf(hand_fx::Glow, playerRgb)` makes that draw from ours (none at alpha 0), pinned against the
wiki by `test_hand_glow`. It is called inside the hand's own Z object, right after the shadows over it, in the world
view only, and is fed `std::nullopt`, so it returns before any bgfx call; his separate queue entry one step after
the hand, his texture read outside the resource caches (full 8 bits, where ours cuts the raw textures to ARGB4444),
his new submit fields (texture override, UV scale and offset, tint) and his gesture flash added to the hand's light
are not taken. Feeding it from `hand_fx::GetGlow` needs a program with the flowing texture and its alpha sheet, and
is a measured commit of its own. His `DrawHandMiracleBands` has no counterpart: our bands are registry entities
(`magic::hand_fx` `MakeBand` / `DrawBand`, the `Power_Up_Band` mesh with `Alpha`, `ObjectColour` and `HandFxPart`),
drawn by the rendering system's rows through the normal model draw and our single Z queue, so his pose maths
(`BandOnHand`, `BandAtCamera`, `BandBetween`, `BandBlended`) maps onto the transform `DrawBand` writes every frame.
Ours stays, as the wiki has it: the temporary bands slerp where his keep the camera's axes, ours has the charge
bands, which his lacks, and the alpha stays a float where his truncates it to a byte. Adding his draw would draw
every band twice.

#### The light sheet's draw

His `Frame::lightSheets` and `draw::AddLightSheet(Frame&, span<const LightSheet::Vertex>,
span<const uint32_t>, sortPoint)` keep their names, but the frame's sheets are our `particles::surf_revol::Surface` (the
stars texture and its alpha, additive mode 13, no depth write, two-sided, `specularInPass`), filled by
`draw::AddLightSheets` from `ParticleSystemInterface::LightSheets()` once a frame in `CollectFrameParticles`, the
newest first as the game walks its list, each keyed at its middle read before its build. Each sheet is one entry in
our single Z queue, after the Queued effects, drained by the `Surface` case through `DrawParticleSurface`, in the
main view only (his draws it in the reflection too; the game does not). His `ParticleSurface` program (vs / fs
`particle_surface`) is taken for the sheet's one pass, the specular added after the texture and clamped; his
material table, `draw::Order` and the shared transient buffers are not, and our effects' surfaces keep their two
passes.

#### The render passes

the passes both trees have already use his names; `MainBlended` stays (not his `Translucent`).

### Sky and weather

#### Snow cover

his `snow_cover` maths, `SnowSystemInterface` and `SnowSystem` (slot `snowSystem`), his `snow.sh`
and `snow_object.sh`, and his `LandUnderSnow` and object-snow hunks fitted to our `fs_terrain`, `vs_object` and
`fs_object`. Until his storm list (A18) is in our tree the snow is fed from our storms: each storm lays its snow in
its own update (`Storms.cpp`) and the atmosphere melts it after them (`Atmos.cpp`), in the original's order, so the
interface takes `AddStorm` and `Melt` in place of his `ProcessTurn(storms)`. Two of his numbers were changed, each
pinned by `test_snow_cover`: the weather grid's byte is half the depth, towards zero, and a storm's snow is multiplied
by its `snowCoverRate`. With no snow the shaders get `u_snow = 0`, which leaves every colour as it was, and the depth
texture is only uploaded when the snow changes.

#### Snowfall

his `snowfall` maths and `SnowfallSystem` (slot `snowfallSystem`), drawn by `RendererSnowfall.cpp`
as one Z object per tile, like the rain. Two things follow the original: each land block near the camera is read as
four 80 by 80 tiles, each at its corners (`snowfall::BlockTiles`, since 2026-10-22; before, one tile a block, #193),
and the flakes are scattered twice at the start-up, the process's first C runtime draws (`weather::StartAtmos`),
where his scatters once, as the service is made; drawing a tile scatters nothing
([the drawn snow in openblack](../world/weather.md#the-drawn-snow-1)).

#### Sky dome tint

his `3D/SkyDome` maths (`AlignmentPair`, `Darkness`, `TintOf`, `ThroughOvercast`) and his
`fs_sky` uniforms on our dome build, fed with the alignment the original's way (0 good to 2 evil, so the darkening
is on the good side), `Darkness` in floats and `TintOf` without the clamp below 0, as the original.

#### Sky clouds

his mist draw per cloud, but in the original's order inside our per-cloud loop (`Clouds::Layout`:
x, y, z, the mist's start, size, k), not his layout-then-`CloudArchetype::Create` split; a behaviour change of its
own.

#### Sky service

the one sky service is his `skySystem` slot with his `ecs::systems::SkySystemInterface`
(`ECS/Systems/Implementations/SkySystem`), held by our sky: the dome mesh and its three blended textures, the sun,
moon and cloud meshes, and `SetTime`/`GetTime` (the script hour the sun and moon are placed at). His `GetClock`
forwards to `dayNightClock`. His `DayNightTimes`, `GetDayNightTimes` and `IsRunning` are on our clock, and the sky's
`GetDayNightTimes` forwards to it. His `ProcessTurn` moves our clock on, called where the game's turn moved it.
His `UpdateFrame(skyDrawn)` folds what the scene draw did one after the other: the frame's sky type sampled
(`skyFrameSystem`), then the dome moved on and its rows blended. The scene draw passes `true`, because our dome moves
on with every scene drawn; his, which moves it only while the sky is drawn and calls it from the game's frame, is a
behaviour change of its own (the sky switch is a debug control), and not taken: the original moves the dome on from
its sky draw inside the scene draw, which a full-screen film skips, while his call from the game's frame runs under
the film too. The dome, sun and moon are entities, with his `Initialize`, `KeepForNextLand`, `GetDome`, `GetSun`,
`GetMoon`, his `SkyArchetype` and his `ecs::components` `SkyDome`, `CelestialBody`, `CelestialGlow`, `Sun` and `Moon`.
They are made where and when his tree makes them, with plain `Create()`: when the sky service is made, in the
empty registry, and again right after each land's reset (`test_sky_archetype`). So every later entity's identifier
is three more than before, which the state hash reads; nothing else changes. What differs from his: `SkyDome`
holds only the dome's mesh id (the blended pictures stay the sky service's texture, not a cached one), the mesh ids are ours (`sky/sky.l3d`, `sky/sun.l3d`, `sky/moon.l3d`),
`Sun` is empty and `Moon` has only the phase and its date read (his placement, colour and strength come with his
frame update, which ours does in the draw), and his `DayNightCycle` is not taken: the clock stays the
`dayNightClock` service's, so `KeepForNextLand` keeps only the moon. The sun's and moon's draws take their textures
from `CelestialBody` and `CelestialGlow`. `GetCurrentSkyType` is not taken (his runs 0 at night to 2 by
day, ours the original's way in `skyFrameSystem`). His `SetTime` also sets the clock and jumps the dome, and his
`GetTime` is the visual hour; ours keep their behaviour behind the names, and the callers set the clock themselves.
His land light inputs kept on the dome (worked out once in `UpdateFrame`, read by the light table, the sky tint and
the moon's colour) are not needed: ours are read once a frame right after the sky type is sampled, as the original's
sky draw does, and the sky tint and the moon's colour read the same inputs from the table built with them.
His test that the night is lit by the land's rows and not by the moon's colour is taken onto our table and its
sky type, which runs 0 by day to 2 at night (`test_land_light`).

#### The moon as last shown

his `ecs::components::Moon` (in `ECS/Components/Sky.h`) with his `phase`, on the moon's
entity (`SkySystemInterface::GetMoon`), kept from land to land by `KeepForNextLand`. Our `DrawMoon` writes it once
the moon shows, as the original, and draws with it. His `GET_MOON_PERCENTAGE` is taken: `moon::ScriptPercentage` of
the moon entity's phase, 1 before the moon first shows.

#### The moon in the sea

his order is taken (the mirrored moon, then its glow, which is the original's), in our
`DrawMoon`; the glow stays ours (`moon::Basis` on the mirrored view at the moon's own centre), not his `SeaGlow`,
until a night shot shows the two byte for byte the same. The moon's Z write: his WRITE_Z is right and ours (no Z
write, inferred) was not; the moon writes Z in the sky and the sea, with the depth test less or equal including the
equal depth (`ZFunc::LessEqualInclusive`), as the original's mode 5 under its LESSEQUAL.

#### The moon's date read

his two-second date re-read (`k_MoonDateReadInterval`, `Moon::date` and
`Moon::dateReadAt`) as he wrote it, as `sky_frame::TakeMoonPhase` in `3D/SkyFrame` (his file and namespace), with
only the moon's part of his `sky_frame::Update`; the date is still our `spooky::UnixTime` and the ticks
`game_clock::TickCount`. His `dateOverride` and `MoonDate` came with his Moon debug window: unset unless the window
sets it.

#### Mist, clouds and village lights

his slots and interfaces, with our behaviour behind them, called from the
renderer's PreDraw at the same moment of the frame as before:
- `mistSystem` (`MistSystemInterface`, `MistSystem`): only a mist on screen moves its animation on, where his moves
  every mist.
- `cloudSystem` (`CloudSystemInterface`, `CloudSystem`): owns our `Clouds` (`GetLayout()`), with no entities and
  no per-cloud random draw; the renderer still lays them out when a land opens, and moves their animation only
  while they are on screen.
- `villageLightSystem` (`VillageLightSystemInterface`): his behavioural `Update(gameTime)` and `IsDark()` over our
  `night_lights` module, which reads the frame's table through `land_light::CurrentTable()`. The lights stay ours,
  with no entities, and keep their random draws at the first rescan rather than his at the lantern's creation.

#### Weather

`weatherSystem` takes his `WeatherSystemInterface` names in front of our weather code, and
`components::WeatherInfo` is his (the same eight bytes as ours, so `weather::WeatherInfo` is now a name for it). The
climate script commands, the climate switches, `KillStormsInArea`, `GetWeather`, `GetWeatherSmooth`, `GetOvercast`,
`GetLightningFlash`, `GetSeason` and `Reset` call our functions over `weather::State`, unchanged: our weather queries
keep the climates' temperature and wind, and our storms keep their draws. `GetDaysFromStart` stays a double. Left
out until our weather can do them without new behaviour: `Update` (ours runs in three slots of the turn),
`GetActiveStorms` and his `Climate` and `Storm` components (our storms are a list with ids, not entities),
`ForceStorm`, `EndStorm`, `ClearStorms`, `StrikeLightning` and the miracle storm methods.

### Physics

#### The Bullet ray caster, under his picking slot

his `dynamicsSystem` slot belongs to his `DynamicsSystemInterface`,
the physics turn manager, so our Bullet system, which only answers ray casts against the land and feature bodies,
left it unchanged for his picking slot: `Locator::pickingSystem`, `PickingSystemInterface` and
`Implementations/PickingSystem`, emplaced and reset where the old slot was. It first held only our Bullet ray casts;
his picking methods (land along a line, the land under a pixel, the pick under the cursor, feeling a model) came into
the same interface with the picking port, and Bullet's casts left it with Bullet (N-C-2, below). The
`dynamicsSystem` slot now holds our turn manager (below).

#### Bullet removed, his 834f657f under our names

the picking slot's Bullet world, its rigid bodies and ray casts
(`AddRigidBody`, `RemoveRigidBody`, `RegisterRigidBodies`, `RegisterIslandRigidBodies`, `RayCastClosestHit`, the
ray cache, `RigidBodyType` / `RigidBodyDetails`), the land blocks' collision shapes (`LandBlock::MakeShape`,
`SharedLandMesh::tree`, `Dynamics/LandBlockBulletMeshInterface.h`), the models' convex hulls
(`L3DMesh::GetPhysicsMesh`), the features' bodies and `ECS/Components/RigidBody.h` go, with the island's rebuild
re-adding its blocks, the CMake and vcpkg lines. The same parts as his, each at our name for it; the `isPhysics`
flag of the sub-meshes stays (the drawing skips them). Before it, the camera's and the hand's land moved onto
`PickingSystemInterface::LandFrom2DPoint`, the original's island ray from a pixel, which his tree does not have
(his camera takes `LandUnderPixel`). The object under the cursor stays our hand's own pick until `PickUnderCursor`
is served.

#### The land line's callers, on his picking slot

the camera's pan stop (`StopPanShortOfLand`, which now takes the
picking interface), the sun's glare and the point behind the hand ask `pickingSystem::LandOrSeaAlong`, as his tree
does, instead of the island's `RayCast`. Each reads only the point's x and z, and only when there is one, so the
value `RayCast` writes for a sea crossing too far away is never read. The lightning fork's cut against the land
still calls `RayCast`. The camera tests' picking fakes answer `LandOrSeaAlong` with the same cell walk over the
island in the locator, so their numbers are unchanged.

#### Feeling a model, our mesh test behind his method

`PickingSystem::FeelModel` is served by the hand's own test
of the mesh (`ecs::hand_mesh_ray`), with the mesh and the matrix the hand's pick finds the object at
(`ecs::hand_mesh_model`, in `HandPlacement.cpp`: the ground under an object that follows the land and a pile's sink,
a one-shot orb's facing, a temple entrance's own mesh). It fills his `screen_pick::MeshHit`'s point, normal and
distance; the triangle and where on it stay 0, as our test does not work them out. The hand's pick asks
`FeelModel` for the surface it rests on, and only reads its point and normal, so the values are the same.

#### The physics body, our engine under his names

our rigid body is his `physics::Body` in `src/Physics/Body.*`,
with his names for the step (`ClearForces`, `TouchGround`, `TouchFaces`, `ApplyContacts`, `Integrate` returning his
`StepResult`), for settling (`SettleOnLand`) and for the overlap test (`OverlapDepth`), his namespace-level
constants and his point fields (`depth`, `firstDepth`, `aheadLength`). The land comes in through his
`physics::Ground`; `ecs::PhysicsGround` answers it from the island the same way the body read it before (no island:
flat at 0, facing up, no sea), and `ecs::GameGround()` gives the game's. The hand-made shapes are his
`shapes::Tree`, `shapes::LivingBody`, `shapes::Fragment` and `shapes::FromModel` in `src/Physics/BodyShapes.*`. The
arithmetic in all of them is still ours, token for token, where his differs (the inverse inertia, the drag, the soak,
the damping, the pose orders): each difference becomes his, the original's or stays ours in its own measured commit.
Our set-up calls (`Initialise`, `SetUpConstants`, `Build`, `BuildShape`, `SetUpPos`) stay until then in place of his
constructor, and the materials stay our `PhysicsData` table until the rows are shown to load the same through his
`MaterialTable`. The turn manager is still ours, in `ecs::physics`, so code there names his with
`openblack::physics::`, and code outside it that meant ours writes `ecs::physics::`.

#### The class hooks, his design over our tables

what each kind of object does in the physics is his
`PhysicsClassHooks` virtual class (declared with `DynamicsSystemInterface`, defaults in `ECS/PhysicsClassHooks.cpp`),
set once with his `SetClassHooks` where the game starts; each default is what an ordinary object does. The game's
kinds are his `ecs::PhysicsGameHooks`, whose overrides pick the class (`ClassOf`) and call the same code our per-class
tables held, so every class runs the code it ran before, at the same points and in the same order. The hooks are
ours where his differ (our `StartPhysics`, the `std::optional` answers of a kind that leaves the rest to the physics,
`CallsBackInMap`, `SetUpBody`, `WeightOf`, the starts without a body and when knocked, the hand's before and after);
his hooks that we do not have are not declared until they are ported. The trees' and pots' parts are still the hand
system's, called through it. The villagers' and animals' parts are his files with our names and our code: the pure
landing rules in `Physics/LivingRules` (`physics::living`), the rest in `Implementations/VillagerPhysics`
(`ecs::villager_physics`), the animals' landing included, which his tree keeps in `PhysicsGameHooks`. Our rocks are
his `ECS/ObjectPhysics` (`ecs::object_physics`: his `IsRock`, `CanTapRock`, `TapRock`, `SplitRock` and `KnockRock`
with our signatures, and our `RockRadius2D`, `RockHeight`, `RockValidForPlaceInHand`), with the tap's height rule in
his `Physics/ObjectRules` (`RockBreaksWhenTapped`). His other object rules come with the commits that take them.

#### The physics' World seams, his design over our locator

the dynamics, the broken buildings and the class hooks
read the game through his seams, `ecs::dynamics::World` (`ECS/DynamicsWorld.h`), `ecs::building_world::World`
(`ECS/BuildingDamageWorld.h`) and `ecs::physics_hooks::World` (`ECS/PhysicsHooksWorld.h`), each with a game version
over the locator that makes exactly the calls the systems made before, in the same order. His method names where
the job is the same (`Entities`, `Land`, `Info`, `CameraOrigin`, `Dynamics`); ours only next to them (`FileSystem`,
`Resources`, `BuildingDamage`, `HandPart`). Each system takes its world in a constructor, the game's by default, so a
test builds its own system over `test::FakeDynamicsWorld` (`test/support/DynamicsFakes.h`) instead of reading the
locator's. The system's helpers are its own private members, working on itself rather than on the locator's copy.
Two tests still reach the locator's physics through the code they call (the tornado's carried objects, a creature's
throw); they move when those systems get seams of their own. His system tests on fakes are not taken yet: they are
written against his entry and hooks API (his `InitialisePhysics(PhysicsStart)`, `InPhysics`, the hooks' impact
feedback), which comes with the behaviour commits.

### Particles

#### His LightningMaths names over our formulas

The bolt's maths are in his file,
`Particles/LightningMaths.{h,cpp}` (`openblack::particles::maths`), with his names and signatures
(`StrikeSearchCells`, `CloudSearchCells`, `ForkCount`, `BoltPose`, `ClashPoint`, `k_ClashShare`,
`k_ClashWidestAngle`, `k_ClashForkScale`, `k_ClashStrength`), and his test file, `test/test_lightning_maths.cpp`.
Every formula is ours, which is the original's, moved out of `Particles/Rules/Lightning.cpp` unchanged. His
bodies differ from the original in four places, which stay ours:
- `ClashPoint` tests the angle against the way to the meeting point without first making it unit length (the
  original normalises it in place, so his accepts clashes the original rejects once the meeting point is more than a
  unit away);
- `ForkCount` is 2 × min(at once, targets) + 2 where the original builds 2 × targets forks. The signature keeps his
  `atOnce`, which ours does not read;
- a search radius of 0 or less gives one cell where the original searches none (ours works the count out in the
  original's 32-bit integer, so a radius of -10 or less searches the square of its negative side);
- he takes the cosine of a float where the original takes it of a double: `k_ClashWidestAngle` is a `double` in
  ours, the one type that differs from his.

Not taken: his `PreferCreatures` and `k_DrawsBoltBelowFizz` (a creature in the cone takes every fork). The original's
rule was read in the exe and is ours, in a behaviour commit of its own (2026-10-19, part of #40): when a bolt from the
hand finds among its targets a creature less than half fizzed out of sight, every target, the ground points included,
is given one of those creatures in turn (`CreatureRetargets`, in his file)
([lightning](../magic/fireball-and-lightning.md#lightning)).

(Port row R30 B2.)

#### His UR_ObjectArcer's file and names, written on our engine from the original

The rule is in his
`Particles/ParticleArcRules.cpp` (`RegisterArcRules`), its maths in his `LightningMaths` (`ArcEnds`, `ArcTangent`,
`ArcJoints`, with his signatures; ours adds `ArcLength`, `ArcSurfacePoint`, `ArcSurfaceNormal`). The queue and the
live count are `particles::manager::State::arcQueue` / `arcAtoms` behind `object_arcer::Queue` (`ParticleArcRules.h`),
where his has `world.TakeArcs` / `HoldArc` / `SurfacePoints`. His code is not copied where it differs from the
original:
- the defaults: his 2, 8, 0.1, 0.2 and 3.0 with `max(1, ...)` and `max(2, ...)`; the original's are 10, 19, 0.1, 0.2
  and 1.0, not clamped;
- his atom takes its first point as its position; the original leaves it where it is made;
- the queue: the original queues an object whose model is not animated (its mesh has no bones) while fewer than
  100 arcs live, and makes the atom (its random byte drawn) before it knows it has points; his skips an object with
  no points without an atom;
- the points: the original picks one sub-mesh, one primitive, then five of its vertices, and turns each normal
  through the whole matrix with the translation, its sums in their own order; his `SurfacePoints` has neither;
- an atom with no arc data: the original deletes it, his keeps it; an atom with no ribbon's collection: the
  original stops the step and detaches, his lays nothing and goes on;
- the ribbon: the original makes no joint without the chain's creator and clears the chain's interpolation bit
  when it makes them; his makes them with no creator and keeps the bit;
- `ArcTangent`: the original scales by (length × scale) once, his by the length and then the scale;
- `ArcJoints`: the original's t is k × (1 / (n − 1)), its curve A t³ + B t², + C t, + D with C and D summed from
  products by 0 and 1, and its length's squares summed z, y, x; his t is k / (n − 1), his curve Horner's, C and D
  the tangent and the start themselves, his length `glm::distance`.

(Port row R30 B2.)

#### A particle's per-frame read of what it draws

where the original does something in a particle's draw that the
game needs (the camera particle's path time and its stop), the particle side has a small per-frame pass over those
particles, called by Game.cpp with the frame updaters after the camera's update, as `town_belief::Step` is
(`particles::forest_camera::UpdateFrame`). It reaches other systems only through their interfaces
(`CameraPathSystemInterface::FollowAt` and `Release`), so they need no query into the particles. Its state is the
rule file's module of the particle system (`ParticleSystemInterface::Module`), not `ParticleManagerState.h`.

#### Explosions

his `explosionSystem` slot and `ExplosionSystemInterface` front our ground marks and camera
shakes, and keep nothing of their own. `AddRubble` is our explosion mark (its turn still drawn by the blast from the
particles' random numbers, before the call), `Update` and `Reset` are the marks' frame step and clear, which take
the uprooted trees' craters along as before, and `AddShake` and `IsShaking` are our camera shakes, which still
count down in the camera's real frame time. `AddShake` takes his fifth argument, `verticalOnly`, and is the particle
sound rule's shake: its seconds are cut to whole milliseconds as the original's rule does (his keeps them as a float),
where the script's SHAKE_CAMERA rounds. The marks stay entities kept in our list, newest first: his
`GroundMark` and `DestructionGhost` components are not taken, since a storage walk would destroy them in another
order, and our object ghosts are not his destroyed building's ghost. His dust puffs (`CollectDrawFrame`) are left
out: our mark's dust is the smoke an object leaves when it goes.

#### The blast's extras (rubble, dust, ghost)

each stays ours where ours already does the original's job, and only
the missing call is added. The rubble's turn is drawn from the effect's own particle numbers, as the original
(his draws it from the game's local stream inside `AddRubbleMark`). The dust is our disappear smoke's brown puff on
the C runtime's numbers, as the original (his `dust_puff` draws the local stream and skips the puffs' turn). The
ghost of a building an effect destroys goes through our `ecs::object_ghosts`, called first in
`abodes::DestroyedByEffect` as the original does; his `DestructionGhost` component and its renderer pass are not
taken. His `ParticleObjectEffects.h` (the fireball and blast object interface) is not ported: it is reached through
his engine's per-effect services, which our engine does not have, and our blast and fireball rules already reach
the same world functions, with the original's order and streams, directly.

#### His behavioural interface in front of our engine

Where the engine's core stays ours (the particle engine,
`particles::manager`), the slot's interface takes all of his methods, also those with no caller in our tree yet, and the
game reaches the engine only through them. `ParticleSystemInterface` has his `Start` (by a file's name and by a
particle type), `StartForSpell`, `ProcessForSpell`, `ProcessByFrame`, `StartSpotVisual`, `SetOrigin`, `SetPlayer`,
`SetDrawPath`, `SetDrawOffset`, `FindShield`, `GetSoundCount`, `AddTarget`, `AddTargetPosition`, `AddBeliefSprite`,
`AddGestureTrail`, `UpdateFrame`, `CloseDown`, `Delete`, `IsRunning`, `Find`, `ProcessTurn`, `Reset`, and the debug
window's `GetEffects` (his `EffectInfo`, with his `DrawStats` for the draw counts), `GetFileNames`, `SetPaused` and
`IsPaused`. `ParticleSystem` forwards each one to the engine, which keeps its own free functions for its rules and
its tests; as in his tree, its bodies are in `Implementations/ParticleSystem.cpp`. A test's fake of the particles
derives from `test::InertParticleSystem` (`test/support/ParticleFakes.h`), which does nothing for every method, and
overrides only what it records. Where his signature would change what our callers mean, ours stays under his name:
`StartForSpell` takes an optional sink and says whether it is synced at every call (his default is synced, ours was
not), and `StartSpotVisual` returns the container object scripts hold (his an effect id). A seconds duration
becomes turns with `particles::manager::SpotVisualTurns`. The engine-only calls (`SetPerFrame`, `GetDrawPath`, `IdOf`,
`RunDebugHooks`, the draw collects) stay free functions until a method of his covers them; the draw goes with the
renderer merge. The decisions taken with his names:
- Where his type is not ported, his name stands for ours. `FindShield` returns our `particles::shields::DefensiveSphere`
  as `particles::ShieldSphere` (his returns a shared pointer): a pointer into the engine's list of spheres, valid
  until a sphere is added or removed, so it is never kept across a step. `particles::GestureTrail` is our
  `magic::gestures::RecognisedGesture` and `particles::BeliefSprite` our `ecs::town_belief::BeliefSprite`. The
  aliases sit beside forward declarations (in the interface and in `Particles/GestureTrail.h`), so that the
  interface includes neither type's header.
- `AddGestureTrail` takes the trail's contents onto the list the recognised-gesture rule reads: moved out when the
  caller hands over its only reference, else copied, so that other holders still see it whole. `AddBeliefSprite`
  queues the sprite on the towns' belief-sprite queue.
- The draw offset is his: `SetDrawOffset` keeps an offset on the running effect, and the collects add it, times each
  atom's `drawWeight` (his atom field, 1 by default), to the effect's sprites, meshes, mists, other atoms and chain
  joints. Not moved: the mesh pieces and surface-of-revolution atoms (drawn from the atom itself), a queued effect's
  sort key and the town belief. His lightning rules' writes of the weight are not ported, so every weight is 1. The
  original's per-joint weight for the lightning is known and is ours already, as the atom's own offset that follows
  the hand ([The spell core](../magic/spell-core.md), Lightning); the per-effect offset and its weight are taken as he
  wrote them, their original behaviour not yet read.
- The pause is his: while it is set, `ProcessTurn` does nothing, and `ProcessForSpell` and `ProcessByFrame` step
  nothing but give true for an effect still running. A new land leaves it.
- `GetSoundCount` is the number of particle sounds playing or dying away, 0 without the audio service.
- `GetFileNames` takes the names from the resource loaders: the names the spell file loader loads by
  (`resources::ParticleFileNames`, beside `ParticleFileLoader`, with its folder and suffix), sorted, each once; none, and no
  log, without the folder, as his.

#### A rule that asks its parent atom for a point

His object rules find the parent's object by scanning the
parent's data with a `dynamic_cast`. Ours keep that lookup but through a small interface,
`particles::SurfacePointSource` (`Particles/ParticleObjectRules.h`): a rule whose atoms stand for something answers
for them (the object rule and the creature-spell rule), and an emitter asks the first such rule of its
parent, else takes the parent's place, as the game's render particles each answer `GetRandomSurfacePos`. His
world-services queries (object position, random surface point) are not taken: the pure pick walks a
`object_surface::Model` given by the game side, so its tests use a fake model.

#### The particle draw list

ours under his names in `Particles/ParticleDrawFrame.h`, at his path; our Z queue and draw order stay.

#### The glowing shell's draw

His `UR_VolFXOnObject` (raffclar PR #294) comes onto our engine: the atom keeps an `std::optional<particles::volume_glow::ObjectGlow>` (`particles::Atom::objectGlow`, copied into `Effect::DrawAtom::objectGlow` by `CollectOrdered` only), and the manager's `SortedFrame::shells` takes a Sorted effect's glowing atoms. `particles::draw::AddShells` (in `ParticleSystem::CollectDrawFrame`, after the sheets of light) makes one `ShellDraw` per glowing atom of every path, its texture slid by the game's milliseconds at the frame's share of the turn. A Sorted shell is one more Z item (`ItemKind::Shell`) at its object's place; a Queued or Immediate shell is drawn inside its effect by `drawOrderedEffect`, found by its atom. `Renderer::DrawParticleShell` (`RendererParticles.cpp`) reads the object's drawn model and matrix from `RenderContext::entityInstances` (its finer model where one stands in), builds each part's band once from the part's collision positions, normals and indices (`VolumeGlowShell`, cached in `_glowShells`), and draws it with his `vs_volume_glow` and `fs_particle` (the `VolumeGlow` program, made last) in mode 13, no depth write, CCW culling, in the main view only (no reflection, as the sheets of light). His `ParticleEffect` / `DrawWalk` / `Sources::objectPosition` are not taken; his `ParticleWorldInterface::ObjectPosition` is our `glintTargets` slot. His boned parts drew no shell because his surfaces keep no corners for them; ours keep bind-pose corners, so they are skipped by their flag. The debug bar's switch `particles::draw::ShellDebug` (Artefacts window) leaves the shells uncollected.

#### The ribbon corners stay on the CPU

The chain ribbons (the lightning bolt and the storm's forks, the gesture
trail, the beams) keep their corners built on the CPU, as the original builds them, in `Graphics/RendererChain.cpp`;
his `vs_particle_chain` shader, which places the corners on the GPU from the joints (`draw::AppendChain`,
`ChainVertex`), is not taken. The formula is the same in exact arithmetic, but a GPU cannot give the same floats:
its square root, reciprocal square root and divide may be 1 to 2.5 ulp from the correctly rounded result, and fxc
may turn `x / y` into `x * rcp(y)`; shader compilers fuse the cross product, the dot and the joint plus side into
multiply-adds and reorder them; D3D11 flushes float denormals, so the tiny side of a still hand's trail becomes 0;
and his eye is the translation of the inverted view matrix, not the camera's origin. A few ulp of a world corner can
move an edge pixel's coverage or the depth by an ulp, on any backend. His version is also less faithful: his V
scroll is wrapped and applied once, where the original applies it twice, in the segment's UVs and again in the
draw's texture offset ([Frame-animated textures](../rendering/textures.md#frame-animated-textures), Chains),
as ours does. His CPU twin `draw::ChainCorner` is tested against the game only to 1e-3. No current check looks at
ribbon pixels (the land view shots show none, and the hand demos compare state); a testbed scenario with a ribbon on
screen is to close that gap before any change to the ribbons' draw is measured.

#### His particle rules on our engine

A rule class only he has is ported onto `particles::Effect` (a `particles::Modifier`,
registered by a `Register*Rules()` call in `ParticleRegistry.cpp`), in his file (`Particles/Particle<Topic>Rules.cpp`),
with its pure maths in his file and namespace (`Particles/<Topic>Maths`, `particles::maths`). Where his maths leans
on a helper our tree already has for the same algorithm, ours is used instead of a second copy, once ours is shown
bit-exact against the original (the simple beam's curve is our `key_points` spline, made bit-exact first and given
the three-axis reader, not his `KeyPointSpline`). His world-services layer is not taken: the rule reads
the land, the registry and the noise as our other rules do, and its tests inject fakes through the Locator.

#### The glints' targets, his names on a slot of their own

`ER_GlintsOnTarget` (`Particles/ParticleGlintRules.cpp`,
its maths in `Particles/GlintMaths`) reads its objects through the four methods his rule calls on his
`ParticleWorldInterface`: `ObjectPosition` (whose `has_value()` is the availability test), `TargetPointCount`,
`TargetPoint` and `TargetScale`. Our engine has no per-effect services to put them on, so they are a Locator slot,
`glintTargets` (`ECS/Systems/GlintTargetsInterface.h`), implemented in his file name,
`ECS/Systems/Implementations/ParticleWorldGlints.cpp` (`GlintTargets`), so that a later port of his world services
can take them in as they are. His implementation serves his globes; ours serves the objects the original gives
glints, the script highlight and the spell seed graphic. The implementation takes the meshes' parts as a function
(the resource cache's by default), so its test runs on fake parts with no renderer.

### World

#### Unlit trees

his `components::Unlit` (the name and the empty tag), assigned where the original turns the model
light off: on our Tree archetype as well as on the DeadTree and BigForest ones (his has it on BigForest, DeadTree
and Flowers; the flowers, in our tree since CREATE_FLOWERS came in, take it as his do: [Flowers](#flowers)). Ours
stays for the colour: `vs_object`'s tree path (the cell's light, then the haze, then the brightness), not his
`vs_vegetation` order (the brightness before the haze); the flag rides per instance in the w flags of the colour column
(`argb_colour::PackInstanceUnlit`) rather than per mesh in a uniform, so a mesh drawn both lit and unlit keeps
each instance's own light. A behaviour change of its own (trees.md, "Drawing").

#### The forest's camera creator

his `AnimWithCameraCreator` (`ParticleForestRules.cpp`) gives the shape, an anim
creator that only drives the camera, and the class name it registers, `ParticleAnimWithCameraCreator`, in our
`Particles/Rules/Forest.cpp` beside the forest's other classes. It is built on our `ParticleAnimCreator` (its own
registered factory reads the object, with no mesh), and the path comes from the camera path cache. Not taken: his
reading of the `.cam` and the `.anm` from disk, his start in `InitAtom`, his clock and his end. Ours runs the
original's particle: its step from `Creator::AfterAtomStep`, its draw once a frame, its removal from
`Creator::AtomRemoved`, the rules as pure functions (`particles::forest_camera`). His Game.cpp wiring is taken in our
frame's order: `HandlePlayerControl` before the camera keys, which are skipped while a path holds the camera, the
path's `Update` in place of the player camera's update (his runs it every frame after the input), and the close near
plane while a path holds the camera.

#### World effects facades

his slots and interfaces, our behaviour behind them.
- `waterRingSystem` (`WaterRingSystemInterface`, `WaterRingSystem`, `3D/WaterRings`) is his text, plus the ring's
  `seaLight`: a ring that takes the land's light gets it when it is added, as before. His drawing helpers
  (`HalfWidth`, `Alpha`, `Corners`, `CellUvs`) and `HandSplash` are left out: our ring draw rounds its constants
  differently, so it stays in the renderer. The pool is never cleared when a land opens, as before. The physics
  files keep `ECS/WaterRings.h` (`ecs::WaterRing`, `ecs::AddWaterRing`), a thin forward to the slot, until they
  move: another change holds them.
- `chimneySmokeSystem` (`ChimneySmokeSystemInterface`, `ChimneySmokeSystem`, `3D/ChimneySmoke`,
  `components::ChimneySmoke`) is his text with our deltas: the puffs start stacked over the origin, not over the
  chimney; the random numbers are the C runtime's (`game_random::crt`); a smoke moves on only in the frames its home
  is drawn, from the renderer, so his `Update` and `ProcessTurn` are left out. In their place the interface has our
  `UpdateHandWind` (once a frame, catching up on the turns it missed, at most 10) and `Drift`.
- `rainSystem` (`RainSystemInterface`, `RainSystem`, `3D/Rain`) holds the streaks that were in the weather's
  state. His `Place`, `Step`, `Follow` and `Ends` are his text. The tiles stay ours (two samples of rain or snow,
  float fades), so his `TileOf` and `StreakAlpha` are left out and our `PhaseFade` is added; the nearest storm
  is read from our storms, and the draws are the C runtime's. The streaks are placed twice at the start-up, after the
  snow's two scatters, and not again when a land opens, as the original (`weather::StartAtmos`). The renderer takes
  the tiles only when the rain's textures are there, as before.
- `vegetation` (`VegetationInterface`, `VegetationSystem`) holds our wind sway: the speeds from the C runtime's
  `Random(1, 2)` as the original (his come from `Locator::rng`), our phase step, and the frame's real time, as
  before. `GetFieldMatrix` is his, over our rounding (scale times 1.75, then the lean); trees read the lean through our
  `GetLean`, since our trees bend from their own component. His tree growth, bend points, rustle and
  `GetTreeMatrix` are left out: our trees do those in `ecs::Trees`. The vegetation shaders are not taken.
  His `UpdateBendPoints(handShown)` (the hand put away for a dialog or a script's cinema bars bends no trees,
  N-A-4) is taken as the original has it: the source cut is what the hand holds, not the hand, and the cut is the
  hand's own invisible state (`NotDrawn`, which the interface's inactive state gives), read by `ecs::Trees` through
  `ecs::HeldBendSource`. The bend pass (`ecs::UpdateTreeBends`) runs after the hand's frame update, where the
  original's landscape draw fills its bend table, so the cut starts and ends in the same frame as the original's.
  His `hand_visibility::IsShown` is not taken; the hand already stays still while put away
  (`HandSystem::Update` and `Place` stop while it is hidden).
  The rustle's rules are his `ECS/Systems/TreeRustle.h` names over our values, which `ecs::Trees` calls: the keys
  keep our alignment 2 (his have 0), and `IdleChance` is our roll, drawn for every tree before the distance tests
  and over 2^31 when paused (his is 0 then and skips the roll). His `k_Bank` and `VegetationInterface::Rustle` are
  not taken; the bank stays the editor's in `AnimationSounds`.
- `fieldSystem` (`FieldSystemInterface`, `FieldSystem`, `field_crop::Look` in `3D/FieldCrop`) runs our fields:
  `ProcessTurn` is every field's turn (`ecs::ProcessField`), `Update` the show and sink with the food, and
  `GetLook` reads our `components::Field`. Our `Update` takes the frame's real time as a `duration<float>` (his
  takes game milliseconds), so the seconds stay bit for bit. His crop model (`field_crop::Type`, `Crop`, `Grow`,
  `Settle`, his `Field` fields) is left out: our growth and colours round differently. The farmers' side stays
  `villagerFields`.

#### Forests, one slot for both jobs

his `forestSystem` slot was already ours, for the forests' own data (ids,
centres, town lists, the last planting turn). His forest miracle methods join that interface, so one
`ForestSystemInterface` serves both. `Plant` (the forest spell's planting when its seed lands, our spiral with the
wiki's rounding points), `CanGrowAt` (the spell's cast check) and `AddTreeNear` (the water miracle's sapling beside
a full grown tree, our 40-turn gap) have their callers move to the slot; `HasTrees` is declared over our forest
data. His `Reset` takes the place of our `Clear`. The draws and their order are unchanged. Our forest spell still
grows and withers its own trees in its turn, in the spell list's order, so his `ProcessTurn` (a separate stage after
the miracles) is not taken. His miracle forest entity and his loose-tree growth are not taken either.

#### Flowers

his `components::Flowers` and `FlowersArchetype::Create(position, type, angle, scale)` under his names,
and his `CreateFlowers(position, FlowersInfo, float, float)` signature, the floats passed as they come. The
archetype's body follows our `FeatureArchetype` rather than his copy of his, since both are the same multi-cell fixed
object in the original: our creation index, `affine::AngleY` for the turn, the static body when the mesh has a
physics mesh, and the map cells insertion; then his `MorphWithTerrain` and `Unlit`. Two things differ from his tree,
from the original: a patch of flowers casts no shadow on the land and receives no projected shadow
(`RenderingSystem`'s static shadow classes, `ShadowList`, `shadow_receiver::Object::flowers`). The patch is the last
created object, as a villager is. Flowers join the class lists where the game's flowers inherit a feature's
behaviour: the map cells (a multi-cell fixed object), the script type (FEATURE, no sub-type), the fire's info row
and the reactions removed when it starts burning.

#### Reward chests opening

his pure rules (`ecs::reward::Open`, `MagicAtLevel`, `TakesGoodList`, `NextMiracle`, `RandomMiracle`,
`StepOpenedClip` and the `OpeningWorld` seam, `ECS/RewardOpening`) come in as he wrote them, with his tests, and his
`RewardSystem::Tap` / `IsOpened` join our reward slot. His world adapter, `GameRewardOpeningWorld`, is refitted
onto ours: the town's belief through `town_stores::AddToBelief`, as the artefacts give it (his reaction system's
`GiveTownBelief` is not taken); the town's miracles through `worship::town::AddMagicTypesHeld` and the town centre's
spell count (his spell icon system is not taken: ours already holds the towns' miracles); the player's towns in
their own list's order (`map_cells::TownsOf`) where he sorted them by id; the player's miracles through
`magic::players`; the opened chest's clip through a `SkeletalAnimation` that our frame's animations pose (none of
his posing comes in); and the gesture on the land through a pure `magic::gestures::LandGestureTrail` beside our
gesture shapes and `particles::utility::ShowGestureOnLand`, in our recognised gestures' effect (his trail builder and
gesture system method are not taken). The tap is registered with our `hand_tap` classes; the owner and not-opened
test is the class's valid-to-tap, as the original's. His material blending component is not taken: our renderer
already draws a mesh's see-through parts blended by their material. Conflicts noted: the town order, the spell
icon system.

### Fire

#### Fire

his `fireSystem` slot and `FireSystemInterface` front our `ecs::fire`, and what it keeps between calls
(the fires, their graphics and their crackle, three slots before) is one `FireSystem` behind it, unchanged. Of his
methods, `ProcessTurn`, `Update`, `Reset`, `SetTemperature`, `SetOnFire`, `PutOut`, `HeatHeldObject`,
`StartedMoving`, `SetCanBeSetOnFire`, `SetHurtByFire`, `GetTemperature`, `IsOnFire`, `IsFireNear` and
`GetCharring` run our code. The magic loop and the script fire commands go through the slot, at the same points
of the turn; the rest of the game still calls `ecs::fire`. Our `FireEffect` stays a record of the fire system
rather than his `Fire` component: it outlives its object until the end of the turn, passes to another object (a
tree that dies) and is linked by address into its blaze and the crackle, so as a component it would live and move
differently. The villagers' fire fields (ours) are in his `Components/Fire.h`, and `IsInOnFireState` takes his
`IsRunningOnFire`. Left out: `MarkBurnPoint` (a fire made during the turn waits because the turn walks a copy of
the list), `ApplyBurn` (ours takes the effect's values), `Forget`, the blaze methods, the looks of charred,
glowing and burning things (the renderer works them out its own way) and `GetFires`.

### Vortex

#### Vortex rules

his `Magic/VortexRules` names (`openblack::vortex`) front our vortex's pure curves, with our
float bodies: `Openness`, `LevelAmount`, `GlowBrightness`, `GroundHoleThreshold`, `SwirlDepth` (the swirl's height
is the ground less it), `LevelsGround` and his fade and pull constants. His bodies work in double and are not taken:
they round differently (the swirl depth at 0.3 is one such case, pinned in `test_vortex`). The levelled square's
rules are his names and signatures with our float bodies (`CentreCell`, `SquareCell`, `AverageAltitude`,
`LevelWeight`, `LevelledAltitude`, `LevelSquare` and his reach and square constants), and our `FlattenLand` levels
through them in place of our `LandOffset`: the game rounds every step to a float, and his double steps leave a few
cells of the 50 to 56 ring one unit off (two such cells pinned in `test_vortex_rules`). `LevelWeight` is a helper
only; the height is not built from it. His `ElapsedSeconds`, `Advance` and `GroundTextureCoordinates` are not taken
until they are measured; our `AlphaByte` and `NextDecal` stay in `ECS/Vortex.h`.

#### Vortex service

his `vortexSystem` slot and `VortexSystemInterface` front our `ecs::vortex`, which keeps its
list in the script state, so `VortexSystem` holds nothing. `Create` keeps our signature (his raises the vortex by an
altitude above the ground, ours stands where the script puts it), `StartFadeOut` says whether the thing was a
vortex, and `ProcessTurn` is our end-of-turn step at the same point of the particle pass. Our two frame steps stay
two methods next to his (`UpdateGroundEffects` before the other effects' frame steps, `UpdateOverLandEffects`
after them), with our `SetParameters`, instead of his one `UpdateFrame`. The magic loop and the script natives go
through the slot; the debug window and the deletion hook still call `ecs::vortex`, the hook so that no entity's
destruction reaches the slot. Not taken: his `GetOpenness`, `GetGroundMarks` (our renderer reads the component's
decal), his `components::Vortex` and `VortexArchetype` (our `LandscapeVortex` has other fields), and his levelling
and hole colour, which are to be measured.

#### The crossing between lands (his PRs #349, #382, #389, #408, #409)

His vortex service gains (`EnterVortex`, `ThrowOutOfVortex`, `StruckBy`, `SetArrival`, `IsThrownOutByAVortex`) are not
taken as methods of a second service: each goes into our `ecs::vortex` where a caller needs it (`TakeIn`,
`TakeQueued`, `HandOff`, `ReactToPhysicsImpact`, `CreatureMustAvoid`, `IsVillagerBeingThrown`), with our
`VortexObjectInfo` records rather than his `DrawnIntoVortex` component. His `ParticleVortexRules.cpp` is rewritten on our
particle engine as `Particles/Rules/VortexAttract.cpp`, whose carrier is a creator as the tornado's is; his
`ParticleWorldInterface` is not taken. Of his pure rules, `Magic/VortexCrossingRules` keeps the pile share, the spiral
(rounded where the original stores, unrounded where it keeps its registers: our float discipline, not his doubles) and
the line structs; `Magic/VortexPullRules` keeps `CreatureMayBeTaken` and `TakesWhatStrikesIt`. His cell search, his
throw-out maths, his second land-script writer (`CommandFormat`, `PlaceText`, `Text`) and his arrival rules are ours
already (`ProcessIn`, `particle_carried_objects::Fling`, `lhscriptx::WriteCommand`, `ProcessOut`). His
`VortexCrossingLines` becomes `ECS/VortexSaveLines`, refitted to our components. His profile store's saved-games folder
is replaced by the game's own `SaveGame` folder (`vortex_save::SaveFolderUnder`), never written in a run of the checks.
His creature carry-over service is our `player_creature::SaveCreature`, called by `creature_removal`'s
`KeepForNextLand` after the hand and the fight let go. Where his differs from the original we follow the original: the
indestructible thing at the middle is not thrown back out in play, and the animals that come out are pointed at one
flock, not made its members.

### Light columns

#### Light columns and the heartbeat (his PR #250)

His `graphics::light_column` (`src/Graphics/LightColumn.h`) and its test are taken as they are; his draw
(`RendererLightColumns.cpp`) is refitted onto our transparency queue: once a frame, outside the temple, the columns
on screen go to `_frameLightColumns`, and each is a Z object of its own (`ZObject::lightColumn`), keyed as a model's,
drawn in mode 13 with its owner's colour set on it unlit and its strength as the instance's fade. His `heartbeatSystem`
slot is ours with his name, but cut down to `Pulse` and `StateOf`: it reads the guidance's heart beat
(`audio::guidance`), which already steps every turn, so his `ProcessTurn`, his `Heartbeat` component and his heartbeat
formulas are not taken (stepping it twice would double the beat), and only the local player has a heartbeat. Not taken
either: his town belief symbols (`UR_TownCentreBelief`, `TownBeliefMaths`), a second copy of our `particles::town_belief`,
whose numbers differ from his and are to be audited, and his public `InfluencePower`, which only his heartbeat read.
The Citadel debug window has the switch and a read-out of the heartbeat. Unverified against v1.42.

### Test hooks

None of its own: each system's hooks are on its page.

## Pending

- The decisions that leave a difference from the original in his version (for example the four bolt formulas of
  `LightningMaths`, `PreferCreatures` and the GPU ribbon corners) stay ours until his code is shown to match; each
  is re-checked when his later commits are audited (after the fixed build 0222f9f5).
