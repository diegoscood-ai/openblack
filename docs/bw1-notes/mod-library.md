# Mod library

Everything that changes the original game is a **mod**, disabled by default (the only exception, requested by the user,
is [`game.skip-intro`](#gameskip-intro)). A mod is a **folder** in `Mods/` with a **`mod.json`**: what the mod
is (name, version, category, image…), its options and what it changes. It may have no code (data only), have a
**Lua** script or a **native** library (DLL / .so, in C). Mods can be **libraries** for other mods and are
grouped into **modpacks**. The library (`src/Mods/`) discovers them on its own at startup, resolves dependencies and
load order, draws the **Mods** window and stores the state of each one in its `settings.cfg`. Everything on this page is
**mod/own** unless stated otherwise (**faithful**, **(approximate)**).

- [For the player](#for-the-player)
  - [Mods window](#mods-window)
  - [Mods folder](#mods-folder)
  - [Command line](#command-line)
- [For making mods](#for-making-mods)
  - [Structure of a mod](#structure-of-a-mod)
  - [mod.json](#modjson)
  - [Options and switches](#options-and-switches)
  - [Replacing meshes, textures, objects and files](#replacing-meshes-textures-objects-and-files)
  - [Lua mods](#lua-mods)
  - [Native mods (DLL)](#native-mods-dll)
  - [Library mods](#library-mods)
  - [Modpacks](#modpacks)
  - [Dependencies and load order](#dependencies-and-load-order)
  - [Old folders (mod.cfg)](#old-folders-modcfg)
- [Reference](#reference)
  - [Engine switches](#engine-switches)
  - [Enumerations](#enumerations)
  - [API: JSON, Lua and C](#api-json-lua-and-c)
- [How it is built (src/Mods)](#how-it-is-built-srcmods)
- [Mod catalogue](#mod-catalogue)
  - [graphics.msaa](#graphicsmsaa)
  - [graphics.mipmaps](#graphicsmipmaps)
  - [graphics.anisotropic](#graphicsanisotropic)
  - [graphics.terrain-x2](#graphicsterrain-x2)
  - [graphics.smooth-smoke](#graphicssmooth-smoke)
  - [graphics.hd-tweaks](#graphicshd-tweaks)
  - [water.living](#waterliving)
  - [world.ground-statics](#worldground-statics)
  - [world.crops](#worldcrops)
  - [world.foliage](#worldfoliage)
  - [Module world.foliage.beach](#module-worldfoliagebeach)
  - [Module world.foliage.butterflies](#module-worldfoliagebutterflies)
  - [test.miracle-dispensers](#testmiracle-dispensers)
  - [game.skip-intro](#gameskip-intro)
  - [Modpack examples](#modpack-examples)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## For the player

### Mods window

The **Mods** button in the menu bar opens the mods window (`src/Debug/ModsWindow.*`), Project Zomboid style, with
four tabs:

- **Modpacks**: each pack with its image, name, version, author and description, and a checkbox to switch all its mods
  on or off. Clicking a pack goes to the filtered Mods tab, «Mods (<pack name>)».
- **Mods**: on the left the list (image, checkbox, name; `*` = needs a restart; blocked ones in red),
  grouped by category, with the modules indented under their mod and a search box. Without a filter the loose mods are
  shown; with a filter, those of the pack (and «< All loose mods» to go back). On the right, the chosen mod: large image,
  name, id, version, authors, category, pack, state (active / off / **blocked: why** / waiting for its parent),
  description, **settings**, what it needs (with its state), what it offers to other mods and its folder. Below, the
  folders in `Mods/` that could not be read, with the error.
- **Load order**: the resolved load order (top to bottom; with two mods changing the same thing the lower one wins), with
  the state and what each one depends on, and arrows to move a mod up or down. Dependencies always take precedence. It is
  stored in `Mods/load_order.cfg`.
- **Restart**: when switching on, switching off or changing a mod that needs a restart (`*`), the window asks «Restart
  needed» with **Restart openblack now** / **Later**, and while any remains pending it shows «Takes effect
  after a restart: …» at the top with the same button. Restarting closes openblack like «Quit» and reopens it with the
  same command line (`Mods/Restart.*`, `main.cpp`); the window is in English.
- **Log**: the messages of the mod library (mods found, manifest errors, blocks, replacement conflicts, Lua and
  DLL errors, and what the mods write), with a filter by level and by mod.

### Mods folder

Everything is in `Mods/` next to the executable:

```
Mods/
  graphics.msaa/settings.cfg              un mod que viene con openblack (su mod.json va dentro del exe)
  world.foliage/foliage.json, *.png ...     sus archivos, y su settings.cfg
  mi.mod/mod.json, icon.png, ...           un mod suelto
  mi.pack/modpack.json, icon.png           un modpack...
  mi.pack/mi.pack.uno/mod.json             ...con sus mods dentro
  load_order.cfg                           el orden de carga del usuario
```

- Each mod stores its state in its `settings.cfg` (openblack writes it at startup if missing and when it is changed in
  the window; a `settings.cfg` that already exists **takes precedence** over the default values of the `mod.json`):
  ```
  # Anti-aliasing (MSAA) (graphics.msaa). For one session only: --mod graphics.msaa[=off], --mod graphics.msaa.<option>=<choice>
  enabled = on
  samples = 4x  # Samples: 2x, 4x, 8x, 16x
  ```
- The mods that come with openblack have their `mod.json` compiled into the exe (`assets/mods/<id>/mod.json`), so
  they exist even if their folder only has the `settings.cfg`; a `mod.json` in the folder with the same id replaces it.
  The build copies `assets/mods` and `mods/examples` next to the exe (`bin/<config>/Mods`), without touching the
  `settings.cfg` files.
- The old single `mods.cfg` is split automatically into the `settings.cfg` files and deleted
  (`ModRegistry::ImportLegacySettings`).

### Command line

- For that session only (not saved): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`, `--mod "game.skip-intro.free start=off"`.
- Old shortcuts (`src/main.cpp`): `--msaa N`, `--mipmaps`, `--anisotropic`, `--enhanced-graphics` (= MSAA 4× +
  anisotropic), `--living-water`, `--ground-static-objects`.

## For making mods

The quick way: copy a folder from the [modpack examples](#modpack-examples) (`Mods/examples/`), change its id and
name, and edit. There is one of each type.

### Structure of a mod

```
Mods/<id>/                  la carpeta se llama como el id (minúsculas, cifras, . - _; 2-64 caracteres)
  mod.json                  el manifiesto (obligatorio)
  icon.png                  su imagen (cuadrada, 64-256 px; opcional)
  settings.cfg              lo escribe openblack
  replace/                  archivos del juego que sustituye, con su misma ruta (Data/..., Scripts/...)
  textures/  meshes/  data/ sus archivos, nombrados desde el mod.json
  scripts/main.lua          su script (mods Lua); scripts/<módulo>.lua con require("<módulo>")
  src/  bin/<id>.dll        su código y su librería (mods nativos)
  include/<id>_v1.h         la cabecera pública (solo los mods librería nativos)
```

### mod.json

| Field | Type | What it is |
|---|---|---|
| `schema` | number | format version (1) |
| `id` | text | stable identifier, same as the folder (`world.foliage`) |
| `name`, `description` | text or `{"en": …, "es": …}` | name and description (per language) |
| `version` | text | semver version, `"1.2.0"` |
| `category` | text | section of the window (`Graphics`, `World`, `Game`…) |
| `authors` | list of texts | |
| `icon` | path | its image (by default `icon.png` if it exists) |
| `url` | text | the mod's page (optional) |
| `api` | range | version of the mod API it was made for, `">=1.0 <2.0"` (today openblack offers 1.2.0: 1.1 adds geometry and the game clock, 1.2 sound) |
| `enabled_by_default` | yes/no | switched on the first time (only if the user asks for it; the normal is `false`) |
| `restart_required` | yes/no | its changes take effect on restart |
| `parent` | id | module of another mod: shown underneath and only counts if the parent is active |
| `dependencies` / `optional` / `incompatible` | `{"id": "rango"}` | see [Dependencies](#dependencies-and-load-order) |
| `load_after` / `load_before` | list of ids | ordering hints (the other one does not have to exist) |
| `provides` | list | interfaces it offers to other mods (`"foliage.v1"`) |
| `entry` | `{"lua": ruta, "native": {"windows": ruta, "linux": ruta}}` | its code |
| `switches` | `{"interruptor": valor}` | switches it sets while active |
| `options` | list | its settings ([Options](#options-and-switches)) |
| `replace` | object | what it replaces ([Replace](#replacing-meshes-textures-objects-and-files)) |

`//` comments are allowed in the JSON. A format error leaves the mod out (it appears in the Mods tab, «Could not be
read», and in the Log); an unknown switch or value only removes that part, with a warning.

### Options and switches

The engine never decides on its own: everything that is not original reads a **switch** from `EngineConfig`, and the
mods set switches by name ([list](#engine-switches)). A `mod.json` needs no code for that:

```json
"switches": { "world.crops.without-farmers": true },
"options": [
  { "id": "density", "label": {"en": "Density", "es": "Densidad"},
    "values": ["low", "medium", "high"], "default": "medium",
    "bind": { "world.foliage.density": { "low": 0.5, "medium": 1.0, "high": 2.0 } } },
  { "id": "speed", "type": "slider", "values": ["x1", "x2", "x10"], "default": "x1",
    "bind": "world.crops.growth" },
  { "id": "sharp", "type": "bool", "default": true,
    "bind": { "graphics.hd-tweaks.mip-bias": -1.0 } }
]
```

- `type`: `choice` (list, the default), `slider` (slider over the values) or `bool` (values `on` / `off`).
- `values` and `default` (the value or its ordinal number); `label` and `description` per language.
- `bind`: for each switch, the value of each choice (those not listed keep the default value); or just a switch
  name: in `bool` it is 1 with `on`, and in the others the number in the choice's text
  (`"x10"` → 10, `"4x"` → 4, `"10s"` → 10).
- With the mod off or blocked its switches return to the default value (the original's). With two mods
  setting the same one, the one later in the load order wins.
- Each switch says when it takes effect (`live` immediately, `map` when loading a land, `restart` on restart): if the
  mod has any restart one, set `"restart_required": true`.

### Replacing meshes, textures, objects and files

```json
"replace": {
  "meshes":   { "AnimalBat1": "meshes/bat.l3d", "#12": "meshes/otro.l3d" },
  "textures": { "pack:47": "textures/47.png", "raw:ATMOS": "textures/atmos.png" },
  "objects":  { "tree": { "Beech": { "woodValue": 500, "normal": "TreeBeech" } },
                "feature": { "Rock1": { "weight": 50 } } }
}
```

- **meshes**: a mesh of `AllMeshes.g3d` by its name (the [`meshes` enumeration](#enumerations), case-insensitive)
  or `#<número>`, replaced by a `.l3d` (or `.zzz`) of the mod. It is loaded instead of the pack's one (the resource cache
  stores the first load, `Game::Initialize`) by the same `L3DLoader`, so it inherits what the engine
  applies afterwards to that mesh by its id: e.g. the bubble (`O_Bibble_up`) and the power-up bands
  (`Power_Up_Band`) keep the original's additive material without Z, and the render modes of `render_modes` (note
  from the sistemas session). A mod that wants another material for those will have to request it when the SDK offers it.
- **textures**: `pack:<id hex>` a texture of `AllMeshes.g3d` (the HD-Tweaks ids, `textures.json`) by a PNG;
  `raw:<nombre>` a `Data/Textures/<nombre>.raw` by a PNG or a `.raw` (if the game does not have it, it is added).
- **objects**: properties of the `info.dat` objects per table and by their debug name (`debugString`; in
  `abode` also `<TRIBU>_<nombre>`, like the scripts, `GAbodeInfo::GetInfoFromText` 0x405A70: the name alone changes
  the building of all tribes): tables
  `feature`, `abode`, `mobileStatic`, `mobileObject`, `pot`, `tree`, `animatedStatic`, `animal`, `bigForest`,
  `fieldType`; common fields (`foodValue`, `woodValue`, `weight`, `heatCapacity`, `combustionTemperature`,
  `sacrificeValue`, `impressiveValue`, `drawImportance`, the `defenceEffect*` / `defenceMultiplier*`, the
  `canCreature*`…) and mesh or scale fields where the table has them (`meshId`, `normal`, `growing`, `burning`, `high`,
  `std`, `low`, `startScale`, `finalScale`; a mesh by name or number). Also `"objects": "data/objects.json"`
  with the same in a file. It is applied to `info.dat` before publishing it (`Game.cpp`, after `InfoFile::LoadFromFile`).
- **replace/**: any game file with its same path inside the mod's `replace/` folder (`replace/Data/
  Sky.raw`, `replace/Scripts/Land1.txt`…) replaces it; those that only the mod has are also visible
  (`FileSystemInterface::AddOverridePath`, in load order: the last one wins).
- All this is read **at startup** (`mods::replace::Collect`): mods with `replace` must carry
  `"restart_required": true`. If two mods replace the same thing, the last one wins and the Log says so.

### Lua mods

`"entry": {"lua": "scripts/main.lua"}`. The script runs once when the engine starts (before the first land), in
an **environment of its own** per mod: without `io`, `os` (except `os.time`, `os.clock`, `os.date`), `package`, `debug`,
`load` or `dofile`; `string.dump`; the `string`, `table`, `math`, `utf8` and `coroutine` libraries are each mod's own
copies; `require("a.b")` loads `scripts/a/b.lua` of the same mod (no paths, drives or `..`); `print` writes to the Log;
only source code is executed, never precompiled Lua). A script error is recorded in the Log and never stops the game;
after 10 errors its event functions are removed, and a call that exceeds about 20 million instructions is
cut off (host rules, not from the original). A mod that is off or blocked receives no events, and since the script is
loaded at startup, a mod with `entry` or `replace` always requires a restart. Do not change the string metatable
(`getmetatable("")`): it is the only table shared by all mods. Table `ob`:

| Function | What it does |
|---|---|
| `ob.log.info(t)`, `.warn(t)`, `.error(t)` | writes to the Log |
| `ob.mod.id`, `.name`, `.version`, `.folder`, `ob.mod.option(id)` | the mod and the choice of an option |
| `ob.switch.get(nombre)`, `ob.switch.set(nombre, valor)`, `ob.switch.list()` | switches (those set by a script count while the mod is active) |
| `ob.on("turn" \| "frame" \| "land_loaded", función)` | events: the turn number, the seconds of the frame, the name of the land |
| `ob.interfaces.provide(nombre, tabla)`, `ob.interfaces.get(nombre)` | [library mods](#library-mods) |
| `ob.enums.meshes`, `ob.enums.magic`…, `ob.enum(nombre)` | [enumerations](#enumerations) as name → number tables |
| `ob.game.turn()`, `ob.game.hour()` | turn and time of the land's clock |
| `ob.game.ground_height(x, z)` | terrain height (nil without a land) |
| `ob.game.camera()`, `ob.game.set_camera(x, y, z, fx, fy, fz)` | the camera (position and focus) |
| `ob.game.cast_miracle(nombre, x, z [, radio, segundos])` | a miracle on the ground, from the neutral player, via the `SPELL_AT_POS` path |
| `ob.game.turn_fraction()`, `ob.game.paused()`, `ob.game.speed()` | (1.1) turn fraction 0..0.99, pause and speed (`game_clock`) |
| `ob.map.distance(x1, z1, x2, z2)` | (1.1) distance on the ground as the game measures it (`GUtils::GetDistanceInMetres` 0x74CD70, the one the game uses most: only x and z, in 16.16 fixed point) |
| `ob.map.angle(x1, z1, x2, z2)`, `ob.map.angle_to_radians(a)`, `ob.map.radians_to_angle(r)` | (1.1) game angles: 0..2047 is one revolution (`gutils::GetAngleFromXZ`, `ConvertGameAngleTo3D`, `ConvertAngle3DToGame`) |
| `ob.map.point_at(x, z, ángulo, metros)` | (1.1) the point at that distance and angle (`gutils::GetXFromAngle` / `GetZFromAngle`) |
| `ob.map.cell(x, z)` | (1.1) the 10 m cell and whether it is inside the 512 x 512 map (`map_coords::CellOf`, `InBounds`) |
| `ob.mesh.radius(nombre [, escala])`, `ob.mesh.height(nombre [, escala])` | (1.1) 2D radius and integer height of a mesh (`object::MeshRadius2D`, `MeshHeight`); nil if it is not loaded |
| `ob.sound.play(banco, muestra [, x, y, z])` | (1.2) a sound effect like those of the game: bank by name (`ob.enums.sound_banks`: `InGame`, `Spells`, `Creature`, `ScriptSfx` (the scripts' one)…), sample by its name in the `.sad` (`"G_PickUpFood.wav"`) or its number; without a position 2D, with one 3D, fixed at that point. Mode 3 without loop (that of a standalone effect in the original, `LH_SamplePlayOptions` 0x10010E90) and the sample's own volume, pitch and distances. Returns whether it played |
| `ob.sound.stop()` | (1.2) stops all of the mod's sounds (`audio::StopOwner`, with the 20 ms ramp) |

### Native mods (DLL)

`"entry": {"native": {"windows": "bin/<id>.dll", "linux": "bin/<id>.so"}}`. A library in C (or in any language
that makes a C library) that includes **a single header**, `components/modsdk/include/openblack/mod_api.h`, and does not
link anything from openblack: the engine passes it its functions when loading it (`SDL_LoadObject`). It exports:

```c
OB_MOD_EXPORT const ob_mod_info* ob_mod_query(void);           // versión de API e id, sin efectos
OB_MOD_EXPORT int32_t ob_mod_load(const ob_host_api* host, ob_mod* self);  // 0 = bien
OB_MOD_EXPORT void ob_mod_unload(void);                         // opcional
```

- openblack checks `ob_mod_query` (same API major version, same id as its `mod.json`) before executing anything
  else from the library.
- `ob_host_api`: `log`, `get_option`, `set_switch`, `get_switch`, `on_event` (`OB_EVENT_TURN`, `_FRAME`,
  `_LAND_LOADED`), `provide_interface`, `get_interface`, `enumeration`, `game_turn`, `game_hour`, `ground_height`,
  `camera`, `set_camera`, `cast_miracle`, `land_name`; and since 1.1 `game_turn_fraction`, `game_paused`,
  `game_speed`, `map_cell`, `map_distance`, `map_angle`, `map_angle_to_radians`, `map_radians_to_angle`,
  `map_point_at`, `mesh_radius`, `mesh_height`; since 1.2 `play_sound` and `stop_sounds` (those of the Lua table). It starts with its size: new functions
  are only added at the end (`OB_HOST_HAS(host, función)` to know whether the running openblack has it; that is how
  `example.native-hello` does it with `map_distance`).
- Rules: everything on the game thread; no C++ exception leaves the library; the strings openblack gives are valid
  during the call, those requested from it go to a buffer of the mod. A native mod cannot be sandboxed like a Lua one:
  only install trusted ones.
- Building one in the repo: `openblack_add_native_mod(<target> <id> <carpeta> <fuentes>)` in `mods/CMakeLists.txt`
  (it puts it in `Mods/<carpeta>/bin`).

### Library mods

A mod that changes nothing by itself and offers functions to others, like Minecraft's mod libraries:

- It declares it in `"provides": ["<nombre>.v1"]` and publishes it when loading: in Lua `ob.interfaces.provide("x.v1", tabla)`;
  in C `host->provide_interface(self, "x.v1", &tabla, sizeof tabla)` with a table of function pointers that starts
  with its size (and a public header `include/x_v1.h` for whoever uses it).
- Whoever uses it puts it in `"dependencies"` (so it loads afterwards and is blocked if missing) and requests it:
  `ob.interfaces.get("x.v1")` / `host->get_interface("x.v1", sizeof(x_v1))`.
- An interface only grows at the end; an incompatible change is another name (`x.v2`). Lua tables are for Lua mods
  and native ones for native mods.
- Examples: `example.lua-library` + `example.lua-consumer`, `example.native-library` + `example.native-consumer`.

### Modpacks

A folder in `Mods/` with a **`modpack.json`** (`schema`, `id`, `name`, `version`, `category`, `description`,
`authors`, `icon`, like a `mod.json`) and its mods inside, each in its subfolder with its `mod.json`. Mod ids are
global. The pack's checkbox switches all its mods on or off; each one is configured separately. Example:
[examples](#modpack-examples).

### Dependencies and load order

- `"dependencies": {"lib.x": "^1.2"}`: required, switched on and within that range; otherwise, this mod is **blocked**
  (shown in red with the reason: «needs lib.x ^1.2, found 1.0.0», «needs X, which is off»…), and so is whatever depends
  on it.
- `"optional"`: if present, it loads first; if not, nothing. `"incompatible"`: this mod is blocked while the other one is
  active.
- `"api"` outside openblack's range → blocked.
- Ranges: `*`, `1.2.3` / `=1.2.3`, `>`, `>=`, `<`, `<=`, `^1.2` (same major, at least 1.2), `~1.2` (same major and
  minor), several separated by spaces (`">=1.0 <2.0"`).
- Order: first what each mod needs (dependencies, present optionals, `load_after`, `load_before`, the parent),
  then the user's order (`Mods/load_order.cfg`) and then the id. A dependency cycle loads by id and the Log
  says so.

### Old folders (mod.cfg)

They are still read, translated to the new format:

- A folder with `mod.cfg` without `module_of` is a **data mod** `data.<carpeta>`: its files with the game's path
  replace it (like `replace/` of a `mod.json`), with restart.
- With `module_of = <id>` it is a **module** of that mod: `name`, `description` and options
  `option.<id> = <etiqueta> | <opción>, <opción>... | <por defecto> [| slider]`.
- A `mod.json` in the folder takes precedence over its `mod.cfg`.

## Reference

### Engine switches

The table is in `src/Mods/EngineSwitches.cpp` (each one a field of `EngineConfig`; the fields and who reads them do not
change). Default value = the original.

| Switch | Type | When | What it does |
|---|---|---|---|
| `graphics.msaa.samples` | int 0-16 | live | MSAA of the buffer (0 = the original); when changed the buffer is rebuilt |
| `graphics.mipmaps` | bool | restart | mipmaps and trilinear filtering |
| `graphics.anisotropic` | bool | restart | anisotropic filtering |
| `graphics.smooth-smoke` | bool | restart | `smokea.raw` with its 8-bit alpha (without the original's ARGB4444 cut) |
| `graphics.terrain.upscale` | bool | map | terrain textures upscaled x2 (Lanczos-3) |
| `graphics.terrain.repeat` | float 1-4 | map | repetitions of the terrain texture per block |
| `graphics.terrain.triplanar` | bool | map | cliffs with the side texture |
| `graphics.hd-tweaks.textures` | bool | live | HD textures for villagers and animals |
| `graphics.hd-tweaks.smooth` | int 0-3 | live | PN rounding level (0 = no) |
| `graphics.hd-tweaks.lighting` | int 0-1 | live | 1 = per-pixel lighting |
| `graphics.hd-tweaks.mip-bias` | float -4-0 | live | mip bias (negative = sharper) |
| `graphics.hd-tweaks.high-detail` | bool | live | high-detail meshes |
| `water.living` | bool | live | the sea reflects everything and ripples |
| `world.ground-statics` | bool | live | lowers floating statics to the ground (also moves those that already exist) |
| `world.foliage.density` | float 0-8 | live | plants per cell (0 = no grass) |
| `world.foliage.distance` | float 50-1000 | live | grass draw distance |
| `world.foliage.fields` | bool | live | fields as growing plants |
| `world.crops.without-farmers` | bool | live | fields that sow themselves |
| `world.crops.growth` | float 1-100 | live | growth speed |
| `game.skip-tutorial` | int 0-3 | restart | answer to the SkipBox (0 play everything … 3 without the glade) |
| `game.free-start` | bool | map | **not original**: the start of the land does not move the camera nor lock |
| `test.dispensers` | bool | map | test dispensers next to the temple |
| `test.dispensers.level` | int 0-3 | map | their level |
| `test.dispensers.seconds` | float 1-600 | live | their recharge |
| `test.dispensers.seed` | bool | live | fireball in the hand at the start |

Adding one: the field in `EngineConfig` (off = the original), read it in the engine and a line in `EngineSwitches.cpp`.

### Enumerations

`ob.enums.<nombre>` (Lua) and `host->enumeration("<nombre>", i, …)` (C): `meshes` (the 626 names of `k_MeshNames`),
`magic` (the `MagicType` by the name of their effect in `info.dat`, after loading the data), `object_tables` and
`object_fields` (what `replace.objects` accepts), `switches`.

### API: JSON, Lua and C

A single implementation, `src/Mods/Api.h`; JSON, Lua (`Mods/Lua/LuaHost.cpp`) and C (`Mods/Native/NativeHost.cpp`) are
translations of it. It only uses the public API of each area, agreed with its owner: height `LandIsland`, miracles
`magic::script::CastSpellAtPos` (by the game's rules, like `SPELL_AT_POS`, with the class check; the
«from» 30 m above the point, like `OPENBLACK_TEST_SPELL` **(inferred)**), camera, clock and switches. Pending:
sound goes only through `src/Audio/Audio.h` (agreed with the audio session): each mod has its owner (`audio::NewOwner`)
and its sounds are stopped together (`audio::StopOwner`) when it is switched off, blocked or openblack is closed.

## How it is built (src/Mods)

| File | What |
|---|---|
| `Mod.h` | `Mod` (Info, options, state, block), `Modpack`, `Dependency` |
| `Manifest.*` | reads `mod.json` / `modpack.json` (nlohmann-json); `PackageMod`: options bound to switches |
| `Semver.*` | versions and ranges |
| `Switches.*`, `EngineSwitches.cpp` | registry of named switches and the `EngineConfig` table |
| `ModRegistry.*` | discovering folders, settings, dependencies, load order, applying, modpacks, mounting `replace/` |
| `BuiltinManifests.h` | the `mod.json` of `assets/mods` compiled into the exe (generated by `src/CMakeLists.txt`) |
| `Replacements.*` | `replace`: meshes, textures, `info.dat` objects, `replace/` folders |
| `Api.*` | the simplified functions |
| `Lua/LuaHost.*` | Lua mods (Lua 5.4 + sol2) |
| `Native/NativeHost.*` | native mods; the C header in `components/modsdk/include/openblack/mod_api.h` |
| `ModLog.*` | the messages of the Log tab |
| `Debug/ModsWindow.*` | the Mods window |

Startup (`Game::Game`): `switches::RegisterEngineSwitches` → `ModRegistry::Discover(<exe>/Mods)` → legacy →
`LoadSettings` → `--mod` → `ApplyAll` (resolve, switches, `Apply`) → `replace::Collect`. `Game::Initialize`
mounts `replace/` and the data mods, and loads meshes, textures and `info.dat` with the replacements. `Game::Run` starts
Lua and the native mods before the first land; `land_loaded` at the end of `LoadMap`, `turn` at the end of each turn,
`frame` in each `Update`. Tests: `test/test_mods.cpp`.

## Mod catalogue

Those that come with openblack (`assets/mods/<id>/mod.json`, without code of their own: only options bound to
[switches](#engine-switches); they used to be C++ classes in `src/Mods/Builtin/`, with the same ids, options and
values, checked in `test_mods` `BuiltinModsSetTheOldValues`):
| Id | Options (default in bold) | Summary | Restart |
|---|---|---|---|
| [`graphics.msaa`](#graphicsmsaa) | `samples` 2x/**4x**/8x/16x | Multisample anti-aliasing | no |
| [`graphics.mipmaps`](#graphicsmipmaps) | — | Mipmaps and trilinear filtering | yes |
| [`graphics.anisotropic`](#graphicsanisotropic) | — | Anisotropic filtering (includes the mipmaps) | yes |
| [`graphics.terrain-x2`](#graphicsterrain-x2) | `repeat` x1/**x2**/x3/x4, `upscale` **off**/on, `cliffs` **triplanar**/stretched | Sharper terrain and sea | yes |
| [`graphics.smooth-smoke`](#graphicssmooth-smoke) | — | Everything that uses `smokea.raw` (smoke, clouds, fogs, water rings, boat puffs, glow of the night lights) with the 8-bit alpha (without the cut to 16 levels) | yes |
| [`graphics.hd-tweaks`](#graphicshd-tweaks) | `textures` **hd**/original, `smooth` off/soft/**round**, `light` **smooth**/original, `sharp` **on**/off, `detail` **high**/original | Better-looking villagers, animals and hand | no |
| [`water.living`](#waterliving) | — | Sea that reflects everything and drifts | no |
| [`world.ground-statics`](#worldground-statics) | — | Lowers floating statics to the ground | no |
| [`world.crops`](#worldcrops) | `speed` **x1**/x2/x5/x10/x20/x50/x100 (slider) | Fields that sow themselves (when off it no longer leaves its speed set: the old C++ class set it even when off, a fidelity bug) | no |
| [`world.foliage`](#worldfoliage) | `density` low/**medium**/high/very high, `distance` near/**medium**/far, `fields` **wheat**/original | Grass, flowers, reeds, bushes and wheat | no |
| [`world.foliage.beach`](#module-worldfoliagebeach) | `density` very low…**medium**…very high | Module: beach | no |
| [`world.foliage.butterflies`](#module-worldfoliagebutterflies) | — | Module: butterflies | no |
| [`test.miracle-dispensers`](#testmiracle-dispensers) | `level` **base**/pu1/pu2/all, `recharge` 2s/5s/**10s**/20s/30s/60s, `seed` **on**/off | Test miracle dispensers | no |
| [`game.skip-intro`](#gameskip-intro) (**enabled by default**) | `skip` tutorial/tutorial and creature training/**tutorial, creature training and the glade**, `free start` **on**/off | Starts Land 1 without the intro | yes |

### graphics.msaa

- Option `samples` 2x/4x/8x/16x (default 4x). No restart.
- Multisample anti-aliasing and alpha to coverage on leaves and fences (and on the plants of `world.foliage`).
- Shortcut `--msaa 0/2/4/8/16`. Multisampled backbuffer (`BGFX_RESET_MSAA_*`). In the opaque passes the cut-outs
  use **alpha to coverage**: `fs_object` turns the cut into a ramp of ~1 pixel with `fwidth`
  (`u_skyAlphaThreshold.z`).

### graphics.mipmaps

- No options. Requires a restart.
- Mipmaps and trilinear filtering. Shortcut `--mipmaps`. It is applied to model textures, L3D skins, terrain materials
  and bump and standalone `.raw` textures (the original has no mips: see
  [rendering.md](rendering.md#the-originals-direct3d-7-states)).
- Implementation (`Graphics/TextureMipmaps.cpp`, `BuildRgba8MipChain`):
  - decodes level 0 to RGBA8 with `bimg::imageDecodeToRgba8` (DXT1/3/5, BGRA4, BGR5A1, R8…);
  - does a 2×2 **alpha-weighted** average, so that transparent texels do not darken the edges;
  - in textures with almost binary alpha (≥85 % of texels with alpha <32 or >223) it **preserves coverage** at each level
    relative to the 0x96 reference, so that trees do not get thinner in the distance (without this they looked much
    thinner).
- `Texture2D::Create`: with `Filter::LinearMipmapLinear` it builds the chain and creates the texture in RGBA8 with mips.
  It frees the original `bgfx::Memory` with `bgfx::release`, which bgfx exports but does not declare in `bgfx.h`.
- `graphics::SurfaceTextureFilter()` returns `Linear` or `LinearMipmapLinear` depending on the mods. It is not applied to
  the heightmap, the footprints, the noise or the sky.
- `fs_terrain`: the small bump is sampled outside the distance `if`, because with mips derivatives are needed in uniform
  control flow.
- Cost: a few more seconds of loading and more video memory (RGBA8 instead of DXT).
- Verification (of `msaa`, `mipmaps` and `anisotropic`): screenshots (they were in `dev\gfx\`, deleted in the clean-up of
  2026-09-30; they are regenerated with these cameras and options):
  - `base_*` versus `enh_*` / `enh2_*`: village `1818,75,2612,1824,44,2636` and panorama
    `1600,160,2350,1900,40,2750`, with `-n 14000 --screenshot-frame 13900`. With mips loading is slower and at 8000
    frames the fly-in has not finished yet.
  - [img/crop_trees_zoom.png](img/crop_trees_zoom.png), grid of four: original, mips, MSAA and everything.

### graphics.anisotropic

- No options. Requires a restart.
- Anisotropic filtering (includes the mipmaps). Shortcut `--anisotropic`: adds `BGFX_SAMPLER_*_ANISOTROPIC` and
  `BGFX_RESET_MAXANISOTROPY`. `--enhanced-graphics` is equivalent to `--msaa 4 --anisotropic`.

### graphics.terrain-x2

Options `repeat` x1/x2/x3/x4, `upscale` off/on, `cliffs` triplanar/stretched. Requires a restart.

- **Repetition** (`repeat`): sharper terrain, each material repeated 1-4 times per block (default x2; wrap
  Repeat). Only scaling is barely noticeable: each 256 px material covers a block of 160 units.
- **Upscaling** (`upscale`): ×2 with Lanczos-3 when loading (`Graphics/TextureUpscale`, with wrap: the LND materials
  are tileable, first and last row/column identical).
- **Cliffs** (`cliffs`): the original projects everything from above (uv = xz position of the block) and on the slopes
  the texture stretches into stripes; with `triplanar` the projections along x and z are also blended (weights
  \|n\|⁴, smooth per-vertex normal from central differences of altitude, `LandVertex::normal`).
- **Picture materials**: the materials that are a single picture per block and not a texture (the figure geoglyph:
  Land1 material 10 and Land5 material 5; the maze: Land5 material 1) stay at ×1. Nothing in the LND marks them (their
  `type` 18/11 is shared by normal grasses, and the large-scale contrast metric does not separate them from a snowy
  rock), so they are recognised by the FNV-1a hash of their texels (`IsPictureMaterial` in LandIsland.cpp) and travel in
  the `w` byte of the vertex material ids (bits 0-2). If a data mod brings another picture, its hash has to be added
  (`dev\herramientas\lnd\lnd_hash.py`).
- **The sea** too: its repetition period (560 at detail level 4) is divided by the repetitions, the original's per-row
  ripple is divided the same way (otherwise, it moves the texture three times as much and leaves bands) and with
  `upscale` `sky.raw`/`skya.raw` are upscaled ×2 with Lanczos when loaded (`Texture2DLoader`, which also copies the data:
  before it passed bgfx a reference to a local vector). After upscaling they are cut to ARGB4444 as in the original
  ([rendering.md](rendering.md#argb4444-textures)).

### graphics.smooth-smoke

- No options. Requires a restart.
- `smokea.raw` keeps its 8 bits of alpha in everything that uses it: chimney smoke, clouds, fogs, water rings,
  boat puffs and the glow of the night lights (`NightLights`). The original cuts it to 16 levels (ARGB4444,
  `fn_00837400`; see [rendering.md](rendering.md#argb4444-textures)), for example 228 → 238/255.
- Implementation: `assets/mods/graphics.smooth-smoke/mod.json` sets the switch `graphics.smooth-smoke` (`EngineConfig::smoothSmokeAlpha`, restart), and `Texture2DLoader`
  skips the `smokea` cut. It was openblack's look before the cut on load existed.

### graphics.hd-tweaks

"HD-Tweaks" (formerly `graphics.hd-people`, renamed 2026-09-30). Options `textures` hd/original, `smooth`
off/soft/round, `light` smooth/original, `sharp` on/off, `detail` high/original. No restart: everything live.
Better-looking villagers, animals and hand. Full section (package, tests, state) in [mods.md](mods.md#hd-tweaks-mod).

- **Textures** (`textures`): the 256² atlases (4 villagers each, about 30 px per face) replaced by ×4 images
  from Real-ESRGAN (`Mods/graphics.hd-tweaks/textures/<id>.png` + `textures.json`; `Resources/HdTextures`,
  `Texture2DLoader::FromImageTag`, always with mipmaps). Each image carries the FNV-1a hash of the DDS it came from: with
  another AllMeshes.g3d it is not used.
- **Animals** (2026-09-30): their 5 atlases in HD, so they are also smoothed and use per-pixel lighting and `sharp`.
- **Hand**: it is also smoothed (`L3DSubMesh::IsHdTweaked`, mesh `Hand_Boned_Base2`) and uses per-pixel lighting; it is
  reloaded live along with the others.
- **Shapes** (`smooth`): the boned meshes whose textures are all from that list (openblack's mesh names
  are shifted relative to the user's package) become curved PN triangles (`3D/PnTessellation`, Vlachos 2001)
  split into 4 (`soft`) or 9 (`round`); the vertices are welded by position in the rest pose (average normal) and each
  new vertex goes back to the bone of the nearest corner, so it works with rigid animations. The collision (hand,
  physics) is still the original mesh.
- **Shapes and animations**: the joint triangles (corners on different bones) are no longer curved inside
  (they bent when animating): fan over their single-bone edge, they stretch like the original's.
- **Live** (2026-09-30): when changing the mod or its options in the menu, `Resources/HdTweaks` (`hd_tweaks::Update`, at
  the start of `Game::Update`) rereads AllMeshes.g3d and reloads only the 18 villager textures and the 113 boned meshes
  that use them (~0.6 s when enabling, ~0.15 s when disabling; the PNGs are decoded in parallel, also at
  startup). Hook `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>`.
- **Visible at gameplay distance** (2026-09-30; at 20-40 m a villager measures 40-70 px and the ×4 textures alone are not
  noticeable):
  - `light` = `smooth` (default: the same lighting as the original, the whole rule of `fn_0084BA90` with the functions of
    `assets/shaders/model_light.sh`, but per pixel in `fs_object` with the smooth normals and with the direction taken in
    world space, (approximate): it only matches with the light far away, by day; at night, with the light 3 units from
    the hand, it differs visibly on a nearby villager; see [Model lighting](rendering-objects.md#model-lighting)) or
    `original` (per vertex). A rim light on the silhouette (0.8·(1-N·V)²) was tried and looked ugly (user, 2026-09-30).
  - `sharp` = mip bias −1 on its textures.
  - `L3DSubMesh::IsPerson`, `u_window.y/z` (Renderer::DrawSubMesh, only lit instances like the original, not
    reflections or the hand).
- **Detail** (`detail`): `high` = villagers and animals with their high-detail mesh (the original always draws the std,
  LOD 1; `ECS/DetailMeshes`, changes the mesh of those that already exist when changing the option).

### water.living

- No options or restart.
- The sea reflects everything, the reflection ripples slowly in a loop and the surface drifts (without the per-row
  ripple).
- Shortcut `--living-water`. "Living water": the sea reflection includes models and sprites (the original only reflects
  sky and land) and ripples in a loop with two layers of `skya.raw` that scroll (wave map), stronger nearby and zero at
  a depth of 1500; it also removes the original's per-row ripple (fixed lines when paused, jitter at modern
  fps) and makes `sky.raw` and `skya.raw` drift together (0.020 / 0.012 textures per time unit). It uses real
  time at a quarter speed (also when paused) that wraps every 1000 units (4000 s); the speeds are
  multiples of 1/1000 texture/s, so the loop does not jump. The original's sea: [rendering.md](rendering.md#sea-skyraw--skyaraw).

### world.ground-statics

- No options or restart.
- Lowers floating rocks and static objects to the ground.

### world.crops

- Option `speed` x1..x100 (x1, x2, x5, x10, x20, x50, x100), slider. No restart.
- Fields sow themselves and are resown when harvested, and grow that multiple faster.
- Without it the engine is **faithful** to the original: the field's constructor leaves it empty and only farmers sow
  it, and openblack does not have jobs yet, so the fields stay empty.

### world.foliage

"Grass and flowers": grass, flowers, reeds and bushes on the terrain (instanced billboards, `3D/Foliage`;
flyers in `3D/FoliageFlyers.cpp`). No restart. Rules and images in `<exe>/Mods/world.foliage/` (`foliage.json`;
in the repo `assets/mods/world.foliage/`; the user's original images in `B&W/Asstes_mods`).

**Format (since 2026-10-02): `foliage.json`**, JSON with `//` comments. It is the same content as the old
`foliage.cfg`, which is still read if there is no `.json`. Each `[nombre]` section is an object of the `rules` list,
with `"section": "<nombre>"` and its keys; the lists (`images`, `texture`, `terrain`, `zone`, `not_zone`, `near`, `over`)
go as `["a", "b"]`, the numbers as numbers and the ranges as text (`"0.68-1.2"`):

```jsonc
{ "schema": 1, "rules": [
    { "section": "grass", "images": ["mono_grass_1.png"], "texture": ["green"], "per_cell": 90, "size": "0.68-1.2" },
    { "section": "field_stage brote", "growth": "0-80", "colour": "90,120,40 - 120,150,60" } ] }
```

`Mods/RuleFiles.h` converts the JSON into the same `clave = valor` lines that the `.cfg` read and passes them to the same
interpreter, so the result is identical. `tools/mod_cfg_to_json.py <foliage.cfg>` converts an old file:
before writing it checks that the rules come out the same, keeps the comments and saves the `.cfg` as `.cfg.old`.
What follows describes the keys by their name, the same in both formats.

**Options**

| Option | Choices | Effect |
|---|---|---|
| `density` | low/medium/high/very high = ×0.5/1/2/4 (default medium) | Multiplies the `per_cell` |
| `distance` | near/medium/far = 120/200/320 (default medium) | Draw distance (`foliageDistance`) |
| `fields` | wheat (default) / `original` = the mesh | [Crop fields](#crop-fields) |

#### Species: foliage.json keys

- One `[nombre]` section per plant in `foliage.json`: `images` (png, one at random per plant), `texture` (look of the
  texture: green/dry/sand/rock/snow), `terrain` (LND type, `TerrainMaterialType`), `per_cell` (per 10×10 cell
  at medium density), `size` (min-max width; the height comes from the image's proportions), `altitude`, `slope`
  (degrees), `patches` (0 uniform .. 1 only in patches, value noise at scale 45), `sway` (wind), `lean`
  (maximum random tilt) and `tint` (grey/all/none). It grows if it meets `texture` or `terrain`.
- `cross = on`: the species is drawn with the two crossed planes (the dry bushes); in each block those
  instances go at the end (`Chunk::crossStart`) and are drawn with the 12 indices of the quad.
- New keys for the module species (valid in any `foliage.json`):
  - `flat = on`: the image goes **lying on the ground**, centred on the point, with the top of the image along the
    `side` of the rotation and tilted like the ground (slope across and along in `i_data4.xy`, `i_data4.z = 2`). It is
    blended by its alpha without writing depth (the plants still cover it) and fades with distance instead of
    shrinking. In each block they go at the end (`Chunk::flatStart`). `lift` = height above the ground (0.04).
  - `coast = on`: it can be on coast cells or cells with water (the `altitude` keeps it out of the water).
  - `share = 0..1`: minimum part of the ground drawn at the point that belongs to its textures (the four corners by their
    bilinear weight and the two materials of each one by their blend, like the shader). The material chosen at random
    for the point (a corner and one of its two materials) can be sand even if almost everything visible is rock: with
    `share = 0.8` the beach only appears where almost everything is sand (the user saw patches and footprints on grey
    ground).
  - `opacity` (0-1, 1 by default): the flat ones are blended with that opacity (`i_data4.w`, which used to be 1 =
    blended; 0 is still alpha-tested). The beach footprints go at 0.45 and the wet sand at 0.6.
  - `shade`: with `tint` all/grey, scale of the ground colour it takes (goes in `i_data3.z` of the flat ones): the wet
    sand (`tint = all`, `shade = 0.7`) is the sand underneath, darker, instead of the image's orange.
- Sizes: on 29-09-2026 all the `size` values were reduced by 25 % (the user found them too big).
- Modules: their `foliage.json` is read after the mod's one with the same parser; the images are looked up next to each
  `foliage.json`, and an animated `.gif` gives one layer per frame (`stbi_load_gif`; the plants show the first one). It
  is reloaded when a module is switched on or off (`Renderer::DrawFoliage`, `_foliageLoadKey`).

#### Ground appearance: texture and terrain

- **The LND `type` does not describe the appearance** (**faithful**, LND data): in Land1 textures 0 and 8 are green grass
  with type 5 `Earth` and 11 is sand with type `Earth`; it is used for sounds/footsteps.
- That is why `texture` classifies each material by its mean colour (`Foliage::ClassifyTexture`, measured on Land1-5):
  green = hue 50-100° and saturation ≥ 0.55; snow = saturation < 0.15 and value > 0.55; sand = value ≥ 0.6; dry = hue
  < 50° and saturation ≥ 0.5; the rest rock (same hue range as grass but saturation 0.29-0.45). The island
  exposes type, "picture" and mean colour with `LandIslandInterface::GetMaterialInfo`.

#### Zones (biomes): zone and not_zone

- `zone` / `not_zone` filter by the cell's ambient zone, the sound code the designer painted on each
  cell (`LNDCell::flags >> 1`, odd ones > 8 count as the previous even one; `Foliage::ZoneOf`). The data is
  **faithful** (LND sound zones); using them as biomes is **mod/own**.
- It is the only thing in the LND that forms clean regions: the `country` values are only the per-height texture
  palette and are very fragmented (Land1: 10 mixed all over the map).
- Zones on the land of Land1-5: 14 birds (`meadow`, almost everything), 6 coast (strip next to the sea), 8 jungle
  (compact patches: Land1 north-west ~1620.2290 and east ~2550.2550; Land5 5-6 patches), 16 forest (Land1 ~2160.3100),
  10 wind = snow and mountain (Land2 all the south-west, Land3, Land5 north-east), 4 slow waves (`swamp`: inland ponds,
  many in Land5) and 5 lake (Land2 centre). 12 desert is not used by any original map.
- Maps in `dev\documentacion\biomes\Land*_snd.png` (`dev\herramientas\lnd\lnd_zones.py`; `dev\herramientas\lnd\lnd_countries.py` for
  the countries).
- Current use: `water_plant` in jungle, lake and ponds; `jungle_grass` in the jungle; `wildflowers` in meadow and forest;
  `poppies` in meadow; `dead_bush_barren` in wind/desert (only rock, dry earth or sand). All with `tint = grey`.

#### Height and water

- **Height** (29-09-2026, study in `dev\documentacion\heights`): `GetHeightAt` **flattens** next to the sea
  (if the cell's base corner is ≤ 4, the corners ≤ 3 count as 0: what physics and vs_object use;
  `GetNormalAt` no longer flattens since 2026-10-02: it is `LH3DIsland::GetNormal` 0x803630, with the raw heights),
  but the terrain mesh that is drawn does not. The plants use `GetUnflattenedHeightAt` and a normal from central
  differences of that height (`GroundNormal`): before they ended up as much as 2 units below the drawn ground in the
  first strip of land and the whole beach gave height 0.
- LND data (**faithful**): altitude byte × 0.67; in Land1-5 the cells with water are 0-1 (0-0.67, transparent
  terrain), the coast ones always 2 (1.34, alpha 0.5) and opaque land starts at 3 (2.01); the maximum is 255
  (170.85). That is why all plant `altitude` values become `0-175` (the minimums 1-2 are no longer needed: the coast
  is excluded; the maximums 120/150 cut off the high meadows of Land3).
- **Water**: the sea is the plane y = 0 (and it is also the water of the rivers, see [rendering.md](rendering.md#rivers) "Rivers"); the
  coast cells (`coastLine`, altitude 2-3 in Land1) are drawn with alpha 0.5 over the sea and the water ones with alpha 0,
  so nothing grows in a cell with any water or coast corner.
- `near = lake, stream, sea` + `water_distance` limit a plant to that distance from water (3-4 chamfer distance map
  at 5 units, `FoliageWaterMap`): water cell = `sea_cells::IsWater` (bit 0x10; a cell without a block
  is also water, MapCoords::IsWater 0x6035B0) or `fullWater`; lake = 4-connected water cells that do not reach the map edge (Land1:
  a pond of 10 cells at x 2130-2160, z 2400-2450 and a single loose cell); river = segments between the points of each
  `Stream` (Land1: 11 rivers, 187 points). In B&W1 there is no water at another height: the rivers are those paths
  (openblack draws them like the original since 101dd844, `ECS/Rivers`, see [rendering.md](rendering.md#rivers)).
- The reeds use `near = lake, stream` at 3-9 units. No plant within 3 units of a river's line (the
  river.l3d channel measures about 4; exact distance to the segments in buckets of 20 units).
- The base of each plant follows the ground: height at its two ends (i_data4) and shear in the vertex shader, sunk
  by 6 %.

#### Placement

- Deterministic per terrain block, **only near the camera** (up to 6 blocks per frame; they are freed when
  moving one block further away): per cell and plant, `per_cell × densidad` candidates; at each point a corner of the
  cell is chosen by its bilinear weight and one of its two materials by the blend coefficient (like the terrain
  shader).
- Nothing in water cells, in picture materials (geoglyph), outside the height or slope, nor within 1 unit of
  `Fixed` entities that are not trees, nor of fields, mobile rocks, piles, storehouse, temple or fish farm (box of
  the mesh).
- Everything is redone when changing island or density and when the objects exist.

#### Ground tint

- The grey texels (saturation < 0.1-0.2) take the colour of the terrain texture under the plant: the vertex shader
  samples the material array at the same material and uv as the terrain (block uv × repetitions of the
  terrain-x2 mod, mip 3); grey 0.5 = the ground as it is, darker at the base and lighter at the tip. The colour
  texels (petals, ears) do not change. `tint = all` tints the whole image; `none` uses its own colours.
- **Dark or colourless ground**: the tint takes the material's texture at low resolution, and some have very dark
  patches (Land1 material 5, heather, type 25: 46 % of its 32×32 texels with brightness < 0.3) or grey ones (material 10
  13 % with saturation < 0.3), which gave grey plants. `LandMaterialInfo::small` stores each material at 32×32
  (average of 8×8 boxes, like the mip 3 the shader samples) and `ground_value` / `ground_saturation` filter by the
  colour of that texel (same uv as the terrain, with the terrain-x2 repetitions). The tinted plants require
  brightness ≥ 0.28 and saturation ≥ 0.3; `dead_bush_dark` / `dead_bush_grey` (with their own colours, `tint = none`)
  fill the patches.

#### Drawing

- One plane per plant with a fixed random orientation (it does not face the camera) and tilted at random up to `lean` so
  that it is visible from above (two crossed planes looked like crosses from above); sunk by 12 % of its height so that
  the bottom edge is not visible.
- The plants sink in the last 20 % of the distance (120/200/320).
- Light = terrain light table[cell brightness] and the same haze; alpha test with a sharp edge (alpha to
  coverage with MSAA). Only in the main pass (not in the reflection).
- 256×512 layers resting at the bottom, with mipmaps; the colour of the transparent texels is the average of the opaque
  ones.

#### Sprites

- The `mono_*` ones (except `dead_bush_dark` / `dead_bush_grey`, which use `dead_bush_*.png` with their colours): the
  user's images (`B&W/Asstes_mods/Plants`, those of v2; the current `mono_*` are in
  `B&W/BnW_openblack/Mods/world.foliage`) converted to grey with
  `assets/mods/world.foliage/tools/mono_sprites.py --width=128`:
  - grass, tall grass, bushes and wheat with `--min-hue=0` (everything to grey);
  - reeds, water plants and flowers with `--min-hue=50 --open=1`: the green (hue 50-170°) to grey with mean 0.62 and of
    the rest only the patches that survive a 3×3 morphological opening remain in colour (petals, reed heads,
    plumes); the thin yellow-brown streaks and the almost white highlights of the leaves also to grey (with
    `--min-hue=50` without the opening, untinted orange streaks came out);
  - the poorly saturated highlights and edges (s <= 0.12, v < 0.85) also to grey and only the almost white ones (v >= 0.85)
    with a cream touch so that they are not tinted.
- The `gen_*` generated by `tools/gen_grass_sprites.py` (in the repo) are no longer used.
- The base of each image is trimmed irregularly by columns (up to 9 % of the height) so that the straight edge is not
  visible.

#### Crop fields

Option `fields` = wheat (default); `original` = the mesh.

- **Plants**: the field mesh is hidden (`Alpha` 0 in `ecs::UpdateFields`; it is still there for the hand, and instances
  with alpha 0 are no longer drawn: they wrote depth) and `Foliage::UpdateFields` puts on its footprint (box of the mesh
  with its rotation and scale) a noisy grid every `[field] spacing` units. Every frame, per field in range: nothing
  if not sown; the `[field_stage ...]` stage according to growth (0-1200) ± `stagger` at random per plant (gradual
  change); width and tint interpolated within the stage (the tint replaces the ground colour: `i_data4.z` = 1, `w`
  = r·65536 + g·256 + b); only the plants with `keep` < food / expected food at that growth remain, so the
  harvest thins it out. Transient instances every frame.
- Current stages: sprout (low grass, 0-80), tall grass (80-350), green wheat (350-750), wheat drying until
  ripe brown (750-1200). Hook `OPENBLACK_TEST_FIELD_GROWTH=0..1200` (all fields start with that
  growth and its food).
- **Farmland**: `[field] soil` (`field_soil.png`, from `tools/gen_field_soil.py`: brown furrows with an irregular edge
  that fades out) is painted onto the footprint texture (`Foliage::DrawFieldFootprints`, from
  `Renderer::DrawFootprintPass`, with the FootprintInstanced program) over the field's box + `soil_margin` per side.
- **From afar**: the field mesh comes back with alpha `(d − 0,8·D) / (0,2·D)` (d = distance to the camera, D =
  `foliageDistance`), the same stretch in which the plants sink into the ground, without sinking with the food (in the
  original a young field is barely visible) and only if it is sown and has food. It appears **dissolving**: a screen
  pattern of crosses that grow in 8×8-pixel cells (fs_object, discards instead of blending).
- **Mesh tint** (`components::MeshTint`, set by `Foliage::UpdateFields`): like the plants, its texels
  become grey × mean colour of the ground under the field (`GroundColourAt`, 9 points, mean colour of the cell's
  materials for its altitude), blended towards its own colour according to `[field] ripening` (350-1200). It travels in
  the w of the instance's third column: 1e6 (2e6 when dissolving) + 5 bits per channel of the ground and of `own`; the
  heightmap shader no longer adds that w as an offset if it exceeds 500000 (the fields are MorphWithTerrain, so they
  lose the sinking offset, which they do not use with the mod). vs_object passes it to fs_object in `v_normal`
  (fs_object does not light with the normal): 1000 + 2·own in x and the colour in the fractions. After touching vs_object
  you have to `touch` the vs_object_*instanced*.sc (openblack-internals.md).
- **Beware**: the field mesh (MSH_T_WHEAT) has its own footprint, and `vs_footprint_instanced` used the whole instance
  columns: the tint's w (> 1e6) broke the projection and its footprint covered the whole footprint texture
  (plain olive-green terrain, without paths or building footprints, only with the opaque field mesh, from afar). Now
  that shader takes only xyz of the first three columns, like vs_object (the same happened with the alpha of a mesh with
  a footprint that fades out).

#### Flyers: [flyer name]

- **`[flyer nombre]`** (`FoliageFlyers.cpp`): flyers over the plants of the species in `over` (by name, from
  any `foliage.json`). When a block is placed, each of those plants has a butterfly with probability `per_plant`
  (`Chunk::homes`).
- **Flight**: every frame, up to 110 units from the camera: it flies `flight` s in a loop of two sines per axis
  around its flower (radius `range`, height `height` above the flower, flapping of ±0.12 rad), takes off from the flower
  and returns to it, and then lands for `rest` s flapping at 1/4 speed. Frame according to the gif's timings (those of
  less than 20 ms count as 100 ms, like browsers). Flat and alpha-tested (they write depth,
  `v_texcoord0.w = 5`), in transient instances like those of the fields. Everything comes from real time and from the
  plant's seed.
- **Fleeing from the hand**: it only stores its flight (`FlyerHome::fleeTime/away/offset`), like the fish with a splash
  (`FishShoals.cpp`) but by proximity: with the hand (`HandSystemInterface::GetPlayerHandPositions`) within `flee`
  (5) horizontally and `2·flee` in height it shoots off in a straight line away from it for 2 s, ×4 its speed
  (`0,6·range·speed`, minimum 1) and braking in the last second, rising `0,4` of what it advances; if the hand is still
  close when it brakes, it shoots off again. Then it returns to its path at its normal speed, facing it, and waits
  where it is while the hand is still within `1,5·flee` (`OPENBLACK_HAND_TRACE=1` writes `Flyer trace`).
- **Folding wings** (`fold`, 0.7 by default): the gif keeps playing (its frames change the pose of the
  wings, not only the width) on a square split by the body (`_foldQuad`, x = -0.5/0/0.5), and each half rises
  rotating about it `fold · acos(ancho del fotograma / el más ancho)` (`Animation::folds`, measured by the opaque pixel
  furthest from the central column), interpolating to that of the next frame; in the shader `i_data4.z = 3`, `w` = the
  fold.
- **Daytime only** (`night = off`): with the game time (`SkyInterface::GetTime`) daylight goes from 0 at
  20:00-5:30 to 1 at 7:00-18:30 and each butterfly leaves when it drops below its random threshold, so they disappear one
  by one.

### Module world.foliage.beach

- "Beach": seaweed, wet sand, shells, starfish, coral and footprints, all `flat` and `coast` on sand
  (`texture = sand`, `terrain = Sand, WetSand`). Option `density` (slider, very low..very high, default
  medium; see [Modules with option.\<id\>](#módulos-con-optionid)).
- The shoreline is marked by the drawn height: seaweed 1.1-2 and wet sand 0.9-1.7 (the coast row), the rest up to 2.2-6.
  `water_distance` measures from the corners of the cells with water, so the visible shoreline ends up at 6-10 units.
- Two strips (30-09-2026, the user: the busy shoreline and the clean sand): the shoreline (`coast`, up to ~20: seaweed
  7-16, wet sand, starfish, shells) with low density, and the dry sand behind (`sand_*`, coral and footprints, 14-80,
  height up to 40, `share` 0.7, without `coast`) with more (shells 1.1, stones 0.6 per cell at medium); the footprints
  only there, from 20.
- In Land1, sandy beach at `1700,2000` (camera `1702,7,1992,1706,0.5,2004`; `dev\herramientas\lnd\lnd_beaches.py` lists the
  sand next to the water).
- The user's images in `B&W/Asstes_mods/Beach`; `.cfg` in `assets/mods/world.foliage.beach`.

### Module world.foliage.butterflies

- `world.foliage.butterflies` ("Butterflies"): the user's 3 gifs (`B&W/Asstes_mods/Buterfly`) over
  `wildflowers` and `poppies`, 0.04 per flower, 0.7-1 wide (bigger than in reality so that they are visible next to the
  0.7-1.2 grass). No options. How they fly: [Flyers](#flyers-flyer-name).
- In Land1 there are about 70 near `1434,57.8,2232` (camera `1428,61.5,2226,1434,57.5,2233`, `OPENBLACK_TIME_OF_DAY=13`).

### test.miracle-dispensers

«Test miracle machines» (category **Test**). **It does not exist in the original**: it is an aid for testing the
miracles, disabled by default. Code: `assets/mods/test.miracle-dispensers/mod.json` (the mod, which sets
`EngineConfig::testDispensers*`) and `src/Worship/TestDispensers.cpp` (what it does in the game). Everything is **mod**;
only the dispensers are the original's ([magic.md](magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)).

- **Enabling it**: **Mods** menu → *Test* section → checkbox «Test miracle machines» (and its two sliders
  below); it is saved in `Mods/test.miracle-dispensers/settings.cfg` (`enabled = on`, `level = base`,
  `recharge = 10s`). For one session only: `--mod test.miracle-dispensers` (plus
  `--mod test.miracle-dispensers.level=all`, `--mod test.miracle-dispensers.recharge=5s`). No restart, but the
  dispensers appear **when a land is loaded** (or on the next turn if it is switched on with the land loaded); when
  switched off they stay until the next load. The recharge is changed live on those already placed.
- **What it does**: when the land's script has run (after `PostLoadCleanup`, in `worship::ProcessTurn`) and the
  human player has a citadel (`citadel::Of`; its position is looked up in the entity, it is not hard-coded),
  it places a `NORSE_ABODE_SPELL_DISPENSER` dispenser (the one from the Land 1 challenge) per miracle, like
  `GiveSpellDispenserReward`: `dispenser::Create` (the player's nearest town, facing the temple),
  `SetMagicProperties(magia, recarga)` and `SetActive` (orb immediately). When the orb is taken, another one appears
  after `recharge` seconds.
- **Where**: in rings around the temple, the first one at the temple's radius (larger half of its mesh in x/z, 25.6 m in
  Land 1) + 12 m and the following ones every 13 m, placed every 13 m of arc (the odd rings shifted half a step). A
  spot is valid if an 8 × 8 m square (9 points) is dry land without water (`sea_cells::IsWater` / `IsDryLand`), with
  less than 2.5 m of height difference, within the player's influence (`CalculatePlayerInfluence > 0`, the casting
  rule), more than 4 m from any fixed object (`Fixed`, buildings, trees, features, rocks, pots, fields,
  street lamps, totem, places of worship) and `map_collide::IsOkToCreateAtPos` accepts it. Constants chosen by openblack
  (mod).
- **Miracles** (the player's 14 seeds of `GSpellSeedInfo`, in their order; the creature's, 12..27, not): STORM
  (storm), NATURE (forest), FIRE (fireball), FOOD (food), SHIELD (shield), PHYSICAL_SHIELD (physical shield),
  LIGHTNING_BOLT (lightning bolt), HEAL (heal), WOOD (wood), WATER (water), FLYING_FLOCK (flock of doves), GROUND_FLOCK
  (pack of wolves), TELEPORT (teleport) and BEAM_EXPLOSION (beam explosion). The electric storm and the
  tornado are not seeds: they are STORM's power-ups.
- **Option `level`** (slider): `base` the base magic of each seed; `pu1` / `pu2` its power-up 1 / 2 (if the
  seed does not have it, the highest it has); `all` one dispenser for each distinct level (25 in total: STORM,
  STORM_PU1 = electric storm, STORM_PU2 = tornado; FIRE ×3; FOOD ×2; LIGHTNING_BOLT ×3; HEAL ×2; WATER ×2;
  BEAM_EXPLOSION ×3; the rest ×1). The orb comes out with the level of its magic (`GetPowerUpGesture`, like the original).
- **Option `recharge`** (slider): seconds until the next orb (`SET_MAGIC_PROPERTIES` in seconds × 10
  turns); the original uses 300 turns (`timeEachMobileObjectTakesToProduce`).
- **Empty machine** (always): one more dispenser at the next free spot of the ring (in Land 1 with `base`,
  (1878.0, 2515.4)), created only with `dispenser::Create`, like a `CREATE(SPELL_DISPENSER)` without
  `SET_MAGIC_PROPERTIES` or `SET_ACTIVE`: it stays inactive and without magic, so it never gives an orb
  (`SpellDispenser::Process` 0x722A70 only produces if it is active). It serves to compare the machine alone with those
  that have an orb. Log: `Mod test.miracle-dispensers: empty dispenser <entidad> at (x, z)`.
- **Option `seed`** (`on` by default): 10 turns after placing the dispensers (so that the land's script
  and its intro have already started) it puts a FIRE seed (fireball, without power-up) in the human player's hand
  via the one-off path, `OneOffSpellSeed::CreateSpellIntoHand` 0x72A730 (the same as `OPENBLACK_TEST_SEED`). If the
  hand is busy it retries every turn (up to 600). This way the transparency of an orb, that of the
  empty machine and that of the seed in the hand can be compared. `--mod test.miracle-dispensers.seed=off` removes it.
  Log: `Mod test.miracle-dispensers: fire seed into the hand -> <entidad>`. Turns and retries chosen by openblack
  (mod).
- **Creation order**: the dispensers and their orbs do not exist in the original, so they are created inside an
  `ecs::object_index::ModScope`: they take indices from a separate range (from `k_ModBase` = 0x40000000) and the
  original's counter does not move (the villagers' speeds, `Villager::SetSpeed`, and the orders of animals and forests
  stay the same). `SpellDispenser::CreateOneOffSpellSeed` / `ApplySeed` open the same scope if the dispenser belongs to
  a mod (`IsModObject`). They do not consume the game's random numbers. They are town abodes and fixed obstacles,
  like the Land 1 dispenser. The seed the orb gives when touched and what the spell creates count as always
  (they are player actions).
- **Log**: one line per dispenser:
  `Mod test.miracle-dispensers: dispenser <entidad> seed <n> (<SEMILLA>) pu <nivel> magic <n> (<MAGIA>) at (x, z)`.
- **Land 1** (`level = base`; temple at (1915.1, 2508.9), ring of 37.6 m, all 14 fit in the first one):

  | Seed | Position (x, z) |
  |---|---|
  | STORM | 1915.1, 2546.5 |
  | NATURE | 1927.9, 2544.2 |
  | FIRE | 1939.2, 2537.7 |
  | FOOD | 1947.6, 2527.7 |
  | SHIELD | 1952.1, 2515.4 |
  | PHYSICAL_SHIELD | 1952.1, 2502.4 |
  | LIGHTNING_BOLT | 1947.6, 2490.1 |
  | HEAL | 1939.2, 2480.1 |
  | WOOD | 1927.9, 2473.6 |
  | WATER | 1915.1, 2471.3 |
  | FLYING_FLOCK | 1902.2, 2473.6 |
  | GROUND_FLOCK | 1890.9, 2480.1 |
  | TELEPORT | 1882.5, 2490.1 |
  | BEAM_EXPLOSION | 1878.0, 2502.4 |

  With `all` the 25 occupy the first ring (18 spots) and 7 of the second (radius 50.6 m).
- **Tested** (2026-10-01, screenshots in `dev\_audit\magic\`): `dispmod_ring.png` (the ring in Land 1),
  `dispmod_cast.png` (the FIRE orb touched, `seed (FIRE, pu -1) in the hand with 3500 chants`, charged and cast:
  the fireball on the ground and its empty dispenser) and `dispmod_all.png` (`level = all`, 25 dispensers). 10 s after
  the touch the dispenser makes another orb. Camera hook: `OPENBLACK_CAMERA_LOCK=1960,85,2580,1915,32,2508`
  (see [Test hooks](#test-hooks)).
- **Tested** (2026-10-01, empty machine and seed): `prism_empty.png` (the empty machine in the foreground, without an orb;
  `OPENBLACK_CAMERA_LOCK=1872,40,2528,1878,33,2515.4`), `prism_seed_hand.png` / `prism_seed_hand2.png` (the fire seed
  in the hand next to the FIRE orb; `seed 2752 (FIRE, pu -1) in the hand with 3500 chants`).
- **The dark «prism» next to a dispenser** (user's screenshot, 2026-10-01 12:41): it is not the physics submesh
  of mesh 557 (no drawing path paints it, see
  [rendering-objects.md](rendering-objects.md#physics-and-lod-0-submeshes)). The log of that game says that the
  user broke the TELEPORT and BEAM_EXPLOSION dispensers with a thrown rock (`Buildings: 2602 hit, life 1.00
  -> 0.00`, `4 pieces`, `2602 destroyed`, and the same for 2606): the BEAM_EXPLOSION orb (the `I_Blast` star) was
  left floating and what is around it are the pieces (`Fragment`) of the broken machine. **(inferred)** In the original
  `SpellDispenser::Draw` 0x722940 calls `MultiMapFixed::Draw` 0x518090 and not `Abode::Draw`, so it never draws the
  FragMesh of a damaged dispenser; it remains to be read whether `Abode::ReactToPhysicsImpact` 0x406240 breaks it
  (pending).

### game.skip-intro

**The only mod enabled by default** (requested by the user, 2026-10-01; the rest remain off). This is done by the new
field `Mod::Info::enabledByDefault`, which the `Mod` constructor copies to `_enabled`. Beware: if
`Mods/game.skip-intro/settings.cfg` already exists, **that file takes precedence** (as in any mod), so a change of
default value does not reach an installation that has already started once; its `settings.cfg` has to be edited.

- Option `skip`: `tutorial`, `tutorial and creature training` or `tutorial, creature training and the glade`
  (**default**). Option `free start`: `on` (default) or `off`. With restart (the answer counts when the game
  starts). For a single time: `--mod game.skip-intro.skip=tutorial`, `--mod game.skip-intro.free start=off`,
  `--mod game.skip-intro=off`.
- The skip itself is not invented: it gives the answer the original asked the player for. In runblack.exe v1.42, at the
  start of each game, `GGame::OnNewGame` (0x55395B) calls `GGame::DoYesNoSkipTutorialRequestersIfNecessary` (0x54CBD0),
  which clears bits 23, 24 and 25 of `g_game+0x14`, pauses the game and shows the **SkipBox** (four checkboxes, the
  first checked by default; no ESC, `SkipBox::CanESCOut` 0x53BD60 gives 0). openblack does not draw that box and plays
  everything, like the default answer; with the mod, `Game::Run` sets the bits of the second (`tutorial`, bit 23), the
  third (`tutorial and creature training`, bits 23 and 24) or the fourth answer (`…and the glade`, bits 23, 24 and 25),
  and the script skips the intro on its own.
- What the script skips (`SetupLand1` / `LandControl1` of challenge.chl): with `CAN_SKIP_TUTORIAL` `FollowUs` (the
  intro: the script camera and `START_MUSIC 54`), `CitadelGuide` (the citadel is built immediately) and
  `ChooseYourCreature` do not run. With `CAN_SKIP_CREATURE_TRAINING` the creature guide's lessons do not run either.
  With `IS_KEEPING_OLD_CREATURE` `CreaturesInGlade` does not run either, which is **the one that takes the camera and the
  dialogue, fades to black, flies the camera and sets `START_MUSIC(63)`** (challenge.chl 44028..44691): that is why the
  fourth answer is the one that leaves the start to the player. Details in
  [map-loading.md](map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial).
- The fourth answer also needs `CURRENT_PROFILE_HAS_CREATURE` (`SetupLand1` `and`s it with bit 25,
  challenge.chl 25432..25434) and openblack has no player profiles: the mod **answers on behalf of the profile** (CHL 463
  gives true when `skipTutorialChoice` is 3). What the script then does instead of the glade is load the profile's
  creature (`LOAD_MY_CREATURE`, not ported: no creature appears, as today).
- **`free start` (not from the original).** Even with the fourth answer the script runs `CreatureDevSeeHome`, which in
  its skip branch (challenge.chl 7056..7085) takes the camera and the dialogue for one turn, switches the widescreen on
  and off, **pins the camera over the village** (`SET_CAMERA_POSITION(1891.04, 31.69, 2520.67)`) and does
  `SET_FADE_IN(2.0)`. With `free start` the engine **swallows that**: the **first script task that takes the camera in a
  new game** is «the start of the land», and while it has it, `SET_CAMERA_POSITION` (001), `SET_CAMERA_FOCUS` (002),
  the other camera opcodes (003, 004, 093, 094, 095, 105, 119, 142, 201, 209, 279, 280, 284, 286, 287), `SET_WIDESCREEN` (032), `SET_FADE` (241),
  `SET_FADE_IN` (242), `START_MUSIC` (044) and `STOP_MUSIC` (045) do nothing and `HAS_CAMERA_ARRIVED` (035) answers «it
  has already arrived». `START_CAMERA_CONTROL` **is granted** and creates the script camera mode as in the original (so
  no other task takes the camera while the opening has it), so that the script's `loop { START_CAMERA_CONTROL }`
  passes and releases the camera as always; but that mode does not move the player's camera (`script_camera::Drives`,
  [script-camera.md](script-camera.md)). As soon as that task does `END_CAMERA_CONTROL` (or stops)
  everything goes back to normal: the miracle scenes, the quests and the vortices stay the same. State:
  `CameraControl::freeStartTask` / `freeStartArmed` (`Help/ScriptControl.h`), armed in `CameraControl::Reset` (every
  map load).
- What the mod does **not** touch: the time of day set by the script (`SET_GAME_TIME(4.59)` of `CreatureDevSeeHome`:
  dawn, as in the original) and the alignment/tribe music, which in the original also plays from the start when the
  tutorial is skipped (the `ENABLE_DISABLE_ALIGNMENT_MUSIC(false)` is inside `FollowUs`, challenge.chl 50102).

### Modpack examples

`mods/examples/` in the repo, `Mods/examples/` next to the exe (the build copies the files and compiles the native ones
into their `bin/`). Off by default. They change nothing in the game except `example.data-only` (living water while it
is on); they write to the Log tab. They are the templates.

| Mod | Type | What it teaches |
|---|---|---|
| `example.data-only` | only `mod.json` | `switches`, an option with `bind`, empty `replace` |
| `example.lua-hello` | Lua | `require` of its own module, option, switch, enumerations, `land_loaded` and `turn` events, camera and height |
| `example.lua-library` | Lua, library | `provides` + `ob.interfaces.provide("example.places.v1", tabla)` |
| `example.lua-consumer` | Lua | `dependencies` + `ob.interfaces.get` |
| `example.native-hello` | C | `ob_mod_query` / `ob_mod_load` / `ob_mod_unload`, option, events, enumeration, height, time |
| `example.native-library` | C, library | `provide_interface("example.counter.v1")` with its public header `include/example_counter_v1.h` |
| `example.native-consumer` | C | `dependencies` + `get_interface` |

## Pending

- Mod SDK (2026-10-01), what is missing: looping sounds or sounds that follow an object (today only standalone effects), casting orbs (when the miracles `one_off::` API is stable), measurements of a specific object (object
  identifiers are needed in the API; today only by mesh),
  memory limit per Lua script, hot reloading of scripts, replacing sound banks (with
  audio, B11), replacing meshes live (today at startup: the physics shapes are taken when each object is created),
  textures embedded in a `.l3d` (`L3DMesh::_skins`) and standalone materials of the `.lnd`, translations
  `lang/<idioma>.json` (today the per-language texts go inside the `mod.json`), and the window's language (today
  English; `mods::SetLanguage`).
- **better physics** (pending, requested by the user, to be made later and disabled by default): building pieces that
  collide with each other and with other objects, and that the hand can pick up ([physics.md](physics.md#pending)). Not
  designed yet: new mods are only noted for now (user, 2026-10-04).
- `world.crops`: without the mod the fields stay empty until openblack has jobs (farmers).
- HD-Tweaks: what remains to be checked is in [mods.md](mods.md#hd-tweaks-mod).
- Review of all mods after base 0e10b735 (2026-10-01): all compile, read their `settings.cfg` and work
  on and off; `world.foliage` reads the water with `sea_cells::IsWater`. `world.ground-statics` was not seen
  lowering anything: at the spots looked at in Land 1 (1327.2432 and 1342.2406) no static floats with the current
  AllMeshes.g3d.

## Test hooks

| Hook | What it does |
|---|---|
| `--mod <id>`, `--mod <id>=off`, `--mod <id>.<opción>=<elección>` | Enables a mod or option only for that session |
| `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` | Changes HD-Tweaks on that frame (live reload) |
| `OPENBLACK_TEST_FIELD_GROWTH=0..1200` | All fields start with that growth and its food |
| `OPENBLACK_HAND_TRACE=1` | Writes `Flyer trace` (butterflies fleeing) |
| `OPENBLACK_TIME_OF_DAY=13` | Game time for seeing the butterflies (daytime only) |
| `OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"` | Puts the camera there every turn (`WorshipDebugHooks.cpp`): in Land 1 the script moves the camera and `OPENBLACK_CAMERA_FLY` no longer gets through |
| `--mod test.miracle-dispensers` + `OPENBLACK_TEST_TAP="1939.2,2537.7,200"` + `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"` | Touches the FIRE orb of Land 1 and casts it |

Cameras: Land1 beach `1702,7,1992,1706,0.5,2004`; Land1 butterflies `1428,61.5,2226,1434,57.5,2233`.

## Sources

- Code: `src/Mods/` ([How it is built](#how-it-is-built-srcmods)), `src/Debug/ModsWindow.*`,
  `components/modsdk/include/openblack/mod_api.h`, `mods/` (examples and their CMake), `test/test_mods.cpp`;
  `game.skip-intro` in `Game::Run` and `CHLApi.cpp` (`CanSkipTutorial`, `FreeStart`), `src/Worship/TestDispensers.cpp`,
  `src/3D/Foliage.*`, `src/3D/FoliageFlyers.cpp`, `src/Resources/HdTweaks`, `src/main.cpp` (shortcuts).
- Manifests in the repo: `assets/mods/<id>/mod.json` (the 13 that come with openblack) and their data:
  `assets/mods/world.foliage`, `assets/mods/world.foliage.beach`, `assets/mods/world.foliage.butterflies`,
  `assets/mods/graphics.hd-tweaks`.
- SDK design: `dev\documentacion\modding\PLAN.md` (with what was taken from Factorio, Fabric, RimWorld, SKSE and Luanti).
- User's images: `B&W/Asstes_mods/{Plants,Beach,Buterfly}`; `mono_*` in `B&W/BnW_openblack/Mods/world.foliage`.
- Studies: `dev\documentacion\heights` (height next to the sea), `dev\documentacion\biomes` (zone maps `Land*_snd.png`).
- LND scripts: `dev\herramientas\lnd\` (`lnd_hash.py`, `lnd_zones.py`, `lnd_countries.py`, `lnd_beaches.py`).
