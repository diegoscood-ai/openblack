# Herramientas y formatos

## Desensamblado (`C:\Users\diewgarc\dev\tmp_dis`)

- `python bwdis.py ADDR:SIZE [ADDR:SIZE...]` desensambla `runblack.exe` con capstone. Anota símbolos, floats de la
  sección de datos (`; =0.67`) y cadenas.
  - Las llamadas virtuales `call [reg + off]` se anotan con la vtable de la clase `VTC` (variable de entorno, por
    defecto `Tree`). **El nombre anotado solo vale si `VTC` es la clase real del objeto**; si no, ignóralo.
- `python callers.py ADDR` lista quién llama a una función (busca `call rel32`).
- `python refs.py ADDR...` lista instrucciones que referencian una dirección (datos o funciones).
- `multi\vt.py Clase [regex]` imprime la vtable de una clase; `multi\potinfo.py` lee la tabla de vasijas de `info.dat`.
- Los símbolos vienen de `bw1-decomp\config\BW1W120\symbols.txt`. Algunos nombres están cambiados, por ejemplo:
  `0x5B3C70` es `HandStateHolding::Update` y `0x5B5E70` es `ObtainRequiredHandPosition`.
- Truco: para leer tablas que se rellenan al arrancar (en `.data` están a cero), busca el inicializador `crt_xc_fn_*`
  que escribe en ellas (ej. la tabla de estados de acción de la interfaz, 0x5D7960).

## Herramientas de openblack (en `cmake-build-presets\ninja-multi-vcpkg\bin\Release`)

- `packtool -M pack.g3d` lista mallas; `packtool -m N -e out.l3d pack.g3d` extrae la malla N;
  `packtool -T pack.g3d` lista texturas (**ids en hexadecimal**); `packtool -t IDX -e out.dds` extrae la textura por
  índice (el id es otro campo).
- `l3dtool read -H|-m|-P|-V|-I|-s file.l3d`: cabecera, submallas, primitivas (material, skinID), vértices (posiciones
  con **un decimal**), índices, skins incrustadas.
- `lndtool write ... --points "x y z"`: genera el terreno de prueba (ver tests en openblack-internals.md).

## Formatos de datos

- `Data\AllMeshes.g3d`: paquete Lionhead con ~626 mallas L3D y sus texturas DDS. El número de malla es el índice del
  paquete, que coincide con el enum de `Data\AllMeshes.h` **solo para ese paquete** (Creature Isle y otros paquetes
  tienen otros índices).
- L3D: cabecera de 19 u32 (magic, flags, size, submeshCount, submeshOffsets, bbox[8], another, skinCount,
  skinOffsets, extraCount, extraOffset, footprintOffset). Las skins incrustadas empiezan con un u32 de id seguido de
  256×256 píxeles de 16 bits.
- Efectos: `Data\Spells\ZSpellFiles\*.zzz` = zlib a partir del byte 4; dentro, un fichero de propiedades de texto
  (`BEGINCLASS` / `PROPERTY`). Ejemplos: `SF_GripLandscape`, `SF_MultiPickUpWood/Food/FoodFish`, `SF_MultiPutDown*`.
- Texturas sueltas `Data\Textures\X.raw` (256×256 RGB) + `Xa.raw` (alfa R8). Hojas de sprites de 8×8 celdas.
- Scripts del mapa `Scripts\LandN.txt`; la tabla de comandos del ejecutable (0xC21190…) da nombre y tipos de
  parámetros (`A` posición, `N` entero, `F` float). Ej.: `CREATE_MOBILE_STATIC` = `ANFFFFF` =
  (pos, tipo, altitud, ángulo X, ángulo Y, ángulo Z, escala).
- `Scripts\info.dat`: tablas de objetos (pots, trees, mobile statics...). openblack lo carga en `InfoConstants`.

## LND y mapas de BWLandEditor

