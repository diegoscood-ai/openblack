# Render: original frente a openblack

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

## Mods gráficos (desactivados por defecto; ids y uso en [mod-library.md](mod-library.md))

| Mod | Línea de comandos | Qué hace |
|---|---|---|
| `graphics.msaa` | `--msaa 0/2/4/8/16` o menú Mods | Backbuffer multimuestreado (`BGFX_RESET_MSAA_*`). Además, en las pasadas opacas los cut-outs usan **alpha to coverage**: `fs_object` convierte el corte en una rampa de ~1 píxel con `fwidth` (`u_skyAlphaThreshold.z`). |
| `graphics.mipmaps` | `--mipmaps` | Añade mips y filtrado trilineal a las texturas de modelos, pieles L3D, materiales y bump del terreno y texturas `.raw` sueltas. |
| `graphics.anisotropic` | `--anisotropic` (implica mipmaps) | Añade `BGFX_SAMPLER_*_ANISOTROPIC` y `BGFX_RESET_MAXANISOTROPY`. |
| — | `--enhanced-graphics` | Equivale a `--msaa 4 --anisotropic`. |
| `water.living` | `--living-water` o menú Mods | "Agua viva": el reflejo del mar incluye modelos y sprites (el original solo refleja cielo y tierra) y ondula en bucle con dos capas de `skya.raw` que se desplazan (mapa de olas), más fuerte cerca y nula a 1500 de profundidad; además quita la ondulación por filas del original (líneas fijas en pausa, temblor a fps modernos) y hace derivar `sky.raw` y `skya.raw` juntos (0,020 / 0,012 texturas por unidad de tiempo). Usa tiempo real a un cuarto de velocidad (también en pausa) que da la vuelta cada 1000 unidades (4000 s); las velocidades son múltiplos de 1/1000 textura/s, así el bucle no salta. |

### Implementación

- `Graphics/TextureMipmaps.cpp`, `BuildRgba8MipChain`:
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

### Verificación

