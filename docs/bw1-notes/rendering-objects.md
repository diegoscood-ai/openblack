# Render de los modelos: original frente a openblack

Cómo se dibujan los objetos del mundo: materiales L3D, luz de los modelos, texturas y sprites, manchas de los pies,
reflejos en el mar y cortes por el plano del agua, bancos de peces, sombras de los objetos y de la mano, LOD y el humo
de las chimeneas. El render del mundo (terreno, mar, cielo, neblina) está en [rendering.md](rendering.md); el agua
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

## Ganchos de prueba

En [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración):

- `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fijo en fracción de la ventana (la mano sale en las capturas); usar puntos
  interiores, en el borde mueve la cámara.
- `OPENBLACK_TEST_PHYSICS="x,z,altura,vx,vy,vz[,escala[,n]]"`: rocas que caen, con sus sombras físicas.
- `OPENBLACK_TEST_CUT=1` con `OPENBLACK_TEST_SEA`: corte por el plano del agua.
- `OPENBLACK_HAND_TEST_FISH=1` y `OPENBLACK_TEST_SPLASH="x,z"`: pesca y susto de los peces.
- `OPENBLACK_TEST_CHIMNEY=all`: todas las chimeneas echan humo.

## Fuentes

- `dev\tmp_dis\render\`: `misc_*` (manchas, reflejos y LOD, con emulación Unicorn de `fn_0081FFF0`),
  `objshadow_notes.txt`, `cut_notes.txt`, `physshadow\`, `l3d_wrap_scan.py`, `animal_notes.txt`, `objlight_*`.
- `dev\tmp_dis\fish\fish_notes.txt` (susto y pesca).
- `dev\tmp_dis\aldeanos\smoke.md` y `smoke\` (humo de las chimeneas).
