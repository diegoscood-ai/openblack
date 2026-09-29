# Paquetes modificados

El `Data\AllMeshes.g3d` actual es un mod del usuario (626 mallas, texturas 0x01–0x70). No hay copia del paquete base.
Referencias de mallas originales: el paquete de Creature Isle (`...\CreatureIsle\Data\AllMeshes.g3d`, **otros
índices**: consultar su `AllMeshes.h`). El paquete `Ultimate\Data\AllMeshes.g3d` es otro mod (704 mallas).

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
  parecen flotar. Solución prevista: físicas.

## Aldeanos (investigación del mod "hd people", 2026-09-29)

- Mallas `MSH_P_*` 413–524 del paquete base (geometría igual a la de Creature Isle, así que no las toca el mod del
  usuario). Cada tribu y sexo tiene **su propia malla**, en 3 niveles: `_1` ~260–300 vértices, `_2` ~110–150, `_3` 26–40.
  La textura es una por tribu, compartida por hombre y mujer (0x4E–0x5C). Algunas mallas repiten geometría con otra
  textura (TIBETAN=TIBT, JAPANESE=JAPN, SHAOLIN_MONK=JAPN_M_A_1, niñas TAN/WHITE, INTRO_M=CULT_PRIEST).
- `MSH_P_INTRO_M`/`_F` (483/484) **no tienen más polígonos**: 258 v / 348 t, como un aldeano normal. INTRO_M es la
  geometría de CULT_PRIEST con la textura 0x59; INTRO_F usa el esqueleto de las aldeanas. Sus texturas son de 256².
  `INTRO.bik` es un vídeo prerenderizado: sus modelos no están en los archivos.
- Esqueleto: 110 de las 112 mallas tienen la misma jerarquía de 22 huesos (EGPT_M_B_2 tiene 21), y todas las
  animaciones `M_P_*` de AllAnims.anm (232) son de 22 huesos. Las poses de reposo varían un poco (grupos: 85 mallas,
  12 femeninas, 10, CULT_PRIEST+INTRO_M).
- openblack: los aldeanos usan solo `highDetail` (sin LOD) y se dibujan en pose de reposo (Renderer.cpp, "Get animation
  frame instead of default"); L3DAnim carga AllAnims pero no hay reproducción.
- Texturas (hoja de contacto en `dev\tmp_dis\hdpeople\tex\sheet.png`): todas son atlas de 256² **nativos**. Las
  que el paquete del usuario tiene a 512 o 1024 son ampliaciones con píxeles duplicados (el error frente a doblar su
  mitad es < 1,5 niveles), salvo 0x5A y 0x47 (512 nativas). INTRO_M/F se ven más finos porque su textura de 256² es
  de **un solo** personaje (los aldeanos: 4 por atlas). Ningún paquete (base, Creature Isle, Ultimate) trae mejores.
- Mod `graphics.hd-people` (mod-library.md): texturas ×4 con Real-ESRGAN `realesrgan-x4plus` (el modelo anime aplana
  la pintura) desde la resolución nativa. Se generan con
  `python assets\mods\graphics.hd-people\tools\make_textures.py <AllMeshes.g3d> <AllMeshes.h> <carpeta del mod>`
  (Real-ESRGAN portable en `C:\Users\diewgarc\dev\tools\realesrgan`, ~3 min con la GPU); no están en git.
- Scripts: `C:\Users\diewgarc\dev\tmp_dis\hdpeople\` (compare.py, anims.py, summary.txt).