Capturas (estaban en `dev\gfx\`, borradas en la limpieza del 2026-09-30; se regeneran con estas cámaras y
opciones):
- `base_*` frente a `enh_*` / `enh2_*`: aldea `1818,75,2612,1824,44,2636` y panorámica
  `1600,160,2350,1900,40,2750`, con `-n 14000 --screenshot-frame 13900`. Con mips la carga es más lenta y a 8000
  fotogramas el vuelo aún no ha terminado.
- [img/crop_trees_zoom.png](img/crop_trees_zoom.png), rejilla de cuatro: original, mips, MSAA y todo.

## Mezcla de materiales L3D (original, no es un mod)

- `L3DSubMesh` traduce el tipo de material a `blend`/`depthWrite`/`thresholdAlpha`, pero el renderizador no usaba
  `blend`: todo salía opaco. Ahora los materiales con mezcla y sin corte de alfa (`AlphaTextured`, `TexturedAlpha`,
  `SmoothAlpha`, `*Nz`, aditivos sin chroma) se dibujan con el alfa de la textura (`u_skyAlphaThreshold.w`),
  `SRCALPHA/INVSRCALPHA` (o `SRCALPHA/ONE` los aditivos), sin escribir Z en los `Nz`, en la vista `MainBlended`
  (después de todo lo opaco). Los `TexturedChroma` siguen con prueba de alfa.
- La mano (`Hand_Boned_Base2`, material `AlphaTextured`) tiene en su piel un degradado de alfa en las filas de abajo:
  la muñeca se desvanece. Antes acababa en un borde blanco duro ([img/hand_zoom.png](img/hand_zoom.png)).
- Mallas de `AllMeshes.g3d` por tipo de material: `Textured` 512, `TexturedChroma` 217, `Smooth` 181,
  `AlphaTextured` 116 (casi todos los edificios `MSH_B_*`), `TexturedChromaAlpha` 6 (arbustos, palmeras).
- Gancho de pruebas `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fijo en fracción de la ventana (la mano aparece en capturas
  sin ratón real).

## Detalle del terreno ("small bump")

Informe completo del desensamblado; direcciones W120:
- `fn_00804830` carga `.\data\Textures\smallbump.raw` con el flag de alfa (0x41). Al primer uso `fn_00837400` lo
  empaqueta en ARGB4444 y mete `smallbumpa.raw` en el nibble de alfa (`"a.raw"`, 0x8375C1). Material modo 0xE.
- Modo 0xE = `fn_0082DD90` (tabla de modos 0xC38728, entradas {fn, flag} de 8 bytes): `ALPHABLENDENABLE`,
  `SRCALPHA/INVSRCALPHA`, etapa 0 `TEXTURE*DIFFUSE` en color y alfa.
- `fn_007A1800` (dibujo de bloques): segunda pasada por bloque con difuso **blanco (sin iluminar)** y alfa = fundido;
  UV × 12 por bloque (una repetición cada 13,33 unidades). No se hace en bloques con material de superposición.
- Fundido (`fn_007FEE60`): línea a 50 unidades delante de la cámara sobre el plano y = min(cam.y, 0,67·165); pleno hasta
  20 unidades antes, nada 20 después (rampa de 40). Depende de la altura y del ángulo de la cámara (no 200 fijo).
- Resultado: `col = mix(col_iluminado, smallbump.rgb, smallbumpa · fundido)` → motas **claras**, también de noche.
  openblack hacía `col *= 1 − smallbumpa` (manchas oscuras) con solo el alfa y UV × 10.
- `SetUseSmallBump` (0x87FD30) solo cambia `[0xC37210]` (registro "UseSmallBump", niveles de detalle).
- `S_TileLandscape.raw` no es del terreno: es la entrada 6 de la tabla de texturas globales 0xBEF484, usada por el
  corazón de la ciudadela (`fn_00465C70`) **(inferido)**. `L_Smallbump_01.raw` no se usa.
- En esta instalación `smallbump.raw` (2017), `smallbumpa.raw`, `Sky.raw` y `S_TileLandscape*.raw` (2021) vienen de un
  pack de texturas, no son de 2001.
- openblack: `LandIsland::CreateSmallBumpTexture` (RGBA con cuantización a 4 bits), fundido por vértice en
  `vs_terrain` (`u_smallBumpLine`, `u_skyAndBump.w`), mezcla en `fs_terrain`.

## Mar (`sky.raw` + `skya.raw`)

Del desensamblado (W120):
- `GLandscape::Open` 0x5E5432 carga `.\Data\Textures\sky.raw` (flag de alfa; `skya.raw` es el alfa, 4 bits) con
  material modo 5 (= `fn_0082DD90`, igual que el 0xE: `SRCALPHA/INVSRCALPHA`, `TEXTURE*DIFFUSE`).
- `GLandscape::Draw`: cielo → tierra reflejada (`LandRef`, alturas × −1, tabla de luz × 0,5, `fn_007FF4F0`) → mar
  (`fn_00879930`) → tierra. El mar se salta solo si `[0xECA670]` (alambre, depuración) o `[0xECA664]` ≠ 0; este lo pone
  `PSysLightMaps` (0x6CA57E) a `clamp(nivel,0,1)·190` y el nivel solo sube/baja al acabar una cinemática
  (`CameraModePath::Cleanup`): **en juego normal el mar se dibuja**.
- Tiras de filas de 2 px en pantalla, `ZFUNC ALWAYS`, sin escribir Z. UV = (mundo + desplazamiento) / P,
  P = 2000 − 1800·WaterTiling (200 en el detalle máximo). Desplazamiento con el viento ambiente (−1/330 por ms).
  Ondulación: cada fila se mueve 0,9·sin(i·π/8) a lo largo del frente horizontal de la cámara,
  i = (fotograma + 2·(fila+1)) & 15, pleno a más de 70 de profundidad, nada a menos de 30.
- Color del vértice = entrada 255 de la tabla de luz del terreno (luz plena de la hora del día); alfa = 255 hasta
  profundidad 7000, baja lineal a 80 en 14000 (0xC39908).

En openblack (`fs_water`/`vs_water`, `Renderer`): P = 200, ondulación, fundido de horizonte, sin escribir Z (se
mantiene la prueba de Z porque bgfx reordena las llamadas de una vista), tierra reflejada a media luz
(`u_terrainPass`). **Pendiente**: desplazamiento con viento (openblack no simula viento), el tinte ya usa la tabla de luz (abajo)
y el reflejo solo lleva cielo y tierra: `GLandscape::Draw` 0x5E48B3–0x5E4900 apaga ZWRITE, llama a `fn_007FF4F0`
(`LH3DIsland::PreDraw` + bloques) y lo vuelve a encender; sin modelos ni sprites.

**Sin cerrar (no aplicado)**: el agente dedujo que el original no dibuja las celdas con bit 0x02 de `flags` (0x875DDC,
0x876A8A, solo en la ruta no SSE), aplana las de altitud ≤ 3 y suma `cell.rgb` como especular (`SPECULARENABLE` = 1 si
`[0xEA9E9C]` = 0, 0x82CC1E). Probado: da una costa en escalones y fondo de arena que no se parece a las capturas del
original, así que se revirtió; openblack sigue con su alfa de celdas (agua 0, costa 0,5). Falta revisar la ruta SSE
`fn_007A1800` y comparar con capturas del original.

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
  `vs_terrain` muestrea por vértice; `u_seaColour` para el mar. Falta: tiempo nublado, relámpagos, suavizado de la
  alineación.

## Luz de los modelos (original, no es un mod)

- Por CPU (`fn_0084BA90`, vértices D3DTLVERTEX); `LH3DObject::DrawTnL` da la misma fórmula con luz D3D.
- Color base del objeto (`fn_00801C90`), uno por objeto y fotograma: `tabla[luminosidad]` de las 4 celdas alrededor
  de su origen, bilineal; especular = RGB de esas celdas leído como D3DCOLOR (R y B cambiados; casi siempre 0).
- Por vértice: `f = 90/256 + 166/256 · max(0, N·L)`, L = normalize(−500000, 500000, −500000 − origen) ≈
  (−0,577, 0,577, −0,577), fijo (no depende de la hora). Difuso = base · f; el especular se suma tras la textura.
- La mano: base × 1,5 (`CHand::AddDrawing` 0x46D135). Primitivas sin textura: color del material × base. Chroma:
  `ALPHAREF = umbral · alfa del objeto / 255 − 5`, `GREATEREQUAL`.
- openblack: `LandIsland::CreateCellMap` (textura RGBA por celda: rgb = color leído como D3DCOLOR, a =
  luminosidad), `vs_object` hace la bilineal y N·L; las normales ahora giran con el modelo y la instancia.
- **Trampa**: `vs_object` también lo usa el cielo (`fs_sky`); añadirle una varying nueva deja el cielo en blanco. El
  especular viaja en `v_texcoord0.zw` y `v_position.w` (después de calcular `gl_Position`).
- Falta: neblina (`fn_007FEB30`), tintes (veneno, fuego, `fn_0080BF10`), color de ventanas de noche, luz de la mano
  estampada en la tierra (`light_hand.raw`, `fn_008229B0`).

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
  31); medianoche 400/900 k 81 (16, 24, 28). Referencia: `tmp_dis
ender\haze_calc.py`.
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
- **Dinámicas** (la mano, hecha; ver abajo): `ShadowInfo` 0x4AC bytes, lista 0xFAA7E0; creature y mano (las únicas que caen sobre
  objetos), objetos lanzados, barcos, SuperVillagers. Silueta 32×32 ARGB4444 (alfa n/15, máx. 53 %), luz: creature
  a ≥45°, mano vertical (+200), SuperVillager el sol; se desvanece entre 50 y 80 radios de distancia; sobre la tierra
  por bloque (`fn_00878350`, modo 6, sin Z, nada en celdas de altitud ≤ 1); sobre objetos con ZFUNC EQUAL.
