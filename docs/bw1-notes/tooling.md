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