`B&W\BWLandEditor-main` es el editor de mapas de Daniels118 (Java, GPL-3). Parte de su código está portado de openblack:
InfoConstants, L3D/G3D y una versión antigua de `fs_terrain`.
- **Cómo lee el LND.** Igual que openblack, más tres extensiones que openblack ya admite (`LNDFile`, `LandIsland`):
  - Al final del fichero pueden venir los bloques `EXT0` (u32 tamaño del bloque entero = 10, u8 versión, u8 bits de
    altitud 8-16) y `META` (u32 tamaño de los datos, datos del editor). Con más de 8 bits, los bits altos de la
    altitud van en los bits bajos de `saveColor` (`LNDCell::Altitude`, `LandIslandInterface::GetCellAltitude`).
  - La cuadrícula puede tener hasta 128×128 bloques y más de 255 bloques. La tabla de la cabecera solo cubre 32×32 e
    índices < 256, así que `LandIsland` monta su tabla con `blockX`/`blockZ` de cada bloque. En los 21 `.lnd`
    originales la tabla coincide con esos campos (`dev\tools\lnd\lnd_check.py`).
  - El editor corrige un `mapX`/`mapZ` que no cuadre con `blockX`/`blockZ`, y `LandIsland` hace lo mismo.
- **Qué se adapta en openblack.** Con más de 8 bits el mapa de alturas pasa de R8 a R32F en la misma escala
  (1 = altitud 255). Por encima de 255 se usa el último material del país, como hace el editor.
  - Las texturas por isla (huellas, sombras estáticas, alfa) bajan de 256 texels por bloque en cuanto pasarían de 8192.
  - El disco que limita la cámara (centro 2560, radio 5120) crece con el tamaño del mapa.
  - Los mapas originales no cambian.
- **Pruebas.** `dev\tools\lnd\lnd_make_tests.py` genera en `dev\lnd_test` tres mapas y sus guiones (arrancar con `-s` y la ruta
  absoluta del `.txt`):
  - `Land1_ext`: Land1 con los bloques del editor al final; idéntico a Land1, altura en (1788.4, 2710) = 28.9173050.
  - `Land1_hi`: 10 bits y altitudes dobladas; altura 57.8346100.
  - `Land5_x2`: Land5 dos veces, cuadrícula de 60, 374 bloques.
- **Byte `flags` de la celda, según el editor.** Bit 0 = "transparent"; bits 1-7 = sonido ambiente: 0 nada,
  2 chapoteo, 3 océano, 4 olas lentas, 5 lago, 6 costa, 7 olas rápidas, 8 jungla, 10 viento, 12 desierto, 14 pájaros,
  16 bosque, 18 río. Los impares por encima de 8 son variantes del par anterior. openblack lo usa en los filtros `zone`/`not_zone`
  de `world.foliage` (1579a51c, ver [mod-library.md](mod-library.md)).
- **Byte `properties` de la celda (+6).** Bits 0-3 = country, bit 4 (0x10) = hasWater, bit 5 (0x20) = coastLine,
  bit 6 (0x40) = fullWater, bit 7 (0x80) = split (diagonal, ver [engine-math.md](engine-math.md)). Para separar el
  mar abierto del agua interior, `lnd_water.py` agrupa las celdas conectadas con agua o sin bloque: las que tocan el
  borde del mapa o el vacío son mar; las demás, lagos o charcas.
- **Scripts de análisis de `.lnd`** (`dev\tools\lnd\`): `lnd_check` (tabla de bloques frente a blockX/blockZ),
  `lnd_make_tests` (mapas de prueba), `lnd_beaches` (arena junto al agua, materiales 6 y 11), `lnd_zones` /
  `lnd_countries` / `lnd_find_country <lnd> <n>` (zonas de sonido, countries y posición mediana de un country),
  `lnd_materials` / `lnd_colours` / `lnd_tile_check` / `lnd_decal_metric` (materiales RGB555 de 256×256),
  `lnd_water` (cuerpos de agua), `lnd_hash` (FNV-1a de los materiales).
- **Texturas de baja resolución.** Atlas de 4×4 subtexturas de 64×64, una por bloque, 4 texels por celda, con X e Y
  intercambiadas. El "unknown" de su cabecera es el número de bloques del atlas. `iu_lrs`/`iv_lrs` son enteros
  (0/64/128/192). openblack no las usa.
- **Diferencias sin comprobar en el original:**
  - El editor elige el material con `min(altitud + ruido/4, 255)`; openblack usa `(altitud + ruido) % 256`.
  - El lector L3D de openblack toma ancho y alto de huella de la cabecera; el editor los lee por entrada.
  - El editor lee info.dat de Creature Isle (627250 bytes; tablas más largas en InfoConstants.java L23-33); openblack
    todavía no.
