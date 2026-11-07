# The minimal mod loader

## Summary

openblack had a mod loader: Lua and native mod hosts, a mod API, asset replacements, engine switches, mod settings and
a Mods menu, plus a set of mods that shipped with the game. It was a proof of concept, and it has been **removed** from
the code base (user, 2026-10-06): an unfinished mod system spreads hooks, global state and compatibility code through an
engine that is still being written.

What stays is this page, which says how to build a mod system well, and a **minimal mod loader** (user, 2026-10-07):
the seam a real SDK will plug into. The real SDK is written once the game itself is fully reconstructed. Its first step
came on 2026-10-21, by the user's choice: the loader lists the mods of the `Mods` folder from their manifests, and the
menu Escape brings up gains a **Mods** page with a switch per mod, kept between runs. The second came on 2026-10-22:
a switched-on mod's files replace the game's files of the same path (data mods). Code mods (grass the first) come
later, through Lua or another script language, not one native library per mod. Beyond that there are no mod hooks,
switches, settings or compatibility code anywhere in the game.

## The original

The original Black & White has no mod loader; the game's own data packs and how players modified them are in
[Modified data packs](../formats/media-files.md#the-installations-pack). Everything below is openblack's design.

### The minimal mod loader

`ModLoader.dll` is a shared library of its own, built from `src/ModLoader/`, with no scripting dependency and nothing
from the engine. The game is not linked against it. Its C API, in `src/ModSdk/ModLoader.h`, is the SDK's first
SDK-facing header.

```cpp
constexpr int k_ModLoaderVersion = 2;
using ModLoaderLogFn = void (*)(const char* line, void* context);
struct ModLoaderModInfo { const char* folder; const char* name; const char* version; const char* author; int enabled; };
using ModLoaderModFn = void (*)(const ModLoaderModInfo* mod, void* context);
using ModLoaderFolderFn = void (*)(const char* folder, void* context);
extern "C" const char* ModLoader_SdkVersion() noexcept;
extern "C" int ModLoader_Version() noexcept;
extern "C" int ModLoader_LoadMods(const char* modsFolder, ModLoaderLogFn log, void* context) noexcept;
extern "C" int ModLoader_ListMods(const char* modsFolder, ModLoaderModFn each, void* context) noexcept;
extern "C" int ModLoader_SetModEnabled(const char* modsFolder, const char* modFolder, int enabled) noexcept;
extern "C" int ModLoader_EnabledModFolders(const char* modsFolder, ModLoaderFolderFn each, void* context) noexcept;
```

`ModLoader_LoadMods` reads `<modsFolder>/version.txt`, which holds one integer. It calls `log` exactly once and returns
0 when the version matches (the call itself loads nothing: a switched-on mod's files are found by the game's file
system, [below](#data-mods-a-mods-files-replace-the-games)), 1 when the file is missing (or no folder is given), 2 when
it cannot be read or holds no integer and 3 when its version is not the loader's; in every case but 0, no mod is read.
That number (`k_ModLoaderVersion`, from `ModLoader_Version`) is the mods folder's version only; it is 2 since
2026-10-21, when the listing and the switches came. The game and the library agree by the SDK's version instead.

**The SDK's version** (user, 2026-10-21). The SDK never holds back a change to the game's code: it adapts to the code.
Every SDK-facing header lives in `src/ModSdk/`, and a build step (`CMakeModules/GenerateModSdkVersion.cmake`, the
`ModSdkVersion` target) hashes them all, by name and content with their line endings made the same, into
`generated/ModSdk/ModSdkVersion.h` (`k_ModSdkVersion`, a SHA-256 in 64 hexadecimal digits). The game and the library
both compile it, and `ModLoader_SdkVersion` gives the library's. Nobody types the version by hand: any change to a
header of `src/ModSdk/` moves it. At start-up the game uses the library only when the library reports the game's own
version (`mod_sdk::Check` in `src/ModSdkHandshake.h`, a pure rule with its tests): without a library, without the
functions this SDK calls, or with a library built from other SDK headers, the game starts unmodded with one log line
saying why, there is no `Locator::modLoader` service and no Mods page.

How a joint notices that it changed the SDK's surface: its diff touches `src/ModSdk/` (`git diff --stat <base> --
src/ModSdk/`), which moves the version. Such a joint must bring the library up to date in the same series (it is built
with the game, so a joint's own `ModLoader.dll` always matches its `openblack.exe`); a library left from an older build
is then refused at start-up rather than misread. A joint that does not touch `src/ModSdk/` keeps the version.

**The mods.** A mod is a folder of the mods folder holding a `mod.txt`, its manifest:

```text
format: 1
name: Greener Grass
version: 1.2
author: Someone
```

`key: value` lines, whitespace trimmed, blank lines and `#` lines ignored. The first key must be `format`, holding 1
(the manifest's own version, apart from the loader's); `name` is needed, `version` and `author` may be left out; other
keys are ignored, so later formats can add some; a key given twice keeps its first value. A folder without a manifest of
that format is not a mod. `ModLoader_ListMods` gives each mod in the order of its folder's name, only when
`version.txt` matches (it returns -1 otherwise, and the count of mods given when it does).

**The switches.** `enabled.txt`, in the mods folder, lists the folders of the mods switched on, one a line; a mod is off
until it is switched on. `ModLoader_SetModEnabled` switches one, writing the file only when the list changes, and
returns 0 when kept, 1 for no such mod (or a folder not read) and 2 when the file cannot be written. The switches live
with the mods rather than in the player's profile (`Profiles/`): they belong to the folder they name and move with it,
only the library reads or writes them, and the game without the library never sees a mod's file. The manifest and list
rules are pure functions of the text (`src/ModLoader/ModManifest`), unit tested on synthetic texts.

The game has one call site. At start-up it looks for `ModLoader.dll` next to the executable; without it, it logs one
line and carries on exactly as before. With it, it checks that the library has the six functions above and reports the
game's own SDK version, asks it to read the `Mods` folder next to the executable and, when that folder is read, to list
its mods and the folders of the mods switched on, and logs the result in one line
(`ModLoader <version>: <its line>; <n> mods`). It keeps the library loaded for as long as the service lives
(`Locator::modLoader`), for the switches. The debug menu shows a **Mods** entry, with the loader's version, its line and
the mods (each with its folder and whether it is on).

**The Mods page** (the user's choice of 2026-10-21). When the library is there and the mods folder is read, the
options' box of the menu Escape brings up has a sixth tab, **Mods**, after Controls; the six tabs share the box's width,
where the original has five. The page has a line at the top (the loader's version, the number of mods and that a switch
counts from the next start), a row a mod (its switch, then its name, version and author), at most eight rows and a
count of the rest, and the Back arrow. A switch is kept through the library; when the library cannot keep it, it goes
back. The start-up menu has no Mods page. The checks run with an empty `Mods` folder, which is not read, so they never
show the page.

### Rules for the SDK

The design approved for the future SDK (2026-10-06). It is built behind a `ModsInterface` and across a separate mod
library, once the game is reconstructed.

#### A versioned handshake, before anything else

The library and the exe agree on the SDK's version (the hash of `src/ModSdk/`, through `ModLoader_SdkVersion`) before
a single mod is read, and the mods folder on its own (`version.txt`, `ModLoader_Version`). The C API across the seam is versioned per interface, so that an old library and a new exe fail
cleanly with a message instead of crashing.

#### One service, by interface

Everything the mod system owns (the mod list, load order, dependencies, the published interfaces, the log, the restart
request) lives in one Locator service (a `ModsInterface` and its implementation), as members. No file-scope statics, no
singletons. The engine and the debug UI reach mods only through the interface.

#### Hosts behind an interface

A `HostInterface` with `Start`, `Stop` and `Running`, and one implementation per language, each holding its state as
members. The service creates the hosts in a fixed order (Lua first, then native, the order in which the game used to
call them), and a test with a mock host checks that order, because the order in which hosts see an event is part of
the game's behaviour. The project's rules now take code mods through Lua or another script language, not one native
library per mod.

#### Game events, not calls inside the loops

The engine publishes `TurnEnded`, `FrameEnded` and `LandLoaded` as plain event structs through the Locator's event
manager (as `src/ECS/Events` types), and the hosts subscribe. The game loop never names a mod host. The event manager
calls handlers synchronously, so the order of work inside a turn does not change, and it is not reentrant: a handler
must not publish the event it is handling (`EventManager::Produce` is not reentrant). A debug assert enforces that.

#### Mod assets through the resource loaders

An asset replacement is a lookup that the resource loaders ask, not a parallel table consulted from the engine's load
code. A replaced file is then loaded by the same loader as the original. With no mods installed there are no
replacements, so the order and number of file reads must be unchanged: compare the load log before and after.

#### Settings as value types

A mod's state is a value type with pure `Parse` and `Format` functions, unit tested. The engine switches are a value
table owned by the service. Nothing reads a settings file from inside game code.

#### Test hooks are not part of the mod system

No test-only environment variable or hook belongs in the mod code. Test hooks live on the debug side, in the debug hooks
service, and drive a mod through the service's interface exactly as the UI does.

#### Built-in mods are mods

Anything that ships with the game, such as graphics tweaks or extra foliage, is a built-in mod that reads its options
through the interface and gives its overrides to the resource loaders. It does not keep state inside the engine's
subsystems. The built-in mods count as "the game without mods" when the game is verified.

#### A small API that only exposes interfaces

The mod API exposes Locator interfaces (camera, terrain, clock, magic, audio) and nothing else, with one `constexpr`
version that the hosts check, instead of version numbers repeated in each language binding.

#### Tests with mocks

The mod system's own tests build their own instance, with mocks of the host and of the engine side, and never touch the
game's locator. Tests that need a scripting runtime are integration tests and skip themselves when it is absent.

### Verifying a mod system against the game

A mod system must not change the game when no mod is installed. Verify with an empty mod folder, as every other change
is verified (see [Testing and verifying a change](testing.md#behaviour-is-identical-the-fidelity-run)): the per-turn
state hash and the random trace over Land 1 and a land cycle, the land view by day and by night, the intro, the
scripted hand demos and the tests in both modes. No check runs with a mod switched on.

### Later

- A stable C ABI per interface, with its own header package.
- The scripting API generated from the C++ interfaces instead of written by hand.
- The Mods window as an ECS/ImGui system a mod can extend.
- More events, as event types, when a mod needs them.
- A manifest schema that drops the legacy data, plus hot reload, signing and a repository.
- Mod ideas only, no work planned (user, 2026-10-07): **large maps**, for example 128×128 blocks. Today's island-wide
  block texture would not fit: the GPU's texture size limit of 16384 allows about 64×64 blocks, and 128×128 blocks would
  need about 4 GB. It would take per-block textures (a texture array), plus streaming and level of detail for the
  distant blocks.

## openblack

**Done.** The removal took out the Lua and native hosts, the replacements, the switches (`EngineSwitches`), the mod
API and its header, the mod registry (`ModRegistry`), the manifests (`BuiltinManifests`), settings, `ModLog` and
`RuleFiles`, the Mods window (`ModsWindow`) and menu item, the mod calls in `Game.cpp`, the
`--mod` / `--msaa` / `--mipmaps` / `--anisotropic` / `--enhanced-graphics` options, `lua` and `sol2`, the built-in
mods (each engine switch kept only its default, no-mod branch) and their tests. The one built-in mod that was on by
default, the intro skip, became nothing new: test runs answer the start-up requester with the existing
`OPENBLACK_TEST_SKIP_ANSWER=3,30` ("keep my creature"), and without it the requester is drawn, as in the original.

- `src/ModSdk/ModLoader.h`: the C API, the SDK's surface; `src/ModLoader/ModLoader.cpp`: the library;
  `src/ModLoader/ModManifest.{h,cpp}`: the manifest's and the switches' rules.
- `CMakeModules/GenerateModSdkVersion.cmake` (the `ModSdkVersion` target of `src/CMakeLists.txt`): the SDK's version,
  generated at every build and written only when it moves; `src/ModSdkHandshake.h`: the game's rule for it.
- `src/Game.cpp`, `LoadModLoader`: the one call site, at start-up; it loads `ModLoader.dll` (`.dylib`, `.so`), checks
  the SDK version and the six symbols, keeps the library's line, its mods and the switched-on mods' folders, and keeps
  the library loaded for the switches (released with the service).
- `src/ModLoaderStatus.h` and `Locator::modLoader`: the version, the line, whether the folder is read, the mods, the
  switched-on mods' folders (`modRoots`) and the switch, emplaced only when the library loaded; the debug menu's
  **Mods** entry (`src/Debug/Gui.cpp`) reads it.
- `src/Gui/GameMenu.{h,cpp}` (`MenuMods`, `Page::Mods`, `BuildMods`, `MakeOptionsPage`) and
  `src/Gui/GameInterface.cpp` (`MenuModsFromLoader`): the Mods page, given to the menu only with a read mods folder.
- `test/test_mod_loader.cpp`, `test/test_mod_manifest.cpp` and `test/test_mod_sdk_handshake.cpp` (the standalone
  `test_mod_loader`): the version file's cases, the listing and the switches on a mods folder of their own, the
  library's SDK version, the manifest's rules, and the handshake (the same version activates, another or a missing
  library starts unmodded);
  `test/gui/test_interface.cpp`: the five tabs without mods, the sixth with them and its switches.
- The audio state's mutex is not a mod leftover: it is there for the music thread, which reaches the audio state.

### Data mods: a mod's files replace the game's

The first kind of mod (user, 2026-10-22): a switched-on mod's folder stands for the game's folder. A file the game
reads through its file system (`FindPath`, so every resource loader) is looked for first below each switched-on mod's
folder, by the same path below the game's folder, then in the game's own places as before. The mods are looked in by
their folders' names, in order (the loader reads `enabled.txt` as a sorted list and writes it so); the first mod
holding the file wins. A file that is written always goes to the game's own place, never into a mod, and the player's
own files (the saves, the profiles, the machine setup, an input recording) are read through `ReadUserFile`, which never
looks in a mod. The switches count from the next start.

- `ModLoader_EnabledModFolders` (`src/ModSdk/ModLoader.h`): the switched-on mods' folder names, each a folder with a
  manifest; a mods folder that is not read gives none.
- `src/FileSystem/ModSearchRoots.h`: the pure rules (a path below the game's folder, the first root holding it);
  `FileSystemInterface::SetModRoots`, set once in `Game::Initialize` from `Locator::modLoader`'s `modRoots`; without
  the library or with no mod on there are no roots and the lookup is unchanged. Each file a mod gives is one log line
  (`<path> from a mod: <file>`), and each mod folder in use one more at start-up.
- Tests (in the standalone `test_mod_loader`): `test/test_mod_search_roots.cpp` (a mod's file found, a missing one
  falling back, two mods in order, no roots, only paths of the game's folder replaced),
  `TheEnabledModsFoldersAreGivenInTheListsOrder` in `test/test_mod_loader.cpp`.

How to make a data mod: a folder in `Mods` (next to the game's executable, with `version.txt` holding 2), with a
`mod.txt`, and the replaced files below it by the game's own paths, for example `Mods/MyMeshes/Data/AllMeshes.g3d`.
Switch it on in the options' Mods tab and start the game again. A data mod can change play as well as the look: the game
reads a replaced file as its own, and a mesh pack with bigger scenery meshes refuses more of the land script's trees
(with the user's pack, 25 of Land 1's trees are never made; [Trees and forests](../world/trees.md#general)).

### Test hooks

None: by the rule above, the mod loader has no test hooks of its own. Check runs use an empty `Mods` folder. The debug
menu's **Mods** entry shows what the loader found.

## Pending

- The page seen in a run: a build with the library and a sample mods folder (`version.txt` holding 2, a mod with its
  `mod.txt`), the page opened, a switch made and `enabled.txt` read back.
- What else a switched-on mod can do (code mods, grass the first, through a script language: the systems and draw-pass
  registries, its events), with the rest of the real SDK; data mods replace files since 2026-10-22.
