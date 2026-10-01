# Paquetes modificados

El `AllMeshes.g3d` de la instalación es un mod del usuario, no el paquete original: aquí están sus diferencias con el
original (texturas incrustadas, origen de las rocas, texturas de aldeanos), lo averiguado sobre las mallas de aldeanos
y el mod de openblack **HD-Tweaks** que las mejora. La librería de mods en general está en
[mod-library.md](mod-library.md).

- [El paquete de la instalación](#el-paquete-de-la-instalación)
- [Texturas incrustadas](#texturas-incrustadas)
- [Origen de las mallas (rocas flotantes)](#origen-de-las-mallas-rocas-flotantes)
- [Aldeanos: mallas y texturas](#aldeanos-mallas-y-texturas)
- [Mod HD-Tweaks](#mod-hd-tweaks)
  - [Opciones](#opciones)
  - [Recarga en vivo y detalles técnicos](#recarga-en-vivo-y-detalles-técnicos)
  - [Pruebas](#pruebas)
  - [Estado](#estado)

## El paquete de la instalación

- El `Data\AllMeshes.g3d` actual es un mod del usuario (626 mallas, texturas 0x01–0x70). No hay copia del paquete
  base.
- Referencias de mallas originales: el paquete de Creature Isle (`...\CreatureIsle\Data\AllMeshes.g3d`, **otros
  índices**: consultar su `AllMeshes.h`).
- El paquete `Ultimate\Data\AllMeshes.g3d` es otro mod (704 mallas).

## Texturas incrustadas

- Las palmeras (mallas 586–589) llevan su textura incrustada con id **0x1001**, pero su material pide 0x85–0x88, que no
  existen en el paquete. Ultimate repite el patrón. En el juego original se ven bien; openblack usa la skin
  incrustada de la malla cuando el material no encuentra su textura.
- En `LH3DMesh::Create` (0x806460) las skins incrustadas se registran con `fn_008379E0`, y el valor 0x1001 podría ser
  formato + id (**inferido**).
- Escáner: `python C:\Users\diewgarc\dev\tmp_dis\mod\scan_skins.py <pack.g3d> <salida.txt>` lista las mallas cuyos
  materiales piden texturas que no están ni en el paquete ni incrustadas.

## Origen de las mallas (rocas flotantes)

- Las mallas de rocas del mod tienen el origen desplazado respecto a las originales:
  `MSH_Z_SPELLROCK01` vértice más bajo +0.8 (original −0.6), `MSH_BOULDER3_LIME` +0.2 (original −0.2).
- Las altitudes de los scripts están pensadas para las mallas originales, así que con el mod quedan en el aire (en
  Land1, 74 de 243 objetos estáticos flotan más de 5 cm). El original no lo corrige.
- Además, muchas rocas del mod quedan apoyadas en una punta por su giro: aunque el vértice más bajo toque el suelo,
  parecen flotar. Solución prevista: físicas (**sin comprobar** si ya se hizo).

## Aldeanos: mallas y texturas

Investigación del mod "hd people" (2026-09-29). Scripts en `C:\Users\diewgarc\dev\tmp_dis\hdpeople\` (compare.py,
anims.py, summary.txt).

**Mallas**

- Mallas `MSH_P_*` 413–524 del paquete base (geometría igual a la de Creature Isle, así que no las toca el mod del
  usuario). Cada tribu y sexo tiene **su propia malla**, en 3 niveles: `_1` ~260–300 vértices, `_2` ~110–150, `_3`
  26–40. La textura es una por tribu, compartida por hombre y mujer (0x4E–0x5C). Algunas mallas repiten geometría con
  otra textura (TIBETAN=TIBT, JAPANESE=JAPN, SHAOLIN_MONK=JAPN_M_A_1, niñas TAN/WHITE, INTRO_M=CULT_PRIEST).
- `MSH_P_INTRO_M`/`_F` (483/484) **no tienen más polígonos**: 258 v / 348 t, como un aldeano normal. INTRO_M es la
  geometría de CULT_PRIEST con la textura 0x59; INTRO_F usa el esqueleto de las aldeanas. Sus texturas son de 256².
  `INTRO.bik` es un vídeo prerenderizado: sus modelos no están en los archivos.
- Esqueleto: 110 de las 112 mallas tienen la misma jerarquía de 22 huesos (EGPT_M_B_2 tiene 21), y todas las
  animaciones `M_P_*` de AllAnims.anm (232) son de 22 huesos. Las poses de reposo varían un poco (grupos: 85 mallas,
  12 femeninas, 10, CULT_PRIEST+INTRO_M).
- Qué malla se dibuja: el original, siempre el LOD 1 (las cargas de LevelOfDetail están anuladas): `stdDetail` /
  `childMeshMedium` para aldeanos (unos 5,7 KB de malla frente a 12 KB de la alta) y `std` para animales. openblack
  hace lo mismo desde 2026-09-30 (**fiel**); la malla alta, el doble de triángulos, solo con el mod HD-Tweaks
  `detail = high` (`ECS/DetailMeshes`). Historia: antes openblack usaba solo `highDetail` (sin LOD) y dibujaba a los
  aldeanos en pose de reposo (Renderer.cpp, "Get animation frame instead of default"; L3DAnim cargaba AllAnims pero
  no había reproducción, hoy ver [animation.md](animation.md)).

**Texturas** (hoja de contacto en `dev\tmp_dis\hdpeople\tex\sheet.png`)

- Todas son atlas de 256² **nativos**. Las que el paquete del usuario tiene a 512 o 1024 son ampliaciones con
  píxeles duplicados (el error frente a doblar su mitad es < 1,5 niveles), salvo 0x5A y 0x47 (512 nativas).
- INTRO_M/F se ven más finos porque su textura de 256² es de **un solo** personaje (los aldeanos: 4 por atlas).
  Ningún paquete (base, Creature Isle, Ultimate) trae mejores.
- **El atlas de los nórdicos del paquete del usuario (0x5A, 1024 px) no es el original**: tiene pintados los
  personajes de la intro (la cara de la mujer rubia de INTRO_F, el hombre de barba, uno con camisa roja y vaqueros).
  El atlas nórdico de Creature Isle (skin 0x74 de su paquete, `MSH_P_NORS_F_A_1` = 580 allí) tiene la ropa original
  (vestido oscuro, hombres de negro con cinturón). Por eso las aldeanas de Land1 (pueblo nórdico) parecen "las de la
  intro": la malla (`NORS_F_A_1`, 498) y el clip (`M_P_Walk_Woman`) son los correctos. Las texturas HD de
  graphics.hd-tweaks salieron de ese atlas. Comparación en `dev\tmp_dis\hdpeople\tex\norse_cmp.png`.

## Mod HD-Tweaks

**Mod/propio**: `graphics.hd-tweaks` (2026-09-29/30), antes `graphics.hd-people`; renombrado porque ya no es solo para
aldeanos. Tabla de opciones en [mod-library.md](mod-library.md). Todo se aplica **en vivo** (sin reiniciar),
desactivado por defecto como todo mod.

### Opciones

- `textures` hd/original: los 18 atlas de aldeanos y los 5 de animales (0x2-0x5, 0x64; 2026-09-30) ×4
  (Real-ESRGAN), `Resources/HdTextures` + `textures.cfg` (hash FNV-1a del DDS de origen: con otro AllMeshes.g3d no se
  usan). Los atlas de animales los comparten algunos objetos (lápidas, una puerta, un tipi...), que también salen en
  HD.
  - Generación: Real-ESRGAN `realesrgan-x4plus` (el modelo anime aplana la pintura) desde la resolución nativa, con
    `python assets\mods\graphics.hd-tweaks\tools\make_textures.py <AllMeshes.g3d> <AllMeshes.h> <carpeta del mod>`
    (Real-ESRGAN portable en `C:\Users\diewgarc\dev\tools\realesrgan`, ~3 min con la GPU); no están en git. El
    generador lee las mallas `MSH_P_*` y `MSH_A_*` del AllMeshes.h del juego y reutiliza las imágenes ya hechas cuyo
    hash sigue siendo el del paquete.
- `smooth` off/soft/round: triángulos PN (`3D/PnTessellation`, Vlachos 2001) partidos en 4 o 9 sobre las mallas de
  aldeanos y animales (con huesos y todas sus texturas en la lista) **y la mano** (`Hand_Boned_Base2`).
  `L3DSubMesh::IsHdTweaked`.
  - Los **triángulos de articulación** (esquinas en huesos distintos) no se curvan por dentro: son un abanico sobre
    su arista de un solo hueso y se estiran como los del original (con puntos interiores pegados a un hueso se
    doblaban al animar).
  - La colisión (mano, físicas) sigue siendo la malla original.
- `light` smooth/original: la luz del original (la regla entera de `fn_0084BA90` con las mismas funciones de
  `assets/shaders/model_light.sh`, el mismo ambiente y la misma luz; ver
  [Luz de los modelos](rendering-objects.md#luz-de-los-modelos)) calculada por píxel en `fs_object` con las normales
  suaves (`u_window.y`), solo en instancias iluminadas como el original (no reflejos ni sombras). Por píxel la dirección
  se toma en el mundo, del píxel a la luz, en vez de en el espacio de la malla **(aproximado)**: no queda varying libre.
  **Probado y descartado**: un borde de luz en la silueta, 0,8·(1−N·V)² ("rim"); al usuario le pareció feo.
- `sharp` on/off: sesgo de mip −1 en esas texturas (`u_window.z`).
- `detail` high/original: aldeanos y animales con su malla alta (`ECS/DetailMeshes`); sin el mod, el LOD 1 del
  original (ver [Aldeanos: mallas y texturas](#aldeanos-mallas-y-texturas)).

### Recarga en vivo y detalles técnicos

- **Recarga en vivo** (`resources::hd_tweaks::Update`, al principio de `Game::Update`): si cambian las opciones relee
  AllMeshes.g3d y recarga solo las texturas de aldeanos, las mallas con huesos que las usan y la mano (~0,6 s al
  activar, ~0,15 s al desactivar; las PNG se decodifican en paralelo, también al arrancar). `detail_meshes::Update`
  cambia la malla de los aldeanos y animales que ya existen.
- **Visibilidad a distancia**: a 20-40 m un aldeano mide 40-70 px y las texturas ×4 solas casi no se notan; lo que se
  nota es la forma redonda + luz por píxel + `sharp`.
- **Variantes de shader de 32 huesos** (sesión mapa, `vs_object_instanced_b32.sc`): incluyen `vs_object.sc` y usan
  `fs_object`, así que el mod funciona igual por ese camino (comprobado con capturas).
- **Carpeta antigua**: si aparece `Mods/graphics.hd-people` (un exe viejo o una copia de `Mods`), `ModRegistry`
  (`MigrateRenamedFolder`) pasa a `graphics.hd-tweaks` lo que le falte y la borra; nunca sale como mod de datos.

### Pruebas

- `OPENBLACK_TEST_HD_TWEAKS=<frame>:<textures>,<smooth>` cambia las opciones a mitad de partida.
- `dev\tools\shot_villager.sh`, `dev\tools\shot_hand.sh` y `dev\tools\shot_animal.sh <n,distancia,ángulo,1>` (copia
  privada en `dev\hdp_run`; los animales solo se siguen con el juego en marcha, sin START_PAUSED).
- `OPENBLACK_START_PAUSED=1` deja a los aldeanos quietos para comparar A/B; `OPENBLACK_TEST_ANIM=<clip>,<ms>` para una
  pose (sentado 369, rezar 343).
- Las capturas en el fotograma 2900 fallan a veces: repetir.

### Estado

- **Comprobado por el usuario** (2026-09-30): con `round` las animaciones ya no se rompen, tampoco las de los animales.
- **Pendiente**: comprobar en juego si `sharp` parpadea en movimiento.
