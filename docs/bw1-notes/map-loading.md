# Carga del mapa y funciones del guion

Qué hace el guion del mapa (`Land*.txt`, LHScriptX) y los CREATE de CHL al cargar una tierra: creación de objetos,
nieblas, rebaños y animales, datos de simulación (pueblos, clima, arenas), piscifactorías, objetos del guion (farolas,
hogueras, árboles muertos, puertas, ciudadela planeada), `IsOkToCreateAtPos` y `BUILT_PERCENTAGE`. Todo **fiel** (leído
en runblack.exe) salvo lo marcado **(inferido)**, *desviación* o **pendiente**.

- [Creación desde CHL](#creación-desde-chl-create-27--create_with_angle_and_scale-252)
- [Niebla del mapa (CREATE_MIST)](#niebla-del-mapa-create_mist)
  - [Dibujo (LH3DMist)](#dibujo-lh3dmist-fn_007fa300)
- [Animales y rebaños](#animales-y-rebaños-create_flock-create_new_animal)
- [Datos de simulación del mapa](#datos-de-simulación-del-mapa-solo-datos-nada-se-dibuja)
- [Piscifactorías](#piscifactorías-create_fish_farm--create_town_fish_farm)
- [Porcentaje de construcción de un Feature](#porcentaje-de-construcción-de-un-feature-built_percentage-propiedad-chl-22)
- [Objetos del guion del mapa](#objetos-del-guion-del-mapa-farolas-hogueras-árboles-muertos-puertas)
- [Ciudades y ciudadela](#ciudades-y-ciudadela)
- [Órdenes de guion que mueven cosas](#órdenes-de-guion-que-mueven-cosas-move_game_thing-033-y-compañía)
- [Pendiente](#pendiente) · [Ganchos de prueba](#ganchos-de-prueba) · [Fuentes](#fuentes)

## Creación desde CHL (CREATE 27 / CREATE_WITH_ANGLE_AND_SCALE 252)

Desensamblado en `tmp_dis\mapa\chl_creatething_6F11A0.txt`.
- `GScript::CreateThing` 0x6F1B20 y `CreateWithAngleAndScale` 0x6F2E10 (ángulo en grados, ×0,0174533) solo aceptan
  los tipos 1..41 y llaman al switch `fn_006F11A0` (tabla 0x6F1A70). Si no se crea nada, el guion recibe **0**
  ("Thing not created"). openblack devolvía la entidad 0 (una entidad válida) para todo lo no soportado.
- El subtipo 5000 solo vale para Timer, SpellDispenser, Whale, Ark, Marker, Ball, Poo y Scaffold.
- **Marker** (`fn_0070D8D0`): un `ScriptMarker` con solo la posición. `MapCoords::Set` 0x603340 guarda
  `y − GetAltitude`, `GET_POSITION` 0x6F88A0 devuelve `GetAltitude + relY` y `ScriptMarker::PhysicsEditorCreate`
  0x561030 no hace nada, así que el marcador devuelve exactamente el vector con que se creó (y = 0 en CHL).
- Los demás objetos se quedan en el suelo: `PhysicsEditorCreate` (GameThingWithPos 0x401980, MobileStatic 0x55D720,
  Bonfire 0x4397C0) pone relY = 0; tras crear, 0x6F1591-0x6F1A42 rehace la matriz en `GetAltitude(pos) + relY` con
  solo el ángulo Y y la escala del objeto (sin inclinación X/Z).
- Casos: Feature `fn_00527350`(ángulo, escala); Villager `Villager::Create` 0x74FBE0 con edad grownUpAge + 1,
  VillagerChild con edad 10 (sin pueblo ni casa); Animal y Bird `fn_00419C20`; MobileStatic y Rock: subtipo 6 →
  GBaseOnly `fn_00609340` (sin ángulo ni escala), 7 y 59 → `GStreetLantern::Create`, el resto `fn_00608770` (info 8 →
  Bonfire, rocas con info +0x128 = 2); MobileObject `0x607000`, Poo = MobileObject 5, Ark = 23; Tree
  `Tree::Create` 0x749EE0 (sin bosque); AnimatedStatic `0x421F50`. Abode, Town, Dance, Flock, InfluenceRing, Citadel,
  WorshipSite, SpellSeed, Mist, Field, ComputerPlayer y TotemStatue dan "Invalid create type" también en el original.
  PuzzleGame (tipo 32, 0x6F184C): `fn_006D6680(pos, subtipo, ftol(ángulo·2048·0,159155), escala)` (ver «Puzle de los
  peces» en [water.md](water.md#puzle-de-los-peces)). Pendientes en openblack: Reward, Creature, DeadTree, Store,
  Timer, Vortex, Ball, Totem, Highlight y Scaffold. Ya portados: WeatherThing (`magic::script::CreateWeatherThing`),
  OneShotSpell, OneShotSpellInHand y SpellDispenser (`Magic/Script/CHLWorship.cpp`) y Whale (el tiburón).
- openblack: `CreateScriptObject` (CHLApi.cpp), `MarkerArchetype`. El círculo de Singing Stones ya se monta en
  (2496,67, 2246,33) sobre el suelo. Las funciones CHL sin implementar se registran una sola vez por función.

## Niebla del mapa (CREATE_MIST)

- `CREATE_MIST` "AFNFF" (0x7155C9) → `Mist::Create` 0x6063D0(pos con relY = F1, tamaño F3, color N2, k F4) →
  `CallVirtualFunctionsForCreation` 0x606420: `LH3DObject::Create(7)` (LH3DMist) en `GetAltitude(x, z) + F1`,
  +0x88 = F3, +0x90 = N2 >> 24 (alfa), bandera +0x80 bit 1; solo si F4 ≠ 1, +0x8C = F4 y bit 2. El constructor de
  LH3DMist 0x7F9560 pone +0x88 = 1, +0x8C = 3, +0x90 = 0x80 y el contador +0x84 = Random(0, 16) & 15.
- `Mist::SetFade` 0x606800(tamaño inicial, tamaño final, alfa inicial, alfa final, segundos): pone ya el tamaño y el
  alfa iniciales y `fn_00606880` suma un paso por turno de 0,1 s durante segundos × 10 turnos (alfa limitado a
  0..255); `Get2DRadius` 0x606660 = escala × la mayor semiextensión x/z de la malla. Lo usan `CREATE_MIST` 263 y
  `SET_MIST_FADE` 264 de CHL (una llamada de cada en challenge.chl; aún sin hacer).
- Land1 tiene 17 (pantano, cueva del flautista...). openblack: `MistArchetype`, `components::Mist`,
  `Renderer::DrawMists` (dibujo más abajo).

### Dibujo (LH3DMist, `fn_007FA300`)


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

## Animales y rebaños (CREATE_FLOCK, CREATE_NEW_ANIMAL)

Desensamblado en `tmp_dis\mapa\all_cases.txt` (casos 24, 25 y 49).
- **CREATE_FLOCK** "NAANNN" (0x71634A): `Flock::Flock` 0x52F780(A1, el jugador actual, id N0) → id en +0x8C, +0x60/+0x6C
  = A1, +0x50 = 0x50, +0x52 = 0x1E, en la lista g_game+0x205C44 (se inserta delante); `SetDomainCentrePos`(A2) → +0x14.
  Radio del dominio +0x50 = N3 (0 → 0x50). Con `VERSION` ≥ 2,1 (0xD9957C; todas las tierras traen 2,3): distancia del
  rebaño +0x52 = N4 y pueblo N5; antes, pueblo N4 y +0x52 se queda en 0x1E. Con pueblo: +0x34 y la lista del pueblo
  +0xF08. Invisible (solo simulación).
- **CREATE_NEW_ANIMAL** (0x716543; CREATE_ANIMAL 0x71649F igual con edad 0): busca el rebaño por +0x8C en esa lista
  (el más nuevo con ese id) y el pueblo con `FindTownWithID` → `fn_00419D10`(pos, info, pueblo, rebaño, edad).
  - Con rebaño: edad 0 → GameRand(20) + 5; crea el animal (`fn_00419E00`) y lo une (`fn_0052FA50`: lista +0x3C
    ordenada por el byte +0xD4 del ser, +0x48 miembros, `Living::SetFlock`); si el animal no se puede pastorear y el
    rebaño tiene pueblo, el rebaño sale de la lista del pueblo y +0x34 = 0; +0x88 = máximo de miembros.
  - Sin rebaño (`fn_00419C20`, también el CREATE de CHL): edad 0 → **GameRand(40)** + 5; el animal recibe un rebaño
    propio (`Flock(Living*)` 0x52F950, en su posición, sin id de guion, +0x50 = info.domainRadius (+0x25C),
    +0x52 = (int)info.flockDistance (+0x21C), sin pueblo).
  - Pueblo del animal (`fn_00417C50`, +0xE0 y la lista +0x984 del pueblo): solo lo guardan los que se pueden pastorear
    (`IsOkToBeShepherd`, vtable +0xBA4 = 0x41D0E0 → 1); los demás, ninguno.
- **Clases** (`fn_00419E00`, salto por info.animalInfo +0x1F4, 27 casos): terrestres (león, tigre, lobo, leopardo,
  SpellWolf, PieceLion/Wolf/Villager; ctor 0x41FD30 o 0x416EB0), de pasto (oveja, tortuga, vaca, caballo, cerdo,
  PieceSheep; ctor 0x41D0B0, se pueden pastorear) y voladores (cuervo, paloma, golondrina, pichón, gaviota, murciélago,
  SpellDove y SpellBat; ctor `Dove` 0x41DCF0). Los tipos 5 (cabra), 7 (cebra), 17-19 y > 26 (caballo, vaca, tortuga y
  cerdo de puzle) **no crean nada**.
- **Voladores**: el ctor `Dove` 0x41DCF0 llama al de Animal (0x416EB0, que llama a `Living::SetState` 0x5F2A80),
  reinicia campos (`fn_00417900`) y pone la altitud de su MapCoords (+0x1C) = info.altitudeNormal (+0x278: paloma,
  pichón y murciélago 20, cuervo, golondrina y gaviota 40); `Game3DObject::SetPosition` 0x63B680 los pone en
  `GetAltitude + altitudeNormal`. `CallVirtualFunctionsForCreation` es 0x41F240 (la de Animal más una llamada al
  objeto 3D). `StandAnimation`: paloma 8 (DOVE_FLAP), golondrina 27 (SWALLOW_FLAP), gaviota 22 (SEAGULL_TAKEOFF),
  murciélago 2 (BAT_GLIDE). Su vuelo ya está decodificado y portado (commit e3a9d81f, «Birds fly like the original»;
  `AnimalClass::Flying` en `AnimalArchetype.cpp`, detalle en
  [animals.md](animals.md#aves-cuervo-paloma-golondrina-paloma-bravía-gaviota-murciélago)).
  *Antes* openblack no los creaba (81 de los 116 animales de Land1); cuentan como Object en el contador de creación.
- openblack: `components::Flock`, `Animal::flock/town`, `Town::flocks`, `RegistryContext::flocks`, `AnimalArchetype`.

## Datos de simulación del mapa (solo datos, nada se dibuja)

- **SET_TOWN_UNINHABITABLE** (caso 5, 0x715542): pueblo +0x5F4 = 1 (`Town::uninhabitable`).
- **CREATE_TOWN_CENTRE** (caso 9, 0x71577C): pueblo o el más cercano (`fn_00552FF0`); `IsOkToCreateAtPos` 0x404B10;
  `Abode::Create`; si es un TownCentre: pueblo +0x9A4 = el centro si estaba vacío (`Town::centre`) y
  `Town::SetWorshipPercentage`(N5·0,001) 0x73C060, que guarda +0x5C0 **solo si el pueblo tiene lugar de culto** (si no,
  0) y lo pasa a la estatua tótem. Sin pueblo, `TotemStatue::SetWorshipPercentage` 0x738270. Todas las tierras pasan 0.
- **CREATE_PLANNED_ABODE** (caso 8, comparte código con CREATE_ABODE): pueblo o el más cercano, si no nada; tipo de
  abode 0x404 (TownCentre) → `PlannedTownCentre::Create` 0x7444D0, si no `PlannedAbode::Create` 0x405600 (pos, info,
  pueblo, ángulo N4·0,001, escala N5·0,001; comida y madera no se usan); invisibles (`PlannedMultiMapFixed::Draw`
  0x648930 = `ret`). openblack: `Town::plannedAbodes`.
- **CREATE_ARENA** (caso 69) → `fn_00424820` → GArena 0x4246F0 (pos, radio +0x30, lista g_game+0x205C7C); su
  GLightSheet solo se dibuja durante un combate. openblack: `components::Arena`.
- **Clima** (casos 60-63): `CREATE_WEATHER_CLIMATE`(id, info, pos, r1, r2) → `fn_00771300`: id 0 = `GClimate(0)`
  0x771020 (ignora el resto); si no, GClimate 0x771170 (pos +0x14, radios ordenados +0x20/+0x24, id +0x28, info +0x2C;
  lluvia y temperatura iniciales del rango de la estación, no portado), lista g_game+0x205CF4 (GClimate en
  [day-night-weather.md](day-night-weather.md#gclimate-los-climas-climatecpp)). `_RAIN`(id, F1, N2, N3,
  N4) → +0x34 {F1, N2, N3, (u8)N4}; `_TEMP`(id, F1, F2) → +0x44/+0x48; `_WIND`(id, F1, F2, F3) → +0x4C..; el clima se
  busca por id (`fn_007731B0`, el más nuevo); id 0 usa el clima del mundo g_game+0x250534, creado al vuelo; id
  desconocido no hace nada. Land1: zonas 1 (2701, 2567; −35/−32 grados, nieve), 2 y 3. openblack:
  `components::Climate`.
- **CREATE_DRINK_WAYPOINT** (caso 95) → 0x770BC0 (WayPoint.cpp, lista g_game+0x205C74): punto donde bebe la criatura.
  Land1 tiene 47. openblack: `components::DrinkWaypoint`.
- **FIRE_FLY_SPELL_REWARD_PROB** (caso 88): `GMagicInfo::GetInfoFromText` 0x5FB3B0 compara sin mayúsculas con el nombre
  de los 42 efectos de magia (el primero que coincide; "NONE" siempre es el 0; si no hay, 42) → 0x52B630: fuera de
  rango no hace nada; si no, tabla 0xCCFBAC[i] = p y rehace las sumas acumuladas en 0xCCFB04. No se reinicia entre
  tierras.
- **Globales**: `VERSION` → 0xD9957C; `SET_LAND_NUMBER` → g_game+0x205A08 (0 en el ctor de GGame; openblack no lo reinicia al cargar un mapa; lo lee
  `DesignedWaterFall` 0x5E3770 para el decorado de Land 3/4, ver rendering.md);
  `SET_TOWN_INFLUENCE_MULTIPLIER` / `SET_PLAYER_INFLUENCE_MULTIPLIER` → g_game+0x250078 / +0x25007C, que
  `GGame::Init` 0x54F66F pone a 1 antes del guion. openblack: `Game::GetMapScriptGlobals`.

## Piscifactorías (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM)

- Caso 31 (0x7166E1): 0x52C7B0(pos, GFishFarmInfo[N1] (0xCCFC78 + 0x128·i; info.dat solo trae el 0), sin pueblo).
  Caso 32 (0x716722): sin el pueblo no crea nada; si no, lo mismo con él. El ctor 0x52C360 guarda en +0x8C **siempre
  el pueblo más cercano** (`Town::GetNearestTownToPos` 0x73B170, cualquier tribu), sea cual sea el del guion.
- Banco de peces (`CallVirtualFunctionsForCreation` 0x52CC10, revisado): con [0xC37BF4] = 0 (sin aplanar el mar,
  `GetAltitude` 0x803090 usa la altura cruda), anillos de radio 2, 4... < 50 y 32 direcciones; la primera dirección con
  altura exactamente 0 en dos radios seguidos da el centro. openblack ya lo hacía igual (`GetUnflattenedHeightAt`), así
  que los 9 de Land1 sin banco (los del lago del pueblo 2 y otros) salen igual que en el original con los mismos
  datos; no se cambió la búsqueda.

## Porcentaje de construcción de un Feature (`BUILT_PERCENTAGE`, propiedad CHL 22)

Estado: **fiel** y portado.

- **Guion** (Land 1): `TheMissionaries` crea `GArk = CREATE(3, 69 = ArkDryDock, (1881,083; 8,1316; 3154,109))` y pone
  `BUILT_PERCENTAGE of GArk = 0,2` (el patrón `GET_PROPERTY; POPI 0; PUSHF v; SET_PROPERTY` es una asignación);
  `TheMissionariesBuildingBoat` suma 0,03 por golpe (con `PLAY_SOUND_EFFECT(RANDOM_ULONG(92, 97))`) hasta
  `ArkIncrement`. La numeración de openblack es la buena: 22 = `BuiltPercentage`.
- `GET_PROPERTY` 22 (0x70E1A9): `dynamic_cast<MultiMapFixed>` → `GetPercentBuilt` (vt+0x880 = 0x4014F0, +0x5C); si no
  es MultiMapFixed, **1**. `SET_PROPERTY` 22 (0x70EC69): MultiMapFixed → `fn_0052EDD0`: +0x5C = valor (0 si es
  negativo, **sin tope**) y, si ≥ 1, `MultiMapFixed::Built` 0x52EBB0 (+0x5C = 1, +0x58 pierde 0x02 y gana 0x08, suelta
  el sitio de obra +0x74, reacción 0xF si tiene pueblo, `RequestChangeTexture`); después la lista de edificios del
  pueblo (0x70EC9B..0x70ECD4), que un Feature no tiene.
- **Valor inicial**: `fn_00527350` → ctor de `MultiMapFixed` 0x52E1E0(pos, info, ángulo, escala, porcentaje,
  planeado): planeado → bit 0x02 y +0x5C = 0; si no, +0x5C = porcentaje y bit 0x08. El `CREATE` del guion pasa 1:
  construido.
- **Dibujo**: `Feature::Draw` 0x518690 = `MultiMapFixed::Draw` 0x518090: si `IsDrawBuilding` (vt+0x8A4), `DrawBuilding`
  0x517F90. `Feature::IsDrawBuilding` 0x527790: **solo para GFeatureInfo 69** (ArkDryDock) es `!IsBuilt()` (0x422110:
  bit 0x02 libre y +0x5C ≥ 1); los demás Features usan `MultiMapFixed::IsDrawBuilding` 0x52F0C0 = "tiene sitio de obra"
  (+0x74), que un Feature nunca tiene: se dibujan siempre enteros. `DrawBuilding`: p = `GetPercentForDrawBuilding`
  0x52EFD0 = min(GetPercentBuilt, GetPercentRepairedFromWhenDamaged 0x52F010 = 1 si no está construido); con p = 0 no
  se dibuja nada; si no, vt+0x110 del objeto estático = `fn_00816AD0` (el dibujo a medio construir de las casas: malla
  principal cortada en pos.y + 2·ext.y·escala·p con paredes interiores y tapa, y el andamio que sube (p < 0,2), entero o
  cortado desde arriba (p > 0,8)).
- openblack: `components::Feature::percentBuilt`, `src/ECS/FeatureBuild.{h,cpp}` (`physics::PartialBuild` pasado a la
  malla local del Feature; sin malla con p = 0; la huella del terreno se mantiene), `GET/SET_PROPERTY` 22 en `CHLApi`.
  El guion de Land 1 no llega aún a `TheMissionaries` (va detrás de elegir criatura): se prueba con
  `OPENBLACK_TEST_BUILT_PERCENTAGE`. A 0,2 se ve el andamio entero y el arca cortada a un quinto; a 1, el arca sobre
  sus puntales.

## Objetos del guion del mapa (farolas, hogueras, árboles muertos, puertas)

Desensamblado en `tmp_dis\mapa\all_cases.txt`, `d_streetlantern.txt`, `d_deadtree_isok.txt` y `d_animstatic_cvffc.txt`.
- **Parámetros**: en el bloque de argumentos del guion, el entero del parámetro i está en +0x6000 + 4i y el float en
  +0x6030 + 4i. `GMobileStaticInfo` ocupa 300 bytes en memoria (0xD3A6D8 + 300·i: MS[6] = 0xD3ADE0, MS[7] = 0xD3AF0C,
  MS[8] = 0xD3B038) y 284 en `info.dat`: en memoria el registro de info.dat empieza en +0x10 (el tipo de objeto está en
  info +0x10 y el clip de un AnimatedStatic en +0x128, que es +0x118 en `GAnimatedStaticInfo` de openblack). Las 61
  infos de MobileStatic tienen el tipo de objeto 0x1C (MOBILE_STATIC).
- **CREATE_STREET_LANTERN** (caso 80, 0x717720) → `GStreetLantern::Create` 0x7346E0(pos, &MS[N1]): no crea nada si en la
  celda del mapa de la posición (`MapCoords::FindType` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0; celdas de 10
  unidades) hay un objeto de tipo 0x1C a menos de 0,5 m en x/z (`GUtils::GetDistanceInMetres` 0x74CD70); cualquier
  cosa hecha con una info de MobileStatic: rocas, hogueras, farolas, árboles muertos. +0x58 = (info ≠ MS[7]).
  `CallVirtualFunctionsForCreation` 0x734810: malla 148 (MSH_B_CAMPFIRE) si +0x58, si no 398 (MSH_O_TOWNLIGHT);
  `SetPosition((x, GetAltitude + y, z), ángulo 0, escala 1)` (sin giro de 180°), la luz `fn_00823240`(ese punto,
  +0x58) en +0x5C y, **en las dos clases** (no mira +0x58, corregido: antes esta nota decía "solo en la de pueblo"), el
  sonido 0x93 en +0x60 (`fn_0071E8C0` = `SoundTag::Create`), salvo si el objeto lleva la marca UNAVAILABLE (+0xA & 1);
  detalle del sonido en [day-night-weather.md](day-night-weather.md). Land1: 8 de tipo 7 y 4 de tipo 59
  (farolillos de campo con la malla de la hoguera, **no** hogueras). El CREATE de CHL con 7 o 59 va por el mismo sitio.
  openblack: `StreetLanternArchetype`, `components::StreetLantern` / `LanternLight`; `night_lights` pone las luces
  según `LanternLight` (antes por la malla, y las hogueras de verdad salían con luz de farolillo).
- **CREATE_BONFIRE** "AFFF" (caso 73, 0x7176AE) → `fn_00439850`(pos, F1 temperatura, F2 ángulo Y, F3 escala) → ctor
  0x4395C0: `Rock`(pos, MS[8], ángulo, escala) y `CreateSpotVisualWithSpecifiedDuration`(pos, 25 SF_Bonfire, 1,0, −1 =
  siempre, la hoguera); la temperatura no se usa al crear (Land1 trae 24,7, que openblack tomaba por el ángulo). Sin
  luz de farolillo. openblack: `BonfireArchetype`.
- **MobileStatic**: `CREATE_MOBILESTATIC` "ANFF" (caso 41) → `fn_00608770`(pos, info, 0, 0, F2 ángulo, F3 escala):
  MS[8] → `Bonfire::Create` con temperatura 100; info +0x128 = 2 → `Rock`; MS[6] → nada; el resto `MobileStatic`.
  `CREATE_MOBILE_STATIC` "ANFFFFF" (caso 42) → `fn_00608840`(pos con relY = F2, info, 0, 0, F3, F4, F5, F6): MS[6] →
  GBaseOnly `fn_00609340`; MS[7] → nada; el resto `fn_00608770`(…, F4, F6); después `SetXYZAnglesAndScale`(F3, F4, F5,
  F6) sobre lo creado (también la base y la hoguera). openblack: `MobileStaticArchetype::CreateFromInfo` /
  `CreateWithXYZAngles`.
- **CREATE_DEAD_TREE** "ALNFFFF" (caso 43, 0x716E64) → `fn_00510BB0`(pos, GTreeInfo[N2], jugador, F3, F4, F5, F6, 0):
  ctor 0x510A30 = `Rock`(MS[3], ángulo 0, escala 1) + `SetLife`(F3); con 0xCC5F10 = 0, `GetDeadTreeMesh` 0x510C60 es la
  malla normal del tipo; luego `SetXYZAnglesAndScale`(F4, F5, F6, 1), la matriz de MobileStatic (x = F4, y = F5,
  z = F6). Land1: 3 (tipos 12, 4 y 4, vida 1, ángulos pequeños). openblack hacía un árbol quemado vivo; ahora
  `DeadTreeArchetype` (`components::DeadTree`, sin Tree ni bosque, se puede coger).
- **CREATE_POT** (caso 38): `IsOkToCreateAtPos` y, si la cantidad N3 ≤ 0 (0x716B19), nada. Quita los 4 montones de
  madera vacíos de Land1.
- **CREATE_NEW_FEATURE** (caso 75): con N5 ≠ 0 crea un `PlannedFeature` 0x527440 (no se dibuja); ninguna tierra lo usa.
- **Nombres**: features `fn_00527740` y animated statics `fn_00422600` comparan con `_stricmp` (si no hay, devuelven el
  número de infos, 0x4C / 0x10); `GAbodeInfo::GetInfoFromText` 0x405A70 recorre las 9 tribus, compara el prefijo con
  `_strnicmp`, exige '_' y la descripción con `_stricmp` (16 por tribu); si no, −1. El original usa el resultado sin
  comprobarlo; openblack registra el fallo y se salta el comando (desviación de robustez deliberada; antes lanzaba).
- **AnimatedStatic** (`CallVirtualFunctionsForCreation` 0x422300): pone el clip de info +0x128 (Norse Gate 191, Gate
  Stone Plinth 195, Piper Cave Entrance 189); `Draw` 0x422770 lo avanza o retrocede según esté abierta, limitado a su
  duración: cerrada es t = 0 (openblack: `SkeletalAnimation` parada en 0). Con malla 212 (Norse Gate, `fn_004230D0`)
  crea 2 `Game3DObject` con la malla 398 en (∓15, 30, 0) de la matriz de la puerta (filas con escala + traslación),
  ángulo 0 y escala 1, cada uno con la luz `fn_00823240`(su posición, 0).
- **CREATE_PLANNED_CITADEL** (caso 20): pueblo y jugador obligatorios; `fn_00467DD0` (PlannedTownCitadelHeart en el
  pueblo) y guarda la posición en 0xC5E258. El templo de verdad sale de `PlannedTownCitadelHeart::CreatePlannedNoFixedCheck`
  0x467EF0 (vtable +0x504: la `Citadel` del jugador si no tiene, `fn_00462B10`, y `CitadelHeart::Create` 0x464E20), que
  llama `Town::AddBuildingSiteNoFixedCheck` 0x73B8A0 desde `Town::RequestBestPlanned`, `Town::ForceBuildingOfPlannedAtPos`
  0x73E560 (`GScript::BuildBuilding` 0x6FAB30 de CHL, y 0x641774 tras `StartPlaygroundGame` con 0xC5E258) y
  `Scaffold::TryToBuildPlannedBuilding`. `GGame::Birthday` → `GPlayer::Birthday` → `Town::Birthday` solo rehace
  estadísticas. Al cargar el mapa **no hay templo**, solo el plan (GameThingWithPos 0x4C, sin malla, sin celda, sin
  índice de creación, sin aplanado; `Draw` 0x648930 = `ret`). Info "Citadel Heart" (info.dat 0x115C0): madera 5,
  timeToBuild 150, desireToBeBuilt 1,0, malla 564 BuildingDummyCitadel; tipo de abode del plan 0x804 (cívico).
  - Conversión 0x467EF0 (arg `float life`): el jugador es el **dueño del pueblo** (`Town+0x2C`, el de CREATE_TOWN o el
    neutral), no el del script (ese solo se valida). `CitadelHeart::Create`(pos, info, citadel, ángulo del plan, escala
    del plan, life, 1): el 1 marca "en construcción" (MultiMapFixed 0x52E1E0, +0x58 bit 1, +0x5C = 0).
    `CallVirtualFunctionsForCreation` 0x4675A0 crea el LH3D tipo 8 a **escala 1** con y = altitud(origen) + alt y llama
    0x882730 (malla B_FIRST_TEMPLE, % construido, **aplana la tierra**): el aplanado es al convertir. Lugares de culto
    (`fn_00464F50`) solo si life ≥ 1. Luego heart+0x94 = pueblo, `PostCreatePlanned` 0x648C50 y se borra el plan.
  - `AddBuildingSiteNoFixedCheck` pasa siempre life 0,0 y crea un `CitadelBuildingSite` (0x468DC0 → 0x43D1E0); lo
    terminan los aldeanos (`CitadelHeart::Built` 0x465000). Con vida < 1 `Draw` 0x882A40 usa `DrawPartialyBuilt`
    0x816AD0 (sin decodificar).
  - Disparadores: **Land 1** = CHL `FollowUs`: `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)` (la pos del plan de
    Land1.txt:95; `GetPlannedAtPos` 0x73E4C0 coge el plan más cercano a menos de radio de la malla 564 × escala + 1 m),
    luego `CALL_NEAR(Citadel 18)` + `SET_PROPERTY(22, 0.375)`; `PreventCitadelCompletion` lo limita a 0,9 y
    `CheckCitadel` espera 1. **Lands 2-5** = IA: `Villager::CheckSatisfyCivicBuildings` 0x758E90 (deseo del pueblo
    FOR_CIVIC_BUILDING 6, 0x748330) → `RequestBestPlanned` 0x73A650 → `GetBestPlanned` 0x73A140 (máscara 4).
  - **CREATE_CITADEL** (`Citadel::CreateCitadel` 0x463240) pasa (ángulo, 1,0, 1,0, 0) a `CitadelHeart::Create`: la
    escala del script se **ignora** (Kapa's Land1 Playground pasa 0, otros mapas 300 o 4121) y sale construido.
  - openblack: CREATE_CITADEL dibuja a escala 1 (antes usaba la del script: en Kapa's Land1 Playground el templo era
    invisible). CREATE_PLANNED_CITADEL exige pueblo y jugador válidos (si no, nada), el templo es del dueño del pueblo
    (`Town::owner`) y se dibuja a escala 1. **Desviación pendiente**: como no hay deseos de pueblo, sitios de
    construcción, BUILD_BUILDING/SET_PROPERTY 22/CALL_NEAR ni dibujo parcial, el templo se sigue creando ya construido
    (y aplanando) al cargar, para que no desaparezca de Land 1-5. Hacerlo fiel requiere portar todo lo anterior.
- **IsOkToCreateAtPos** 0x638C40: falla si `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe`
  0x601D10 da el bit 8 y la celda no es agua. El bit 8 sale de un círculo `NewCollide::Obj` de radio 0,5 (0x82AD90)
  contra el `GetCollideData` (vtable +0x858) de cada objeto fijo de la lista +4 de la celda (`Obj::Collide` 0x829140);
  los demás bits vienen de `MapCell::Collide` 0x601BD0 (bit 0x10 del bloque de tierra, fuera del mapa). Informe
  completo: `tmp_dis\mapa\flecos_isok.md` (simulación `isok\sim.py`).
  - **Quién lo llama**: solo CREATE_TREE (27, 0x716235), CREATE_NEW_TREE (28, 0x7162EE), CREATE_POT (38, 0x716B0C, antes
    de mirar la cantidad) y CREATE_MOBILEOBJECT (40, 0x716C71). Si falla, no crea nada, no escribe nada y el guion sigue.
    Ángulo y escala no se usan. Los handlers CHL no lo llaman. CREATE_TOWN_CENTRE usa otro (`GAbodeInfo::IsOkToCreateAtPos`
    0x404B10, sin portar: en Land1-5 no rechaza ninguno). Abodes, campos, features, mobile statics, hogueras y árboles
    muertos se crean sin mirar nada.
  - **La prueba**: círculo de 0,5 en (x, z) del guion (la altura no cuenta, `MapCoords(char*)` deja y = 0) contra los
    objetos de **su celda**; prueba 2D `dx² + dz² <= (ra + rb)²` y luego los hijos. Con agua en la celda (bit 0x10,
    `hasWater`) se crea siempre; fuera del mapa (o en un bloque vacío) también.
  - **Formas**: árbol = círculo de 0,3 en su posición, solo en su celda (0x74C5F0). MultiMapFixed (abode, centro,
    campo 594, feature, animated static, mobile static, roca, hoguera, árbol muerto, dispensador) = `NewCollide(LH3DObject)`
    0x829390 desde el bbox de la malla: centro del bbox girado con `x' = x·cos a − z·sin a`, `z' = x·sin a + z·cos a`;
    semiejes `max(1, escala·mitad)` en x y z; si largo/corto > 1,4, círculo exterior `sqrt(ex²+ez²)` con
    `int(largo/corto)+1` hijos de radio corto en fila por el eje largo (0x82ADD0 / 0x828F40); si no, un círculo de
    `max(ex, ez)`. Se mete en cada celda cuyo círculo (centro de la celda, 7,1) la toca. El bbox (0x8081B0) pasa las
    mallas con huesos (flag 0x100) por `LH3DAnim::SetTransform`, como el de openblack. Sin collide data: BigForest,
    vasijas, mobile objects, aldeanos, animales, farolas y planificados.
  - **openblack** (`ECS/MapCollide.h/.cpp`, `openblack::ecs::map_collide`): rejilla de celdas que se vacía en
    LOAD_LANDSCAPE y se llena con los parámetros del guion (malla del `Mesh` del objeto creado, ángulo Y y escala del
    guion, no el Transform). Sin registrar aún: piscifactorías, CitadelHeart (0x468FB0) y WorshipSite (0x77E490), sin
    decodificar. `OPENBLACK_LOG_ISOK=1` escribe una línea por rechazo (orden, posición, qué lo tapa).
  - **Resultado** (comprobado con `OPENBLACK_DUMP_ENTITY_COUNTS`): Land1 1395 → 1351 árboles (44 rechazos: los 43 de
    `sim.py` − 2 bajo la Piper Cave Entrance + 3 bajo los árboles muertos), mobile objects 51; Land2 921 → 915 (+1
    vasija); Land3 1397 → 1373 y 26 → 20 mobile objects; Land4 799 → 764 y 1 → 0 (+1 vasija); Land5 804 → 782 y
    19 → 13; LandT 341 → 316. Diferencias con `sim.py`: la Piper Cave Entrance es una malla con huesos y `sim.py` usaba
    los vértices sin transformar; los árboles muertos `sim.py` no los modelaba (son Rock con la malla normal del tipo,
    creados con ángulo 0 y escala 1, y los ángulos del guion son casi 0). Captura: Land1 junto al Boulder1 Lime
    (2120, 2494), ya sin los árboles de encima.

## Ciudades y ciudadela

Resumen de lo que la wiki ya dice de pueblos, templo y ciudadela, con enlaces (no se ha movido texto):

- **Datos del pueblo al cargar**: `SET_TOWN_UNINHABITABLE`, `CREATE_TOWN_CENTRE` (centro y porcentaje de culto) y
  `CREATE_PLANNED_ABODE`, en [Datos de simulación del mapa](#datos-de-simulación-del-mapa-solo-datos-nada-se-dibuja).
  El tótem del centro del pueblo (`components::TotemStatue`) y el hundimiento de los abodes hasta su cimiento, en
  [openblack-internals.md](openblack-internals.md#render).
- **`Town::owner`** (`Town+0x2C`, el jugador de `CREATE_TOWN` o el neutral): es el dueño del templo que sale de
  `CREATE_PLANNED_CITADEL`, no el jugador del guion
  ([Objetos del guion del mapa](#objetos-del-guion-del-mapa-farolas-hogueras-árboles-muertos-puertas)).
- **Templo**: al cargar solo hay el plan (`PlannedTownCitadelHeart`); `CitadelHeart::Create` lo convierte y aplana la
  tierra (0x882730), y `CREATE_CITADEL` ignora la escala del guion (misma sección). openblack lo crea ya construido en
  `CitadelArchetype` (*desviación* pendiente); el aplanado (plano hasta 35 unidades, mezcla hasta 70) está en
  [openblack-internals.md](openblack-internals.md#render).
- **Ciudadela y culto**: los seis huecos de lugares de culto en la entidad del templo (`components::Temple`) en
  [magic.md](magic.md#la-ciudadela-y-sus-seis-huecos-worshipcitadelcpp); su influencia y la diferencia heredada del
  templo ya construido en [magic.md](magic.md#influencia-m1i-srcecsinfluence).
- **Colisión (`MapCollide`)**: `IsOkToCreateAtPos` y la rejilla `ecs::map_collide`, en
  [Objetos del guion del mapa](#objetos-del-guion-del-mapa-farolas-hogueras-árboles-muertos-puertas); CitadelHeart y
  WorshipSite aún no se registran en ella.
- **Dentro de la ciudadela** (`g_game+0x205A28 == 1`): qué suena en [audio.md](audio.md#original-gaudio-lhaudio-y-qmixer)
  y [objects-and-resources.md](objects-and-resources.md#sonidos-informe-tmp_dissoundnotestxt); el fotograma en
  [original-frame.md](original-frame.md#otros-casos-templo-vídeo-y-2d); la paridad gráfica en [parity.md](parity.md).
- Árboles del pueblo (bosque escénico, lista de bosques Town +0x608):
  [trees.md](trees.md#búsquedas-de-árboles-y-bosques-para-los-aldeanos-informe-tmp_distrees2villager_queriesmd).

## Órdenes de guion que mueven cosas (MOVE_GAME_THING 033 y compañía)

La intro de **Land 1** (CHL `FollowUs`, `Scripts\Quests\challenge.chl`; código en `tmp_dis\mapa\rt_chl_code.txt`
desde la línea 49528) crea a la familia (madre = VILLAGER 49, padre = 53, hijo = 52, CREATE 027), la lleva con
`MOVE_GAME_THING(cosa, punto, 0.0)` y espera con `GET_DISTANCE(GET_POSITION(cosa), punto) == 0` (`FollowUs_loop_4` y
`_loop_6`) o `< 1` (`_loop_5`, `_loop_77..79`). Luego les hace actuar con `SET_SCRIPT_ULONG(cosa, clip, veces)` +
`SET_SCRIPT_STATE(cosa, 200)` y espera `PLAYED(cosa)` (`_loop_8`, 11, 17, 45, 48, 49, 74, 80..82). Todo esto era stub.

### MOVE_GAME_THING (GScript::MoveGameThing 0x6F8E80)

Saca radio, z, y, x y la cosa (0x6F8E91..0x6F8EE9); `GetScriptGameThing` 0x70D220 (si no: "Thing no longer valid"
0xC0C258). `MapCoords(pos)` 0x603160 (x / z; la y queda relativa a la tierra). Por tipo, en este orden:

| prueba (vtable) | qué hace | openblack |
|---|---|---|
| IsCreature (+0x34) | `dynamic_cast<Creature*>` ("no creature for script" 0xC0D598), IsObjectInMap (+0x178) → fn_004F6B60(pos, radio): `PrepareCreatureForScriptedAction` 0x4F6A90 y subacciones (`AddSubAction` 0x4FF240). Solo aquí se usa el radio | **pendiente** (no hay IA de criatura) |
| IsLiving (+0x3C4) | IsObjectInMap y !IsDrowning (+0x17C), si no nada; `AreWeThere(coords, 0.0)` (+0x85C, 0x60AD60) == 0 → `Living::SetupMoveToPos(coords, 4 IN_SCRIPT)` 0x5F2830 (0x6F8FCA); si ya está → `GScript::SetScriptState(cosa, 4)` 0x6F82E0 (0x6F8FD7) | aldeanos exacto; animales **(aproximado)**, abajo |
| IsFlock (+0x3EC) | `Flock::SetDomainCentrePos` 0x52FC20: destino (+0x80) del primer miembro y centro (+0x14) del rebaño | `AnimalBrain::goal` del primero y `Flock::domainCentre` |
| IsWeather (+0x3FC) | fn_00774550: +0x78 (su sistema) +0x5C = pos | no hay cosas de clima |
| IsComputerPlayer (+0x4B8) | fn_00658510(pos, 60,0) | no hay jugadores de la CPU |
| resto | "Jonty - Thing must be living to move it!" (0xC0D56C) y `SetPos(coords)` (+0xFC, 0x401940) | solo se mueve el `Transform` **(aproximado)** |

- **Living::SetupMoveToPos** 0x5F2830 (pos, final): estado de movimiento = byte de GLivingInfo +0x124 (1 MOVE_TO_POS
  en todos los aldeanos), o 3 MOVE_ON_STRUCTURE si GameThingWithPos +0x24 & 0x80 (solo lo pone `Living::MoveOnStructure`;
  openblack no lo tiene: nunca); `SetCurrentAndDestinationState(movimiento, final)` (+0x8DC) y, solo si da 1,
  `MobileWallHug::SetupMobileMoveToPos(pos)` 0x60AAD0: destino +0x80 = pos, `InitStepsXZ` 0x60BFA0, fuera de las listas
  de rodeo (fn_00611AC0 / 00611610 / 00612BB0 / 00610590, +0x76 = 0), y `AreWeThere(0)` → +0x5E = 1 ARRIVED; si no,
  `CircleHugInfo::Reset`, +0x78 = 1, +0x5E = 0xB **STEP_THROUGH** (recto, sin rodear: no es el LINEAR de
  `SetupMoveToWithHug` 0x5F2890). openblack: `ecs::villager::SetupMoveToPos` (`src/ECS/Villager/VillagerScript.*`), con
  la marca `MoveStateStepThroughTag` o `MoveStateArrivedTag` del PathfindingSystem.
- **MobileWallHug::AreWeThere(pos, r)** 0x60AD60: `dx² + dz² < (velocidad u16 +0x5A + r)²` (estricto, en MapCoords);
  `AreWeThere(r)` 0x60AD40 = `AreWeThere(GetDestPos() (+0x860), r)`. openblack en metros con `WallHug::speed`
  **(aproximado: flotantes en vez de enteros 16.16)**.
- Animales **(aproximado)**: `animal_ai::MoveTo(…, IN_SCRIPT)` (Living::SetupMoveToPos), la altitud del destino sobre
  la tierra (+0x88) se toma 0 y, si ya está, `animal_ai::SetState(IN_SCRIPT)` en lugar de SetScriptState.

### Lo que el guion consulta después

- **GET_POSITION** (GScript::GetPosition 0x6F88A0): rebaño → Pos del primer miembro o `GetFlockPos` 0x530570 (el
  centre +0x14); **un MobileWallHug que no es criatura (+0x408, +0x34) devuelve su destino (+0x80) si
  `AreWeThere(0)`** (0x6F8977..0x6F89AF), si no su Pos. Altura = `GetAltitude` + la +8 de esas MapCoords; openblack da la
  de la tierra en el destino **(aproximado: WallHug guarda solo x / z; GET_DISTANCE no mira la y)**.
- **GET_DISTANCE** (GScript::GetDistance 0x6F8CA0): `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 =
  `hypotenuse(dx, dz)` 0x74F6C0: **solo x y z**; 0 si |dx| y |dz| ≤ 0,0001 (0x8BF518), si no `1 / InvSqrt(dx²+dz²)` con la
  raíz aproximada por tabla `_FUN_0074f620` (tabla 0xDA5A10, la misma que `AnimalLairs.cpp`); **por debajo de 0,5
  (0x8AA3B4) da 0**. Antes openblack medía en 3D y sin el corte, así que `== 0` no se cumplía nunca.

### SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020, PLAYED 064 y los estados de guion

- **SET_SCRIPT_STATE** 0x6F8370: estado (primer pop) y cosa ("Object no longer valid" 0xC0D428). Contenedor de guion
  (+0x3F8: g_game +0x250090 +0x24 y la función de bucle de la tabla 0xC0C73C) **pendiente**; `dynamic_cast<Living*>` y
  !IsDrowning → `GScript::SetScriptState(living, estado)` 0x6F82E0; si no, "Object not living for set state" 0xC0D440.
- **GScript::SetScriptState** 0x6F82E0, no criatura: IsAvailable (+0x2C; Villager 0x751D50: no borrándose y final ≠ 14
  DYING) e IsObjectInMap (+0x178: +0x24 & 1; openblack: no está en la mano, **(aproximado)**) → `StorePreviousState`
  (+0x8EC, 0x763470), `CallExitStateFunction(estado)` (+0x904) y `CallEntryStateFunction(estado)` (+0x90C) sin mirar
  el resultado, `Living::SetAnim(1)` (+0x8FC, 0x5ECB80 → SetAnim(GetAnimId(), 1) 0x5ECBA0: el clip del estado desde 0) y
  +0x58 = 0. La rama de criatura (fn_0047B140 / 004F6E30 / 004F6F10) **pendiente**.
- **SET_SCRIPT_ULONG** 0x6F8770: veces (primer pop), clip, cosa. Villager: +0x120 = veces, +0x11C = clip
  (`Villager::scriptAnimLoops` / `scriptAnim`); criatura +0x1290 / +0x128C **pendiente**; si no, "setting the state of
  something neither a creature nor a villager" (0xC0D484).
- **PLAYED** 0x6F9DC0 en un aldeano: `Villager::IsScriptAnimationComplete` 0x7689D0: TOP 23 WAIT_FOR_ANIMATION → 0; TOP
  200 → veces == 0; si no 1. Otro Living: GetFinalState == 4 (0x6F9EC4) **pendiente**.
- Fila **4 IN_SCRIPT**: estado `StateInScript` 0x5ED9A0 (crea DataForScriptRemind si no hay; 1), entrada
  `EnterInScript` 0x5ED7E0 (vt +0x940: 1 si `IsStateEntryFunctionSameAs(final, next)` 0x7524D0 o no hay recuerdo de
  guion), salida `ExitInScript` 0x5ED9C0 (vt +0x914: `CircleHugInfo::Reset`; `IsScriptState(next)` (+0x960, fichero
  0x18) → 1; si no guarda el recuerdo y `ExitNoChangeState(next)` 0x768780 = 1 si next es interrumpible por guion
  (fichero 0x1C), IN_HAND (`IsStateForInterface` 0x417070) o `IsStateExitFunctionSameAs`).
- Fila **200 SCRIPT_PLAY_ANIM**: `ScriptPlayAnim` 0x768970: con veces > 0, una menos y `PlayAnimThenSetState(veces ?
  200 : 4)`; entrada `EnterPlayAnim` 0x768840 (como EnterInScript), salida `ExitPlayAnim` 0x7689C0 = ExitInScript; clip
  `ScriptAnimation` 0x768A00 = +0x11C (`AnimFn::Script` de `VillagerAnimations.cpp`).
- **Living::PlayAnimThenSetState** 0x5ECAC0: `CallExitStateFunction(s)` y, si 1, `CallEntryStateFunction(23, s)`: TOP 23
  y FINAL s, el clip no cambia. Fila **23 WAIT_FOR_ANIMATION**: `WaitForAnimation` 0x5EC990: `IsReadyForNewAnimation(1)`
  → `SetTopStateToFinal` y 0, si no 1.
- **No portado**: `DataForScriptRemind` (Living +0xB0, `Create` 0x5EF190, `KeepThatInMind` 0x5EF1D0, fn_005EF2A0), con el
  que un aldeano sacado de un estado de guion recuerda su paseo y lo retoma al volver. Sin él, las ramas de
  EnterInScript / EnterPlayAnim que lo retoman no se toman nunca **(inferido)**.
- **(aproximado)** En el original el paseo solo avanza desde la función de MOVE_TO_POS (`Living::MoveToPos` 0x5EC270 →
  `MobileWallHug::MoveTo` 0x60AF20); el PathfindingSystem de openblack mueve toda entidad con marca, sea cual sea su
  estado. Por eso `SetScriptState` quita las marcas si el aldeano ya no está en MOVE_TO_POS: sin ello el padre seguía
  andando (y se pasaba del destino) mientras actuaba en 200.

### CAST de la máquina virtual

En el `ScriptLibraryR.dll` original (`Plug Ins`), el opcode 23 INTCAST (0x10008EE0, tabla 0x100090E4 por tipo − 1)
**convierte**: CASTI lee los bits como float y hace `__ftol` (0x1001568C, trunca; la palabra baja del entero de 64 bits),
CASTF lee los bits como entero sin signo de 32 bits (fild qword con la parte alta 0); CASTV, CASTO y CASTB solo cambian
el tipo y el tipo 5 no hace nada. openblack solo cambiaba el tipo, así que `PUSHF 1.0 CASTI` llegaba a SET_SCRIPT_ULONG
como 0x3F800000 veces. Corregido en `components/ScriptLibrary/src/LHVM.cpp` (`Opcode23Cast`).

### En juego (Land 1)

Con `OPENBLACK_TEST_TEXT_CLICK=1` (los textos con interacción 1 esperan un clic, como en el original) `FollowUs` pasa
`_loop_4` a los ~15 s (la familia llega a las marcas del beso), anda a la playa, el hijo corre al mar, la cámara salta
con los SET_CAMERA_POSITION del guion, salen los textos (`¡Has salvado a nuestro hijo!` … `Te enseñaré cómo seguirlos.`)
y la familia pasa `_loop_77..79` (llega a `StartPath`). **Se para en `HAS_CAMERA_ARRIVED` (035)**, stub que da 0: en el
original es `GCamera::Arrived` 0x443050 (el modo de cámara activo, +0x58 / +0x28, vt +0x34; sin modo, 1) tras
`IsMultiplayerGame` 0x552F80 (multijugador → 1) y el aviso de ciudadela (g_game +0x205A28 == 1). Necesita el modo de
cámara de guion (`START_CAMERA_CONTROL`) y `MOVE_CAMERA_POSITION` / `MOVE_CAMERA_FOCUS` (003 / 004, también stubs:
por eso la cámara no se desliza). `END_CAMERA_CONTROL` está al final de `FollowUs` (tras `RUN Drag`), aún lejos.
Además, tras el `SET_FADE` a negro de 4 s (línea 51012) la pantalla queda negra: viene `SET_AVI_SEQUENCE(1, 1)` (203,
stub) y no hay `SET_FADE_IN` en `FollowUs`.

## Pendiente

- CREATE de CHL sin portar: Reward, Creature, DeadTree, Store, Timer, Vortex, Ball, Totem, Highlight y Scaffold.
- `CREATE_MIST` 263 y `SET_MIST_FADE` 264 de CHL (`CHLApi.cpp`, sin implementar).
- Lluvia y temperatura iniciales de `GClimate` (rango de la estación): aquí consta como no portado; comprobar con
  [day-night-weather.md](day-night-weather.md#gclimate-los-climas-climatecpp).
- `GAbodeInfo::IsOkToCreateAtPos` 0x404B10 de `CREATE_TOWN_CENTRE`; registrar en `MapCollide` las piscifactorías,
  CitadelHeart (0x468FB0) y WorshipSite (0x77E490).
- Ciudadela fiel: deseos del pueblo, sitios de construcción, `BUILD_BUILDING`, `CALL_NEAR`, dibujo parcial
  (`DrawPartialyBuilt` 0x816AD0); hasta entonces el templo sale construido al cargar.
- `SET_LAND_NUMBER` no se reinicia al cargar un mapa en openblack.

## Ganchos de prueba

- `OPENBLACK_TEST_BUILT_PERCENTAGE`: `BUILT_PERCENTAGE` del arca de Land 1.
- `OPENBLACK_LOG_ISOK=1`: una línea por rechazo de `IsOkToCreateAtPos`.
- `OPENBLACK_DUMP_ENTITY_COUNTS`: cuenta de entidades por tipo tras cargar (resultado de `IsOkToCreateAtPos`).
- `OPENBLACK_SCRIPT_THING_TRACE=1`: una línea por MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG y por PLAYED
  verdadero (para seguir `FollowUs`).
- `OPENBLACK_TEST_TEXT_CLICK=1`: cada turno, si un texto espera el clic (RUN_TEXT con interacción 1), hace el clic
  izquierdo (`HelpSystem::ProcessInterface(true)`, que lo ignora hasta 1 s de texto).

## Fuentes

- `C:\Users\diewgarc\dev\tmp_dis\mapa\`: `chl_creatething_6F11A0.txt`, `all_cases.txt`, `d_streetlantern.txt`,
  `d_deadtree_isok.txt`, `d_animstatic_cvffc.txt`, `flecos_isok.md`, `isok\sim.py`.
- Órdenes de guion: `tmp_dis\miracles\all.asm` (GScript::MoveGameThing 0x6F8E80, SetScriptState 0x6F8370,
  SetScriptUlong 0x6F8770, Played 0x6F9DC0, GetPosition 0x6F88A0, GetDistance 0x6F8CA0, HasCameraArrived 0x6ED170),
  `bwdis.py` en las funciones de Living / Villager / MobileWallHug citadas, y `_scratch\mapa\sldis.py` sobre
  `Plug Ins\ScriptLibraryR.dll` (INTCAST).
