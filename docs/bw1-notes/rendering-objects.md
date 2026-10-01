# Render de los modelos: original frente a openblack

Cómo se dibujan los objetos del mundo: materiales L3D, luz de los modelos, texturas y sprites, manchas de los pies,
reflejos en el mar y cortes por el plano del agua, bancos de peces, sombras de los objetos y de la mano, LOD, el humo
de las chimeneas, los objetos que miran a la cámara (billboards), las texturas animadas por fotogramas y las mallas pegadas al suelo. El render del mundo (terreno, mar, cielo, neblina) está en [rendering.md](rendering.md); el agua
como juego, en [water.md](water.md).

- [Mezcla de materiales L3D](#mezcla-de-materiales-l3d)
- [Luz de los modelos](#luz-de-los-modelos)
- [Repetición o recorte de texturas](#repetición-o-recorte-de-texturas)
- [Sprites en el orden de transparentes](#sprites-en-el-orden-de-transparentes)
- [Manchas de aldeanos, reflejos de objetos y LOD](#manchas-de-aldeanos-reflejos-de-objetos-y-lod)
- [Submallas de física y de LOD 0](#submallas-de-física-y-de-lod-0)
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
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

Estado: todo lo de esta página es **fiel** (original, no un mod) salvo lo que se marca **(aproximado)**, las
desviaciones que se dicen en cada sección y lo que está en [Pendiente](#pendiente).

## Mezcla de materiales L3D

**Fiel** (del original, no es un mod).

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

## Luz de los modelos

**Fiel** (del original, no es un mod).

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

## Repetición o recorte de texturas

**Fiel** (hecho).

- `fn_00850FC0` 0x851779: tras poner el modo, `SetD3DTillingOn` si `g_b_need_tilling` (0xECA614) o el bit 2 del byte
  +5 del material; si no, `SetD3DTillingOff` (CLAMP). `g_b_need_tilling` lo copia cada Draw del objeto 3D de su bit
  0x200 de Flags1 (vt+0xE4 = `fn_007F9B30`), y ese bit solo lo pone el setter vt+0xE0 desde mallas de partículas
  (`ParticleMeshCreatorAnimTextured`, `ParticleVolBlendMeshCreator`): los objetos del mundo dependen solo del material.
- `AllMeshes.g3d`: 1675 primitivas con el bit, 161 sin él; ninguna de estas tiene UV fuera de 0..1, así que el recorte
  solo cambia el filtrado bilineal en los bordes de la textura. openblack: `Primitive::wrap` y flags de sampler en
  `Renderer::DrawSubMesh`. Script: `tmp_dis\render\l3d_wrap_scan.py`.

## Sprites en el orden de transparentes

**Fiel** (hecho).

`LH3DSprite::Draw` también va al Z-sorter. En la pasada principal, los `components::Sprite` (polvo, partículas de coger,
destellos, luciérnagas...) entran en la lista de atrás a delante de `MainBlended` con los modelos transparentes, por la
distancia a la cámara; antes se dibujaban antes que todos ellos y un modelo transparente detrás los tapaba.

**Lo que no es un sprite de un efecto va en la misma cola** (hecho, 2026-10-01). `PSysManager::AddDrawing` 0x6797D0
encola **un solo** Z-objeto por efecto, con clave |origen − g_camera|² (0x6797E5..0x679834) y retrollamada
`fn_00679860`. Esa retrollamada dibuja el gestor entero: `fn_006798B0` recorre cada colección, dibuja sus átomos con
`fn_00679920` (que llama a `vt+0xFC` `DrawAt`, 0x67CBA0 para un átomo `ZR_SurfRevol`) y después su cadena. La cadena
toma la rama «dibujar ya» `fn_0067B370` (0x6798DD) porque `[0xC0215D]`, copiado del +0xAE del gestor, es 0; solo el
camino directo `Draw_(float, bool)` (`fn_00679840`) con la marca puesta le da a la cadena un Z-objeto propio por
`fn_0067B380` (clave = la articulación central).

Así que los discos de `ZR_SurfRevol` (el charco del teletransporte, el disco del dispensador) y las cintas de las
cadenas **no** son pasadas aparte: `Renderer::CollectPSysSurfaces` y `CollectPSysChains` les ponen la clave de su
efecto y el bucle de `sorted` los dibuja, justo detrás de los sprites de ese efecto. La ordenación es
`std::stable_sort`, que es el desempate del original: `NewZObject` 0x83F310 inserta una entrada nueva delante de la
primera con clave *estrictamente* menor (0x83F36A..0x83F376), así que con claves iguales se dibuja antes la que entró
antes. **(aproximado)** el original intercala los átomos y la cadena dentro de la colección; aquí van primero los
sprites del efecto, luego sus superficies y luego sus cintas.

Esto arregla el disco del dispensador tapado por la burbuja de la bola de un uso: `OneOffSpellSeed::Draw` 0x518E90
encola la burbuja con su punto de orden empujado hacia la cámara por su radio, así que su clave es **menor** (más
cerca) que la del efecto; la burbuja es de modo 12, aditiva y **escribe Z** (0x82ECA6, fiel), de modo que el disco,
dibujado después, fallaba la prueba de profundidad. Ahora el disco va antes. Traza: `OPENBLACK_ORB_TRACE=1`.

**La lluvia sigue siendo un grupo aparte, a propósito.** `LH3DAtmos::Render3D` encola un Z-objeto **por casilla** que
llueve (`fn_008341B0`, llamada 0x83427F), con clave |(x, `GetAltitude`(x, z), z) − g_camera|² y la casilla y el alfa
empaquetados en el dato de usuario K; no pasa por `PSysManager::AddDrawing`, así que darle la clave de un efecto sería
falso. Sigue siendo el hueco H3 de `dev\tmp_dis\unify2\lh3d_zsorter_openblack.md`.

## Manchas de aldeanos, reflejos de objetos y LOD

**Fiel**. Informe: `tmp_dis\render\misc_*` (con emulación Unicorn de `fn_0081FFF0`).
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
con `drawAll`), la sombra estática (`DrawStaticShadowPass`), la sombra de la mano, `PhysicsShadows`, `FragMesh` (trozos
de edificios), `PartialBuild` y el picado (`L3DMesh::RayIntersect`). Comprobado el 2026-10-01 (capturas
`dev\_audit\magic\prism_*.png` con el mod `test.miracle-dispensers`): el prisma no aparece en ningún dispensador.

## Reflejos de objetos y sombra de la mano sobre objetos

**Fiel** (hechos). Informes: `tmp_dis\render\objshadow_notes.txt`, `cut_notes.txt`.
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
    r = `PhysOb::Radius`; los lanzados de la mano que no estén en física, con el radio de la caja),
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

## Cortar por el plano del agua (`DrawCutByPlane`)

**Fiel** (hecho, W11).

- **DrawCutByPlane** (vt+0x11C: animado `fn_00811C70`; estático `LH3DStaticObject` vt 0x9A2974 = `fn_0080C050`, **no**
  es un `ret`: el `ret` `fn_00815F90` es solo la vtable base 0x9A2748): plano de `fn_00822560`, (0, −1, 0, 0) → queda
  lo de **y ≤ 0**, (0, 1, 0, 0) → y ≥ 0; recorte por CPU por triángulo (`fn_0081D2C0`), luz por vértice `fn_00858BA0`:
  I = 255·(L·n) con la luz 0xF03140 (la de `fn_0084BA90`), I < 0 → 90, si no 90 + (255 − 90)·I >> 8
  (`[0xC39264]` = 90); rgb = color.rgb·I >> 8, A = color.A, el especular del objeto; el modo del material. Lo usan los
  SuperVillagers con `M_P_Swim2`, los tiburones (`MSH_SHARK_BONED`: la parte de abajo antes del mar en 0xFF303070 y la
  de arriba en su Draw con tabla[255]) y la red del puzle de peces (Land 4, estático). **No aplica en Land1**.
  openblack (W11): `L3DMeshSubmitDesc::cutByPlane` (−1 / 1) + `cutColour` → modo 4 de `u_objectLight` en `vs_object`
  (la misma cuenta entera; I se guarda con `fistp` en 0x858CDF:
  redondeo al más cercano, mitades a par, no truncado) y descarte por fragmento en `fs_object` (`u_objectClip.x` < 0 descarta
  y > 0) en vez del recorte por CPU; `mirrorInSea` espeja la malla en y = 0 para el destino del reflejo (el culling
  vuelve a CCW). `Renderer::DrawCutByPlane(vista, entidad, keep, argb, espejo)` y `DrawCutBelowWater` (en la pasada
  de reflejo, antes de los peces: las entidades con `components::CutByPlane`). La parte de arriba la llama el dueño
  del objeto en lugar de su dibujo normal: `CutByPlane::drawAbove` (los tiburones) hace que la pasada normal salte esa
  instancia y `Renderer::DrawCutAboveWater` la dibuje con keep = 1 y `LandLightTable::GetRaw(255)`, en la
  pasada principal tras las mallas instanciadas. `DrawCutByPlane` usa la pose de `SkeletalAnimation` si la hay.
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

- **Puzle de los peces** (Land 4, la red `FishPlot` y sus bancos con cebo): en [water.md](water.md#puzle-de-los-peces).

## Sombras de los objetos físicos

**Fiel** (hechas). Informe: `tmp_dis\render\physshadow\`.
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

## Sombra dinámica de la mano

**Fiel** (hecha).

- Silueta de la mano (las dos instancias del mesh) en un R8 de 64×64 (`RenderPass::DynamicShadow`,
  `vs_dynamic_shadow_instanced`), proyectada desde la luz 200 unidades encima de la mano sobre el plano del suelo,
  en una caja de ±2 radios; `fs_terrain` la cuelga vertical y oscurece × (1 − 8/15 · cobertura · fundido), fundido
  entre 50 y 80 radios desde la cámara; hacia el agua se funde con el color de vértice 0 de las altitudes ≤ 1
  (`fn_00878350`, ver [Costa](rendering.md#costa)).

## Animales: manchas y malla

Movido a [animals.md](animals.md#manchas-y-malla-de-los-animales): las manchas de EBone de los animales
(`fn_0081FFF0`) y la malla / escala de creación.

## Humo de las chimeneas (LH3DSmoke)

**Fiel** (hecho; desviaciones al final). Investigación completa: `dev\tmp_dis\aldeanos\smoke.md` (scripts en `tmp_dis\aldeanos\smoke\`). Todo leído en el
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

## Objetos que miran a la cámara (billboards)

**Fiel**, salvo lo marcado. Todo lo que se orienta hacia la cámara pasa por una sola API, `graphics::billboard`
(`src/3D/Billboard.{h,cpp}`, solo matemática glm), con una función por cada modo del original.

**Convenios.**
- LH3D usa vector fila (p' = p·M, fn_0084BA90). La fila k es la imagen del eje local k; en glm es la columna k, con la
  misma memoria.
- Los giros de LH3D van al revés que `glm::rotate`: `SetAngleY(a)` 0x674360 (filas (c,0,s) / (0,1,0) / (−s,0,c)) es
  `glm::rotate(−a, Y)`. Lo mismo pasa con RotateY 0x5198F0, fn_0086AFA0 y UpdateRuleRotatePrincipalAxis 0x6A1150, que
  giran en el sitio y en glm van a la izquierda.

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
| `YawToEye` (modo C) | código inline: `TownCentre::DrawPSys` 0x69BE76..0x69BE8A, fn_00466BB0, `TownDesireFlags::Draw` 0x746BFC, fn_00719E90, `ScriptHighlight::Draw` | θ = atan2(ojo.z − p.z, ojo.x − p.x) + π/2 ([0x8C78D8]); ejes Ry(θ). El +Z local va del ojo al objeto | nadie (columnas de influencia, banderas de deseo, ShowNeeds y ScriptHighlight están sin portar) |
| `ParticleYaw` (C') | `Particle3DObj::DrawAt` 0x679FD0, FaceCamera +0x4D, 0x67A032..0x67A1C3 | θ = atan2(d.z, d.x) − atan2(r2.z, r2.x), con d = p − ojo en XZ; r0' = c·r0 + s·r2, r2' = c·r2 − s·r0; r1 × HeightStretch | mallas de PSys con FaceCamera (`PSys/Creators/Mesh.cpp`) |
| `FullSprite` (D) | FaceCameraSprite +0x4C, 0x67A250..0x67A451 | Identidad × escala. Luego cada fila (x, y) := (cos φ·x + sin φ·y, cos φ·y − sin φ·x), con φ = π/2 − atan2(d.y, \|d.xz\|) (0x67A367), y después fn_0067A4A0(ψ), con ψ = atan2(d.z, d.x). El +Y local mira al ojo y la Z queda horizontal | `Mesh.cpp`, después de FaceCamera como en el original; ningún SF lo activa |
| `LookAtCentre` | la burbuja: fn_00518720, desde `OneOffSpellSeed::Draw` 0x518E90 | d = W − ojo, con W = el centro de la caja en el mundo (0x518746..0x5187B8). Si \|d.x\| y \|d.z\| son < 1e-4 (el double [0x8C79D8]), d.x pasa a ±1e-4 (0x518875..0x5188B4). D = normalize(d), U = normalize(Y − (Y·D)D). Las filas (U×D, −D, U) salen de invertir con fn_007FB3F0 (0x518B0C). Luego M = T(−c)·R·s y traslación W − c·R·s (fn_00518B90, fn_00518BF0, fn_0044CF90). Tras el empujón de 1e-4, d y U nunca son cero, así que la prueba de ceros de 0x5188BC..0x5188ED no salta nunca y no se porta | nadie todavía. `Magic/Core/OneOffSpellSeed.cpp` (de Milagros, sin commitear) sigue con su copia sin el empujón; pasará a `LookAtCentre` después de su HEAD |
| `BandToEye` | las bandas de potencia: fn_0051A830 (si el byte [0xBE8E8E] = 1, que nadie escribe), desde `DrawSpellGraphic` 0x51A773 | d = T − ojo, con T la traslación de la banda; el mismo empujón de 1e-4 que la burbuja; D = d / sqrt(d.y² + d.z² + d.x²), U = normalize(Y − (Y·D)D) con Y = (0, 1, 0) en 0xCC62C0. La matriz de columnas (−D, U, U×D) se invierte con fn_007FB3F0 (0x51AB5A), así que sus filas son −D, U, U×D. Luego M = M·R con fn_0046D9D0 y se repone T. En glm, R·(giro y escala de la banda) | `Worship/SpellSeedGraphic.cpp` (las bandas de la bola y de los iconos) |
| `MoonBasis` / `MoonModel` / `MoonHalo` (E) | fn_0086AC60 y fn_0086A930, leídas enteras | Ver la luna, debajo | `Renderer::DrawMoon` |
| `MistBasis` / `MistShrunkSize` (F) | `LH3DMist::Draw` fn_007FA300 0x7FA38F, rama del efecto 0x7FA483..0x7FA539 | Las 9 celdas son 0xEA1C98. Con el efecto, la fila 0 lleva el tamaño y las filas 1-2 tamaño / (1 + (k − 1)(1 − \|d.y\|/\|d\|)), sin límite. **(aproximado)** 1/\|d\| se saca con `std::sqrt` y no con la tabla de InverseSquareRoot 0x841170. **(inferido)** con d = 0 devuelve el tamaño | nieblas (`RendererMists.cpp`, `mists::Submit`) y nubes (`Renderer::DrawClouds`) |
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
FaceCamera, cadenas y la burbuja. Detalle por archivo: `dev\tmp_dis\unify\U1_changes.md`.

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
- Cargadores: `LoadStackedFrames` (GetBitmap 0x6A9D40: fotogramas Pitch × Pitch apilados, RGB o grises) y
  `SampleStackedFrame` (openblack, bilineal); `LoadGif` y `GifDelayMs` para mods (stb; los retrasos de menos de 20 ms
  valen 100 ms, como en los navegadores).
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
- (aproximado) `graphics::lh3d::Random` (src/3D/LH3DRandom.h) es Random 0x81D180 con un `rand()` de MSVC propio que
  empieza en la semilla 1. Lo comparten las nieblas del mapa, las de PSys y las bocanadas de tormenta. El original usa
  la serie de `rand()` de todo el programa, sembrada con srand(time).
- (aproximado) TownBelief toma g_game_time_inc como el tiempo real entre dos fotogramas.
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

## Pendiente

- Luz de los modelos: neblina (`fn_007FEB30`), tintes (veneno, fuego, `fn_0080BF10`), color de ventanas de noche, luz de la mano
  estampada en la tierra (`light_hand.raw`, `fn_008229B0`). Revisar si sigue vigente: la neblina de los modelos ya se
  aplica ([rendering.md](rendering.md#neblina-de-distancia-original-detalle-fog-niveles-36)) y las ventanas de noche y
  la luz de la mano estampada están en [day-night-weather.md](day-night-weather.md).
- Bancos de peces: el tono de los sonidos, el texto de ayuda ("Pick up") y los pescadores.
- Sombras de los objetos físicos: el filtro 2×2 de los árboles y el rehorneado de la sombra estática al salir un árbol
  o un MobileObject.
- Reflejos y sombras dinámicas de la criatura y de los SuperVillagers (no existen aún en openblack).
- Humo de las chimeneas: nada sube `Abode::presentAtHome` (los aldeanos no vuelven a casa) y falta la cuenta de
  andamio de los talleres.
- Billboards:
  - capturas del original para confirmar V4-a (la luna en un borde de la pantalla) y V4-b (un rayo en la mano);
  - portar los usuarios de `YawToEye` (columnas de influencia, banderas de deseo, ShowNeeds, ScriptHighlight), de
    `PlaneOfMatrix` (fn_008274A0) y los HelpDude (base (R, U, D), HelpDude::Update1 0x5BE302);
  - el oy heredado por el vapor y el humo del fuego;
  - la burbuja con `LookAtCentre`, después del HEAD de Milagros (el trozo está en `U1_changes.md`);
  - la aprobación de D2b, D2c, RotateAxis, RandomAngle y el corte por near.
- Texturas animadas:
  - portar los usuarios que solo tienen reloj (InfluenceCircle, Gooloo, GoldenShower, las correas y la habitación de
    la criatura, HelpDude, el cursor 3D, JCSpecial) y HandGlow / fn_0083F270;
  - el resto de la rama de las fiolas de 0x519AD0 (bote, aplastamientos del switch 0x519D76);
  - en las cadenas, el suavizado por puntos medios (0x67BD43..0x67BE78, con [0xD4EC14] = 0) y UseDynamicLighting
    (la interpolación de SurfRevol ya está, `frame_anim::RotatingUvClock`: GameUpdate 0x6C8BC0 entero);
  - HandEffects como efectos PSys de verdad;
  - una captura del rayo en la mano en el original, para comparar el ancho de las cintas;
  - capturas antes y después (lista de escenas en `dev\tmp_dis\unify\U2_changes.md`).
- Mallas pegadas al suelo:
  - capturas antes/después del escudo físico, el disco del dispensador, el teletransporte, el arca y el dinosaurio
    de Land 4, la marca de la explosión de rayo y el cráter (escenas en `dev\tmp_dis\unify\U3_changes.md`);
  - portar la cortina del anillo de influencia (`InfluenceCurtain` ya está), la correa de la criatura (`fn_008491B0`,
    `fn_00848600` / `fn_00848830`) y los quads de la criatura (`fn_0081F360`) cuando tengan casa en openblack;
  - la entrada del templo, el templo a medio hacer, el andamio y los demás usuarios de `GetExtraPos`;
  - el `SmokyStuff` de modo 1 de las marcas del suelo.

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
  `dev\tmp_dis\unify\U1_changes.md`.
- Texturas animadas: `OPENBLACK_TEST_ONESHOT="<semilla>,x,z[,pu]"` (la burbuja; con pu, las bandas que miran a la
  cámara; con una fiola de criatura, su hoja 8×4), `OPENBLACK_TEST_DISPENSER`, `OPENBLACK_TEST_FIRE` (llamas),
  `OPENBLACK_HAND_TEST_FISH=1` y `OPENBLACK_TEST_SPLASH` (peces y anillos), `OPENBLACK_TEST_SEED=LIGHTNING_BOLT`
  (cadenas), `OPENBLACK_TIME_OF_DAY=22` (faroles), `OPENBLACK_TEST_WEATHER` (bocanadas de tormenta),
  `OPENBLACK_TEST_CHIMNEY=all` (humo). La lista de escenas está en `dev\tmp_dis\unify\U2_changes.md`.
- Orden de transparentes y burbuja: `OPENBLACK_ORB_TRACE=1` escribe por fotograma el sitio y la clave de cada disco
  `ZR_SurfRevol` y de cada burbuja de bola de un uso en la lista ordenada, con la fase, el fotograma, el `[1][3]`
  empaquetado, el alfa y el recorte de cada burbuja (ver
  [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)). Escena: un dispensador con
  `OPENBLACK_TEST_DISPENSER` y la cámara fija con `OPENBLACK_CAMERA_LOCK`.
- Mallas pegadas al suelo: `OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,x,z,..."` con `OPENBLACK_TEST_SHIELD_SHOT` (el escudo
  físico se funde con la tierra), `OPENBLACK_TEST_DISPENSER` y `OPENBLACK_TEST_TELEPORT` (los discos cortados),
  `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` y `OPENBLACK_TEST_EXPLOSION_SHOT` (la marca del suelo; `UR_Explosion` solo
  está en `SF_BeamExplosion*`), `OPENBLACK_TEST_TUG`
  (el cráter); `OPENBLACK_SPELL_TRACE=1` escribe `Explosion: ground mark`. La lista de escenas está en
  `dev\tmp_dis\unify\U3_changes.md`.

## Fuentes

- `dev\tmp_dis\render\`: `misc_*` (manchas, reflejos y LOD, con emulación Unicorn de `fn_0081FFF0`),
  `objshadow_notes.txt`, `cut_notes.txt`, `physshadow\`, `l3d_wrap_scan.py`, `animal_notes.txt`, `objlight_*`.
- `dev\tmp_dis\fish\fish_notes.txt` (susto y pesca).
- `dev\tmp_dis\aldeanos\smoke.md` y `smoke\` (humo de las chimeneas).
- `dev\tmp_dis\unify\billboard_original.md` (los modos del original, con su verificación), `billboard_openblack.md`
  (inventario de openblack) y `U1_changes.md` (la migración).
- `dev\tmp_dis\unify\animtex_original.md` (los relojes del original, con su verificación), `animtex_openblack.md`
  (inventario de openblack) y `U2_changes.md` (la migración).
- `dev\tmp_dis\unify\drape_original.md` (los algoritmos del original, con su verificación), `drape_openblack.md`
  (inventario de openblack) y `U3_changes.md` (la migración); `dev\tmp_dis\morph\morph_notes.txt` (UpdateMelting).
