# Refactor guide

How the code base is organised after the refactor, and how to add code that fits the project's conventions.

See [PROGRESS.md](PROGRESS.md) for how much of the code base already follows these rules and what is left.
[TESTING.md](TESTING.md) describes the checks every change passes, and [SCENARIOS.md](SCENARIOS.md) how to write and
run a testbed scenario.

See [DEBUG_WINDOWS.md](DEBUG_WINDOWS.md) for the debug windows, what each controls and where a new debug control goes.

## Goals

- Reproduce Black & White's behaviour and visuals faithfully, with modern C++20 and a modern BGFX renderer. The design
  patterns of the original game are not copied.
- Keep game state out of globals and singletons, so that logic can be unit tested with fakes.
- Load every asset once, through the resource caches.

## Where state lives

| Kind of state | Where it goes | Example |
|---|---|---|
| Data that belongs to one entity | A component, i.e. a plain struct in `src/ECS/Components` | a villager's fire state, a town centre's belief symbols |
| World or game state shared by many systems | A Locator service | the map cells, the spell list, the weather |
| Anything loaded from a file | A resource cache entry, through a loader in `src/Resources/Loaders.cpp` | fonts, meshes, gesture templates, `info.dat` |
| Pure logic | A free function with no state | distance formulas, the spell grid's decay |
| Debug and test-hook state | `Locator::debugHooks` | trace throttles, scripted shot lists |
| What only changes how a thing is drawn | Its draw-pose component, never its `Transform` (the state hash covers `Transform`) | `CreatureDrawPose::scale`, a creature drawn smaller in its temple's pen |
| A value the original reads from the registry or the player's profile | `EngineConfig` through `Locator::config`, set by a command-line option with a fixed default, so every run is the same | `--creature-file`, the profile's creature |

- **A system's member that is really one entity's data** moves into a component on that entity, made when the state is
  first written and read through the const registry, so lands without such an entity get no new storage (the fight's
  held press `CreatureFightPress` on the fighter; the leash's last refusal `PlayerLeashRefusal` on the player's
  entity). The local player comes from `PlayerSystemInterface::LocalPlayer()`, not from the audio queries.

### Services

A service has:

1. an interface in `src/ECS/Systems/<Name>Interface.h`, with `[[nodiscard]]` on the methods that return a value;
2. an implementation in `src/ECS/Systems/Implementations/<Name>.{h,cpp}`, included only under
   `LOCATOR_IMPLEMENTATIONS` (that is, by `Locator.cpp` and by tests that build their own locator);
3. a `using` line in `src/Locator.h`, and its emplace and reset in `src/Locator.cpp`.

Lifetime rules:

- **Order.** Services are emplaced in the order of today's initialisation and reset in the order of today's teardown.
  A service that entities use while they are destroyed is reset after the entity registry.
- **Keep what tests set up.** Services that replace a former global keep a fake that a test set up before the game was
  created (`if (!has_value())`).
- **Fail clearly.** Release builds have no `ENTT_ASSERT`, so code that reaches a missing service fails with a clear
  message instead of a null dereference.
- **Free functions.** Many existing free functions now forward to their service, so call sites did not have to change.
  New code may call the service directly.
- **A facade owns its state.** Where his tree has one service in front of a whole area, the state its free functions
  need belongs to that service, and there is no public slot per store. The free functions reach their store through
  the facade (`Locator::magicSystem::value().SpellStore()`), with the same clear message when it is missing. A test
  that puts in a fake of the facade gets real stores with it (`test::InertMagicSystem` owns them).
- **A player's state that outlives the land.** Data that belongs to a player is a component on the player's entity,
  even when it lasts the whole game, but a land's clear takes the entity with the registry. The player system keeps a
  copy while no entity carries it: the land's clear (`magic::players::Reset`) moves the component off the entity into
  that copy, and the next land's entity takes it back when `PlayerArchetype` makes it, so there is only ever one copy.
  The entity's component is looked up through the const registry, so a land without that entity gets no new storage.
  The local player's `InterfaceAlignment` and `InfluenceCrossing` work this way
  (`magic::players::LocalInterfaceAlignment`, `LocalInfluenceCrossing`).

### Events

One-way notifications that used to be setter hooks are plain structs in `src/ECS/Events`, published through
`Locator::events`, for example a villager's death, help text or decision steps. The event manager calls handlers at
once, so the order inside a turn is unchanged. A handler must not publish an event of the same type it is handling.

### Resources

- Use `Locator::resources` and an `entt::hashed_string` id, and pass `.value()` to `Contains`.
- Files whose owners parse them themselves go to the **byte cache**. The owner parses from memory, through a span stream
  where needed.
- Do not read files with `std::ifstream` or `fopen` in game code. The only exceptions are user files (settings, saves,
  screenshots), crash logs and streaming audio or video.
