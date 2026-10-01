# Render del mundo: original frente a openblack

El dibujo del mundo en el original y en openblack: estados de Direct3D, terreno y small bump, mar y costa, tabla de luz,
neblina, cámara, sombras sobre el terreno, cielo (sol, luna, nubes), ríos, fundido de pantalla, texto y la niebla del
mapa. Los modelos (materiales, luz, reflejos, sombras de objetos, sprites, humo) están en
[rendering-objects.md](rendering-objects.md); el agua como juego, en [water.md](water.md); el estado de cada etapa,
en [parity.md](parity.md).

- [Estados de Direct3D 7 del original](#estados-de-direct3d-7-del-original)
- [Mods gráficos](#mods-gráficos)
- [Detalle del terreno ("small bump")](#detalle-del-terreno-small-bump)
- [Mar](#mar-skyraw--skyaraw)
- [Costa](#costa)
- [Tabla de luz del terreno](#tabla-de-luz-del-terreno-0xedd90c)
- [Neblina de distancia](#neblina-de-distancia-original-detalle-fog-niveles-36)
- [Cámara](#cámara)
- [Sombras (tres sistemas del original)](#sombras-tres-sistemas-del-original)
- [Cielo: sol, luna y nubes](#cielo-sol-luna-y-nubes-original)
- [Ríos](#ríos)
- [Fundido de pantalla y bandas de cine](#fundido-de-pantalla-y-bandas-de-cine)
- [Texto: fuentes del original y el mensaje de la mano](#texto-fuentes-del-original-y-el-mensaje-de-la-mano)
- [Partículas (PSys)](#partículas-psys)
- [Niebla del mapa (LH3DMist)](#niebla-del-mapa-lh3dmist-fn_007fa300)
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

Estado: todo lo de esta página es **fiel** (original, verificado en el ejecutable) salvo lo que se marca
**(aproximado)** o **(inferido)**, las desviaciones que se dicen en cada sección y lo que está en
[Pendiente](#pendiente).

## Estados de Direct3D 7 del original

`LH3DRender` pone los estados con dos envoltorios, `LH3DRender::SetRenderState` (0x412940) y
`LH3DRender::SetTextureStageState` (0x82B9C0), y también con llamadas directas a la vtable de
`IDirect3DDevice7`: `+0x50` es SetRenderState y `+0x94` es SetTextureStageState. Para listarlos todos con sus
constantes, usa `tmp_dis\render\scan_states.py` (envoltorios) y `scan_vt.py` (vtable).

- **Filtrado**: en la configuración por defecto (`0x82CA40`, dentro de `fn_0082C8F0`, junto a `OpenD3D`),
  MINFILTER y MAGFILTER = 2 (**lineal**) en las etapas 0 y 1. `SetupThing` (0x4133A2… y `DrawBevBox` 0x413C20),
  en el frontend 2D, cambia a 1 (punto) mientras dibuja y después lo restaura.
- **No hay mipmaps**: nunca se pone `D3DTSS_MIPFILTER` (el valor por defecto de D3D7 es NONE) y las texturas DDS
  de `AllMeshes.g3d` solo tienen un nivel.
- **No hay antialiasing**: no aparecen ni `D3DRENDERSTATE_ANTIALIAS` ni `EDGEANTIALIAS`, ni tampoco
  `MAXANISOTROPY`.
- Dither activado (`DITHERENABLE`), que era para los modos de 16 bits.
- **Sin filtro cúbico** (revisado a petición del usuario): todas las escrituras de MAGFILTER (caché 0xEC8230/0xEC8630;
  `fn_0082C8F0` 0x82CADB, `fn_00836E20` 0x836FA4, `HelpDude::Feel` 0x5B9CAF, `SetupThing::DrawTab`/`DrawBevBox`) usan
  1 (punto) o 2 (lineal); nunca 3/4 (D3DTFG_FLATCUBIC / GAUSSIANCUBIC). No hay opción de filtrado en el registro.
  `d3dim.dll` / `d3dim700.dll` de la instalación son los "resolution limit remover" de UCyborg (2016), no tocan el
  filtrado. Un aspecto más suave en el original vendría del controlador o de la capa de compatibilidad de Windows.
- Cut-outs: los materiales `TexturedChroma` (árboles, vallas) usan umbral 0x96 (0,588); el pino tiene
  `MSH_T_CONIFER` 577 y `MSH_T_PINE` 590. Su piel (BGRA4) tiene ~94,5 % de texels con alfa 0 o 15 y el resto
  intermedio.

openblack ya coincide por defecto: bilineal, sin mips y sin MSAA.

## Mods gráficos

Los mods gráficos (`graphics.msaa`, `graphics.mipmaps`, `graphics.anisotropic`, `--enhanced-graphics`, `water.living`;
desactivados por defecto) están, con su implementación y cómo verificarlos, en
[mod-library.md](mod-library.md#catálogo-de-mods).

## Detalle del terreno ("small bump")

Informe completo del desensamblado; direcciones W120:
- `fn_00804830` carga `.\data\Textures\smallbump.raw` con el flag de alfa (0x41). Al primer uso `fn_00837400` lo
  empaqueta en ARGB4444 y mete `smallbumpa.raw` en el nibble de alfa (`"a.raw"`, 0x8375C1). Material modo 0xE.
- Modo 0xE = `fn_0082DD90` (tabla de modos 0xC38728, entradas {fn, flag} de 8 bytes): `ALPHABLENDENABLE`,
  `SRCALPHA/INVSRCALPHA`, etapa 0 `TEXTURE*DIFFUSE` en color y alfa.
- `fn_007A1800` (dibujo de bloques): segunda pasada por bloque, sin iluminar, UV × 12 por bloque (una repetición cada
  13,33 unidades). No se hace en bloques con material de superposición.
- **Difuso de vértice de esa pasada** (x87 `fn_00874AA0` 0x8758E7–0x87598B, SSE `fn_007A1800` 0x7A2FD0–0x7A3136; las
  dos rutas iguales), con d la distancia con signo a la línea de fundido y e = 20:
  - d ≥ e → `especular & 0xFF000000 | 0xFFFFFF`: blanco con alfa 255, o **alfa 0 si la altitud del vértice es ≤ 1**
    (el alfa especular, ver [Costa](#costa));
  - −e < d < e → `fistp(255 − (e − d)·255/40) << 24 | 0xFFFFFF` si el alfa especular no es 0, si no 0 (negro, alfa 0);
  - d ≤ −e → 0.
  Máscaras de la ruta SSE: 0xFC01B0 = 0xFF000000, 0xFC01C0 = 0x00FFFFFF, comparada con 0 (`[esp+0x460]`, 0x7A1C3B).
  Así el detalle **se funde (Gouraud) hacia cada vértice de altitud ≤ 1**: no acaba en un borde recto donde el agua
  somera toca las celdas de mar abierto. Omitir los triángulos con los 3 vértices a 0 (0x7A31A0) no cambia nada.
- Fundido (`fn_007FEE60`; constantes de `fn_007FE7B0`: 50 = 0x42480000, rampa 40 = 0x42200000, e = 20 = 0x41A00000):
  línea a 50 unidades delante de la cámara sobre el plano y = min(cam.y, 0,67·165); pleno hasta 20 unidades antes,
  nada 20 después. Depende de la altura y del ángulo de la cámara (no 200 fijo): **al acercar o alejar la cámara la
  zona con detalle crece o mengua, también sobre la orilla, igual que en el original**.
- Resultado: `col = mix(col_iluminado, smallbump.rgb · c, smallbumpa · a)` con (c, a) el difuso de arriba → motas
  **claras**, también de noche. openblack hacía `col *= 1 − smallbumpa` (manchas oscuras) con solo el alfa y UV × 10.
- Hasta la auditoría de la orilla (2026-10-01) openblack daba alfa = fundido en todo triángulo con algún vértice de
  altitud > 1, también en sus vértices de la orilla: una película verdosa semitransparente sobre el agua somera cerca de
  la cámara, cortada en las aristas de los triángulos (rectas y diagonales) y que cambiaba con el zoom. Ahora
  `vs_terrain` calcula (c, a) por vértice como el original (`v_smallBumpFade`, vec2). Capturas antes/después:
  `_audit\agua\shore_ab_near_crop.png`, `before_top.png` / `after_top.png`, `before_refl2.png` / `after_refl2.png`.
- `SetUseSmallBump` (0x87FD30) solo cambia `[0xC37210]` (registro "UseSmallBump", niveles de detalle).
- `S_TileLandscape.raw` no es del terreno: es la entrada 6 de la tabla de texturas globales 0xBEF484, usada por el
  corazón de la ciudadela (`fn_00465C70`) **(inferido)**. `L_Smallbump_01.raw` no se usa.
- En esta instalación `smallbump.raw` (2017), `smallbumpa.raw`, `Sky.raw` y `S_TileLandscape*.raw` (2021) vienen de un
  pack de texturas, no son de 2001. El usuario recuerda (2026-10-01) que el moteado sobre el agua somera puede
  venir de ese `smallbump.raw`: con el original de 2001 se vería distinto, la regla de dibujo es la misma.
- openblack: `LandIsland::CreateSmallBumpTexture` (RGBA con cuantización a 4 bits), fundido por vértice en
  `vs_terrain` (`u_smallBumpLine`, `u_skyAndBump.w`), mezcla en `fs_terrain`.

## Mar (`sky.raw` + `skya.raw`)

Del desensamblado (W120):
- `GLandscape::Open` 0x5E5432 carga `.\Data\Textures\sky.raw` (flag de alfa; `skya.raw` es el alfa, 4 bits) con
  material modo 5 (= `fn_0082DD90`, igual que el 0xE: `SRCALPHA/INVSRCALPHA`, `TEXTURE*DIFFUSE`).
- `GLandscape::Draw`: cielo (con la luna reflejada) → tierra reflejada (`LandRef`, alturas × −1, tabla de luz × 0,5,
  `fn_007FF4F0`) → partes bajo el agua (mano, objetos, peces…) → brillo de la mano (0x5E4D89) → mar
  (`fn_00879930`) → tierra. El mar se salta solo si `[0xECA670]` (alambre, depuración) o `[0xECA664]` ≠ 0; este lo pone
  `PSysLightMaps` (0x6CA57E) a `clamp(nivel,0,1)·190` y el nivel solo sube/baja al acabar una cinemática
  (`CameraModePath::Cleanup`): **en juego normal el mar se dibuja**.
- Tiras de filas de 2 px en pantalla, `ZFUNC ALWAYS`, sin escribir Z. UV = (mundo + desplazamiento) / P,
  P = 2000 − 1800·WaterTiling (560 en el detalle por defecto, 200 en el máximo). Desplazamiento con el viento ambiente
  (−1/330 por ms, aplicado dos veces por fotograma), que en partida vale 0.
  Ondulación: cada fila se mueve 0,9·sin(i·π/8) a lo largo del frente horizontal de la cámara,
  i = (fotograma + 2·(fila+1)) & 15, pleno a más de 70 de profundidad, nada a menos de 30.
- Color del vértice = entrada 255 de la tabla de luz del terreno (luz plena de la hora del día); alfa = 255 hasta
  profundidad 7000, baja lineal a 80 en 14000 (0xC39908). Un solo color para todo el mar: **el mar no recibe sombras
  de nubes** (solo la tierra reflejada que se ve a través).

En openblack (`fs_water`/`vs_water`, `Graphics/RendererSea.cpp`, `Graphics/SeaRows`): **hecho como el original**
(W7).
- **Zona del mar** (`fn_00879500`, `sea::ComputeScreenRange`): quad de 30000×30000 en y = 0 centrado en (2560, 2560)
  (esquinas −12440 / 17560), recortado con el plano cercano y los cuatro lados (sin plano lejano). `top` = y mínima y
  `bot` = y máxima de pantalla, con su 1/z; `top` se sube a 0 y `bot` se baja a alto − 1 sin corregir su 1/z. Rareza
  copiada: el primer vértice solo puede fijar `bot` (`if y > bot … else if y < top`). El recorte es el del original:
  vértices 0..3 = (−12440, −12440), (17560, −12440), (17560, 17560), (−12440, 17560) y triángulos (0, 2, 1), (0, 3, 2)
  (0x879537); cada uno pasa por el recortador recursivo `fn_0081A760` (x87) / `fn_007A3A50` (SSE, P4), un plano por
  bit de 0x20 (cercano) a 0x02, dos vértices nuevos por corte al final de la tabla y, si quedan dos triángulos, el
  primero recortado por una llamada recursiva; los vértices nuevos llevan solo los códigos de los planos siguientes
  y su y de pantalla se pinza a 0..alto−1 (`g_MaxScreen`, 0x81E130). `bot`/`top` se leen en el orden de esa lista.
  Comprobado con una emulación Unicorn de `fn_00879500` (`tmp_dis\agua\re\emu_sea_range.py`): las dos rutas dan la
  misma lista y la misma y (±0,05 px de redondeo) en 400 cámaras al azar; `test_sea_rows` lleva dos casos de la
  emulación. En 30000 cámaras sobre la isla la rareza de "solo `bot`" nunca movió `top` más de 1 px.
  **Fallo del original, copiado**: para decidir si hace falta recortar suma los códigos de 0, 1, 2 y de la entrada
  **4** de la tabla (`[0xE3B5F0]`, en vez de la 3 en 0xE3B5EC), que es un resto del último dibujo por LH3DP3. Si esa
  entrada y los códigos 0..2 son 0 con el vértice 3 fuera, dibuja sin recortar con la x', y', z' de cámara del
  vértice 3 (un `top` basura). Hace falta ver las esquinas 0, 1 y 2 a la vez: no pasa en ninguna de 200000 cámaras
  del disco de la isla (radio 5120, altura 3..4000), así que se toma la entrada 4 = 0. Plano cercano: `[0xE839E0]`,
  que `GCamera::Update` 0x4424AF pone cada fotograma con `LandFeature::GetNearClipping` 0x5E2F30 = 0,3 + 0,16·h
  (h = cámara − `GetAltitude`; 0,3 si h ≤ 0, 3,5 si h > 20; 0,1 con `SET_GRAPHICS_CLIPPING` [0xD1A2F8]): es el
  `cameraNearClip` que openblack ya recalcula en `Game` y el que usa el mar.
- **Filas** (`fn_00879930`): filas de vértices r = 0..n en y = ftol(top) + 2r, n = (ftol(bot) − ftol(top) + 2)/2;
  1/z de la fila = 1/z(top) + r·(1/z(bot) − 1/z(top))/(n − 1). `vs_water` dibuja un quad de pantalla completa y
  `fs_water` rehace cada fila por píxel: punto del plano bajo la fila, ondulación en las filas pares (entera si
  1/z < 1/70, (1/z − 30)·0,025 si < 1/30), las filas impares repiten la UV de la anterior (2 px iguales y 2 px
  interpolados, sin corrección de perspectiva), alfa por fila redondeado y **fila 0 con alfa 0x20** si ftol(top) ≥ 1.
  Se cuenta desde `top` y con `u_viewRect`, así no depende de la API.
- Fuera de las filas y bajo el horizonte (entre el borde lejano del quad y el horizonte, o sin mar en pantalla) se ve
  lo que hay bajo el mar sin mezclar: la **franja de cielo espejado** sobre el borde del mar (unos 25 px a 864 de alto
  con la cámara a 300 de altura). Sobre el horizonte, el cielo de la vista principal.
- **Fotograma** `[0xFA938C]`: sube 1 (& 15) por fotograma dibujado solo si el mar está en pantalla y el juego corre
  (`g_game_time_inc` ≠ 0): **en pausa las líneas del mar no se mueven** (capturas en pausa idénticas). A fps modernos
  tiembla más rápido que a ~30 fps, como haría el original a esa velocidad.
- **Deriva con el viento** (`sea::Drift`): off += viento·ms·(−1/330), dos veces (0x879963 siempre, 0x879A69 si hay mar
  en pantalla), envuelta a P; en el nivel 0, off0 += viento·ms·(−1/330000) envuelta a (−1, 1). El viento ambiente vale
  0 (`sea::k_AmbientWind`), así que no se mueve: el código está para el día que no valga 0.
- **Nivel de detalle 0** (`fn_0087A090`, WaterTiling = 0): quad del mundo de ±70000, UV (x + 70000)/2800 y
  (70000 − z)/2800 (la V crece hacia −z) más su deriva, alfa de vértice 255, sin ondulación ni fundido.
- **Textura en 4 bits** (`fn_00837400`): al cargar, `sky.raw` y `skya.raw` se quedan con `v >> 4` (n/15, como
  ARGB4444 en D3D). El color de `sky.raw` tiene una desviación de 5-8 sobre 255, así que quedan 4-6 niveles por canal y
  de cerca se ven manchas planas. El mod `graphics.terrain-x2` (reescalado) sigue en 8 bits.
- **Lo que hay bajo el mar** es el objetivo de reflejo, ahora **del tamaño de la vista principal** (se recrea al cambiar
  de tamaño; antes era de 1024²) y pintado **en orden** (vista secuencial, como `GLandscape::Draw` 0x5E48AE–0x5E4E6B):
  cielo espejado, luna reflejada, tierra reflejada (`fn_007FF4F0`: media luz, **sin small bump** porque
  `[0xC37210]` = 0, sin sombras dinámicas; las de nubes sí; **sin escribir Z**: `GLandscape::Draw` 0x5E48C5–0x5E4900
  pone ZWRITEENABLE (14) a 0 antes de `fn_007FF4F0` y a 1 después, así que no tapa nada de lo que viene detrás y,
  entre bloques, gana el último dibujado: el más lejano, ver [Costa](#costa)), mano y objetos bajo el agua, bancos de
  peces y el **brillo de la mano**. openblack la dibujaba escribiendo Z (sus celdas planas en y = 0, aunque fueran
  transparentes, y los montes espejados tapaban los reflejos que venían detrás); desde 2026-10-01 ya no. Sin modelos ni sprites (salvo el mod `water.living`). `fs_water` mezcla `luz·sky` sobre él con
  alfa `skya·alfa de fila`, igual que SRCALPHA/INVSRCALPHA sobre el fotograma.
- Mar con ZFUNC ALWAYS, sin escribir Z y sin culling; la vista principal es secuencial y la tierra que va después lo
  tapa.
- Gancho `OPENBLACK_SEA_TRACE=1` (cada 500 fotogramas: primera fila, n, 1/z, paso, fila suave, fotograma, deriva).
  Test: `test_sea_rows`.

## Costa

**Fiel** (hecha). Informe: `tmp_dis\agua\sea_render.md` §4. Las rutas normal y SSE hacen lo mismo. El intento anterior (aplanar y omitir
celdas sin más) dio "fondo de arena en escalones" porque le faltaba la pieza que da la forma de la orilla: el **alfa
de la textura de bloque**.
- **Alfa costero por texel** (`fn_008732C0`, SSE `fn_007AB4B0`, 16×16 texels por celda, 256 por bloque): altitud
  pesada con conos `h = Σ w_k·alt_k` (`fn_00871560`: `w_k = max(0, 14 − d_k)` a las 4 esquinas, truncados a Σ = 255
  y +1 a la mayor), `h < 0x100` → 0, `h ≥ 0x400` → 15, en medio `e = h + 4·(int8)ruido` (ruido del LND,
  `[x·256 + z]`, el mismo en todos los bloques) e `idx = 15 + trunc((15e − 14250)/438)` sobre la tabla 0xC39814 =
  0xC2AAC0 (16 dwords ya desplazados 12 bits: nibbles 0,0,0,1,2,4,6,8,9,10,10,11,12,13,14,15). Transparente por
  debajo de una altitud de ~1,2–2, opaco hacia ~2,9–3,7; el ruido dibuja la orilla.
- **Celdas de mar abierto** (bit 0x02 del byte de `flags`, `word(celda+6) & 0x200`): no se emiten sus triángulos
  (0x875DDC, 0x876A8A **y** SSE 0x7A9B14, 0x7AAB14) y el builder pone a 0 sus texels (0x8739F8). 3297 celdas en Land1.
  El bit 0x02 **no** es parte del código de sonido: los bits 2..5 del byte son el tipo ATMOS.
- **Aplanado de la malla**: `y = alt ≤ 3 ? 0 : alt·0,67` en cada vértice (0x874B95, SSE 0x7A1EE7, `cmpleps` contra 3,0
  en 0xFC01F0). El juego (`GetAltitude` 0x803121) solo aplana junto a una esquina base ≤ 4; en Land1 no hay ninguna
  celda donde las dos reglas den alturas distintas (base > 4 con una esquina ≤ 3: 0 de 25600).
- **Especular**: `dword de la celda | 0xFF000000`; su alfa es 0 si UseSmallBump y alt ≤ 1 (0x874BA9, SSE 0x7A1F16).
  La pasada de small bump lo copia a su difuso (alfa 0 en esos vértices, ver
  [Detalle del terreno](#detalle-del-terreno-small-bump)) y omite los triángulos con los 3 vértices así (SSE 0x7A31A0).
  El small bump **no** se modula con el alfa costero, pero se funde a 0 hacia los vértices de altitud ≤ 1: sobre el agua
  somera sus motas solo salen junto a vértices más altos, sin borde.
- **Ningún borde duro junto a las celdas de mar abierto**: en las 6 tierras, las celdas 0x02 tienen las 4 esquinas a
  altitud 0 y el alfa costero de las celdas dibujadas vecinas vale 0 en todos los texels de la arista común
  (`tmp_dis\agua\shore\edge_stats.py`: Land1 871 aristas, Land2 1722, Land3 900, Land4 1283, Land5 1808, LandT 556,
  todas con nibble máximo 0). Un borde recto en openblack no puede venir del alfa costero.
- **Sombras dinámicas** (`fn_00878350`): color de vértice 0 donde el byte de altitud ≤ 1 → se funden hacia el agua
  (interpolado), sin corte duro.
- La tierra se dibuja en modo 14 (SRCALPHA/INVSRCALPHA) sobre el mar ya pintado y **escribe Z aunque el alfa sea 0**
  (`fn_0082DD90` pone ALPHATESTENABLE = 0 en 0x82DE2A y no toca ZWRITEENABLE). Consecuencia, en el original y en
  openblack: lo que esté **bajo y = 0** y se dibuje después con su Draw normal (un árbol u otro objeto físico que se
  hunde antes de borrarse a −4R, ver [water.md](water.md#hundirse-ahogarse-y-borrarse)) queda tapado en todas las
  celdas dibujadas y se ve entero, como si flotara, sobre las celdas 0x02, que no escriben Z: sale **cortado en
  rectángulos** por el borde de las celdas de mar abierto (`_audit\agua\after_treecut.png`).
- **Orden de los bloques** (tierra y tierra reflejada): la lista de `LH3DIsland::PreDraw` (0x7FF45F–0x7FF4DD),
  ascendente por `block+0x9BC` = distancia de la cámara al centro (x + 80, 0, z + 80) del bloque con LandRef
  (`fn_00877210` 0x87722C–0x877296, 0x877C8A–0x877CCD): **el más cercano primero**; `fn_007FF610` y `fn_007FF4F0`
  recorren la misma lista (+0x9B8). openblack los ordena igual en `Renderer::DrawPass`. Sin Z en la tierra reflejada,
  ese orden decide qué monte espejado tapa a cuál.
- openblack: `3D/CoastAlpha` (port de `tmp_dis\agua\sea_coast_alpha.py`, idéntico texel a texel en Land1),
  hoy el canal alfa de la RGBA8 `BlockTexture` de `LandIsland` (ver abajo; filtro lineal, filas a lo largo de +z; se
  rehace en `RebuildAltitudes`);
  `LandBlock` aplana, colapsa a un punto las celdas 0x02 (la forma física de Bullet las conserva) y pasa por vértice
  el "fundido de orilla" (alt > 1); `fs_terrain`: `a = min(costa, LandAlpha)` (los ríos siguen con `min`), sin
  `discard`, las dos pasadas como un color premultiplicado (mezcla ONE/INV_SRC_ALPHA: `(tierra+esp)·a·(1−b) +
  (bump+esp)·b`) y las sombras multiplican también lo que se ve detrás. `GetDrawnHeightAt` = altura de la malla
  dibujada (la vegetación de los mods se apoya en ella, pero elige sus plantas con la altura sin aplanar como antes).
  Diferencias: una sola textura para la isla (el filtro bilineal cruza los bordes de bloque; el original tenía una
  textura por bloque).
- **Pesos de cono, desempate**: el +1 va a la mayor y, entre iguales, a la **última** (w3 antes que w0): la tabla que
  construye `fn_00871560` en 0xE3A3E0, leída de una ejecución Unicorn, da p. ej. en el texel (8, 8) 63, 63, 63, 66.
  31 de los 256 texels (las líneas i = 8 y j = 8) cambian respecto a la regla anterior (el alfa costero apenas).
- **Color de la textura de bloque (hecho)**: `fn_00873790` por celda y `fn_008732C0` por texel, con el índice
  `(x·256 + z)` del bloque para el material, el ruido y el bump (`[0xFA7698]`, el bump del LND):
  - `h < 0x100` → texel 0. Si no, entrada `mat = min((h >> 8) + ruido, 255)` de la tabla del país (`[0xFA75C0 +
    4·país]`, país = nibble bajo del byte 6 de la celda): {índice 0, índice 1, coef}. Color de 5 bits de las texturas
    de material (`[0xFA74F8 + 4·i]`, B5G5R5 tras el u16 de tipo): `c0·coef + c1·(256 − coef)` (el **primero** pesa el
    coeficiente; solo c0 si son iguales), × bump y a 4 bits: R = (Σ(c & 0x7C00)·bump) >> 18, G >> 17, B >> 16, cada
    uno pinzado a 15 (0x873438..0x8734EA); alfa el nibble costero.
  - Si las 4 esquinas de la celda no son del mismo país, se construye una vez por país de esquina y `fn_00871850`
    mezcla por canal de 4 bits (alfa incluido) con los pesos de cono: `floor(Σ canal_k·w_k / 255)`.
  - Comprobado: la emulación Unicorn de `fn_00873790` (`tmp_dis\agua\re\emu_block_texel.py`) coincide con la
    referencia en los 163840 texels de 40 filas de celdas de Land1 por la ruta x87; la SSE (P4, `fn_007AB4B0` con
    `pmulhuw`) baja en 1 el nibble azul (a veces el verde) en ~1,8 % de los texels. Se sigue la x87. La textura que
    genera openblack (`OPENBLACK_DUMP_BLOCK_TEXTURE`) es idéntica a la referencia en 524288 texels comparados
    (`tmp_dis\agua\re\cmp_block_dump.py`).
  - openblack: `3D/BlockTexture` (`CountryTexel`, `BlendCorners`, `BuildIslandBlockTexture`); `LandIsland` guarda las
    texturas de material y el bump en CPU y sube una RGBA8 `BlockTexture` (nibble × 17, color y alfa costero) que
    sustituye a la R8 `CoastAlpha`; `fs_terrain` toma de ella el color (ya lleva el bump) y el alfa; encima van las
    huellas, las sombras estáticas y la luz como antes. Los mods de terreno (`terrain-x2`, acantilados triplanares)
    siguen con los materiales por vértice, ahora con la entrada `min((255·alt >> 8) + ruido, 255)` (el texel de la
    esquina) y el coeficiente del lado bueno (antes estaba al revés: `mix(id0, id1, coef)`). Esto cierra la duda de
    tooling.md: ni el `min(alt + ruido/4)` del editor ni el `(alt + ruido) % 256` de openblack.

## Tabla de luz del terreno (0xEDD90C)

- `fn_00869850`, cada fotograma desde `GLandAlignement::DrawSky`: 256 colores indexados por la luminosidad de la
  celda, a partir de `Data\WeatherSystem\palette.raw` (32×32 RGBA, una para todas las islas). Filas 0..2: color de
  la tierra buena/neutral/mala según la hora (columna = (2 − Time2SkyType)·15: 0 medianoche, 15 ocaso, 30 mediodía);
  filas 3..7 por alineación (columna = X·15, X = 1 − alineación: 0 bueno, 2 malo).
- base = lerp de las filas 0/1/2 por X (con t = −1 exacto en neutral, fallo del original que se conserva), limitada a
  255 − 96·tiempo nublado. Entradas 48..255 = `(c3·(255−i) + base·i) / 200` (`fn_00869790`; LightBoost del registro,
  0 por defecto) con tope 255; 0..47 otra rampa (la tierra de Land1 usa 48..255). Relámpago: lerp a blanco.
- Uso: difuso del vértice de tierra = tabla[luminosidad]; tierra reflejada × 0,5; mar = tabla[255].
- Valores a mediodía neutral: [48] 686d66, [128] 9eab9f, [218] daf0e0, [255] f3fffb (casi blanco); medianoche
  [255] 3d5b6c; ocaso d68e79; malo a mediodía dbc8ff.
- openblack: `3D/LandLightTable` (port exacto, comprobado contra `tmp_dis\render\light_lut.py`), textura 256×1 que
  `vs_terrain` muestrea por vértice; `u_seaColour` para el mar. `Build(skyType, alineación, nublado, destello)`:
  tope `min(c, ftol(255 − 96·nublado))` sin recortar el nublado (0x869ADB), destello `c += ((0xFF − c)·f) >> 8` con alfa
  0xFF en toda la tabla (0x869C25) y la neblina de tormenta/relámpago; `LandLightTable::Current().GetRaw(i)` (copia
  del último `Build`) es la tabla global 0xEDD90C que leen los creadores de anillos (`ECS/WaterRings`); el
  renderizador usa `GetRaw` de su propia tabla; `Current()` guarda también la base [0xFA26A4] (`GetRawBase`) y la
  neblina, que leen las nieblas del PSys y las nubes de las tormentas. Nublado = `[0xFA2754]` (`GCamera::Update` 0x4426BA: byte 3 de
  `GetWeatherSmooth` en la cámara × 0,01) y destello = `[0xFA2768]` (`Update3D` 0x83587C, la tormenta más cercana que
  contiene la cámara). El nublado sale de una sola fuente, `Clouds::WeatherOvercastAtCamera()`; `3D/SkyWeather` solo
  da el destello (`weather::LightningFlashAtCamera(cámara)` de `ECS/Weather/LightningFlash`). Test:
  `test_land_light` contra `tmp_dis\agua\light_lut_testgen.py` (entradas exactas en binario: con 1,3 o 0,6 las
  columnas en float caen al otro lado de un entero que los double de Python). La alineación es la suavizada del cielo (`Renderer::_skyAlignment`, [0xBF3378]).

## Neblina de distancia (original, detalle "Fog", niveles 3–6)

- Parámetros por fotograma en `fn_00869850` (0x869CB8..0x869F78 → `fn_007FEAA0` / `fn_007FEAD0`), a partir de la
  base de la tabla de luz (con el tope de tiempo nublado): `k = min(255, (R + 4G + 3B)/8 + 8)`, color = (R/3, G/3,
  B/3); con v' = 0 de día y de noche y 1 al ocaso: `near = 1/(0,0025 + 0,0075·v'²)`, `far = 1/(0,00111111 +
  0,000138889·v'²)` (400→900 / 100→800). Tormenta: color → (c>>3)+32, k → 48, near/far → 15/350 por w;
  relámpago: color y k → 255. Los 15/400/64 de los inicializadores estáticos no se usan.
- `t = clamp((z − near)/(far − near))`, z = profundidad de vista a lo largo del eje de la cámara. Difuso ×
  `(256 − trunc((256 − k)·t))/256`; especular += `round(color·t)` con saturación.
- Tierra: por vértice (`fn_00874AA0` 0x874C5B, SSE `fn_007A1800`), clases de bloque 0/1/2 (block+0x940); el
  especular de la tierra = RGB de la celda (D3DCOLOR, R y B cambiados) + neblina, sumado tras el small bump.
  Modelos: una vez por objeto en su origen (`fn_007FEB30`), sin neblina más cerca de near. No se aplica al mar, al
  cielo (solo tinte de tormenta), al sol y la luna (alfa ÷ (1 + 8w)), ni a partículas PSys, sombras o la mano.
- Valores (neutral, despejado): mediodía near 400 far 900 k 211 color (63, 70, 65); ocaso 100/800 k 120 (56, 37,
  31); medianoche 400/900 k 81 (16, 24, 28). Referencia:
  `tmp_dis\render\haze_calc.py`.
- openblack: `LandLightTable::GetHaze`, `u_haze` / `u_hazeColour` en `vs_terrain` y `vs_object`.

## Cámara

- FOV horizontal 70°, sin plano lejano; el cercano sigue la altura sobre el suelo: `0,3 + 0,16·h` (0,3–3,5).
  openblack lo recalcula cada fotograma en `Game` tras `camera.Update`.

## Sombras (tres sistemas del original)

Informe completo: disassembly en `tmp_dis\render\shadow_*.txt`.
- **Estáticas** (hechas): `fn_008721A0` (hilo de texturas de bloque, tras nieve y huellas). Proyectan todos los
  Fixed y MobileObject (`SetShadowOnTexture` en `Create3DObject` 0x52DE30 / 0x607210), árboles y bosques con prueba de
  alfa (`DrawTextureShadow`); no AnimatedStatic, DeadTree, flores, vasijas, comida mágica, cultivos, escudos,
  semillas, tótems. LOD 0, cizalla por el sol x' = x + h, z' = z + h (h sobre la base del objeto), cobertura 4×2 por
  texel; el RGB del texel × (255 − I/2)/256, es decir ×0,5 con cobertura total (×0,75 con texturas de 128 px).
  openblack: `RenderPass::StaticShadow`, `vs_static_shadow_instanced`/`fs_static_shadow` (MAX), textura R8 de la isla
  (`LandIsland::GetStaticShadowFramebuffer`), rango propio de instancias (`CastsStaticShadow` en
  `RenderingSystem.cpp`), aplicada en `fs_terrain` tras las huellas.
- **Dinámicas** (la mano, hecha; ver [rendering-objects.md](rendering-objects.md#sombra-dinámica-de-la-mano)): `ShadowInfo` 0x4AC bytes, lista 0xFAA7E0; creature y mano (las únicas que caen sobre
  objetos), objetos lanzados, barcos, SuperVillagers. Silueta 32×32 ARGB4444 (alfa n/15, máx. 53 %), luz: creature
  a ≥45°, mano vertical (+200), SuperVillager el sol; se desvanece entre 50 y 80 radios de distancia; sobre la tierra
  por bloque (`fn_00878350`, modo 6, sin Z, nada en celdas de altitud ≤ 1); sobre objetos con ZFUNC EQUAL.
- **Manchas de aldeanos y animales** (hechas): `human_shadow.raw` 32×32 entre dos huesos (`fn_0081FFF0`), modo 6;
  ver [rendering-objects.md](rendering-objects.md#manchas-de-aldeanos-reflejos-de-objetos-y-lod).
- Sombras de los objetos físicos y de la mano sobre objetos: [rendering-objects.md](rendering-objects.md#sombras-de-los-objetos-físicos).

## Cielo: sol, luna y nubes (original)

Informe: `tmp_dis\render\sky_*.txt`.
- La hora que usan el sol, la luna y el cielo es la **hora de guion** (umbrales fijos 3,5 / 7,5 / 8 / 8,5 h); el reloj
  real del juego es la hora visual con los umbrales del ciclo. Ver [day-night-weather.md](day-night-weather.md).
- **Sol** (`fn_0086C020` / `fn_0086C140`): `sun.l3d` (quad vertical 9928), `sun.raw`, modo 13 (aditivo SRCALPHA/ONE,
  sin Z), en (−30000, y, −30000) girado 3π/4, y = 7500·(clamp(min(T, 24−T), 6, 12) − 6)/6, color 0x957C63, alfa
  0 → 255 entre 3 y 6 h y 255 → 0 entre 18 y 21 h (÷(1 + 8·nubes)). **Resplandor** (`fn_0086BB60`, al final del
  fotograma): la misma malla ×1,8, color 0xA06A35, sin prueba de Z, visibilidad por 5 muestras ocultas por el
  terreno, suavizada 1 %/ms.
- **Luna** (`LH3DAtmos::UpdateGame` 0x8356E0): cámara + (4000, 1100·cos θ − 150, 800·sin θ), θ = T·π/12; alfa
  m = min(200, 0,5·y − 110); color = fila 5 de `palette.raw` por alineación. Halo (`fn_0086A930`): quad de 4000 en la
  base de `fn_0086AC60` (+Z del ojo a la luna, X horizontal en la vista), `atmos.raw` UV 0,25–0,49375 (v0 abajo a la
  izquierda = 0,25), aditivo, color (R/6, G/5, B/4, m). Malla: `moon.l3d` (cargada sin skins, textura `weather.raw`
  por código), esa base ×4, `fn_0086AFA0`(−0,1309) = Rz(+7,5°) en glm, RotateY(fase + π) = Ry(−(fase + π)) en glm,
  ×0,65 (detalle en [rendering-objects.md](rendering-objects.md#objetos-que-miran-a-la-cámara-billboards)); la fase sale del **reloj real**
  (2π(1 − frac((días − 10962)/29,5306))) y regenera las UV. Culling normal (bit 0 = 0).
  **Reflejo** (`fn_0086B010` 0x86B61D; hecho, W8): la primera llamada dibuja halo + malla + la `DrawUnderWater` de la
  malla (vt+0x118: la luna espejada en y = 0, sin luz); la segunda, con `pos.y = −pos.y` y `[0xFA2774]` = 1, solo el
  halo. Mismo m y mismo color, en la etapa del cielo (la tierra reflejada lo tapa). En openblack, `DrawMoon(…,
  mirrored)` en la pasada de reflejo: el halo con la vista de la cámara espejada y la malla con la de la principal
  (sale volteada, como `DrawUnderWater`, con el culling invertido). La inclinación k = −1 de la segunda llamada no se
  ve nunca: esa llamada no dibuja la malla (`fn_0086A930` 0x86AC0F). El sol **no** se refleja (`fn_0086C140` se llama
  una sola vez).
- **Brillo de la mano de noche sobre el agua** (hecho, W9; `3D/HandWaterGlow`, `Renderer::DrawHandWaterGlow`):
  `GLandscape::Draw` 0x5E4D89, si k = `[0xD20184]` > 0,01 (k = clamp((120 − media del color base)/15, 0, 1), la misma
  que la luz de la mano en tierra): color = fila 6 de `palette.raw` (`[0xFA26E0]`) movida un cuarto hacia
  (255, 128, 64) por canal (entero: floor((3c + objetivo)/4)), alfa clamp(ftol(k·190), 0, 190). `fn_005E3F70`: solo
  si alguna celda de [(x − 70)/10, (x + 70)/10] × [(z − 70)/10, (z + 70)/10] (límites inferiores recortados a 0..511,
  los superiores no) tiene altitud < 5 o no tiene celda; quad horizontal en y = 0 de (x ± 60, z ± 60), UV
  (0,75; 0,375)–(0,796875; 0,421875) de `atmos.raw` (u con x, v con z), `LH3DAtmos::AdditiveMaterial` (modo 13,
  SRCALPHA/ONE; con la textura `data\textures\atmos.raw` (`fn_00835AD0` 0x835C20..0x835C4E crea `AtmosMaterial` modo 6 y `AdditiveMaterial` modo 13 con ella)), ZFUNC ALWAYS, justo antes del mar. En openblack va al
  final de la pasada de reflejo (un quad en y = 0 es su propio espejo) y se ve a través del mar; con la mano sobre
  tierra alta no sale nada.
- **Nubes** (`CloudInSky::Open` 0x5E23F0, `fn_005E25C0`): 70 + 2 fijas, x ∈ ±8000 (viento a 70 u/s, ángulo 3π/4,
  alrededor de (1280, 1280)), y 300–500, z ±5000, tamaño 13–50, k 2,5–5; alfa de borde por encima de ±6000.
  `mist.l3d` sin skins: material de humo `smoke.raw` + `smokea.raw` (modo 6, dos caras, `fn_0080BBD0`). Son
  objetos LH3DMist (+0x80 |= 2, rama efecto), así que usan el mismo `fn_007FA300`: el **billboard**
  mat3(derecha, −delante, arriba) de la sección "Niebla del mapa" y la **escala no uniforme** s = tamaño/(1 + (k − 1)
  (1 − |dy|/|d|)) con el tamaño en la fila 0 (ancho en pantalla) y s en las filas 1 y 2 (profundidad y alto). Cerca del
  horizonte |dy|/|d| ≈ 0,05–0,2, luego cada nube es una **elipse horizontal ~k veces más ancha que alta** (2,5–5; las
  dos fijas, tamaño 300 y k 20, bandas casi planas); solo se ven redondas justo debajo. Atlas 8×8 animado
  (fotograma (contador/20) & 15, UV ((f&7)/8, (f>>3)/8 + 0,25)), luz cenital con ambiente 210/256; color de
  alineación × tabla[255] · 186/256 + 35;
  alfa 0 en tierra buena, 200 neutral, 255 mala.
  - **Colocación** (`CloudInSky::Open` 0x5E2439..0x5E24F4, informe `tmp_dis\mapa\clouds_placement.md`): cinco
    `Random` por nube en este orden (x, y, z, tamaño, k), también las nubes 0 y 1, cada una por su cuenta y uniforme en
    la caja: **las nubes del cielo no van en grupos**. Los grupos que se ven salen del azar (≈1500 u de media entre
    68 nubes, con rachas y huecos) y de la perspectiva; los grupos de verdad del original son las nubes de tormenta
    (`GWeather::DrawClouds` 0x83FC90: hasta 16 bolas por tormenta alrededor de su centro, oscurecidas y con neblina;
    openblack no las tiene aún, ver [day-night-weather.md](day-night-weather.md)).
  - `Random` 0x81D180 = min + (max − min)·(rand()·3,0518509e−05f) con el `rand()` de la CRT de MSVC (0x7C8837,
    s = s·214013 + 2531011, (s >> 16) & 0x7FFF), sembrado una vez con `srand(time(NULL))` (0x577721), no el GRand
    sincronizado: **otro cielo en cada sesión y en cada tierra**. `GLandscape::Open` → `GLandAlignement::Open` 0x5E1D10
    → `CloudInSky::Open` rehace las 70 nubes en cada carga de tierra. openblack: `Clouds` (el mismo generador; semilla
    fija con `OPENBLACK_CLOUD_SEED=<n>`; `Clouds::OnLandscapeOpened` desde `InitializeLevel`).
  - Paso (`fn_005E25C0`): x += inc·70·0,001; pasado 8000, t = x + 8000, x = t − ftol(t/16000)·16000 − 8000 (solo x:
    cada nube vuelve por la misma línea a la misma altura); borde = fistp((x ± 8000)·0,1275) (redondeo), fijas 192;
    alfa = borde·A/255 en enteros.
  - **Color** (`fn_005E1DE0`, cada fotograma desde `GLandAlignement::DrawSky`; informe
    `tmp_dis\mapa\clouds_colour.md`): i = trunc(X), f = trunc((X − i)·256), cada byte a + floor((b − a)·f/256) entre
    00FFFFFF / C8FFFFFF / FFAAA066 (0xBF339C), RGB·tabla[255] (c·t >> 8), luego c + floor((8960 − 70c)/256) (255 →
    **220**: el original nunca las pinta blancas). Luz por vértice (`fn_0084BA90`): I = fistp(255·N·L),
    f = 210 + (45·I >> 8) (210..**254**), difuso (c·f) >> 8. La hora entra solo por la tabla (filas 0-2 de
    `palette.raw`) y el tiempo solo por su tope de nublado. Mediodía neutral ≈ (172..208, 179..217, 177..214), gris
    claro que con α ≈ 0,7 sobre el azul se ve blanquecino; al atardecer salmón; mala: ocre opaca; buena: ninguna.
    **Land1 empieza a las 7,3 h de guion** (casi pleno atardecer) con el reloj parado: de ahí las nubes pardas; con
    `OPENBLACK_TIME_OF_DAY=12` salen gris claro como en el original.
  - **Alineación del cielo** [0xBF3378] (0 buena, 1 neutral, 2 mala; empieza en 1 y cargar tierra no la toca):
    objetivo [0xBF337C] = (1 − clamp((v + 1)/2, 0, 1))·2 (`fn_005E2240`), con v = `GetAlignmentValue` del jugador con
    más influencia en la posición de la interfaz (`fn_0064AC30` desde `GPlayer::ProcessPlayers` 0x64A697, cada turno;
    `DoCitadelMultiplayer` fuerza 0,5). `DrawSky` 0x5E2160 la mueve 0,001 por ms (inc·0,01·0,1) y la ajusta al pasarse;
    la usan las nubes, la tabla de luz y el cielo. openblack: `SkyAlignment` (Renderer), objetivo
    `Clouds::InfluentialPlayerAlignment()` (`ecs::effects::alignment::GetInterfaceAlignment()` × 2 − 1, o el
    deslizador de depuración, o `OPENBLACK_TEST_SKY_ALIGNMENT` de −1 mala a 1 buena; `atmos_banks::Alignment()` usa el
    mismo valor) y nublado
    `Clouds::WeatherOvercastAtCamera()` = byte 3 de `weather::atmos::GetWeatherSmooth(cámara, 1)` × 0,01 (0 sin
    tormentas).
  - **Animación**: cada nube es un LH3DMist con su propio contador +0x84; `LH3DMist::AddDrawing` 0x7FA7F0 (vt+0x100)
    solo la manda al Z-sorter si su esfera (semidiagonal de la malla × tamaño × 0,55) toca la pantalla, y solo entonces
    avanza el contador en el Draw: las nubes se desfasan entre sí. **No hay fundido entre fotogramas**: `fn_007FA300`
    calcula fotograma = (contador·45)/900 en enteros (0x7FA3F4..0x7FA41B, sin fracción), pone un solo desplazamiento de
    UV (vt+0xE8 = 0x7F9B70: +0x68/+0x6C) y dibuja una vez (`fn_0080DB30`); el modo 6 (`fn_0082DF10`) solo configura la
    etapa 0 (MODULATE textura × difuso). El cambio es de golpe cada 20 cuentas (≈78 ms, 16 fotogramas en ≈3,5 s);
    lo mismo para la niebla del mapa.
- **Sombras de nubes**: `sclouds.raw` 40×40, un texel por celda desde la esquina de la nube,
  `lum = min(lum, max(48, 255 − (255 − s)·α/255))`; openblack: `Clouds::BuildShadowCap` → textura R8 por celda que
  usan `vs_terrain` y `vs_object` antes de la tabla de luz.
- **Ids de skin 0xFF…** (`mist.l3d` 0xFFD0EBBE, `moon.l3d` 0xFF52BA30): no se resuelven nunca; esas mallas se
  cargan sin skins (`LH3DMesh::Create(data, 1)`) y su textura la pone el código. Además su tabla de skins empieza en
  el final del archivo (como `sun.l3d`): el parser de L3D de openblack ahora lo tolera.
- **Arreglo general**: `HashIdentifier(hashed_string)` volvía a hacer hash del número; `Contains`/`Load` con
  `entt::hashed_string` no encontraban nada (era la causa de las "texturas raw/* que faltan").

## Ríos

**Fiel** (hechos). Informe completo con direcciones y pseudo-C++: `C:\Users\diewgarc\dev\tmp_dis\streams\streams.md`.

- **No hay renderizador de ríos.** `GStream` (GameThing 0x47, Stream.cpp) guarda sus puntos en orden de script
  (`CREATE_STREAM_POINT` 0x717550 pone y = altura del suelo y añade al final); los tramos son p[i] → p[i+1]
  (openblack enlazaba cada punto con el más cercano anterior: corregido, `Stream::points`).
- `GStream::CreateAll` 0x733FF0 (tras el script) → `CreateRiver` 0x7341E0: por tramo, ángulo θ = atan2(dz, dx),
  posición p[i], escala solo en X local = longitud 3D / 30, y dos huellas de terreno (fn_0081E9E0):
  `data\river2.l3d` (cauce marrón, se mezcla en el color del bloque como la huella de un edificio, fn_008728A0) y
  `data\river.l3d` (bandera +0x38 = 1: solo su alfa, que **baja** el nibble de alfa del bloque: min(dst, src),
  fn_00872AB0; orillas 15/15, canal 10-14/15). Huellas de 32×64 ARGB4444, 10,6 × 30 unidades.
- El agua visible es **el mar**: se dibuja antes de la tierra (ZFUNC ALWAYS) y la tierra (modo 14, SRCALPHA) deja
  verlo en el canal. Sin textura, scroll, partículas ni LOD propios; el "flujo" es el movimiento del mar.
- openblack: `ECS/Rivers` crea al cargar el mapa dos entidades `StreamFootprint` + `Transform` por tramo; el cauce va
  en la pasada de huellas (`Renderer::DrawRiverFootprints`) y el canal en `RenderPass::LandAlpha`, un R8 de toda la
  isla borrado a 1 con mezcla MIN (`fs_land_alpha`, texel más cercano, cuantizado a 1/15); `fs_terrain` multiplica su
  alfa de salida por ese valor.
- Sin analizar: el sonido `ATMOS_TYPE_RUNNING_WATER` (`audio/sfx/atmos/stream.sad`). Las cascadas: ver
  [water.md](water.md#decorado-fijo-por-tierra-cascada-de-land-3-arca-y-dinosaurio-de-land-4) (`GWaterfall` no dibuja nada; la de `waterfall3.l3d` es `DesignedWaterFall`, solo en Land 3).

## Fundido de pantalla y bandas de cine

**Fiel** (hecho; sin el negro de `OnNewGame`). Informe: `tmp_dis\render\fade_notes.txt` (+ `fade_script.txt`, `fade_widescreen.txt`, `fade_chl_scripts.txt`).
- Estado GScript (g_game+0x250090): +0xB0 paso de alfa por turno, +0xB4 alfa, +0xB8 color ARGB.
  - `SET_FADE(r, g, b, t)` (0x6FCD70 → `SetupScreenFadeTo` 0x6EBA90; todo truncado, t como char): t ≤ 0 → A = 255 al
    instante; si no, alfa 0 y paso 255/(10t) (el byte A no cambia hasta el turno siguiente).
  - `SET_FADE_IN(t)` (0x6FCE00 → 0x6EBB00): t ≤ 0 → A = 0; si no, alfa 255 y paso −255/(10t).
  - `FADE_FINISHED` = paso == 0. `ProcessFade` 0x6EB9D0 una vez por turno de juego (100 ms): no avanza en pausa.
- Dibujo `fn_0086FEE0` (FinishFrame, justo antes de EndScene, tras la ayuda y los rectángulos 2D): si A ≠ 0, quad de
  color x 0..W−1, y h'..H−1−h' (h' = h − 1 con bandas), modo 1, ZFUNC ALWAYS; luego las bandas otra vez encima; el color
  se pone a 0 cada fotograma.
- Bandas `[0xEB9950]` = f: altura `trunc((H − 0,5625·W)·f)/2` (16:9 con f = 1; nada en pantallas más anchas),
  negras. `SET_WIDESCREEN` (32, `HelpSystem::SetWideScreen` 0x5C6AD0) desliza f linealmente en
  `HelpSystemInfo.wideScreenTime` = 2 s de tiempo de juego, continuando desde donde esté; `WIDESCREEN_TRANSISTION_FINISHED` (132).
- Otras fuentes: `OnNewGame` 0x553980 pone negro al instante en Land 1 (la intro `FollowUs` lo quita con
  `SET_FADE_IN(12)`); `Temple::UpdateFade` 0x794280 en la ciudadela (tiempo real, 1/s), sin portar.
- openblack: `3D/ScreenFade`, vista `RenderPass::ScreenOverlay`, `Renderer::DrawScreenOverlay`. **No** se aplica el negro
  de `OnNewGame`: la intro de openblack aún se queda antes de su `SET_FADE_IN` (`START_CAMERA_CONTROL` y otros son stubs)
  y la pantalla quedaría negra.

## Texto: fuentes del original y el mensaje de la mano

**Fiel** (hecho; el margen respecto a la mano es **(aproximado)**). Informes: `tmp_dis\font\font_notes.txt` (formato), `tmp_dis\numbers\NOTES_numbers.md` (mensajes).
- Fuentes `Data\j0` ("Ocean Sans MM", la de los mensajes), `f1`, `f3`: `.met` = u32 alto de celda 80, wchar[128]
  nombre, u32 número, registros de 28 bytes {u16 código, u16 ancho del bitmap, s16, u16, f32 izquierda, f32 ancho,
  f32 derecha, u32 desplazamiento y u32 tamaño en el `.fnt`}. `.fnt` (`CachePage::RenderChar` 0x830C10): longitudes de
  tramos de 1 bit alternando 0/1 desde 0; un byte, o 0xFF + u16; bitmap de ancho × 80 por filas.
- Caché de glifos: ARGB4444 blanco, alfa por bloques 2×2 con la tabla 0x9A3990 {0, 4, 8, 12, 15}/15, un texel
  transparente a cada lado; filas 40..59 a un cuarto (tamaños < 26). `DrawTextRaw` 0x832C60: s = tamaño/80,
  X0 = x + avance + izquierda·s, X1 = X0 + (ancho + 2)·s, alto = tamaño; avance += (izquierda + ancho + derecha)·s; sin
  kerning; modo 16 (SRCALPHA/INVSRCALPHA, prueba de alfa ≥ 5, sin Z).
- Mensaje de la cantidad en la mano: `ToolTips::ForceToolTips(0xEEA, cantidad)` cada turno de una selección bloqueada
  (montones, campos, piscifactorías) y 12 turnos después; texto 0xEEA de `Scripts\InfoScript2.txt` ("Cantidad:
  %3.0f"; el id es el orden de las líneas ADD_TEXT). `CameraHelp::DrawKeyOrMouse` 0x447EA0: junto a la mano en
  pantalla, caja de H/25 (texto a 2/3), texto amarillo con dos copias negras a ±1 px; pasa al otro lado de la mano
  pasado 2/3 de la pantalla. El usuario comprobó en el original que **no hay fondo** (la caja aditiva del código no
  se ve) y que la cantidad **se muestra mientras la mano la sostiene**, hasta soltarla.
- openblack: `Graphics/GameFont` (atlas R8 con la misma rasterización), `Common/HelpText`, `Renderer::DrawHandToolTip`
  (vista `ScreenOverlay`), `fs_text`. Gancho `OPENBLACK_TEST_TOOLTIP=<n>`. El margen del texto respecto a la mano
  (media caja) es una estimación.
  Faltan los demás mensajes (al pasar sobre montones y almacenes, "Recoger"...).

## Partículas (PSys)

El motor PSys (formato de los archivos, paso por turno, dibujo, efectos de guion, las reglas portadas por agua y las
creencias del pueblo) está en [particles.md](particles.md#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua).

## Niebla del mapa (LH3DMist, `fn_007FA300`)

El dibujo de la niebla del mapa (`fn_007FA300`, billboards, orden, fundido y las otras nieblas de `mists::Submit`)
está en [map-loading.md](map-loading.md#dibujo-lh3dmist-fn_007fa300), junto con su creación (CREATE_MIST).

## Pendiente

- Nubes de tormenta (`GWeather::DrawClouds` 0x83FC90): ver [day-night-weather.md](day-night-weather.md).
- Fundido: el negro de `OnNewGame` (la intro de openblack aún no llega a su `SET_FADE_IN`) y `Temple::UpdateFade` en
  la ciudadela.
- Texto: los demás mensajes (al pasar sobre montones y almacenes, "Recoger"...); el margen del texto respecto a la
  mano es una estimación.
- Ríos: el sonido `ATMOS_TYPE_RUNNING_WATER` (sin analizar, ver [water.md](water.md#audio-del-agua)).

## Ganchos de prueba

En [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración):

- `OPENBLACK_SEA_TRACE=1` (filas del mar cada 500 fotogramas) y el test `test_sea_rows`.
- `OPENBLACK_DUMP_BLOCK_TEXTURE` (textura de bloque de la costa) y el test `test_land_light` (tabla de luz).
- `OPENBLACK_CLOUD_SEED=<n>`, `OPENBLACK_TIME_OF_DAY=<h>` y `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (cielo y nubes).
- `OPENBLACK_TEST_FADE="r,g,b,segundos"` y `OPENBLACK_TEST_WIDESCREEN=1` (fundido y bandas).
- `OPENBLACK_TEST_TOOLTIP=<n>` (mensaje de la mano).

## Fuentes

- `dev\tmp_dis\render\`: `scan_states.py`, `scan_vt.py` (estados D3D), `shadow_*.txt`, `sky_*.txt`, `light_lut.py`,
  `haze_calc.py`, `fade_notes.txt` (+ `fade_script.txt`, `fade_widescreen.txt`, `fade_chl_scripts.txt`).
- `dev\tmp_dis\agua\`: `sea_render.md`, `sea_coast_alpha.py`, `light_lut_testgen.py`, `re\emu_sea_range.py`,
  `re\emu_block_texel.py`, `re\cmp_block_dump.py`; `shore\edge_stats.py` (alfa costero en las aristas de las celdas
  0x02, todas las tierras) y `shore\map_around.py` (mapa de altitudes y celdas 0x02 alrededor de una celda).
- `dev\tmp_dis\mapa\`: `clouds_placement.md`, `clouds_colour.md`, `emu_inv.py`.
- `dev\tmp_dis\streams\streams.md` (ríos), `dev\tmp_dis\font\font_notes.txt` y `dev\tmp_dis\numbers\NOTES_numbers.md`
  (texto).
