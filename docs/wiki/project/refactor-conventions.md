# Refactor conventions

## Summary

How openblack's code base is organised after the refactor, and how to add code that fits the project's conventions:
the goals, where each kind of state lives (components, Locator services, events, resource caches), the coding
conventions, the general rules for following raffclar's tree, and the exceptions that remain. The per-system decisions
taken while porting his code are on the six port-decision pages linked from
[Following raffclar's tree](#following-raffclars-tree). How much of the code base already follows these rules is in
[Progress counts](#progress-counts); the checks every change passes are in
[Testing and verifying a change](testing.md#summary), and how to write and run a testbed scenario is in
[Testbed scenarios](scenarios.md#summary). The debug windows, what each controls and where a new debug control goes,
are in [Debugging](debugging.md#windows).

## The original

These are openblack's conventions, not the original's. The one rule they take from the original is fidelity: the
behaviour and the visuals of the frozen v1.42 `runblack.exe` are reproduced exactly, with modern C++20 and a modern
BGFX renderer, and its design patterns are not copied ([How to read the pages](reading-the-wiki.md#how-to-read-the-pages)).
The addresses and decompiled names that explain a piece of code live in this wiki, never in the code
([Coding conventions](#coding-conventions-summary)).

## openblack

### Goals

- Reproduce Black & White's behaviour and visuals faithfully, with modern C++20 and a modern BGFX renderer. The design
  patterns of the original game are not copied.
- Keep game state out of globals and singletons, so that logic can be unit tested with fakes.
- Load every asset once, through the resource caches.

### Where state lives

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
  entity). The local player comes from `PlayerSystemInterface::GetLocalPlayer()`, not from the audio queries.

#### Services

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
  copy while no entity carries it: his `PlayerSystemInterface::KeepForNextLand`, at the start of the land's clear
  (`Game::PrepareNewLand`), moves the component off the entity into that copy, and his `TakeUpKept`, which
  `PlayerArchetype` calls as it makes the next land's entity, gives it back, so there is only ever one copy.
  The entity's component is looked up through the const registry, so a land without that entity gets no new storage.
  The local player's `InterfaceAlignment` and `InfluenceCrossing` work this way
  (`magic::players::LocalInterfaceAlignment`, `LocalInfluenceCrossing`).

#### Events

One-way notifications that used to be setter hooks are plain structs in `src/ECS/Events`, published through
`Locator::events`, for example a villager's death, help text or decision steps. The event manager calls handlers at
once, so the order inside a turn is unchanged. A handler must not publish an event of the same type it is handling.

#### Resources

- Use `Locator::resources` and an `entt::hashed_string` id, and pass `.value()` to `Contains`.
- Files whose owners parse them themselves go to the **byte cache**. The owner parses from memory, through a span stream
  where needed.
- Do not read files with `std::ifstream` or `fopen` in game code. The only exceptions are user files (settings, saves,
  screenshots), crash logs and streaming audio or video.
- **Writing a file** the game later reads goes through the file system service too (`resources::WriteFile` in
  `src/Resources/CreatureFileWriter.h`, which opens it with `Stream::Mode::Write`), and the writer drops the cache's
  entry for that file, so that the next load through the cache reads what was written (`resources::SaveCreatureMind`,
  used when a land change saves the player's creature).
- A file that his tree reads into a **typed cache** gets that cache, with his manager, getter and id names, even where
  ours parses it out of the byte cache: the parse moves into his loader's `FromBufferTag`, unchanged, and the bytes
  still come through the byte cache. The physics materials are the first: `PhysicsMaterialsManager`,
  `GetPhysicsMaterials()`, `physics::k_MaterialsId`, with our reading of the file (`physics::ParseMaterials`).
  The hand demos follow (`HandDemoManager`, `GetHandDemos()`, by the name's hash), with our records
  (`hand_demo::ParseRecords`) rather than his `components/hnd` file.
  The clips' sounds too (`ClipSoundsManager`, `GetClipSounds()`, `audio::clip_sounds::k_TableId`), with our
  reading of SmallSounds.SAS (`audio::clip_sounds::Parse`) rather than his `components/sas`.
- **Camera zones:** his `camera_zones::CrossFence` (`Camera/CameraZones`) replaces our `InsideInclusion`, shown to
  give the same answers, bit for bit, on made-up fences (`CameraZones.TheCrossingIsTheOneItReplaced`). His other zone
  rules (the push back inside the fence, the height limits, the flights, the influence) come with the fence's
  behaviour row, once settled against the original.
  The zone files go into his `CameraZoneManager` (`GetCameraZones()`, by "camera/zones/<name>") with our reader
  (`player_camera::ReadZoneFile`) rather than his `components/exc`: his drops a file cut short, ours keeps what was read
  before the cut, closer to the original, which checks no read.
  The zones a script sets live in his `cameraZoneSystem` slot (`CameraZoneSystemInterface`: `SetZones`, `GetZones`,
  `GetFileName`), made with the script state they lived in and again with each land, as his and as the original,
  which clears them with the last map. His fence's `FenceHit`, `GetForceField` and `GetFenceHits` come with its
  behaviour.

### Coding conventions (summary)

- **Comments:** plain English that describes behaviour. Never mention decompiled function names, Mac symbols, exe
  addresses or assembly; that detail belongs in this wiki.
- **Modern C++:**
  - `std::unique_ptr`, `std::shared_ptr` and value types; never raw `new`/`delete`;
  - a C library's handles held as `std::unique_ptr`s with deleters, declared in the order they are made so that
    they go in reverse (the OpenAL device, context and sources: `audio::device::DeviceHandles`, as his
    `AudioPlayer`);
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
  starts about two near clips ahead and stays for the debug tools ([the mouse ray](../hand/animation-and-placement.md#the-mouse-ray)).

### Following raffclar's tree

raffclar's stack (`raffclar/openblack`, `stack/81-creature-mode`) is the architecture base, and his code is brought in
step by step (the merge plan). Where both trees have the same thing, ours takes his layout and names:

The target is a fixed build of his (user, 2026-10-11): his `master` at `0222f9f5`. Every lane works on the rows of the
port audit up to that build first; his later commits are read only when one closes an open row or fixes a fidelity
difference (user, 2026-10-15). The work stays on our tree, so that every step can be verified bit for bit:

- **Where both trees do the same job,** his version is taken only once it is shown to be better or to behave
  correctly, bit for bit, against ours (the same checks, or a test that pins the original's numbers). If it is not,
  ours stays, moved behind his names and interfaces.
- **Where only we have something,** ours is ported onto his architecture: his directory layout, names, Locator slot
  names, `<X>SystemInterface` / `Implementations/<X>System` pairs, and his test layout.
- **Where only he has something,** it stays as he wrote it.
- A move or rename keeps behaviour identical, and is parked rather than forced if it cannot. A deliberate change of
  behaviour (taking his version of something, or a fix) is its own commit, measured and described.
- Ported systems are wired into the game as soon as they can be, at the point and with the trigger the original uses
  (user, 2026-10-14).

The general rules:

- **Names follow his code, comments stay plain English.** Identifiers take his names, including the ones he kept from
  the original game (`map_coords::JustMapXZ`, `gutils::LHArcTan`, `gutils::ConvertGameAngleToScawenAngle`). A name
  that doesn't say what it does gets a one-line comment that does. The comment rule above is unchanged.

- **Where shared code lives:**
  - map coordinates in `src/3D/MapCoords.h` (`openblack::map_coords`);
  - angle and distance utilities in `src/Common/GUtilsAngle.h` and `GUtilsDistance.h` (`openblack::gutils`);
  - the transparency sort in `src/Graphics/ZSort.h` (`graphics::zsort`);
  - the player's alignment in `components::Alignment`;
  - the influence components in `src/ECS/Components/Influence.h`.

- **Test files and suites take his names, the bodies stay ours** where they pin our functions:
  `test_gutils_angle` (`GUtilsAngle`), `test_gutils_distance` (`GUtilsDistance`), `test_alignment` (`Alignment`).
  `test_game_angles` stays ours: it pins `affine::ArcTanOctant`, `GetYAngle` and `WrapAngle`, which he has no
  test of. His `test/pack/test_pack_bank_info` is in the audio group, without his headers-only read case, which needs
  his `ReadFile` with unread blocks and `GetUnreadBlock`; ours does that with `ReadAudioHeaders`.

- **A field only the original has goes in its own component beside his.** His component stays as he wrote it; ours
  sits next to it on the same entity and is named for what it holds. The player's creature, the one the original keeps
  for each player, is `components::PlayerCreatureSlot` beside his `PlayerCreatures` list. A player without an entity
  has the same value kept by the service, as `PlayerMagic` is (`MagicWithoutEntity`).

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

- **A reader comes before its use.** A file format's fields are taken from his tree first, with the tests that pin the
  bytes, while the code that reads them stays ours; the change of behaviour is its own commit later. The fields reach
  the game only through the resource loaders, so nothing of the running game changes until that commit.

- **Renames are scripted** (`git mv` plus a list of replacements), so that they can be re-run on a new tip instead of
  rebased.

- **A pure rule of his is taken as his text, and its call site is fitted to ours.** `creature_spells::ValuesToSave`
  (what a creature is saved as while miracles are on it) is his function unchanged; `CreatureMindSystem::SaveMind` calls
  it on our components, with our `creature::LocalPlayer()` in place of a hard-coded first player and our
  `creature_morph::k_HeightAtSizeOne` in place of a second copy of the height of a creature of size one. The rule over a
  land script's globals is a pure function (`mind_detail::CurseValuesToSave`, over "does this script run" and "this
  global's value"), tested with fakes; only the file-local adapter reads the script machine, named apart from the
  system's names so that an unqualified call cannot find something else. `ValuesToSave` came in with R09, ahead of the
  rest of R03: the R03 port leaves out its `ValuesToSave` and `SavedBody` hunks.

- **A field of the game's tables keeps his name and his reading when the original agrees.** The miracle table's
  `knownAtStart` (field +0x54, [Learning by watching](../creature/mind.md#learning-by-watching)) is the computer players'
  teaching filter: it makes nothing known, and a creature starts knowing no miracle (R02).

- **A pure module of his is taken with the original's edges.** `Creature/PerceivedDesires` keeps his names and
  functions; the add follows the original's compare order (a NaN becomes 0, which `std::clamp` would keep), the index is
  signed as the original's, and the look (`LookYaw`) is added; the 30-value rings wait for the mind save. The writers reach the mind
  through `CreatureMindSystemInterface::EmpathiseWithPlayer` / `EmpathiseWithTownDesire` (the original's two player
  functions), not through his `MagicSystem::Empathise`; the villagers' town needs keep our event, passed on by
  `creature_mimic::AddMimicEventHandlers` beside the deeds (R07a).

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

- **Model inverse square root:** our table-and-Newton inverse square root (the old `affine::InverseSquareRoot`) is his
  `gutils::ModelInverseSquareRoot` in `Common/ModelInverseSquareRoot`, his file as he wrote it, since the arithmetic is
  the same (the table with entry 0x40 = 0xFF, the exponent and mask, the order of the Newton step's products); it only
  reads the table with `at` where ours used `[]` on the same masked index. `affine::NormaliseRows` and the other
  callers take it from there. `gutils::InvSqrt`, the distance one, stays separate. He has no test of it; ours, with its
  golden values, stays in `test_damage_mesh_light`.

- **Pure rule files taken whole, ours applies them.** Where his tree has the rules of a mechanism as pure functions
  over a small value type and ours has the same rules over a component, his file is taken as it is and ours keeps its
  own functions as thin adapters: they copy the component's fields into his value type, call his rule, and write back
  what it changed. The call sites and the component stay as they are. First case: the prayer power of a running
  miracle (`Magic/SpellChants`, applied by `Magic/Core/Chants` to the Spell component, the creator behind a
  `SpellCasterInterface` made from the chant context) and of a worship site (`Magic/WorshipBattery`, applied by the
  end of the turn of `Worship/WorshipSite`). The two were shown to be the same rules by reading them side by side,
  and the tests that pinned our numbers now run through his functions. A call site in a file another lane owns
  changes only by the hunk agreed with that lane; the rest of his file stays unused (reached by his tests only)
  until its callers are ported or the owner agrees, and the commit says so. Where his rule differs from ours in a case
  the game does not reach yet, the adapter keeps ours (the spell point of a turn paid without a creator).

- **New-game choice:** his `Story/NewGameChoice.h` (the four answers, `TutorialSkip`, `SkipFor`, `AsksAtNewGame`,
  `ClampChoice` and the developers' `NewGameStart`) is taken as he wrote it, with his tests in
  `test/gui/test_new_game_choice.cpp`. His file is `test/gui/test_skip_box.cpp`, which also holds his SkipBox tests;
  those stay out, as our SkipBox is ours, so our file is named for what it covers and has its own CMake line.

- **Start-up screens:** his launch options `--pre-intro` and `--skip-logos`, as he wrote them, and his
  `Gui/StartupRules` and `Gui/LoadingScreenRules` with his tests. His `startup::Zoomer` is not taken: the logo still's
  fades run on our shared `Zoomer`, the port of the original's. His `Game::PlayStartupScreens` and the
  `Gui/LoadingScreen` it draws with are his names and layout over our parts: the films are read with our own decoder
  (`video::VideoFile`, `video::ReadStill` in `Video/StillPicture`, not his YUV planes and video overlay) through the
  full-screen player's 16-bit copy, and drawn in the player's 256-texel tiles (`video::PictureTiles`) on the
  interface canvas. Only the keys and mouse buttons are taken from the window's queue while they show; every other
  event, a close of the window included, is left for the game.
  His 16-bit copy and widening in `LoadingScreenRules` (`Rgb16`, `ToSixteenBit`, `ToRgba8`) are not kept: the
  blur reads the full-screen player's copy (`graphics::rgb16`), and his test of them is dropped. The loading screen's
  fonts, texts and button atlas come through the resource caches, as the interface's. His `Game::LoadMap(path, look)`
  is taken: a script's land change shows the "please wait" banner, every other load the tips screen.
  Ours (2026-10-15): `--skip-loading-screens` and the debug bar's Debug > Start-up and Loading > Loading screens
  switch (`EngineConfig::drawLoadingScreens`) draw no loading screen: the tip is still picked and the loading time
  still counts, so the lands load the same; the check runs pass it. Beside it, "Logos at start-up" shows whether this
  game played the logos (`--skip-logos`); it is not a switch, as they play only at start-up and no settings file keeps
  it.

- **R21 / 7.7:** the fireball's heat row is GetPowerUpLevel()==0 ? 1 : 0 and the absorb multiplies by 1 + the ball's
  info +0x108, as the original; his .at(0) is not taken.

The decisions taken for each system are on the port-decision pages:

- [Engine and world](port-decisions-engine-world.md#summary): the renderer, physics, particles, the sky, the land and the weather.
- [Villagers, towns and animals](port-decisions-beings.md#summary).
- [The creature](port-decisions-creature.md#summary).
- [Magic](port-decisions-magic.md#summary).
- [Hand, camera and interface](port-decisions-hand-camera-interface.md#summary).
- [Audio, video and scripts](port-decisions-audio-video-scripts.md#summary).

### What stays on purpose

Each of these carries a comment saying why:

- environment variables read once into a `static const` local, and a `std::once_flag` for a log-once warning;
- the physics bodies, an ordered list owned by `DynamicsSystem`: components would change the iteration order the
  simulation depends on;
- the few `new` expressions and C arrays a C API or a private constructor requires.

### Exceptions that remain

Some state stays where it is, each with a comment that says why:

- a lazily read environment variable that never changes, kept as a `static const` local;
- a `std::once_flag` for a warning that is logged once;
- the physics bodies, which stay an ordered list owned by `DynamicsSystem`, because components would change the
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

### Efficiency work parked

From the efficiency audit of the temple interior and the debug windows (2026-10-08). Each would keep the output
identical, but carries a risk or touches shared code for a small gain:

- the temple's light glows: one transient mesh and one draw call per sprite; batching them by texture and blend state
  changes what is dropped if the frame's transient buffer runs out;
- `L3DMeshSubmitDesc::instanceDesc` owns its `InstanceDesc`, so every temple draw allocates one; a non-owning pointer
  is an API change in shared code;
- the temple map's mesh is rebuilt every frame: its cell callback could be a template instead of a `std::function`
  (safe), and rebuilding it only when the land changes needs a land change counter (parked);
- the scroll read in front is written and laid out every frame; caching it needs a key with every fact it shows;
- the uniforms that stay the same for a submesh's primitives are set for each primitive; setting them once relies on
  bgfx keeping uniform values between submits;
- the debug windows, only while open: the Consciences message-set table draws all its rows without a list clipper,
  and the outliner formats every entity's label every 30 frames.

### Progress counts

How far the code base has moved towards the conventions in this page. The counts come from text searches
over `src` (excluding the tests where noted), so treat them as close, not exact; the percentages are derived from the
two counts on each row.

Measured at the commit this branch was published from. "Before" is the commit the refactor started from.

| What | Before | Now | Done | What is left |
|---|---:|---:|---:|---|
| Mutable globals and file- or function-scope statics in game code | 185 | 20 | ~89% | Mostly log-once warning flags inside functions, the `Game` instance pointer, and three file-scope objects; a handful stay on purpose (see below) |
| `Game::Instance()` call sites | 89 | 2 | ~98% | Two in the debug UI: loading a map and reading the turn delta. Both need `Game` split further |
| Locator services | 29 | 81 | — | New services replaced the globals; the number grows as state moves |
| Plain `enum` declarations | 14 files | 0 | 100% | Bit-flag enums became `constexpr` constants with the same type and values |
| `new` / `delete` in game code | 43 | 7 | ~84% | Required by a C API or a private constructor |
| C arrays | 44 | 8 | ~82% | Required by a C API (`argv`, OS buffers, a union member) |
| Direct file reads (`std::ifstream`, `fopen`) | 20 | 9 | ~55% | All of them out of scope: the file system itself, the crash handler, a user save, streaming video and a bank fallback |
| Comments naming the original binary (`fn_…`) | 5354 | 0 | 100% | Across `src`, `components` and the shaders. The detail lives in this wiki |
| Comments quoting the original's addresses | — | 0 | 100% | The 19 hexadecimal values left in comments are data: colours, masks, flags, file-format offsets |
| Identifiers named after the original binary or unclear | 330 to rename | 41 of 49 batches done | ~84% | Seven batches left, listed in order; 161 names that come from upstream openblack are left alone on purpose |
| Files that fail the CI's clang-format 17 check | ~700 | 0 | 100% | One formatting commit, then every later change formatted on the files it touched |
| Test files | 151 | 165 | — | Grouped into 13 executables; 46 tests gained a synthetic twin. 1487 tests pass; 53 skip when the game data they need is not found |

### Known gaps

- `Game` is not fully split: the two remaining `Game::Instance()` call sites, and the map load path, still reach into it.
- Seven rename batches are left; until then some names still mirror the original's rather than describing what they do.
- The shader build does not track `#include`d shader files, so a variant that includes a changed shader is only rebuilt
  from scratch; verification deletes the generated shaders first.
- Land 2 to Land 5 and multiplayer have no automatic behaviour check yet; only Land 1 and a Land 1 → Land 2 → Land 1
  cycle are compared (see [Testing and verifying a change](testing.md#summary)). Since the player's creature is loaded into Land 1, the Land 1
  runs cover it too: its own part of the state hash, its mind's random draws and the hand resting on it.
- The player's creature does not follow its player to the next land yet: ours belongs to the land's registry.

### Still to do

Counts are in [Progress counts](#progress-counts).

- Remove the `Game::Instance()` singleton. The map script globals, the day/night clock and the screen fade are already
  in progress.
- Move the exe addresses that production tables keep as data into the docs. Function names now follow raffclar's
  tree (see [Following raffclar's tree](#following-raffclars-tree)).
- The Locator slots with a counterpart in raffclar's tree now take his names (`time`, `particleSystem`,
  `villageLightSystem`, with their interfaces and implementations). Their shapes follow his in the conformance pass.
- Build a mod SDK once the game is fully rebuilt (see [The minimal mod loader](mod-loader.md#summary)).

### Test hooks

None of its own: the conventions for debug controls and test hooks are in
[Logic only reached by input](testing.md#logic-only-reached-by-input) and [Debugging](debugging.md#where-new-controls-go).

## Pending

- The seven rename batches left, the `Game::Instance()` call sites and the map load path ([Known gaps](#known-gaps)).
- The shader build's tracking of `#include`d shader files.
- An automatic behaviour check for Land 2 to Land 5 and for multiplayer.
- The player's creature following its player to the next land.
