# Librería de mods

Todo lo que cambia el juego original es un **mod**, desactivado por defecto. La librería (`src/Mods/`) los registra,
genera el menú **Mods**, guarda el estado de cada uno en su carpeta `Mods/<mod>/settings.cfg` y los activa desde la
línea de comandos.

## Para el jugador

- Menú **Mods** del juego: una casilla por mod (con descripción al pasar el ratón), agrupadas por categoría; las
  opciones (p. ej. muestras de MSAA) debajo. `*` = hace falta reiniciar.
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
  Las carpetas con el id de un mod integrado son de ese mod; las demás son mods de datos. Cada mod escribe su
  `settings.cfg` al arrancar si no lo tiene (con su estado por defecto) y al cambiarlo en el menú:
  ```
  # Anti-aliasing (MSAA) (graphics.msaa). For one session only: --mod graphics.msaa[=off], --mod graphics.msaa.<option>=<choice>
  enabled = on
  samples = 4x  # Samples: 2x, 4x, 8x, 16x
  ```
- El antiguo `mods.cfg` único (junto al ejecutable o en `Mods/`) se reparte solo en los `settings.cfg` al arrancar y se
  borra (`ModRegistry::ImportLegacySettings`).
- Línea de comandos, solo para esa sesión (no se guarda): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`. Los interruptores anteriores siguen como atajos: `--msaa N`, `--mipmaps`,
  `--anisotropic`, `--enhanced-graphics` (= MSAA 4× + anisótropo), `--living-water`, `--ground-static-objects`.

## Mods integrados

| Id | Qué hace | Reinicio |
|---|---|---|
| `graphics.msaa` (+ `samples` 2x/4x/8x/16x) | Antialiasing multimuestreo y alpha to coverage en hojas y vallas | no |
| `graphics.mipmaps` | Mipmaps y filtrado trilineal | sí |
| `graphics.anisotropic` | Filtrado anisótropo (incluye los mipmaps) | sí |
| `graphics.terrain-x2` (+ `repeat` x1/x2/x3/x4, `upscale` off/on, `cliffs` triplanar/stretched) | Terreno más nítido: cada material repetido 1-4 veces por bloque (por defecto x2; wrap Repeat), escalado ×2 con Lanczos-3 al cargar (`Graphics/TextureUpscale`, con wrap: los materiales del LND son tileables, primera y última fila/columna idénticas) y acantilados triplanares: el original proyecta todo desde arriba (uv = posición xz del bloque) y en las pendientes la textura se estira en rayas; con `triplanar` se mezclan también las proyecciones a lo largo de x y z (pesos \|n\|⁴, normal suave por vértice de diferencias centrales de altitud, `LandVertex::normal`). Los materiales que son un dibujo único por bloque y no una textura (el geoglifo de la figura: Land1 material 10 y Land5 material 5; el laberinto: Land5 material 1) se quedan en ×1. Nada en el LND los marca (su `type` 18/11 lo comparten hierbas normales, y la métrica de contraste a gran escala no los separa de una roca nevada), así que se reconocen por hash FNV-1a de sus texels (`IsPictureMaterial` en LandIsland.cpp) y viajan en el byte `w` de los ids de material del vértice (bits 0-2). Si un mod de datos trae otro dibujo, hay que añadir su hash (`dev\lnd_hash.py`). Solo escalar apenas se nota: cada material de 256 px cubre un bloque de 160 unidades. **El mar** también: su periodo de repetición (560 a nivel de detalle 4) se divide por las repeticiones, la ondulación por filas del original se divide igual (si no, mueve la textura el triple y deja bandas) y con `upscale` `sky.raw`/`skya.raw` se escalan ×2 con Lanczos al cargarse (`Texture2DLoader`, que además copia los datos: antes pasaba a bgfx una referencia a un vector local) | sí |
| `water.living` | El mar refleja todo, el reflejo ondula despacio en bucle y la superficie deriva (sin la ondulación por filas) | no |
| `world.ground-statics` | Baja las rocas y objetos estáticos que flotan hasta el suelo | no |
| `world.foliage` (+ `density` low/medium/high/very high = ×0.5/1/2/4, `distance` near/medium/far = 120/200/320) | Hierba, flores, juncos y matorrales sobre el terreno (billboards instanciados, `3D/Foliage`). Reglas e imágenes en `<exe>/Mods/world.foliage/` (`foliage.cfg`; en el repo `assets/mods/world.foliage/`; imágenes originales del usuario en `B&W/Asstes_mods`). Detalles abajo | no |

Detalles de cada uno en [rendering.md](rendering.md) y [openblack-internals.md](openblack-internals.md).

## Mods de datos

- Una carpeta por mod en `Mods/` (salvo las que se llaman como un mod integrado), con la misma estructura que el juego (`Data/...`,
  `Scripts/...`) y un `mod.cfg` opcional:
  ```
  name = Agua azul
  description = Sustituye Sky.raw y Skya.raw
  ```
- Id `data.<carpeta>`. Se activan en el menú (con reinicio) o con `--mod data.<carpeta>`.
- Un archivo del mod sustituye al del juego con la misma ruta; si dos mods lo tienen, gana la carpeta posterior en
  orden alfabético. Los archivos que solo están en el mod también se ven (p. ej. mapas o texturas nuevas).
- Cómo funciona: `FileSystemInterface::AddOverridePath`; `FindPath` mira primero los mods (solo archivos, nunca
  carpetas) y `Iterate` mezcla la carpeta del juego con la de cada mod.

## Para programar un mod integrado

1. Un archivo propio en `src/Mods/Builtin/<Nombre>Mod.cpp` con una clase derivada de `mods::Mod` y una función
   `Register<Nombre>Mod`, declarada y llamada en `BuiltinMods.h`. Sus archivos van en el repo en `assets/mods/<id>/`
   y en el juego en `Mods/<id>/` (junto a su `settings.cfg`); los lee con `ModRegistry::GetModFilesDirectory(id)`.
2. `Info`: id estable (`categoria.nombre`), nombre, descripción, categoría, `restartRequired`.
3. Opciones con `AddOption({"id", "Etiqueta", {"elección1", "elección2"}, índicePorDefecto})`; se leen con
   `GetChoice("id")`.
4. `Apply()`: pone en marcha el estado actual. Se llama al arrancar (después de los `settings.cfg` y la línea de comandos) y
   cada vez que el mod o una opción cambia. Lo normal es escribir un interruptor de `EngineConfig` que lee el motor.
5. El motor nunca decide por su cuenta: todo lo que no es original mira un interruptor que solo pone un mod.

Pendiente (nivel 3): mods externos (Lua o DLL) sobre esta misma API.

## world.foliage: plantas sobre el terreno

- **Campos de cultivo** (opción `fields` = wheat, por defecto; `original` = la malla): la malla del campo se oculta
  (`Alpha` 0 en `ecs::UpdateFields`; sigue ahí para la mano y los instantes con alfa 0 ya no se dibujan: escribían
  profundidad) y `Foliage::UpdateFields` pone en su huella (caja de la malla con su giro y escala) una rejilla con
  ruido cada `[field] spacing` unidades. Cada fotograma, por campo al alcance: nada sin sembrar; la etapa
  `[field_stage ...]` según el crecimiento (0-1200) ± `stagger` al azar por planta (cambio gradual); ancho y tinte
  interpolados dentro de la etapa (el tinte sustituye al color del suelo: `i_data4.z` = 1, `w` = r·65536 + g·256 + b);
  solo quedan las plantas con `keep` < comida / comida esperada a ese crecimiento, así que la cosecha lo aclara.
  Etapas actuales: brote (hierba baja, 0-80), hierba alta (80-350), trigo verde (350-750), trigo secándose hasta
  marrón maduro (750-1200). Instancias transitorias cada fotograma. Gancho `OPENBLACK_TEST_FIELD_GROWTH=0..1200`
  (todos los campos empiezan con ese crecimiento y su comida).
- Tamaños: el 29-09-2026 todos los `size` se redujeron un 25 % (el usuario las veía muy grandes).
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
- **Suelo oscuro o sin color**: el tinte toma la textura del material a baja resolución, y algunas tienen manchas muy
  oscuras (Land1 material 5, brezo, tipo 25: 46 % de sus texeles de 32×32 con brillo < 0,3) o grises (material 10
  un 13 % con saturación < 0,3), que daban plantas grises. `LandMaterialInfo::small` guarda cada material en 32×32
  (media de cajas de 8×8, como el mip 3 que muestrea el shader) y `ground_value` / `ground_saturation` filtran por el
  color de ese texel (mismo uv que el terreno, con las repeticiones de terrain-x2). Las plantas tintadas piden
  brillo ≥ 0,28 y saturación ≥ 0,3; `dead_bush_dark` / `dead_bush_grey` (con sus colores, `tint = none`) ocupan las
  manchas.
- `cross = on`: la especie se dibuja con los dos planos cruzados (los matorrales secos); en cada bloque esas
  instancias van al final (`Chunk::crossStart`) y se dibujan con los 12 índices del quad.

- Una sección `[nombre]` por planta en `foliage.cfg`: `images` (png, uno al azar por planta), `texture` (aspecto de la
  textura: green/dry/sand/rock/snow), `terrain` (tipo del LND, `TerrainMaterialType`), `per_cell` (por celda de 10×10
  con densidad media), `size` (ancho mín-máx; el alto sale de la proporción de la imagen), `altitude`, `slope` (grados),
  `patches` (0 uniforme .. 1 solo en manchas, ruido de valor a escala 45), `sway` (viento), `lean` (inclinación máxima
  al azar) y `tint` (grey/all/none). Crece si cumple `texture` o `terrain`.
- **Zonas (biomas)**: `zone` / `not_zone` filtran por la zona de ambiente de la celda, el código de sonido que el
  diseñador pintó en cada celda (`LNDCell::flags >> 1`, los impares > 8 cuentan como el par anterior; `Foliage::ZoneOf`).
  Es lo único del LND que forma regiones limpias: los `country` son solo la paleta de texturas por altura y están
  muy fragmentados (Land1: 10 mezclados por todo el mapa). Zonas en la tierra de Land1-5: 14 pájaros (`meadow`, casi
  todo), 6 costa (franja junto al mar), 8 jungla (manchas compactas: Land1 noroeste ~1620,2290 y este ~2550,2550;
  Land5 5-6 manchas), 16 bosque (Land1 ~2160,3100), 10 viento = nieve y montaña (Land2 todo el suroeste, Land3,
  Land5 noreste), 4 olas lentas (`swamp`: charcas interiores, muchas en Land5) y 5 lago (Land2 centro). 12 desierto
  no lo usa ningún mapa original. Mapas en `dev\tmp_dis\biomes\Land*_snd.png` (`dev\lnd_zones.py`; `dev\lnd_countries.py`
  para los country). Uso actual: `water_plant` en jungla, lago y charcas; `jungle_grass` en la jungla; `wildflowers`
  en prado y bosque; `poppies` en prado; `dead_bush_barren` en viento/desierto (solo roca, tierra seca o arena). Todas con `tint = grey`.
- **Tinte por el suelo**: los texeles grises (saturación < 0,1-0,2) toman el color de la textura del terreno bajo la
  planta: el vertex shader muestrea el array de materiales en el mismo material y uv que el terreno (uv del bloque ×
  repeticiones del mod terrain-x2, mip 3); gris 0,5 = el suelo tal cual, más oscuro en la base y más claro en la punta.
  Los texeles de color (pétalos, espigas) no cambian. `tint = all` tinta toda la imagen; `none` usa sus colores.
- Sprites: solo los `mono_*`, las imágenes del usuario (`B&W/Asstes_mods/Plantsv2`; las de la primera versión en `B&W/Asstes_mods/Plantsv1`) pasadas a gris con `assets/mods/world.foliage/tools/mono_sprites.py --width=128`: hierba, hierba alta, matorrales y trigo con `--min-hue=0` (todo a gris); juncos, plantas de agua y flores con `--min-hue=50 --open=1`: lo verde (tono 50-170°) a gris con media 0,62 y del resto solo quedan en color las manchas que sobreviven a una apertura morfológica de 3×3 (pétalos, cabezas de los juncos, penachos); las vetas finas amarillo-marrón y los brillos casi blancos de las hojas también a gris (con `--min-hue=50` sin apertura salían vetas naranjas sin tintar). Los brillos y bordes poco saturados (s <= 0,12, v < 0,85) también a gris y solo los casi blancos (v >= 0,85) con un toque crema para que no se tinten. Los `gen_*` generados por `tools/gen_grass_sprites.py` (en el repo) ya no se usan. La base de cada imagen se recorta irregular por columnas (hasta el 9 % del alto) para que no se vea el borde recto.
- **El `type` del LND no describe el aspecto**: en Land1 las texturas 0 y 8 son hierba verde con tipo 5 `Earth` y la 11
  es arena con tipo `Earth`; sirve para sonidos/pasos. Por eso `texture` clasifica cada material por su color medio
  (`Foliage::ClassifyTexture`, medido en Land1-5): verde = tono 50-100° y saturación ≥ 0,55; nieve = saturación < 0,15
  y valor > 0,55; arena = valor ≥ 0,6; seca = tono < 50° y saturación ≥ 0,5; el resto roca (misma gama de tono que la
  hierba pero saturación 0,29-0,45). La isla expone tipo, "dibujo" y color medio con `LandIslandInterface::GetMaterialInfo`.
- Colocación determinista por bloque de terreno, **solo cerca de la cámara** (hasta 6 bloques por fotograma; se liberan
  al alejarse un bloque más allá): por celda y planta, `per_cell × densidad` candidatos; en cada punto se elige una
  esquina de la celda por su peso bilineal y uno de sus dos materiales por el coeficiente de mezcla (como el shader del
  terreno). Nada en celdas de agua, en materiales dibujo (geoglifo), fuera de la altura o pendiente, ni a menos de 1
  unidad de entidades `Fixed` que no sean árboles, ni de campos, rocas móviles, pilas, almacén, templo o piscifactoría
  (caja de la malla). Todo se rehace al cambiar de isla o densidad y cuando existen los objetos.
- **Agua**: el mar es el plano y = 0 (y es también el agua de los ríos, ver rendering.md "Ríos"); las celdas de costa (`coastLine`, altitud 2-3 en Land1) se dibujan con alfa 0,5
  sobre el mar y las de agua con alfa 0, así que nada crece en una celda con alguna esquina de agua o costa. `near =
  lake, stream, sea` + `water_distance` limitan una planta a esa distancia de agua (mapa de distancias 3-4 chamfer a
  5 unidades, `FoliageWaterMap`): lago = celdas de agua 4-conectadas que no llegan al borde del mapa (Land1: una
  charca de 10 celdas en x 2130-2160, z 2400-2450 y una celda suelta); río = segmentos entre los puntos de cada
  `Stream` (Land1: 11 ríos, 187 puntos). En B&W1 no hay agua a otra altura: los ríos son esos caminos (openblack aún no
  los dibuja). Los juncos usan `near = lake, stream` a 3-9 unidades. Ninguna planta a menos de 3 unidades de la línea de un río (el canal de river.l3d mide unas 4; distancia exacta a los tramos en cubos de 20 unidades). La base de cada planta sigue el suelo: altura en sus dos extremos (i_data4) y cizalla en el vertex shader, hundida un 6 %.
- Dibujo: un plano por planta con orientación fija al azar (no mira a cámara) e inclinado al azar hasta `lean` para
  que se vea desde arriba (dos planos cruzados se veían como cruces desde arriba); hundido un 12 % de su alto para que
  no se vea el borde inferior; las plantas se hunden en el último 20 % de la distancia (120/200/320); luz = tabla de
  luz del terreno[luminosidad de la celda] y la misma neblina; alpha test con borde nítido (alpha to coverage con
  MSAA). Solo en la pasada principal (no en el reflejo). Capas de 256×512 apoyadas abajo, con mipmaps; el color de los
  texeles transparentes es la media de los opacos.
