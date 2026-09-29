# Librería de mods

Todo lo que cambia el juego original es un **mod**, desactivado por defecto. La librería (`src/Mods/`) los registra,
genera el menú **Mods**, guarda su estado en `mods.cfg` y los activa desde la línea de comandos.

## Para el jugador

- Menú **Mods** del juego: una casilla por mod (con descripción al pasar el ratón), agrupadas por categoría; las
  opciones (p. ej. muestras de MSAA) debajo. `*` = hace falta reiniciar.
- `mods.cfg` junto al ejecutable (se crea al cambiar algo en el menú):
  ```
  graphics.msaa = on
  graphics.msaa.samples = 4x
  water.living = on
  data.MiPackDeTexturas = on
  ```
- Línea de comandos, solo para esa sesión (no se guarda): `--mod water.living`, `--mod graphics.msaa=off`,
  `--mod graphics.msaa.samples=8x`. Los interruptores anteriores siguen como atajos: `--msaa N`, `--mipmaps`,
  `--anisotropic`, `--enhanced-graphics` (= MSAA 4× + anisótropo), `--living-water`, `--ground-static-objects`.

## Mods integrados

| Id | Qué hace | Reinicio |
|---|---|---|
| `graphics.msaa` (+ `samples` 2x/4x/8x/16x) | Antialiasing multimuestreo y alpha to coverage en hojas y vallas | no |
| `graphics.mipmaps` | Mipmaps y filtrado trilineal | sí |
| `graphics.anisotropic` | Filtrado anisótropo (incluye los mipmaps) | sí |
| `graphics.terrain-x2` (+ `repeat` x1/x2/x3/x4, `upscale` off/on, `cliffs` triplanar/stretched) | Terreno más nítido: cada material repetido 1-4 veces por bloque (por defecto x2; wrap Repeat), escalado ×2 con Lanczos-3 al cargar (`Graphics/TextureUpscale`, con wrap: los materiales del LND son tileables, primera y última fila/columna idénticas) y acantilados triplanares: el original proyecta todo desde arriba (uv = posición xz del bloque) y en las pendientes la textura se estira en rayas; con `triplanar` se mezclan también las proyecciones a lo largo de x y z (pesos \|n\|⁴, normal suave por vértice de diferencias centrales de altitud, `LandVertex::normal`). Los materiales que son un dibujo único por bloque y no una textura (el geoglifo de la figura: Land1 material 10 y Land5 material 5; el laberinto: Land5 material 1) se quedan en ×1. Nada en el LND los marca (su `type` 18/11 lo comparten hierbas normales, y la métrica de contraste a gran escala no los separa de una roca nevada), así que se reconocen por hash FNV-1a de sus texels (`IsPictureMaterial` en LandIsland.cpp) y viajan en el byte `w` de los ids de material del vértice (bits 0-2). Si un mod de datos trae otro dibujo, hay que añadir su hash (`dev\lnd_hash.py`). Solo escalar apenas se nota: cada material de 256 px cubre un bloque de 160 unidades | sí |
| `water.living` | El mar refleja todo, el reflejo ondula despacio en bucle y la superficie deriva (sin la ondulación por filas) | no |
| `world.ground-statics` | Baja las rocas y objetos estáticos que flotan hasta el suelo | no |
| `world.foliage` (+ `density` low/medium/high/very high = ×0.5/1/2/4, `distance` near/medium/far = 120/200/320) | Hierba, flores, juncos y matorrales sobre el terreno (billboards instanciados, `3D/Foliage`). Reglas e imágenes en `<exe>/ModAssets/Foliage/` (`foliage.cfg`; plantilla en el repo `assets/mods/Foliage/foliage.cfg`; imágenes originales del usuario en `B&W/Asstes_mods`). Detalles abajo | no |

Detalles de cada uno en [rendering.md](rendering.md) y [openblack-internals.md](openblack-internals.md).

## Mods de datos

- Una carpeta por mod en `Mods/` junto al ejecutable, con la misma estructura que el juego (`Data/...`,
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

1. Una clase derivada de `mods::Mod` en el archivo de su categoría (`GraphicsMods.cpp`, `WaterMods.cpp`,
   `WorldMods.cpp`, o uno nuevo con su `Register…Mods` en `BuiltinMods.h`).
2. `Info`: id estable (`categoria.nombre`), nombre, descripción, categoría, `restartRequired`.
3. Opciones con `AddOption({"id", "Etiqueta", {"elección1", "elección2"}, índicePorDefecto})`; se leen con
   `GetChoice("id")`.
4. `Apply()`: pone en marcha el estado actual. Se llama al arrancar (después de `mods.cfg` y la línea de comandos) y
   cada vez que el mod o una opción cambia. Lo normal es escribir un interruptor de `EngineConfig` que lee el motor.
5. El motor nunca decide por su cuenta: todo lo que no es original mira un interruptor que solo pone un mod.

Pendiente (nivel 3): mods externos (Lua o DLL) sobre esta misma API.

## world.foliage: plantas sobre el terreno

- Una sección `[nombre]` por planta en `foliage.cfg`: `images` (png, uno al azar por planta), `texture` (aspecto de la
  textura: green/dry/sand/rock/snow), `terrain` (tipo del LND, `TerrainMaterialType`), `per_cell` (por celda de 10×10
  con densidad media), `size` (ancho mín-máx; el alto sale de la proporción de la imagen), `altitude`, `slope` (grados),
  `patches` (0 uniforme .. 1 solo en manchas, ruido de valor a escala 45), `sway` (viento), `lean` (inclinación máxima
  al azar) y `tint` (grey/all/none). Crece si cumple `texture` o `terrain`.
- **Tinte por el suelo**: los texeles grises (saturación < 0,1-0,2) toman el color de la textura del terreno bajo la
  planta: el vertex shader muestrea el array de materiales en el mismo material y uv que el terreno (uv del bloque ×
  repeticiones del mod terrain-x2, mip 3); gris 0,5 = el suelo tal cual, más oscuro en la base y más claro en la punta.
  Los texeles de color (pétalos, espigas) no cambian. `tint = all` tinta toda la imagen; `none` usa sus colores.
- Sprites: `gen_*` los genera `dev\gen_grass_sprites.py` (hojas grises curvas y afinadas, flores de pétalos
  saturados); `mono_*` son los del usuario (`B&W/Asstes_mods`) con lo verde (tono 32-170°) pasado a gris con media
  0,62; los brillos y bordes poco saturados (s <= 0,12, v < 0,85) también a gris y solo los casi blancos (v >= 0,85) con un toque crema para que no se tinten (`dev\mono_sprites.py`). La base de cada imagen se recorta irregular por columnas (hasta el 9 % del alto) para que no se vea el borde recto. El trigo queda en color.
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
