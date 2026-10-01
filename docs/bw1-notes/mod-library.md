# Librería de mods

Todo lo que cambia el juego original es un **mod**, desactivado por defecto. La librería (`src/Mods/`) los registra,
genera el menú **Mods**, guarda el estado de cada uno en su carpeta `Mods/<mod>/settings.cfg` y los activa desde la
línea de comandos. Esta página cubre cómo se usan, los tres tipos de mod, cómo se programa uno y el catálogo de los
integrados. Todo lo de esta página es **mod/propio** salvo que se diga lo contrario (**fiel**, **(aproximado)**).

- [Para el jugador](#para-el-jugador)
  - [Menú Mods](#menú-mods)
  - [Carpeta Mods y settings.cfg](#carpeta-mods-y-settingscfg)
  - [Línea de comandos](#línea-de-comandos)
- [Tipos de mod](#tipos-de-mod)
  - [Mods integrados](#mods-integrados)
  - [Mods de datos](#mods-de-datos)
  - [Módulos](#módulos)
- [Para programar un mod](#para-programar-un-mod)
  - [Mod integrado: API ModRegistry](#mod-integrado-api-modregistry)
  - [Opciones y deslizadores](#opciones-y-deslizadores)
  - [Módulos con option.\<id\>](#módulos-con-optionid)
- [Catálogo de mods](#catálogo-de-mods)
  - [graphics.msaa](#graphicsmsaa)
  - [graphics.mipmaps](#graphicsmipmaps)
  - [graphics.anisotropic](#graphicsanisotropic)
  - [graphics.terrain-x2](#graphicsterrain-x2)
  - [graphics.hd-tweaks](#graphicshd-tweaks)
  - [water.living](#waterliving)
  - [world.ground-statics](#worldground-statics)
  - [world.crops](#worldcrops)
  - [world.foliage](#worldfoliage)
  - [Módulo world.foliage.beach](#módulo-worldfoliagebeach)
  - [Módulo world.foliage.butterflies](#módulo-worldfoliagebutterflies)
  - [test.miracle-dispensers](#testmiracle-dispensers)
  - [game.skip-intro](#gameskip-intro)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

## Para el jugador

### Menú Mods

- Menú **Mods** del juego: una casilla por mod (con descripción al pasar el ratón), agrupadas por categoría; las
  opciones (p. ej. muestras de MSAA) debajo. `*` = hace falta reiniciar.
- Los módulos salen debajo de su mod padre, sangrados ([Módulos](#módulos)).

### Carpeta Mods y settings.cfg

- Todo lo de los mods está en la carpeta `Mods/` junto al ejecutable, **una carpeta por mod**:
  ```
  Mods/
    graphics.msaa/settings.cfg
    graphics.terrain-x2/settings.cfg
    water.living/settings.cfg
    world.foliage/settings.cfg + foliage.cfg + imágenes   (los archivos del mod, si los tiene)
    world.ground-statics/settings.cfg
    MiPackDeTexturas/settings.cfg + mod.cfg + Data/...    (un mod de datos)
  ```
- Las carpetas con el id de un mod integrado son de ese mod; las demás son mods de datos (o módulos, si su `mod.cfg`
  tiene `module_of`). Cada mod escribe su `settings.cfg` al arrancar si no lo tiene (con su estado por defecto) y al
  cambiarlo en el menú:
  ```
  # Anti-aliasing (MSAA) (graphics.msaa). For one session only: --mod graphics.msaa[=off], --mod graphics.msaa.<option>=<choice>
  enabled = on
  samples = 4x  # Samples: 2x, 4x, 8x, 16x
  ```
- El antiguo `mods.cfg` único (junto al ejecutable o en `Mods/`) se reparte solo en los `settings.cfg` al arrancar y se
  borra (`ModRegistry::ImportLegacySettings`).

### Línea de comandos

- Solo para esa sesión (no se guarda): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`.
- Los interruptores anteriores siguen como atajos (`src/main.cpp`): `--msaa N`, `--mipmaps`, `--anisotropic`,
  `--enhanced-graphics` (= MSAA 4× + anisótropo), `--living-water`, `--ground-static-objects`.

## Tipos de mod

### Mods integrados

Código en `src/Mods/Builtin/<Nombre>Mod.cpp`; uno por subsección del [Catálogo de mods](#catálogo-de-mods).

| Id | Opciones (por defecto en negrita) | Resumen | Reinicio |
|---|---|---|---|
| [`graphics.msaa`](#graphicsmsaa) | `samples` 2x/**4x**/8x/16x | Antialiasing multimuestreo | no |
| [`graphics.mipmaps`](#graphicsmipmaps) | — | Mipmaps y filtrado trilineal | sí |
| [`graphics.anisotropic`](#graphicsanisotropic) | — | Filtrado anisótropo (incluye los mipmaps) | sí |
| [`graphics.terrain-x2`](#graphicsterrain-x2) | `repeat` x1/**x2**/x3/x4, `upscale` **off**/on, `cliffs` **triplanar**/stretched | Terreno y mar más nítidos | sí |
| [`graphics.hd-tweaks`](#graphicshd-tweaks) | `textures` **hd**/original, `smooth` off/soft/**round**, `light` **smooth**/original, `sharp` **on**/off, `detail` **high**/original | Aldeanos, animales y mano mejor vistos | no |
| [`water.living`](#waterliving) | — | Mar que refleja todo y deriva | no |
| [`world.ground-statics`](#worldground-statics) | — | Baja al suelo los estáticos que flotan | no |
| [`world.crops`](#worldcrops) | `speed` **x1**/x2/x5/x10/x20/x50/x100 (deslizador) | Campos que se siembran solos | no |
| [`test.miracle-dispensers`](#testmiracle-dispensers) | `level` **base**/pu1/pu2/all, `recharge` 2s/5s/**10s**/20s/30s/60s (deslizadores), `seed` **on**/off | Un dispensador de cada milagro junto al templo (más uno vacío) y una bola de fuego en la mano, para probarlos | no |
| [`game.skip-intro`](#gameskip-intro) | `skip` **tutorial**/tutorial and creature training | Empieza Land 1 sin la intro (la respuesta «saltar el tutorial» del original) | sí |
| [`world.foliage`](#worldfoliage) | `density` low/**medium**/high/very high, `distance` near/**medium**/far, `fields` **wheat**/original | Hierba, flores, juncos, matorrales y trigo | no |

Más detalles en [rendering.md](rendering.md), [rendering-objects.md](rendering-objects.md), [openblack-internals.md](openblack-internals.md) y
[mods.md](mods.md#mod-hd-tweaks) (HD-Tweaks).

### Mods de datos

- Una carpeta por mod en `Mods/` (salvo las que se llaman como un mod integrado o son módulos), con la misma estructura
  que el juego (`Data/...`, `Scripts/...`) y un `mod.cfg` opcional:
  ```
  name = Agua azul
  description = Sustituye Sky.raw y Skya.raw
  ```
- Id `data.<carpeta>`. Se activan en el menú (con reinicio) o con `--mod data.<carpeta>`.
- Un archivo del mod sustituye al del juego con la misma ruta; si dos mods lo tienen, gana la carpeta posterior en
  orden alfabético. Los archivos que solo están en el mod también se ven (p. ej. mapas o texturas nuevas).
- Cómo funciona: `FileSystemInterface::AddOverridePath`; `FindPath` mira primero los mods (solo archivos, nunca
  carpetas) y `Iterate` mezcla la carpeta del juego con la de cada mod.

### Módulos

Enchufables a otro mod.

- Una carpeta de `Mods/` cuyo `mod.cfg` dice `module_of = <id de un mod>` es un **módulo** de ese mod, no un mod de
  datos: no sustituye archivos, trae más archivos del tipo que lee el mod padre, con sus mismas reglas. Id = el nombre
  de la carpeta (`world.foliage.beach`), `name` / `description` del `mod.cfg`, misma categoría que el padre.
- En el menú sale debajo del padre, sangrado y deshabilitado si el padre está apagado. Solo cuenta si él y su padre
  están encendidos (`ModRegistry::IsActive`). Tiene su `settings.cfg` en su carpeta, apagado por defecto, y
  `--mod <id>` como cualquier mod.
- Puede declarar opciones propias ([Módulos con option.\<id\>](#módulos-con-optionid)).
- Hoy solo `world.foliage` tiene módulos: [Beach](#módulo-worldfoliagebeach) y
  [Butterflies](#módulo-worldfoliagebutterflies). Módulos del repo en `assets/mods/world.foliage.beach` y
  `assets/mods/world.foliage.butterflies` (solo los `.cfg`; las imágenes del usuario están en `B&W/Asstes_mods/Beach`
  y `Buterfly` y se copian a la carpeta del juego).

## Para programar un mod

### Mod integrado: API ModRegistry

1. Un archivo propio en `src/Mods/Builtin/<Nombre>Mod.cpp` con una clase derivada de `mods::Mod` y una función
   `Register<Nombre>Mod`, declarada y llamada en `BuiltinMods.h`. Sus archivos van en el repo en `assets/mods/<id>/`
   y en el juego en `Mods/<id>/` (junto a su `settings.cfg`); los lee con `ModRegistry::GetModFilesDirectory(id)`.
2. `Info`: id estable (`categoria.nombre`), nombre, descripción, categoría, `restartRequired`.
3. Opciones: ver [Opciones y deslizadores](#opciones-y-deslizadores).
4. `Apply()`: pone en marcha el estado actual. Se llama al arrancar (después de los `settings.cfg` y la línea de
   comandos) y cada vez que el mod o una opción cambia. Lo normal es escribir un interruptor de `EngineConfig` que lee
   el motor.
5. El motor nunca decide por su cuenta: todo lo que no es original mira un interruptor que solo pone un mod.
6. Si tiene módulos, el padre pide sus carpetas con `ModRegistry::GetModuleDirectories("<su id>")` (orden alfabético)
   o, con sus opciones, con `ModRegistry::GetModules` (carpeta + opciones).

### Opciones y deslizadores

- `AddOption({"id", "Etiqueta", {"elección1", "elección2"}, índicePorDefecto})`; se leen con `GetChoice("id")`.
- Con un quinto campo `true` (`ModOption::slider`) se dibuja como deslizador sobre las opciones en vez de lista
  desplegable, para las que son una escala (`world.crops` `speed`). El `settings.cfg` guarda igual el nombre de la
  opción elegida.

### Módulos con option.\<id\>

- Opciones de un módulo: su `mod.cfg` las declara con `option.<id> = <etiqueta> | <opción>, <opción>... | <por
  defecto> [| slider]` (`ModuleMod`); salen en el menú y en su `settings.cfg` como las de cualquier mod, y el padre las
  lee con `ModRegistry::GetModules` (carpeta + opciones).
- `world.foliage` lee `density` de cada módulo (very low 0,25, low 0,5, medium 1, high 2, very high 4, como su propia
  densidad): multiplica los `per_cell` de ese módulo (`Foliage::Load`, `moduleDensities`) y recarga al cambiarla.
  Beach la tiene (deslizador; commit 8c024d07):
  ```
  option.density = Density | very low, low, medium, high, very high | medium | slider
  ```

## Catálogo de mods

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
  pasaba a bgfx una referencia a un vector local).

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
  - `light` = `smooth` (por defecto: la misma luz del original, ambiente 90/256 + 166/256 N·L, pero por píxel en
    `fs_object` con las normales suaves) u `original` (por vértice). Un borde de luz en la silueta (0,8·(1-N·V)²) se
    probó y quedaba feo (usuario, 2026-09-30).
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
milagros, desactivada por defecto. Código: `src/Mods/Builtin/MiracleDispensersMod.cpp` (el mod, que pone
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

- Opción `skip`: `tutorial` (por defecto) o `tutorial and creature training`. Con reinicio (cuenta al empezar la
  partida). Para una sola vez: `--mod game.skip-intro` (y `--mod game.skip-intro.skip=tutorial and creature training`).
- No inventa ningún salto: da la respuesta que el original pedía al jugador. En runblack.exe v1.42, al empezar cada
  partida, `GGame::OnNewGame` (0x55395B) llama a `GGame::DoYesNoSkipTutorialRequestersIfNecessary` (0x54CBD0), que
  borra los bits 23, 24 y 25 de `g_game+0x14`, pausa el juego y enseña el **SkipBox** (cuatro casillas, la primera
  marcada por defecto; sin ESC, `SkipBox::CanESCOut` 0x53BD60 da 0). openblack no dibuja ese cuadro y juega todo, como
  la respuesta por defecto; con el mod, `Game::Run` pone los bits de la segunda (`tutorial`, bit 23) o la tercera
  respuesta (`tutorial and creature training`, bits 23 y 24), y el guion se salta la intro por su cuenta.
- Qué salta el guion (`SetupLand1` / `LandControl1` de challenge.chl): con `CAN_SKIP_TUTORIAL` no corren `FollowUs` (la
  intro: la cámara del guion y `START_MUSIC 54`), `CitadelGuide` (la ciudadela se construye al momento) ni
  `ChooseYourCreature`; se va directo a elegir criatura en el claro (`CreaturesInGlade`, que también coge la cámara).
  Con `CAN_SKIP_CREATURE_TRAINING` además no corren las lecciones del guía de la criatura. Detalle en
  [map-loading.md](map-loading.md#saltar-el-tutorial-skipbox-y-can_skip_tutorial).
- La cuarta respuesta del original (bits 23 a 25, conservar la criatura antigua) no se ofrece: el guion también pide
  `CURRENT_PROFILE_HAS_CREATURE` y openblack no tiene perfiles.

## Pendiente

- Nivel 3: mods externos (Lua o DLL) sobre esta misma API.
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

- Código: `src/Mods/` (`ModRegistry`, `Mod`, `Builtin/*Mod.cpp`; `game.skip-intro` en `Builtin/SkipIntroMod.cpp`, `Game::Run` y
  `CHLApi.cpp` `CanSkipTutorial`), `src/Worship/TestDispensers.cpp`, `src/3D/Foliage.*`, `src/3D/FoliageFlyers.cpp`,
  `src/Resources/HdTweaks`, `src/main.cpp` (atajos de la línea de comandos).
- Datos del mod en el repo: `assets/mods/world.foliage`, `assets/mods/world.foliage.beach`,
  `assets/mods/world.foliage.butterflies`, `assets/mods/graphics.hd-tweaks`.
- Imágenes del usuario: `B&W/Asstes_mods/{Plants,Beach,Buterfly}`; `mono_*` en `B&W/BnW_openblack/Mods/world.foliage`.
- Estudios: `dev\tmp_dis\heights` (altura junto al mar), `dev\tmp_dis\biomes` (mapas de zonas `Land*_snd.png`).
- Scripts del LND: `dev\tools\lnd\` (`lnd_hash.py`, `lnd_zones.py`, `lnd_countries.py`, `lnd_beaches.py`).
