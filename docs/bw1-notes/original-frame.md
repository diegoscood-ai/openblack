# El fotograma original de B&W (runblack.exe W120, D3D7, LH3D)

Mapa de un fotograma de juego del original, con direcciones: cómo se lanza el fotograma, el orden de dibujo de cada
etapa, los 19 modos de render (estados D3D por tipo de material L3D) y el estado global (proyección, neblina,
borrado, niveles de detalle). El estado de cada etapa en openblack está en [parity.md](parity.md) y los detalles de lo
hecho en [rendering.md](rendering.md).

Todo es **fiel** (leído en el ejecutable) salvo lo marcado **(inferido)**. Volcados de trabajo en
`C:\Users\diewgarc\dev\tmp_dis\render\`: `frame_map.md`, `frame_*.txt`, `frame_A_modes.py/txt`, `frame_B_*.txt`,
`frame_C_*` (y `objlight_*` para la luz de los modelos).

- [1. Bucle del fotograma](#1-bucle-del-fotograma)
- [2. Etapas de dibujo en orden](#2-etapas-de-dibujo-en-orden)
  - [Otros casos: templo, vídeo y 2D](#otros-casos-templo-vídeo-y-2d)
- [3. Modos de render](#3-modos-de-render)
- [4. Estado global](#4-estado-global)
- [5. Comparación con openblack](#5-comparación-con-openblack)

## 1. Bucle del fotograma

`GGame::Loop` 0x54CF20 (decomp `src/Black/Game.cpp`), en cada vuelta:

- ProcessGraphicsEngine 0x54D850 → [ratón, camera->Update (fija el FOV y el plano cercano dinámico),
  GInterface::PreDrawProcess 0x5CE9E0 (colisión de la mano, icono del discípulo), **Process3dEngine 0x54DA80**,
  BMan/editor de cámara de depuración, GInterface::PostDrawProcess (solo actualiza correa y colisión, no dibuja),
  HelpSystem::PostDrawProcess (marca)] → ScriptedScreenShot → **GGame::FlipScreen 0x54D800 → LHScreen::Flip
  0x7DE090** (texto de tiempos, LHMouse::Draw por software, LHFlip 0x7DE580 y luego el *borrado* del fotograma
  fn_0082EE70, ver [4. Estado global](#4-estado-global)).
- Process3dEngine: LH3DRender::StartFrame 0x82F0E0 → vídeo (Bink) → switch field_0x205a28 (0 = mundo,
  1 = ciudadela/templo, 2 = vídeo del hechizo que cae) → fundido / ayuda / influencia → **LH3DRender::FinishFrame
  0x82F460** (vaciado del Z-sort + retrollamadas + EndScene) → 2D de depuración (CreatureMentalEditor, info de correa,
  DisplayHowImpressed, jugadores de la CPU, texto de cuenta atrás) → LH3DAtmos::Render2D (mapa de clima de depuración
  con Lock, **(inferido)**).
- StartFrame: fninit/control de la FPU, tiempo delta (g_delta_time 0xC38134, fps suavizados 0xEC7FC0),
  **BeginScene** (vt+0x14), g_frame++, g_started_frame=1, **reinicio del zsorter fn_0083F3B0**, fn_00813770,
  fn_0085BF00, SetLight/SetProjMatrix solo con T&L por hardware (nunca: start_system fuerza [0xC386E4]=1),
  fn_00821270.

## 2. Etapas de dibujo en orden

Caso 0 = mundo.

| # | Etapa | Función | Qué hace / estados | Orden / recorte |
|---|---|---|---|---|
| 1 | landscape.PreDraw | GLandscape::PreDraw 0x5E3F60 → LH3DIsland::PreDraw 0x7FF2D0 → fn_00877210 | lista de bloques visibles (32×32 bloques de 160), clase de niebla por bloque +0x940 | recorte por frustum y plano cercano con la caja de cada bloque; lista 0xFAA?/0xFA92D8 ordenada por distancia +0x9BC **de delante a atrás** |
| 2 | texturas de sombra | TemporaryShadow::UpdateAll 0x825190 (y fn_00874850 por SuperVillager) | siluetas rasterizadas por CPU en texturas de sombra por objeto (fn_008801D0, **(inferido)**), lista 0xFAA7E0 | — |
| 3 | preparación | LH3DCreature::PrepareForDrawing 0x4ED320 por criatura, CHand::PrepareForDrawing 0x46C550, PSysLightMaps::AddDrawing 0x6CA6E0, LH3DLandscape::TextureUpdateThread 0x871F00 (texturas de bloque: huellas y calcomanías fn_008721A0/fn_00872FA0), LH3DAtmos::Update3D | no dibuja | — |
| 4 | GLandscape::Draw 0x5E42E0 | (detalle en 4a–4o) | | |
| 4a | preparación | cursor 3D (Get3DPointFromScreen), fn_00802550, Windmill::PreDraw, Tree::PreDraw (vaivén por el viento), fn_008296D0 (temporizador de fundido de 8 s, modo 0xF), fn_005E5830 (posición de la mano; luz nocturna de la mano fn_00823460/fn_0086D360; nubes fn_005E25C0 con la clave Clouds) | | |
| 4b | **cielo** | GLandAlignement::DrawSky 0x5E2160 (no en alambre) → fn_0086A330 → tabla de luz fn_00869850 + parámetros de niebla, fn_0086B7F0 (sky_{good,ntrl,evil}_{day,dusk,night}.555 → 3 texturas, modo 2) → fn_0086B010 | **luna** moon.l3d + quad de resplandor aditivo 500 (AdditiveMaterial modo 13), y una copia espejada en Y (reflejo, **(inferido)**); **cúpula** sky.l3d con ZFUNC ALWAYS, 2 pasadas (textura de la alineación base con α255, luego la 2.ª alineación con alfa global (alineación−1)·255), oscurecida por la tormenta, hacia blanco con el relámpago; **sol** sun.l3d fn_0086C140, color 0x957C63, visible de 6 a 18 h, ÷(1+8·nubes). **Sin estrellas.** | el cielo cubre toda la pantalla (el búfer de color no se borra) |
| 4c | **tierra reflejada** (clave LandRef) | fn_007FF4F0, ZWRITEENABLE apagado alrededor (0x5E48B3–0x5E4900) | alturas ×−1, tabla de luz ×0.5, small bump forzado a apagado; solo tierra, sin modelos | lista de bloques espejada |
| 4d | reflejos en el mar | PetitNavire::PreDraw 0x5DFF20 (barcos); DrawUnderWater de la mano vt+0x118 con especular 0x65A0A0A0 mediante la **tabla de modos alternativa 0xC387C8**; objeto en la mano de la criatura; fn_00646FE0 (objetos físicos), fn_00775120 (?), fn_00824B90 (peces de FishFarm), fn_005DFCE0 (UV2 de las huellas, **(inferido)**) | antes del mar. La mano, lo que sostiene, los objetos físicos y los barcos son **reflejos**: `DrawUnderWater` los dibuja espejados en y = 0 y recortados a lo que está sobre el agua (ver [rendering.md](rendering.md); antes se había supuesto que eran sus partes bajo el agua); los peces de FishFarm son sprites bajo el agua | |
| 4e | SuperVillagers | lista 0xEB9A08: Draw vt+0x610 + sombra fn_00874850; los aldeanos que nadan generan anillos de agua | | |
| 4f | resplandor nocturno de la mano | fn_005E3F70: quad aditivo en el suelo de ±60, ZFUNC ALWAYS | | |
| 4g | **mar** | fn_00879930 (se salta si [0xECA664] o en alambre): sky.raw/skya.raw modo 5, filas de 2 px, ZFUNC ALWAYS, sin escribir Z, color = tabla de luz[255], alfa 255→80 entre 7000 y 14000, periodo P = 2000−1800·WaterTiling | | |
| 4h | vórtice | fn_005FF310 → fn_005FFBB0 (LandscapeVortex, **(inferido)**) | | |
| 4i | **tierra** | fn_007FF610: por bloque visible de delante a atrás, bloques de transición fn_00877D20, bloque fn_00874AA0 / SSE fn_007A1800 (bloques en modo 14, tabla de luz por vértice, neblina por software, 2.ª pasada de small bump en modo 14), luego **sombras dinámicas proyectadas** por bloque fn_00878350 (material de sombra modo 6) | de delante a atrás |
| 4j | depuración | fn_0081F820: triángulos del campo de visión de los guiones | | |
| 4k | corazón de la ciudadela | fn_00467360 → CitadelHeart::DrawNow 0x4670D0 | | |
| 4l | anillos de agua | fn_005E5100 sobre 1024×0x38 en 0xEAB7C8 (círculo sprite de GWater, LH3DSprite::Draw directo) | | |
| 4m | **modelos** | fn_005E5CD0: lista de dibujo de objetos (≤3000, GLandscape::DrawObjects 0xD1D28C) de los bloques visibles (dist < VanishObjectDist 100000) + lista global; se rehace según DrawListRebuildCount / 10 turnos / cambio de tierra; con la cámara quieta solo se redibujan los últimos objetos en pantalla. Draw de un objeto (p. ej. MobileObject::Draw 0x518150): luz fn_00801C90 ([rendering.md](rendering.md#luz-de-los-modelos-original-no-es-un-mod)), niebla fn_007FEB30, Game3DObject::AddForDrawing 0x63B5D0 → LH3DObject::AddDrawing fn_00815A70: frustum CheckRegionOnScreen 0x868C80, **LOD** por distancia (23.3f/66.7f/86.7f → LOD 1/2/4, fundido 86.7f–173.3f, recorte más allá si IsDisappear; humanos lejanos → sprite impostor; **inactivo en este ejecutable**: las cargas de LevelOfDetail están anuladas, siempre LOD 1, ver rendering.md), IsGlowing → sprite de resplandor, **NeedSorting (malla con alfa, flag 0x200) → Z-sorter; si no, Draw inmediato** | opacos: orden de bloques, sin ordenar; con alfa: Z-sorter |
| 4n | resto de 5E5CD0 | lista de SuperVillagers 0xEB9A10 (OverrideMaterial con alpha-ref forzado 0xA), lista del juego vt+0x610, marcadores, sprites de Reward, PetitNavire::PostDraw, hand_intro, emisores de sprites fn_008274A0/fn_00823570/fn_00827B90/fn_00828E50 | | |
| 4o | final | fosa de almacén / LandFeature::DrawWorm | | |
| 5 | destellos de pelea de criaturas | LH3DCreature::DrawFightSparkles 0x48DD70 | | |
| 6 | correas | GInterface::DrawAllLeashes 0x5D9310 (fn_008491B0; material de correa modo 15) | | |
| 7 | objetos físicos | PhysicsObject::DrawAll 0x646DE0 (AddForDrawing, DrawOutOfMap) | como los modelos | |
| 8 | mano | CHand::UpdateHeldObject; **CHand::AddDrawing 0x46D100 → Z-sorter** (luz ×1.5); objetos en la mano de otros jugadores con DrawInHand | Z-sorter | |
| 9 | interfaz | GInterface::Draw 0x518640 → fn_005FAF80 (mano mágica), estado vt+0x500 (rastro de gestos, etc.; gestos en modo 13) | | |
| 10 | partículas líquidas | DrawLiquidParticles 0x845C50 → Z-sorter | Z-sorter | |
| 11 | superposición de depuración | GGame::Draw 0x5533B0 (solo si g_game->field_0x14 & 0x4000: barras de alineación y creencia) | | |
| 12 | varios | CreatureLessonChooser::UpdateDraw, EditorIconBase::DrawMouseOver | | |
| 13 | partículas | PSysGlobal::DrawLoop 0x68F5E0 (gestores PSys → PSysManager::AddDrawing 0x6797D0 → Z-sorter), FireFly::DrawAll, Spell::DrawSpells 0x7203F0, GParticleContainer::DrawParticleContainers, GPlayer::DrawPlayers (destellos del jugador, blobs.raw modo 13), TownCentre::DrawAll (DrawPSys) | Z-sorter | |
| 14 | contadores | ValueSpinner::Update/AddDrawing (no durante la cinemática de ayuda) | Z-sorter (**(inferido)**) | |
| 15 | depuración | LH3DStorm::DebugDrawAll, LH3DAtmos::DrawWindField | | |
| 16 | **clima** | LH3DAtmos::Render3D 0x836250: por tormenta (lista 0xFA92D8…, tormentas a dist < 560) y por casilla de 80×80, intensidad de GetWeather en las 4 esquinas; si >5: lluvia fn_008341B0 → retrollamada del Z-sorter 0x833F80 (trazos, AtmosMaterial modo 6 atmos.raw, salpicaduras g_water_drop_cb, cantidad según la clave RainSplash); nieve fn_00834290 → retrollamada 0x834120 (snow.raw modo 9). Relámpagos: LightSheet::DoTheDrawing 0x83E8C0 (modo 13) vía GLightSheet::Draw → Z-sorter. Color de la lluvia = (base de la tabla de luz/2 & 0x7f7f7f)+0x7f7f7f. | Z-sorter por distancia² de la casilla | |
| 17 | campo de fuerza de la cámara | ForceField de CameraModeNew3 (solo con la marca) | | |
| 18 | nombres de aldeanos | VillagerName::AddDrawing → Z-sorter | | |
| 19 | fundido | GScript::ProcessFade 0x6EB9D0 o Temple::UpdateFade 0x794280 → fija el color de fundido de pantalla [0xFA51D8] (se dibuja en FinishFrame) | | |
| 20 | ayuda 3D | HelpSystem::Draw3D 0x5C59A0 (personaje de ayuda, flechas) | | |
| 21 | ClearLight 0x5E57B0 | quita la luz de la mano (fn_00822F90/fn_00823780/fn_0086D460) | | |
| 22 | giros de poder | PowerSpinRunner → Z-sorter (PowerSpin::Draw) | | |
| 23 | anillo de influencia | InfluenceCircle::Draw 0x826C90 (en el mundo solo si la cámara está a más de 100 de altura: alfa 0→120 entre 100 y 200; modo 6, sin culling, wrap, desplazamiento UV 0.0001/−0.0002 por ms), Draw3DWorldTriangle **inmediato** | | |
| 24 | **FinishFrame 0x82F460** | (a) **vaciado del Z-sorter fn_0082F280** de atrás a delante (clave = dist² a la cámara, máx. 2048 entradas); (b) retrollamadas "antes" (prioridad ascendente, 0xEC8130): FallingSpell (0x526480), HelpDude 0x5C2E30 (100), CameraModeNew3 0x4562E0 (1000); (c) fn_0086BB60 posproceso del cielo (sol y destello de lente, **(inferido)**); (d) **quad que reinicia la Z**: quad de pantalla completa FVF 0x1C4, z=1, rhw=0, color 0, material [0xEDD494] modo 1, ZFUNC ALWAYS → Z=1 en toda la pantalla; (e) bandas de cine si [0xEB9950] (rectángulos 2D en cola fn_0081E590, altura fn_0081E8B0); (f) retrollamadas "después": HelpDude 0x5C2E10 (100), **vaciado de la cola de rectángulos 2D 0x81E7D0→fn_0081E3C0 (10000; modo 1, ZFUNC ALWAYS, sin escribir Z)**, HelpText 0x5CD020 (20000: cuadros de texto de ayuda), LHVideoPlayer::thedraw 0x844E30 (0x8000), start_system 0x6424E0 (0xA0000); (g) fn_00836200 clima de depuración; (h) **fundido de pantalla fn_0086FEE0**: quad de pantalla completa de color [0xFA51D8], modo 1, ZFUNC ALWAYS, si alfa≠0; luego los rectángulos de las bandas y el reinicio del fundido; (i) EndScene (vt+0x18) | | |

### Otros casos: templo, vídeo y 2D

- Caso 1 (ciudadela/templo): Update3D, LH3DSky::g_b_we_are_inside_citadel=1, TemporaryShadow::UpdateAll, DrawSky
  (sin sol), Temple::Draw 0x794370 (luces propias con fn_0081E1F0 SetLight, salas; claves de detalle Citadel*),
  Temple::Update, partículas líquidas.
- Caso 2: vídeo de FallingSpell + partículas líquidas.
- Vídeo: LHVideoPlayer::DrawToScreen (bandas 16:9, fundido de alfa; FallingSpell 80).
- HUD y 2D: B&W no tiene HUD clásico; el 2D son los cuadros de texto de ayuda (retrollamada), tooltips y texto con
  GatheringText (fuentes en modo 6), rectángulos en cola, fundido, bandas y el cursor por software en Flip
  (**(inferido)**: el cursor suele ser la mano 3D).

## 3. Modos de render

Tabla 0xC38728, 19 entradas {fn, flag}; el tipo de material L3D es el índice del modo.

Común a todos: color = TEXTURE×DIFFUSE; ALPHAFUNC GREATEREQUAL fijado una sola vez (0x82CBA6); ALPHAREF = mat+4 (o el
forzado [0xECA65C] si [0xECA658]); bit 0 del byte +5 del material → cull NONE, si no CCW; bit 2 o g_b_need_tilling →
WRAP, si no CLAMP; caché de modo [0xC38718] (se reinicia a 0x14 cada fotograma). Ningún modo toca niebla, especular,
luz ni la etapa 1.

| # | fn | Estados | Tipo L3D / usos |
|---|---|---|---|
| 0 | 82D470 | sin textura, opaco, escribe Z | Smooth |
| 1 | 82D5C0 | sin textura, SA/ISA, escribe Z, etapa 0 sin tocar | SmoothAlpha; rectángulos 2D, fundido, quad que reinicia la Z |
| 2 | 82D820 | con textura, opaco, α=tex | Textured; cúpulas del cielo |
| 3 | 82D920 | SA/ISA, α=tex×diff, escribe Z | TexturedAlpha |
| 4 | 82DC20 | SA/ISA, α=tex, escribe Z | AlphaTextured (edificios, mano; la muñeca se desvanece con el alfa) |
| 5 | 82DD90 | SA/ISA, α=tex×diff, escribe Z | AlphaTexturedAlpha; mar |
| 6 | 82DF10 | como 5, sin escribir Z | …AlphaNz; fuentes, influencia, human_shadow, sombras dinámicas, lluvia atmos, vídeo |
| 7 | 82D6F0 | sin textura, SA/ISA, sin escribir Z | SmoothAlphaNz |
| 8 | 82DAA0 | como 6 | TexturedAlphaNz |
| 9 | 82E080 | SA/ISA **+ prueba de alfa**, α=tex, escribe Z | TexturedChroma (árboles 0x96), nieve |
| 10 | 82E830 | aditivo SA/ONE + prueba de alfa, α=tex×diff, escribe Z | …AdditiveChroma |
| 11 | 82E9C0 | como 10, sin escribir Z | …AdditiveChromaNz |
| 12 | 82EB50 | aditivo SA/ONE, escribe Z | …Additive |
| 13 | 82ECD0 | aditivo SA/ONE, sin escribir Z | …AdditiveNz: gestos, relámpagos, resplandor de la luna, destellos, fuego, humo |
| 14 | 82DD90 | = 5 (flag 0) | bloques de tierra, small bump |
| 15 | 82E470 | SA/ISA + prueba de alfa, α=tex×diff, escribe Z | TexturedChromaAlpha; correa |
| 16 | 82E6A0 | como 15, sin escribir Z | caché de texto |
| 17 | 82D820 | = 2 | — |
| 18 | 82E2A0 | solo Z (ZERO/ONE) + prueba de alfa | ChromaJustZ; vórtice, burbujeo (fizz) |

Tabla alternativa 0xC387C8 (fundido de objetos / DrawWithGlobalAlpha, reflejo de la mano con DrawUnderWater):
0,1→D5C0; 2,3,17→D920; 4,5→DD90; 9→E470 (ref escalada por el alfa del objeto); el resto igual. Palabra de flags: no se
encontró quién la lee.

## 4. Estado global

- **Proyección.** Sin T&L por hardware: transformación por CPU a XYZRHW. FOV horizontal [0xEA1DD0], por defecto 70°
  (1.22173), aspecto ancho/alto [0xE839EC]; sz = 1−near/z, rhw = near/z, **sin plano lejano**. Plano cercano
  [0xE839E0] dinámico según la altura de la cámara sobre el suelo: 0.3 + 0.16·h limitado a [0.3, 3.5] (0.1 en las
  cámaras de trayectoria, 0.2 en la ciudadela con fov 90°).
- **Niebla.** Sin niebla D3D. Neblina por software (clave Fog [0xC37204]): inicio y fin según el tipo de cielo
  (mediodía/medianoche 400→900, atardecer 100→800; tormenta →15/350), color = base de la tabla de luz/3; por vértice
  oscurece el difuso hacia "dark" y suma el RGB de la niebla al especular (**(inferido)**); tierra por bloque +0x940,
  objetos fn_007FEB30.
- **Borrado.** En Flip, después de presentar, solo cada ~2000 ms (2 fotogramas) en juego: negro 0xFF000000, z=1. La Z
  se reinicia cada fotograma con el quad de FinishFrame; el búfer de color depende del cielo.
- **Gamma y filtrado.** Sin gamma. LightBoost (solo en detalle personalizado) cambia el divisor de la tabla de luz.
  Filtrado bilineal, sin mips, sin AA, tramado activo ([rendering.md](rendering.md)).
- **Nivel de detalle.** fn_00823AD0 desde start_system 0x643026; nivel = detailidx del registro (<5), si no **4**;
  5 = personalizado (fn_008237B0 lee cada clave). Tabla 0x9A3704 (L0..L6):
  - En vivo: LevelOfDetail (muerta), UseSmallBump 1 en todos, Clouds/CloudShadows 0001111, WaterTiling
    0/.2/.4/.6/**.8**/.5/1 (nivel 4 → periodo del mar **560**; 200 solo en L6), LandRef 0001111, CitadelReflections
    0000111, CitadelLightmaps 0111111, CitadelGlows/People 0011111, CitadelVolumeLight 0001111, RainSplash
    0,0,3,5,8,8,8, LightBoost 0, Fog 0001111.
  - Solo al arrancar: Weather 0001111, Light 0011111 (flag de luz dinámica de objeto 0x20), ShadowsOnObjects 0001111
    (flag 0x40), UseHighTexture 0000111 (texturas de tierra de 256 frente a 128 px), UseMultiLayerOnLandscape
    invertida (1 solo en L0), FixeLand.
  - HardwareTnL/MaxObjectDistance se leen pero se ignoran. Las distancias de detalle de la tierra 0xE9C508 dependen
    del número de texturas en VRAM. VanishObjectDist 100000 (guion).

## 5. Comparación con openblack

La tabla de paridad al día es [parity.md](parity.md). La lista que había aquí (2026-09-29) se ha quitado porque buena
parte ya no era cierta: daba como ausentes en openblack el sol y la luna, las nubes, los reflejos de objetos en el mar,
la neblina, las sombras dinámicas, las partículas, el fundido y las bandas y los anillos de agua, y como distintos el
FOV, el orden de transparentes y sprites, el LOD y la prueba de alfa de TexturedChroma; hoy todo eso está hecho (igual
o aproximado al original) según parity.md (commits 967d4c54, ac58688b, 5712a06f, cd6e99be, 1217d68f).

Sigue igual la única diferencia inofensiva: openblack borra el color a 0x274659 (el original no lo borra; no se ve).
Lo que falta según parity.md: lluvia, nieve y relámpagos; correas, gestos y anillo de influencia; vídeo Bink; barcos;
la criatura (reflejos y sombras).

Datos del original que solo estaban en esa lista:

- TexturedChroma = prueba de alfa ≥0x96 **y** mezcla SA/ISA.
- Las sombras dinámicas caen sobre la tierra y, con ShadowsOnObjects, sobre los objetos.
- Huellas y calcomanías horneadas en las texturas de bloque.
- Nubes y sombras de nubes desde el detalle 3.
