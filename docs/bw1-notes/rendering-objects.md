# Render de los modelos: original frente a openblack

Cómo se dibujan los objetos del mundo: materiales L3D, luz de los modelos, texturas y sprites, manchas de los pies,
la cola única de transparentes, reflejos en el mar y cortes por el plano del agua, bancos de peces, sombras de los objetos y de la mano, LOD, el humo
de las chimeneas, los objetos que miran a la cámara (billboards), las texturas animadas por fotogramas, las mallas pegadas al suelo y los modos de render y materiales del original. El render del mundo (terreno, mar, cielo, neblina) está en [rendering.md](rendering.md); el agua
como juego, en [water.md](water.md).

- [Mezcla de materiales L3D](#mezcla-de-materiales-l3d)
- [Luz de los modelos](#luz-de-los-modelos)
- [Aritmética de LH3DColor](#aritmética-de-lh3dcolor)
- [Repetición o recorte de texturas](#repetición-o-recorte-de-texturas)
- [La cola única de transparentes (LH3DZSorter)](#la-cola-única-de-transparentes-lh3dzsorter)
- [Manchas de aldeanos, reflejos de objetos y LOD](#manchas-de-aldeanos-reflejos-de-objetos-y-lod)
- [Submallas de física y de LOD 0](#submallas-de-física-y-de-lod-0)
- [La pasada bajo el mar (`graphics::sea_pass`)](#la-pasada-bajo-el-mar-graphicssea_pass)
- [Reflejos de objetos y sombra de la mano sobre objetos](#reflejos-de-objetos-y-sombra-de-la-mano-sobre-objetos)
- [Cortar por el plano del agua (`DrawCutByPlane`)](#cortar-por-el-plano-del-agua-drawcutbyplane)
- [Bancos de peces de las piscifactorías](#bancos-de-peces-de-las-piscifactorías)
- [Sombras de los objetos físicos](#sombras-de-los-objetos-físicos)
- [Sombra dinámica de la mano](#sombra-dinámica-de-la-mano)
- [Animales: manchas y malla](#animales-manchas-y-malla)
- [Humo de las chimeneas (LH3DSmoke)](#humo-de-las-chimeneas-lh3dsmoke)
- [Objetos que miran a la cámara (billboards)](#objetos-que-miran-a-la-cámara-billboards)
- [Texturas animadas por fotogramas](#texturas-animadas-por-fotogramas)
- [Mallas pegadas al suelo (land_morph)](#mallas-pegadas-al-suelo-land_morph)
- [Modos de render y materiales (render_modes)](#modos-de-render-y-materiales-render_modes)
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

Estado: todo lo de esta página es **fiel** (original, no un mod) salvo lo que se marca **(aproximado)**, las
desviaciones que se dicen en cada sección y lo que está en [Pendiente](#pendiente).

## Mezcla de materiales L3D

**Fiel** (del original, no es un mod).

- `L3DSubMesh` traduce el tipo de material a `blend`/`depthWrite`/`thresholdAlpha`, pero el renderizador no usaba
  `blend`: todo salía opaco. Ahora los materiales con mezcla y sin corte de alfa (`AlphaTextured`, `TexturedAlpha`,
  `SmoothAlpha`, `*Nz`, aditivos sin chroma) se dibujan con el alfa de la textura (`u_skyAlphaThreshold.w`),
  `SRCALPHA/INVSRCALPHA` (o `SRCALPHA/ONE` los aditivos), sin escribir Z en los `Nz`, en la vista `MainBlended`
  (después de todo lo opaco). Los `TexturedChroma` siguen con prueba de alfa. Los estados de cada tipo salen de
  `render_modes` ([Modos de render y materiales](#modos-de-render-y-materiales-render_modes)).
- La mano (`Hand_Boned_Base2`, material `AlphaTextured`) tiene en su piel un degradado de alfa en las filas de abajo:
  la muñeca se desvanece. Antes acababa en un borde blanco duro ([img/hand_zoom.png](img/hand_zoom.png)).
- Mallas de `AllMeshes.g3d` por tipo de material: `Textured` 512, `TexturedChroma` 217, `Smooth` 181,
  `AlphaTextured` 116 (casi todos los edificios `MSH_B_*`), `TexturedChromaAlpha` 6 (arbustos, palmeras).
- Gancho de pruebas `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fijo en fracción de la ventana (la mano aparece en capturas
  sin ratón real).

## Luz de los modelos

**Fiel** (del original, no es un mod).

- **Una sola regla, entera y por CPU** (`fn_0084BA90`, vértices D3DTLVERTEX), la de todos los modelos normales
  (edificios, aldeanos, árboles, rocas): `I = fistp(255 · N·L)` (0x84BBAF..0x84BBBE, redondeo al más cercano con los
  empates a par, el modo normal de la FPU), `f = I < 0 ? amb : amb + ((255 − amb)·I >> 8)` (0x84BBC3..0x84BBE5) con
  `amb` = [0xC39264] = 90, y el difuso por canal `(c·f) >> 8` truncado, con el alfa intacto (0x84BBEA..0x84BC1D). Así
  que `f` llega como máximo a 254/256, nunca a 1. Variante con truncado `__ftol` en vez de a par: 0x859649
  (`fn_00859530`, `fn_00859D90`, `fn_00878C70`).
- **La fórmula float de 166 no se ejecuta nunca** en esta compilación: es la ruta D3D `fn_0082C680`, a la que solo llega
  `LH3DObject::DrawTnL`, y `DrawTnL` pide el flag de T&L por hardware [0xECA60C] (0x80DC7A), que `OpenD3D` solo pone a 1
  si [0xC386E4] es 0 (0x82D0F5); `start_system` le escribe 1 sin condición (0x642EA7). Por eso openblack ya no la tiene.
- Color base del objeto (`fn_00801C90`), uno por objeto y fotograma: `tabla[luminosidad]` de las 4 celdas alrededor
  de su origen, bilineal; especular = RGB de esas celdas leído como D3DCOLOR (R y B cambiados; casi siempre 0).
- **La luz es un punto y se mueve con la hora.** LH3DTech guarda una sola luz, [0xEA9E90] (`SetLight` `fn_0081E1F0`;
  guardar y restaurar es cosa del llamador, 0x8254A3/0x82551F). `fn_005E5830`, que llama `GLandscape::Draw` (0x5E488E)
  una vez por fotograma antes de los modelos, la deja en el sol por defecto [0xEA1C88] = (−500000, 500000, −500000)
  (inicializador `__xc_a` `fn_00818920` 0x818930) salvo en **plena noche**: si el tipo de cielo es > 1,5 (el double de
  [0x8C5838]; `LH3DSky::Time2SkyType` 0x86A1B0 del tiempo visual calculado ahí, 0x5E58D1..0x5E58DF, 2 = noche;
  openblack: `sky_type::At(hora visual)`) la pone a 3 unidades ([0x8C2C50]) de la
  **mano** hacia la cámara, con la mano subida a por lo menos 10 ([0x8AB414]) sobre el terreno que tiene debajo.
  - Con el cursor fuera del terreno (en el cielo) el original sigue moviendo la mano por el rayo del ratón a su
    distancia de la vista (`ObtainRequiredHandPosition` 0x5B5E70; `CHand::fn_0046DF60` se queda con
    |cámara − posición| si el rayo no toca tierra). En openblack `HandSystem::Place` deja la mano donde se colocó por
    última vez (o en su sitio inicial), así que de noche la luz se queda allí y las caras de los modelos lejanos pueden
    quedarse casi solo con el ambiente: **diferencia conocida** (pendiente en el sistema de la mano). Para probar la luz
    de noche, poner el cursor sobre el terreno (`OPENBLACK_MOUSE_AT`). Sin mano o sin cámara, `Renderer::DrawScene`
    deja el sol por defecto (la rama de día, 0x5E5B70) **(inferido)**.
- **N·L va en el espacio de la malla, no con la normal girada**: `fn_00855340` lleva la luz al espacio del objeto con
  la inversa general de su matriz (`LHMatrix::SetInverse` 0x7FB290) y la normaliza (0xF03140); la rama con huesos hace
  `SetInverse` de cada matriz de hueso (0x84BD9E) sobre la luz ya pasada a cámara (0x84BDA3), que es lo mismo por
  hueso si esas matrices van del hueso a la cámara y la cámara se cancela **(inferido)**. La normal del vértice entra
  cruda, sin girar ni normalizar, y la dirección sale del **origen** del hueso o del objeto. Con escala uniforme da lo
  mismo que girar la normal; con escala por eje (mecer un árbol, la cizalla de un campo) no.
- El corte por plano (`DrawCutByPlane`, `fn_00858BA0`) usa la luz [0xF03140] en el espacio del **objeto**: los dos
  llamadores la ponen una vez con `fn_00855340` sobre obj+0x14 (estático 0x80C0EE, animado 0x811D2F) y `fn_00858BA0`
  la lee en sus dos ramas (rígida 0x858CB1, con huesos 0x859049); las matrices de hueso (0x858F77) solo mueven las
  posiciones. `vs_object` modo 4 usa solo la matriz de la instancia.
- Las nieblas y las nubes suben el ambiente a 210 mientras se dibujan (`fn_007FA300` 0x7FA56D, de vuelta a 90 en
  0x7FA586; la luz se guarda y se sube en 0x7FA53C..0x7FA563 y se restaura en 0x7FA590). Los objetos «sin luz» van
  por `fn_00856D40`/`fn_0085BA30`, con el color base [0xC37D8C] tal cual (0x856D89, 0x85BA68): el selector es el
  argumento edx de `fn_0080D910` (0x80D926 `test edx, edx`: `fn_0084BA90` si no es 0, `fn_00856D40` si es 0); que
  ese edx venga de la ranura vt+0x5C es **(inferido)**. Las primitivas con el bit 0x1000 (con [0xE9FE44]) no van por
  ahí: toman el color alternativo [0xC37D98] (`fn_0080AD90` 0x80ADBC, `fn_0080AF80` 0x80AFAC) y se iluminan en
  `fn_00859530` (la regla con `__ftol`); qué es ese bit está **(no verificado)**.
- La mano: base × 1,5 (`CHand::AddDrawing` 0x46D135). Primitivas sin textura: color del material × base. Chroma:
  `ALPHAREF` = el umbral del material, `GREATEREQUAL`; solo los modos 9 / 15 de un objeto con alfa propio usan
  `umbral · alfa del objeto / 255 − 5` ([Modos de render](#modos-de-render-y-materiales-render_modes)).
- Las sombras estáticas **no** siguen esta luz: `fn_008721A0` (0x8721E1) y `fn_0080ECB0` (0x80EDA8) leen [0xEA1C88], el
  sol fijo, así que de noche siguen con el del día (`vs_static_shadow_instanced`).
- openblack: `LandIsland::CreateCellMap` (textura RGBA por celda: rgb = color leído como D3DCOLOR, a = luminosidad);
  `vs_object` hace la bilineal y la luz. Un solo sistema con una sola API: `src/Graphics/ModelLight.h`
  (`model_light::Light/SetLight/ScopedLight`, `Ambient/ScopedAmbient`, `UpdateFrameLight` = `fn_005E5830`,
  `LightInMeshSpace` = `fn_00855340`, `Apply` para las rutas por CPU) y su gemelo de GPU
  `assets/shaders/model_light.sh` (`ModelLightI`, `ModelLightFactor`, `ModelLightDiffuse`, `ModelLightLocal` y el
  uniforme `u_modelLight`: xyz la luz, w el ambiente). Lo usan `vs_object` (objetos, átomos de malla de PSys y el modo
  cut), `fs_object` (el mod hd-tweaks, por píxel) y `vs_cloud` (nubes y nieblas). `Renderer::DrawScene` llama a
  `UpdateFrameLight` una vez por fotograma y `ECS/Trees.cpp` usa `model_light::Light()`.
- En `fs_object` el mod hd-tweaks usa las mismas funciones, pero con la normal interpolada del mundo contra la
  dirección del píxel a la luz **(aproximado)**: no queda ninguna varying libre para la luz local. Coincide solo con la
  luz lejos (de día, el sol a 500000); en plena noche, con la luz a 3 unidades de la mano, la dirección píxel→luz y la
  de origen→luz difieren mucho en un aldeano cercano (diferencia nocturna conocida del mod).
- `model_light::Intensity/Factor/Apply` aún no tienen llamador: esperan a las rutas por CPU aplazadas (`FragMesh`,
  primitivas de `fn_00859530`); `Renderer::DrawCloud` usa `ScopedLight` + `ScopedAmbient(k_MistAmbient)` y pasa
  `Ambient()` a `u_cloud.z`.
- **Trampa**: `vs_object` también lo usa el cielo (`fs_sky`); añadirle una varying nueva deja el cielo en blanco. El
  especular viaja en `v_texcoord0.zw` y `v_position.w` (después de calcular `gl_Position`).

## Aritmética de LH3DColor

**Fiel** (del original, no es un mod). Un LH3DColor es un D3DCOLOR, 0xAARRGGBB. El motor combina dos colores con dos
familias de operaciones, y **todas truncan, ninguna redondea**:

- `(c·t) >> 8` por canal: `imul` sobre el byte enmascarado y `shr 8`. Con t = 0xFF cada canal pierde 1 (0xFF → 0xFE).
- `c·l / 255` por canal: el truco 0x80808081 (`sar 7` más el bit de signo, o `mul` y `shr 7`), que da exactamente
  trunc(x/255) para x de 0 a 65025.

Cada rutina tiene su propia regla para el alfa. Cuando el alfa se **conserva**, es siempre el del **primer**
argumento:

| Rutina | Qué hace | Alfa | openblack |
|---|---|---|---|
| `fn_0080BF10` difuso (0x80BFA3..0x80C00B) | `(c·t)>>8` → +0x4C | multiplicado (0x80BFC5..0x80BFD3) | `MulShr8_4` |
| `fn_0080BF10` especular (0x80BF1B..0x80BFB9) | `min(a+b, 255)` → +0x50 | sumado y saturado (`cmp 0xFF`/`jb`) | `AddSat_4` |
| `fn_00809D80` (solo 0x80A290: color del objeto × parte) | `(a·b)>>8` | el de a (0x809DCF) | `MulShr8_3KeepA` |
| `fn_00809DE0` (0x80A2A6: especular del objeto + parte) | `min(a+b, 255)` | el de a (0x809E46) | `AddSat_3KeepA` |
| `fn_0084BA90` 0x84BBEA, `Tree::Draw` 0x74B077, `fn_0074B3A0` | `(c·k)>>8` por un escalar | el del objeto (0x74B0BD, 0x74B4C9) | `ScaleShr8_3KeepA` |
| `fn_007ACF70` (0x7ACF79..0x7AD03C) | `trunc(a·b/255)` | multiplicado | `Mul255_4` |
| `LH3DMist` 0x7FA6C8..0x7FA75F | color × luz `/255` | el del color, no el de la luz (0x7FA753) | `Mul255_3KeepA` |
| `LH3DCreature::DrawNow` 0x48EF00..0x48EF98 | cuerpo × objeto `/255` | 0xFF (0x48EF8F) | `Mul255_3OpaqueA` |
| `fn_005E25C0` 0x5E2729..0x5E273B | borde de la nube × alfa `/255` | — (escalar) | `Mul255` |

Detalles leídos en el binario:

- Un campo: `Field::Draw` llama a `fn_0080BEC0` con el color del campo (BlendColor, alfa 0xFF en 0x528510), así que su
  alfa final es (0xFF·0xFF)>>8 = **254**. Con fuego (0x528809..0x528862) el tinte es antes ese color por el gris de
  carbonizado de `fn_00730570`, en los 4 canales (= `MulShr8_4`).
- La bola de un uso (0x518DDA y 0x519002): tinte `([0xBE8E8C] & 0xFF) << 24 | 0xFFFFFF` con [0xBE8E8C] = 0x00010196
  (sin escritor), así que el alfa es (0xFF·0x96)>>8 = 0x95.
- `SpellWolf::Draw`: +0x4C = `fistp(alpha) << 24 | 0xFFFFFF` (0x51C701..0x51C714) y la translucidez se decide con ese
  alfa crudo (0x51C71E..0x51C727); ardiendo, +0x4C × carbonizado en los 4 canales (0x51C751..0x51C7B7) y
  `fn_0080BEC0` con el brillo `fn_00730480`.
- **`Tree::Draw` no llama a `fn_0080BEC0`**: su +0x4C es `fn_00802120` (0x74AB1B) más la neblina `fn_007FEB30`
  (0x74AB60), y después el brillo [0xC22FA0] por `(c·k)>>8` con el alfa conservado (0x74B077..0x74B0C4). La neblina va
  **antes** del brillo, al revés que en `vs_object`.
- El árbol ardiendo (`fn_0074B3A0`): gris 50, o `ftol(255 − (1 − vida)·2550)` con un mínimo de 50 si vida > 0,9, y
  luego `min(gris, [0xC22FA0])` sin signo (`jb`, 0x74B47B..0x74B484); el alfa **se conserva** (0x74B4C9, confirmado).
- El gris de carbonizado `fn_00730570` es `255 − ceil(175k/256)` (0x730585..0x7305D7): k = 255 da 80.

openblack: `src/Graphics/Lh3dColour.h` (`lh3d_colour::`, sin estado, todo `constexpr`), con la regla del alfa en el
nombre (`_4`, `_3KeepA`, `_3OpaqueA`), más `Argb`, `Red/Green/Blue/Alpha` y las conversiones de bgfx sin original
(`ToAbgr(argb)`, `ToAbgr(argb, alfa)`, `ToAbgr(vec4)` redondeando y `ToVec4/ToVec3` = byte/255). El gemelo de GPU es
`assets/shaders/lh3d_colour.sh` (`Lh3dMulShr8`, `Lh3dAddSat`, `Lh3dMul255`, `Lh3dUnpackRgb24`), que incluyen
`vs_object.sc` (la columna de colores de la instancia, el color del corte y el color sin luz de `u_objectLight.z`) y
`vs_foliage.sc` (el color de las cosechas).
`Lh3dMul255` es floor((c·l + 0,5)/255): la división de un shader no redondea bien (a menudo x·rcp(255)) y un
floor(x/255) a secas puede dar k − 1 cuando x = 255k; con el + 0,5 la fracción queda en [0,002, 0,998]
(aproximado hasta probarlo en una GPU). `ScaleShr8_3KeepA` vale para cualquier k: las máscaras van tras cada `imul`
(0x74B099 / 0x74B09F / 0x74B0B2), así que los canales no se pisan; los llamadores pasan 0..255. `fn_0080BEC0` es
«dibujar con el color del terreno» por la propiedad `DrawWithLandscapeColor` del PSys (`Particle3DObj::DrawAt`
0x67A00C); symbols.txt la llama `GetPoisonColor@Pot`, y el nombre es nuestro (inferido). La luz
de modelos (`model_light::Apply`), el color de las nubes (`Clouds::Colour`, 0x5E1ECE..0x5E1F24), las neblinas
(`RendererMists`), la bola de un uso y las conversiones de `Renderer`, `RendererBoat`, `RendererSea`,
`RendererSmoke`, `Dust`, `GameFont` y `ScreenFade` ya lo usan, sin cambio visible. `test_lh3d_colour` compara
`MulShr8_4` con una emulación instrucción a instrucción de 0x80BFA3..0x80C00B y `Mul255` con las dos formas de
0x80808081 para todos los productos de dos bytes.

### Los campos de color del objeto en la instancia

**Transporte propio de openblack** (sin original): el motor guarda los colores en el LH3DObject (obj+0x4C difuso,
+0x50 especular, +0x54 ventanas) y los usa la CPU al iluminar; openblack los lleva por instancia a `vs_object`. Cada
instancia tiene **cinco columnas** (80 bytes, `i_data0..i_data4`): la matriz y una quinta con un float por campo, cada
uno un entero de como mucho 2^24 que el float guarda exacto. La escriben `lh3d_colour::PackInstance*`
(`src/Graphics/Lh3dColour.h`) en `RenderContext::instanceColours`, y `RenderingSystemCommon::UploadInstances` intercala
las dos listas en el búfer.

| Campo | Valor | Qué es |
|---|---|---|
| x (+0x4C) | 0 | la luz de tierra sola (`fn_00801C90` sin `fn_0080BF10`) |
| | −1 − rgb (`PackInstanceTint`) | un tinte t que multiplica la luz de tierra (`(c·t)>>8`): el de `fn_0080BF10`, o el propio de `Tree::Draw` (ver w) |
| | 1 + rgb (`PackInstanceColour`) | el color de `SetColorSpecular` 0x7F9770 (vt 0x2C), en lugar de la luz de tierra |
| y (+0x50) | rgb (`PackInstanceSpecular`) | el especular, 8 bits por canal, sumado con saturación al de la tierra (0x80BF1B..0x80BFB9) |
| z (+0x54) | 0 o 1 + rgb (`PackInstanceWindow`) | el color de las ventanas de `Abode::Draw` (vt 0x30, 0x516068); 0 = apagadas |
| w | 0 o 1 (`PackInstanceTreeTint`) | 1 = el tinte va después de la neblina: `Tree::Draw` no llama a `fn_0080BF10`, ilumina con `fn_00802120` (0x74AB1B), aplica la neblina (0x74AB60) y luego multiplica el +0x4C (0x74B077..0x74B0C4; ardiendo, `fn_0074B3A0` 0x74B48F..0x74B4D3) |

Así color y especular van a la vez (antes compartían el w de la cuarta columna y ganaba el último). Quién pasa qué
(`DrawColoursOf` en `RenderingSystem.cpp`, leído en cada Draw):

| Objeto | Tinte | Especular | Dirección |
|---|---|---|---|
| Aldeano | ardiendo: gris de carbonizado; si +0xD0 ≠ 0: blanco 0xFFFFFFFF; envenenado: 0xFFE8FFDD; si no, nada | brillo del fuego / +0xD0 / 0xFF001000 | `fn_0051B3D0` 0x51B402..0x51B488 |
| Animal | ardiendo: carbonizado; +0xD0 ≠ 0: blanco; si no, nada (no mira el veneno) | brillo / +0xD0 | `Animal::Draw` 0x51C4C6..0x51C51C |
| Lobo del milagro | siempre blanco (+0x4C = alfa << 24 \| 0xFFFFFF); ardiendo, × carbonizado en los 4 canales | brillo / +0xD0 | 0x51C709..0x51C7E1 |
| Vasija o pila envenenada sin fuego | 0xFFE8FFDD | 0xFF001000 | `Pot::Draw` 0x51BB8F..0x51BBA3, `PileFood::Draw` 0x51C191..0x51C1B8 |
| Icono de milagro de un centro (`TownCentreSpellIcon`) | blanco, siempre (con vida del centro > 0) | +0x10C (sin portar: 0) | `TownCentre::Draw` 0x5164A6..0x5164B2 |
| Icono de milagro de un lugar de culto | blanco solo si +0x10C ≠ 0 (0x519672..0x51967C, 0x5198A8); como +0x10C no está portado, nada | 0 | `SpellIcon::Draw` 0x519650 |
| Escudo físico | blanco | 0 | `PhysicalShield::DrawShield` 0x72D0D4 |
| Bola de un uso | blanco (su alfa va en `components::Alpha`) | 0 | 0x519002..0x51901E |
| Campo | su color; ardiendo, × carbonizado (`MulShr8_4`) | 0 / brillo | `Field::Draw` 0x528809..0x52888A |
| Árbol | brillo [0xC22FA0]; ardiendo, `TreeDrawColour`; los dos después de la neblina (w = 1) | 0 | `Tree::Draw` 0x74B077..0x74B0C4, `fn_0074B3A0` 0x74B48F..0x74B4D3 |
| Cualquier otro con fuego (edificios, rocas, árboles muertos, tótems...) | gris de carbonizado `fn_00730570` | brillo `fn_00730480` | `fn_00518050` (11 llamadores) y `DrawBuilding` 0x517FD4 |
| Bandas de poder | color del jugador (`SetColorSpecular`) | 0x141414 ([0xBE8EA0] = 20) | `DrawSpellGraphic` 0x51A370..0x51A3BE; la de la mano escribe los campos directamente (`PHandFX` Band::Draw +0x4C 0x68D87D / 0x68D8AB, +0x50 0x68D8B1; (inferido) sin `fn_00801C90` detrás) |
| Átomo de malla PSys | DrawData+8 (tinte con `DrawWithLandscapeColor`, si no `SetColorSpecular`) | 0 (aproximado: falta DrawData+0xC, leído en 0x67A012 y 0x67A023; se pierde en los dos caminos) | `Particle3DObj::DrawAt` 0x67A00C..0x67A02F |

El tinte blanco quita 1 a cada canal (`(c·255)>>8`): antes no se aplicaba. El alfa del tinte no se lleva. El dibujo
del LH3DObject copia el +0x4C entero a [0xC37D8C] (0x80DEF8; el +0x50 a [0xE9FE2C], 0x80DEFE) para todos los objetos,
y varias rutinas lo leen (`fn_007A4170` 0x7A6A2C, 0x7A7E85, `fn_00805CD0` 0x805EAA, `fn_00809E50`); que su alfa solo
cuente en los que se desvanecen es **(inferido)** (no se siguieron esas lecturas hasta el color del vértice). Para
esos openblack usa `components::Alpha` (aproximado: el escudo debería quedar en 0xFE y el lobo en (A·0xFF)>>8).
`test_lh3d_colour` comprueba los extremos (−2^24, 2^24, el negro, la ventana negra encendida) y 200 000 colores al azar
decodificados como en el shader.

**(aproximado)** El especular +0xD0 de los seres vivos: el original mira el dword entero con su alfa (0x51B416 /
0x51C4D6 `test eax,eax`), y el chakra de curación escribe alfa 0xFF (`fn_006A0E30` 0x6A0EF5), así que sus fotogramas
con rgb 0 siguen con el tinte blanco; openblack quita `SpecularColour` con rgb 0 (`PSys/Rules/Heal.cpp`) y esos
fotogramas van con la luz de tierra sola.

**(inferido)** Toda clase de las instancias que puede arder pasa por `fn_00518050` o `DrawBuilding`, o lleva el mismo
par en línea (el lobo 0x51C751); faltan por portar los pares en línea del FragMesh de la casa (0x5160AF), de
`Object::DrawOutOfMap` (0x51C839) y del objeto de predicción de la física (0x646F8C) (ver Pendiente).

## Repetición o recorte de texturas

**Fiel** (hecho).

- `fn_00850FC0` 0x851779: tras poner el modo, `SetD3DTillingOn` si `g_b_need_tilling` (0xECA614) o el bit 2 del byte
  +5 del material; si no, `SetD3DTillingOff` (CLAMP). `g_b_need_tilling` lo copia cada Draw del objeto 3D de su bit
  0x200 de Flags1 (vt+0xE4 = `fn_007F9B30`), y ese bit solo lo pone el setter vt+0xE0 desde mallas de partículas
  (`ParticleMeshCreatorAnimTextured`, `ParticleVolBlendMeshCreator`): los objetos del mundo dependen solo del material.
- `AllMeshes.g3d`: 1675 primitivas con el bit, 161 sin él; ninguna de estas tiene UV fuera de 0..1, así que el recorte
  solo cambia el filtrado bilineal en los bordes de la textura. openblack: `Primitive::wrap` y flags de sampler en
  `Renderer::DrawSubMesh`. Script: `documentacion\render\l3d_wrap_scan.py`.

## La cola única de transparentes (LH3DZSorter)

**Fiel**, con lo que se marca. API `graphics::zsorter`, en `src/Graphics/ZSorter.{h,cpp}` (núcleo del Renderer, sin
dueño de área). Informe: `dev\documentacion\unify2\lh3d_zsorter_original.md` (con su verificación); cambios:
`dev\documentacion\unify\U6_changes.md`.

**El original.** Todo lo que se dibuja con mezcla en el mundo pasa por **una sola cola**:

- `LH3DZSorter::NewZObject` 0x83F310 (32 llamadas en 31 funciones): lista enlazada en un búfer estático de 0x800
  entradas de 0x18 bytes (0xEDDD30; cuenta 0xEE9D34, cabeza 0xEE9D38). La entrada nueva va **delante de la primera con
  clave estrictamente menor** (`fld cur.clave; fcomp clave; test ah, 1`, 0x83F36A..0x83F376) o al final (0x83F39A):
  de lejos a cerca y, con claves iguales, primero la que llegó antes (estable). Con la cola llena se **pierde la
  nueva**, sea cual sea su clave (0x83F315/0x83F31C). Con una clave NaN, `fcomp` dice «menor» (C0).
- Reinicio `fn_0083F3B0` desde `StartFrame` 0x82F1F9. Vaciado `fn_0082F280` desde `FinishFrame` 0x82F480, una sola vez
  ([0xECA610]), de la cabeza al final, copiando el dato de usuario K en [0xEE9D30] (0x82F29D) antes de cada
  retrollamada. Va **después** de todo lo que se dibuja al momento en el fotograma y **antes** de las retrollamadas de
  fin de fotograma ([original-frame.md](original-frame.md)).
- La clave es `LH3DTech::GetValueForZSorter` (en línea en W120; Mac 0x010E7360): |P − g_camera|² en `float`
  (g_camera 0xEA1DB8). La suma x87 sigue el orden de las cargas, y con la FPU a 24 bits (fn_007DEE00, 0xFCFF en
  0x7DEE0D; desde `InitOneTimeOnly` y desde `Process3dEngine` tras `FinishFrame`, 0x54E426) cada paso redondea a
  `float` **(inferido: que nada entre fn_007DEE00 y los AddDrawing vuelva a subir la precisión; D3D7 sin FPUPRESERVE
  también la deja a 24 bits)**: (x² + y²) + z² en los sprites 0x840C70, las nieblas 0x7FA83C, el humo 0x7F8D3E, los efectos
  0x6797E5, la mano 0x46D1BD, fn_00813340 y fn_00679F60; **(x² + z²) + y²** en `LH3DObject::AddDrawing` 0x815F0F y en
  la lluvia 0x834215 (`zsorter::SumOrder`). Solo puede cambiar el último bit.
- El objeto `g_zsorter` (0xECA648, `fn_0083F2B0` en `OpenD3D` 0x82CDC1) no se lee nunca: no se porta.

**La API.**

| Función | Original |
|---|---|
| `Queue<Item>::Begin()` | `fn_0083F3B0` (y el `[0xECA610] = 0` de `StartFrame` 0x82F123) |
| `Queue<Item>::Submit(item, key, user = 0)` | `NewZObject` 0x83F310: inserción estable, tope `k_Capacity` = 0x800 |
| `Queue<Item>::Drain()` | `fn_0082F280`: las entradas de lejos a cerca, con su K; vacío si ya se vació |
| `Key(p, camera, order)` | `GetValueForZSorter`, en `float`, con el orden de suma del llamador |
| `PackRainUser` / `UnpackRainUser` | 0x834233..0x834259 / 0x833F83: alfa·65536 + 256·trunc(z/80) + trunc(x/80) |

**Qué entra y con qué punto** (`Renderer::DrawPass`, la cola `sorted`, solo en la vista principal):

| Qué | Original | Punto de la clave |
|---|---|---|
| modelos cuya malla tiene la marca 0x200, enteros (también los que se desvanecen) | `LH3DObject::AddDrawing` 0x815F53 | traslación de la instancia (+0x38), (x² + z²) + y² |
| la burbuja de la bola de un uso, siempre | 0x815F53 (bit forzado en 0x72A4AA) | `sortPoint` (`OneOffSpellSeed::Draw` 0x518E90) |
| la mano, entera | `CHand::AddDrawing` 0x46D203 | el origen +0x38 del LH3DObject [CHand+0x482C] (0x46D1B7); aquí la traslación de la instancia **(inferido: que sea ese origen)** |
| efecto `Sorted`: cada sprite | `LH3DSprite::AddDrawing` 0x840CB3, desde 0x67B0D2 | el sprite (`SortedFrame::sprites`) |
| efecto `Sorted`: cada átomo de malla, también los opacos y los cortados | `fn_00679F60` 0x679FC7, desde 0x67A246 | la traslación (+0x38..+0x40) |
| efecto `Sorted`: cada cadena | `fn_0067B380`, desde 0x6798DF | la articulación n/2 |
| efecto `Sorted`: cada niebla | `LH3DMist::AddDrawing` 0x7FA87B, desde 0x67A782 | la niebla (por `mists::Submit`) |
| efecto `Queued`, entero | `PSysManager::AddDrawing` 0x679834 | `GetOrigin` del efecto |
| `components::Sprite` | `LH3DSprite::AddDrawing` 0x840CB3 | el sprite |
| nieblas del mapa y de `mists::Submit` | `LH3DMist::AddDrawing` 0x7FA87B | la niebla |
| **nubes** | 0x7FA87B, desde `fn_005E25C0` 0x5E2813 | la nube |
| humo de chimeneas | `LH3DSmoke::AddDrawing` 0x7F8D8E | la chimenea |
| **lluvia, una entrada por casilla** | `fn_008341B0` 0x83427F | (x, `GetAltitude`, z) del centro del bloque (+0x90C/+0x910 + 80, 0x8362DB); aquí `tile.origin` con la altura de `LandHeightAt` **(aproximado)** |
| **sprites del barco, uno a uno** | 0x840CB3 (estela `PetitNavire::PostDraw` 0x5E08D0, bocanadas fn_00823F70 0x82411A) | el sprite |

Las nubes entran con el cielo (como en `GLandscape::Draw`); lo demás, con los modelos. Sin entidades (vista de
depuración) la cola tiene solo nubes y nieblas y se vacía al final de la pasada.
Los `components::Sprite` (polvo, partículas de coger, destellos, luciérnagas...) van con los modelos transparentes
desde antes de U6; hasta entonces se dibujaban antes que todos ellos y un modelo transparente detrás los tapaba.

**Qué entra y qué no: los modelos** (D7). `LH3DObject::AddDrawing` 0x815A70 (vt+0x100 de los objetos estáticos,
animados y morfables) manda un objeto a la cola **solo** por el bit 0x10 de su +4 (vt+0x44 = `fn_007F97C0`, leído en
0x815AC2 y probado en 0x815F0B): con él, el objeto **entero** (NewZObject 0x815F53, retrollamada 0x7FA980 → Draw
vt+0x108); sin él, el Draw **al momento** (0x815F62), primitivas mezcladas incluidas. El bit lo copia `SetMesh` 0x7F9E10
de la marca 0x200 de la malla (vt+0x3C = `fn_007F9D40`, mesh+4 & 0x200, 0x7F9E48; vt+0x40 = `fn_007F97A0`,
0x7F9E51..0x7F9E64). mesh+4 son las banderas de la cabecera del L3D (`GetChimneyPos` 0x7F9F17 prueba ahí 0x400,
HasChimney): `L3DMeshFlags::Unknown10`, `L3DMesh::IsZSorted`. La bola de un uso fuerza el bit (vt+0x40(1) 0x72A4AA,
tras su `SetMesh` 0x72A49D). `SetGlobalAlpha` (bit 0x80, vt+0x48 = `fn_007F9D60`) **no** cuenta en 0x815A70: solo
`fn_00813340` (vtable 0x9A3068, la de `Particle3DAnim`) encola por él (vt+0x4C 0x8133B9). Por eso un objeto que se
funde con una malla sin 0x200 se dibuja al momento con la tabla 0xC387C8 (`render_modes::Table::GlobalAlpha`), en la
vista principal. Afecta al escudo físico (`PhysicalShield::DrawShield`: `SetGlobalAlpha(1)` 0x72D0CC, `AddForDrawing`
0x72D0E2): su malla `SpellSolidShield` tiene la marca en `Data\AllMeshes.g3d` (el del juego, leído entero: 626 L3D), así
que sigue en la cola. Con la marca, en ese fichero, 40 mallas: casi todas las de los milagros (`SpellBlast*`, `SpellPhile*`,
`SpellRainCone`, `SpellSolidShield`, `SpellSpellBallSurface02`, `SpellSpellDispenser`, `SpellPulseIn/Out`...), las
cuevas, el foso de almacén azteca, `I_SpellGem`, `ObjectBoxFrame`, `RewardChestExplode`, el buitre y
`TreeWheatInField`. openblack (`Renderer::DrawPass`): `instancedDrawDescs` y `translucentDrawDescs` por la marca de la
malla (y la burbuja por su `sortPoint`); los demás modelos, al momento, con todas sus primitivas.
**(inferido)** que todo modelo pase por 0x815A70 (`Game3DObject::AddForDrawing` 0x63B5D0 → vt+0x100) y que ninguno sea
de la clase 0x9A3068.

**Los tres caminos de un efecto PSys** (`psys::DrawPath`; [particles.md](particles.md), veredicto
`dev\documentacion\miracles\polish\psys_draw_paths_verdict.md`, interfaz `drawpath_fix.md`). `fn_00679860` copia el +0xAE
del gestor en [0xC0215D] (0x679884) y cada átomo lo mira:

- **`Sorted`** (`Draw_(t, 1)` 0x55EDA0 → `fn_00679840`, +0xAE = 1 en 0x67984E; `Spell::Draw` 0x720441, tormenta
  0x72DCB7, escudo 0x72D160, teletransporte 0x5FCDC5, dispensador 0x722A13, bandada 0x72420D, utilidades de la mano,
  creencia del pueblo 0x69BF19): el efecto **no tiene Z-objeto**. `manager::CollectSorted` da cada elemento con su
  punto y `DrawPass` hace un `Submit` por elemento, (x² + y²) + z²: cada sprite (`LH3DSprite::AddDrawing` 0x840C70
  desde 0x67B0D2, en la posición del LH3DSprite, subida por altura·tamaño·0,5 con `CentreAtBase`, 0x67AFAB..0x67AFD6),
  cada átomo de malla, **opacos y cortados también** (`fn_00679F60` desde 0x67A246, en su traslación, tras
  `CheckRegionOnScreen` 0x679F75; aquí la esfera de la caja **(aproximado)**), cada cadena (`fn_0067B380`, en la
  articulación n/2: base +0x44, paso 0x1C, índice (n − (n >> 31)) >> 1, 0x67B389..0x67B3A1) y cada niebla
  (`fn_007FA7F0` desde 0x67A782, en mist+0x38: llega por `mists::Submit`). Los discos `ZR_SurfRevol` no miran
  [0xC0215D] (0x67CBA0 → `RenderParticleGJMesh::DrawAt` 0x67C150 → `Draw3DWorldTriangle` 0x81C090, 0x67C9F2): se
  dibujan **al momento**, en la vista principal tras los modelos (`Spell::DrawSpells` 0x7203F0 va después de los
  modelos, desde 0x54E023), sin ordenar. Al vaciar, los sprites seguidos de la cola van en una sola llamada si
  comparten material (`DrawPSysSprites`: mismo orden y mismos estados, los mismos píxeles).
- **`Queued`** (`AddDrawing` 0x55EDC0 → `PSysManager::AddDrawing` 0x6797D0, +0xAE = 0 en 0x6797DE; solo la semilla en
  la bola, el icono, la recompensa o el mapa 0x51A2CA y los contenedores con `GSpotVisualInfo+0x4C == 1` 0x63E26A; y,
  fuera del PSys, el fuego `FireGraphic` 0x73261D): **un** Z-objeto por efecto con clave = `GetOrigin`
  (0x6797E5..0x679834). En el vaciado (retrollamada `fn_00679860`) se dibujan todos sus elementos al momento, en el
  orden de `fn_006798B0` (0x6798B0..0x679912: los átomos de la colección, su cadena, las colecciones hijas;
  `manager::OrderedEffect::items`): sprites con `LH3DSprite::Draw` (0x67B0DF), mallas con `fn_00679F20` (0x67A458;
  vt+0x104, o vt+0x11C cortada con el bit 4 de +0x24), nieblas con vt+0x104 = `fn_007FA790` (0x67A78C: la prueba de
  pantalla y el Draw), discos (0x67CBA0) y cadenas con `fn_0067B370` (0x6798DD).
- **`Immediate`** (`Draw_(t, 0)`): el efecto de la semilla en la mano. `CHand::Draw` 0x46D210 dibuja, dentro del
  Z-objeto de la mano, la malla (0x46D258), el objeto sostenido (0x46D27C) y luego `DrawSpellInHand` (0x46D2AE →
  0x46E680), que hace `Draw_(1.0, 0)` (0x46E76A): `manager::HandEffects`, dibujados como un `Queued` justo detrás de la
  malla de la mano, en la misma entrada. El objeto sostenido de openblack no se dibuja desde esa entrada **(inferido: no
  cambia nada, es opaco y va antes)**.

Por eso la lluvia y el fuego de un milagro ya no se pelean con la burbuja de un dispensador: antes el efecto entero iba
con la clave de su origen, delante o detrás de la burbuja de una vez; ahora cada gota y cada llama van con la suya. El
disco del dispensador (que es del efecto `Sorted` del dispensador, `Draw_(1.0, 1)` 0x722A13) se dibuja al momento, antes
de toda la cola, así que la burbuja (modo 12, aditiva y que **escribe Z**, 0x82ECA6) suma su luz encima, como en el
original. Prueba: `test_psys_sorted_queue` (clave de cada sprite y de la cadena, un `Queued` entre dos sprites de un
mismo `Sorted`). Traza: `OPENBLACK_ORB_TRACE=1` (los discos con su camino y su sitio).

La lluvia, en cambio, **no** pasa por `PSysManager::AddDrawing`: lleva su propia entrada por casilla con su propio
punto (tabla de arriba); darle la clave de un efecto sería falso.

**Lo que cambió al unificar** (arreglos leídos en el binario):

1. Todo es estable: la ordenación propia de las nubes y la de las nieblas de reserva eran `std::sort` (0x83F36A).
2. Tope de 0x800 con pérdida de la nueva (0x83F315/0x83F31C).
3. Las nubes van en la cola, mezcladas por distancia con todo (antes, siempre encima; 0x7FA87B, 0x5E2813).
4. Las cintas de las cadenas, ya dentro del objeto de su efecto (`fn_006798B0` 0x6798DD), pasan por la API.
5. La lluvia va en la cola, una entrada por casilla (antes, un grupo detrás de la lista; 0x83427F).
6. Los sprites del barco, cada uno en la cola (antes, un lote detrás de la lista; 0x840CB3).
7. La mano va siempre a la cola y entera (antes, sus partes opacas en la vista principal y solo las mezcladas en la
   lista, y solo si tenía alguna; 0x46D1B7..0x46D203 no prueba nada).
8. Los efectos PSys por su camino (arriba); los modelos a la cola solo por la marca 0x200 de su malla, enteros (D7).

Además la clave es la distancia al cuadrado en `float` y no la distancia: el mismo orden salvo claves que la raíz
juntaba.

**Lo que no entra, a propósito.** El orden de bloques del paisaje (`LH3DIsland::PreDraw` 0x7FF2D0, opaco, de cerca a
lejos), los anillos del agua (`LH3DSprite::Draw` **inmediato**, 0x5E526C), las manchas de los aldeanos, el brillo de la
mano en el agua (0x5E4D89) y el sol (`fn_0086BB60`, después del vaciado, **(inferido)**). Las sombras proyectadas
tampoco son Z objects (ninguna rutina de sombra está entre los 32 llamadores): las de tierra van con cada bloque
(`fn_007FF610` 0x7FF749) y las de objetos al final del Draw de cada receptor (`fn_0080DB30` 0x80E457..0x80E4D7,
`fn_00812170` 0x81311A..0x81317C), así que van al momento con un objeto dibujado al momento (sin la marca 0x200 de su malla,
0x815F62) y **dentro de su Z object** con uno encolado (0x7FA980 → vt+0x108). openblack lo hace igual (`Renderer::DrawShadowsOnObject` detrás de su `DrawMesh`, en
Main o desde el vaciado; ver [Sombra dinámica sobre objetos](#reflejos-de-objetos-y-sombra-de-la-mano-sobre-objetos)); hasta el punto 5 iban
todas detrás del vaciado y oscurecían las burbujas, nubes y sprites que había delante. El reflejo no tiene cola (en
el original no se ha leído **(inferido)**); se queda como estaba.

**(aproximado)**:
- El orden de llegada (el desempate con claves iguales) no es el del original: aquí nubes, modelos por malla (un
  `std::map`), desvanecidos, sprites, mallas y cadenas de los `Sorted`, efectos `Queued`, sprites, nieblas, humo,
  lluvia, barco; allí el orden de los AddDrawing del fotograma (`original-frame.md`, pasos 4m..22).
- El punto de la lluvia es `tile.origin` con la altura de `LandHeightAt` de Rain.cpp, no `GetAltitude` 0x803090 sobre
  `MapCoords(x × 65536 × 0,1, z × 65536 × 0,1)` (0x8341D2..0x834210): eso es U3.
- `CheckRegionOnScreen` de una malla `Sorted` (0x679F75) es la esfera de su caja; la de un modelo (0x815AB1) no se
  hace: un modelo fuera de la vista ocupa un sitio del tope que el original no gasta.
- Las mallas animadas de un `Sorted` (`Particle3DAnim::DrawAt` 0x67A9D9 → `fn_00813340`: a la cola solo con el bit
  0x10, el 0x80 o +0xB8, si no al momento) van como las demás mallas **(no portado)**.
- La lluvia viaja por su índice y no por K: `CollectTiles` ya aplica el fundido por distancia de fn_00834370, así que
  una K hecha con ese alfa no sería la del original.
- Las primitivas mezcladas de un modelo dibujado al momento van a la vista `MainBlended` (`DrawSubMesh`), por delante
  de toda la cola pero detrás de todo lo opaco de la vista principal; en el original, en el sitio de su Draw.

La traza `OPENBLACK_ORB_TRACE` sigue escribiendo la distancia (la raíz de la clave), como antes. Traza nueva:
`OPENBLACK_ZSORTER_TRACE=1`.

## Manchas de aldeanos, reflejos de objetos y LOD

**Fiel**. Informe: `documentacion\render\misc_*` (con emulación Unicorn de `fn_0081FFF0`).
- **Manchas** (`fn_0081FFF0`, desde el Draw de objetos animados `fn_00812170`): pies = huesos 21 y 18 (fin de las dos
  piernas), en el suelo + 0,2; D = O·s − ((O·s)·n)·n con O = (√2, 0, √2), s = escala, n = normal del terreno;
  quad 1 desde el pie 21 con V = D + (P18 − P21)/2, quad 2 simétrico; esquinas C − 0,02V ± U y C + V ± U con
  U = 0,2·norm(1, 0, −1) (ancho fijo 0,4); UV (0,0)(1,0)(1,1)(0,1), alfa 1 en los pies y 0 en la punta; modo 6, sin
  Z, dos caras, `human_shadow.raw` (byte & 0xF0 como alfa). No si y ≤ 0,2 (en el agua), muerto o en la mano de la
  criatura. Animales: puntos de sus datos EBone (2 o 4 quads). openblack: `Renderer::DrawHumanShadows`;
  `human_shadow.raw` se corta a 4 bits al cargar (0x81FCDD, [rendering.md](rendering.md#texturas-argb4444)) y
  `fs_blob` solo lo muestrea.
- **Reflejos en el mar** (`GLandscape::Draw` 0x5E490F): `DrawUnderWater` dibuja el objeto espejado en y = 0, sin luz,
  recortado para que solo se refleje lo que está sobre el agua: la mano (0x65A0A0A0) y lo que sostiene, **el cuerpo de
  la criatura** (0x65A0A0D0 + especular 0x30; no lo que lleva), barcos (0xFF303070), objetos físicos (su color).
  Tiburones y SuperVillagers nadando se dibujan **cortados bajo el agua** (`DrawCutByPlane`, 0xFF303070); los peces
  de piscifactoría son sprites. Detalle en las dos secciones siguientes y en
  [Bancos de peces](#bancos-de-peces-de-las-piscifactorías).
- **LOD**: `g_last_distance` es la profundidad lineal del centro de la esfera; S = min((importancia + 1) · radio ·
  LevelOfDetail, 100000). En este ejecutable las dos cargas de LevelOfDetail están anuladas con NOP, S = 100000:
  **siempre LOD 1**, sin fundido ni impostores (con el valor de diseño 0,5, un aldeano cambiaría a 11,5 / 33 / 43 u).
- Trampa de las pruebas: `OPENBLACK_MOUSE_AT` en un borde de la ventana activa el desplazamiento por borde y mueve la
  cámara; usar puntos interiores.

## Submallas de física y de LOD 0

**Fiel** (regla del cargador de L3D: el bit `isPhysics` de la cabecera de submalla y `lodMask`). Una submalla con
`isPhysics` (o sin el bit 1 de `lodMask`, salvo las ventanas) es solo para las físicas (`BuildFromVertices` 0x7FBAE0
la usa como casco) y **nunca se dibuja**. Ejemplos: el dispensador de milagros (malla 557 `SpellSpellCreator`, la de
`ABODE_SPELL_DISPENSER` en info.dat) tiene una submalla 0 de física de 16 vértices, un tronco de prisma de 7 lados
(radio 1,9 abajo, 1,5 arriba, alto 3,1, UV 0, piel 0x4F), y el orbe `O_Bibble_up` una esfera lisa. En openblack
todos los caminos la saltan: `Renderer::DrawSubMesh` (todo lo que pasa por `DrawMesh`: objetos, transparentes
ordenados, átomos de malla del PSys, reflejos, la mano, barcos, tiburones, peces; solo el visor de mallas la pinta
con `drawAll`), la sombra estática (`DrawStaticShadowPass`), las sombras proyectadas (`ShadowList.cpp`), `FragMesh` (trozos
de edificios), `PartialBuild` y el picado (`L3DMesh::RayIntersect`). Comprobado el 2026-10-01 (capturas
`dev\_audit\magic\prism_*.png` con el mod `test.miracle-dispensers`): el prisma no aparece en ningún dispensador.

## La pasada bajo el mar (`graphics::sea_pass`)

**Fiel** salvo lo marcado. Una sola API para todo lo que el original dibuja «debajo» del mar antes de él
(`GLandscape::Draw` 0x5E48AE..0x5E4E8C): `src/Graphics/SeaPass.h` (CPU, `namespace graphics::sea_pass`) y su gemelo
`assets/shaders/sea_plane.sh` (GPU, `u_objectClip`, `SeaPlaneDiscard`, `SeaUnmirror`); prueba `test/test_sea_pass.cpp`.

El original tiene tres mecanismos y un solo plano:

| Mecanismo | Qué hace | Plano | Luz |
|---|---|---|---|
| A, tierra espejada (`fn_007FF4F0`) | unidad de altura [0xC3720C] = 0,67 × [0x8AB678] = −1,0 (0x7FF515..0x7FF52F), [0xFA92DC] = 1 (los bloques invierten sus índices, 0x7FF535) | — | tabla >> 1 (0x7FF53F..0x7FF564), sin small bump ([0xC37210] = 0, 0x7FF566..0x7FF577), sin escribir Z (0x5E48C5..0x5E4900) |
| B, `DrawUnderWater` (vt+0x118: `fn_00811010` estático, `fn_00810E20` animado, `fn_00813300` complejo → `fn_00850FC0`) | espeja el objeto en y = 0 (fsubp 0x851094 / 0x8510BD / 0x8510E5) | prueba el punto **espejado**: d > 0 fuera (0x85111C..0x851149) | color constante obj+0x4C / +0x50 (0x811033..0x81103F → [0xC37D8C] / [0xE9FE2C], leídos en 0x851082 / 0x85102F), sin luz |
| C, `DrawCutByPlane` (vt+0x11C: `fn_0080C050` estático / complejo, `fn_00811C70` animado → `fn_00858BA0`) | **no** espeja (0x858C5D..0x858CAE) | prueba el punto: d < 0 fuera (0x858D49..0x858D80) | 90 + 165·I >> 8 sobre obj+0x4C, + obj+0x50 (ver la sección siguiente) |

- B y C comparten el recortador de CPU `fn_0081D2C0` (sus únicos llamadores: 0x8515E7 / 0x8516AD y 0x85930E /
  0x8593C5) y el plano de usuario de `fn_00822560` (mundo [0xF03128], vista [0xF03118]). El plano por defecto
  (0, 1, 0, 0) lo ponen los inicializadores `fn_0084A380` (0x84A39A: [0xF0312C] = 0x3F800000) y `fn_0084A3C0`; los
  nadadores ponen (0, −1, 0, 0) (0x5E4C4A..0x5E4C5A, dwords, 0xBF800000) y lo devuelven en 0x5E4D76. El tiburón
  (`fn_00774E30` 0x774FF5..0x77501A, devuelto en 0x7750E6..0x775106) y la red (`fn_00829BC0` 0x829C91..0x829CB7,
  devuelto en 0x829D25..0x829D45) ponen **su propio** (0, −1, 0, 0), antes del bucle de los nadadores
  (`sea_pass::k_SharkPlane`, `k_NetPlane`: los mismos valores que `k_SwimPlane`). Con
  (0, b, 0, 0) los dos mecanismos dejan el mismo lado de la y **real**: b > 0 → y ≥ 0, b < 0 → y ≤ 0 (el espejo y la
  prueba contraria se anulan). `sea_pass::Kept(Mechanism, plano)` → `SeaPlane {None, KeepAbove, KeepBelow}`.
- **Cara**: el material decide en los tres sitios (el Draw 0x84C34A, B 0x851798..0x8517D1, C 0x8594CF..0x8594ED:
  `((~mat+5) & 1)·2 + 1`, `push 0x16` = CULLMODE). `SeaPassState::FaceCull(Surface, dosCaras, unmirror)` junta los
  cuatro sitios de openblack: modelos (C1), luna (C2), cielo (C3) y tierra (C4, que en openblack tiene el orden de
  vértices contrario: es un dato de openblack, sin dirección).
- **openblack**: la pasada `RenderPass::Reflection` dibuja con la cámara espejada (`ReflectionXZCamera`), que ya da el
  espejo de B. Lo que C dibuja dentro de esa pasada se **des-espeja** (`SeaDraw::unmirror`, `SeaUnmirror` en
  `vs_object`, en sus dos ramas), y el plano es un descarte por fragmento sobre la y real (`fs_object`,
  **(aproximado)**: estricto por píxel, y = 0 se queda en los dos lados; el original recorta triángulos). Lo mismo con
  los sprites de los peces (`Unmirror` en la CPU) y la vista de la luna (`UnmirrorView`).
- `sea_pass::ForPass(pase)`: `mirrored`, `landLightScale` 0,5, `landWriteZ` y `smallBump` falsos en Reflection
  (sustituye a `DrawSceneDesc::cullBack` y al `mirrored` de `DrawMoon`, que ya no existen).
- `L3DMeshSubmitDesc::sea` (`sea_pass::SeaDraw`: luz, plano, unmirror, argb, especular, color por instancia) sustituye a
  los seis campos de antes (`unlitColour`, `landColourOnly`, `clipBelowSea`, `cutByPlane`, `cutColour`,
  `mirrorInSea`). `u_objectLight.x` sale de `SeaDraw::light`: `Normal` 1, `Constant` 2 (B, z = rgb, w = especular),
  `LastDraw` 3 (B con lo que dejó el último Draw), `Cut` 4 (C, y = alfa, z = rgb o −1 = el de cada instancia,
  w = especular). `u_objectClip` = `PackClip` (x plano, y unmirror); el cielo escribe 0 (bgfx guarda el último valor de
  cada uniforme).
- Entradas de `Renderer` (`RendererCut.cpp`): `DrawUnderWater(vista, malla, …, SeaDraw)` y `DrawUnderWater(vista,
  entidad, SeaDraw)` (la mano con `UnderWater(k_HandColour, k_HandSpecular)`, lo sostenido y los objetos físicos con
  `UnderWaterLastDraw()`, el barco con `UnderWater(0xFF303070, 0)`), `DrawCutByPlane(vista, entidad, SeaPlane, argb,
  especular)` (tiburones: especular 0, `push 0` 0x775027) y `DrawFishPlots(vista, SeaPlane)` (la red: +0x50 = 0 del
  ctor 0x8164FE). El orden es el del binario: tiburones (`fn_00775120` 0x5E4B26), peces y redes (`fn_00824B90`
  0x5E4B2B), el sitio de los nadadores (0x5E4B4C..0x5E4D76) y el brillo de la mano (0x5E4D89). El reflejo del barco
  sigue con `ObjectInstanced`: el casco no tiene `MorphWithTerrain`. Una instancia que se pega al suelo no se dibuja bajo el mar: el `DrawUnderWater` de las vtables
  morfables (vt+0x118 de 0x9A2E34 / 0x9A2BFC) es un `ret` (0x80BA40), como su `DrawCutByPlane` (0x80BA50).
- Colores de la pasada (`SetColorSpecular` vt+0x2C antes de la llamada): mano 0x65A0A0A0 / 0 (0x5E496E / 0x5E496C),
  criatura 0x65A0A0D0 / 0x30 (0x5E4ACF / 0x5E4ACD), nadadores 0xFF303070 / 0 (0x5E4C69 / 0x5E4C68), barco 0xFF303070
  (`mov [eax+0x4C]` 0x5E016C, sin tocar +0x50: **(inferido)** openblack pone 0).
- **El alfa 0x65 de la mano no se ve** (D-O3, **(inferido)**): `fn_00811010` solo pone la tabla 0xC387C8 (0x8110CF)
  si vt+0x4C = `fn_007F9D80` ((+4 >> 7) & 1) devuelve 1 (0x8110BF, `test eax, eax / je 0x81114C` 0x8110C2..0x8110C4).
  El objeto LH3D de la mano es `LH3DObject::Create(3)` (`Morphable::MorphInit` 0x61731A → 0x80B5B2, vtable compleja
  0x9A3068, cuyo vt+0x118 es `fn_00813300`: [0xC37D9C] = obj+0x80 y luego `fn_00811010`), sus banderas empiezan en
  0x10009 (0x816537) y una búsqueda de bytes no encontró ninguna llamada a su vt+0x48 (`fn_007F9D60`, Flags1 | 0x80) en
  los 0x30 bytes tras una lectura de [x+0x482C] (no descarta un setter por otro camino): su `DrawUnderWater` usa la
  tabla normal y su material `AlphaTextured` toma el alfa de la textura.
- Alrededor del `DrawUnderWater` de la mano: vt+0x58(0) 0x5E497E (`fn_008168C0`: quita Flags1 0x20; solo lo pone si
  [0xC38224] ≠ 0) y vt+0x58(edi) 0x5E4991 lo devuelve. **(no portado)**: no se sabe qué hace Flags1 0x20 en este
  dibujo (`fn_00811010` no lo prueba).
- La **luna** sí usa la tabla 0xC387C8: vt+0x48(1) antes de su Draw (0x86AC05) y antes de su `DrawUnderWater`
  (0x86AC3B). Modo 4 → 5 (0x82DD90: mismo blend y Z, ALPHAOP MODULATE(TEXTURE, DIFFUSE)); `fs_celestial` siempre
  modula el alfa con `u_colour`, así que `DrawMoon` ya dibuja como el modo 5 en las dos pasadas.
- **Átomos de malla del PSys con `DrawCutByPlane`** (fidelidad 3): el bit es `+0x24 & 4` del átomo, no `& 0x10`
  (`fn_00679F20` `test al, 4` 0x679F29; lo pone `CreateParticle` desde +0x5F, 0x6A8B94..0x6A8B9A). Con el bit, vt+0xF8
  (0x679F2F, devuelve la malla), luego `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80 (llamada directa en 0x679F3C;
  si da 0 se salta, 0x679F43) y vt+0x11C (0x679F4A) en vez del Draw vt+0x104 (0x679F52); también en el camino
  ordenado (la vuelta de `fn_00679F60` es `fn_00679F20`, 0x679FBC). openblack: `RenderContext::cutAtomInstances` (con
  `psys::manager::k_DrawByPath` todos, opacos o no, en `psysAtoms`: con su Z-objeto en un `Sorted`, en su sitio dentro
  del efecto en un `Queued`), `sea_pass::CutAtoms` (plano por defecto, color y especular de la instancia;
  **(aproximado)** alfa de vértice 0xFF: `fn_00858BA0` lo toma de obj+0x4C & 0xFF000000, 0x858C42 → [ebp−0x24], OR en
  0x858D60, es decir el alfa de DrawData+8, que la instancia no lleva).
  `psys::mesh_atoms::Instance` lleva el bit (`cutByPlane`, desde `MeshCreator::drawCutByPlane`) y el especular
  DrawData+0xC (`specular`), que `RenderingSystem` lee; el alfa de DrawData+8 (`alpha`) no llega al corte. Un átomo
  con `DrawCutByPlane` y `DrawWithLandscapeColor` se apunta una vez en el registro y se dibuja sin cortar
  **(inferido: no se conoce ningún efecto con los dos)**. La prueba de caja en pantalla 0x679F3C la da el recorte de
  bgfx **(aproximado)**.
- **Huecos sin usuario** (constantes y un TODO con dirección en `Renderer.cpp`): la criatura (`DrawUnderWater` tras los
  objetos físicos, 0x5E4A84..0x5E4AE6, si su bloque se ve (+0x920 & 1), el +0x9BC del bloque ≤ [0xC37200] = 100000,
  y < [0x8AB35C] = 6 y +0xA0 < [0x8AB244] = 0,2) y los SuperVillagers que nadan (lista [0xEB9A08], animación
  «M_P_Swim2» 0xBF3598, plano de los nadadores, vt+0x11C 0x5E4C77, tras `fn_00824B90` (0x5E4B2B: peces y redes) y
  antes del brillo de la mano 0x5E4D89).

## Reflejos de objetos y sombra de la mano sobre objetos

**Fiel** (hechos). Informes: `documentacion\render\objshadow_notes.txt`, `cut_notes.txt`. El código común está en
[La pasada bajo el mar](#la-pasada-bajo-el-mar-graphicssea_pass).
- **DrawUnderWater** (estático 0x811010 → `fn_00850FC0` por primitiva; animado 0x810E20; complejo 0x813300): mundo =
  objeto × vértice, clip = W2C·(x, −y, z), orden de índices invertido, plano (0, 1, 0, 0) que quita lo que tenía y < 0.
  Difuso = obj+0x4C y especular = obj+0x50, **sin luz**. La tabla de modos alternativa 0xC387C8 solo con Flags1 & 0x80
  (la mano no la usa: el alfa 0x65 no tiene efecto con su material modo 4).
  - Mano: omitida si hand+0xAC; 0x65A0A0A0; después lo que sostiene (hand+0x8C) **con su propio color**.
  - Criatura: su cuerpo LH3D si su bloque se ve, y < 6 y obj+0xA0 < 0,2 (campo sin identificar); 0x65A0A0D0, especular 0x30.
  - Objetos físicos (`fn_00646FE0`, array 0xD47814, paso 0x1DC): si y > −r (r = distancia máxima de un vértice al
    centro de masas), sin límite de distancia.
  - El "color propio" es lo que `fn_00801C90` dejó en obj+0x4C/+0x50 en su último Draw (lo llaman `PhysicsObject::DrawAll`
    0x646F9F, `MobileObject::Draw`, `Rock::Draw`...): la luz de tierra bilineal y el especular de las celdas, sin N·L ni neblina.
  - Barcos (`PetitNavire::PreDraw` 0x5DFF20): **un** `DrawUnderWater` por fotograma del casco en 0xFF303070 (luego
    `fn_00801C90` le devuelve la luz de tierra). Las ramas 0x5E0100-0x5E0190 (modo 0, botadura: además corrige la y con
    `GetAltitude` y la sombra) y 0x5E0380-0x5E03EE (modo 1, travesía) se excluyen por +0x30, las dos con el espejo
    diag(−1, 1, 1) sobre la pista del casco (determinante −1) y el `RotateY(π/2)`: no hay segunda parte. openblack:
    `Renderer::DrawBoatReflection` (modo 2 de `vs_object` con el rgb empaquetado). Ver [water.md](water.md#barco-de-los-misioneros-petitnavire).
  - openblack: `Renderer::DrawObjectReflections` en la pasada de reflejo (lo que sostiene la mano y **toda** la lista
    física 0xD47814 por `PhysicsObjects::ForEach`: lanzados, golpeados y los proxies en reposo, con y del centro > −r,
    r = `PhysOb::Radius`; los lanzados de la mano que no estén en física, con el radio de la caja), cada uno por
    `Renderer::DrawUnderWater(vista, entidad, sea_pass::UnderWaterLastDraw())` (modo 3 de `u_objectLight` en
    `vs_object`, plano KeepAbove).
- **Sombra dinámica sobre objetos**: al final de cada Draw (estático `fn_0080DB30` 0x80E457..0x80E4D7, animado
  `fn_00812170` 0x81311A..0x81317C, vt+0x15C `fn_00810720` 0x810CD6 y `fn_00817930` 0x8185AB, morfable 0x80E74B...),
  si el objeto tiene Flags1 0x40 (vt+0x7C), para cada `ShadowInfo` de la lista (de la más nueva a la más vieja) con
  `fn_00881030`, si+0xC = 0 (la mano, la criatura y el barco, 0x5E11BE; objetos físicos y SuperVillagers ponen 1: solo
  tierra), que no sea el emisor (si+0x464 ≠ obj) ni la si propia del objeto complejo (vt+0x1A8 / vt+0x1B8), y cuya caja
  si+0x2C {x0, z0, x1, z1} toque la caja XZ de la malla (centro ± mitad + posición, sin giro ni escala; vt+0x1BC =
  `fn_007F9E80`): `fn_0080B050` (modo 6 por la tabla actual, que ya vuelve a ser la normal también en un objeto que se
  funde, 0x80E197; color blanco; `fn_0084E200` con u = (Wx − x0)/(x1 − x0), v = (Wz − z0)/(z1 − z0), `fn_00880770`:
  **proyección vertical**; todo el oscurecimiento va en el alfa de la textura, con el fundido horneado).
  - Prueba de Z: el estático y `fn_00810720` ponen ZFUNC EQUAL antes de cada sombra (0x80E484 / 0x810C8F) y LESSEQUAL
    al acabar (0x80E4CE / 0x810CF2); el animado no toca ZFUNC: queda el LESSEQUAL del fotograma.
  - **Dónde**: dentro del Draw del receptor, así que al momento con un objeto dibujado al momento (sin la marca 0x200,
    0x815F62) y en su hueco de la cola con uno encolado (0x7FA980 → vt+0x108); nunca detrás del vaciado ([la cola](#la-cola-única-de-transparentes-lh3dzsorter)).
  - Reciben (Flags1 0x40, `Object::Create3DObject` 0x6365F0 si ShadowsOnObjects): todos los objetos salvo los que llaman
    vt+0x78(0) (`xor edx, edx; call [eax+0x78]`): árboles (0x749FA3), bosques (0x439098), flores (0x527A5D), comida
    mágica (0x5FAAC8), la comida en la mano (pot 12, 0x66D180), los credos (0x50B46E), las banderas del pueblo
    (`TownDesireFlags`, 0x746DD4), las bolas de un uso (`OneOffSpellSeed`, 0x72A4B4), los escudos (MagicShield
    0x72C2B4, PhysicalShield 0x72CCF4), la carga de iconos y tótems (`TChargingData` 0x72675F, 0x780BBB) y los cultivos al borrarse (0x607EC5). Al coger
    un objeto se le quita (`SetHeldObject` 0x816842); al lanzarlo se restaura.
  - Y no reciben los que no pasan por `Create3DObject`: un LH3DObject nuevo tiene el bit a 0 (el ctor de
    `LH3DMeshedObject` pone +4 = 0x10009, 0x816537) y vt+0x78 (`fn_008168A0`) solo lo pone con argumento ≠ 0 y
    [0xC38220] ≠ 0. Así, las bandas de power-up de la mano (`Band`, `fn_0068CA30` → `LH3DObject::Create` 0x68CA98) y la
    del icono de hechizo (`CreatePUBand` 0x727080 → `Game3DObject::Create` 0x63ABB0, que salta a `LH3DObject::Create`)
    no reciben. La malla del icono sí (`fn_00727190`, vt+0x78(1) en 0x727245).
  - Corrección: las mallas PSys no llaman vt+0x78(0), sino vt+0x78 con el byte +0x54 del creador (`mov dl, [edi+0x54]`
    en 0x6A8ACE / 0x6A8D65; el mismo byte va a vt+0x80). Ese byte vale 0 en los dos ctores (0x6A8986, 0x6A8BDE) y
    ninguna propiedad lo escribe (`DefineProperties` 0x6B37A0 / 0x6B38B0 / 0x6B3970; no hay otra escritura en
    0x6A8000..0x6B4000), así que tampoco reciben. `ParticleAnimCreator::CreateLH3DObject` (0x6A9760) no llama vt+0x78.
  - openblack: `RendererShadows.cpp`. `CollectShadowReceivers` (al empezar los objetos de la vista Main: receptores de
    `RenderContext::entityInstances` con `receivesDynamicShadow`, sombras con `onObjects`), `DrawShadowsOnObject`
    (detrás del `DrawMesh` de cada malla opaca en Main, o detrás de su entrada en el vaciado de `graphics::zsorter`, en
    `MainBlended`; con las matrices con que se dibujó) y `DrawShadowsOnCutObjects` (las partes de los tiburones sobre el agua, justo detrás de
    `DrawCutAboveWater` y no una a una **(inferido: son opacas y la prueba Z del receptor ya descarta lo que se dibuja
    delante)**). Un receptor que no se dibujó en el fotograma (fuera de la vista, ya transparente del todo, o pasado el
    tope 0x800 de la cola) no recibe sombra: `ClearShadowReceivers` vacía la lista al final de los objetos, como la
    cola de un Draw que no se ejecutó (0x80E457..0x80E4D7). ZFUNC Equal, salvo las mallas con huesos y las
    morfables (`ZFunc::LessEqualInclusive`: GEQUAL = LESSEQUAL con la Z invertida; **(inferido)** que una malla con huesos es de la clase animada;
    el Draw morfable `fn_0080E550` no toca ZFUNC alrededor de su bucle 0x80E768..0x80E874).
    `fs_object_shadow` con `shadow.sh`. `ReceivesDynamicShadow` (RenderingSystem.cpp) deja fuera también las bolas de
    un uso, los escudos y las bandas de power-up (`HandFxPart` y la malla `Power_Up_Band`). Clave de detalle `shadowsOnObjects` (niveles 3–6).
  - **Receptores morfables** (hecho, sesión «shaders», 2026-10-02): el Draw morfable (`fn_0080E550`, vt+0x108 de
    0x9A2E34; en openblack `MorphWithTerrain`) no usa `ContainsThisBoundingBox` sino su propia prueba de círculo
    (0x80E78E..0x80E857), y dibuja con `fn_0080AE40` (la misma tabla de modos y el mismo CULLMODE que `fn_0080B050`,
    con los vértices fundidos de [0xF05180]): R = (obj+0x44 · malla+0x30) + máx(x1 − x0, z1 − z0) · 1,4142
    ([0x932D08] `8104b53f` = 1,41419995, **no** es el float más cercano a √2), el centro de la malla +0x18..0x20 por la
    matriz del objeto obj+0x14 y la distancia en x, z al centro de la caja ((x0 + x1) · 0,5 [0x8AA3B4]); se dibuja si
    dx² + dz² < R², estricto (0x80E84E). No mira vt+0x1A8 / vt+0x1B8, solo si+0x464 (0x80E782).
    `shadow_math::ReachesMorphable`, probado en `test_shadow_math`. **(inferido)** que `MorphWithTerrain` sea la
    clase morfable (vtable 0x9A2E34, Get3DType 1): la clase CITADEL (Get3DType 8, `CitadelHeart` 0x464B40; vtable
    0x9A2BFC) dibuja con `fn_00882A40`, que llama al Draw estático `fn_0080DB30` (0x882AB5), con
    `ContainsThisBoundingBox` y ZFUNC EQUAL; hoy ninguna entidad de tipo 8 lleva el componente (`CitadelArchetype` no lo
    pone; `CitadelPart` es tipo 1, 0x4694B0). La prueba lee obj+0x14 de la matriz que openblack dibuja; vale mientras
    ninguna entidad `MorphWithTerrain` reciba los retoques de `RenderingSystem` (vaivén de campos y árboles, la
    inclinación y el encogimiento de los árboles), que son la matriz de dibujo del original y no obj+0x14 (inferido).

## Cortar por el plano del agua (`DrawCutByPlane`)

**Fiel** (hecho, W11).

- **DrawCutByPlane** (vt+0x11C: animado `fn_00811C70`; estático `LH3DStaticObject` vt 0x9A2974 = `fn_0080C050`, **no**
  es un `ret`: el `ret` `fn_00815F90` es solo la vtable base 0x9A2748): plano de `fn_00822560`, (0, −1, 0, 0) → queda
  lo de **y ≤ 0**, (0, 1, 0, 0) → y ≥ 0; recorte por CPU por triángulo (`fn_0081D2C0`), luz por vértice `fn_00858BA0`:
  I = 255·(L·n) con la luz 0xF03140 (la de `fn_0084BA90`), I < 0 → 90, si no 90 + (255 − 90)·I >> 8
  (`[0xC39264]` = 90); rgb = color.rgb·I >> 8, A = color.A, el especular del objeto; el modo del material. Lo usan los
  SuperVillagers con `M_P_Swim2`, los tiburones (`MSH_SHARK_BONED`: la parte de abajo antes del mar en 0xFF303070 y la
  de arriba en su Draw con tabla[255]) y la red del puzle de peces (Land 4, estático). **No aplica en Land1**.
  openblack (W11, con [sea_pass](#la-pasada-bajo-el-mar-graphicssea_pass)): `L3DMeshSubmitDesc::sea =
  sea_pass::Cut(plano, argb, especular, pase)` → modo 4 de `u_objectLight` en `vs_object` (la misma cuenta entera;
  I se guarda con `fistp` en 0x858CDF: redondeo al más cercano, mitades a par, no truncado; + el especular en w) y
  descarte por fragmento en `fs_object` (`SeaPlaneDiscard`) en vez del recorte por CPU; dentro de la pasada Reflection
  la malla se des-espeja en y = 0 (`unmirror`; el culling vuelve a CCW). `Renderer::DrawCutByPlane(vista, entidad,
  SeaPlane, argb, especular)` y `DrawCutBelowWater` (en la pasada de reflejo, antes de los peces: las entidades con
  `components::CutByPlane`, KeepBelow con el plano propio del tiburón `k_SharkPlane`, 0x774FF5..0x77501A). La parte de arriba la llama el dueño del objeto en lugar de
  su dibujo normal: `CutByPlane::drawAbove` (los tiburones) hace que la pasada normal salte esa instancia y
  `Renderer::DrawCutAboveWater` la dibuje con KeepAbove y `LandLightTable::GetRaw(255)`, en la pasada principal tras
  las mallas instanciadas. `DrawCutByPlane` usa la pose de `SkeletalAnimation` si la hay.
  Gancho: `OPENBLACK_TEST_CUT=1` con `OPENBLACK_TEST_SEA`.

## Bancos de peces de las piscifactorías

**Fiel** (hechos), salvo lo que se dice que falta. El puzle de los peces y lo demás del agua están en
[water.md](water.md); la creación de las granjas desde el guion, en
[map-loading.md](map-loading.md#piscifactorías-create_fish_farm--create_town_fish_farm).

- **Piscifactorías** (`FishFarm::CallVirtualFunctionsForCreation` 0x52CC10): anillos de radio 2, 4... < 50 × 32
  direcciones, con el aplanado del mar **desactivado** (`[0xC37BF4]` = 0); la primera dirección con altitud 0 en dos
  radios seguidos da el centro (x', y de la granja, z'). 15 peces (`fn_00824740`): sprite `misc0.raw` horizontal (flag
  0x40: quad girado en Y con su x local según el rumbo), media anchura 0,8–1,2, posición centro + (±5, −1..0, ±5), rumbo
  ±π, velocidad 0,5–1,5, giro velocidad·(1 ± 0,1)·0,6283; celdas 8–23 (fotograma += dt·velocidad·25; la celda se toma antes de la vuelta −15·ftol(f/15), así que sale la 23;
  `frame_anim::FishFrame`).
  Movimiento `fn_008248E0` (dt ≤ 0,1 s), objetivo del banco `fn_00824DA0` (centro ± 7, temporizador 0,5·distancia);
  a más de 300 no se dibuja, alfa desde 200 (con el desbordamiento de byte del original). Dibujo modo 6 antes del mar.
  - openblack: `FishFarmArchetype`, `ecs::UpdateFishShoals` (tiempo de juego del fotograma), `Renderer::DrawFishShoals`.
    Como `fs_water` compone el mar opaco con la textura de reflejo, "lo que hay detrás del mar" es la pasada de reflejo:
    los peces se dibujan ahí **espejados** (y → −y) y sin prueba de Z, sobre la tierra reflejada (que en el original no
    escribe Z). Lo mismo valdría para los cortes bajo el agua.
  - **Susto** (informe `documentacion\fish\fish_notes.txt`): el punto de chapoteo global 0xEA9F40 / bandera 0xEB99F0 lo ponen
    el **inicio del agarre del terreno sobre el agua** (`StartLandscapeGrip` fn_005D1AB0, botón de agarre: anillo de
    crecimiento 7 y los sonidos `G_HANDINWATER_01..10` por turno), las pisadas de la criatura con el pie bajo y 1
    (fn_00483290) y un objeto físico que cae al agua (fn_0074F2D0). Cada banco a menos de 300 de la cámara: objetivo =
    centro + 2·(cos r, 0, −sin r), temporizador 2 s; los peces visibles a menos de 8 (distancia 3D) huyen 2 s con rumbo
    atan2(fz − sz, fx − sx). La bandera se borra al final del fotograma.
  - **Pesca**: con el botón de acción sobre el agua sin otro objeto debajo, un pez visible a < 2 (x, z; fn_00824B10) hace
    de la granja el objeto de la acción; `NetworkFriendlyStartLockedSelect` 0x52D770 pone en la mano un HandFood de
    amountPickedUpInitially (25) **sin quitarlo de la reserva**, con las partículas `SF_MultiPickUpFoodFish`
    (`S_Spangle_A` celdas 48–63, 40 fps). `ProcessInInteract` 0x52D950 por turno: n = (int)(8 + 62·t²), t = turnos/60,
    ≤ 1400 y ≤ 20000 − lo que hay en la mano; `RemoveFood` 0x52CED0 quita n (o lo que quede) y la mano recibe **n**
    (rareza); 0 → se acaba. No se pueden devolver.
  - **Reserva**: +0x94, 1400 al crearla (GFishFarmInfo 0: foodValue 1400, 16 turnos por unidad, 4 pescadores); `Process`
    0x52D130 suma 1 cada 16 turnos. Peces visibles = (int)(15·reserva/1400): los de índice alto desaparecen primero y
    los ocultos ni se mueven ni se dibujan. El último argumento de `CREATE_TOWN_FISH_FARM` es el índice de GFishFarmInfo.
  - openblack: `ecs::SplashWater` / `ProcessFishFarmsTurn` / `FindFishFarmAt` / `RemoveFishFarmFood` (FishShoals.cpp),
    `HandFish.cpp` (`SplashHand`, `TryPickUpFish`, `UpdateFishPickUp`), salpicadura al aterrizar los objetos lanzados en
    `UpdateThrown`. Faltan el tono de los sonidos, el texto de ayuda ("Pick up") y los pescadores.

- **Puzle de los peces** (Land 4, la red `FishPlot` y sus bancos con cebo): en [water.md](water.md#puzle-de-los-peces).

## Sombras de los objetos físicos

**Fiel** (hechas). Informe: `documentacion\render\physshadow\`.
- `fn_00646FE0` (desde `GLandscape::Draw` 0x5E49DC) → `fn_007FCE80` por objeto físico que no esté en reposo (byte
  elem+0x19C = PhysOb+0x174) ni con y ≤ −r: si no tiene sombra y proyecta sombra estática (Flags1 0x1000 / 0x2000) o
  está animado, `fn_008745A0` crea un `ShadowInfo` solo de tierra (si+0xC = 1). No la tienen las vasijas, la comida
  mágica, flores, cultivos, DeadTree, AnimatedStatic ni los trozos de edificio (`SetShadowOnTexture(0)`). Se libera en
  `PhysOb::DeInitialise`. Sin límite de número ni clave de detalle.
- Luz: la posición del objeto + (0, 15000, 0) (0x9A3C10), no el sol: proyección prácticamente vertical. Caja = la
  mínima de los vértices proyectados (sin margen). Silueta 32×32 con 4×2 submuestras por texel (`fn_00806F60`), alfa =
  submuestras cubiertas / 15 (máx. 8/15) sin escribir el anillo exterior (`fn_00880FC0`, tabla 0xFA95C4); los árboles
  por la ruta chroma (textura con alfa y filtro 2×2). Fundido 50–80 radios desde la cámara hasta el suelo bajo el
  objeto (`fn_00874600`, con la prueba de los 9 bloques), **horneado** a saltos de nibble. Sobre la tierra
  (`fn_00878350`): t' con H = GetAltitude del emisor (una por sombra), nada en celdas de altitud ≤ 1; modo 6 negro.
  Todo el detalle, en [rendering.md](rendering.md#sombras-proyectadas-shadowinfo).
- openblack: una entrada más de `graphics::shadow_list` (`PhysicsObjects::ForEach` + `CastsPhysicsShadow` de
  `ShadowList.cpp`), rasterizada en la CPU con la pose de cada vértice, sin tope; se dibuja sobre cada bloque que toca
  (`Renderer::DrawLandShadows`). `Graphics/PhysicsShadows` y su bucle de 16 en `fs_terrain` ya no existen (punto 5).
- **Sombra estática de lo que no está en el mapa**: el horneado (`fn_008721A0`) toma los emisores de las celdas del mapa
  (`0x5E2A90` / `0x5E2C30`); coger un objeto (`fn_005DC330`) o darle físicas (`Object::InitialisePhysics*`) lo saca de
  ellas hasta que aterriza (`EndPhysics` → `InsertMapObject`). openblack: `CastsStaticShadow` excluye el objeto en la
  mano y los que vuelan. Rareza no reproducida: solo los Fixed/MultiMapFixed vuelven a hornear el bloque al salir; la
  sombra vieja de un árbol o un MobileObject se queda en el suelo hasta que otro cambio rehornea ese bloque.
- Prueba: `OPENBLACK_TEST_PHYSICS="1490,2140,760,0,0,0,0.6,3"` con la cámara `1450,60,2095,1492,6,2140` y `-n 4000`
  (las rocas caen a ~70 u/s; en la captura están a ~110 de altura y sus tres sombras se ven en el suelo).

## Sombra dinámica de la mano

**Fiel** (S5, hecho; fuente: capturas del original del usuario, 2026-10-02; ver
[rendering.md](rendering.md#sombras-proyectadas-shadowinfo)).

- Original: `CHand::CHand` 0x46BC0B → `CreateDynamicShadow` 0x80C020 (si [0xC3820C] ≠ 0, 1 en los datos), una
  `ShadowInfo` compleja (`fn_00814FD0`) con si+0x3C = 1 (0x80C037: el relleno se salta las subfilas pares, 0x880141,
  como mucho **4/15**), la luz 200 sobre la mano (0x8151C4, [0x8C7B34]), la base en la y de la mano (0x8152B1), t' = 1
  sobre la tierra (no hay si+0x464) y el objeto sostenido (si+0, `SetHeldObject` vt+0x234 = `fn_00816830`, solo si
  `IsG3DObjectDrawnInHand`) dentro de la misma textura a densidad completa (0x807532..0x8075B7). Cae sobre la tierra
  y sobre los objetos (si+0xC = 0).
- openblack: la entrada de la mano de `graphics::shadow_list`, con `k_HandShadowAsOriginal = true` (`ShadowList.h`):
  32×32, 4/15 como mucho, la base en la y de la mano y el objeto sostenido (un orbe cogido del dispensador, por
  ejemplo) a densidad completa en la misma textura, dibujada como las demás (sobre cada bloque y sobre los objetos en
  su sitio de la cola). Comparada con cuatro capturas del original
  ([img/original_hand_shadow_over_dispenser.png](img/original_hand_shadow_over_dispenser.png),
  [img/original_hand_shadow_orb_over_dispenser.png](img/original_hand_shadow_orb_over_dispenser.png),
  [img/original_hand_shadow_red_orb_over_dispenser.png](img/original_hand_shadow_red_orb_over_dispenser.png),
  [img/original_hand_shadow_orb_over_ground.png](img/original_hand_shadow_orb_over_ground.png)): silueta clara con los
  dedos, sombra oscura y redonda del orbe sostenido, orbes enteros encima. Con `false` vuelve el aspecto de antes de
  la lista (64×64, 8/15, sobre el suelo bajo la mano, sin el sostenido; la vieja `DrawHandShadowPass`), solo para
  comparar.

## Animales: manchas y malla

Movido a [animals.md](animals.md#manchas-y-malla-de-los-animales): las manchas de EBone de los animales
(`fn_0081FFF0`) y la malla / escala de creación.

## Humo de las chimeneas (LH3DSmoke)

**Fiel** (hecho; desviaciones al final). Investigación completa: `dev\documentacion\aldeanos\smoke.md` (scripts en `documentacion\aldeanos\smoke\`). Todo leído en el
desensamblado de W1.20; los puntos dudosos (fn_007F8E00, 0x7F9F10, 0x5E4310, fn_005DBC60) se volvieron a leer al portarlo.

- **Creación** (`Abode::CallVirtualFunctionsForCreation` 0x403200): si la malla tiene el flag 0x400 (en openblack
  `L3DMeshFlags::HasChimney`, antes `Unknown11`), `LH3DSmoke::Create` 0x7F8B60 en la chimenea = **punto extra [1]** de
  la malla (el [0] es la puerta) por la matriz 3×3 del objeto (giro y escala) + su posición (con la altitud de los
  cimientos) (`LH3DStaticObject::GetChimneyPos` 0x7F9F10). Color 0x808080 si es un taller, 0xFFFFFF si no. Casas,
  guarderías, talleres, almacenes y maravillas; `MSH_B_AMCN_5` tiene el punto en (0,0,0) (humo desde el pie, como el
  original). ARK, ARK_DRY_DOCK y APPLE_BARREL tienen el flag pero no son Abode.
- **Cuándo** (`Abode::Draw` 0x516288): solo si el edificio salió en pantalla este fotograma; fuera de pantalla el humo
  se congela. `PresentAtHome` (+0xB6, aldeanos dentro) ≠ 0, o taller con la cuenta de andamio (+0xC4) ≠ 0 → estado 0;
  si no, 0 → 2 (se apaga: cada bocanada acaba su vida y renace oculta) y, cuando no se dibuja ninguna, 3 (muerto). Sin
  condición de día/noche, clima ni fuego.
- **Partículas** (`fn_007F8E00`, el callback del Z-sorter, que simula mientras dibuja): 10 sprites, edad inicial i·90
  (en 1/255 s), ocultos; ángulo Random(0, π), sentido de giro al azar, giro ±dt·0,765. Deriva W del fotograma (igual
  para las 10): la **mano** (a < 15 u de la chimenea y velocidad > 1: `handWind`) o Random(−3, 3) en x y z. Al pasar de
  900 renace en la chimenea con τ = edad/255; si no τ = dt. v' = v + 1,5·τ·W; p += (v + v')·τ/2; sube 2,55 u/s. Celda
  (edad/20) & 15 (8 por fila de `smoke.raw`), media anchura edad/450 + 0,5, alfa 79 hasta 225 y luego lineal a 0 en 900
  (enteros). Material `g_smoke_mat` (modo 6: SRCALPHA/INVSRCALPHA, prueba de Z sin escritura, sin luz ni neblina):
  **de noche el humo sigue blanco**, como en el original. Vida 3,53 s.
- **Viento de la mano** (`GLandscape::Draw` 0x5E4310): si el objeto 3D de la mano tiene malla, handPos = su posición,
  handSpeed = clamp(|v|·0,1, 0,5, 5), handWind = v/|v|·handSpeed (v = 0 → 0), con v = `GInterfaceStatus::HandVelocity`
  (`fn_005DBC60`: v += 0,6·(Δpos·1000/100 − v), por turno (inferido)). Valores iniciales handPos (−10000, 0, 0),
  handWind (1, 0, 0). No lee el clima ni LH3DAtmos.
- **openblack**: `ecs::components::ChimneySmoke` (src/ECS/Components/ChimneySmoke.h, en el Abode desde
  `AbodeArchetype::Create`), `ecs::chimney_smoke` (src/ECS/ChimneySmoke.{h,cpp}: Create, Attach, UpdateHandWind,
  UpdateState, Advance), dibujo en `Renderer::CollectChimneySmoke` / `DrawChimneySmoke` (src/Graphics/RendererSmoke.cpp):
  un objeto por chimenea en la lista de atrás adelante de la pasada principal (`ZObject::smoke`, clave la
  distancia al cuadrado a la chimenea), sus bocanadas en orden 0..9 con el shader Sprite (`smokea.raw`, tinte premultiplicado,
  ONE/INVSRCALPHA = modo 6, giro −ángulo en el plano de la pantalla). `Abode::presentAtHome` existe pero **nada lo sube
  aún** (los aldeanos no vuelven a casa), así que sin el gancho no sale humo.
- **Desviaciones**: el paso de edad conserva la fracción (el original trunca `(int)(dt·255)` cada fotograma: vida
  3,75 s a 60 fps, 6,3 s a 144 fps y humo parado por encima de 255 fps, y openblack va sin vsync), como las nieblas;
  "en pantalla" es la esfera de la caja del edificio contra el frustum (aproximado); la mano "con malla" = la mano no
  está en el origen (aproximado); la deriva al azar es por fotograma como el original, así que su amplitud depende de
  los fps (como en el original). Falta la cuenta de andamio de los talleres (sin talleres que fabriquen).
- **Gancho**: `OPENBLACK_TEST_CHIMNEY=all` (todas las chimeneas echan humo). Capturas de Land1 de día y de noche en
  `dev\_scratch\mapa\` (`day_near`, `day_close`, `day_far`, `night_far`; `day_far_nohook` sin gancho: sin humo).

## Objetos que miran a la cámara (billboards)

**Fiel**, salvo lo marcado. Todo lo que se orienta hacia la cámara pasa por una sola API, `graphics::billboard`
(`src/3D/Billboard.{h,cpp}`, solo matemática glm), con una función por cada modo del original.

**Convenios.**
- LH3D usa vector fila (p' = p·M, fn_0084BA90). La fila k es la imagen del eje local k; en glm es la columna k, con la
  misma memoria.
- Los giros de LH3D van al revés que `glm::rotate`: `SetAngleY(a)` 0x674360 (filas (c,0,s) / (0,1,0) / (−s,0,c)) es
  `glm::rotate(−a, Y)`. Los giros en el sitio RotateY 0x5198F0 y fn_0086AFA0 mezclan **filas**: en glm van **a la
  derecha** (`M·R(−a)`). UpdateRuleRotatePrincipalAxis 0x6A1150 mezcla las componentes de cada fila: va **a la
  izquierda** (`R(−a)·M`). Todo está en `lh_matrix` ([engine-math.md](engine-math.md#matrices-lh)).

**Cámara de la pasada.** `CameraFrame::From(camera)` se construye una vez por pasada, con la cámara principal o la
reflejada. Contiene:
- el ojo, g_camera 0xEA1DB8;
- right / up / forward;
- la vista, W2C 0xEA1D28 (`UpdateWorldToCamera` 0x819690, el lookAtLH de openblack), su inversa (SetInverse 0x7FB290)
  y la rotación W2C;
- el plano cercano [0xE839E0], sacado de la proyección. **(aproximado)** `Game.cpp` lo calcula como
  `LandFeature::GetNearClipping` 0x5E2F30, pero solo cambia la proyección si se mueve más de 0,01, así que puede ir
  hasta 0,01 por detrás del original;
- la matriz de las nieblas 0xEA1C98 = mat3(right, −forward, up) (UpdateCamera 0x819A62..0x819AF3 y fn_00819F50).

En la pasada de reflejo, el ojo y right / up / forward siguen siendo los de la cámara principal, porque
`ReflectionXZCamera` solo refleja `GetViewMatrix`. En cambio, la vista, su inversa, la rotación W2C y la matriz de las
nieblas sí salen reflejadas. En esa pasada no hay que mezclar los dos grupos. Hoy solo `DrawMoon` y `drawSprite` montan
un `CameraFrame` en el reflejo, y solo usan la vista y su inversa.

**`Sprite`.** Son los campos útiles del LH3DSprite de 0x34 bytes, con los valores por defecto de SetToZero 0x8404F0:

| Campo | Contenido |
|---|---|
| +0x00 | posición |
| +0x0C | tamaño (media anchura) |
| +0x10 | estiramiento (media altura = tamaño × estiramiento) |
| +0x14 | ángulo |
| +0x18 / +0x1C | origen, en unidades de mundo; **se resta** |
| +0x20 | color |
| +0x28 | celda (bits 0-5) y 0x40 horizontal |
| +0x30 | celdas por fila (8) |

`SpriteQuad` devuelve las 4 esquinas en el orden v0..v3 de 0x840530 y sus UV. `CellUv` hace col·(1/n) +
(8/n)·{0, .125, .125, 0} y fila·(1/n) + (8/n)·{0, 0, .125, .125} (0xC390CC / 0xC390DC, 8 = [0x8C2C70]), con la celda
& 0x3F. Los triángulos son {0,1,2}, {0,2,3}. Los `components::Sprite` (luces de noche, luciérnagas, polvo y los efectos
de la mano) sacan su `uvMin` de `CellUv(celda, 8)[0]`. Su `Transform::rotation` no se usa, porque el modo A solo tiene
el ángulo +0x14.

| Modo (función) | Original | Matemática | Usuarios en openblack |
|---|---|---|---|
| `Screen` (modo A) | `LH3DSprite::Draw` 0x840530, flag 0x40 = 0 | Cuadrado en el plano de la pantalla a la profundidad del sprite: es paralelo a la pantalla y no gira hacia el ojo. x local = {−s − ox, s − ox}, y = {hs − oy, −hs − oy} (0x840831..0x8408CF). La x local va a (cos, −sin) en la pantalla y la y a (sin, cos) (0x84071D..0x84082B), es decir, giro horario. Ángulo 0 = sin giro (0x840770). Orden TL, TR, BR, BL. No dibuja nada si la profundidad es ≤ near (`InFrontOfNear`, 0x84055D..0x840585) | sprites de PSys (`RendererPSys.cpp`: todos los SF, TownBelief, FireGraphic), bocanadas de SmokyStuff (`RendererBoat.cpp`) y, en la GPU, el humo de las chimeneas y los `components::Sprite` |
| `ScreenSpriteModel` | el mismo modo A en `vs_sprite.sc` | T(pos)·Rz(−ángulo)·S(media anchura, media altura, 1). El shader suma u_invView·(modelo·(x, y, 0, 0)) en el plano −1..1, con v = 0 arriba: es `Screen` con origen 0. El corte por near se hace en la CPU | `drawSprite` (luces de noche, luciérnagas, polvo, efectos de la mano, brillos del templo, marcadores de cámara) y `DrawChimneySmoke` |
| `Horizontal` (modo B) | flag 0x40 (0x8405FE..0x840704): `SetHorozontal` 0x6AA093, `GWater::InitialiseCircles` 0x54BA84, fn_00824740 0x8247EF | Ry(ángulo), con filas (c,0,s) / (0,1,0) / (−s,0,c), más la posición. x = {−s − ox, s − ox}, z = {−hs − oy, hs − oy} (0x84085D). No depende de la cámara ni tiene corte por near | estela del barco, peces de las piscifactorías, anillos de agua, la rama horizontal de PSys (SF_ManaPathNew, mapas de luz) |
| `PlaneOfMatrix` | `LH3DSprite::DrawSpecial1` 0x840CC0 | El cuadrado del modo B en el plano XZ de una matriz dada, girado sobre su Y local si el ángulo ≠ 0 (r0' = c·r0 + s·r2, r2' = c·r2 − s·r0, 0x840CEF..0x840D82). No usa la posición del sprite | nadie (los 7 anillos de fn_008274A0 están sin portar) |
| `YawToEye` (modo C) | código inline: `TownCentre::DrawPSys` 0x69BE76..0x69BE8A, fn_00466BB0, `TownDesireFlags::Draw` 0x746BFC, fn_00719E90, `ScriptHighlight::Draw` | θ = atan2(ojo.z − p.z, ojo.x − p.x) + π/2 ([0x8C78D8]); ejes `lh_matrix::AngleY(θ)` (las filas de SetAngleY 0x674360). El +Z local va del ojo al objeto | nadie (columnas de influencia, banderas de deseo, ShowNeeds y ScriptHighlight están sin portar) |
| `ParticleYaw` (C') | `Particle3DObj::DrawAt` 0x679FD0, FaceCamera +0x4D, 0x67A032..0x67A1C3 | θ = atan2(d.z, d.x) − atan2(r2.z, r2.x), con d = p − ojo en XZ; r0' = c·r0 + s·r2, r2' = c·r2 − s·r0; r1 × HeightStretch | mallas de PSys con FaceCamera (`PSys/Creators/Mesh.cpp`) |
| `FullSprite` (D) | FaceCameraSprite +0x4C, 0x67A250..0x67A451 | Identidad × escala. Luego cada fila (x, y) := (cos φ·x + sin φ·y, cos φ·y − sin φ·x), con φ = π/2 − atan2(d.y, \|d.xz\|) (0x67A367), y después fn_0067A4A0(ψ), con ψ = atan2(d.z, d.x). El +Y local mira al ojo y la Z queda horizontal | `Mesh.cpp`, después de FaceCamera como en el original; ningún SF lo activa |
| `LookAtCentre` | la burbuja: fn_00518720, desde `OneOffSpellSeed::Draw` 0x518E90 | d = W − ojo, con W = el centro de la caja en el mundo (0x518746..0x5187B8). Si \|d.x\| y \|d.z\| son < 1e-4 (el double [0x8C79D8]), d.x pasa a ±1e-4 (0x518875..0x5188B4). D = normalize(d), U = normalize(Y − (Y·D)D). Las filas (U×D, −D, U) salen de invertir con fn_007FB3F0 (0x518B0C). Luego M = T(−c)·R·s y traslación W − c·R·s (fn_00518B90, fn_00518BF0, fn_0044CF90). Tras el empujón de 1e-4, d y U nunca son cero, así que la prueba de ceros de 0x5188BC..0x5188ED no salta nunca y no se porta | nadie todavía. `Magic/Core/OneOffSpellSeed.cpp` (de Milagros, sin commitear) sigue con su copia sin el empujón; pasará a `LookAtCentre` después de su HEAD |
| `BandToEye` | las bandas de potencia: fn_0051A830 (si el byte [0xBE8E8E] = 1, que nadie escribe), desde `DrawSpellGraphic` 0x51A773 | d = T − ojo, con T la traslación de la banda; el mismo empujón de 1e-4 que la burbuja; D = d / sqrt(d.y² + d.z² + d.x²), U = normalize(Y − (Y·D)D) con Y = (0, 1, 0) en 0xCC62C0. La matriz de columnas (−D, U, U×D) se invierte con fn_007FB3F0 (0x51AB5A), así que sus filas son −D, U, U×D. Luego M = M·R con fn_0046D9D0 y se repone T. En glm, R·(giro y escala de la banda) | `Worship/SpellSeedGraphic.cpp` (las bandas de la bola y de los iconos) |
| `MoonBasis` / `MoonModel` / `MoonHalo` (E) | fn_0086AC60 y fn_0086A930, leídas enteras | Ver la luna, debajo | `Renderer::DrawMoon` |
| `MistBasis` / `MistShrunkSize` (F) | `LH3DMist::Draw` fn_007FA300 0x7FA38F, rama del efecto 0x7FA483..0x7FA539 | Las 9 celdas son 0xEA1C98. Con el efecto, la fila 0 lleva el tamaño y las filas 1-2 tamaño / (1 + (k − 1)(1 − \|d.y\|/\|d\|)), sin límite. **(aproximado)** 1/\|d\| se saca con `std::sqrt` y no con la tabla de InverseSquareRoot 0x841170. **(inferido)** con d = 0 devuelve el tamaño | nieblas (`RendererMists.cpp`, `mists::Submit`) y nubes (`Renderer::DrawCloud`) |
| `ScreenVelocity` (G) | `UR_OrientSpriteWithVelocity` 0x69A790 (0x69A8ED..0x69A94B) y fn_006840E0 (UR_Flocking) | x = w·right, y = w·up (la rotación W2C); `SetAngleY(atan2(−y, x) + π/2)`. Con `Screen`, el +y del sprite queda a lo largo de la velocidad en pantalla | `PSys/Rules/Orient.cpp`, `PSys/Rules/Flock.cpp` |
| `RibbonSide` / `RibbonHalfWidth` (H) | fn_0067B3F0 (lee g_camera en 0x67B4BA) | lado de cada extremo del tramo = (ojo − articulación) × (cola − cabeza) (0x67B86C..0x67B924: la vista desde esa articulación, primero la cabeza y luego la cola), que es la misma dirección que normalize(cross(normalize(segmento), normalize(articulación − ojo))). Vértices = articulación ± lado·(+0xC de la articulación)/\|lado\| (0x67B9E6..0x67BA70). Ese +0xC es la escala del PSR (ChainJoint::DrawAt 0x679E9A), la misma que un sprite toma como semitamaño, así que la **semianchura es la escala** | `Graphics/RendererChain.cpp` (rayos, horquillas, rastro del gesto) |
| `VolBlend` (I) | `RenderParticleVolBlendMesh::DrawAt` 0x67CCB0 ([0xC029C4] = 1) | a = normalize(fila 0), d = normalize(ojo − p), b = normalize(a × d), c = d × b; filas (c, b, d) × escala | nadie (ningún SF usa ParticleVolBlendMeshCreator) |

**Sprites de PSys** (`Particle3DSprite::DrawAt` 0x67AE80):
- tamaño = la escala del PSR, con un mínimo de 0,0001 (0x67AEAF);
- ángulo = atan2(M[0][2], M[0][0]) del PSR, salvo con IgnoreRotation (0x67AF6B..0x67AF87, [0xC029A8] = 1);
- origen: ox = OriginX·tamaño y oy = OriginY·tamaño·estiramiento (CreateSprite 0x6AA0A8 y 0x67AEC4..0x67AF2D);
- CentreAtBase: y += estiramiento·tamaño·0,5 (0x67AFB4, [0x8AA3B4]);
- celda = (FileOffset + fotograma) & 63.

Todo eso vale también con SetHorozontal. Las llamas de `FireGraphic` usan el origen de su sprite compartido 0xDA09E8:
+0x1C = −2·tamaño (0x732382..0x732395), es decir, `SpriteOriginY` = −1, y no CentreAtBase. Así la base queda en el
punto a lo largo del «arriba» de la pantalla.

**Luna (modo E).**
- Base (fn_0086AC60 0x86AC67..0x86AEBD):
  - v = la posición en la cámara, n = normalize(v), t = normalize(n.z, 0, −n.x), u = n × t;
  - se lleva al mundo con SetInverse(W2C) (0x86AE64 / 0x86AE6F) y × [0xFA2750] = 4,0 (0x86A3F4, que sobrescribe el
    3,0 de 0x86A3D6);
  - el +Z local va del ojo a la luna y la X queda horizontal en la vista.
- Malla: esa base, inclinada con fn_0086AFA0(α = [0x9A3BF0] = −0,1309), luego RotateY(fase + π) 0x5198F0, luego
  × 0,65 [0x8AC420]. En glm es Rz(+7,5°)·Ry(−(fase + π)).
- Halo (fn_0086A930):
  - esquinas p ∓ 500(r0 + r1) y p ± 500(r0 − r1) (500 = [0x8C78EC], con las filas ya × 4);
  - UV (0,25, 0,25), (0,49375, 0,25), (0,25, 0,49375), (0,49375, 0,49375) (0xEDC304; v0 = abajo a la izquierda);
  - índices {0,1,3, 3,2,0} (0xEDC310).
- Reflejo (fn_0086B010 0x86B662..0x86B69C): la segunda llamada, en (x, −y, z) con [0xFA2774] = 1, solo dibuja su halo
  (0x86AC0F se salta el objeto luna). La luna reflejada es el DrawUnderWater (vt+0x118) de la primera. Con la cámara
  reflejada de openblack:
  - el halo se monta con la vista reflejada;
  - la luna usa la vista principal (`view·mirror(y)`).

**Diferencias con lo que había antes en openblack (arreglos de U1).**
- El giro de los sprites de PSys iba al revés (D1). Lo compensaban dos reglas que también giraban al revés:
  - `UR_OrientSpriteWithRandomAngle` ahora hace SetAngleY((1 − PSysFloatRand(2))·RandomAngle + DefaultAngle) en cada
    paso (0x6A2187..0x6A21AC);
  - `UpdateRuleRotatePrincipalAxis` ahora es `rotate(−dt·AngularVel)` (Z 0x6A116B, Y 0x6A1218, X fn_006A12F0). El
    original también gira la 4.ª fila de la matriz del AtomCore (+0x68..+0x70), que SetAngleY 0x674360 pone a cero;
    no se porta (**inferido**: no se usa para dibujar).

  Como las dos están arregladas, también giran como el original las mallas que usan la segunda.
- El origen se sumaba; ahora se resta (V4-b).
- CentreAtBase sumaba h·s entero; ahora suma h·s·0,5 (D2).
- La rama horizontal de PSys no tenía origen, IgnoreRotation ni CentreAtBase (D2b), y llevaba las V al revés: v0/v1
  van a −hs − oy (D2c, 0x84085D..0x840897).
- Faltaba el corte por near del modo A (0x84055D..0x840590).
- La luna usaba el plano de la vista y tenía invertidos los signos de la inclinación y de la fase (V4-a).
- Las V del halo estaban al revés (V4-d).
- `MistShrunkSize` ya no limita \|d\| a ≥ 1. Solo cambia algo con una niebla o una nube a menos de 1 m del ojo.

D2b, D2c, los dos arreglos de RotateAxis y RandomAngle y el corte por near no estaban en la lista inicial del plan.
Están verificados en el binario, pero **falta la aprobación del jefe de sesión**.

Lo demás da los mismos vértices que antes: estela, peces, anillos, SmokyStuff, nieblas y nubes a más de 1 m,
FaceCamera, cadenas y la burbuja. Detalle por archivo: `dev\documentacion\unify\U1_changes.md`.

**Huecos.**
- (inferido) El vector w de UR_OrientSpriteWithVelocity antes de 0x69A8ED.
- (inferido) Con IgnoreRotation, +0x14 vale 0 (fn_006A84C0 sin leer).
- (inferido) El dibujo de los mapas de luz como sprite horizontal.
- (inferido) La 4.ª fila del AtomCore que gira UR_RotatePrincipalAxis no se usa para dibujar.
- (aproximado) El near de `CameraFrame` va hasta 0,01 por detrás de [0xE839E0].
- (aproximado) `MistShrunkSize` usa `std::sqrt`, no la tabla de 128 bytes de InverseSquareRoot 0x841170
  (MakeInverseSqrtLookupTable 0x8411D0, más un paso de Newton en 0x8411B0..0x8411C2).
- (aproximado) El vapor y el humo del fuego salen centrados. En el original heredan el oy = −2·tamaño de la última
  llama dibujada con el sprite 0xDA09E8, porque 0x7323F0..0x7325C5 no escriben +0x18 / +0x1C. fn_00732200 dibuja por
  cada fuego las llamas, después el vapor y después el humo, así que es la llama más vieja de ese fuego, o la del fuego
  anterior si no tiene llamas. Así que en el original salen 2 veces ese tamaño más arriba. Para portarlo haría falta un
  origen por átomo en `psys::manager::DrawAtom` y saber el orden de los fuegos.
- Sin portar, aunque leído:
  - el recorte a [0, 639] × [0, 479] de los vértices de un sprite sin recortar (0x840A47 / 0x840A85);
  - la ruta fn_007A8DB0 con [0xEA9EB4] ≠ 0;
  - el empujón de 1e-4 de la burbuja en la vertical (0x518875..0x5188B4): está en `LookAtCentre`, pero
    `OneOffSpellSeed.cpp` es de Milagros y lo tiene sin commitear. Sin él, con la cámara justo en la vertical
    openblack conserva el giro anterior.

## Texturas animadas por fotogramas

**Fiel**, salvo lo marcado. **El original nunca mezcla dos fotogramas.** Cada usuario elige un fotograma entero
(`__ftol`, que trunca, o una división entera; solo el brillo de la mano usa `fistp`, que redondea) y dibuja una sola
vez. Lo que parece fluido sale de dos cosas: muchos fotogramas, de 15 a 25 por segundo, y los desplazamientos
continuos de UV (scroll). Las PSys interpolan el **número** de fotograma entre dos pasos y después lo truncan: eso es
un escalón, no una mezcla. En openblack, fundir dos fotogramas solo existe como opción de mod
(`AnimatedSprite::blend`), apagada por defecto.

Tampoco hay un reloj común: cada usuario guarda su acumulador (por objeto, o global donde el original lo tiene global)
con sus constantes. Por eso la API tiene **una función por reloj del original** y el estado lo guarda quien llama, en
el sitio donde lo guarda el original.

Los relojes se calculan en float porque el original corre la FPU a 24 bits: `fn_007DEE00` borra los bits de precisión
(ver [camera-tracks.md](camera-tracks.md)). Así, cada fadd, fmul o fsub de la pila redondea como una operación de float,
y el valor que queda en la pila entre un `fmod` y el `__ftol` es el de float. Por eso, cuando el `fmod` da un negativo
diminuto, el «+ 32» de las fiolas redondea a 32.

**API.** `graphics::frame_anim` (`src/3D/FrameAnim.{h,cpp}`), solo lógica:
- Primitivas del motor:
  - `SpriteCell` / `SpriteCellUv`: la celda del LH3DSprite, `flags & 0x3F` (LH3DSprite::Draw 0x840530). Sus UV son las
    de `billboard::CellUv`, que no se duplica.
  - `UvOffset` / `IsAnimatedUv` / `OffsetUv`: `SetAnimatedUV_1` (vt +0xE8, 0x7F9B70; +0x68 / +0x6C). Cada dibujo de
    LH3DObject lo copia a [0xECA62C] / [0xECA630], con [0xECA628] = 1 si no son los dos 0. DrawTriangle 0x82F8BE..0x82F8F7
    lo suma a cada vértice **salvo** si el material tiene el bit 0x10 del byte +5. fn_0082F920, fn_0082FD70 y
    fn_00884750 lo suman sin mirar el bit.
  - `PackUvOffset` (openblack): el transporte a `vs_object.sc`, v + 4·round(256·frac(u)). Es exacto para todos los
    usuarios del original: los cuartos de la burbuja, los pasos de 32/256 de las fiolas y las celdas de píxeles
    enteros de AnimTextured. Solo un SlideU de 1000 fotogramas se redondea a 1/256.
  - `AnimTexturedCell`: Particle3DObjAnimTextured::DrawAt 0x67A530, leído entero. Con celdas, cols = 256 / W (idiv),
    u = (W/256)·(f % cols) y v = (H/256)·(f / cols), con f sin signo. Con deslizamiento, u = W·f / (N·256) y
    v = H·f / (N·256) ([0x8D45CC] = 256). openblack añade una guarda: cols ≥ 1.
- Relojes (tabla de abajo).
- Cargadores: `LoadBitmapFromFile` (`GJBitmap::LoadBitmapFromFile` 0x57CA90: solo con el tamaño exacto, 0x57CAD2;
  mín(framesInUse, framesInFile) fotogramas de Pitch × Pitch sacados de la rejilla de √n por fila de `fn_0057CB40`) y
  `FrameTexels` (un fotograma, `fn_006CA280` 0x6CA2E3); `land_light::LoadBitmapFile` lee el archivo. `LoadGif` y
  `GifDelayMs` para mods (stb; los retrasos de menos de 20 ms valen 100 ms, como en los navegadores).
- Mods: `DelayClock` (duraciones por fotograma, en bucle, fotogramas enteros; da también la fracción dentro del
  fotograma) y `AnimatedSprite` (celdas o capas consecutivas desde `first`, con `blend` apagado por defecto).

| Reloj | Original | Regla | Usuarios en openblack |
|---|---|---|---|
| `OneOffFrame` | OneOffSpellSeed::UpdateFrame 0x72A570 | fase = fmod(fase + ms·18·0,001, 16) ([0x981FB4], double [0x982820]); celda (f % 4, f / 4)·0,25 ([0x981FB8]) | la burbuja (`one_off::UpdateFrames`) |
| `SpellIconFrame` | DrawSpellGraphic 0x519AD0, rama de las fiolas (0x519B79..0x519C1B) | +0x34 = fmod(+0x34 − 15·dt, 32), +32 si < 0 ([0x8D86F0], [0x8D8740], [0x8CF134]); u = (f % 8)·(1/256)·32, v = (f / 8)·(1/256)·32 | las fiolas de hechizo de criatura (`seed_graphic::DrawSpellGraphic`) |
| `HandFlowFrame` | PHandFX::Draw 0x68D0C0 (0x68D29B..0x68D374) | +0x58 += dt·(−20); con ritmo > 0, fmod(.., 64) si pasa de 64; con ritmo ≤ 0, fmod(.., 64) + 64 si < 0; f = **fistp** (redondea) % 32; (f % 8, f / 8)·0,125 | el brillo de la mano (`hand_fx`, calculado y sin dibujar) |
| `PSysFrameAdvance` | AtomCore, fn_00673EA0 (0x673FB8..0x6740D8) | prev = cur; **solo con [0xC029DC] y PlayAnim (+0x118) = 1** (si no, 0x673FC6..0x673FDE saltan a 0x67406A: ni paso ni vuelta), cur = dt·ritmo + prev; con ritmo > 0, mientras los dos pasan de 2N, −N; con ritmo ≤ 0, mientras alguno es < 0, +2N | todos los átomos (`Effect::PostUpdate`, con `Atom::playAnim`), el polvo, los granos y los peces de `HandEffects` |
| `PSysFrameLerp` / `PSysFrameIndex` | fn_00679920 (0x679A7D..0x679B61) | t' = clamp(t, 0, 5) ([0x9357B8]) en bucle y clamp(t, 0, 1) sin él; f = prev + (cur − prev)·t'; en bucle ftol(fmod(f, N)) (+N antes si es negativo), sin bucle ftol(f) en 0..N−1 | sprites de PSys, mallas AnimTextured, TownBelief, FireGraphic, mapas de luz |
| `MistCell` / `MistCellUv` / `MistAdvance` | LH3DMist, fn_007FA300 | +0x84 += ftol(ms·0,255) ([0x9A2BA8]), %= 900 si > 900 (0x384); celda (c·45/900) & 15; UV ((f & 7)·0,125, (f >> 3)·0,125 (+0,25 en la rama de efecto, 0x7FA466; sin él, 0x7FA69E)) | nieblas del mapa, nubes, bocanadas de tormenta, nieblas de PSys |
| `MistCell` / `SmokeAgeStep` | LH3DSmoke, fn_007F8E00 | dt = min(ms·0,001, 100) ([0x8AB41C]); edad += ftol(dt·255) ([0x8AB270]); celda = MistCell(edad) | humo de las chimeneas |
| `FireCell` | fn_007321B0 | ftol(fmod(−25·edad, 32) + 32) ([0x999668]); edad = SpritePos +0x2C | llamas de FireGraphic |
| `SteamCell` | SteamGetOffsetFromAge 0x7321E0, fn_0073250A | ftol(fmod(25·edad, 32)) ([0x99966C]) | vapor y humo gris de FireGraphic |
| `FishFrame` | fn_008248E0 (0x824960..0x8249CE) | dt = min(dt, 0,1) ([0x8AB22C]); +0x1C += dt·vel·25 ([0x8C7BD0]); celda (8 + (ftol & 15)) & 63 **antes** de la vuelta; luego −= 15·ftol(+0x1C·(1/15)) | peces de las piscifactorías |
| `LanternAdvance` / `LanternStart` / `LanternCell` | fn_00823570 (0x823599..0x82362D); fn_00823240 (0x8233E4..0x8233F8) | reloj **global** [0xEB99C4] += ms, %= 700 si > 700 ([0xC383C8]); a = c·31/700; llama i: (10i + 31 − ((inicio[i] + a) & 31)) & 31. Inicio = la tabla **global** 0xC383BC ({0, 13, 0} en el fichero): cada luz nueva escribe ftol(Random(0, 31)) en sus tres entradas, así que todas las luces usan los valores de la última creada | faroles y hogueras (`night_lights`) |
| `LeashCell` / `LeashScroll` | fn_00466730 (0x46690A..0x466955, 0x466855..0x46687B) | +0x74 += 10·dt, −= 15·ftol(+0x74/15), celda ftol & 63; +0x60 += 0,5·dt, −= ftol | sin portar (correas de la ciudadela) |
| `GoldenShowerCell` | fn_006CA990 (0x6CAB1F..0x6CAB5B) | (t/50 + base + gota) % 32, con signo | sin portar |
| `CreatureRoomCell` | CreatureRoom::DrawAdditional 0x788630 (0x7889A5..0x7889CC) | 31 − (((GetTickCount() >> 5) + i) & 31): reloj **real** | sin portar |
| `CursorCell` | CameraModeNew3 0x456BED | (GetTickCount() / 50) & 15: reloj real, 20 por s | sin portar (cursor 3D) |
| `HelpSystemCell` | fn_005C0700 (0x5C0A7A..0x5C0AAF) | [0xD15AB0] += fn_005557E0(); (c / 200) & 15 | sin portar (HelpDude) |
| `JCSpecialCell` | fn_00828A70 (0x828CDE..0x828D7A) | f += ms·0,01; si f > 15, f = 0 (no fmod); ftol & 15 | sin portar |
| `PlayerSymbolCell` / `PlayerSymbolSpin` | PlayerSymbolSprite::Draw 0x69D7E0 | capa 0: +0xC −= ms·0,02 ([0x937538]); capa 1: +0x10 −= ms·0,023 ([0x937534]); +32 mientras < 0; ftol & 63. Giro del segundo brillo: +0x14 += ms·0,002 ([0x92A544]), −2π mientras > 2π. El ctor fn_0069D5A0 los pone a 0 | los brillos de TownBelief (un acumulador por símbolo) |
| `SmokyStuffCell` | fn_00823F70 (0x8240F9..0x824115) | ftol(vida·15) & 63 | bocanadas de SmokyStuff |
| `DustCell` | fn_00846010 (Dust.cpp) | 16 + ((rand % 16 + ftol(2·edad)) & 15) | polvo de los choques |
| `WaterfallScroll` | DesignedWaterFall 0x5E392E..0x5E3972 | V −= 0,5·dt ([0x8AA3B4]), menos su parte entera; SetAnimatedUV_1(0, V) | la cascada de Land 3 |
| `GoolooFrame` | fn_005E6390 | t de 500 ms a 0; x = t/500; UV (2·cos x, 1,7·sin(0,7·x)); byte +4 del material = 255 − ftol(255·t/500) | sin portar (fantasma al quitar un objeto) |
| `RotatingUv` / `RotatingUvClock` | RenderParticleGJMeshRotatingUV::DrawAt 0x67CBA0 y GameUpdate 0x6C8BC0 | Dibujo: lerp(+0x24 → +0x2C, t), lerp(+0x28 → +0x30, t) con t = DrawData +0x14 (0x67CBA8..0x67CBC1); se resta el periodo (+0x3C, +0x40) mientras lo pasa (0x67CBC8..0x67CBFC; nada si es negativo). Paso: la regla suma dt·SpeedU/V al **destino** +0x34/+0x38 y `GameUpdate`, al final de `PostUpdateAtoms` fn_00673EA0 (0x674080, `vt+0x108`), sube un periodo +0x34 y +0x2C mientras los dos están por debajo de −2·periodo (0x6C8BDF..0x6C8C5A) y los baja mientras los dos pasan +2·periodo (0x6C8C5B..0x6C8CCC) — se mueven en pareja, así que la diferencia que interpola el dibujo no cambia —, y después copia +0x2C → +0x24 y +0x34 → +0x2C (0x6C8CCD..0x6C8CE2) | discos de SurfRevol |
| `ChainScroll` | fn_0067B3F0 (0x67BE88..0x67BED5) | +0x3C += ms·ritmo·0,001, fmod(FrameHeight/256), + eso si < 0 | cadenas de PSys |
| `ChainSegmentUv` | fn_006C8920 | ver «Cadenas» abajo | cadenas de PSys |

**Las cadenas.** fn_006C8920 da las UV del tramo i de S = articulaciones − 1:
- k = ((i + 1)·T − 1) / S, b = k·S / T, n = (k + 1)·S / T − b y j = i − b, todo en enteros. T es NumTexturesForWholeChain,
  o S cuando vale −1 (CreateChain 0x6AA8DC).
- F = FileOffset + (k = T − 1 ? FrameOfHead : k = 0 ? FrameOfTail : 0).
- uv0 / uv1 = (F·W, H·j/n) y ((F+1)·W, H·j/n); uv2 / uv3 = lo mismo con j + 1. Todo × 1/256 ([0x938EBC]) y el scroll
  +0x3C sumado a las cuatro v.
- Es decir, **la V corre a lo largo de la cadena y la U cruza la cinta** por una columna de W píxeles. Los valores por
  defecto del creador son FrameHeight 64, FrameWidth 32 y NumTexturesForWholeChain −1 (0x6AA739..0x6AA747).
- El ritmo del scroll (+0x4C) solo lo ponen UR_SimpleBeam (SpeedV, 0x6762EE) y UR_Plasma (0x676898). Ninguno está
  portado, así que el scroll de todas las cadenas de openblack vale 0.
- fn_0067B3F0 hace cuatro vértices por tramo (0x67BA82..0x67BB0D): v0 = cabeza + lado, v1 = cabeza − lado,
  v2 = cola + lado y v3 = cola − lado. Les da uv0..uv3 (0x67BEE4..0x67BEFD). fn_0081C780 copia la UV de cada vértice
  tal cual en LH3DP3::Table1 +0x18 / +0x1C (0x81C9C7..0x81C9D0) y dibuja con DrawTriangle 0x82F810 (0x81CCB2). Por
  eso U = F·W va del lado +lado.
- Luego 0x67BFAC..0x67BFEE pone el mismo scroll en [0xECA630] ([0xECA62C] = 0, [0xECA628] = 1), y DrawTriangle
  0x82F8BE lo suma otra vez: el material de fn_006AA800 solo tiene los bits 0 y 2 de +5. (La verificación de animtex,
  V1-2, decía que la tira usaba fn_0082F920. No es así: la llamada de 0x81D152 está en otra función, la de 0x81CCD0,
  porque fn_0081C780 acaba en 0x81CCBE.)

**Arreglos deliberados** (el original es distinto de lo que hacía openblack):
- PSys:
  - el fotograma se guarda en [0, 2N) con su anterior (fn_00673EA0), y el índice se elige como en fn_00679920 (fmod y
    +N una sola vez; antes era fmod(fmod + N));
  - sin PlayAnim no hay ni paso ni vuelta (0x673FC6..0x673FDE), así que un fotograma negativo se queda como está;
  - un átomo sin bucle con ritmo negativo salta a ~2N y enseña el último fotograma, no el primero. Ningún .zzz del
    juego tiene LoopAnim 0 con FrameRate < 0 o con RandomiseFrameDirection, así que en el juego no se ve. Son 169
    creadores con la propiedad PlayAnim (137 sprites, 21 mapas de luz, 6 mallas AnimTextured, 4 de animación y 1 de
    animación con cámara), y los diez con ritmo negativo (bolas de fuego, SF_Flash, el tornado y el cono de lluvia)
    van todos en bucle.
- Nieblas de PSys: cada átomo lleva su contador (`Atom::mist`). Empieza en ftol(Random(0, 16)) & 15 (0x7F95F8) y solo
  avanza si la niebla sale en pantalla. Antes había un solo contador global.
- Bocanadas de tormenta: el contador avanza solo si la bocanada pasa de alfa 5 (AddDrawing) **y** su esfera está en
  pantalla (`mists::InView`, la prueba de LH3DMist::AddDrawing 0x7FA7F0). Ya no está (aproximado).
- Peces: la celda se toma antes de la vuelta (puede salir la 23) y la vuelta es −15·ftol(f/15).
- Faroles: el inicio de cada llama sale de la tabla global 0xC383BC, que reescribe cada luz nueva con
  ftol(Random(0, 31)). Así, todas las luces van en fase con los valores de la última creada (antes cada luz tenía los
  suyos). El reloj es entero y solo corre si el alfa del pueblo no es 0 y hay luces (0x82357A..0x823593). El halo es
  la celda 56 de `smoke` ((flags & ~7) | 0x38, 0x8233BC..0x8233EB).
- TownBelief: los brillos llevan su acumulador por símbolo (+0xC, +0x10 y el giro +0x14, desde 0), avanzado con los
  ms enteros de cada fotograma. Antes usaba el tiempo global (−s·20, −s·23).
- Brillo de la mano: el fotograma se redondea (fistp 0x68D323), no se trunca, y la vuelta es «> 64», no «≥ 64».
- Cadenas: las UV de fn_006C8920 (antes la U recorría la cadena con la textura entera repetida), los valores por
  defecto del creador, FileOffset y el scroll. Además, uv0 va del lado +lado (antes del −lado: la U estaba
  espejada) y la semianchura es la escala, no 0,5·escala, así que **las cintas salen el doble de anchas**.
- Fiolas de hechizo de criatura: la animación UV de 0x519AD0, que antes no estaba.
- Bandas de potencia: `billboard::BandToEye`, ver [billboards](#objetos-que-miran-a-la-cámara-billboards).
- Discos de SurfRevol: el desplazamiento UV se interpola entre dos pasos (`frame_anim::RotatingUvClock`, DrawAt
  0x67CBA0 con t = DrawData +0x14 y GameUpdate 0x6C8BC0 entero). Antes la regla envolvía el valor en [0, periodo) cada
  paso y el dibujo lo usaba tal cual, así que el disco giraba a saltos de un paso de PSys.

**Igual que antes, bit a bit:** la burbuja, las llamas y el vapor (la celda 32 en el primer fotograma ya estaba), el
humo de las chimeneas, SmokyStuff, la estela del barco, los anillos (celda fija `& 63` de la hoja 8×8 de `smoke.raw`,
fn_005E5100), las nieblas del mapa y las nubes, la cascada, los montones de comida (0x51C0FD), la disposición
de celdas de AnimTextured, la malla y las UV de los discos de SurfRevol, el polvo, la mano y las mariposas GIF del
mod. El polvo al
agarrar tierra y los granos y peces al coger comida van ahora por `PSysFrameAdvance` / `PSysFrameIndex`: dan las
mismas celdas, salvo el redondeo de sumar dt·ritmo en vez de multiplicar edad·ritmo.

**Huecos.**
- (aproximado) Todos los relojes de niebla y humo guardan la fracción de ms·0,255 (o dt·255) de un fotograma al
  siguiente. El `ftol` del original pararía la animación a más de 250 fps. `MistAdvanceExact` es la fórmula tal cual.
- (aproximado) Las bocanadas de tormenta empiezan con el contador a 0, no en Random(0, 16) & 15: es la misma celda 0.
- `graphics::lh3d::Random` (src/3D/LH3DRandom.h) reenvía a `game_random::crt::Random` (Random 0x81D180 sobre el
  `rand()` de la CRT, semilla 1 al arrancar: `__initptd` 0x7D2323; el `srand(time)` de 0x577721 solo corre al guardar
  `creature.lhp`). Lo comparten las nieblas del mapa, las de PSys, las bocanadas de tormenta, la lluvia y el temblor de
  cámara. (aproximado) el original tiene una semilla por hilo; aquí una.
- TownBelief toma g_game_time_inc de `game_clock::FrameGameMs()` (0x69D855; antes, aproximado, del reloj de pared).
- Los faroles (fn_00823570), las nubes del cielo (su movimiento, su contador de atlas y el alineamiento del cielo,
  `Renderer::UpdateClouds`), las nieblas del mapa (`CollectMists`, fn_007FA300) y el humo de las chimeneas
  (`CollectChimneySmoke`, fn_007F8E00) toman también g_game_time_inc de `game_clock::FrameGameMs()` (U7). Antes salía
  del reloj de pared, escalado por la velocidad del juego y con tope de 100 ms, y los faroles guardaban la fracción de
  ms (`WholeMilliseconds`, que se quita: el reloj del juego ya da ms enteros y guarda él el resto del turno). Las
  nieblas y el humo se recogen una vez por fotograma, solo en la vista principal.
- (inferido) Que S_Fire se dibuje en 8×8 como S_SpriteSheet3.
- (inferido) GoldenShower: t en milisegundos. Gooloo: que el byte +4 del material sea el ALPHAREF.
- HandEffects (polvo al agarrar tierra, granos y peces al coger comida) sigue siendo una copia a mano de efectos que en
  el original son PSys (SF_GripLandscape, ER_MultiPickup). Sus relojes de fotograma ya son los de PSys.
- Sin portar, con su reloj y su prueba en la API: InfluenceCircle (scroll 0,0001 / −0,0002 por ms, 0x826C90),
  Gooloo, GoldenShower, las correas, la habitación de la criatura y el mapa del mundo de la ciudadela, HelpDude, el
  cursor 3D y JCSpecial. También HandGlow / fn_0083F270, el scroll que no es de LightSheet sino del objeto de
  fn_0083F100 / fn_0083F210, leído por fn_0084F910.
## Mallas pegadas al suelo (land_morph)

**Fiel**, salvo lo marcado. Todo lo que se amolda al terreno pasa por una sola API, `openblack::land_morph`
(`src/3D/LandMorph.{h,cpp}`), y en GPU por un solo include, `assets/shaders/land_altitude.sh`. Toda altura es
`LH3DIsland::GetAltitude` 0x803090: en CPU `LandIsland::HeightAt` (exacta, 16.16), en GPU `LandAltitude` (la misma
lógica en float: diagonal `split` y aplanado junto al mar), en el x, z de mundo del punto (`fn_004427B0` /
`fn_00653150`: x·65536·0,1 truncado, [0x8AC408], [0x8AC404]). En el original no hay una rutina única: hay una
altura y cuatro algoritmos que la aplican.

| # | Algoritmo | Original | Cuándo | API | Usuarios en openblack |
|---|---|---|---|---|---|
| A | Cortar la malla por el terreno y subir cada vértice `y += H(v) − H(origen)` | `fn_00686980` + corte `fn_00686D90` | una vez al crear (0x6867F9), solo con `DoRaiseAboveLandscape`; sin la «respiración» de cada fotograma (0x686805) | `SplitByPlane`, `CellPlanes`, `RaiseAboveLandscape` | `PSys/Rules/SurfRevol.cpp`: `SF_TeleportVortex`, `SF_SpellDispenserVortex` |
| B | *Melting*: un delta por vértice `(H(v) − H0) / escala`, en espacio modelo, a lo largo de la Y local | `UpdateMelting` 0x8168F0 + Draw morfable 0x80E550 | al crear (`Snapshot`); `PhysicalShield` en cada dibujo (`Live`) | `components::MorphWithTerrain{mode}`, `Melting`, `MeltingDeltas`, `LandMelting` (GPU), `ObjectProgram` | vs_object_hm_instanced: edificios morfables, campos, montones, BigForest, escudo físico, arca y dinosaurio, marcas del suelo |
| C | *Bake*: el mismo delta escrito en los vértices | FragMesh `fn_007F72B0`; ClampToLandscape `RenderParticleGJMesh::DrawAt` 0x67C313; MeltBorder 0x816350; ciudadela vt+0x208 `fn_00882B10` | FragMesh al romperse; ClampToLandscape cada fotograma, todas las primitivas, sin corte | `Bake`, `Raised`, `BakeAgainstY` | `ECS/Physics/FragMesh.cpp`; `SurfRevol.cpp` (`SF_LandscapeVolcano*`, `SF_LandscapeVortex*`); llamas de un objeto morfable (`ECS/Fire/FireGraphic.cpp`); el punto del tótem sobre el centro del pueblo (`GetExtraPos` 0x80FF20, `AbodeArchetype.cpp`) |
| D | Geometría hecha sobre el suelo, `y = H + constante` | manchas `fn_0081FFF0`; InfluenceCircle `fn_008265F0`; correa `fn_008491B0`; quads de la criatura `fn_0081F360` | al crear / cada fotograma | `OnGround`, `k_BlobLift`, `k_LeashRibbonLift`, `k_CreatureQuadLift`, `InfluenceCurtain` | manchas (`Renderer::DrawHumanShadows`); el picking de los morfados (`HandPlacement.cpp`) |

**A, cortar y subir.** `fn_00686980(M, mesh)` pasa la malla al mundo con la matriz del átomo (`fn_00673E40` en
0x6867E8: el marco local de `fn_00673DB0` en la jerarquía, `fn_006752D0`), saca la caja de todas las primitivas
(`fn_006C99E0`: centro = (máx + mín)·0,5 y semiejes = (máx − mín)·0,5) y corta **solo la primera primitiva del
primer grupo**:

| Paso | Detalle | Dirección |
|---|---|---|
| Índices | x0 = −1 − ftol(−0,1·mín.x), x1 = 1 − ftol(−0,1·máx.x), igual en z (= ⌊mín/10⌋ − 1 .. ⌊máx/10⌋ + 1 con x ≥ 0) | 0x6869EF..0x686A41, [0x8C7B10] |
| Planos | x = 10i (n = (1,0,0)), z = 10j (n = (0,0,1)), x + z = 10k y x − z = 10k (n = (s,0,±s), s = 1/√2 en double, por (10i, 0, 10·z1); i de x0 − dz a x1 y de x0 a x1 + dz, dz = z1 − z0) | 0x686A53, 0x686AF9, 0x686BDB, 0x686C8B; [0x8AB414], [0x8AC410] |
| d del plano | −n·p con `fn_00453F50` ((z z' + y y') + x x') | 0x686AB2..0x686ACC |
| Corte | dist = ((y ny + z nz) + x nx) + w; dist ≤ 0 cuenta como negativo; si los tres signos son iguales no se corta; A = el primer vértice cuyo signo es el producto de los tres | `fn_00686D90` 0x686E3B..0x686F9A |
| Nuevos vértices | t = −dA/(dO − dA); posición, UV y normal = A + t(O − A) (la normal sin renormalizar); cada byte de color cA + ((cO − cA)·ftol(255t) >> 8); difuso, especular y normales solo si hay uno por posición | 0x686FEB..0x68732B, [0x8AB270] |
| Triángulos | n1 (arista AB) y n2 (AC) al final; tri := (A, n1, n2), se añaden (B, C, n2) y (B, n2, n1); solo se prueban los triángulos que había | 0x687386..0x687413 |
| Subir | H0 = H(M+0x24, M+0x2C); y = (H(v) − H0) + y en la primera primitiva | 0x686D01..0x686D55 |
| Volver | la inversa de M (`SetInverse` 0x7FB290) | 0x686D61..0x686D78 |

Como `GetAltitude` es lineal dentro de cada triángulo de tierra y los cortes siguen las aristas de las celdas y sus dos
diagonales (sea cual sea el bit `split`), la malla cortada sigue el terreno **exactamente**. openblack lo hace en
`ZR_SurfRevol::MakeSurface`, al crear la superficie, con el marco del átomo en la jerarquía (`Effect::LocalToGlobal` /
`GlobalToLocal`), y la deja en espacio local: al dibujar se mueve y escala con el átomo, como los deltas locales del
original. **(aproximado)** El marco es el que usa `surf_revol::Collect` al dibujar (giro · baseScale · ruleScale, sin
el estiramiento en Y de `fn_00673DB0`): se toma la M de `fn_00673E40` como la PSR dibujada de `fn_00673EA0`, y
`Collect` no aplica el estiramiento. Los dos usuarios tienen estiramiento 1 (sin `StretchVertically` ni reglas que lo
cambien). Antes se subía cada vértice al dibujar y sin cortes, así que las cuerdas del disco atravesaban los pliegues
del terreno. `HeightAboveLandscape` y `RaiseAboveLandscapeRadius` no los lee nadie (0x6B30EF, 0x6B312A).

**B, melting.** `UpdateMelting` 0x8168F0 sale si no hay búfer de deltas ([+0x80] == 0) o si vt+0x1B0 ≠ 0 (0x7F9BF0 =
0 en las dos vtables morfables); a0 = H(+0x38, +0x40), inv = 1/[+0x44]; para cada vértice de cada submalla,
w = v·M + pos y delta = (H(w) − a0)·inv (0x816A5E..0x816A77). El Draw 0x80E550 lo suma a la y del modelo antes de la
matriz. En openblack lo hace `LandMelting` en vs_object (programas `ObjectHeightMap*`): en el mundo es
columna 1 · (H(w) − H(origen)) / escala, con la columna 1 de la matriz dibujada (su eje arriba) y la escala de la
columna 0 (giro por escala uniforme; los vaivenes solo tocan la columna 1). Con la columna (0, s, 0) es
`y += H(w) − H(origen)` exacto, como antes. El único morfado inclinado es el campo maduro: su vaivén (`Field::Draw`
0x528570) cizalla la columna 1 a lo largo de z, así que sus vértices suben H − H0 y además se corren
1,75·vaivén·(H − H0) en z, como en el original (antes solo subían). Las clases morfables (Get3DType
1 / 8) están en `MorphWithTerrain.h`. El escudo físico es una (Get3DType 0x72CE50 = 1); el mágico no (0x72C340 → el
estático).
- **Snapshot / Live (aproximado).** `MorphWithTerrain::mode` guarda cuándo toma el original los deltas: `Snapshot` al
  crear, `Live` en cada dibujo. `Live` es solo `PhysicalShield`: `CallVirtualFunctionsForCreation` 0x72CD23,
  `SetUpPhysOb` 0x72CEB8 y `DrawShield` 0x72D01E, tras interpolar la matriz y la escala. El shader los calcula en cada
  dibujo para los dos: los deltas congelados harían falta por instancia y por vértice, y las instancias comparten la
  malla. Solo difieren si la tierra cambia después de crear el objeto, y en openblack solo cambia en
  `FlattenLandUnderTemple` (`CitadelArchetype.cpp`), mientras se carga el mapa.
- **Un programa.** `land_morph::ObjectProgram(shaders, morph, pass)` es la única elección entre `ObjectInstanced` y
  `ObjectHeightMapInstanced` (y sus `Shadow`): la pasada principal, la mezclada, el reflejo de objetos y la sombra de la
  mano sobre objetos.
- **Corte por el plano.** El `DrawCutByPlane` de las vtables morfables (vt+0x11C) es 0x80BA50, un `ret`: un objeto
  morfable cortado no se dibuja. `Renderer::DrawCutByPlane` sale igual. Hoy ningún morfado lleva `CutByPlane` salvo el
  montón de comida de `OPENBLACK_TEST_SEA=...,pot` con `OPENBLACK_TEST_CUT=1`, que ya no se dibuja cortado.

**C, bake.** `Bake` / `Raised` hacen `y = (H(v) − H0) + y`, el orden de float del original:
- FragMesh `fn_007F72B0` (v.y − (H0 − H) en cada FragVertex, 0x7F7510..0x7F7563; solo si `IsStaticMorphable`,
  vt+0x1F4 → [0xE920E8], 0x7F6FF5): `FragMesh::FromEntity`.
- **ClampToLandscape** (`RenderParticleGJMesh::DrawAt` 0x67C313..0x67C38C; el byte +0x22 que pone 0x686432 desde la
  regla +0x6C): cada fotograma, todas las primitivas, **sin corte**, con H0 en la posición de la matriz dibujada. Solo
  lo llevan los 5 `SF_Landscape*` (volcán y vórtice de terreno, `FunctionIndex` 1-3). Está en `surf_revol::Collect`.
- Las llamas de un objeto morfable (bit 0 de +0xB5, `fn_00732220` 0x7322A9..0x7322D2 y 0x73230A..0x732366): el mismo
  delta sobre cada llama.
- MeltBorder 0x816350 (solo 0x49D303) y la ciudadela `fn_00882B10` (`v.y − (pos.y − H)`, 0x882DBE..0x882DD7; desde
  0x462FE2 y 0x8829AF): `BakeAgainstY`, sin usuarios todavía.

**D, sobre el suelo.** `OnGround(ground, xz, k) = H + k`. Las manchas de aldeanos y animales ponen cada punto en
H + [0xEAA3C4] (= [0xC381DC] = 0,2, de 0x81E350). El picking de `HandPlacement` pone el origen de un morfado en
H + el hundimiento del montón: **(aproximado)**, porque el origen dibujado es la y guardada; da lo mismo mientras la y
guardada sea esa. `InfluenceCurtain` es la cortina de `fn_008265F0`, leída entera:

| Dato | Valor | Dirección |
|---|---|---|
| Segmentos N | clamp(ftol(2πr·0,05), 8, 250) | [0x8AB210], [0x8C7BD4], 0x826659..0x826670 |
| Vértices | 3 por segmento, más 3 que cierran el anillo en el ángulo 0: (cos·r + cx, (H − H0) + H0 + {0, 20, 40}, sin·r + cz) | [0x8C7658], [0x8CF300] |
| U | += (1 − ftol(2πr·(−1/111))) / N | [0x9A3930] |
| V | += min(ftol(r/60), 6) / N; filas v, v + 0,2 y v + 0,4 | [0x8C5818], [0x8AB244], [0x8C7A44] |
| Color | el del jugador, [0xEA9EFC + 4·jugador] | 0x82695B |
| Triángulos | (b, b+3, b+4), (b, b+4, b+1), (b+1, b+4, b+5), (b+1, b+5, b+2), b = 3i | 0x8269DA..0x826A34 |

La correa (`fn_008491B0`: bordes en H + 0,1, [0x8AB22C]) y los quads de la criatura (`fn_0081F360`: H + 0,15,
[0x8CF110]) solo tienen su constante: no están portados.

**Marcas del suelo** (`ecs/GroundMarks`). `fn_00825240` (lista 0xEB9A00) es un `LH3DObject::Create(1)` de la malla
0x251 (`TreeRootsPile`). Se funde con la tierra una vez (0x8252C3), dura 15000 ms y se desvanece el último segundo:
`fn_00825350`, desde 0x5E6197, pone alfa ftol(vida·0,255) con vida ≤ 1000 ([0x9A2BA8]), resta `g_game_time_inc` y a 0
la borra con `fn_00825300`. Tiene dos usuarios:
- el cráter de un árbol arrancado (`fn_008251F0` ← `fn_0074BD20`, escala (extensión x + z)·escala·0,3, [0x8AB23C]);
- la marca de una explosión en tierra seca (`fn_008251C0` ← `UR_Explosion::InitCollection` 0x67E395: giro
  `PSysFloatRand(2π)` con `SetPosition` 0x423140, escala [0x9357D4] = 8).

La lista se vacía con el mapa (`ClearAllStuff` 0x82AED0, desde `GGame::ClearMap` 0x552F22): `ground_marks::Clear` en
`Game::LoadMap`. **(aproximado)** `g_game_time_inc` es un entero de ms que se resta tal cual (0x8253B1..0x8253C0); el
paso de openblack es float, así que la fracción espera al fotograma siguiente.

Sin portar: el `SmokyStuff::Create(pos, 1, 1, 0xFFFFFFFF)` de 0x8252EB, porque el modo 1 de SmokyStuff no está
portado (el cráter sigue con el polvo del agarre).

**No es de esta familia.** Las sombras sobre la tierra (`ShadowInfo`, `fn_008745A0` / `fn_00874850` / `fn_00878350`:
vuelven a dibujar la propia tierra; ver [Sombras de los objetos físicos](#sombras-de-los-objetos-físicos)), los
cimientos (`GetAltitudeFondation` 0x63ABC0: un hundimiento rígido), el aplanado bajo el templo (0x882730: edita la
tierra), la niebla (`LH3DMist`, Draw estático) y los sprites planos (bandera 0x40 de `LH3DSprite::Draw`).

**Huecos.**
- (aproximado) Snapshot y Live son iguales en GPU (arriba).
- (aproximado) `LandAltitude` en float frente a la aritmética 16.16 de 0x803090.
- (inferido) El orden de las sumas de la x, z de mundo en `fn_00882B10` (`BakeAgainstY`).
- (inferido) El +0x98 / +0xA0 del fuego, que da el H0 de las llamas, es la posición del objeto.
- Sin portar: la entrada del templo y el templo a medio hacer (0x4676CD, `DrawPartialyBuilt` 0x816AD0), el andamio
  (`Scaffold::SetPhantomMesh` 0x6E8B03), los demás usuarios de `GetExtraPos` 0x80FF20 (vt+0x1CC, sin llamadas
  directas; el único portado es el tótem, que hace la misma suma, (H(p) − H(pos)) + p.y, 0x8100B7..0x8100D0), y la
  cortina, la correa y los quads de D.

## Modos de render y materiales (render_modes)

**Fiel**, salvo lo marcado. En el original toda la mezcla, la prueba de alfa, la escritura de Z y la etapa de textura
de un dibujo salen de **un material de 16 bytes** (`LH3DMaterial`, `LH3DRender::CreateMaterial` 0x82FD30) y **una de
19 funciones de modo** (tabla 0xC38728). En openblack todos esos dibujos piden su estado de bgfx a una sola API,
`openblack::graphics::render_modes` (`src/Graphics/RenderModes.{h,cpp}`); nadie escribe ya la mezcla ni la Z a mano.

**El material.** `CreateMaterial(modo, textura)`: `new(0x10)`, `inc [0xECA654]` (0x82FD40), +0 modo, +4 = 0
(ALPHAREF), +5 = 0 (banderas: bit 0 dos caras, bit 2 repetición, bit 4 sin desplazamiento de UV), +8 textura,
+0xC = 0xFF0000FF (0x82FD59; nadie lo lee, (inferido)). La textura no es del material: smoke.raw la comparten el modo 6
[0xEA1ABC] y el modo 13 [0xEA1AC4] (0x80BC7D / 0x80BCB8); atmos.raw, AtmosMaterial (modo 6, +5 |= 1 | 4: 0x835C58,
0x835C6F..0x835C79) y AdditiveMaterial (modo 13, +5 |= 1: 0x835C61). En la API: `Material` (el modo y las banderas
+5), `State(material, opciones)` (el culling del material si el dibujo no pone otro) y `materials::k_Smoke`,
`k_SmokeAdditive`, `k_Misc0`, `k_Atmos`, `k_AtmosAdditive`, que usan sus ocho dibujos. La textura y la repetición
(+5 bit 2, `SetD3DTillingOn/Off` 0x82FF10 / 0x82FF50 con `g_b_need_tilling` [0xECA614]) las pone cada dibujo; en las
mallas L3D, `Primitive::wrap`.

**El SetMaterial en línea** (unas 105 copias, p. ej. `SetupThing::DrawLine` 0x412662..0x4126BD): llama a
`g_set_render_mode_data[0xECA618][modo].fn(m, etapa 0)`, pone la repetición y `CULLMODE = ((~m[5]) & 1)·2 + 1`
(1 NONE, 3 CCW). En la API: `Select(modo, tabla)` y `CullFor(dosCaras, espejado)`.

**Los 19 modos** (`k_Modes`, tabla 0xC38728, volcada del ejecutable; 2, 6, 9, 10, 11, 16 y 18 releídos):

| Modo | Función | Mezcla | ATEST | ZWRITE | Alfa de la etapa 0 | Tipo L3D |
|---|---|---|---|---|---|---|
| 0 | 0x82D470 | no | no | sí | sin textura | Smooth |
| 1 | 0x82D5C0 | SA/ISA | no | sí | sin textura (no toca la etapa) | SmoothAlpha |
| 2 | 0x82D820 | no | no | sí | textura | Textured |
| 3 | 0x82D920 | SA/ISA | no | sí | textura × difuso | TexturedAlpha |
| 4 | 0x82DC20 | SA/ISA | no | sí | textura | AlphaTextured |
| 5 | 0x82DD90 | SA/ISA | no | sí | textura × difuso | AlphaTexturedAlpha |
| 6 | 0x82DF10 | SA/ISA (0x82DF6A / 0x82DF91) | no | **no** (0x82E063) | textura × difuso | AlphaTexturedAlphaNz |
| 7 | 0x82D6F0 | SA/ISA | no | no | sin textura | SmoothAlphaNz |
| 8 | 0x82DAA0 | SA/ISA | no | no | textura × difuso | TexturedAlphaNz |
| 9 | 0x82E080 | SA/ISA | sí | sí | textura | TexturedChroma |
| 10 | 0x82E830 | **SA/ONE** (0x82E87C) | sí (0x82E88E) | sí | textura × difuso | …AdditiveChroma |
| 11 | 0x82E9C0 | SA/ONE | sí | no (0x82EAAF) | textura × difuso | …AdditiveChromaNz |
| 12 | 0x82EB50 | SA/ONE | no | sí | textura × difuso | …Additive |
| 13 | 0x82ECD0 | SA/ONE | no | no | textura × difuso | …AdditiveNz |
| 14 | 0x82DD90 | = 5 | | | | (la tierra) |
| 15 | 0x82E470 | SA/ISA | sí | sí | textura × difuso | TexturedChromaAlpha |
| 16 | 0x82E6A0 | SA/ISA | sí | no (0x82E78F) | textura × difuso | TexturedChromaAlphaNz |
| 17 | 0x82D820 | = 2 | | | | — |
| 18 | 0x82E2A0 | **ZERO/ONE** (SRCBLEND 5 en 0x82E2F3 y 1 en 0x82E320) | sí | sí | textura | ChromaJustZ |

ALPHAFUNC es GREATEREQUAL para todos (0x82CBA6). Ningún modo toca ZFUNC, la niebla ni el culling. La palabra de la
tabla (1 en los mezclados) no la lee nadie.

**La tabla del alfa global** 0xC387C8 (`k_GlobalAlphaModes`, `Table::GlobalAlpha`): 0, 1 → 1; 2, 3, 17 → 3; 4, 5 → 5;
9 → 15; los demás, igual. La elige el Draw de un objeto con su propio alfa (vt+0x4C = `fn_007F9D80`, bit 7 de obj+4;
0x80DF09). En openblack, los objetos con `components::Alpha` (en la cola si su malla tiene la marca 0x200, si no al
momento) y los átomos de malla translúcidos del PSys (`L3DMeshSubmitDesc::table`).

**ALPHAREF** (`AlphaRef`): el +4 del material, o [0xECA65C] si el interruptor [0xECA658] está puesto. Con la tabla
0xC387C8, los modos 9 y 15 lo escalan: `max(0, ftol(ref·A·(1/255) − 5))` (0x82E15C..0x82E1CE, 0x82E557..0x82E5C9;
[0x900058], [0x8AB6E4], [0x8AA398]), con A el alfa del difuso del objeto [0xC37D8C]. D3D prueba el alfa que **sale de
la etapa 0**: el de la textura en 9 y 18 (SELECTARG1, 0x82E120 / 0x82E384), textura × alfa del objeto en 10, 11, 15 y
16 (MODULATE, 0x82E510 en el 15). `PrimitiveAlpha(modo dibujado, tabla, ref, A)` da a `fs_object` los dos uniformes:
`u_skyAlphaThreshold.y` = ALPHAREF / 255 (−1 sin prueba) y `.w` = el alfa de la etapa (0 ninguno: modos sin mezcla ni
prueba; 1 la textura; 2 textura × difuso). El shader descarta si `round(a·255) < ref`. El modo dibujado es el de
verdad: el de la primitiva por la tabla, o el forzado (`L3DMeshSubmitDesc::mode`), así que un átomo aditivo (13) o la
sombra de la mano (6) no tienen prueba de alfa, como en el original. Las sombras estáticas y de física
(`fs_static_shadow`) usan el mismo `PrimitiveAlpha` con la tabla normal ((inferido)).

**Lo que pone cada dibujo** (`StateOptions`): ZFUNC (LESSEQUAL 0x82CCC5, EQUAL de las sombras sobre objetos 0x80E488,
ALWAYS para dibujar encima), los ZWRITEENABLE 0 escritos a mano (rectángulos 2D 0x81E64C, mar 0x879FD9, tierra
reflejada 0x5E48C5..0x5E4900) y lo propio de openblack: escribir el alfa del destino, MSAA, la formulación
premultiplicada ONE/INVSRCALPHA del modo 6 (humo, sprites, tierra; mismo color), el mar que compone el reflejo en su
shader y el tipo de primitiva. `k_ModelPass` es la pasada de modelos opacos. `ModeFromProperties` es
`GJUtils::SetMaterialProperties` 0x57E120 (4 → 6; sin alfa → 3; aditivo → 13; con Z 6 → 5, 13 → 12, 8 → 3, 16 → 9; sin Z
5 → 6, 12 → 13, 3 / 2 → 8, 9 → 16), la usan `L3DSubMesh` y las partículas.

**Quién la usa.** Las mallas L3D (`L3DSubMesh` y `Renderer::DrawSubMesh`: el tipo de material es el modo); las nubes,
nieblas, humo, barco, lluvia, manchas, peces, anillos, sol, luna, mar, brillo de la mano en el mar, tierra, texto de la
mano, fundido de pantalla, sprites y los sprites, cadenas y superficies de PSys, la sombra de la mano sobre objetos (todas
las primitivas en el modo 6 con ZFUNC EQUAL), los átomos aditivos de PSys (modo 13) y el visor de mallas. Fuera, por
ser técnicas propias de openblack sin modo del original: las pasadas MAX / MIN de sombras y ríos, las huellas y el mod
de follaje.

**Arreglos al unificar** (antes, la tabla de openblack y las ramas de `DrawSubMesh`):
1. Los modos 10 y 11 salían SA/ISA (ahora SA/ONE) y el 11 escribía Z.
2. El modo 16 escribía Z.
3. El modo 18 pintaba color con SA/ISA; ahora ZERO/ONE, solo Z.
4. ALPHAREF: el −5 se aplicaba siempre y sin el factor A/255 (`fs_object`, `fs_static_shadow`); ahora es el +4 exacto
   salvo 9 / 15 con la tabla 0xC387C8.
5. Las primitivas sin Z (6, 7, 8, 16) de un objeto que se funde escribían Z.
6. Las entradas 14 y 17 de la tabla eran {sin mezcla, sin Z}; ahora 14 = 5 y 17 = 2 (latente: no hay L3D con esos tipos).
7. La sombra de la mano sobre objetos ponía SA/ONE en las primitivas aditivas (12, 13); ahora todas van en el modo 6
   del material de la sombra: `fn_0080B050` 0x80B06A..0x80B08B hace el SetMaterial de [si+0x460] (`CreateMaterial(6)`
   en `fn_0087FD50` 0x87FE12) por la tabla actual, y `fn_0084E200` dibuja cada primitiva sin estado propio.
8. Una primitiva chroma con ALPHAREF 0 (o un 9 / 15 que se funde con `ref·A < 1530`, que da 0) mezcla con el alfa de su
   textura (modo 9: SELECTARG1 0x82E120, SA/ISA); antes salía opaca.
9. La sombra de la mano sobre objetos con el culling de su material: +5 = 0 (0x87FE12) da CULLMODE CCW
   (`fn_0080B050` 0x80B0AD..0x80B0E6) en todas las primitivas; antes las de dos caras la recibían también por detrás.
10. La prueba de alfa y el alfa de la etapa 0 son los del modo dibujado (ver ALPHAREF): un 9 que se funde (→ 15) prueba
    textura × A (antes, el alfa crudo de la textura: con ref 0x96 y A = 128 se quedaban los texeles desde 70 en vez de
    desde ~140); un 2 que se funde (→ 3) mezcla con textura × A (antes, solo A); en un átomo aditivo (13) los tipos 0 / 2
    usan textura × difuso (antes, el quad entero) y los chroma pierden la prueba de alfa, igual que bajo la sombra de la
    mano (6).

En `AllMeshes.g3d` no hay primitivas de los tipos 10, 11, 16 ni 18 (los recuentos de [Mezcla de materiales
L3D](#mezcla-de-materiales-l3d)): los arreglos 1-3 se ven en las mallas de los milagros y en las que pasan por
`SetMaterialProperties` (un `TexturedChroma` sin Z sale 16). El 4 se ve en los bordes de todos los chroma (árboles con
corte 0x96: un poco más finos).

**Huecos.**
- (inferido) El alfa del objeto en byte: `AlphaByte` redondea `1 − [0][3]` de la instancia.
- (inferido) La comparación de la prueba de alfa en bytes redondeados (`fs_object`, `fs_static_shadow`).
- (inferido) La luna sin la Z del modo 4 (0x82DC20) y el cielo con los modos de sus mallas.
- (inferido) Los modos de las cadenas (`fn_006AA860`, sin leer) y de la pasada especular de SurfRevol (13).
- (inferido) Los anillos del agua y los bancos de peces con [0xEA1AC4] / [0xEA1AB0]: solo las referencias 0x54BA72 y
  0x8247CC, sin decodificar dentro de la rutina.
- (aproximado) Los peces con ZFUNC ALWAYS: el original deja LESSEQUAL (0x82CCC5); da lo mismo porque la tierra
  reflejada de debajo no escribió Z (0x5E48C5).
- (aproximado) El mar sin culling: su material (+5 = 4, 0x5E5474; `GLandscape::Draw` le pasa [this+4] en 0x5E4E85)
  da CULLMODE CCW (0x879F54..0x879F86), que deja todas las filas del mar del original; la malla del mar de openblack
  no son esas filas.
- (aproximado) Con el alfa de la etapa «textura» (4, 9, 18) `fs_object` lo multiplica aún por el alfa del objeto; en
  el original es el de la textura solo (SELECTARG1). Es lo mismo mientras el objeto tiene alfa 255 (fuera de los
  fundidos, que pasan a 5 / 15, y del corte por el plano).
- Sin portar: el ALPHAREF forzado de sus escritores (árbol que arde `fn_0074B3A0` 0x74B4E0..0x74B51E, que
  `FireGraphic.cpp` llama «render mode 230»; `TownArtifact::Draw` 0x51CBB3; corte fijo de 10 en `fn_005E6350`
  0x5E649C, `Scaffold::Draw` 0x6EA730, `fn_00826470` 0x826488; FragMesh `fn_007F7960`). `AlphaRef` ya lo acepta.
- El fundido de objetos por prueba de alfa (`fn_0080E940` y copias 0x80F0A0 / 0x80F3D0) es otro sistema; openblack hace
  el patrón de cruces de `fs_object`.

## Pendiente

- Luz de los modelos: neblina (`fn_007FEB30`), tintes (veneno, fuego, `fn_0080BF10`), color de ventanas de noche, luz de la mano
  estampada en la tierra (`light_hand.raw`, `fn_008229B0`). Revisar si sigue vigente: la neblina de los modelos ya se
  aplica ([rendering.md](rendering.md#neblina-de-distancia-original-detalle-fog-niveles-36)) y las ventanas de noche y
  la luz de la mano estampada están en [day-night-weather.md](day-night-weather.md).
- Luz de los modelos, copias que faltan por unificar con `model_light`:
  - `FragMesh::BuildMesh` (`src/ECS/Physics/FragMesh.h`): el original la hace por cara y a dos caras por CPU
    (`fn_007F7ED0`, `fistp` 0x7F82A8, `neg` 0x7F82AF, las dos ramas de ambiente 0x7F82B1..0x7F82EC); openblack genera la
    cara de atrás como geometría aparte y la deja al programa de objetos, que ahora sí usa la regla entera y la luz
    compartida. Pasarlo a `model_light::Apply` pide color por vértice en la malla generada.
    **En curso, parado (2026-10-03, sesión «shaders», a petición del usuario):** rama `local/fragmesh-wip`
    (commit `52ef177f`, sobre `ba5e6b64`), compilada pero SIN auditoría, sin tests revisados y sin capturas. Pinta
    cada fotograma como triángulos de mundo los edificios rotos (Abode::Draw 0x5160E9) y los fragmentos
    (Fragment::Draw 0x76EC2A, PhysicsObject::DrawAll 0x646E77): L = normalize(luz − posición) con InverseSquareRoot
    0x841170, luz de tierra fn_00801C90 × tinte (carbonizado / brillo, o 0xFFFFFFFF / 0) y neblina fn_007FEB30 una
    vez por dibujo, un `fistp` I por cara (0x7F82A8), delante con I y detrás con −I (`model_light::TwoSided`),
    especular por vértice como fn_0081C780; test `test_fragmesh_light` (emula 0x7F82AA..0x7F8363). Falta: auditoría
    de suposiciones, revisión, capturas antes/después de fragmentos y edificios rotos, diff a sistemas
    (`WorldTriangles`, `Renderer.cpp`) y fusión.
  - `RendererSurfRevol.cpp`: la malla GJ va sin luz (`UseLighting` sin portar; que esté activa es **(inferido)**).
- Aritmética de LH3DColor, lo que falta por pasar a `lh3d_colour`:
  - tras el reempaquetado de la instancia: (el transporte `u_objectLight` ya pasa por `sea_pass::SeaDraw` y
    `Lh3dUnpackRgb24` en los modos 2 y 4, punto 4 de shaders); el alfa del tinte (el lobo, `SpellFlock.cpp`: (0xFF·a)>>8 con la translucidez decidida con el alfa crudo, 0x51C724;
    el escudo 0xFE); el especular del átomo PSys (DrawData+0xC, leído en 0x67A012 y 0x67A023: falta en `psys::mesh_atoms::Instance` y se pierde en los dos caminos); el
    especular +0x10C de los iconos (con él, el tinte blanco de los iconos de los lugares de culto, 0x519672); que
    `DrawBuilding` no aplica la neblina nunca, arda o no (0x517F90..0x518046 no llama a `fn_007FEB30`: Abode 0x516129,
    MultiMapFixed 0x5180A6, WorshipSite 0x5193E9, SpellIcon 0x519668, Totem 0x51ABC3; el «arreglo 7» de `LandLightOf`);
    `LandLightOf` da a todos los `SpellIcon` {Cell, sin neblina}, pero los de un centro (`TownCentre::Draw`
    0x5164B2 → `fn_0080BEC0`) van con la luz bilineal y neblina; los colores pasados en línea sin portar: el objeto de
    predicción de la física ardiendo (`PhysicsObject::DrawAll` 0x646F81..0x646F8C: blanco + brillo), el FragMesh de la
    casa dañada (`Abode::Draw` 0x5160A6..0x5160E9, por `fn_007F7960`: +0x10 = 0xFFFFFFFF / +0x14 = 0 sin fuego,
    carbonizado / brillo ardiendo), `Object::DrawOutOfMap` (0x51C837..0x51C84F) y `CitadelHeart::DrawNow`
    (0x4670DD..0x4670EE: tinte +0xA4, especular vt 0x5A4); el especular +0xD0 con alfa (ver arriba, `Heal.cpp`);
  - en zonas de otras sesiones: las copias de `src/PSys` (Mist 0x67A6C1, `TintWithPlayerColour` 0x6A865C, Storm
    0x6D2C21, SurfRevol, Heal), `NightLights`, `LandLightTable` y `RendererChain` / `RendererPSys` (`ToAbgr`);
  - `ECS/Fire/FireGraphic.cpp` (dos arreglos exactos): `TreeDrawColour` debe limitar con `ecs::TreeBrightness()`, no
    con 255 (0x74B47B); `CharringGrey` debe ser `255 − ceil(175k/256)` (0x730585..0x7305D7; con k = 255 openblack da
    81 y el original 80);
  - `fn_00809D80` / `fn_00809DE0` (color por parte de malla, 0x80A290 / 0x80A2A6) y `LH3DCreature::DrawNow` no tienen
    aún usuario en openblack; `fn_007ACF70` solo existe en GPU (`fs_object.sc`, en float sin truncar).
- Bancos de peces: el tono de los sonidos, el texto de ayuda ("Pick up") y los pescadores.
- Sombras de los objetos físicos: el filtro 2×2 de los árboles y el rehorneado de la sombra estática al salir un árbol
  o un MobileObject.
- Reflejos y sombras dinámicas de la criatura y de los SuperVillagers (no existen aún en openblack). Sus llamadas de la
  pasada bajo el mar ya tienen sitio, constantes y TODO con dirección (`sea_pass::k_Creature*`, `k_Swimmer*`,
  `Renderer.cpp`): ver [La pasada bajo el mar](#la-pasada-bajo-el-mar-graphicssea_pass).
- Pasada bajo el mar: que milagros2 añada a `psys::mesh_atoms::Instance` el bit `cutByPlane` (`creator->drawCutByPlane`
  en `Mesh.cpp`, el `push_back` de `Collect`), `specular` (DrawData+0xC) y el alfa de DrawData+8, y corrija el
  comentario de `Mesh.h` (el bit vale 4, 0x679F29) y el de `Mesh.cpp` («no plane cuts a static mesh»: falso, R4 / R6);
  entonces la cúpula del escudo y los demás átomos con `DrawCutByPlane` pierden lo que quede bajo y = 0 sin tocar
  `Renderer` ni los shaders. Captura pendiente: la cúpula más metida en el mar (p. ej. `PHYSICAL_SHIELD,1800,3120`;
  la de 1825,3140 queda casi toda sobre la playa). El especular +0x50 del casco del barco (lo que dejó su último Draw,
  **(inferido)** 0).
- Pasada bajo el mar, pruebas con capturas: que los pasos 1-6 no cambian ningún píxel **no está demostrado** con
  capturas (el código sí lo da: mano 0xA0/255, especular 0 del modo 4, mismo culling, `UnmirrorView` = vista·diag(1,
  −1, 1, 1)); dos ejecuciones del mismo exe ya difieren porque el mar y las nubes siguen el reloj real, y algunas
  vistas (costa, luna, roca cortada, red de Land 4) salen algo por encima de ese ruido medido con solo dos ejecuciones.
  Hace falta un gancho de paso de tiempo fijo en los dos exes. Faltan también: el reflejo del casco del barco (la vista
  del plan enseña el arca en tierra, sin mar delante), un BEFORE del tiburón con un exe a b8985c33 (solo hay el de un
  exe anterior) y por qué el primer BEFORE de la roca física en reposo (y = 0,75) salió sin reflejo y los demás sí
  (fallo intermitente de `DrawObjectReflections` anterior al punto 4; y si el reflejo debe ser tan claro, S3, luz ½).
- Humo de las chimeneas y ventanas de noche: `Abode::presentAtHome` ya lo suben y bajan los aldeanos (V4, asistente,
  ba5e6b64, 0x405FA0 / 0x405FB0) y las ventanas lo leen como el original (Abode::Draw 0x515F78: +0xB6 ≠ 0 y luego
  IsVisualNight 0x5575E0); falta la cuenta de
  andamio de los talleres.
- Confirmado por el usuario (2026-10-02): la luna tras V4-a/V4-d, el ancho de las cintas del rayo (semianchura = la
  escala del PSR, fn_0081C780), el aro del orbe que a veces tapa la burbuja según la animación, y que la burbuja ya no
  parpadea al volver a empezar su atlas.
- Billboards:
  - portar los usuarios de `YawToEye` (columnas de influencia, banderas de deseo, ShowNeeds, ScriptHighlight), de
    `PlaneOfMatrix` (fn_008274A0) y los HelpDude (base (R, U, D), HelpDude::Update1 0x5BE302);
  - el oy heredado por el vapor y el humo del fuego;
- Texturas animadas:
  - portar los usuarios que solo tienen reloj (InfluenceCircle, Gooloo, GoldenShower, las correas y la habitación de
    la criatura, HelpDude, el cursor 3D, JCSpecial) y HandGlow / fn_0083F270;
  - el resto de la rama de las fiolas de 0x519AD0 (bote, aplastamientos del switch 0x519D76);
  - en las cadenas, UseDynamicLighting (el suavizado por puntos medios ya está, de milagros2; la interpolación de
    SurfRevol también, `frame_anim::RotatingUvClock`: GameUpdate 0x6C8BC0 entero);
  - HandEffects como efectos PSys de verdad;
- Mallas pegadas al suelo:
  - capturas antes/después del escudo físico, el disco del dispensador, el teletransporte, el arca y el dinosaurio
    de Land 4, la marca de la explosión de rayo y el cráter (escenas en `dev\documentacion\unify\U3_changes.md`);
  - portar la cortina del anillo de influencia (`InfluenceCurtain` ya está), la correa de la criatura (`fn_008491B0`,
    `fn_00848600` / `fn_00848830`) y los quads de la criatura (`fn_0081F360`) cuando tengan casa en openblack;
  - la entrada del templo, el templo a medio hacer, el andamio y los demás usuarios de `GetExtraPos`;
  - el `SmokyStuff` de modo 1 de las marcas del suelo.
- Modos de render: portar el ALPHAREF forzado del árbol que arde y de sus otros escritores; comprobar con el original
  si el aro de piedra del dispensador debe tapar la burbuja que se funde (escena 3 de `dev\documentacion\unify\U4_changes.md`).

- Cola de transparentes: capturas antes y después de las escenas de `dev\documentacion\unify\U6_changes.md` (nubes
  contra modelos y nieblas, lluvia, barco, mano); el reflejo (no leído); `CheckRegionOnScreen` antes de encolar un
  modelo (0x815AB1); las mallas animadas de un `Sorted` (fn_00813340);
  portar los llamadores que faltan (LightSheet, HandGlow fn_0083F100, VillagerName, ValueSpinner, PowerSpin,
  LandscapeVortex, PlayerSymbolSprite, DrawLiquidParticles, fn_006CA930, Gooloo y los dos de clave 0) con `Submit`.

- `world_triangles` (sesión «sistemas» cerrada el 2026-10-03): `RendererSurfRevol.cpp` usa ya el culling del
  material; la luz de los discos (`UseLighting`) y el reparto del especular de fn_0081C780 siguen pendientes
  ([SF_TeleportVortex y ZR_SurfRevol](miracles.md#sf_teleportvortex-y-zr_surfrevol-srcpsysrulessurfrevol-srcgraphicsrenderersurfrevolcpp)).
  La luz a dos caras de FragMesh (rama `local/fragmesh-wip`) toca `WorldTriangles` y `Renderer.cpp`: su diff
  se revisa antes de fusionar (ver arriba).
- Fotos y guiones de la sesión «sistemas» que cita la wiki: `dev\documentacion\unify\shots\` (u7, drawpath, video,
  wtri) y `dev\documentacion\unify\scripts\`; el plan y las notas de U1-U9 en `dev\documentacion\unify\` (las rutas
  `dev\_audit\sistemas\...` que citan esas notas ya no existen).

### Dudas para el usuario (sesión «sistemas», cola de transparentes)

- **Llamas detrás de los árboles** (D7: solo van a la cola las mallas con la marca 0x200, SetMesh 0x7F9E48 /
  AddDrawing 0x815F0B): los árboles se dibujan ahora al momento y con Z, así que las llamas de un árbol de atrás
  quedan tapadas por el follaje de los de delante, y por encima se ven más claras y sin el humo oscuro
  (`dev\documentacion\unify\shots\drawpath\fire_tree_*`). ¿Era así en el original?

### Dudas para el usuario (sesión «shaders», SHADERS_PLAN)

Se implementó todo «como el original» leído en el binario; estas dudas solo dependen de cómo se veía el juego.
Resueltas por el usuario (2026-10-02, capturas del original `img/original_hand_shadow_*.png`): la sombra de la mano es
su silueta gris clara y semitransparente (4/15), el orbe sostenido da una sombra más oscura, los orbes se dibujan
enteros por encima de las sombras (paso S5 aplicado).

- **Luz de los modelos de noche** (`model_light`): con tipo de cielo > 1,5 (double de 0x8C5838) la luz se pone a 3
  unidades de la mano, del lado de la cámara (fn_005E5830 0x5E5A7D..0x5E5B64). ¿Se parece a lo que recuerdas de noche?
- **Pasada bajo el mar** (`sea_pass`): (1) ¿la cúpula del escudo y los efectos de malla del PSys junto al mar se
  cortaban a ras de agua y sin reflejo? (0x679F4A → fn_00858BA0; hecho así); (2) el reflejo de la mano tiene color
  0x65A0A0A0: ¿gris y semitransparente?; (3) el original mezcla el mar sobre el cielo sin espejarlo y openblack refleja
  el cielo: ¿se notaba en el agua lejana?; (4) los objetos morfables (casas, campos, arca, escudo físico) no se reflejan
  (su DrawUnderWater es un `ret`, 0x80BA40; hecho así): ¿lo recuerdas igual?
- **Sombras proyectadas** (`shadow_list`): (1) el barco de los misioneros de Land 1: ¿su sombra iba en diagonal (sol
  fijo) y caía sobre el dique y los marineros? (0x5E11B6 / 0x5E11BE); (2) entre 50 y 80 radios, ¿las sombras de los
  objetos lanzados se aclaraban a saltos o suave? (0x80769A); (3) ¿la sombra de un árbol lanzado era suave y tan oscura
  como la de una roca?; (4) la sombra del orbe sostenido sale algo más oscura que en las capturas (el máximo 8/15 del
  código con alfa 255, **(aproximado)**); (5) la sombra propia del dispensador es la estática (Abode, fn_008721A0), que
  apenas se distingue en las tomas de openblack.
- ~~Para milagros2: el núcleo blanco quemado del orbe~~ **resuelto** por milagros2 (hand-hbn `78c4cfa8`): el PSys de
  soporte de la semilla se dibujaba sin el alfa 0x95 del orbe (DrawSpellGraphic 0x51A252 → SetAlpha 0x55ED50 →
  [0xC0215C], aplicado en fn_00679920 0x679BC2) y la semilla del jugador se ilumina con la celda de tierra
  (0x803340); la burbuja ya era fiel (textura cian × la luz de la hierba). Comparación con las capturas del original:
  `dev\documentacion\audit_magic\orbcolour_compare.png`.

## Ganchos de prueba

En [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración):

- `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fijo en fracción de la ventana (la mano sale en las capturas); usar puntos
  interiores, en el borde mueve la cámara.
- `OPENBLACK_TEST_PHYSICS="x,z,altura,vx,vy,vz[,escala[,n]]"`: rocas que caen, con sus sombras físicas.
- `OPENBLACK_TEST_CUT=1` con `OPENBLACK_TEST_SEA`: corte por el plano del agua.
- `OPENBLACK_HAND_TEST_FISH=1` y `OPENBLACK_TEST_SPLASH="x,z"`: pesca y susto de los peces.
- `OPENBLACK_TEST_CHIMNEY=all`: todas las chimeneas echan humo.
- Billboards: `OPENBLACK_TEST_SEED=FIREBALL` / `LIGHTNING_BOLT` con `OPENBLACK_MOUSE_AT` (sprites de PSys en la mano:
  origen, CentreAtBase y giro); `OPENBLACK_TEST_FIRE` (llamas); `OPENBLACK_TIME_OF_DAY=22` con `OPENBLACK_CAMERA_LOCK`
  (la luna, centrada y en un borde); `OPENBLACK_TEST_ONESHOT` (la burbuja). La lista de escenas está en
  `dev\documentacion\unify\U1_changes.md`.
- Texturas animadas: `OPENBLACK_TEST_ONESHOT="<semilla>,x,z[,pu]"` (la burbuja; con pu, las bandas que miran a la
  cámara; con una fiola de criatura, su hoja 8×4), `OPENBLACK_TEST_DISPENSER`, `OPENBLACK_TEST_FIRE` (llamas),
  `OPENBLACK_HAND_TEST_FISH=1` y `OPENBLACK_TEST_SPLASH` (peces y anillos), `OPENBLACK_TEST_SEED=LIGHTNING_BOLT`
  (cadenas), `OPENBLACK_TIME_OF_DAY=22` (faroles), `OPENBLACK_TEST_WEATHER` (bocanadas de tormenta),
  `OPENBLACK_TEST_CHIMNEY=all` (humo). La lista de escenas está en `dev\documentacion\unify\U2_changes.md`.
- Orden de transparentes y burbuja: `OPENBLACK_ORB_TRACE=1` escribe por fotograma el sitio y la clave de cada disco
  `ZR_SurfRevol` y de cada burbuja de bola de un uso en la lista ordenada, con la fase, el fotograma, el `[1][3]`
  empaquetado, el alfa y el recorte de cada burbuja (ver
  [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)). Escena: un dispensador con
  `OPENBLACK_TEST_DISPENSER` y la cámara fija con `OPENBLACK_CAMERA_LOCK`.
- Cola de transparentes: `OPENBLACK_ZSORTER_TRACE=1` escribe una vez por segundo cuántas entradas tiene la cola de
  cada clase (modelos, nubes, casillas de lluvia, sprites del barco, efectos, superficies, cintas, nieblas, humo,
  sprites, mano), las perdidas por el tope y las claves extremas. Escenas: el cielo de Land 1 con
  `OPENBLACK_CLOUD_SEED=7`, una tormenta con `OPENBLACK_TEST_WEATHER="x,z,60"`, el barco con `OPENBLACK_BOAT_TRACE=1`
  y la mano con `OPENBLACK_MOUSE_AT` (lista en `dev\documentacion\unify\U6_changes.md`).
- Mallas pegadas al suelo: `OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,x,z,..."` con `OPENBLACK_TEST_SHIELD_SHOT` (el escudo
  físico se funde con la tierra), `OPENBLACK_TEST_DISPENSER` y `OPENBLACK_TEST_TELEPORT` (los discos cortados),
  `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` y `OPENBLACK_TEST_EXPLOSION_SHOT` (la marca del suelo; `UR_Explosion` solo
  está en `SF_BeamExplosion*`), `OPENBLACK_TEST_TUG`
  (el cráter); `OPENBLACK_SPELL_TRACE=1` escribe `Explosion: ground mark`. La lista de escenas está en
  `dev\documentacion\unify\U3_changes.md`.
- Modos de render: el test `test_render_modes` (las tablas, los estados de cada sitio antes y después y los arreglos);
  escenas con `OPENBLACK_HAND_TEST_TREE` y `OPENBLACK_CAMERA_LOCK` (bordes chroma), `OPENBLACK_TEST_ONESHOT` y
  `OPENBLACK_TEST_DISPENSER` (fundidos y aditivos), `OPENBLACK_MOUSE_AT` (sombra de la mano sobre objetos). La lista
  está en `dev\documentacion\unify\U4_changes.md`.

## Fuentes

- `dev\documentacion\render\`: `misc_*` (manchas, reflejos y LOD, con emulación Unicorn de `fn_0081FFF0`),
  `objshadow_notes.txt`, `cut_notes.txt`, `physshadow\`, `l3d_wrap_scan.py`, `animal_notes.txt`, `objlight_*`.
- `dev\documentacion\fish\fish_notes.txt` (susto y pesca).
- `dev\documentacion\aldeanos\smoke.md` y `smoke\` (humo de las chimeneas).
- `dev\documentacion\unify\billboard_original.md` (los modos del original, con su verificación), `billboard_openblack.md`
  (inventario de openblack) y `U1_changes.md` (la migración).
- `dev\documentacion\unify\animtex_original.md` (los relojes del original, con su verificación), `animtex_openblack.md`
  (inventario de openblack) y `U2_changes.md` (la migración).
- `dev\documentacion\unify2\lh3d_zsorter_original.md` (la cola del original, con su verificación),
  `lh3d_zsorter_openblack.md` (inventario de openblack) y `dev\documentacion\unify\U6_changes.md` (la migración).
- `dev\documentacion\unify\drape_original.md` (los algoritmos del original, con su verificación), `drape_openblack.md`
  (inventario de openblack) y `U3_changes.md` (la migración); `dev\documentacion\morph\morph_notes.txt` (UpdateMelting).
- `dev\documentacion\unify2\shader_lh3dcolour_instance_original.md` (las rutinas, con su verificación),
  `shader_lh3dcolour_instance_openblack.md` (inventario de openblack) y `SHADERS_PLAN.md` §3 (aritmética de LH3DColor).
- `dev\documentacion\unify2\lh3d_render_modes_original.md` (el original, con su verificación), `lh3d_render_modes_openblack.md`
  (inventario de openblack) y `dev\documentacion\unify\U4_changes.md` (la migración).
- `dev\documentacion\unify2\shader_sea_reflection_pass_original.md` (las tres rutas del original, con su verificación),
  `shader_sea_reflection_pass_openblack.md` (inventario de openblack) y `PLAN_4_sea_pass.md` (las reglas R1-R14
  comprobadas otra vez y la migración a `sea_pass`).
