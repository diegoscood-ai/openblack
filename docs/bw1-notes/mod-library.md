# Librería de mods

Todo lo que cambia el juego original es un **mod**, desactivado por defecto (la única excepción, pedida por el usuario,
es [`game.skip-intro`](#gameskip-intro)). Un mod es una **carpeta** de `Mods/` con un **`mod.json`**: lo que el mod
es (nombre, versión, categoría, imagen…), sus opciones y qué cambia. Puede no tener código (solo datos), tener un
script **Lua** o una librería **nativa** (DLL / .so, en C). Los mods pueden ser **librerías** para otros mods y se
agrupan en **modpacks**. La librería (`src/Mods/`) los descubre solos al arrancar, resuelve dependencias y orden de
carga, dibuja la ventana **Mods** y guarda el estado de cada uno en su `settings.cfg`. Todo lo de esta página es
**mod/propio** salvo que se diga lo contrario (**fiel**, **(aproximado)**).

- [Para el jugador](#para-el-jugador)
  - [Ventana Mods](#ventana-mods)
  - [Carpeta Mods](#carpeta-mods)
  - [Línea de comandos](#línea-de-comandos)
- [Para crear mods](#para-crear-mods)
  - [Estructura de un mod](#estructura-de-un-mod)
  - [mod.json](#modjson)
  - [Opciones e interruptores](#opciones-e-interruptores)
  - [Reemplazar mallas, texturas, objetos y archivos](#reemplazar-mallas-texturas-objetos-y-archivos)
  - [Mods Lua](#mods-lua)
  - [Mods nativos (DLL)](#mods-nativos-dll)
  - [Mods librería](#mods-librería)
  - [Modpacks](#modpacks)
  - [Dependencias y orden de carga](#dependencias-y-orden-de-carga)
  - [Carpetas antiguas (mod.cfg)](#carpetas-antiguas-modcfg)
- [Referencia](#referencia)
  - [Interruptores del motor](#interruptores-del-motor)
  - [Enumeraciones](#enumeraciones)
  - [API: JSON, Lua y C](#api-json-lua-y-c)
- [Cómo está hecho (src/Mods)](#cómo-está-hecho-srcmods)
- [Catálogo de mods](#catálogo-de-mods)
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
  - [Módulo world.foliage.beach](#módulo-worldfoliagebeach)
  - [Módulo world.foliage.butterflies](#módulo-worldfoliagebutterflies)
  - [test.miracle-dispensers](#testmiracle-dispensers)
  - [game.skip-intro](#gameskip-intro)
  - [Modpack examples](#modpack-examples)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

## Para el jugador

### Ventana Mods

El botón **Mods** de la barra de menú abre la ventana de mods (`src/Debug/ModsWindow.*`), estilo Project Zomboid, con
cuatro pestañas:

- **Modpacks**: cada pack con su imagen, nombre, versión, autor y descripción, y una casilla para encender o apagar
  todos sus mods. Al pulsar un pack se va a la pestaña Mods filtrada, «Mods (<nombre del pack>)».
- **Mods**: a la izquierda la lista (imagen, casilla, nombre; `*` = hace falta reiniciar; en rojo los bloqueados),
  agrupada por categoría, con los módulos sangrados bajo su mod y un buscador. Sin filtro salen los mods sueltos; con
  filtro, los del pack (y «< All loose mods» para volver). A la derecha, el mod elegido: imagen grande, nombre, id,
  versión, autores, categoría, pack, estado (activo / apagado / **bloqueado: por qué** / esperando a su padre),
  descripción, **ajustes**, lo que necesita (con su estado), lo que ofrece a otros mods y su carpeta. Debajo, las
  carpetas de `Mods/` que no se pudieron leer, con el error.
- **Load order**: el orden de carga resuelto (de arriba abajo; con dos mods cambiando lo mismo gana el de abajo), con
  el estado y de quién depende cada uno, y flechas para subir o bajar un mod. Las dependencias siempre mandan. Se
  guarda en `Mods/load_order.cfg`.
- **Reinicio**: al encender, apagar o cambiar un mod que necesita reiniciar (`*`), la ventana pregunta «Restart
  needed» con **Restart openblack now** / **Later**, y mientras quede alguno pendiente muestra arriba «Takes effect
  after a restart: …» con el mismo botón. Reiniciar cierra openblack como «Quit» y lo vuelve a abrir con la misma línea
  de comandos (`Mods/Restart.*`, `main.cpp`); la ventana está en inglés.
- **Log**: los mensajes de la librería de mods (mods encontrados, errores de manifiesto, bloqueos, conflictos de
  reemplazos, errores de Lua y de los DLL, y lo que los mods escriben), con filtro por nivel y por mod.

### Carpeta Mods

Todo está en `Mods/` junto al ejecutable:

```
Mods/
  graphics.msaa/settings.cfg              un mod que viene con openblack (su mod.json va dentro del exe)
  world.foliage/foliage.cfg, *.png ...     sus archivos, y su settings.cfg
  mi.mod/mod.json, icon.png, ...           un mod suelto
  mi.pack/modpack.json, icon.png           un modpack...
  mi.pack/mi.pack.uno/mod.json             ...con sus mods dentro
  load_order.cfg                           el orden de carga del usuario
```

- Cada mod guarda su estado en su `settings.cfg` (lo escribe openblack al arrancar si falta y al cambiarlo en la
  ventana; un `settings.cfg` que ya existe **manda** sobre los valores por defecto del `mod.json`):
  ```
  # Anti-aliasing (MSAA) (graphics.msaa). For one session only: --mod graphics.msaa[=off], --mod graphics.msaa.<option>=<choice>
  enabled = on
  samples = 4x  # Samples: 2x, 4x, 8x, 16x
  ```
- Los mods que vienen con openblack tienen su `mod.json` compilado dentro del exe (`assets/mods/<id>/mod.json`), así
  que existen aunque su carpeta solo tenga el `settings.cfg`; un `mod.json` en la carpeta con el mismo id lo sustituye.
  El build copia `assets/mods` y `mods/examples` junto al exe (`bin/<config>/Mods`), sin tocar los `settings.cfg`.
- El antiguo `mods.cfg` único se reparte solo en los `settings.cfg` y se borra (`ModRegistry::ImportLegacySettings`).

### Línea de comandos

- Solo para esa sesión (no se guarda): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`, `--mod "game.skip-intro.free start=off"`.
- Atajos antiguos (`src/main.cpp`): `--msaa N`, `--mipmaps`, `--anisotropic`, `--enhanced-graphics` (= MSAA 4× +
  anisótropo), `--living-water`, `--ground-static-objects`.

## Para crear mods

La forma rápida: copiar una carpeta del [modpack examples](#modpack-examples) (`Mods/examples/`), cambiarle el id y
el nombre, y editar. Hay uno de cada tipo.

### Estructura de un mod

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

| Campo | Tipo | Qué es |
|---|---|---|
| `schema` | número | versión del formato (1) |
| `id` | texto | identificador estable, igual que la carpeta (`world.foliage`) |
| `name`, `description` | texto o `{"en": …, "es": …}` | nombre y descripción (por idioma) |
| `version` | texto | versión semver, `"1.2.0"` |
| `category` | texto | sección de la ventana (`Graphics`, `World`, `Game`…) |
| `authors` | lista de textos | |
| `icon` | ruta | su imagen (por defecto `icon.png` si existe) |
| `url` | texto | página del mod (opcional) |
| `api` | rango | versión de la API de mods para la que se hizo, `">=1.0 <2.0"` (hoy openblack ofrece la 1.0.0) |
| `enabled_by_default` | sí/no | encendido la primera vez (solo si el usuario lo pide; lo normal es `false`) |
| `restart_required` | sí/no | sus cambios cuentan al reiniciar |
| `parent` | id | módulo de otro mod: sale debajo y solo cuenta si el padre está activo |
| `dependencies` / `optional` / `incompatible` | `{"id": "rango"}` | ver [Dependencias](#dependencias-y-orden-de-carga) |
| `load_after` / `load_before` | lista de ids | pistas de orden (el otro no tiene que existir) |
| `provides` | lista | interfaces que ofrece a otros mods (`"foliage.v1"`) |
| `entry` | `{"lua": ruta, "native": {"windows": ruta, "linux": ruta}}` | su código |
| `switches` | `{"interruptor": valor}` | interruptores que pone mientras está activo |
| `options` | lista | sus ajustes ([Opciones](#opciones-e-interruptores)) |
| `replace` | objeto | lo que sustituye ([Reemplazar](#reemplazar-mallas-texturas-objetos-y-archivos)) |

Se admiten comentarios `//` en el JSON. Un error de formato deja el mod fuera (sale en la pestaña Mods, «Could not be
read», y en el Log); un interruptor o un valor desconocido solo quita esa parte, con un aviso.

### Opciones e interruptores

El motor nunca decide por su cuenta: todo lo que no es original lee un **interruptor** de `EngineConfig`, y los mods
ponen interruptores por su nombre ([lista](#interruptores-del-motor)). Un `mod.json` no necesita código para eso:

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

- `type`: `choice` (lista, por defecto), `slider` (deslizador sobre los valores) o `bool` (valores `on` / `off`).
- `values` y `default` (el valor o su número de orden); `label` y `description` por idioma.
- `bind`: por cada interruptor, el valor de cada elección (las que no salen dejan el valor por defecto); o un nombre de
  interruptor solo: en `bool` vale 1 con `on`, y en las demás el número que haya en el texto de la elección
  (`"x10"` → 10, `"4x"` → 4, `"10s"` → 10).
- Con el mod apagado o bloqueado sus interruptores vuelven al valor por defecto (el del original). Con dos mods
  poniendo el mismo, gana el que va después en el orden de carga.
- Cada interruptor dice cuándo cuenta (`live` al momento, `map` al cargar una tierra, `restart` al reiniciar): si el
  mod tiene alguno de reinicio, pon `"restart_required": true`.

### Reemplazar mallas, texturas, objetos y archivos

```json
"replace": {
  "meshes":   { "AnimalBat1": "meshes/bat.l3d", "#12": "meshes/otro.l3d" },
  "textures": { "pack:47": "textures/47.png", "raw:ATMOS": "textures/atmos.png" },
  "objects":  { "tree": { "Beech": { "woodValue": 500, "normal": "TreeBeech" } },
                "feature": { "Rock1": { "weight": 50 } } }
}
```

- **meshes**: una malla de `AllMeshes.g3d` por su nombre (la [enumeración `meshes`](#enumeraciones), sin mayúsculas
  que importen) o `#<número>`, cambiada por un `.l3d` (o `.zzz`) del mod. Se carga en lugar de la del pack (la caché
  de recursos guarda la primera carga, `Game::Initialize`) por el mismo `L3DLoader`, así que hereda lo que el motor
  aplica después a esa malla por su id: p. ej. la burbuja (`O_Bibble_up`) y las bandas de power-up
  (`Power_Up_Band`) quedan con el material aditivo sin Z del original, y los modos de render de `render_modes` (nota
  de la sesión sistemas). Un mod que quiera otro material para esas tendrá que pedirlo cuando el SDK lo ofrezca.
- **textures**: `pack:<id hex>` una textura de `AllMeshes.g3d` (los ids de HD-Tweaks, `textures.cfg`) por un PNG;
  `raw:<nombre>` un `Data/Textures/<nombre>.raw` por un PNG o un `.raw` (si el juego no lo tiene, se añade).
- **objects**: propiedades de los objetos de `info.dat` por tabla y por su nombre de depuración (`debugString`; en
  `abode` también `<TRIBU>_<nombre>`, como los guiones, `GAbodeInfo::GetInfoFromText` 0x405A70: el nombre solo cambia
  el edificio de todas las tribus): tablas
  `feature`, `abode`, `mobileStatic`, `mobileObject`, `pot`, `tree`, `animatedStatic`, `animal`, `bigForest`,
  `fieldType`; campos comunes (`foodValue`, `woodValue`, `weight`, `heatCapacity`, `combustionTemperature`,
  `sacrificeValue`, `impressiveValue`, `drawImportance`, los `defenceEffect*` / `defenceMultiplier*`, los
  `canCreature*`…) y de malla o escala donde la tabla los tiene (`meshId`, `normal`, `growing`, `burning`, `high`,
  `std`, `low`, `startScale`, `finalScale`; una malla por nombre o número). También `"objects": "data/objects.json"`
  con lo mismo en un archivo. Se aplica a `info.dat` antes de publicarlo (`Game.cpp`, tras `InfoFile::LoadFromFile`).
- **replace/**: cualquier archivo del juego con su misma ruta dentro de la carpeta `replace/` del mod (`replace/Data/
  Sky.raw`, `replace/Scripts/Land1.txt`…) lo sustituye; los que solo tiene el mod también se ven
  (`FileSystemInterface::AddOverridePath`, en orden de carga: gana el último).
- Todo esto se lee **al arrancar** (`mods::replace::Collect`): los mods con `replace` deben llevar
  `"restart_required": true`. Si dos mods sustituyen lo mismo, gana el último y el Log lo dice.

### Mods Lua

`"entry": {"lua": "scripts/main.lua"}`. El script corre una vez al arrancar el motor (antes de la primera tierra), en
un **entorno propio** por mod: sin `io`, `os` (salvo `os.time`, `os.clock`, `os.date`), `package`, `debug`, `load` ni
`dofile`; `string.dump`; las librerías `string`, `table`, `math`, `utf8` y `coroutine` son copias propias de cada mod;
`require("a.b")` carga `scripts/a/b.lua` del mismo mod (sin rutas, unidades ni `..`); `print` escribe en el Log; solo
se ejecuta código fuente, nunca Lua precompilado). Un error de un script se apunta en el Log y nunca para el juego;
tras 10 errores se quitan sus funciones de eventos, y una llamada que pase de unos 20 millones de instrucciones se
corta (reglas del anfitrión, no del original). Un mod apagado o bloqueado no recibe eventos, y como el script se carga
al arrancar, un mod con `entry` o `replace` es siempre de reinicio. No hay que cambiar la metatabla de las cadenas
(`getmetatable("")`): es la única tabla que comparten todos los mods. Tabla `ob`:

| Función | Qué hace |
|---|---|
| `ob.log.info(t)`, `.warn(t)`, `.error(t)` | escribe en el Log |
| `ob.mod.id`, `.name`, `.version`, `.folder`, `ob.mod.option(id)` | el mod y la elección de una opción |
| `ob.switch.get(nombre)`, `ob.switch.set(nombre, valor)`, `ob.switch.list()` | interruptores (los que pone un script cuentan mientras el mod está activo) |
| `ob.on("turn" \| "frame" \| "land_loaded", función)` | eventos: el número de turno, los segundos del fotograma, el nombre de la tierra |
| `ob.interfaces.provide(nombre, tabla)`, `ob.interfaces.get(nombre)` | [mods librería](#mods-librería) |
| `ob.enums.meshes`, `ob.enums.magic`…, `ob.enum(nombre)` | [enumeraciones](#enumeraciones) como tablas nombre → número |
| `ob.game.turn()`, `ob.game.hour()` | turno y hora del reloj de la tierra |
| `ob.game.ground_height(x, z)` | altura del terreno (nil sin tierra) |
| `ob.game.camera()`, `ob.game.set_camera(x, y, z, fx, fy, fz)` | la cámara (posición y foco) |
| `ob.game.cast_miracle(nombre, x, z [, radio, segundos])` | un milagro en el suelo, del jugador neutral, por el camino de `SPELL_AT_POS` |

### Mods nativos (DLL)

`"entry": {"native": {"windows": "bin/<id>.dll", "linux": "bin/<id>.so"}}`. Una librería en C (o en cualquier lenguaje
que haga una librería C) que incluye **una sola cabecera**, `components/modsdk/include/openblack/mod_api.h`, y no
enlaza nada de openblack: el motor le pasa sus funciones al cargarla (`SDL_LoadObject`). Exporta:

```c
OB_MOD_EXPORT const ob_mod_info* ob_mod_query(void);           // versión de API e id, sin efectos
OB_MOD_EXPORT int32_t ob_mod_load(const ob_host_api* host, ob_mod* self);  // 0 = bien
OB_MOD_EXPORT void ob_mod_unload(void);                         // opcional
```

- openblack comprueba `ob_mod_query` (misma versión mayor de API, mismo id que su `mod.json`) antes de ejecutar nada
  más de la librería.
- `ob_host_api`: `log`, `get_option`, `set_switch`, `get_switch`, `on_event` (`OB_EVENT_TURN`, `_FRAME`,
  `_LAND_LOADED`), `provide_interface`, `get_interface`, `enumeration`, `game_turn`, `game_hour`, `ground_height`,
  `camera`, `set_camera`, `cast_miracle`, `land_name`. Empieza por su tamaño: las funciones nuevas solo se añaden al
  final (`OB_HOST_HAS(host, función)` para saber si el openblack que corre la tiene).
- Reglas: todo en el hilo del juego; ninguna excepción C++ sale de la librería; los textos que da openblack valen
  durante la llamada, los que se le piden van a un búfer del mod. Un mod nativo no se puede aislar como uno Lua: solo
  hay que instalar los de confianza.
- Compilar uno en el repo: `openblack_add_native_mod(<target> <id> <carpeta> <fuentes>)` en `mods/CMakeLists.txt`
  (lo deja en `Mods/<carpeta>/bin`).

### Mods librería

Un mod que no cambia nada por sí mismo y ofrece funciones a otros, como las librerías de mods de Minecraft:

- Lo declara en `"provides": ["<nombre>.v1"]` y lo publica al cargar: en Lua `ob.interfaces.provide("x.v1", tabla)`;
  en C `host->provide_interface(self, "x.v1", &tabla, sizeof tabla)` con una tabla de punteros a funciones que empieza
  por su tamaño (y una cabecera pública `include/x_v1.h` para quien la use).
- Quien la usa la pone en `"dependencies"` (así carga después y se bloquea si falta) y la pide:
  `ob.interfaces.get("x.v1")` / `host->get_interface("x.v1", sizeof(x_v1))`.
- Una interfaz solo crece al final; un cambio incompatible es otro nombre (`x.v2`). Las tablas de Lua son para mods Lua
  y las nativas para mods nativos.
- Ejemplos: `example.lua-library` + `example.lua-consumer`, `example.native-library` + `example.native-consumer`.

### Modpacks

Una carpeta de `Mods/` con un **`modpack.json`** (`schema`, `id`, `name`, `version`, `category`, `description`,
`authors`, `icon`, como un `mod.json`) y sus mods dentro, cada uno en su subcarpeta con su `mod.json`. Los ids de los
mods son globales. La casilla del pack enciende o apaga todos sus mods; cada uno se ajusta por separado. Ejemplo:
[examples](#modpack-examples).

### Dependencias y orden de carga

- `"dependencies": {"lib.x": "^1.2"}`: hace falta, encendido y en ese rango; si no, este mod se **bloquea** (sale en rojo
  con el porqué: «needs lib.x ^1.2, found 1.0.0», «needs X, which is off»…), y lo que depende de él también.
- `"optional"`: si está, carga antes; si no, nada. `"incompatible"`: este mod se bloquea mientras el otro esté activo.
- `"api"` fuera del rango de openblack → bloqueado.
- Rangos: `*`, `1.2.3` / `=1.2.3`, `>`, `>=`, `<`, `<=`, `^1.2` (misma mayor, al menos 1.2), `~1.2` (misma mayor y
  menor), varios separados por espacios (`">=1.0 <2.0"`).
- Orden: primero lo que cada mod necesita (dependencias, opcionales presentes, `load_after`, `load_before`, el padre),
  luego el orden del usuario (`Mods/load_order.cfg`) y luego el id. Un círculo de dependencias carga por id y lo dice
  el Log.

### Carpetas antiguas (mod.cfg)

Se siguen leyendo, traducidas al formato nuevo:

- Una carpeta con `mod.cfg` sin `module_of` es un **mod de datos** `data.<carpeta>`: sus archivos con la ruta del juego
  lo sustituyen (como `replace/` de un `mod.json`), con reinicio.
- Con `module_of = <id>` es un **módulo** de ese mod: `name`, `description` y opciones
  `option.<id> = <etiqueta> | <opción>, <opción>... | <por defecto> [| slider]`.
- Un `mod.json` en la carpeta manda sobre su `mod.cfg`.

## Referencia

### Interruptores del motor

La tabla está en `src/Mods/EngineSwitches.cpp` (cada uno un campo de `EngineConfig`; los campos y quién los lee no
cambian). Valor por defecto = el original.

| Interruptor | Tipo | Cuándo | Qué hace |
|---|---|---|---|
| `graphics.msaa.samples` | int 0-16 | live | MSAA del búfer (0 = el original); al cambiar se rehace el búfer |
| `graphics.mipmaps` | bool | restart | mipmaps y filtrado trilineal |
| `graphics.anisotropic` | bool | restart | filtrado anisótropo |
| `graphics.smooth-smoke` | bool | restart | `smokea.raw` con su alfa de 8 bits (sin el corte ARGB4444 del original) |
| `graphics.terrain.upscale` | bool | map | texturas del terreno ampliadas x2 (Lanczos-3) |
| `graphics.terrain.repeat` | float 1-4 | map | repeticiones de la textura del terreno por bloque |
| `graphics.terrain.triplanar` | bool | map | acantilados con la textura de lado |
| `graphics.hd-tweaks.textures` | bool | live | texturas HD de aldeanos y animales |
| `graphics.hd-tweaks.smooth` | int 0-3 | live | nivel de redondeo PN (0 = no) |
| `graphics.hd-tweaks.lighting` | int 0-1 | live | 1 = luz por píxel |
| `graphics.hd-tweaks.mip-bias` | float -4-0 | live | sesgo de mip (negativo = más nítido) |
| `graphics.hd-tweaks.high-detail` | bool | live | mallas de alto detalle |
| `water.living` | bool | live | el mar lo refleja todo y ondula |
| `world.ground-statics` | bool | live | baja al suelo los estáticos que flotan (mueve también los que ya existen) |
| `world.foliage.density` | float 0-8 | live | plantas por celda (0 = sin hierba) |
| `world.foliage.distance` | float 50-1000 | live | distancia de dibujo de la hierba |
| `world.foliage.fields` | bool | live | campos como plantas que crecen |
| `world.crops.without-farmers` | bool | live | campos que se siembran solos |
| `world.crops.growth` | float 1-100 | live | velocidad de crecimiento |
| `game.skip-tutorial` | int 0-3 | restart | respuesta al SkipBox (0 jugar todo … 3 sin el claro) |
| `game.free-start` | bool | map | **no original**: el principio de la tierra no mueve la cámara ni bloquea |
| `test.dispensers` | bool | map | dispensadores de prueba junto al templo |
| `test.dispensers.level` | int 0-3 | map | su nivel |
| `test.dispensers.seconds` | float 1-600 | live | su recarga |
| `test.dispensers.seed` | bool | live | bola de fuego en la mano al empezar |

Añadir uno: el campo en `EngineConfig` (apagado = el original), leerlo en el motor y una línea en `EngineSwitches.cpp`.

### Enumeraciones

`ob.enums.<nombre>` (Lua) y `host->enumeration("<nombre>", i, …)` (C): `meshes` (los 626 nombres de `k_MeshNames`),
`magic` (los `MagicType` por el nombre de su efecto en `info.dat`, tras cargar los datos), `object_tables` y
`object_fields` (lo que `replace.objects` admite), `switches`.

### API: JSON, Lua y C

Una sola implementación, `src/Mods/Api.h`; JSON, Lua (`Mods/Lua/LuaHost.cpp`) y C (`Mods/Native/NativeHost.cpp`) son
traducciones de ella. Solo usa la API pública de cada área, acordada con su dueño: altura `LandIsland`, milagros
`magic::script::CastSpellAtPos` (por las reglas del juego, como `SPELL_AT_POS`, con la comprobación de la clase; el
«desde» 30 m sobre el punto, como `OPENBLACK_TEST_SPELL` **(inferido)**), cámara, reloj e interruptores. Pendiente:
sonido (solo `src/Audio/Audio.h`, con un dueño por mod; acordado con la sesión audio).

## Cómo está hecho (src/Mods)

| Archivo | Qué |
|---|---|
| `Mod.h` | `Mod` (Info, opciones, estado, bloqueo), `Modpack`, `Dependency` |
| `Manifest.*` | lee `mod.json` / `modpack.json` (nlohmann-json); `PackageMod`: opciones atadas a interruptores |
| `Semver.*` | versiones y rangos |
| `Switches.*`, `EngineSwitches.cpp` | registro de interruptores con nombre y la tabla de `EngineConfig` |
| `ModRegistry.*` | descubrir carpetas, ajustes, dependencias, orden de carga, aplicar, modpacks, montar `replace/` |
| `BuiltinManifests.h` | los `mod.json` de `assets/mods` compilados en el exe (generado por `src/CMakeLists.txt`) |
| `Replacements.*` | `replace`: mallas, texturas, objetos de `info.dat`, carpetas `replace/` |
| `Api.*` | las funciones simplificadas |
| `Lua/LuaHost.*` | mods Lua (Lua 5.4 + sol2) |
| `Native/NativeHost.*` | mods nativos; la cabecera C en `components/modsdk/include/openblack/mod_api.h` |
| `ModLog.*` | los mensajes de la pestaña Log |
| `Debug/ModsWindow.*` | la ventana Mods |

Arranque (`Game::Game`): `switches::RegisterEngineSwitches` → `ModRegistry::Discover(<exe>/Mods)` → legacy →
`LoadSettings` → `--mod` → `ApplyAll` (resolver, interruptores, `Apply`) → `replace::Collect`. `Game::Initialize`
monta `replace/` y los mods de datos, y carga mallas, texturas e `info.dat` con los reemplazos. `Game::Run` arranca Lua
y los nativos antes de la primera tierra; `land_loaded` al final de `LoadMap`, `turn` al final de cada turno, `frame`
en cada `Update`. Tests: `test/test_mods.cpp`.

## Catálogo de mods

Los que vienen con openblack (`assets/mods/<id>/mod.json`, sin código propio: solo opciones atadas a
[interruptores](#interruptores-del-motor); antes eran clases C++ en `src/Mods/Builtin/`, con los mismos ids, opciones y
valores, comprobado en `test_mods` `BuiltinModsSetTheOldValues`):

| Id | Opciones (por defecto en negrita) | Resumen | Reinicio |
|---|---|---|---|
| [`graphics.msaa`](#graphicsmsaa) | `samples` 2x/**4x**/8x/16x | Antialiasing multimuestreo | no |
| [`graphics.mipmaps`](#graphicsmipmaps) | — | Mipmaps y filtrado trilineal | sí |
| [`graphics.anisotropic`](#graphicsanisotropic) | — | Filtrado anisótropo (incluye los mipmaps) | sí |
| [`graphics.terrain-x2`](#graphicsterrain-x2) | `repeat` x1/**x2**/x3/x4, `upscale` **off**/on, `cliffs` **triplanar**/stretched | Terreno y mar más nítidos | sí |
| [`graphics.smooth-smoke`](#graphicssmooth-smoke) | — | Todo lo que usa `smokea.raw` (humo, nubes, nieblas, anillos de agua, bocanadas de barco, brillo de las luces nocturnas) con el alfa de 8 bits (sin el corte a 16 niveles) | sí |
| [`graphics.hd-tweaks`](#graphicshd-tweaks) | `textures` **hd**/original, `smooth` off/soft/**round**, `light` **smooth**/original, `sharp` **on**/off, `detail` **high**/original | Aldeanos, animales y mano mejor vistos | no |
| [`water.living`](#waterliving) | — | Mar que refleja todo y deriva | no |
| [`world.ground-statics`](#worldground-statics) | — | Baja al suelo los estáticos que flotan | no |
| [`world.crops`](#worldcrops) | `speed` **x1**/x2/x5/x10/x20/x50/x100 (deslizador) | Campos que se siembran solos (apagado ya no deja su velocidad puesta: la clase C++ antigua la ponía aunque estuviera apagado, un fallo de fidelidad) | no |
| [`world.foliage`](#worldfoliage) | `density` low/**medium**/high/very high, `distance` near/**medium**/far, `fields` **wheat**/original | Hierba, flores, juncos, matorrales y trigo | no |
| [`world.foliage.beach`](#módulo-worldfoliagebeach) | `density` very low…**medium**…very high | Módulo: playa | no |
| [`world.foliage.butterflies`](#módulo-worldfoliagebutterflies) | — | Módulo: mariposas | no |
| [`test.miracle-dispensers`](#testmiracle-dispensers) | `level` **base**/pu1/pu2/all, `recharge` 2s/5s/**10s**/20s/30s/60s, `seed` **on**/off | Dispensadores de milagros de prueba | no |
| [`game.skip-intro`](#gameskip-intro) (**activado por defecto**) | `skip` tutorial/tutorial and creature training/**tutorial, creature training and the glade**, `free start` **on**/off | Empieza Land 1 sin la intro | sí |

### graphics.msaa

- Opción `samples` 2x/4x/8x/16x (por defecto 4x). Sin reinicio.
- Antialiasing multimuestreo y alpha to coverage en hojas y vallas (y en las plantas de `world.foliage`).
- Atajo `--msaa 0/2/4/8/16`. Backbuffer multimuestreado (`BGFX_RESET_MSAA_*`). En las pasadas opacas los cut-outs
  usan **alpha to coverage**: `fs_object` convierte el corte en una rampa de ~1 píxel con `fwidth`
  (`u_skyAlphaThreshold.z`).

### graphics.mipmaps

- Sin opciones. Hace falta reiniciar.
- Mipmaps y filtrado trilineal. Atajo `--mipmaps`. Se aplica a las texturas de modelos, pieles L3D, materiales y
  bump del terreno y texturas `.raw` sueltas (el original no tiene mips: ver
  [rendering.md](rendering.md#estados-de-direct3d-7-del-original)).
- Implementación (`Graphics/TextureMipmaps.cpp`, `BuildRgba8MipChain`):
  - decodifica el nivel 0 a RGBA8 con `bimg::imageDecodeToRgba8` (DXT1/3/5, BGRA4, BGR5A1, R8…);
  - hace una media 2×2 **ponderada por alfa**, para que los texels transparentes no oscurezcan los bordes;
  - en texturas de alfa casi binaria (≥85 % de texels con alfa <32 o >223) **conserva la cobertura** en cada nivel
    respecto a la referencia 0x96, para que los árboles no adelgacen a lo lejos (sin esto se veían mucho más finos).
- `Texture2D::Create`: con `Filter::LinearMipmapLinear` construye la cadena y crea la textura en RGBA8 con mips.
  Libera el `bgfx::Memory` original con `bgfx::release`, que bgfx exporta pero no declara en `bgfx.h`.
- `graphics::SurfaceTextureFilter()` devuelve `Linear` o `LinearMipmapLinear` según los mods. No se aplica al
  heightmap, las huellas, el ruido ni el cielo.
- `fs_terrain`: el small bump se muestrea fuera del `if` de distancia, porque con mips hacen falta derivadas en flujo
  uniforme.
- Coste: unos segundos más de carga y más memoria de vídeo (RGBA8 en lugar de DXT).
- Verificación (de `msaa`, `mipmaps` y `anisotropic`): capturas (estaban en `dev\gfx\`, borradas en la limpieza del
  2026-09-30; se regeneran con estas cámaras y opciones):
  - `base_*` frente a `enh_*` / `enh2_*`: aldea `1818,75,2612,1824,44,2636` y panorámica
    `1600,160,2350,1900,40,2750`, con `-n 14000 --screenshot-frame 13900`. Con mips la carga es más lenta y a 8000
    fotogramas el vuelo aún no ha terminado.
  - [img/crop_trees_zoom.png](img/crop_trees_zoom.png), rejilla de cuatro: original, mips, MSAA y todo.

### graphics.anisotropic

- Sin opciones. Hace falta reiniciar.
- Filtrado anisótropo (incluye los mipmaps). Atajo `--anisotropic`: añade `BGFX_SAMPLER_*_ANISOTROPIC` y
  `BGFX_RESET_MAXANISOTROPY`. `--enhanced-graphics` equivale a `--msaa 4 --anisotropic`.

### graphics.terrain-x2

Opciones `repeat` x1/x2/x3/x4, `upscale` off/on, `cliffs` triplanar/stretched. Hace falta reiniciar.

- **Repetición** (`repeat`): terreno más nítido, cada material repetido 1-4 veces por bloque (por defecto x2; wrap
  Repeat). Solo escalar apenas se nota: cada material de 256 px cubre un bloque de 160 unidades.
- **Escalado** (`upscale`): ×2 con Lanczos-3 al cargar (`Graphics/TextureUpscale`, con wrap: los materiales del LND
  son tileables, primera y última fila/columna idénticas).
- **Acantilados** (`cliffs`): el original proyecta todo desde arriba (uv = posición xz del bloque) y en las pendientes
  la textura se estira en rayas; con `triplanar` se mezclan también las proyecciones a lo largo de x y z (pesos
  \|n\|⁴, normal suave por vértice de diferencias centrales de altitud, `LandVertex::normal`).
- **Materiales dibujo**: los materiales que son un dibujo único por bloque y no una textura (el geoglifo de la figura:
  Land1 material 10 y Land5 material 5; el laberinto: Land5 material 1) se quedan en ×1. Nada en el LND los marca (su
  `type` 18/11 lo comparten hierbas normales, y la métrica de contraste a gran escala no los separa de una roca
  nevada), así que se reconocen por hash FNV-1a de sus texels (`IsPictureMaterial` en LandIsland.cpp) y viajan en el
  byte `w` de los ids de material del vértice (bits 0-2). Si un mod de datos trae otro dibujo, hay que añadir su hash
  (`dev\tools\lnd\lnd_hash.py`).
- **El mar** también: su periodo de repetición (560 a nivel de detalle 4) se divide por las repeticiones, la
  ondulación por filas del original se divide igual (si no, mueve la textura el triple y deja bandas) y con `upscale`
  `sky.raw`/`skya.raw` se escalan ×2 con Lanczos al cargarse (`Texture2DLoader`, que además copia los datos: antes
  pasaba a bgfx una referencia a un vector local). Después del escalado se cortan a ARGB4444 como en el original
  ([rendering.md](rendering.md#texturas-argb4444)).

### graphics.smooth-smoke

- Sin opciones. Hace falta reiniciar.
- `smokea.raw` conserva sus 8 bits de alfa en todo lo que lo usa: humo de chimeneas, nubes, nieblas, anillos de agua,
  bocanadas de barco y el brillo de las luces nocturnas (`NightLights`). El original lo corta a 16 niveles (ARGB4444,
  `fn_00837400`; ver [rendering.md](rendering.md#texturas-argb4444)), por ejemplo 228 → 238/255.
- Implementación: `assets/mods/graphics.smooth-smoke/mod.json` pone el interruptor `graphics.smooth-smoke` (`EngineConfig::smoothSmokeAlpha`, reinicio), y `Texture2DLoader` se
  salta el corte de `smokea`. Era el aspecto de openblack antes de que existiera el corte al cargar.

### graphics.hd-tweaks

"HD-Tweaks" (antes `graphics.hd-people`, renombrado 2026-09-30). Opciones `textures` hd/original, `smooth`
off/soft/round, `light` smooth/original, `sharp` on/off, `detail` high/original. Sin reinicio: todo en vivo. Aldeanos,
animales y mano mejor vistos. Sección completa (paquete, pruebas, estado) en [mods.md](mods.md#mod-hd-tweaks).

- **Texturas** (`textures`): los atlas de 256² (4 aldeanos cada uno, unos 30 px por cara) sustituidos por imágenes ×4
  de Real-ESRGAN (`Mods/graphics.hd-tweaks/textures/<id>.png` + `textures.cfg`; `Resources/HdTextures`,
  `Texture2DLoader::FromImageTag`, siempre con mipmaps). Cada imagen lleva el hash FNV-1a del DDS del que salió: con
  otro AllMeshes.g3d no se usa.
- **Animales** (2026-09-30): sus 5 atlas en HD, así que también se suavizan y usan la luz por píxel y `sharp`.
- **Mano**: también se suaviza (`L3DSubMesh::IsHdTweaked`, malla `Hand_Boned_Base2`) y usa la luz por píxel; se
  recarga en vivo con las demás.
- **Formas** (`smooth`): las mallas con huesos cuyas texturas son todas de esa lista (los nombres de malla de openblack
  van desplazados respecto al paquete del usuario) pasan a triángulos PN curvos (`3D/PnTessellation`, Vlachos 2001)
  partidos en 4 (`soft`) o 9 (`round`); los vértices se sueldan por posición en la pose de reposo (normal media) y cada
  vértice nuevo vuelve al hueso de la esquina más cercana, así que sirve con animaciones rígidas. La colisión (mano,
  físicas) sigue siendo la malla original.
- **Formas y animaciones**: los triángulos de articulación (esquinas en huesos distintos) ya no se curvan por dentro
  (se doblaban al animar): abanico sobre su arista de un solo hueso, se estiran como los del original.
- **En vivo** (2026-09-30): al cambiar el mod o sus opciones en el menú, `Resources/HdTweaks` (`hd_tweaks::Update`, al
  principio de `Game::Update`) relee AllMeshes.g3d y recarga solo las 18 texturas de aldeanos y las 113 mallas con
  huesos que las usan (~0,6 s al activar, ~0,15 s al desactivar; las PNG se decodifican en paralelo, también al
  arrancar). Gancho `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>`.
- **Visible a distancia de juego** (2026-09-30; a 20-40 m un aldeano mide 40-70 px y las texturas ×4 solas no se
  notan):
  - `light` = `smooth` (por defecto: la misma luz del original, la regla entera de `fn_0084BA90` con las funciones de
    `assets/shaders/model_light.sh`, pero por píxel en `fs_object` con las normales suaves y con la dirección tomada en
    el mundo, (aproximado): coincide solo con la luz lejos, de día; de noche, con la luz a 3 unidades de la mano,
    difiere de forma visible en un aldeano cercano; ver [Luz de los modelos](rendering-objects.md#luz-de-los-modelos)) u `original` (por
    vértice). Un borde de luz en la silueta (0,8·(1-N·V)²) se probó y quedaba feo (usuario, 2026-09-30).
  - `sharp` = sesgo de mip −1 en sus texturas.
  - `L3DSubMesh::IsPerson`, `u_window.y/z` (Renderer::DrawSubMesh, solo instancias iluminadas como el original, no
    reflejos ni la mano).
- **Detalle** (`detail`): `high` = aldeanos y animales con su malla de detalle alto (el original dibuja siempre la std,
  LOD 1; `ECS/DetailMeshes`, cambia la malla de los que ya existen al cambiar la opción).

### water.living

- Sin opciones ni reinicio.
- El mar refleja todo, el reflejo ondula despacio en bucle y la superficie deriva (sin la ondulación por filas).
- Atajo `--living-water`. "Agua viva": el reflejo del mar incluye modelos y sprites (el original solo refleja cielo y
  tierra) y ondula en bucle con dos capas de `skya.raw` que se desplazan (mapa de olas), más fuerte cerca y nula a
  1500 de profundidad; además quita la ondulación por filas del original (líneas fijas en pausa, temblor a fps
  modernos) y hace derivar `sky.raw` y `skya.raw` juntos (0,020 / 0,012 texturas por unidad de tiempo). Usa tiempo
  real a un cuarto de velocidad (también en pausa) que da la vuelta cada 1000 unidades (4000 s); las velocidades son
  múltiplos de 1/1000 textura/s, así el bucle no salta. El mar del original: [rendering.md](rendering.md#mar-skyraw--skyaraw).

### world.ground-statics

- Sin opciones ni reinicio.
- Baja las rocas y objetos estáticos que flotan hasta el suelo.

### world.crops

- Opción `speed` x1..x100 (x1, x2, x5, x10, x20, x50, x100), deslizador. Sin reinicio.
- Los campos se siembran solos y se vuelven a sembrar al cosecharlos, y crecen ese múltiplo más rápido.
- Sin él el motor es **fiel** al original: el constructor del campo lo deja vacío y solo los granjeros lo siembran, y
  openblack aún no tiene oficios, así que los campos se quedan vacíos.

### world.foliage

"Grass and flowers": hierba, flores, juncos y matorrales sobre el terreno (billboards instanciados, `3D/Foliage`;
voladores en `3D/FoliageFlyers.cpp`). Sin reinicio. Reglas e imágenes en `<exe>/Mods/world.foliage/` (`foliage.cfg`;
en el repo `assets/mods/world.foliage/`; imágenes originales del usuario en `B&W/Asstes_mods`).

**Opciones**

| Opción | Elecciones | Efecto |
|---|---|---|
| `density` | low/medium/high/very high = ×0.5/1/2/4 (por defecto medium) | Multiplica los `per_cell` |
| `distance` | near/medium/far = 120/200/320 (por defecto medium) | Distancia de dibujo (`foliageDistance`) |
| `fields` | wheat (por defecto) / `original` = la malla | [Campos de cultivo](#campos-de-cultivo) |

#### Especies: claves de foliage.cfg

- Una sección `[nombre]` por planta en `foliage.cfg`: `images` (png, uno al azar por planta), `texture` (aspecto de la
  textura: green/dry/sand/rock/snow), `terrain` (tipo del LND, `TerrainMaterialType`), `per_cell` (por celda de 10×10
  con densidad media), `size` (ancho mín-máx; el alto sale de la proporción de la imagen), `altitude`, `slope`
  (grados), `patches` (0 uniforme .. 1 solo en manchas, ruido de valor a escala 45), `sway` (viento), `lean`
  (inclinación máxima al azar) y `tint` (grey/all/none). Crece si cumple `texture` o `terrain`.
- `cross = on`: la especie se dibuja con los dos planos cruzados (los matorrales secos); en cada bloque esas
  instancias van al final (`Chunk::crossStart`) y se dibujan con los 12 índices del quad.
- Claves nuevas para las especies de los módulos (valen en cualquier `foliage.cfg`):
  - `flat = on`: la imagen va **tumbada en el suelo**, centrada en el punto, con lo alto de la imagen a lo largo del
    `side` del giro e inclinada como el suelo (pendiente a lo ancho y a lo largo en `i_data4.xy`, `i_data4.z = 2`). Se
    mezcla por su alfa sin escribir profundidad (las plantas la tapan igual) y se desvanece con la distancia en vez de
    encogerse. En cada bloque van al final (`Chunk::flatStart`). `lift` = altura sobre el suelo (0,04).
  - `coast = on`: puede estar en las celdas de costa o con agua (la `altitude` la deja fuera del agua).
  - `share = 0..1`: parte mínima del suelo dibujado en el punto que es de sus texturas (las cuatro esquinas por su peso
    bilineal y los dos materiales de cada una por su mezcla, como el shader). El material elegido al azar para el
    punto (una esquina y uno de sus dos materiales) puede ser arena aunque casi todo lo que se ve sea roca: con
    `share = 0.8` la playa solo sale donde casi todo es arena (el usuario veía manchas y huellas en suelo gris).
  - `opacity` (0-1, 1 por defecto): las planas se mezclan con esa opacidad (`i_data4.w`, que antes era 1 = mezclada; 0
    sigue siendo con alfa probado). Las huellas de la playa van a 0,45 y la arena mojada a 0,6.
  - `shade`: con `tint` all/grey, escala del color del suelo que toma (va en `i_data3.z` de las planas): la arena
    mojada (`tint = all`, `shade = 0.7`) es la arena de debajo, más oscura, en vez del naranja de la imagen.
- Tamaños: el 29-09-2026 todos los `size` se redujeron un 25 % (el usuario las veía muy grandes).
- Módulos: su `foliage.cfg` se lee después del del mod con el mismo parser; las imágenes se buscan junto a cada
  `foliage.cfg`, y un `.gif` animado da una capa por fotograma (`stbi_load_gif`; las plantas muestran el primero). Se
  recarga al encender o apagar un módulo (`Renderer::DrawFoliage`, `_foliageLoadKey`).

#### Aspecto del suelo: texture y terrain

- **El `type` del LND no describe el aspecto** (**fiel**, datos del LND): en Land1 las texturas 0 y 8 son hierba verde
  con tipo 5 `Earth` y la 11 es arena con tipo `Earth`; sirve para sonidos/pasos.
- Por eso `texture` clasifica cada material por su color medio (`Foliage::ClassifyTexture`, medido en Land1-5): verde
  = tono 50-100° y saturación ≥ 0,55; nieve = saturación < 0,15 y valor > 0,55; arena = valor ≥ 0,6; seca = tono
  < 50° y saturación ≥ 0,5; el resto roca (misma gama de tono que la hierba pero saturación 0,29-0,45). La isla
  expone tipo, "dibujo" y color medio con `LandIslandInterface::GetMaterialInfo`.

#### Zonas (biomas): zone y not_zone

- `zone` / `not_zone` filtran por la zona de ambiente de la celda, el código de sonido que el diseñador pintó en cada
  celda (`LNDCell::flags >> 1`, los impares > 8 cuentan como el par anterior; `Foliage::ZoneOf`). Los datos son
  **fiel** (zonas de sonido del LND); usarlas como biomas es **mod/propio**.
- Es lo único del LND que forma regiones limpias: los `country` son solo la paleta de texturas por altura y están
  muy fragmentados (Land1: 10 mezclados por todo el mapa).
- Zonas en la tierra de Land1-5: 14 pájaros (`meadow`, casi todo), 6 costa (franja junto al mar), 8 jungla (manchas
  compactas: Land1 noroeste ~1620,2290 y este ~2550,2550; Land5 5-6 manchas), 16 bosque (Land1 ~2160,3100), 10 viento
  = nieve y montaña (Land2 todo el suroeste, Land3, Land5 noreste), 4 olas lentas (`swamp`: charcas interiores, muchas
  en Land5) y 5 lago (Land2 centro). 12 desierto no lo usa ningún mapa original.
- Mapas en `dev\tmp_dis\biomes\Land*_snd.png` (`dev\tools\lnd\lnd_zones.py`; `dev\tools\lnd\lnd_countries.py` para
  los country).
- Uso actual: `water_plant` en jungla, lago y charcas; `jungle_grass` en la jungla; `wildflowers` en prado y bosque;
  `poppies` en prado; `dead_bush_barren` en viento/desierto (solo roca, tierra seca o arena). Todas con `tint = grey`.

#### Altura y agua

- **Altura** (29-09-2026, estudio en `dev\tmp_dis\heights`): `GetHeightAt` y `GetNormalAt` **aplanan** junto al mar
  (si la esquina base de la celda vale ≤ 4, las esquinas ≤ 3 cuentan como 0: lo que usan las físicas y vs_object),
  pero la malla del terreno que se dibuja no. Las plantas usan `GetUnflattenedHeightAt` y una normal por diferencias
  centrales de esa altura (`GroundNormal`): antes quedaban hasta 2 unidades bajo el suelo dibujado en la primera
  franja de tierra y toda la playa daba altura 0.
- Datos del LND (**fiel**): byte de altitud × 0,67; en Land1-5 las celdas con agua valen 0-1 (0-0,67, terreno
  transparente), las de costa siempre 2 (1,34, alfa 0,5) y la tierra opaca empieza en 3 (2,01); el máximo es 255
  (170,85). Por eso todas las `altitude` de las plantas pasan a `0-175` (los mínimos 1-2 ya no hacen falta: la costa
  está excluida; los máximos 120/150 cortaban los prados altos de Land3).
- **Agua**: el mar es el plano y = 0 (y es también el agua de los ríos, ver [rendering.md](rendering.md#ríos) "Ríos"); las
  celdas de costa (`coastLine`, altitud 2-3 en Land1) se dibujan con alfa 0,5 sobre el mar y las de agua con alfa 0,
  así que nada crece en una celda con alguna esquina de agua o costa.
- `near = lake, stream, sea` + `water_distance` limitan una planta a esa distancia de agua (mapa de distancias 3-4
  chamfer a 5 unidades, `FoliageWaterMap`): celda de agua = `sea_cells::IsWater` (bit 0x10; una celda sin bloque
  también es agua, MapCoords::IsWater 0x6035B0) o `fullWater`; lago = celdas de agua 4-conectadas que no llegan al borde del mapa (Land1:
  una charca de 10 celdas en x 2130-2160, z 2400-2450 y una celda suelta); río = segmentos entre los puntos de cada
  `Stream` (Land1: 11 ríos, 187 puntos). En B&W1 no hay agua a otra altura: los ríos son esos caminos (openblack los
  dibuja como el original desde 101dd844, `ECS/Rivers`, ver [rendering.md](rendering.md#ríos)).
- Los juncos usan `near = lake, stream` a 3-9 unidades. Ninguna planta a menos de 3 unidades de la línea de un río (el
  canal de river.l3d mide unas 4; distancia exacta a los tramos en cubos de 20 unidades).
- La base de cada planta sigue el suelo: altura en sus dos extremos (i_data4) y cizalla en el vertex shader, hundida
  un 6 %.

#### Colocación

- Determinista por bloque de terreno, **solo cerca de la cámara** (hasta 6 bloques por fotograma; se liberan al
  alejarse un bloque más allá): por celda y planta, `per_cell × densidad` candidatos; en cada punto se elige una
  esquina de la celda por su peso bilineal y uno de sus dos materiales por el coeficiente de mezcla (como el shader del
  terreno).
- Nada en celdas de agua, en materiales dibujo (geoglifo), fuera de la altura o pendiente, ni a menos de 1 unidad de
  entidades `Fixed` que no sean árboles, ni de campos, rocas móviles, pilas, almacén, templo o piscifactoría (caja de
  la malla).
- Todo se rehace al cambiar de isla o densidad y cuando existen los objetos.

#### Tinte por el suelo

- Los texeles grises (saturación < 0,1-0,2) toman el color de la textura del terreno bajo la planta: el vertex shader
  muestrea el array de materiales en el mismo material y uv que el terreno (uv del bloque × repeticiones del mod
  terrain-x2, mip 3); gris 0,5 = el suelo tal cual, más oscuro en la base y más claro en la punta. Los texeles de
  color (pétalos, espigas) no cambian. `tint = all` tinta toda la imagen; `none` usa sus colores.
- **Suelo oscuro o sin color**: el tinte toma la textura del material a baja resolución, y algunas tienen manchas muy
  oscuras (Land1 material 5, brezo, tipo 25: 46 % de sus texeles de 32×32 con brillo < 0,3) o grises (material 10
  un 13 % con saturación < 0,3), que daban plantas grises. `LandMaterialInfo::small` guarda cada material en 32×32
  (media de cajas de 8×8, como el mip 3 que muestrea el shader) y `ground_value` / `ground_saturation` filtran por el
  color de ese texel (mismo uv que el terreno, con las repeticiones de terrain-x2). Las plantas tintadas piden
  brillo ≥ 0,28 y saturación ≥ 0,3; `dead_bush_dark` / `dead_bush_grey` (con sus colores, `tint = none`) ocupan las
  manchas.

#### Dibujo

- Un plano por planta con orientación fija al azar (no mira a cámara) e inclinado al azar hasta `lean` para que se vea
  desde arriba (dos planos cruzados se veían como cruces desde arriba); hundido un 12 % de su alto para que no se vea
  el borde inferior.
- Las plantas se hunden en el último 20 % de la distancia (120/200/320).
- Luz = tabla de luz del terreno[luminosidad de la celda] y la misma neblina; alpha test con borde nítido (alpha to
  coverage con MSAA). Solo en la pasada principal (no en el reflejo).
- Capas de 256×512 apoyadas abajo, con mipmaps; el color de los texeles transparentes es la media de los opacos.

#### Sprites

- Los `mono_*` (salvo `dead_bush_dark` / `dead_bush_grey`, que usan `dead_bush_*.png` con sus colores): las imágenes
  del usuario (`B&W/Asstes_mods/Plants`, las de la v2; los `mono_*` actuales están en
  `B&W/BnW_openblack/Mods/world.foliage`) pasadas a gris con
  `assets/mods/world.foliage/tools/mono_sprites.py --width=128`:
  - hierba, hierba alta, matorrales y trigo con `--min-hue=0` (todo a gris);
  - juncos, plantas de agua y flores con `--min-hue=50 --open=1`: lo verde (tono 50-170°) a gris con media 0,62 y del
    resto solo quedan en color las manchas que sobreviven a una apertura morfológica de 3×3 (pétalos, cabezas de los
    juncos, penachos); las vetas finas amarillo-marrón y los brillos casi blancos de las hojas también a gris (con
    `--min-hue=50` sin apertura salían vetas naranjas sin tintar);
  - los brillos y bordes poco saturados (s <= 0,12, v < 0,85) también a gris y solo los casi blancos (v >= 0,85) con
    un toque crema para que no se tinten.
- Los `gen_*` generados por `tools/gen_grass_sprites.py` (en el repo) ya no se usan.
- La base de cada imagen se recorta irregular por columnas (hasta el 9 % del alto) para que no se vea el borde recto.

#### Campos de cultivo

Opción `fields` = wheat (por defecto); `original` = la malla.

- **Plantas**: la malla del campo se oculta (`Alpha` 0 en `ecs::UpdateFields`; sigue ahí para la mano y los instantes
  con alfa 0 ya no se dibujan: escribían profundidad) y `Foliage::UpdateFields` pone en su huella (caja de la malla con
  su giro y escala) una rejilla con ruido cada `[field] spacing` unidades. Cada fotograma, por campo al alcance: nada
  sin sembrar; la etapa `[field_stage ...]` según el crecimiento (0-1200) ± `stagger` al azar por planta (cambio
  gradual); ancho y tinte interpolados dentro de la etapa (el tinte sustituye al color del suelo: `i_data4.z` = 1, `w`
  = r·65536 + g·256 + b); solo quedan las plantas con `keep` < comida / comida esperada a ese crecimiento, así que la
  cosecha lo aclara. Instancias transitorias cada fotograma.
- Etapas actuales: brote (hierba baja, 0-80), hierba alta (80-350), trigo verde (350-750), trigo secándose hasta
  marrón maduro (750-1200). Gancho `OPENBLACK_TEST_FIELD_GROWTH=0..1200` (todos los campos empiezan con ese
  crecimiento y su comida).
- **Tierra de cultivo**: `[field] soil` (`field_soil.png`, de `tools/gen_field_soil.py`: surcos marrones con borde
  irregular que se desvanece) se pinta en la textura de huellas (`Foliage::DrawFieldFootprints`, desde
  `Renderer::DrawFootprintPass`, con el programa FootprintInstanced) sobre la caja del campo + `soil_margin` por lado.
- **De lejos**: la malla del campo vuelve con alfa `(d − 0,8·D) / (0,2·D)` (d = distancia a la cámara, D =
  `foliageDistance`), el mismo tramo en que las plantas se hunden en el suelo, sin hundirse con la comida (en el
  original un campo joven casi no se ve) y solo si está sembrado y con comida. Aparece **disolviéndose**: una trama
  de pantalla de cruces que crecen en celdas de 8×8 píxeles (fs_object, descarta en vez de mezclar).
- **Tinte de la malla** (`components::MeshTint`, puesto por `Foliage::UpdateFields`): como las plantas, sus texeles
  pasan a gris × color medio del suelo bajo el campo (`GroundColourAt`, 9 puntos, color medio de los materiales de
  la celda para su altitud), mezclado hacia su propio color según `[field] ripening` (350-1200). Viaja en el w de la
  tercera columna de la instancia: 1e6 (2e6 disolviendo) + 5 bits por canal del suelo y de `own`; el sombreador de
  mapa de alturas ya no suma ese w como desplazamiento si pasa de 500000 (los campos son MorphWithTerrain, así que
  pierden el desplazamiento de hundirse, que con el mod no usan). vs_object lo pasa a fs_object en `v_normal`
  (fs_object no ilumina con la normal): 1000 + 2·own en x y el color en las fracciones. Tras tocar vs_object hay que
  hacer `touch` de los vs_object_*instanced*.sc (openblack-internals.md).
- **Ojo**: la malla del campo (MSH_T_WHEAT) tiene huella propia, y `vs_footprint_instanced` usaba las columnas de la
  instancia enteras: el w del tinte (> 1e6) rompía la proyección y su huella tapaba toda la textura de huellas
  (terreno verde oliva liso, sin caminos ni huellas de edificios, solo con la malla del campo opaca, de lejos). Ahora
  ese sombreador toma solo xyz de las tres primeras columnas, como vs_object (le pasaba igual al alfa de una malla con
  huella que se desvanece).

#### Voladores: [flyer nombre]

- **`[flyer nombre]`** (`FoliageFlyers.cpp`): voladores sobre las plantas de las especies de `over` (por nombre, de
  cualquier `foliage.cfg`). Al colocar un bloque, cada planta de esas tiene una mariposa con probabilidad `per_plant`
  (`Chunk::homes`).
- **Vuelo**: cada fotograma, hasta 110 unidades de la cámara: vuela `flight` s en un lazo de dos senos por eje
  alrededor de su flor (radio `range`, altura `height` sobre la flor, aleteo de ±0,12 rad), despega de la flor y
  vuelve a ella, y luego se posa `rest` s aleteando a 1/4 de velocidad. Fotograma según los tiempos del gif (los de
  menos de 20 ms cuentan 100 ms, como los navegadores). Planas y con alpha test (escriben profundidad,
  `v_texcoord0.w = 5`), en instancias transitorias como las de los campos. Todo sale del tiempo real y de la semilla
  de la planta.
- **Huida de la mano**: solo guarda su huida (`FlyerHome::fleeTime/away/offset`), como los peces con un chapoteo
  (`FishShoals.cpp`) pero por cercanía: con la mano (`HandSystemInterface::GetPlayerHandPositions`) a menos de `flee`
  (5) en horizontal y de `2·flee` en altura sale disparada en línea recta lejos de ella 2 s, ×4 su velocidad
  (`0,6·range·speed`, mínimo 1) y frenando en el último segundo, subiendo `0,4` de lo que avanza; si la mano sigue
  cerca cuando frena, vuelve a salir. Luego regresa a su camino a su velocidad normal, mirando hacia él, y espera
  donde está mientras la mano siga a menos de `1,5·flee` (`OPENBLACK_HAND_TRACE=1` escribe `Flyer trace`).
- **Alas plegables** (`fold`, 0,7 por defecto): el gif se sigue reproduciendo (sus fotogramas cambian la pose de las
  alas, no solo el ancho) sobre un cuadrado partido por el cuerpo (`_foldQuad`, x = -0,5/0/0,5), y cada mitad sube
  girando sobre él `fold · acos(ancho del fotograma / el más ancho)` (`Animation::folds`, medido por el píxel opaco
  más alejado de la columna central), interpolando al del fotograma siguiente; en el shader `i_data4.z = 3`, `w` = el
  pliegue.
- **De día solamente** (`night = off`): con la hora del juego (`SkyInterface::GetTime`) la luz del día va de 0 a las
  20:00-5:30 a 1 a las 7:00-18:30 y cada mariposa se va cuando baja de su umbral al azar, así que desaparecen una a
  una.

### Módulo world.foliage.beach

- "Beach": algas, arena mojada, conchas, estrellas de mar, coral y huellas, todas `flat` y `coast` en arena
  (`texture = sand`, `terrain = Sand, WetSand`). Opción `density` (deslizador, very low..very high, por defecto
  medium; ver [Módulos con option.\<id\>](#módulos-con-optionid)).
- La orilla la marca la altura dibujada: algas 1,1-2 y arena mojada 0,9-1,7 (la fila de costa), el resto hasta 2,2-6.
  `water_distance` mide desde las esquinas de las celdas con agua, así que la orilla visible queda a 6-10 unidades.
- Dos franjas (30-09-2026, el usuario: la orilla cargada y la arena limpia): la orilla (`coast`, hasta ~20: algas
  7-16, arena mojada, estrellas, conchas) con poca densidad, y la arena seca detrás (`sand_*`, coral y huellas, 14-80,
  altura hasta 40, `share` 0,7, sin `coast`) con más (conchas 1,1, piedras 0,6 por celda en medium); las huellas solo
  ahí, desde 20.
- En Land1, playa de arena en `1700,2000` (cámara `1702,7,1992,1706,0.5,2004`; `dev\tools\lnd\lnd_beaches.py` lista la
  arena junto al agua).
- Imágenes del usuario en `B&W/Asstes_mods/Beach`; `.cfg` en `assets/mods/world.foliage.beach`.

### Módulo world.foliage.butterflies

- `world.foliage.butterflies` ("Butterflies"): los 3 gif del usuario (`B&W/Asstes_mods/Buterfly`) sobre
  `wildflowers` y `poppies`, 0,04 por flor, 0,7-1 de ancho (más grandes que de verdad para que se vean junto a la
  hierba de 0,7-1,2). Sin opciones. Cómo vuelan: [Voladores](#voladores-flyer-nombre).
- En Land1 hay unas 70 cerca de `1434,57.8,2232` (cámara `1428,61.5,2226,1434,57.5,2233`, `OPENBLACK_TIME_OF_DAY=13`).

### test.miracle-dispensers

«Máquinas de milagros de prueba» (categoría **Test**). **No existe en el original**: es una ayuda para probar los
milagros, desactivada por defecto. Código: `assets/mods/test.miracle-dispensers/mod.json` (el mod, que pone
`EngineConfig::testDispensers*`) y `src/Worship/TestDispensers.cpp` (lo que hace en el juego). Todo es **mod**; solo
los dispensadores son los del original ([magic.md](magic.md#dispensadores-y-luciérnagas-worshipspelldispensercpp-worshipfireflyrewardcpp)).

- **Activarlo**: menú **Mods** → sección *Test* → casilla «Máquinas de milagros de prueba» (y sus dos deslizadores
  debajo); se guarda en `Mods/test.miracle-dispensers/settings.cfg` (`enabled = on`, `level = base`,
  `recharge = 10s`). Solo para una sesión: `--mod test.miracle-dispensers` (más
  `--mod test.miracle-dispensers.level=all`, `--mod test.miracle-dispensers.recharge=5s`). Sin reinicio, pero los
  dispensadores salen **al cargar una tierra** (o en el turno siguiente si se enciende con la tierra cargada); al
  apagarlo se quedan hasta la próxima carga. La recarga sí se cambia en vivo en los ya puestos.
- **Qué hace**: cuando ha corrido el guion de la tierra (después de `PostLoadCleanup`, en `worship::ProcessTurn`) y el
  jugador humano tiene ciudadela (`citadel::Of`; se busca su posición en la entidad, no está escrita en el código),
  pone un dispensador `NORSE_ABODE_SPELL_DISPENSER` (el del desafío de Land 1) por milagro, como
  `GiveSpellDispenserReward`: `dispenser::Create` (pueblo más cercano del jugador, mirando al templo),
  `SetMagicProperties(magia, recarga)` y `SetActive` (orbe al momento). Cuando se coge el orbe, a los `recharge`
  segundos sale otro.
- **Dónde**: en anillos alrededor del templo, el primero a radio del templo (mitad mayor de su malla en x/z, 25,6 m en
  Land 1) + 12 m y los siguientes cada 13 m, puestos cada 13 m de arco (los anillos impares desplazados medio paso). Un
  sitio vale si un cuadrado de 8 × 8 m (9 puntos) es tierra seca sin agua (`sea_cells::IsWater` / `IsDryLand`), con
  menos de 2,5 m de desnivel, dentro de la influencia del jugador (`CalculatePlayerInfluence > 0`, la regla de
  lanzamiento), a más de 4 m de cualquier objeto fijo (`Fixed`, edificios, árboles, rasgos, rocas, ollas, campos,
  farolas, tótem, lugares de culto) y lo acepta `map_collide::IsOkToCreateAtPos`. Constantes elegidas por openblack
  (mod).
- **Milagros** (las 14 semillas del jugador de `GSpellSeedInfo`, en su orden; las de la criatura, 12..27, no): STORM
  (tormenta), NATURE (bosque), FIRE (bola de fuego), FOOD (comida), SHIELD (escudo), PHYSICAL_SHIELD (escudo físico),
  LIGHTNING_BOLT (rayo), HEAL (curar), WOOD (madera), WATER (agua), FLYING_FLOCK (bandada de palomas), GROUND_FLOCK
  (manada de lobos), TELEPORT (teletransporte) y BEAM_EXPLOSION (explosión de rayo). La tormenta eléctrica y el
  tornado no son semillas: son los power-ups de STORM.
- **Opción `level`** (deslizador): `base` la magia base de cada semilla; `pu1` / `pu2` su power-up 1 / 2 (si la
  semilla no lo tiene, el más alto que tenga); `all` un dispensador por cada nivel distinto (25 en total: STORM,
  STORM_PU1 = tormenta eléctrica, STORM_PU2 = tornado; FIRE ×3; FOOD ×2; LIGHTNING_BOLT ×3; HEAL ×2; WATER ×2;
  BEAM_EXPLOSION ×3; el resto ×1). El orbe sale con el nivel de su magia (`GetPowerUpGesture`, como el original).
- **Opción `recharge`** (deslizador): segundos hasta el siguiente orbe (`SET_MAGIC_PROPERTIES` en segundos × 10
  turnos); el original usa 300 turnos (`timeEachMobileObjectTakesToProduce`).
- **Máquina vacía** (siempre): un dispensador más en el siguiente sitio libre del anillo (en Land 1 con `base`,
  (1878,0, 2515,4)), creado solo con `dispenser::Create`, como un `CREATE(SPELL_DISPENSER)` sin
  `SET_MAGIC_PROPERTIES` ni `SET_ACTIVE`: queda inactivo y sin magia, así que nunca da orbe
  (`SpellDispenser::Process` 0x722A70 solo produce si está activo). Sirve para comparar la máquina sola con las que
  tienen orbe. Registro: `Mod test.miracle-dispensers: empty dispenser <entidad> at (x, z)`.
- **Opción `seed`** (`on` por defecto): 10 turnos después de poner los dispensadores (para que el guion de la tierra
  y su intro ya hayan empezado) pone una semilla de FIRE (bola de fuego, sin power-up) en la mano del jugador humano
  por el camino de un uso, `OneOffSpellSeed::CreateSpellIntoHand` 0x72A730 (el mismo que `OPENBLACK_TEST_SEED`). Si la
  mano está ocupada lo reintenta cada turno (hasta 600). Así se comparan la transparencia de un orbe, la de la
  máquina vacía y la de la semilla en la mano. `--mod test.miracle-dispensers.seed=off` la quita. Registro:
  `Mod test.miracle-dispensers: fire seed into the hand -> <entidad>`. Turnos y reintentos elegidos por openblack
  (mod).
- **Orden de creación**: los dispensadores y sus orbes no existen en el original, así que se crean dentro de un
  `ecs::object_index::ModScope`: toman índices de un rango aparte (desde `k_ModBase` = 0x40000000) y el contador del
  original no se mueve (las velocidades de los aldeanos, `Villager::SetSpeed`, y los órdenes de animales y bosques
  quedan iguales). `SpellDispenser::CreateOneOffSpellSeed` / `ApplySeed` abren el mismo ámbito si el dispensador es
  de un mod (`IsModObject`). No consumen números aleatorios del juego. Sí son abodes del pueblo y obstáculos fijos,
  como el dispensador de Land 1. La semilla que da el orbe al tocarlo y lo que crea el hechizo cuentan como siempre
  (son acciones del jugador).
- **Registro**: una línea por dispensador:
  `Mod test.miracle-dispensers: dispenser <entidad> seed <n> (<SEMILLA>) pu <nivel> magic <n> (<MAGIA>) at (x, z)`.
- **Land 1** (`level = base`; templo en (1915,1, 2508,9), anillo de 37,6 m, los 14 caben en el primero):

  | Semilla | Posición (x, z) |
  |---|---|
  | STORM | 1915,1, 2546,5 |
  | NATURE | 1927,9, 2544,2 |
  | FIRE | 1939,2, 2537,7 |
  | FOOD | 1947,6, 2527,7 |
  | SHIELD | 1952,1, 2515,4 |
  | PHYSICAL_SHIELD | 1952,1, 2502,4 |
  | LIGHTNING_BOLT | 1947,6, 2490,1 |
  | HEAL | 1939,2, 2480,1 |
  | WOOD | 1927,9, 2473,6 |
  | WATER | 1915,1, 2471,3 |
  | FLYING_FLOCK | 1902,2, 2473,6 |
  | GROUND_FLOCK | 1890,9, 2480,1 |
  | TELEPORT | 1882,5, 2490,1 |
  | BEAM_EXPLOSION | 1878,0, 2502,4 |

  Con `all` los 25 ocupan el primer anillo (18 sitios) y 7 del segundo (radio 50,6 m).
- **Probado** (2026-10-01, capturas en `dev\_audit\magic\`): `dispmod_ring.png` (el anillo en Land 1),
  `dispmod_cast.png` (el orbe de FIRE tocado, `seed (FIRE, pu -1) in the hand with 3500 chants`, armado y lanzado:
  la bola de fuego en el suelo y su dispensador vacío) y `dispmod_all.png` (`level = all`, 25 dispensadores). A los
  10 s del toque el dispensador hace otro orbe. Gancho de cámara: `OPENBLACK_CAMERA_LOCK=1960,85,2580,1915,32,2508`
  (ver [Ganchos de prueba](#ganchos-de-prueba)).
- **Probado** (2026-10-01, máquina vacía y semilla): `prism_empty.png` (la máquina vacía en primer plano, sin orbe;
  `OPENBLACK_CAMERA_LOCK=1872,40,2528,1878,33,2515.4`), `prism_seed_hand.png` / `prism_seed_hand2.png` (la semilla
  de fuego en la mano junto al orbe de FIRE; `seed 2752 (FIRE, pu -1) in the hand with 3500 chants`).
- **El «prisma» oscuro junto a un dispensador** (captura del usuario, 2026-10-01 12:41): no es la submalla de física
  de la malla 557 (ningún camino de dibujo la pinta, ver
  [rendering-objects.md](rendering-objects.md#submallas-de-física-y-de-lod-0)). El registro de esa partida dice que el
  usuario rompió con una roca lanzada los dispensadores de TELEPORT y BEAM_EXPLOSION (`Buildings: 2602 hit, life 1.00
  -> 0.00`, `4 pieces`, `2602 destroyed`, y lo mismo 2606): el orbe de BEAM_EXPLOSION (la estrella de `I_Blast`) se
  quedó flotando y lo de alrededor son los trozos (`Fragment`) de la máquina rota. **(inferido)** En el original
  `SpellDispenser::Draw` 0x722940 llama a `MultiMapFixed::Draw` 0x518090 y no a `Abode::Draw`, así que nunca dibuja la
  FragMesh de un dispensador dañado; falta leer si `Abode::ReactToPhysicsImpact` 0x406240 lo rompe (pendiente).

### game.skip-intro

**Único mod activado por defecto** (pedido por el usuario, 2026-10-01; el resto siguen apagados). Lo hace el campo
nuevo `Mod::Info::enabledByDefault`, que el constructor de `Mod` copia a `_enabled`. Ojo: si ya existe
`Mods/game.skip-intro/settings.cfg`, **manda ese archivo** (como en cualquier mod), así que un cambio de valor por
defecto no llega a una instalación que ya haya arrancado una vez; hay que editar su `settings.cfg`.

- Opción `skip`: `tutorial`, `tutorial and creature training` o `tutorial, creature training and the glade`
  (**por defecto**). Opción `free start`: `on` (por defecto) u `off`. Con reinicio (la respuesta cuenta al empezar la
  partida). Para una sola vez: `--mod game.skip-intro.skip=tutorial`, `--mod game.skip-intro.free start=off`,
  `--mod game.skip-intro=off`.
- El salto en sí no se inventa: da la respuesta que el original pedía al jugador. En runblack.exe v1.42, al empezar cada
  partida, `GGame::OnNewGame` (0x55395B) llama a `GGame::DoYesNoSkipTutorialRequestersIfNecessary` (0x54CBD0), que
  borra los bits 23, 24 y 25 de `g_game+0x14`, pausa el juego y enseña el **SkipBox** (cuatro casillas, la primera
  marcada por defecto; sin ESC, `SkipBox::CanESCOut` 0x53BD60 da 0). openblack no dibuja ese cuadro y juega todo, como
  la respuesta por defecto; con el mod, `Game::Run` pone los bits de la segunda (`tutorial`, bit 23), la tercera
  (`tutorial and creature training`, bits 23 y 24) o la cuarta respuesta (`…and the glade`, bits 23, 24 y 25), y el
  guion se salta la intro por su cuenta.
- Qué salta el guion (`SetupLand1` / `LandControl1` de challenge.chl): con `CAN_SKIP_TUTORIAL` no corren `FollowUs` (la
  intro: la cámara del guion y `START_MUSIC 54`), `CitadelGuide` (la ciudadela se construye al momento) ni
  `ChooseYourCreature`. Con `CAN_SKIP_CREATURE_TRAINING` además no corren las lecciones del guía de la criatura. Con
  `IS_KEEPING_OLD_CREATURE` tampoco corre `CreaturesInGlade`, que es **la que coge la cámara y el diálogo, funde a
  negro, vuela la cámara y pone `START_MUSIC(63)`** (challenge.chl 44028..44691): por eso la cuarta respuesta es la
  que deja el principio al jugador. Detalle en
  [map-loading.md](map-loading.md#saltar-el-tutorial-skipbox-y-can_skip_tutorial).
- La cuarta respuesta necesita además `CURRENT_PROFILE_HAS_CREATURE` (`SetupLand1` lo hace `and` con el bit 25,
  challenge.chl 25432..25434) y openblack no tiene perfiles de jugador: el mod **contesta por el perfil** (CHL 463 da
  verdadero cuando `skipTutorialChoice` es 3). Lo que el guion hace entonces en vez del claro es cargar la criatura del
  perfil (`LOAD_MY_CREATURE`, sin portar: no sale criatura, como hoy).
- **`free start` (no es del original).** Aun con la cuarta respuesta el guion corre `CreatureDevSeeHome`, que en su rama
  de salto (challenge.chl 7056..7085) coge la cámara y el diálogo un turno, enciende y apaga la pantalla ancha, **clava
  la cámara sobre el poblado** (`SET_CAMERA_POSITION(1891.04, 31.69, 2520.67)`) y hace `SET_FADE_IN(2.0)`. Con
  `free start` el motor **se come eso**: la **primera tarea del guion que coge la cámara en una partida nueva** es «el
  principio de la tierra», y mientras la tenga, `SET_CAMERA_POSITION` (001), `SET_CAMERA_FOCUS` (002),
  `SET_WIDESCREEN` (032), `SET_FADE` (241), `SET_FADE_IN` (242), `START_MUSIC` (044) y `STOP_MUSIC` (045) no hacen nada
  y `HAS_CAMERA_ARRIVED` (035) contesta «ya ha llegado» (si no, el guion esperaría para siempre: `MOVE_CAMERA_POSITION`
  y `MOVE_CAMERA_FOCUS` tampoco están implementados). `START_CAMERA_CONTROL` **sí se concede**, para que el
  `loop { START_CAMERA_CONTROL }` del guion pase y suelte la cámara como siempre; en openblack la cámara del jugador no
  se le quita de todas formas (`Help/ScriptControl.cpp`). En cuanto esa tarea hace `END_CAMERA_CONTROL` (o se para)
  todo vuelve a la normalidad: las escenas de los milagros, las misiones y los vórtices siguen igual. Estado:
  `CameraControl::freeStartTask` / `freeStartArmed` (`Help/ScriptControl.h`), armado en `CameraControl::Reset` (cada
  carga de mapa).
- Lo que **no** toca el mod: la hora del día que pone el guion (`SET_GAME_TIME(4.59)` de `CreatureDevSeeHome`: amanece,
  como en el original) y la música de alineamiento/tribu, que en el original también suena desde el principio cuando se
  salta el tutorial (el `ENABLE_DISABLE_ALIGNMENT_MUSIC(false)` está dentro de `FollowUs`, challenge.chl 50102).

### Modpack examples

`mods/examples/` en el repo, `Mods/examples/` junto al exe (el build copia los archivos y compila los nativos en sus
`bin/`). Apagado por defecto. No cambian nada del juego salvo `example.data-only` (agua viva mientras está encendido);
escriben en la pestaña Log. Son las plantillas.

| Mod | Tipo | Qué enseña |
|---|---|---|
| `example.data-only` | solo `mod.json` | `switches`, una opción con `bind`, `replace` vacío |
| `example.lua-hello` | Lua | `require` de un módulo propio, opción, interruptor, enumeraciones, eventos `land_loaded` y `turn`, cámara y altura |
| `example.lua-library` | Lua, librería | `provides` + `ob.interfaces.provide("example.places.v1", tabla)` |
| `example.lua-consumer` | Lua | `dependencies` + `ob.interfaces.get` |
| `example.native-hello` | C | `ob_mod_query` / `ob_mod_load` / `ob_mod_unload`, opción, eventos, enumeración, altura, hora |
| `example.native-library` | C, librería | `provide_interface("example.counter.v1")` con su cabecera pública `include/example_counter_v1.h` |
| `example.native-consumer` | C | `dependencies` + `get_interface` |

## Pendiente

- SDK de mods (2026-10-01), lo que falta: sonido en Lua y C (envoltorio de `Audio.h` con un dueño por mod, acordado
  con audio), lanzar orbes (cuando la API `one_off::` de milagros sea estable), `ecs::object` y `game_clock` (sistemas2),
  límite de memoria por script Lua, recarga en caliente de scripts, reemplazar bancos de sonido (con
  audio, B11), reemplazar mallas en vivo (hoy al arrancar: las formas físicas se toman al crear cada objeto), texturas
  incrustadas en un `.l3d` (`L3DMesh::_skins`) y materiales sueltos del `.lnd`, traducciones `lang/<idioma>.json`
  (hoy los textos por idioma van dentro del `mod.json`), y el idioma de la ventana (hoy inglés; `mods::SetLanguage`).
- `world.crops`: sin el mod los campos se quedan vacíos hasta que openblack tenga oficios (granjeros).
- HD-Tweaks: lo que queda por comprobar está en [mods.md](mods.md#mod-hd-tweaks).
- Revisión de todos los mods tras la base 0e10b735 (2026-10-01): todos compilan, leen su `settings.cfg` y funcionan
  encendidos y apagados; `world.foliage` lee el agua con `sea_cells::IsWater`. `world.ground-statics` no se vio
  bajar nada: en los sitios mirados de Land 1 (1327,2432 y 1342,2406) ningún estático flota con el AllMeshes.g3d actual.

## Ganchos de prueba

| Gancho | Qué hace |
|---|---|
| `--mod <id>`, `--mod <id>=off`, `--mod <id>.<opción>=<elección>` | Activa un mod u opción solo para esa sesión |
| `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` | Cambia HD-Tweaks en ese fotograma (recarga en vivo) |
| `OPENBLACK_TEST_FIELD_GROWTH=0..1200` | Todos los campos empiezan con ese crecimiento y su comida |
| `OPENBLACK_HAND_TRACE=1` | Escribe `Flyer trace` (huida de las mariposas) |
| `OPENBLACK_TIME_OF_DAY=13` | Hora del juego para ver las mariposas (solo de día) |
| `OPENBLACK_CAMERA_LOCK="ox,oy,oz,fx,fy,fz"` | Pone la cámara ahí cada turno (`WorshipDebugHooks.cpp`): en Land 1 el guion mueve la cámara y `OPENBLACK_CAMERA_FLY` ya no llega |
| `--mod test.miracle-dispensers` + `OPENBLACK_TEST_TAP="1939.2,2537.7,200"` + `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"` | Toca el orbe de FIRE de Land 1 y lo lanza |

Cámaras: playa de Land1 `1702,7,1992,1706,0.5,2004`; mariposas de Land1 `1428,61.5,2226,1434,57.5,2233`.

## Fuentes

- Código: `src/Mods/` ([Cómo está hecho](#cómo-está-hecho-srcmods)), `src/Debug/ModsWindow.*`,
  `components/modsdk/include/openblack/mod_api.h`, `mods/` (ejemplos y su CMake), `test/test_mods.cpp`;
  `game.skip-intro` en `Game::Run` y `CHLApi.cpp` (`CanSkipTutorial`, `FreeStart`), `src/Worship/TestDispensers.cpp`,
  `src/3D/Foliage.*`, `src/3D/FoliageFlyers.cpp`, `src/Resources/HdTweaks`, `src/main.cpp` (atajos).
- Manifiestos en el repo: `assets/mods/<id>/mod.json` (los 13 que vienen con openblack) y sus datos:
  `assets/mods/world.foliage`, `assets/mods/world.foliage.beach`, `assets/mods/world.foliage.butterflies`,
  `assets/mods/graphics.hd-tweaks`.
- Diseño del SDK: `dev\tmp_dis\modding\PLAN.md` (con lo que se tomó de Factorio, Fabric, RimWorld, SKSE y Luanti).
- Imágenes del usuario: `B&W/Asstes_mods/{Plants,Beach,Buterfly}`; `mono_*` en `B&W/BnW_openblack/Mods/world.foliage`.
- Estudios: `dev\tmp_dis\heights` (altura junto al mar), `dev\tmp_dis\biomes` (mapas de zonas `Land*_snd.png`).
- Scripts del LND: `dev\tools\lnd\` (`lnd_hash.py`, `lnd_zones.py`, `lnd_countries.py`, `lnd_beaches.py`).