- **Manchas de aldeanos y animales** (hechas): `human_shadow.raw` 32×32 entre dos huesos (`fn_0081FFF0`), modo 6.

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
  m = min(200, 0,5·y − 110); color = fila 5 de `palette.raw` por alineación. Halo (`fn_0086A930`): quad de 4000 hacia
  la cámara, `atmos.raw` UV 0,25–0,49375, aditivo, color (R/6, G/5, B/4, m). Malla: `moon.l3d` (cargada sin skins,
  textura `weather.raw` por código), billboard ×4, −7,5° en Z, fase + π en Y, ×0,65; la fase sale del **reloj real**
  (2π(1 − frac((días − 10962)/29,5306))) y regenera las UV. Culling normal (bit 0 = 0).
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
    `Clouds::InfluentialPlayerAlignment()` (hoy neutral o el deslizador de depuración; `OPENBLACK_TEST_SKY_ALIGNMENT`
    de −1 mala a 1 buena) y nublado `Clouds::WeatherOvercastAtCamera()` (hoy 0): los dos esperan la alineación de
    jugadores (`GAlignment::Update` 0x414410) y el tiempo (`GWeather`/`LH3DAtmos`) de la otra rama.
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

## Repetición o recorte de texturas (hecho)

- `fn_00850FC0` 0x851779: tras poner el modo, `SetD3DTillingOn` si `g_b_need_tilling` (0xECA614) o el bit 2 del byte
  +5 del material; si no, `SetD3DTillingOff` (CLAMP). `g_b_need_tilling` lo copia cada Draw del objeto 3D de su bit
  0x200 de Flags1 (vt+0xE4 = `fn_007F9B30`), y ese bit solo lo pone el setter vt+0xE0 desde mallas de partículas
  (`ParticleMeshCreatorAnimTextured`, `ParticleVolBlendMeshCreator`): los objetos del mundo dependen solo del material.
- `AllMeshes.g3d`: 1675 primitivas con el bit, 161 sin él; ninguna de estas tiene UV fuera de 0..1, así que el recorte
  solo cambia el filtrado bilineal en los bordes de la textura. openblack: `Primitive::wrap` y flags de sampler en
  `Renderer::DrawSubMesh`. Script: `tmp_dis\render\l3d_wrap_scan.py`.

## Sprites en el orden de transparentes (hecho)

`LH3DSprite::Draw` también va al Z-sorter. En la pasada principal, los `components::Sprite` (polvo, partículas de coger,
destellos, luciérnagas...) entran en la lista de atrás a delante de `MainBlended` con los modelos transparentes, por la
distancia a la cámara; antes se dibujaban antes que todos ellos y un modelo transparente detrás los tapaba.

## Partículas (PSys, en curso)