- **Writing a file** the game later reads goes through the file system service too (`resources::WriteFile` in
  `src/Resources/CreatureFileWriter.h`, which opens it with `Stream::Mode::Write`), and the writer drops the cache's
  entry for that file, so that the next load through the cache reads what was written (`resources::SaveCreatureMind`,
  used when a land change saves the player's creature).

## Coding conventions (summary)

- **Comments:** plain English that describes behaviour. Never mention decompiled function names, Mac symbols, exe
  addresses or assembly; that detail belongs in `docs/bw1-notes`.
- **Modern C++:**
  - `std::unique_ptr`, `std::shared_ptr` and value types; never raw `new`/`delete`;
  - `std::optional`, `std::span`, `std::array` and `enum class`;
  - `constexpr` constants named `k_PascalCase`;
  - designated initialisers.
- **Naming:** inclusive language (main/replica, allowlist/denylist).
- **Drawn poses:** what moves once a turn is drawn between turns from a draw-only component (`DrawPosition`,
  `PhysicsDrawPose`, `CreatureDrawPose`); the logic and the state hash read only the Transform. An object's attached
  parts (eyes, hair, footprints, what it holds, the hand's touch) take the one matrix its body is drawn with
  (`ecs::DrawnModel` / `DrawnBodyModel`, which decides the drawn pose per row with const lookups and makes no storage),
  never a placement of their own from the Transform; what is sized apart from that matrix is scaled by the share the
  body is drawn at (`creature_pose::DrawnSizeShare`).
- **Rays from the screen:** the game's rays (the hand, the camera models, the gestures) start at the eye and are built
  from the camera's axes, field of view and aspect (`Camera::RayFromEye`), as the original's; `DeprojectScreenToWorld`
  starts about two near clips ahead and stays for the debug tools ([the mouse ray](../bw1-notes/hand-and-interface.md#the-mouse-ray)).

## Following raffclar's tree

raffclar's stack (`raffclar/openblack`, `stack/81-creature-mode`) is the architecture base, and his code is brought in
step by step (the merge plan). Where both trees have the same thing, ours takes his layout and names:

- **Names follow his code, comments stay plain English.** Identifiers take his names, including the ones he kept from
  the original game (`map_coords::JustMapXZ`, `gutils::LHArcTan`, `gutils::ConvertGameAngleToScawenAngle`). A name
  that doesn't say what it does gets a one-line comment that does. The comment rule above is unchanged.
- **Where shared code lives:**
  - map coordinates in `src/3D/MapCoords.h` (`openblack::map_coords`);
  - angle and distance utilities in `src/Common/GUtilsAngle.h` and `GUtilsDistance.h` (`openblack::gutils`);
  - the transparency sort in `src/Graphics/ZSort.h` (`graphics::zsort`);
  - the player's alignment in `components::Alignment`;
  - the influence components in `src/ECS/Components/Influence.h`.
- **A Locator slot takes his name together with his interface,** never alone. A slot renamed while it still holds our
  interface would let code ported from his tree compile against the wrong type.
- **Debug windows** are `debug::gui::Window` classes listed under Debug > Windows, one file pair each, with their logic
  in a `<Window>Model.{h,cpp}` of pure functions that a test covers. A window that acts on the land overrides
  `TakesEvent`, so the game doesn't also get the click. A switch that must hold with the window closed is kept in
  `UpdateAlways`, and gives back what it changed when it is turned off. Closed with every switch off, a window reads and
  writes nothing. A window that shows a module's private state reads a copy of it, a `View` struct returned by a
  getter of that module (`atmos_banks::GetView`, for his Atmos tab on our banks), never the state itself, and the
  getter asks nothing that has a side effect. The copy itself is a template over the state
  (`atmos_banks::MakeView`), so a test checks every field on a fake state, without the module's data.
- **The conformance pass** brings each area to his shape after the renames: his interface in front of our code, state
  holders turned into behaviour behind their interface, per-entity data into components. Every step keeps the output
  identical. A step that cannot (an unordered walk that no ordered structure reproduces, entities where we have none)
  is parked, and the user decides whether to take his behaviour.
- **His pure value types run by our module where we lack his system:** when his feature sits in a system we don't
  have yet, his pure part comes in unchanged with its test (`Magic/TribalPowerSpin`, `test_tribal_power_spin`; at
  most an accessor our call sites need, said in the commit) and the module that already owns the job in our tree runs
  it, with the original's call sites. When his system's slot comes, the module goes behind it (the tribal power
  column: `magic::hand_fx` behind `miracleFxSystem`, below).
- **Script natives with logic:** `CHLApi.cpp` only pops, turns ids into entities and pushes; the body is a free
  function over the injected service interfaces and the registry (`ECS/PlayerCreature`), tested with a recording fake.
  What it decides from the popped values is a pure plan (`PlanLoad`, `PlanScriptLoad`: species, body and point, or
  none), and two natives that make the same thing share the step that makes it (`LOAD_MY_CREATURE` and
  `LOAD_CREATURE` create through one helper), so they cannot drift apart.
- **A reader comes before its use.** A file format's fields are taken from his tree first, with the tests that pin the
  bytes, while the code that reads them stays ours; the change of behaviour is its own commit later. The fields reach
  the game only through the resource loaders, so nothing of the running game changes until that commit.
- **Renames are scripted** (`git mv` plus a list of replacements), so that they can be re-run on a new tip instead of
  rebased.
- **Reaction service:** his `Locator::reactionSystem` slot and `ReactionSystemInterface` name front our reaction
  list. Of his methods it carries the ones our list serves as he describes them: `RemoveFrom` (both),
  `Remove`, `IsActive`, `HasReaction`, `Reset` and `GetReactions`, over our `Reaction` record (`Find` keeps his
  name and returns our record). The shut-down of removed reactions moved into the service unchanged. His `Create`, `Move`, `ProcessTurn`, `ReactionsAt`, the land
  balance and his `Source`/`Active` records are not taken: our reactions are made and spread once by
  `ECS/Effects/Reactions` (the original offers a reaction once), where his re-spreads them every turn.
- **Villagers running from and watching miracles:** his state ids and the names of his two state functions
  (`villager_reactions::Fleeing` for 6, `Watching` for 7 and 30) are taken, behind our reaction slot and our scoring
  (the reaction type table's rows, `ScoreOf`, the records and the switch rule). Where his reading differs from the
  original, the original's is followed: the distances are in the plane, to the miracle where it is now; the urgency of
  a frightening miracle is whole-number arithmetic on the fast distance; the "a caster never runs" test is the
  creature's, not the villager's; the object gone also ends the reaction. The flee point is our animals' rule made a
  pure module (`ECS/Effects/FleeGeometry`), its arithmetic and its coming-towards test (in three dimensions) the
  original's; the animals keep their own copy until it is measured for them.
- **Belief from the miracles villagers react to:** his `Magic/Impressiveness` module and names come in
  (`ImpressionInputs`, `ImpressiveValue`, `ReactionMultiplier`, `ImpressionAlignment`), merged into our town belief:
  the belief goes through `town_stores::AddToBelief` and our `TownBelief` boredom, the distance's share is our
  `gutils::DistanceChangeToBelief` (his copy of the curve is left out), and the alignment through our
  `effects::alignment`. The impression is the villager's `UpdateHowImpressed`, called where the original calls it
  (the villager taking up a reaction), not his reaction system's. Where his reading differs, the original's is
  followed: the factors multiply in the original's order, the share comes from the town's stats, not a count of its
  villagers, and the shield's per-town overrides are added (`ShieldImpressiveValue`). His `BoredomAfterImpression`,
  `BoredomAtTownTurn`, the creature's multipliers and `FleeFromSpellPriority` are not taken: our town belief and the
  flee geometry already do those jobs. His belief voice (its own gaps, the hand's distance) is not taken either: the
  belief sound is the original's guidance path we already had (`audio::guidance::PlayBeliefRemark`), now asked for
  by `town_stores::AddToBelief` with the local interface's map point.
- **A pure rule of his is taken as his text, and its call site is fitted to ours.** `creature_spells::ValuesToSave`
  (what a creature is saved as while miracles are on it) is his function unchanged; `CreatureMindSystem::SaveMind` calls
  it on our components, with our `creature::LocalPlayer()` in place of a hard-coded first player and our
  `creature_morph::k_HeightAtSizeOne` in place of a second copy of the height of a creature of size one. The rule over a
  land script's globals is a pure function (`mind_detail::CurseValuesToSave`, over "does this script run" and "this
  global's value"), tested with fakes; only the file-local adapter reads the script machine, named apart from the
  system's names so that an unqualified call cannot find something else. `ValuesToSave` came in with R09, ahead of the
  rest of R03: the R03 port leaves out its `ValuesToSave` and `SavedBody` hunks.
- **A field of the game's tables keeps his name and his reading when the original agrees.** The miracle table's
  `knownAtStart` (field +0x54, [creature.md](../bw1-notes/creature.md#learning-by-watching)) is the computer players'
  teaching filter: it makes nothing known, and a creature starts knowing no miracle (R02).
- **A pure module of his is taken with the original's edges.** `Creature/PerceivedDesires` keeps his names and
  functions; the add follows the original's compare order (a NaN becomes 0, which `std::clamp` would keep), the index is
  signed as the original's, and the look (`LookYaw`) is added; the 30-value rings wait for the mind save. The writers reach the mind
  through `CreatureMindSystemInterface::EmpathiseWithPlayer` / `EmpathiseWithTownDesire` (the original's two player
  functions), not through his `MagicSystem::Empathise`; the villagers' town needs keep our event, passed on by
  `creature_mimic::AddMimicEventHandlers` beside the deeds (R07a).
- **Audio service:** his `Locator::audio` slot and `AudioManagerInterface` (with a virtual destructor) front
  our engine. `AudioManager` is the engine, `AudioManagerNoOp` only a base for test fakes, never a device
  fallback. The free functions of `Audio/Audio.h` are thin wrappers over the slot (`Audio/AudioApi.cpp`); a test
  injects a fake through the slot with `RestoreService`. The slot is reset after the registry.
- **Influence service:** his `Locator::influenceSystem` slot and `InfluenceSystemInterface` front our influence code
  in `ECS/Influence`, which keeps its state with the land in the registry. `InfluenceSystem` keeps nothing of its own.
  The turn's reach, the border drawn again, the hand's crossing with its ripples, the circles and the ripples the
  renderer draws go through the slot. Our anti rings and the measure (`CalcType`, allies) are methods too, and the
  border has its own `UpdateBorders`, because it is drawn later in the turn than the reach is worked out. Three of his
  methods are left out. `PlayerInfluence` by map position would round the distances ours measures in metres. `Reset`
  isn't needed, because a new land's registry starts the state afresh. `GetScrollOffset` isn't served, because the
  scroll clock is the renderer's. His `InfluenceSource` comes later. The other callers still use the `influence::`
  functions, which tests reach with fakes. The slot is emplaced unless a test put one in, and reset after the
  registry.
- **Animal service:** his `Locator::animalSystem` slot and `AnimalSystemInterface` name front our animals' shared
  state (`ecs::animal_ai` still runs each animal's turn in the living list). Of his methods it has the ones our
  components answer as they are: `SetScale`, `RadiusOf`, `LeaderOf`, `MembersOf` (leader first, our flock order),
  `GoalOf`, `GoalHeightOf`, and his `IsFrighteningToCreature` and `CanPlayerPickUp` as he wrote them. The hand keeps
  `animal_ai::ValidForPlaceInHand`. His turn, drawing, reset, creation, sending, fading and killing methods are left
  out: ours do those inside the animal AI, in another order.
- **Sound tags:** his `soundTagSystem` slot and the names of his `SoundTagSystemInterface` and `SoundTagSystem`
  front our port of the original's tags. Only the names are his: the signatures are ours (`ProcessTurn()` without the
  camera, `SetActive` and the rest on a `TagId`, two `CreatePointSound` overloads with the original's arguments, plus
  `Create`, `CreateAtMarker`, `Remove`, `Delete`, `Exists`, `Clear`, `TagSoundPoint` and `Point`), so a later port of
  his callers (the weather's thunder, the lanterns) must be adapted to them. His tags are entities with an audio
  emitter; ours stay one list inside the system, as the record `SoundTagSystem::Tag` of the implementation,
  because the original walks its tags newest first with ids as channel owners, and a land without tags must get no new
  storage. His component name `components::SoundTag` is left free for a later port of his code. The functions of
  `audio::tags` hand their calls to the slot, which is emplaced and reset next to `audioState`, where the tags' state
  lived before. The ambient point tag (the thunder's) is a `CreatePointSound` overload that takes an `AtmosType`. The
  tests inject the system through the slot and call it on their own reference.
- **Thunder:** his `lightning::k_ThunderClaps` and `k_FirstThunderClap` (eleven claps of the rain's bank, from the
  second) are taken into our `3D/Lightning.h`. The rest stays ours. The clap is `lightning::ThunderClap(ticks)`, picked
  from the wall clock's milliseconds as the original does, where his draws a C runtime number, which would add a draw
  to the random trace at every clap. The sound is `audio::tags::Thunder`, a delayed point tag in the rain's ambient
  bank. It is our storms' sheet lightning callback, which `audio::Init` sets and `audio::Shutdown` clears, so his
  thunder code inside the weather's update is not taken. The ticks come from `audio::TickCount()`, the game clock,
  which is fixed in the deterministic runs. His `ThunderSound(clap)` (a hashed sound id) is left out: our tags
  address the bank by sample number. His strikes near the camera are left out too.
- **Gui, the files taken from his tree:** `Dialog`, `TextDatabase` and `CreatureCaveScreen` are his text. `Canvas`,
  `DialogPainter` and `Controls` are his text with our deltas put back, each needed for the same pixels:
  - `Canvas`: his view and `Blend`/`SetBlend`, plus our coverage batches and the `InterfaceText` program that draws
    `graphics::GameFont`'s one-channel atlas;
  - `DialogPainter`: his `DrawPointer(Canvas&)`, plus `enum class Edges` with `HasEdge`, `MidTextSize()` and
    `SmallTextSize()` (one bigger when the help system asks for bigger text), and `DrawString` through
    `graphics::GameFont`;
  - `Controls`: the message box's fit rule, the label and edit box sizes through the text size functions,
    `IsCaretShown` as a tested pure function, `std::accumulate` for a list's height, and our `Zoomer` calls;
  - `CreatureCaveScreen`: with no creature the cave's window is not drawn at all, as in the original's empty cave.
- **Gui, the files that stay ours** (his versions would change what is drawn or when):
  - `GameFont` adapts the exact port in `graphics::GameFont`, read through the font cache. His parses the font files
    himself, with other glyph metrics and line breaks, so every line of text would move.
  - `ToolTips` is a handle over the help system's tooltips (`Help/ToolTips`). His is a separate state machine whose
    timing and priority differ, which would change the tooltips and the hand demos.
  - `GameInterface` keeps the SkipBox, which the deterministic runs answer, and loads its textures through the
    resource cache. His newer panels (tooltip glow and arrows, creature status and fight panels, cinema bars) are to
    come later as behaviour changes; his reads files directly, outside the loaders.
    It takes his pointer canvas: the game's pointer is drawn in its own view, `RenderPass::Cursor`, right after the
    ImGui view, so it shows over the debug windows. It is drawn where it was before (over the start-up requester and an
    open menu); his drawing of it over any debug window waits for `IsMouseOverWindow` (M21).
  - `GameMenu` keeps the escape menu's states and the pure functions behind them (`EscapeState`, `EscapeOverQuestion`,
    `PageAfterAnswer`, the video change prompt, the help speed slider), all covered by tests. His later edits are to be
    compared against it one by one.
- **File formats in components:** when he has a reader of one of the game's files as a component under
  `components/<name>` (his `Open`/`Write`, his result enum), it is taken as his text once a test shows that it gives
  the same fields as our parse, on the real file and on a synthetic twin. Our parse stays only as that test's
  reference. Our code reads through the component and converts to its own types. It still gets its bytes from the
  resource caches, through his `Open(std::span)`, never his `Open(path)`. Example: `components/gestures`
  (`GestureFile`, `Gestures.jty`), read by `magic::gestures::LoadTemplates` and so by `GestureTemplatesLoader`. As
  before, it ignores bytes after the last record. `test_gesture_file` pins it to the former parse.
- **Snow cover:** his `snow_cover` maths, `SnowSystemInterface` and `SnowSystem` (slot `snowSystem`), his `snow.sh`
  and `snow_object.sh`, and his `LandUnderSnow` and object-snow hunks fitted to our `fs_terrain`, `vs_object` and
  `fs_object`. Until his storm list (A18) is in our tree the snow is fed from our storms: each storm lays its snow in
  its own update (`Storms.cpp`) and the atmosphere melts it after them (`Atmos.cpp`), in the original's order, so the
  interface takes `AddStorm` and `Melt` in place of his `ProcessTurn(storms)`. Two of his numbers were changed, each
  pinned by `test_snow_cover`: the weather grid's byte is half the depth, towards zero, and a storm's snow is multiplied
  by its `snowCoverRate`. With no snow the shaders get `u_snow = 0`, which leaves every colour as it was, and the depth
  texture is only uploaded when the snow changes.
- **Snowfall:** his `snowfall` maths and `SnowfallSystem` (slot `snowfallSystem`), drawn by `RendererSnowfall.cpp`
  as one Z object per tile, like the rain. Two departures from his: the tiles are one per land block, from the same two
  weather samples the rain takes (the original's `Render3D` reads both from one loop), and the flakes are scattered on
  our CRT stream the first time a tile is drawn, not when the system is made, so a land without snow draws no number.
  The original scatters twice at start-up; that is a pending behaviour change, written in the wiki.
- **Creature shadow:** his `CreatureShadow` names (`Alpha`, `LightPoint`, the fade and light constants) over our
  `shadow_math` and `shadow_list`; the creature's entry takes the hand's half rows. His GPU cells and
  `creature_shadow.sh` are not taken.
- **Golden scenarios from his tree** (his emulations of the original code, such as `test/audio/scenarios`) are taken
  as data, with the scripts that made them (not built). His test is pointed at our code: through a pure function (a
  step extracted unchanged from a service, as `atmos_banks::StepBankVolume`) or through the Locator with fakes. Where
  our result differs, the game is not changed to fit: the test is `DISABLED_` with a comment that names the
  difference, and the difference goes to the behaviour items (and the wiki's Pending).
- **Pure rules ported ahead of their callers.** A port of his takes the pure code first (free functions in
  `src/<Area>/`, with their tests), and the system that calls it later. Until then the new kinds his step machines gain
  (a creature's cast and gesture steps, its sub-moves) are built by nothing, the new interface methods are virtuals with
  a default that does nothing, and the new component fields keep the value the game already behaves by, each with a
  comment saying so. The game's behaviour is unchanged because no agenda, system or script sets them: the proof is the
  code path, not a measurement. The switch over a step or movement kind still handles every new kind, so the build stays
  warning-free and the new kinds cannot be silently dropped once they are built. A fault his text has in a path nothing
  reaches yet is carried as he wrote it and written into the area's Pending, naming the commit it belongs to, rather
  than fixed inside the port: a change of behaviour is its own commit, and the original is read before it is made. What
  a port leaves out of a row of the plan (the fight interface's additions, when only the mind's are wanted) is said in
  the commit message and in the area's page, so the row that takes it can be found.
- **Sky dome tint:** his `3D/SkyDome` maths (`AlignmentPair`, `Darkness`, `TintOf`, `ThroughOvercast`) and his
  `fs_sky` uniforms on our dome build, fed with the alignment the original's way (0 good to 2 evil, so the darkening
  is on the good side), `Darkness` in floats and `TintOf` without the clamp below 0, as the original.
- **Near clipping:** his `Camera/NearClipping.h` whole (`NearPlane`, (h·0.05)·3.2 + 0.3, the original's float steps)
  and his close clipping (`SET_GRAPHICS_CLIPPING`, `CinematicDirectorSystem::SetCloseClipping`) in our `Game` update;
  the projection is made again whenever the plane changes, as the original sets it every frame. His
  `Camera::SetNearClip` and his path-camera close clipping are not taken.
- **Sampler defaults:** his `Graphics/ShaderSamplers` and `Graphics/SamplerDefaults` as he wrote them, his
  `ShaderProgram::Submit` on every draw and his `ShaderManager` reflection, `DiscardBindings` and `FrameEnded`; our
  `ShaderProgram` keeps its sampler flags, its uniform arrays and its warn-once log.
- **Sun glare:** his `ThroughOvercast` gives the glare's alpha too (`sun::GlareAlpha`), with the original's five
  samples hidden by our land ray cast (`sun::GlareSamplePoints`, `sun::HiddenSamples`); a behaviour change of its own.
- **Sky clouds:** his mist draw per cloud, but in the original's order inside our per-cloud loop (`Clouds::Layout`:
  x, y, z, the mist's start, size, k), not his layout-then-`CloudArchetype::Create` split; a behaviour change of its
  own.
- **Vertex blends:** his `3D/VertexBlend` (`Partners`, `Apply`), his blend sources and his `vs_object` seam blend on
  our creature morph program; the weight stays a float (row 1 of the source, no 32767ths) and is not clamped, and a
  vertex moves by placed + (partner - placed) x weight, as the original. His seam switch is not taken.
- **Land drawing:**
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
- **Render helpers:** his `sun::`, `moon::`, `ground_blobs::`, `tree_brightness::`, `HandLight`, `hand_water_glow::`
  (now in `src/Graphics`), `land_colour_stamps::`, `lightning::`, `FrameLandLight`, `mists::` (`3D/Mists.h`) and
  `detail_level::` names hold our maths. Where his numbers differ, ours stay:
  - the moon's hour angle is `t * pi / 12`, not his precomputed constant;
  - the hand light's strength keeps the fraction of the land colour's mean (his rounds it down);
  - the water glow looks no further than the island's own last cell (his: always 511);
  - the stamps' cell weights are worked out in floats (his in doubles);
  - the lightning's dip runs from 0.2 to 0.5 seconds (his from 0.1);
  - the trees are fully bright when the view or the light has no direction.

  The old names that other code calls (`frame_anim::Mist*`, `billboard::MistShrunkSize`, `guidance::MoonPhase`)
  forward to his. Not taken: his `Combine` stamp enum, `ThunderSound` and `FrameLandLightInputs` (our land light's
  inputs are the renderer's).
- **Graphics infrastructure:** his `UniformTable`, the dynamic `VertexBuffer` with `Bind(first, count)`, the
  `FrameBuffer` multisample and wrapping flags (bgfx's caps are asked only for a multisampled one), and the L3D blend
  spans, copy and `Edit*` accessors are his text. Our draws don't use them yet.
- **The hand's pure pieces** take his files and names over our formulas: `3D/HandCrossFade.h` (the state blend, owned
  by `HandSystem`), `3D/HandMorph` (`hand_morph`, the alignment look, a member of `HandSystem`), `3D/HandOrientation`
  (the heading along a ray), `Magic/HandMotion` (the grain's smoothers, `CubicSpline` and the pour) and
  `Magic/HandHoldPose` (`hand_hold`). His tests come with them, with our numbers where ours differ: hold times are not
  rounded, the spline is natural, and the morph blends at the drawn alignment. Left ours, because his would change what
  is drawn: his `Components/HandMorph.h` (ours lasts the whole game, not one land), `Pick`/`NextPoint` (ours uses the
  interaction point), `TurnToHeading`/`StandOnSlope` (our `HandMatrixRotation`), `HoldCycle`/`HoldFrame` (ours picks
  the clip by name) and `HandFade` (the state blend already does it). His navigation poses (H1) are a behaviour change
  of their own.
- **Mist, clouds and village lights:** his slots and interfaces, with our behaviour behind them, called from the
  renderer's PreDraw at the same moment of the frame as before:
  - `mistSystem` (`MistSystemInterface`, `MistSystem`): only a mist on screen moves its animation on, where his moves
    every mist.
  - `cloudSystem` (`CloudSystemInterface`, `CloudSystem`): owns our `Clouds` (`GetLayout()`), with no entities and
    no per-cloud random draw; the renderer still lays them out when a land opens, and moves their animation only
    while they are on screen.
  - `villageLightSystem` (`VillageLightSystemInterface`): his behavioural `Update(gameTime)` and `IsDark()` over our
    `night_lights` module, which reads the frame's table through `land_light::CurrentTable()`. The lights stay ours,
    with no entities, and keep their random draws at the first rescan rather than his at the lantern's creation.
- **Miracles:** his `magicSystem` slot and `MagicSystemInterface`, in front of our miracles under `src/Magic`.
  `MagicSystem` owns the miracles' state, as his keeps it as its own members: the spell store, the magic objects,
  the falling spell and the hand magic state are value members, reached through `SpellStore`, `MagicObjects`,
  `FallingSpellStore` and `HandMagic` (ours), and have no slot of their own. They are destroyed spells first and
  falling spell last, the order their slots were reset in, when the service is reset after the registry. Each method
  hands on to our `magic::` functions, in his groups and names with our types (`psys::ProcessInfo`,
  `magic::SpellCastData`): the casts, `CloseDown`, the spell events and paying, `SpellStrength`, `SpellAt`,
  `RainOnFire`, the dispensers and one-shot bubbles, the seed in the hand, the debug state and `GetSpells`, plus our
  turn steps (`ProcessGameInputs`, `ProcessTurnStart`, `ProcessForests`, `RunDebugHooks`,
  `ProcessSpellParticlesEndOfLoop`, `ProcessHandTurn`). In `Game.cpp` and the Miracles window the casts, one-shots
  and turn, frame and land steps go only through it; the hand effects' interface, the players' state hash and the
  window's read-only lookups (spell and effect info, strength, chant context, gesture icons) still call our `magic::`
  functions directly. His hand drive (`UpdateHand`, the tap, press and release actions, `GetHandCastState`,
  `OrbAlong`, `GetLastHandResult`) waits for the testbed's runner: our hand system casts by itself, and calling both
  would cast twice. His `MagicWorldInterface` serves his spell behaviours, which are not taken, so it waits for them
  too.
- **Gestures:** his `gestureSystem` and `gestureEvents` slots (one object, the second not owning) and his
  `GestureEventsInterface` as his text. `GestureSystemInterface` keeps only the members that map one to one onto our
  `magic::gestures` module: `Update` (the test stroke, the mouse sampling and `ProcessPowerUpSystem`, called from
  `hand_casting::Update` at the same point), `Reset` on a new land, `ForgetPath`, `GetScreenAspect`,
  `GetCircleSecondsLeft` and `IsGesturing` (the gesture trail's condition). Our recogniser, buffer and
  `PowerUpSystem` stay, with their state in `handMagicState`; they act on a gesture at once, so they put no events in,
  and the spiral selection and the R repeat stay ours only. His request list, `GestureRecorder`, `DrawPath` and
  per-frame view are not taken (our sampling reads the cursor, window and camera itself, every 28 ms of mouse
  events), nor his leash gestures (a later item, into `PowerUpSystem`). The per-turn `ProcessPowerUpSystem` stays a
  direct call.
- **Shields:** his `magicShieldSystem` slot and `MagicShieldSystemInterface`, in front of our shields
  (`magic::map_shield` and `magic::spell_shield`); his implementation is not taken. `MagicShieldSystem` holds no
  state: the shield lists stay in the magic objects and spell services. Declared, with a caller each: `ProcessTurn`
  (the domes' turn, still at the start of the miracles' turn), `Update(seconds)` (the domes' draw between turns, at the
  same place of the miracles' frame; the seconds are not used), `Reset` (both shield lists, in a new land) and
  `KeepsReactionOff` (the reaction spread's check, with map positions). The miracles' turn, frame and land steps and
  the reactions reach the shields only through it; the physics, the route plan and the villagers' shield reaction
  still call our functions directly. His other methods come with their first caller: `ShieldStruck` (ours goes through
  the spell class's table), `HasObject`, `CreatureAvoids` (our route plan asks per object), `ShieldAt` (the scripts do
  not ask yet). His `DomeDraw` and `GetDomes(turnFraction)` are declared with an empty default, and his
  `Renderer::DrawShieldDomes` (`Graphics/RendererShields.cpp`) reads them in the world view only, pushing each dome
  into our single Z queue keyed as a model (`zsort::SumOrder::XZY`, `graphics::shield_domes::Submit`) right after the
  models; it makes no bgfx call. `MagicShieldSystem` does not override `GetDomes`: our domes stay `MapShield` map
  objects, lerped by their rows and drawn and queued with the other models, re-melted on the land and not reflected,
  as the wiki has it. His Reflection draw, his bgfx-depth sort of a translucent view, his angle lerp and his
  `alphaTexturedAdditive` submit flag are not taken (our mesh's 4/5 -> 13 rewrite does what the flag does). Feeding
  `GetDomes` means taking the dome's mesh off the map object in the same, measured commit, or it is drawn twice. The
  queue's entry (`ZObject`) is in `Graphics/ZObject.h`, so that the hooks in their own files can push to it. The
  shield list as an order key on `MapShield` (plan C4) comes with the fireballs' and the shared counter, not with the
  slot.
- **Teleport, tornado and flocks:** his `teleportSystem` and `tornadoSystem` slots (`TeleportSystemInterface`,
  `TornadoSystemInterface`) and his `magic::FlockMiracleInterface`, in front of our stones (`Magic/Objects/MagicTeleport`),
  our tornado rule (`Particles/Rules/Storm.cpp`) and our flock spells (`Magic/Spells/SpellFlock.cpp`). The pattern is
  the miracles' one: the implementations hold no state and hand on to our functions; only the methods with a caller
  are declared, with our types, and the read-only lookups (a stone's seed, position, reaction and player, the hand's
  touch stones, the traces) still call our functions.
  - Teleport, his names: `CanPlaceStone` (the cast rule), `ShouldReact` and `DoTeleport` (the villagers' teleport
    reaction), `DropOnStone` (the hand; the dropper is not read: the stone must be the villager's player's), and
    `RouteStoneFor` (the worshippers), `ProcessTurn` and `Reset`; ours: `CanDropOnStone`, `RegisterDestination`,
    `Update(seconds)` and `RunDebugHooks`. The miracles' loop drives the turn, frame and land steps through it, at the
    same places as before. Our spell class still makes and removes its stones, so his `CreateStone`, `RemoveStone`,
    `GetStones`, `StoneOf`, `BlocksNewBuilding`, `SetupReact`, `StoneAlong` and `RouteWorshipper` wait, with the
    creature's teleport.
  - Tornado: `Carry(object)`, whether the particle carries the object (his returns the rotation to start with; ours
    keeps reading it from the object), `Update()` (ours puts what is carried where its particle is drawn, with no turn
    fraction) and `CarriedCount`. Our tornado lets go of what it carried when the particle goes (verdict D), so his
    `ProcessTurn` and `LetGo` are not taken; his `Candidates` and `TakeFromPile` wait, since our pile split draws its
    size from the effect after the new pot is made; `CatchCreature` comes with the creature's branch, and his `Reset`
    has nothing to clear in ours. Our carried mark takes his name, `CarriedByTornado`, and stays an empty component.
  - Flocks: `FlockMiracle` is held by `MagicSystem` and reached through `MagicSystemInterface::Flocks()` (his tree has
    it on his spell services). Both flock classes' table of operations hands `ParticleTypeOf`, `Start` and
    `ProcessTurn` to it, and it picks the flying or the ground flock's own step by the spell's class. The maths stay
    ours (verdict C). His `AnimalsLeft` and `FollowsHand` wait for his spell behaviours.
  - Same names, other signatures: a later port must not read his meaning into these. The teleport methods have his
    signatures. The others do not:
    - `TornadoSystemInterface::Carry`: his is `std::optional<glm::mat3> Carry(const std::shared_ptr<CarriedObject>&)`,
      the rotation the carried object starts with, or none; ours is `bool Carry(entt::entity object)`, whether the
      particle carries the object (our rule keeps its own carried list and reads the rotation from the object).
    - `TornadoSystemInterface::Update`: his is `Update(float turnFraction)`; ours is `Update()`, which puts what is
      carried where its particle is drawn and takes no fraction.
    - `FlockMiracleInterface::ParticleTypeOf`: his takes `const Spell&`; ours takes the spell's entity.
    - `FlockMiracleInterface::Start`: his is `void Start(SpellServicesInterface&, Spell&)`; ours is
      `int Start(entt::entity spell, const glm::vec3& position, SpellCastData*, const psys::ProcessInfo&)`, our spell
      table's start operation, with its result.
    - `FlockMiracleInterface::ProcessTurn`: his is `void ProcessTurn(SpellServicesInterface&, Spell&)`; ours is
      `int ProcessTurn(entt::entity spell)`, our spell table's turn operation, with its result.
- **A file format his tree has as a component is taken as a component,** once a test shows it reads the game's files
  to the same data as our reader: `components/psys` (the spell files, the enum headers and the light map bitmaps) is
  his text, and `test_particle_file_parity` keeps our former reader as the reference over synthetic files and, with
  the game installed, over every file of the kind. Our loaders keep their cache slots and call the component. Where our
  code adds something to his type (the spell file's name for the logs), our type derives from his rather than copying
  it, and an alias keeps the name the callers use until the interface port renames them. A function of ours whose name
  the component takes for a type is renamed (`SoundActionValue` → `SoundActionByName`).
- **Tables that are the same data** take his text (`Particles/ParticleTypes`: his 150 rows are ours, with a name
  column and his `particles::` namespace).
- **Pure rule files taken whole, ours applies them.** Where his tree has the rules of a mechanism as pure functions
  over a small value type and ours has the same rules over a component, his file is taken as it is and ours keeps its
  own functions as thin adapters: they copy the component's fields into his value type, call his rule, and write back
  what it changed. The call sites and the component stay as they are. First case: the prayer power of a running
  miracle (`Magic/SpellChants`, applied by `Magic/Core/Chants` to the Spell component, the creator behind a
  `SpellCasterInterface` made from the chant context) and of a worship site (`Magic/WorshipBattery`, applied by the
  end of the turn of `Worship/WorshipSite`). The two were shown to be the same rules by reading them side by side,
  and the tests that pinned our numbers now run through his functions. A call site in a file another session owns
  changes only by the hunk agreed with that session; the rest of his file stays unused (reached by his tests only)
  until its callers are ported or the owner agrees, and the commit says so. Where his rule differs from ours in a case
  the game does not reach yet, the adapter keeps ours (the spell point of a turn paid without a creator).
- **Camera help:** his `cameraHelpSystem` slot and interface over our `CameraHelp`, keeping our bits 0x10 (double-click
  flight) and 0x20 (second zoom), as `script-camera.md` reads them, where his has fights and turning around the mouse.
- **World camera:** `DefaultWorldCameraModel` takes his `_features`, `GetZoomScale`, `k_PitchPerInput` and
  `GetHandCues`, `camera_help::AutoPitchInput` and his hints and drag state (not acted on yet) over our feature bits,
  help feeds, map disc and woosh (M15). The land grip takes his `LandGrip`, `UpdateModeDragging` and `camera_pan::Pan`;
  the stop short of the land goes through our `LandIslandInterface::RayCast` (the original's own land cast, with the
  7500 sea rule) rather than his physics land cast and `SeaHit`, and the give-up first, with the original's flight.
  The edge and pitch drags act through his classifier, `EdgeRotate` (with `WarpCursor`) and `PitchStep`, plus the
  original's 0.45 edge in a window (our display mode), its RotateCCW report on still frames and its feature gates.
  Auto pitch is his `AutoPitchInput` and height, with the original's fifth, dropped player tilt and height rule.
  The wheel is his 60 a notch from `GetMouseWheelDelta`, read only with a zoom action, as the original.
  Both buttons take his `TwoButtonTurn` and cursor freeze call site, gated with the middle button on our bit 0x20.
  The clear view takes his half-second zoomer, on our `Zoomer`, and adds the original's glide to the hand's point.
  Watching a fight is ours (`camera::FightWatch`, `fight_orbit::`), his `DuringFight` mask under our bit 0x10.
- **Cinematics:** his `cinematicDirectorSystem` slot (the `screenFade` slot is gone) over our fade and bars, split into
  his `Gui/ScriptFade` and `Gui/CinemaBars`; his `Gui/ScreenFade` name holds our temple fade, which the falling spell's
  film still owns and which writes the script fade's colour. Our signatures stay: float seconds truncated as the
  original does, the bars' time passed by the caller, `SetFadeColour` and `SnapWideScreen`. Not taken: his owner,
  interface and dialog flags (ours are the help system's), close clipping (M11) and the reset as a land opens.
  His `test_screen_fade` is not taken: it expects a fade that lands exactly on its target to count a turn and reset,
  where the original holds it there (`docs/bw1-notes/video.md`, the temple fade), as ours does.
- **Placed camera paths:** his `CameraPathSystem` slot keeps `HoldsCamera` and `HandlePlayerControl`, with the path
  as an `entt::resource` from the camera path cache rather than a path read by the caller. His `FollowPlaced` is
  reshaped to the original's camera mode: `Begin` (refused while a script holds the camera), `FollowAt` (the path's
  time from the animation's drawn frame, given by its owner every frame) and `Release`, on the camera's own zoomers.
  Not taken: his own clock, his end at 999/1000 of the clip's ms and his end with the miracle.
- **The forest's camera creator:** his `AnimWithCameraCreator` (`ParticleForestRules.cpp`) gives the shape, an anim
  creator that only drives the camera, and the class name it registers, `ParticleAnimWithCameraCreator`, in our
  `Particles/Rules/Forest.cpp` beside the forest's other classes. It is built on our `ParticleAnimCreator` (its own
  registered factory reads the object, with no mesh), and the path comes from the camera path cache. Not taken: his
  reading of the `.cam` and the `.anm` from disk, his start in `InitAtom`, his clock and his end. Ours runs the
  original's particle: its step from `Creator::AfterAtomStep`, its draw once a frame, its removal from
  `Creator::AtomRemoved`, the rules as pure functions (`psys::forest_camera`). His Game.cpp wiring is taken in our
  frame's order: `HandlePlayerControl` before the camera keys, which are skipped while a path holds the camera, the
  path's `Update` in place of the player camera's update (his runs it every frame after the input), and the close near
  plane while a path holds the camera.
- **A particle's per-frame read of what it draws:** where the original does something in a particle's draw that the
  game needs (the camera particle's path time and its stop), the particle side has a small per-frame pass over those
  particles, called by Game.cpp with the frame updaters after the camera's update, as `town_belief::Step` is
  (`psys::forest_camera::UpdateFrame`). It reaches other systems only through their interfaces
  (`CameraPathSystemInterface::FollowAt` and `Release`), so they need no query into the particles. Its state is the
  rule file's module of the particle system (`ParticleSystemInterface::Module`), not `PSysManagerState.h`.
- **Villager speed:** his pure `ecs::villager_speed` (`StateSpeed`, `PersonalFactor`, `FinalSpeed`, in
  `ECS/VillagerSpeed`) and `test_villager_speed` are taken, term for term the same maths as ours. Our entity layer
  (`SetVillagerStateSpeed`, `SetVillagerSpeed`) gathers the inputs and calls them, with the player's tribal power and
  the town's belief left out until openblack has them. Ours stays in one place: a carrying capacity or a town needs
  divisor of 0, which only made-up villagers have, gives no term instead of a division by 0.
- **Weather:** `weatherSystem` takes his `WeatherSystemInterface` names in front of our weather code, and
  `components::WeatherInfo` is his (the same eight bytes as ours, so `weather::WeatherInfo` is now a name for it). The
  climate script commands, the climate switches, `KillStormsInArea`, `GetWeather`, `GetWeatherSmooth`, `GetOvercast`,
  `GetLightningFlash`, `GetSeason` and `Reset` call our functions over `weather::State`, unchanged: our weather queries
  keep the climates' temperature and wind, and our storms keep their draws. `GetDaysFromStart` stays a double. Left
  out until our weather can do them without new behaviour: `Update` (ours runs in three slots of the turn),
  `GetActiveStorms` and his `Climate` and `Storm` components (our storms are a list with ids, not entities),
  `ForceStorm`, `EndStorm`, `ClearStorms`, `StrikeLightning` and the miracle storm methods.
- **Town desires:** his `townDesireSystem` slot and `TownDesireSystemInterface` front our `town_desire` code, and the
  desire data (`TownDesire`, `TownStats`, `DesireSort`, `k_TownDesireCount`) is in his `Components/TownDesire.h`.
  The data stays inside `Town`, which includes that header, and each town still works out its desires in its own
  process, after its influence as in the original. His `ProcessTurn` is left out: his pass runs outside the town
  process, and his inputs would leave half the desires at 0 and lose the warnings and the amounts.
- **Dispenser rules:** `Magic/DispenserRules` takes his names (`DispenserTimer`, `DispenserStep`, `StepDispenser`,
  `OrbPosition`, `OrbStillThere`, `FaceTowards`, `DispensableMiracles`, the `k_Orb*` constants), and the
  `SpellDispenser` component his fields (`magicType`, `timer`, `orb`, `effect`). Our counting stays behind them: the
  still-there distance keeps the touch tolerance, `MakeOrb` leaves the count to the orb being made, and periods go
  through `game_clock::TicksForSeconds` (his `PeriodTurns` is not taken). His stored `orbPosition` is not taken
  either: ours measures against where the dispenser makes its orb now. The rest of the dispenser stays in
  `Worship/SpellDispenser`. The rules' tests take his layout: `test/test_magic_spells.cpp`, suite `DispenserRules`, in
  the `test_magic` group, written to our counting.
- **World effects facades:** his slots and interfaces, our behaviour behind them.
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
    is read from our storms, and the draws are the C runtime's. The streaks are placed when a land opens, not when
    the service is made. The renderer takes the tiles only when the rain's textures are there, as before.
  - `vegetation` (`VegetationInterface`, `VegetationSystem`) holds our wind sway: the speeds from the C runtime's
    `Random(1, 2)` as the original (his come from `Locator::rng`), our phase step, and the frame's real time, as
    before. `GetFieldMatrix` is his, over our rounding (scale times 1.75, then the lean); trees read the lean through our
    `GetLean`, since our trees bend from their own component. His tree growth, bend points, rustle and
    `GetTreeMatrix` are left out: our trees do those in `ecs::Trees`. The vegetation shaders are not taken.
  - `fieldSystem` (`FieldSystemInterface`, `FieldSystem`, `field_crop::Look` in `3D/FieldCrop`) runs our fields:
    `ProcessTurn` is every field's turn (`ecs::ProcessField`), `Update` the show and sink with the food, and
    `GetLook` reads our `components::Field`. Our `Update` takes the frame's real time as a `duration<float>` (his
    takes game milliseconds), so the seconds stay bit for bit. His crop model (`field_crop::Type`, `Crop`, `Grow`,
    `Settle`, his `Field` fields) is left out: our growth and colours round differently. The farmers' side stays
    `villagerFields`.
- **Fire:** his `fireSystem` slot and `FireSystemInterface` front our `ecs::fire`, and what it keeps between calls
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
- **Explosions:** his `explosionSystem` slot and `ExplosionSystemInterface` front our ground marks and camera
  shakes, and keep nothing of their own. `AddRubble` is our explosion mark (its turn still drawn by the blast from the
  particles' random numbers, before the call), `Update` and `Reset` are the marks' frame step and clear, which take
  the uprooted trees' craters along as before, and `AddShake` and `IsShaking` are our camera shakes, which still
  count down in the camera's real frame time. The marks stay entities kept in our list, newest first: his
  `GroundMark` and `DestructionGhost` components are not taken, since a storage walk would destroy them in another
  order, and our object ghosts are not his destroyed building's ghost. His dust puffs (`CollectDrawFrame`) are left
  out: our mark's dust is the smoke an object leaves when it goes.
- **The blast's extras (rubble, dust, ghost):** each stays ours where ours already does the original's job, and only
  the missing call is added. The rubble's turn is drawn from the effect's own particle numbers, as the original
  (his draws it from the game's local stream inside `AddRubbleMark`). The dust is our disappear smoke's brown puff on
  the C runtime's numbers, as the original (his `dust_puff` draws the local stream and skips the puffs' turn). The
  ghost of a building an effect destroys goes through our `ecs::object_ghosts`, called first in
  `abodes::DestroyedByEffect` as the original does; his `DestructionGhost` component and its renderer pass are not
  taken. His `ParticleObjectEffects.h` (the fireball and blast object interface) is not ported: it is reached through
  his engine's per-effect services, which our engine does not have, and our blast and fireball rules already reach
  the same world functions, with the original's order and streams, directly.
- **The mouse for the camera:** the action map takes his `GetMouseWheelDelta` (the notches turned this frame),
  `WarpCursor` / `GetCursorWarp` and the cursor freeze (`AllowCursorFreeze`, `IsCursorFrozen`, his
  `Input/CursorFreeze` and its test), off until the camera allows it. In the test runs (fixed mouse, ignored real
  input, `OPENBLACK_MOUSE_AT`) a warp or a freeze moves only the game's cursor, never the real pointer. Not taken:
  his scripted pointer, which our tree has no testbed for. Nothing calls them yet.
- **His behavioural interface in front of our engine.** Where the engine's core stays ours (the particle engine,
  `psys::manager`), the slot's interface takes all of his methods, also those with no caller in our tree yet, and the
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
  becomes turns with `psys::manager::SpotVisualTurns`. The engine-only calls (`SetPerFrame`, `GetDrawPath`, `IdOf`,
  `RunDebugHooks`, the draw collects) stay free functions until a method of his covers them; the draw goes with the
  renderer merge. The decisions taken with his names:
  - Where his type is not ported, his name stands for ours. `FindShield` returns our `psys::shields::DefensiveSphere`
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
    the hand ([miracles.md](../bw1-notes/miracles.md), Lightning); the per-effect offset and its weight are taken as he
    wrote them, their original behaviour not yet read.
  - The pause is his: while it is set, `ProcessTurn` does nothing, and `ProcessForSpell` and `ProcessByFrame` step
    nothing but give true for an effect still running. A new land leaves it.
  - `GetSoundCount` is the number of particle sounds playing or dying away, 0 without the audio service.
  - `GetFileNames` takes the names from the resource loaders: the names the spell file loader loads by
    (`resources::PSysFileNames`, beside `PSysFileLoader`, with its folder and suffix), sorted, each once; none, and no
    log, without the folder, as his.
- **A rule that asks its parent atom for a point.** His object rules find the parent's object by scanning the
  parent's data with a `dynamic_cast`. Ours keep that lookup but through a small interface,
  `psys::SurfacePointSource` (`Particles/ParticleObjectRules.h`): a rule whose atoms stand for something answers
  for them (the object rule today, the creature-spell rule next), and an emitter asks the first such rule of its
  parent, else takes the parent's place, as the game's render particles each answer `GetRandomSurfacePos`. His
  world-services queries (object position, random surface point) are not taken: the pure pick walks a
  `object_surface::Model` given by the game side, so its tests use a fake model.
- **A slot over several of our modules.** His `miracleFxSystem` (`MiracleFxSystemInterface`, `MiracleFxSystem`)
  fronts three modules of ours that keep their state where it was: the hand's effects (`magic::hand_fx`, in the hand's
  magic state), the globes (`magic::one_off`) and the piles (`PotArchetype`). The system keeps no state and forwards.
  His single `Update(seconds, gameSeconds)` runs the globes, the hand and the piles one after the other; ours are
  stepped at three moments of the frame (the magic loop's start, the hand casting step, the hand system's update), so
  his private steps are public methods under his names (`UpdateGlobes`, `UpdateHand`, `UpdatePiles`), each called
  where its module was, and his `Update` is not taken. Where our call needs more than his signature gives,
  ours is added to it (`SeedInHand` takes the seed, whose effect plays in the hand). His callers guard with
  `has_value()`, and so do ours; the test services emplace the slot, since the modules it fronts always ran without
  any set-up. The renderer reads the tribes' names through the slot: his `GetTextTexture` and `GetTribalPowerText`,
  the latter for one ring or column (ours), and our `GetTribalPowerRunners`, all with empty defaults and forwarded to
  `magic::hand_fx`. Each ring or column stays a Z object of its own in our single queue
  (`graphics::tribal_power::Submit`), keyed at its centre and drawn with our text shader, as the wiki has it; his one
  submit of every name in a fixed slot of the frame is not taken. His `BubbleMesh`, `BandMesh` and
  `UpdateTribalPower` wait for the renderer merge.
- **The globes' draw.** His `Renderer::DrawGlobes` is in his file, `Graphics/RendererMiracles.cpp`, but it reads a
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
  (`RingTurn` is `BandRotation` transposed). His `k_GlobeAlpha` 150 is the orb's tint byte; the original multiplies
  it with white first, so the globe is drawn at 149 and its rings at 34, ours; his draw takes 150 itself (rings 35),
  which is not taken. Feeding the list means taking the entities' meshes off in the same, measured commit, or every
  globe is drawn twice.
- **The exploded pieces' draw.** His `Renderer::DrawParticleFragment` is the draw of a model's exploded pieces (our
  `psys::Creator::Kind::MeshPiece` atoms, his fragments), in `Graphics/RendererParticles.cpp`, but with our data and
  signature: it takes our `world_triangles::Frame` and the atom tag, has no depth argument (the original gives the
  pieces no Z object), and its body is the `world_triangles::Submit` both call sites made before. The pieces stay
  built on the CPU (`mesh_pieces`), lit with the integer land light and drawn at once with each primitive's material,
  in the world view only. His fragment shader, translucent-pass submit, sorting, two-sided culling and blend state are
  not taken, since each of them would change the pixels against the wiki; the drain's `Fragment` case stays empty.
- **The hand's glow and bands.** His `Renderer::DrawHandGlow` is in `Graphics/RendererMiracles.cpp`, but it takes the
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
- **The particle draw list:** ours under his names in `Graphics/ParticleDrawFrame.h`; our Z queue and draw order stay.
- **The light sheet's draw.** His `Frame::lightSheets` and `draw::AddLightSheet(Frame&, span<const LightSheet::Vertex>,
  span<const uint32_t>, sortPoint)` keep their names, but the frame's sheets are our `psys::surf_revol::Surface` (the
  stars texture and its alpha, additive mode 13, no depth write, two-sided, `specularInPass`), filled by
  `draw::AddLightSheets` from `ParticleSystemInterface::LightSheets()` once a frame in `CollectFrameParticles`, the
  newest first as the game walks its list, each keyed at its middle read before its build. Each sheet is one entry in
  our single Z queue, after the Queued effects, drained by the `Surface` case through `DrawParticleSurface`, in the
  main view only (his draws it in the reflection too; the game does not). His `ParticleSurface` program (vs / fs
  `particle_surface`) is taken for the sheet's one pass, the specular added after the texture and clamped; his
  material table, `draw::Order` and the shared transient buffers are not, and our effects' surfaces keep their two
  passes.
- **His particle rules on our engine.** A rule class only he has is ported onto `psys::Effect` (a `psys::Modifier`,
  registered by a `Register*Rules()` call in `PSysRegistry.cpp`), in his file (`Particles/Particle<Topic>Rules.cpp`),
  with its pure maths in his file and namespace (`Particles/<Topic>Maths`, `particles::maths`). Where his maths leans
  on a helper our tree already has for the same algorithm, ours is used instead of a second copy, once ours is shown
  bit-exact against the original (the simple beam's curve is our `key_points` spline, made bit-exact first and given
  the three-axis reader, not his `KeyPointSpline`). His world-services layer is not taken: the rule reads
  the land, the registry and the noise as our other rules do, and its tests inject fakes through the Locator.
- **The glints' targets, his names on a slot of their own.** `ER_GlintsOnTarget` (`Particles/ParticleGlintRules.cpp`,
  its maths in `Particles/GlintMaths`) reads its objects through the four methods his rule calls on his
  `ParticleWorldInterface`: `ObjectPosition` (whose `has_value()` is the availability test), `TargetPointCount`,
  `TargetPoint` and `TargetScale`. Our engine has no per-effect services to put them on, so they are a Locator slot,
  `glintTargets` (`ECS/Systems/GlintTargetsInterface.h`), implemented in his file name,
  `ECS/Systems/Implementations/ParticleWorldGlints.cpp` (`GlintTargets`), so that a later port of his world services
  can take them in as they are. His implementation serves his globes; ours serves the objects the original gives
  glints, the script highlight and the spell seed graphic. The implementation takes the meshes' parts as a function
  (the resource cache's by default), so its test runs on fake parts with no renderer.
- **The object draw list, ours only.** The pure stages are in `ECS/DrawList/` (no Locator), the state is a service
  (`DrawListSystemInterface`, slot `drawListSystem`) whose `Update` takes everything it reads as arguments, so its
  tests build it directly. What belongs to an object stays on the object: its listed mark is the component
  `DrawListed` and its DontDraw mark the component `DontDraw`, so both go with the entity and an index made again is a
  new object. The objects reach the service through a probe (`draw_list::RegistryObjects` over the registry it is
  given, a fake in the tests), and an object's Draw is a consumer the caller passes per type.
- **The render passes:** the passes both trees have already use his names; `MainBlended` stays (not his `Translucent`).
- **The map's cells:** `entitiesMap` takes his `MapInterface` shape: `GetFixedInGridCell` and `GetMobileInGridCell`
  as spans, `GetAllInCell`, `Sync` (was `Rebuild`) and `Refile`, and the registry has his `OnConstruct`. Behind them
  `MapProduction` reads our ordered linked lists (`ecs::map_cells`), which our hooks keep as things are made, move and
  go; his `CellLists` and his registry listeners are not taken. A span is a copy of the cell, kept until the next call,
  since our lists run through the things in them. As in his tree, the walkers' obstacle grid moves to the
  pathfinding (`FileObstacles`, `ObstaclesIn`), but it stays our grid and is filed where the map was rebuilt before (at
  the start of each turn, on load and before a reaction spreads outside the turn), from the map's `Sync`: the filing
  order and the sets' order stay as they were. Not taken: his second `Sync` after the living's turn (ours refiles
  the walkers as they step; the other movers wait for the next turn, so that `Sync` would be a change of behaviour),
  and his map reset before the registry (our map listens to no registry signal, so his reason does not apply, and
  nothing shows that the registry's teardown never reaches the map). His `test_map_cells` tests his `CellLists`, so
  it is not taken; `test_map_interface` covers ours under his names.
- **His creature mode test is not taken:** its follow, Creature Mode and Creature Cave tests are already ours, word for
  word, in `test_creature_follow` and `test_creature_cave`. Its double-click test is of his own press timer; our
  Creature Mode reads the game's own `DOUBLE_CLICK` action instead, which `test_creature_mode_system` covers.
- **Blocked actions and missed lettings go:** the action map takes his `SetBlockedActions` (blocked actions read as
  not held, nothing blocked by default; the camera's help will feed it in a later change), his `PointerState` and his
  making up of the key and button releases that went elsewhere (`ReleaseKeysNoLongerHeld`, and the buttons the menu
  or a debug window took). `PointerState` keeps our test-run reads: under a fixed mouse or ignored real input it reads
  no real button and gives the fixed or the game's own cursor. The made-up releases only run outside the test runs
  (no fixed mouse, no ignored input, no `OPENBLACK_MOUSE_AT`), so the runs read no real key or button state.
- **The left-handed hand:** his `EngineConfig::rightHandedHand`, the debug bar's Hand menu and the options box's
  left-handed box kept in step with it. A right-handed hand mirrors the model's x scale, which our hand applies where
  it sets the scale (`hand_detail::HandModelScale`) rather than in Game. Ours defaults to off, unlike his: our hand
  has always been drawn as the mesh is, and the mirror is to be checked against the original before it becomes the
  default. His seed-holding pose's handedness is not wired: our hold pose doesn't use it yet.
- **Forests, one slot for both jobs:** his `forestSystem` slot was already ours, for the forests' own data (ids,
  centres, town lists, the last planting turn). His forest miracle methods join that interface, so one
  `ForestSystemInterface` serves both. `Plant` (the forest spell's planting when its seed lands, our spiral with the
  wiki's rounding points), `CanGrowAt` (the spell's cast check) and `AddTreeNear` (the water miracle's sapling beside
  a full grown tree, our 40-turn gap) have their callers move to the slot; `HasTrees` is declared over our forest
  data. His `Reset` takes the place of our `Clear`. The draws and their order are unchanged. Our forest spell still
  grows and withers its own trees in its turn, in the spell list's order, so his `ProcessTurn` (a separate stage after
  the miracles) is not taken. His miracle forest entity and his loose-tree growth are not taken either.
- **Town aggression:** his `ECS/TownAggression` rules (`Record`, `Attacked`, `ProcessTurn`, `AggressionFromDamage`)
  are his text, every constant checked against the exe, with his test; `SecondsSinceAttacked` is added beside them for
  `GET_TIME_SINCE_OBJECT_ATTACKED`, so the native only pops and pushes. His `TownAggression` component is not taken:
  the record is a field of our `Town` component, which already held the last aggressor and the wants of protection
  and mercy (now the record's), so no new storage appears in the registry. His `TownDesireSystem` turn and his
  `world_objects::AttackTown` are not taken either: the fade runs in our town process at the original's step 13,
  and the attacks reach the record through our `town_emergency::UpdateAggressor` and `AttackTown`, called where the
  original calls it (a destructive effect, burning, a Living's physics impact, a physical shield struck). Unlike his
  `AttackTown`, an attack with no harm still counts, as in the original.
- **The timed dominant desire:** his `Creature/CreatureSpellMind` names are the API (`SetCheatDominant`,
  `StepCheat`, `ClearCheatDominance`, `HeldDownByCheat`, `MakeFullyDominantOverOthers`, and his `Cheat`, now the
  state the mind component keeps), as thin wrappers over our rules read from the original, which move to
  `creature_desires::detail`; `CheatTurnsLeft` is added beside them for the creature window. His `k_CheatSeconds`
  (20000) is the mood and need spells' own seconds, so it stays as the default and the caller may pass others (a
  leash, a town tie, a script). Where his reading differed, the original's wins: every other desire goes to the
  species' floor (so `SetCheatDominant` takes the floor), the chosen one's sources are filled, the wish to make friends
  under compassion ends back at the floor, and the clear lets go only when a desire was dominant (so
  `ClearCheatDominance` takes the cheat). His `MakeFullyDominantOverOthers` stays his (let go, then dominant over the
  floor, no sources and no activation, as the original's two Hunger calls that do the same), and his
  `MakeLeastDominant` stays his text, with his test's expectation (measuring it against the original is its own
  commit). His other tests run on the original's numbers.

## Exceptions that remain

Some state stays where it is, each with a comment that says why:

- a lazily read environment variable that never changes, kept as a `static const` local;
- a `std::once_flag` for a warning that is logged once;
- the physics bodies, which stay an ordered list owned by `PhysicsObjectsSystem`, because components would change the
  iteration order the simulation depends on;
- a few C arrays and `new` expressions required by a C API or a private constructor.
- **The file dialog** (`Common/FileDialog`, `Debug/FileBrowser`, raffclar's code taken as it is, M23). It runs other
  programs and touches files outside the resource caches: on Linux it finds zenity or kdialog with `std::system`
  (`command -v`) and runs them with `popen`, on macOS it runs `osascript` with `popen`; on Windows it shows the
  shell's own dialog through COM (`IFileOpenDialog`, `IFileSaveDialog`), with no shell command. The debug browser
  lists folders with `std::filesystem::directory_iterator`, and `RememberPath` reads and writes
  `remembered_paths.txt` in SDL's per-user preferences folder with `std::ifstream`/`std::ofstream`. None of this is a
  resource-cache load: the player picks a file outside the game's data, at runtime, from a debug tool, so there is no
  asset id to cache it under and nothing to load once. `RememberPath` and `RememberedPath` are taken with the file but have no caller in our tree (his callers, the creature spawner's mind files and the testbed, are not ported): `remembered_paths.txt` is never read or written, not at start-up, not when a dialog is used, not in the runs; the tests cover only the pure `ParsePaths`/`FormatPaths`. The dialog itself is reached only from an explicit button in a debug window
  (the **File Dialog** window, closed by default); nothing calls it at start-up or in the verification runs.

## Still to do

Counts are in [PROGRESS.md](PROGRESS.md).

- Remove the `Game::Instance()` singleton. The map script globals, the day/night clock and the screen fade are already
  in progress.
- Move the exe addresses that production tables keep as data into the docs. Function names now follow raffclar's
  tree (see "Following raffclar's tree").
- The Locator slots with a counterpart in raffclar's tree now take his names (`time`, `particleSystem`,
  `villageLightSystem`, with their interfaces and implementations). Their shapes follow his in the conformance pass.
- Build a mod SDK once the game is fully rebuilt (see [SDK_FOUNDATION.md](SDK_FOUNDATION.md)).