Informe completo (formato, 136 clases, fórmulas, tiempo de ejecución, dibujo, tablas de efectos):
`tmp_dis\psys\psys_report.md`; los 132 archivos descomprimidos en `tmp_dis\psys\zzz\`.
- Archivos: `Data\Spells\ZSpellFiles\SF_X_txt.zzz` (u32 tamaño + zlib) con texto del editor: cabecera
  `BEGINPROPERTIES` (DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge) y bloques
  `BEGINCLASS <Clase> <Nombre>`. Un `.txt` suelto con el mismo nombre tiene prioridad (`LHLoadData`): sirve para mods.
- Modelo: cada modificador tiene `Group` (0..24) y `Condition`; una *colección* es una instancia viva de un grupo;
  `InitiallyCreated` crea las raíces en el origen; `NextGroups` da a cada átomo nuevo sus subcolecciones; `Hierarchies`
  pone los átomos hijos en el marco local del padre. Nada se mueve solo: solo las reglas.
- Paso por turno (dt = 0,1 s) con el estado de dibujo anterior y actual, interpolado al dibujar con la fracción del turno.
  Fin: sin átomos ni reglas de creación, o edad > MaxSpellAge; `CloseDown` activa `TrueOnCloseDown`, suelta las
  reglas `RemoveOnCloseDown` y borra al momento si `DeleteOnCloseDown`.
- Dibujo: cada efecto es un objeto del Z-sorter (`PSysManager::AddDrawing`), sus átomos en orden de lista; sprites de
  `S_SpriteSheet{1,2,3}` (8×8 celdas de 32 px, celda = (FileOffset + fotograma) & 63), quad orientado a la pantalla
  con giro atan2(M[0][2], M[0][0]) o plano XZ (`SetHorozontal`); modo 13 aditivo (102 de 137) o 6, sin escribir Z
  salvo `MaterialUpdateZBuffer`; sin luz ni neblina salvo `UseLandscapeColor` (no hecho).
- Guiones: `SPECIAL_EFFECT_POSITION` / `_OBJECT` (CHL 52/53) → `GParticleContainer` con la tabla `GSpotVisualInfo`
  (50 entradas → PARTICLE_TYPE → archivo); duración en segundos (−1 siempre, 0 la vida de la tabla); sigue al objeto
  y se cierra si desaparece; devuelve un objeto que el guion puede borrar.
- openblack: `src/PSys/PSysFile` (lector), `PSys` (colecciones, átomos, reglas: CreateRuleAnAtom/Sphere, emisores
  Simple/Disk/Conical, UR_WillowWisp, reglas de borrado, AR_FadeAlpha/FadeCollectionAlpha/FadeOutOnceConditionTrue,
  UR_ChangeScale, SetScale, SetAtomAlpha, UpdateRuleGravity, UR_UpdatePosnFromVelocity, UR_GustyWind (con un ruido
  propio: VLNoise3To1 sin portar), UpdateRuleRotatePrincipalAxis, FollowOrigin, UR_FollowParent, ForceConstant*,
  UR_SphereSurfaceTracer, UR_OrientSpriteWithRandomAngle; condiciones y proveedores de float), `PSysManager`
  (efectos, contenedores de guion, gancho de prueba) y `Graphics/RendererPSys.cpp`. Las clases sin portar se registran
  una vez en el log ("not ported yet") y no hacen nada.
- Prueba: `OPENBLACK_TEST_PSYS="SF_Bonfire,1790,2630,0,1"` con la cámara `1775,45,2600,1790,30,2630`, `-n 5000`
  (hoguera con llamas y humo); `OPENBLACK_PSYS_TRACE=1` escribe átomos y edad de cada efecto cada 20 turnos.
- **Creencias sobre el centro del pueblo** (`src/PSys/TownBelief.cpp`; informe `tmp_dis\psys\towncentre_notes.md`): cada centro funcional tiene TOWN_BELIEF (SF_TownBelief, `UR_TownCentreBelief` 0x69BF30), que avanza una vez por fotograma con dt = 0,1 s. Un símbolo por jugador con creencia: el primero (rango 0) quieto 2 unidades sobre la cima del tótem; los demás giran (radio y velocidad por la creencia, a 2,5 por rango de altura) y el segundo pelea (destellos). Se dibuja con dos brillos de S_SpriteSheet3 (color del jugador y blanco girando) y el símbolo. El símbolo del humano es la celda del "player symbol" del perfil (registro; 0 sin él, como en esta instalación) copiada de ChooseSymbol (PlayerSymbol::OpenOnce 0x5DE2F0); los rivales usan imágenes .cps (no hecho). Base: el tótem (`components::TotemStatue` de campos): x/z del pedestal, y = baseY + alto de la malla del icono × escala + 2. Falta la columna SpellColumn del dueño.
- Pendiente: creadores de malla, niebla, cadenas, animación, mapas de luz (se estampan en la luz del terreno), las
  reglas de hechizos y del pueblo (`UR_TownCentreBelief` ya está: ver arriba), `CreateRule_GameObjectRef` (el brillo de las llaves de la
  puerta de Land1, SF_HighlightOnObject), los sonidos, y pasar a este motor los efectos de la mano de `HandEffects.cpp`.

## Sombras de los objetos físicos (hechas)

Informe: `tmp_dis\render\physshadow\`.
- `fn_00646FE0` (desde `GLandscape::Draw` 0x5E49DC) → `fn_007FCE80` por objeto físico que no esté en reposo (byte
  elem+0x19C = PhysOb+0x174) ni con y ≤ −r: si no tiene sombra y proyecta sombra estática (Flags1 0x1000 / 0x2000) o
  está animado, `fn_008745A0` crea un `ShadowInfo` solo de tierra (si+0xC = 1). No la tienen las vasijas, la comida
  mágica, flores, cultivos, DeadTree, AnimatedStatic ni los trozos de edificio (`SetShadowOnTexture(0)`). Se libera en
  `PhysOb::DeInitialise`. Sin límite de número ni clave de detalle.
- Luz: la posición del objeto + (0, 15000, 0) (0x9A3C10), no el sol: proyección prácticamente vertical. Caja = la
  mínima de los vértices proyectados (sin margen). Silueta 32×32 con 4×2 submuestras por texel (`fn_00806F60`), alfa =
  submuestras cubiertas / 15 (máx. 8/15) sin escribir el anillo exterior (`fn_00880FC0`, tabla 0xFA95C4); los árboles
  por la ruta con prueba de alfa y un filtro 2×2 (este último no se hace aquí). Fundido 50–80 radios desde la cámara
  hasta el suelo bajo el objeto (`fn_00874600`). Sobre la tierra (`fn_00878350`): UV = XZ del vértice en la caja
  (aumento 1 + h/15000, despreciable), sin atenuar con la altura, nada en celdas de altitud ≤ 1; modo 6 negro.
- openblack: `Graphics/PhysicsShadows` (`PhysicsObjects::ForEach`): atlas de 4×4 siluetas a 4×2 de resolución
  (vista `PhysicsShadow`), un pase que cuenta las submuestras en texels 32×32 (`PhysicsShadowResolve`,
  `fs_physics_shadow_resolve`) y un bucle en `fs_terrain` sobre las cajas (hasta 16). Las cajas de mallas con huesos
  salen de las 8 esquinas de su caja (piel rígida en espacio de hueso).
- **Sombra estática de lo que no está en el mapa**: el horneado (`fn_008721A0`) toma los emisores de las celdas del mapa
  (`0x5E2A90` / `0x5E2C30`); coger un objeto (`fn_005DC330`) o darle físicas (`Object::InitialisePhysics*`) lo saca de
  ellas hasta que aterriza (`EndPhysics` → `InsertMapObject`). openblack: `CastsStaticShadow` excluye el objeto en la
  mano y los que vuelan. Rareza no reproducida: solo los Fixed/MultiMapFixed vuelven a hornear el bloque al salir; la
  sombra vieja de un árbol o un MobileObject se queda en el suelo hasta que otro cambio rehornea ese bloque.
- Prueba: `OPENBLACK_TEST_PHYSICS="1490,2140,760,0,0,0,0.6,3"` con la cámara `1450,60,2095,1492,6,2140` y `-n 4000`
  (las rocas caen a ~70 u/s; en la captura están a ~110 de altura y sus tres sombras se ven en el suelo).

## Sombra dinámica de la mano (hecha)

- Silueta de la mano (las dos instancias del mesh) en un R8 de 64×64 (`RenderPass::DynamicShadow`,
  `vs_dynamic_shadow_instanced`), proyectada desde la luz 200 unidades encima de la mano sobre el plano del suelo,
  en una caja de ±2 radios; `fs_terrain` la cuelga vertical (sin la cara de mar, altura > 0,67) y oscurece
  × (1 − 8/15 · cobertura · fundido), fundido entre 50 y 80 radios desde la cámara.

## Manchas de aldeanos, reflejos de objetos y LOD (original)

Informe: `tmp_dis\render\misc_*` (con emulación Unicorn de `fn_0081FFF0`).
- **Manchas** (`fn_0081FFF0`, desde el Draw de objetos animados `fn_00812170`): pies = huesos 21 y 18 (fin de las dos
  piernas), en el suelo + 0,2; D = O·s − ((O·s)·n)·n con O = (√2, 0, √2), s = escala, n = normal del terreno;
  quad 1 desde el pie 21 con V = D + (P18 − P21)/2, quad 2 simétrico; esquinas C − 0,02V ± U y C + V ± U con
  U = 0,2·norm(1, 0, −1) (ancho fijo 0,4); UV (0,0)(1,0)(1,1)(0,1), alfa 1 en los pies y 0 en la punta; modo 6, sin
  Z, dos caras, `human_shadow.raw` (byte & 0xF0 como alfa). No si y ≤ 0,2 (en el agua), muerto o en la mano de la
  criatura. Animales: puntos de sus datos EBone (2 o 4 quads). openblack: `Renderer::DrawHumanShadows`.
- **Reflejos en el mar** (`GLandscape::Draw` 0x5E490F): `DrawUnderWater` dibuja el objeto espejado en y = 0, sin luz,
  recortado para que solo se refleje lo que está sobre el agua: la mano (0x65A0A0A0) y lo que sostiene, **el cuerpo de
  la criatura** (0x65A0A0D0 + especular 0x30; no lo que lleva), barcos (0xFF303070), objetos físicos (su color).
  Tiburones y SuperVillagers nadando se dibujan **cortados bajo el agua** (`DrawCutByPlane`, 0xFF303070); los peces
  de piscifactoría son sprites. Detalle en la sección siguiente.
- **LOD**: `g_last_distance` es la profundidad lineal del centro de la esfera; S = min((importancia + 1) · radio ·
  LevelOfDetail, 100000). En este ejecutable las dos cargas de LevelOfDetail están anuladas con NOP, S = 100000:
  **siempre LOD 1**, sin fundido ni impostores (con el valor de diseño 0,5, un aldeano cambiaría a 11,5 / 33 / 43 u).
- Trampa de las pruebas: `OPENBLACK_MOUSE_AT` en un borde de la ventana activa el desplazamiento por borde y mueve la
  cámara; usar puntos interiores.

## Reflejos de objetos, sombra de la mano sobre objetos y bajo el agua (hechos)

Informes: `tmp_dis\render\objshadow_notes.txt`, `cut_notes.txt`.
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
  - Barcos (`PetitNavire::PreDraw`): matriz × diag(−1, 1, 1), 0xFF303070, y una segunda parte girada π/2.
  - openblack: `Renderer::DrawObjectReflections` en la pasada de reflejo (lo que sostiene la mano y `HandSystem::GetThrownObjects`),
    `landColourOnly` (modo 3 de `u_objectLight` en `vs_object`) y `clipBelowSea`.
- **Sombra dinámica sobre objetos**: al final de cada Draw (estático 0x80E457, animado 0x81311A, morfable 0x80E74B...),
  si el objeto tiene Flags1 0x40, para cada `ShadowInfo` con alfa ≠ 0, si+0xC = 0 (solo la mano y la criatura; barcos,
  objetos físicos y SuperVillagers ponen 1: solo tierra), que no sea el emisor, y cuya caja si+0x2C {x0, z0, x1, z1}
  toque la caja XZ de la malla (centro ± mitad + posición, sin giro ni escala; `fn_007F9E80`): ZFUNC EQUAL y `fn_0080B050`
  (modo 6, color blanco, u = (Wx − x0)/(x1 − x0), v = (Wz − z0)/(z1 − z0): **proyección vertical**; todo el oscurecimiento
  va en el alfa de la textura, con el mismo fundido de 50–80 radios).
  - Reciben (Flags1 0x40, `Object::Create3DObject` 0x6365F0 si ShadowsOnObjects): todos los objetos salvo árboles
    (0x749FA3), bosques (0x439098), flores, comida mágica (0x5FAAC8), la comida en la mano (pot 12, 0x66D180), cultivos,
    credos, escudos, semillas... Al coger un objeto se guarda y se quita; al lanzarlo se restaura.
  - openblack: `Renderer::DrawHandShadowOnObjects` al final de `MainBlended` (tras los transparentes, para que EQUAL
    encuentre su profundidad), `fs_object_shadow`, `RenderContext::entityInstances` (índice de instancia por entidad y
    `receivesDynamicShadow`). Clave de detalle `shadowsOnObjects` (niveles 3–6).
- **Piscifactorías** (`FishFarm::CallVirtualFunctionsForCreation` 0x52CC10): anillos de radio 2, 4... < 50 × 32
  direcciones, con el aplanado del mar **desactivado** (`[0xC37BF4]` = 0); la primera dirección con altitud 0 en dos
  radios seguidos da el centro (x', y de la granja, z'). 15 peces (`fn_00824740`): sprite `misc0.raw` horizontal (flag
  0x40: quad girado en Y con su x local según el rumbo), media anchura 0,8–1,2, posición centro + (±5, −1..0, ±5), rumbo
  ±π, velocidad 0,5–1,5, giro velocidad·(1 ± 0,1)·0,6283; celdas 8–23 (fotograma += dt·velocidad·25, módulo 15).
  Movimiento `fn_008248E0` (dt ≤ 0,1 s), objetivo del banco `fn_00824DA0` (centro ± 7, temporizador 0,5·distancia);
  a más de 300 no se dibuja, alfa desde 200 (con el desbordamiento de byte del original). Dibujo modo 6 antes del mar.
  - openblack: `FishFarmArchetype`, `ecs::UpdateFishShoals` (tiempo de juego del fotograma), `Renderer::DrawFishShoals`.
    Como `fs_water` compone el mar opaco con la textura de reflejo, "lo que hay detrás del mar" es la pasada de reflejo:
    los peces se dibujan ahí **espejados** (y → −y) y sin prueba de Z, sobre la tierra reflejada (que en el original no
    escribe Z). Lo mismo valdría para los cortes bajo el agua.
  - **Susto** (informe `tmp_dis\fish\fish_notes.txt`): el punto de chapoteo global 0xEA9F40 / bandera 0xEB99F0 lo ponen
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
- **Anillos de agua** (hechos, `fn_005E5100`, tras la tierra y antes de los modelos): por anillo, edad += (int)(ms de
  juego · ritmo), fuera a 700; media anchura max(edad·crecimiento/700, 0,0001) (z × aspecto); alfa (int)((255 − 0,364286·
  (edad % 700))·A) >> 8, RGB del color; giro en Y, celda & 63 de la hoja 8×8 de `smoke.raw`/`smokea.raw`, modo 13
  (SRCALPHA/ONE, sin Z); deriva con el viento si +0x1C. Chapoteo de la mano: (x, 0,2, z), crecimiento 7, ángulo al azar,
  celda 0x30, 0xB0 + tabla de luz[255]. Objeto físico en el agua (0x6466D2): (x, 0,1, z), crecimiento 2·radio, ritmo
  1/radio, celda 0x3F, blanco. openblack: `ecs/WaterRings`, `Renderer::DrawWaterRings`; gancho `OPENBLACK_TEST_SPLASH="x,z"`
  (un chapoteo por segundo; los anillos solo avanzan con el juego en marcha).
- **DrawCutByPlane** (animado `fn_00811C70`; en estáticos es un `ret`): plano (0, −1, 0, 0) → queda lo de **y ≤ 0**, recorte
  por CPU por triángulo, luz 90 + N·L, color 0x303070 opaco, el modo del material. Solo lo usan los SuperVillagers con la
  animación `M_P_Swim2`, los tiburones (`MSH_SHARK_BONED`) y el cebo del puzle de peces (Land 4). **No aplica en Land1**.
  Anillos de agua (`fn_005E5100`, 1024 × 0x38 en 0xEAB7C8): viven 700, media anchura edad·crecimiento/700, alfa
  (255 − 0,364·edad)·A, `smoke.raw` modo 13 horizontal; pendientes hasta que haya nadadores o tiburones.

## Manchas de los animales (hechas)

Informe: `tmp_dis\render\animal_notes.txt`, datos `animal_ebone_dump.txt`.
- En `fn_00812170`: si no es humano, con `IsHumanShadowed` (flag 0x4000000, `SetHumanShadowed(1)` en el Create de cada
  especie; 0 mientras la criatura lo sostiene), y > 0,2 y malla con `ContainsEBone` → `fn_0081FFF0(obj, normal, ebone)`.
- Bloque EBone (836 bytes) tras los de huella (tamaño en +8), UV2, nombre y métricas extra: `u32 tamaño; float m[16][12];
  int32 hueso[16]`. Se usan las posiciones de m[0..3] en el espacio de su hueso: P = objeto × hueso × pos, y = suelo + 0,2.
  Par (0, 1) siempre, par (2, 3) si hueso[2] ≠ −1 (todos los cuadrúpedos: 4 quads). Aves y murciélagos no tienen EBone.
- **Rareza del original**: el primer quad de cada par recibe V = D (construye D + (P1 − P0)/2 pero pasa &D); el segundo
  D + (P0 − P1)/2.
- openblack: `L3DFile::GetEBone`, `L3DMesh::GetBlobPoints`, bucle de animales en `Renderer::DrawHumanShadows`.

## Animales (base mínima para las manchas)

- `CREATE_ANIMAL` (24, "ANNN": tipo, rebaño, pueblo) y `CREATE_NEW_ANIMAL` (25, "ANNNN": + edad) → `fn_00419D10`
  (rebaños y clases en objects-and-resources.md). Malla: `Object::CallVirtualFunctionsForCreation` 0x636BE0 da al
  LH3DObject `GetDetailMesh(2, 1, 0)` (info +0x1FC + 4k: alta, std, baja) y el LOD es siempre 1: **la std** (también
  `GetMesh`); openblack usaba la alta. Escala (`InitialiseScale` 0x417B20): jóvenes
  ageToScale[edad − 1] + FloatRand(0,75·(ageToScale[edad + 1] − s)); adultos 1,05 − FloatRand(0,1). Sin ángulo inicial.
- Land1 crea 116 (palomas 40, gaviotas 22, golondrinas 14, caballos 12, vacas 10, cerdos 7, tortugas 6, murciélagos 5).
  openblack crea solo los terrestres: los voladores faltan hasta decodificar su vuelo (ver objects-and-resources.md).
  Están quietos en la pose de reposo, como los aldeanos (sin IA ni animación de animales todavía).
- openblack: `components::Animal`, `AnimalArchetype`.

## Fundido de pantalla y bandas de cine (hechos)

Informe: `tmp_dis\render\fade_notes.txt` (+ `fade_script.txt`, `fade_widescreen.txt`, `fade_chl_scripts.txt`).
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

## Ríos (hechos, original)

Informe completo con direcciones y pseudo-C++: `C:\Users\diewgarc\dev\tmp_dis\streams\streams.md`.

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
- Sin analizar: el sonido `ATMOS_TYPE_RUNNING_WATER` (`audio/sfx/atmos/stream.sad`) y las cascadas (`GWaterfall`,
  `waterfall3.l3d`).

## Texto: fuentes del original y el mensaje de la mano (hecho)

Informes: `tmp_dis\font\font_notes.txt` (formato), `tmp_dis\numbers\NOTES_numbers.md` (mensajes).
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

## Niebla del mapa (LH3DMist, `fn_007FA300`)

- **Otras nieblas** (API `mists::Submit(const MistDesc&)`, `src/Graphics/Mists.h`): quien tenga sus propios objetos
  LH3DMist (las bocanadas de tormenta de `GWeather::DrawClouds` 0x83FC90) los envía cada fotograma con posición,
  tamaño, color ARGB, rama efecto/normal, k y su contador; `CollectMists` los recorta con la misma esfera y van a la
  misma lista de atrás adelante que las del mapa y los modelos con mezcla (`_frameMists`, `DrawMist(índice)`).

- Misma malla `mist.l3d` (cúpula de radio 20, base en el origen), material de humo 0xEA1ABC (`fn_0080BBD0`, modo 6:
  mezcla SRCALPHA/INVSRCALPHA, color y alfa = textura × difuso, sin escritura de Z, dos caras) y atlas 8×8 que las
  nubes. Creación (`CallVirtualFunctionsForCreation` 0x606420): +0x80 |= 1 siempre; si F4 ≠ 1, +0x8C = F4 y
  +0x80 |= 2 (rama "efecto"). Color N2 en +0x4C (ARGB), +0x50 (especular) = 0.
- **Rotación** (clave): 0x7FA38F copia a la matriz del objeto la 0xEA1C98, que `UpdateCamera` 0x819A62 monta en dos
  pasos. Primero permuta las columnas de la mundo→cámara A = 0xEA1D28: fila i = (A[3i], −A[3i+2], A[3i+1]),
  traslación 0. Y **después** (0x819AC5 `mov ecx, 0xEA1C98`, 0x819AF3 `call fn_007FB3F0`; la otra copia de
  `UpdateCamera`, 0x81A1AD, hace lo mismo en 0x81A265) la **invierte en su sitio**: `fn_007FB3F0` es la inversa de
  la matriz 4×3 (cofactores / determinante, traslación = −t·M⁻¹). Al ser ortonormal, la inversa es la transpuesta, así
  que las filas finales son derecha, −delante y arriba. A usa vectores fila y la matriz del objeto se aplica igual
  (x' = m0 x + m3 y + m6 z, `fn_0084BA90`), luego la fila k es la imagen del eje local k: en glm
  **mat3(derecha, −delante, arriba)**, es decir un **billboard**. X local = derecha de la pantalla, Y local (el eje de
  la cúpula) hacia la cámara, Z local = arriba, así que la cúpula siempre se ve de cara, como un disco del humo, y
  nunca de canto (comprobado emulando 0x819690 + la permutación + 0x7FB3F0 con varias cámaras,
  `tmp_dis\mapa\emu_inv.py`). Su centro está en el suelo, así que el test de Z corta la mitad baja del disco (también
  en el original).
- **Rama efecto** (bit 2; en Land1 todas tienen k = 1, en Land4/Land5 k = 3,78 / 2,64): s = tamaño/(1 + (k − 1)
  (1 − |dy|/|d|)); 0x7FA4DC..0x7FA539 escalan la fila 0 (X local) por el tamaño y las filas 1 y 2 (Y, Z) por s: **escala
  no uniforme**. Luz en (0, 500000, 0), ambiente 0xD2, sin luz de la tierra, atlas V + 0,25 (0x7FA44D: filas 2-3).
- **Rama normal** (0x7FA5B0): las 9 celdas × tamaño. `fn_00801C90` da la luz (tabla[lum] bilineal de las 4 celdas)
  y deja en +0x50 el RGB bilineal de esas celdas (el primer dword leído como D3DCOLOR: rojo = byte azul). `fn_007FEB30`
  aplica la neblina: luz × (256 − trunc((256 − k) t)) >> 8 y especular += round(color de neblina × t). Luego cada
  canal = floor(N2 × luz / 255), alfa = alfa de N2, y la luz de los modelos (luz en (−500000, 500000, −500000),
  ambiente 90). **Sin** el + 0,25 del atlas (0x7FA675: filas 0-1 de `smokea.raw`, picos 171-197; las filas 2-3 llegan
  a 228-248).
- Luz por vértice (`fn_0084BA90`): I = round(255 · n_local · L_local), L_local = normalize(M⁻¹ (Lpos − pos)); con
  escala no uniforme no es la luz de la normal girada.
- Contador +0x84 += ftol(g_game_time_inc · 0,255), módulo 900 solo si pasa de 900; fotograma (contador/20) & 15. Solo
  avanza dentro de Draw, es decir, con la niebla en pantalla.
- Orden: `LH3DMist::AddDrawing` 0x7FA7F0 descarta con `CheckRegionOnScreen` (radio = radio de la malla × tamaño ×
  0,55) y manda la niebla al `LH3DZSorter` (clave |pos − cámara|², callback 0x7FA980), junto a los modelos
  transparentes y los sprites.
- openblack: `Renderer::CollectMists` / `DrawMist` (RendererMists.cpp) entran en la lista de atrás adelante de la
  pasada principal (`DrawPass`, `SortedInstance::mist`); `DrawMists` solo si esa lista no se usa. `vs_cloud`
  recibe `u_cloudLight` (L_local) y `fs_cloud` suma `u_cloudSpecular` (0 en las nubes y en la rama efecto).
  Desviaciones: el contador conserva la fracción (como `Clouds.cpp`), porque sin vsync openblack pasa de 250 fps y
  el paso truncado del original sería 0; la textura alfa no se cuantiza a 4 bits (el original la carga en ARGB4444,
  `a.raw` 0x8375C1: 228 → 238/255), porque cuantizar tras filtrar en el shader haría bandas y `raw/smokea` se
  comparte con otros sistemas.

## Humo de las chimeneas (LH3DSmoke, hecho)

Investigación completa: `dev\tmp_dis\aldeanos\smoke.md` (scripts en `tmp_dis\aldeanos\smoke\`). Todo leído en el
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
  un objeto por chimenea en la lista de atrás adelante de la pasada principal (`SortedInstance::smoke`, clave la
  distancia a la chimenea), sus bocanadas en orden 0..9 con el shader Sprite (`smokea.raw`, tinte premultiplicado,
  ONE/INVSRCALPHA = modo 6, giro −ángulo en el plano de la pantalla). `Abode::presentAtHome` existe pero **nada lo sube
  aún** (los aldeanos no vuelven a casa), así que sin el gancho no sale humo.
- **Desviaciones**: el paso de edad conserva la fracción (el original trunca `(int)(dt·255)` cada fotograma: vida
  3,75 s a 60 fps, 6,3 s a 144 fps y humo parado por encima de 255 fps, y openblack va sin vsync), como las nieblas;
  "en pantalla" es la esfera de la caja del edificio contra el frustum (aproximado); la mano "con malla" = la mano no
  está en el origen (aproximado); la deriva al azar es por fotograma como el original, así que su amplitud depende de
  los fps (como en el original). Falta la cuenta de andamio de los talleres (sin talleres que fabriquen).
- **Gancho**: `OPENBLACK_TEST_CHIMNEY=all` (todas las chimeneas echan humo). Capturas de Land1 de día y de noche en
  `dev\_scratch\mapa\` (`day_near`, `day_close`, `day_far`, `night_far`; `day_far_nohook` sin gancho: sin humo).
