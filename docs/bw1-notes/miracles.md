# Milagros uno a uno

Una sección por milagro del jugador: qué hace el hechizo, su objeto, sus partículas y lo que falta. El núcleo común
(tablas, ciclo de vida, cánticos, eventos y efectos, semillas, lanzar desde la mano, culto, influencia, alineación,
reacciones, vida y el modelo del fuego) está en [magic.md](magic.md); el motor de partículas, en
[particles.md](particles.md); el tiempo y el clima, en [day-night-weather.md](day-night-weather.md#tiempo-y-clima-m6a-srcecsweather).

Plan e informes: `dev\tmp_dis\miracles\` (`PLAN.md` y los informes que cita: `core.md`, `casting.md`, `sources.md`,
`destructive.md`, `resources.md`, `protect_creature.md`, `visuals_sound.md`). Direcciones W120. Esta página recoge lo
verificado en `runblack.exe` al portarlo, por hitos.

- [Comida y madera](#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)
- [Agua](#agua-m4a-magicspellsspellwater-psyscreatorsmist)
- [Curar](#curar-m4-m4h-magicspellsspellhealcpp-psysruleshealcpp)
- [Bosque](#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)
- [Bandadas](#bandadas-m4c-magicspellsspellflock-psysrulesflockcpp)
- [Bola de fuego y rayo](#bola-de-fuego-y-rayo-m5-magicobjectsmagicfireball-psysrulesfireballlightning)
- [Escudos](#escudos-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)
- [Teletransporte](#teletransporte-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)
- [Tormenta, tormenta eléctrica y tornado](#tormenta-tormenta-eléctrica-y-tornado-m6-storm-magicspellsspellstormandtornado-psysrulesstorm-ecsweatherlightningflashstormclouds)
- [Explosión de rayo y clases de PSys que faltaban](#explosión-de-rayo-y-clases-de-psys-que-faltaban-m6b-psysrulesexplosionkeypointsorientforestcpp)
- [Milagros de la criatura](#milagros-de-la-criatura-m8-pendiente)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

## Comida y madera (M3, `Magic/Spells/SpellResource`, `Magic/Objects/Magic{Food,Wood}`, `ECS/PotResource`)

Informe: `resources.md` §1. Lo de abajo está leído en el exe (W120) salvo lo marcado.

- **SpellResource** (MAGIC_TYPE 14 FOOD, 15 FOOD_PU1, 21 WOOD; `GMagicResourceInfo::AllocSpell` 0x5FAC20, +0xEC «primer
  evento hecho» a 0 en fn_00724C80; sin Process propio).
  - `SpellEvent` 0x724D80: primero el evento por defecto (EffectValues de FOOD/WOOD: solo alineamiento 1, reacción 21);
    luego, salvo el evento 1 o si está cerrado, n = `resourceAmountFirstEvent` la primera vez y
    `resourceAmountPerEvent` después; `PayFor(costPerUnit × n)` (el resultado no se mira); fuera del mapa, en agua
    (`IsDryLand` 0x603620: byte de altitud ≥ 4) o con fuerza 0 se paga y no cae nada; si no,
    `Pot::AddResourceToPos(pos, IS, tipo, ftol(poderTribal × n), 0, x)`.
  - Comida: 200 la primera vez (1400 cánticos), luego 18 (126); FOOD_PU1 20; madera 500 (1500), luego 20 (60).
  - `HasEnoughChantsAndLifeForRecast` 0x724C90 = costPerUnit × primero ≤ cánticos.
  - **El campo `poisoned` de FOOD_PU1 no envenena**: fn_00724CE0 empuja `(pos, IS, FOOD, n, 0, poisoned == 1)` y en
    `AddResourceToPos` el 5.º argumento ([esp+0x60]) va a `SetPoisoned` (vt 0x69C, con `IsPoisoned` vt 0x4A4) y el 6.º
    ([esp+0x64]) a `SetSpeedUp` (vt 0x864, con vt 0x4A8). La comida PU hace pilas «speed-up» (chispas
    PILEFOOD_SPEEDUP, 46). `resources.md` §1.4 tenía los dos argumentos al revés.
- **`Pot::AddResourceToPos` 0x66F270** (`ECS/PotResource.cpp`, lo usan la mano al soltar una vasija y los granos):
  - fuera del mapa, 0; recorre 9 celdas de 10 m con `GUtils::Spiral` 0x74D7E0 (la de pos y sus 8 vecinas), en cada una
    la lista fija (+4) y luego la móvil (+0), mientras quede algo;
  - acepta un objeto si `IsResourceStore(tipo)` (vt 0x680) o `IsPot` (vt 0x4B4) y `GetResourceType` (vt 0x690) == tipo,
    y `IsCloseToEqual(pos, GetDefaultFireCentrePos, Get2DRadius × GetRadiusMultiplierForApplyingPotToPos)` (vasija 2,
    almacén/objeto 1,2); el primero que cumple toma lo que acepta (`AddResource(tipo, resto, IS, envenenado, &pos, 0)`,
    vt 0x9C), no el más cercano;
  - lo que sobra, si pos no es agua, hace una pila nueva (fn_005FA8B0) con el sonido de pila fn_0066D1A0 (< 200:
    `77 + t % 6` / `92 + t % 6`; si no `75 + (t & 1)` / `86 + t % 6`), `SetPoisoned` y `SetSpeedUp`.
  - **La voz de guía al soltar (pendiente, leída entera).** 0x66F4D8..0x66F509: solo si el IS es
    `GGame::MyInterfaceStatus` 0x555880 (en openblack, `dropper.isMyInterface`), con el punto y un `RESOURCE_RAIN_TYPE`
    = 1 para comida (tipo 0) y 2 para madera (tipo 1), 0 para lo demás (0x66F4EB..0x66F502). `GGuidance::ResourceDropSFX`
    0x71B570 pide: `GGuidance::PlayNow(1)` 0x71AF50 sobre la guía del IS (+0x30) distinto de 0; un pueblo a menos de
    100 m ([0x98013C], `MapCoords::GetNearestTown` 0x6020E0); y `GGuidance::GetResourceDropSample` 0x71B5F0, que suma
    tres flotantes de ese pueblo (comida +0xC4 + +0x108 + +0x19C; madera +0xC8 + +0x10C + +0x1A0; el tipo 3, el de
    `DoDeleteObjectAndTakeResource` 0x63A9E6, usa +0xEC + +0x130 + +0x1C4) y por encima de 0,5 ([0x980140]) elige con
    `LocalRand(3)` un id de HELP_TEXT entre 0x1352/0x1353/0x1354 (comida) o 0x1355/0x1356/0x1357 (madera), por encima de
    0,25 ([0x980144]) entre 0x135B/0x135C/0x135D (comida; la madera repite los mismos tres) y por debajo nada. Luego
    `GGuidance::PlaySample` 0x71C6F0 (1, la muestra, jugador +0xB5, 1, 0x7F, 0x64, 0x5A, el punto, 200 [0x980148], 1).
    Falta el canal de guía (`Audio/Services/Voices.h`, hito B7 del plan de audio) y esos campos del pueblo.
  - **El desvío de la madera de un almacén (pendiente, identificado).** `StoragePit::AddResource` 0x732F60, antes de
    nada (0x732F67..0x732F99): si su +0x74 no es nulo y el tipo es WOOD (1) o ANY (−2), reenvía la llamada entera al
    `AddResource` (vt 0x9C) de ese objeto y devuelve su respuesta. **+0x74 es `MultiMapFixed::building_site`**, un
    `BuildingSite*` (bw1-decomp `src/Black/MultiMapFixed.h`; StoragePit hereda de Abode, cuyos campos propios empiezan en
    0x7C): un almacén en obras manda su madera a la obra. openblack no tiene obras ni nadie construye casas
    (`ECS/Components/Town.h`), así que ningún almacén puede tener una y el desvío es inalcanzable.
  - **R16, el radio 2D de la pila:** `Object::Get2DRadius` 0x638180 = escala × max(+0x24, +0x2C) de la malla, que
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 llena con la media extensión (max − min)/2 de todas las submallas;
    `PileFood::Get2DRadius` 0x66F180 lo multiplica por `GetProportionRaised` (vt 0x86C). En Land1: MagicFood de 200
    → 1,95 m (acepta a 3,9 m), crece con la pila (acepta a 4,14 m con 218); MagicWood de 500 → 2,04 m (acepta a 4,07 m).
- **Pilas** (`Magic/Objects`): MagicFood 0x5FA9F0 (PotInfo 10, MSH_S_GRAIN_PILE, escala 0,3, dueño +0xBC) y MagicWood
  0x600E20 (PotInfo 9, MSH_B_WOOD_01, escala 0,7, dueño +0xB4); sin Process ni caducidad.
  - **Las sombras de la pila de comida (identificado; ya se cumple).** `MagicFood::CallVirtualFunctionsForCreation`
    0x5FAAB0, después de la de PileFood 0x66E1A0, llama a dos setters del Game3DObject +0x40: vt 0x78(0) en 0x5FAAC8 y
    vt 0x80(0) en 0x5FAAD2. Game3DObject hereda de LH3DObject sin virtuales propias (bw1-decomp
    `src/Black/Game3DObject.h`), así que son `LH3DObject::SetCastDynamicShadow` y `LH3DObject::SetShadowOnTexture`
    (huecos 0x78 y 0x80 de `src/Lionhead/LH3DLib/development/LH3DObject.h`): **la pila de comida mágica no da sombra ni
    dinámica ni horneada**. (aproximado) esa cabecera no marca la vt 0x80 como `__fastcall` y el exe le pasa el argumento
    en edx (0x5FAAD0 `xor edx, edx`), igual que a la vt 0x78. openblack ya la deja fuera de las tres: `CastsStaticShadow`
    de `RenderingSystem.cpp` rechaza cualquier Pot y su `ReceivesDynamicShadow` cita esta misma llamada (0x5FAAC8) para
    los tipos MagicFood y HandFood; `CastsPhysicsShadow` de `Graphics/ShadowList.cpp` rechaza también cualquier Pot.
    Visto en `polish_fix_bosque2_food_drop.png`: ni la pila mágica ni el grano del granero proyectan sombra.
  - **La pila de madera sí las conserva:** `MagicWood::CallVirtualFunctionsForCreation` 0x600F10 es solo una llamada a la
    de PileResource 0x66E300, sin esos dos setters.
- **Efecto SF_Food / SF_Wood** (`PSys/Rules/Sprinkle.cpp`, `PSys/Creators/Mesh.cpp`):
  - `UR_HandSprinkle` 0x6A0220: un átomo fuente en la posición del gesto (como mucho 58 m sobre el suelo); en el primer
    paso, si lanza la interfaz de este ordenador, arranca `HandStateGrain` (fn_005B2F70). Velocidad
    (0, Δy/dt − 20 si lanza un humano, 0).
  - Los granos los emite `UR_WillowWisp` con el hechizo *habilitado*: max(dt × MaxAtoms / DieAge,
    movido / 2,5 m) por paso, o sea **18 por segundo aunque la mano esté quieta** (el plan decía que una mano quieta no
    suelta nada: no es así). Cada grano que toca tierra (`LandscapeCollide SendEvent 1`) es un `SpellEvent 3`.
  - `AppearanceRuleTumble` 0x6A6200 (troncos) y `ParticleMeshCreator` 0x6A8B00 / `…AnimTextured` 0x6A8DA0 (troncos
    MSH_I_OFFERING_WOOD, cono de lluvia): los átomos de malla se dibujan como instancias con los objetos.
  - Arreglo del lector de ficheros de hechizo: las ARRAY pueden llevar flotantes (`KeyPoints` de UR_HandSprinkle); antes
    la lectura entera se paraba allí y SF_Food, SF_Wood (y cualquier fichero así) no cargaban.
- **HandStateGrain** (`HandGrain.cpp`, estado 8 de la mano): `fn_005B3000` guarda (ClampHand, TotalTime, altura,
  ángulo, bucle) y la posición de la mano (+0x1EC); `fn_005B2DA0` en cada turno: t += dt / TotalTime, spline cúbica
  natural de fn_005B3760 por (0,0) (0,2,1) (0,8,1) (1,0) (el ctor pasa 1e30 en los dos extremos porque +0x148 = 1,
  0x5B2CF2; el pico vale 1,61); altura = v × 10 m, inclinación =
  v × 1,07 rad (pico a t = 2 s: 16,1 m y 1,73 rad). Con ClampHand (comida y madera) `HandStateHolding::Update`
  0x5B3FD4 (vt 0x1C) pone la
  posición requerida de la mano en el punto donde empezó, y suma la altura (vt 0x18). `Spell::CoreCloseDown` 0x720160 la
  para.
  - **El sentido del giro de la inclinación (verificado; fiel desde la sesión asistente).** El ángulo viene del fichero y
    es **positivo**: `AngleToRaise 1,07257` (SF_Food.txt línea 143, SF_Wood.txt línea 114), y fn_005B2DA0 lo usa tal
    cual (0x5B2EF3: altura = v × +0x134, inclinación = v × +0x138). `ObtainRequiredHandPosition` 0x5B6DE0 arma el eje
    mano → cámara normalizado (`LH3DTech::g_camera` menos la posición, 0x5B6E1B..0x5B6ECE), llama a fn_007FB180(matriz,
    eje, ángulo) y transforma el vector (0, 1, 0). fn_007FB180 escribe la matriz de Rodrigues **estándar** por filas
    (0x7FB1F7 `[ecx]` = x² + (1 − x²)c, 0x7FB207 `[ecx+0xC]` = xy(1 − c) + zs, 0x7FB21C `[ecx+0x18]` =
    xz(1 − c) − ys, …), pero el transporte de 0x5B6EE8 es por **vector fila**: out.x = R00·v.x + R10·v.y + R20·v.z + t.x,
    es decir vᵗ·M = Rᵗ·v = R(−ángulo)·v. Por eso `HandPlacement.cpp` gira con `-tilt`
    (`glm::rotate(mat4(1), -tilt, eje)`).
  - **Con ClampHand la mano no se mueve mientras cae el grano**, así que todos los granos caen en el mismo punto y
    hacen una sola pila que crece (el plan esperaba una línea de 20 m: no la hay con `ClampHand 1`, y el agua, que
    tiene `ClampHand 0`, sí sigue a la mano). El punto es el que tenía la mano al empezar el chorro, y el bucle no
    lo vuelve a tomar.
- **Pruebas:** `test_food_wood` (spline, bucle, costes, sonidos, `GetProportionRaised`, la emisión del sprinkle y la
  lectura de KeyPoints; con `OPENBLACK_GAME_PATH` las filas reales). Capturas en `dev\_audit\magic\`:
  `m3_food_pile.png` (`OPENBLACK_TEST_SPELL=FOOD,1808,2626`: la columna de grano y la pila; el registro da 200 → pila
  nueva, luego +18 por grano, 1400 y 126 cánticos), `m3_wood_pile.png` (WOOD: troncos que caen girando sobre una
  MSH_B_WOOD_01 de 500 + 20·n), `m3_food_store.log` (junto al almacén: todo entra en el almacén), `m3_foodpu_pile.png`
  (FOOD_PU1: 200 y luego +20 (140 cánticos), la pila nueva con SetSpeedUp y su visual 46 SF_SparklesFromObject) y,
  con la mano (`OPENBLACK_TEST_SEED=FOOD` + `OPENBLACK_TEST_CAST=press@2,release@20` de la lane M2),
  `m3_hand_raise_730.png` (la mano alzada y volcada, el chorro de grano y la pila junto al almacén) con
  `m3_hand_raise.log`, donde `Grain trace` da el pico 16,14 m / 1,731 rad a t = 0,50 de los 4 s.
  `polish_fix_bosque2_food_drop.png` (`OPENBLACK_TEST_SPELL=FOOD,1790,2625`, cámara `1780,42,2612,1790,34,2625`): la
  pila mágica de 200 + 18 por grano, sin sombra; la traza `Pot trace: new MagicFood pile … 2D radius 1.95` y los
  `18 of food into pile …` con el radio creciendo de 3,91 a 5,25 m.

## Agua (M4a, `Magic/Spells/SpellWater`, `PSys/Creators/Mist`)

Informe: `resources.md` §2; desensamblado propio en `dev\tmp_dis\miracles\impl\m4a\` (`process.txt`,
`alloc_getters.txt`, `obj_water.txt`, `field_water.txt`, `tree_water.txt`, `mist_creator.txt`, `mist_draw.txt`). MAGIC_TYPE
22 WATER y 23 WATER_PU1, semilla 9 WATER. Estado: **fiel** salvo lo marcado.

### El hechizo (`SpellWater.cpp`)

- `GMagicWaterInfo::AllocSpell` 0x5FAC70: 0xF4 bytes, vtable 0x8F553C; fn_00724EC0 pone a 0 +0xEC (edad del último
  anillo) y +0xF0 (la reacción «apagando un fuego»): `water::SpellWaterData`. **La vtable solo redefine `Process`**
  (vt 0x528); el resto es el `Spell` normal (InitWithPos, SpellEvent, CloseDown...).
- Getters (`info+0x10 − 0x16`): radio de lluvia fn_005FACE0 = **6** (WATER) / **12** (PU) / 1; crecimiento del anillo
  fn_005FACC0 = **2** / **4** / 1; `GetRippleEvery` 0x5FAD00 = **0,1**.
- `SpellWater::Process` 0x724ED0, **una gota por turno** (comprobado instrucción a instrucción):
  1. `Spell::Process` 0x720710 y su resultado, que se devuelve siempre; si la reacción +0xF0 ya no está disponible, a 0;
     si el hechizo está cerrado, nada más.
  2. `r = GameFloatRand(R) × 0,7 + 0,3` (0,3 … 0,7·R + 0,3 m: 4,5 m con WATER, 8,7 m con PU) y
     `a = GameFloatRand(2π)`; la gota P = posición de lanzamiento (+0xCC, la que sigue a la mano en un lanzamiento con
     la mano) + (r cos a, r sin a), con `y = GetAltitude(P) + 0,2`.
  3. `SpellEvent{2, P, mov. 0, fuerza 1, 0, sin objetivo}` (vt 0x52C → `ApplyDefaultSpellEffect`): los EffectValues de
     WATER (**quemar −4000**, radio 1 m) × fuerza × poder tribal, `costPerEvent` 10, la reacción 21. Su resultado no se
     mira: la gota y el anillo salen aunque no queden cánticos.
  4. Las 9 celdas de 10 m desde la de P (`GUtils::Spiral`, dirección 1, cuenta 1), cada una con su lista fija y luego
     la móvil (fn_00603500 / fn_007252D0): cada objeto con `2,5 × GetPower > dist2D(obj, P) − GetRadius` (estricto)
     recibe `ApplyWaterSpell` (vt 0x67C). `GetPower` de un hechizo es el de `GameThingWithPos` 0x56FE60 = 1, así que el
     alcance es **2,5 m desde el borde** (PLAN §4.1.2). `GetRadius` (vt 0x60) es `Get2DRadius` (Object 0x638110 salta
     a vt 0x64): **5 m en un campo** (Field 0x528E80), en el resto la media extensión de la malla × escala.
  5. Un anillo cuando `0,1 < edad − último` **en simple precisión** (fld/fsub/fstp dword; con la edad sumando 0,1 por
     turno sale en casi todos los turnos, no en todos: el primer turno no, 0,1 − 0 no es mayor que 0,1). Color
     `{0xFF80CBC5, 0xFF8599C5, 0xFFBA97B2, 0xFFB9CA86, 0xFFBD9C8A}[GameRand(5)]`, ángulo `GameFloatRand(2π)`, en el
     primer hueco de los 1024 de 0xEAB7C8: +0 P, +0x0C bandera 1, +0x10 edad 0, +0x18 crecimiento 2/4, +0x20 ángulo,
     +0x24 1, +0x28 aspecto 1, +0x2C ritmo 1, +0x30 celda 0x30, +0x34 color. **Los anillos caen en tierra**, no solo en el
     agua (`ecs::AddWaterRing` de la lane del agua, desde `AddDropRing` en `SpellWater.cpp`; el color es una de esas
     constantes, no la tabla de luz, y el anillo lo guarda toda su vida).
- `ApplyWaterSpell` por clase (la lista de símbolos solo tiene estas tres):
  - **Object** 0x63A8E0: si el objeto arde (`IsOnFire`) y +0xF0 está vacío, `+0xF0 = CreateReaction(hechizo, 34
    REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE, jugador del hechizo, 1)`. Devuelve 0.
  - **Tree** 0x74C390: la parte de Object y luego `ecs::ApplyWaterSpell(árbol, magicType == 23)` (de «arboles»: crecer,
    con PU pasar del máximo, o un brote del bosque cada > 40 turnos). Con brote y jugador:
    `GPlayer::FUN_0064DA80(0xE, 1)` —**solo hace algo en multijugador** (`IsMultiplayerGame` 0x552F80), aquí nada— y
    `GAlignment::Update(jugador, árbol nuevo, 1)` 0x4145A0 = `alignment::UpdateForTree(jugador, true)`. Devuelve 1.
  - **Field** 0x528F30 (`ecs::ApplyWaterSpellToField` en `ECS/Fields.cpp`): la parte de Object; con jugador,
    `ConsiderMakingCreatureMimicPlayer(acción 0x21, magia 0x16)` (M8, no portado); si no arde: con `cultivos ≤ 30`
    (`timesToSow`) pasa a **31** de golpe (sembrado); si no, mientras `crecimiento ≤ 1200`, `crecimiento += 2`
    (`effectOfWaterSpell`, info.dat, igual en las 6 filas) y `comida += 2 × 350 / 1200`. `IsUnripe` se llama y se
    descarta.
- **Apagar fuegos**: lo hace la parte 3 (quemar −4000 en 1 m): `ApplyEffectToFireEffectIfNecessary` lleva la
  temperatura hacia ambiente − 4000 con el tope `max(10·ΔT/capacidad, ΔT)`, así que **una sola gota apaga** un árbol
  ardiendo a 500 (Tc 110, capacidad 100). Como el evento va antes que `ApplyWaterSpell`, el objeto ya no arde cuando le
  llega el agua y la reacción 34 solo nace si la gota no le alcanzó con el radio de 1 m pero sí con los 2,5 m. Las
  tormentas (fn_0072DCC0) son de M6.
- La niebla del original para el `SPELL_AT_POS` está a la altura de «desde» (el gancho la pone 30 m sobre el blanco,
  inferido); con la mano, en la mano, que `UR_HandSprinkle` sube 8 m en 8 s sin sujetarla (`ClampHand 0`).

La niebla del agua (`ParticleMistCreator`, también la de las nubes de la tormenta) está en
[`ParticleMistCreator`](particles.md#particlemistcreator-psyscreatorsmistcpp).

### Arreglo en `ECS/Effects`: las celdas del mapa

`effects::FixedObjectsInMapCell` / `ObjectsInMapCell` (EffectValues.h). **(aproximado)** La rejilla de openblack
(`MapProduction`) solo mete un objeto fijo en las celdas cuyo centro está a menos de su radio + 1 m, así que un árbol
pequeño lejos del centro de su celda no estaba en ninguna, y ni `ApplyEffectToMapPos` (fuego, rayo, agua) ni el agua lo
veían (el arbusto ardiendo junto al almacén de Land1 no se apagaba). El original enlaza cada objeto en la celda de su
posición: ahora la lista fija de una celda es la de la rejilla más los fijos cuya posición cae en ella, ordenados por
entidad. `ApplyEffectToMapPos` la usa para su parte fija.

### Pruebas y capturas

- `test_water`: los getters, la distancia de la gota y el alcance (estricto, 5 m de un campo), los anillos en simple
  precisión, el color de la niebla, la nube de `UR_HandSprinkle` con su niebla y, con `OPENBLACK_GAME_PATH`, las filas de
  info.dat (quemar −4000, radio 1, `costPerEvent` 10, 6 / 10 s, reacción 21, partículas 16/17, coste 5000/7000),
  `effectOfWaterSpell` = 2 en las 6 filas de campo y SF_Water / SF_WaterPU1 ejecutados (nube + cono).
- Gancho `OPENBLACK_TEST_WATER_SHOT` ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)).
- Capturas en `dev\_audit\magic\`:
  - `m4a_water_field.png` + `m4a_water_field_end.log` (`OPENBLACK_TEST_SPELL=WATER,1830,2750,10,6`, campos de Land1 en
    1825-1835 × 2745-2755 con `OPENBLACK_TEST_FIELD_GROWTH=500`): anillos sobre la tierra; el registro da
    `crops 0 -> 31` en los cuatro campos y luego `growth +2, food +0,58` por gota. Un campo con poca comida se dibuja
    hundido (Field::Draw), así que el sembrado no se ve hasta que crece.
  - `m4a_water_hand.png` + `m4a_water_hand_end.log` (`OPENBLACK_TEST_SEED=WATER` + `OPENBLACK_TEST_CAST` +
    `OPENBLACK_TEST_CAST_PATH=1822,2742,1838,2756`): la nube y el cono de lluvia sobre la mano, los anillos en el
    suelo; las gotas siguen a la mano por los campos.
  - `m4a_water_cloud.png`: la nube azulada con el cono de lluvia vista desde abajo.
  - `m4a_water_fire_burning.png` → `m4a_water_fire_out.png` + `m4a_water_fire_burning.log`
    (`OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"`, agua en el turno 200): el fuego 1 se borra en el turno 201,
    tras la primera gota; sin el arreglo de las celdas seguía ardiendo pasado el turno 260.

### Sin portar / pendiente (agua)

- El mimetismo de la criatura (M8) y `FUN_0064DA80` (multijugador).
- SF_WaterInHand / SF_WaterOnHolder usan la misma niebla, que ahora se dibuja; no se han revisado en una captura.
- La distancia 2D usa la raíz exacta: el original va por `hypotenuse` 0x74F680 con una raíz inversa sobre MapCoords
  16.16 (diferencias de submilímetro, aproximado).

## Curar (M4 «m4h», `Magic/Spells/SpellHeal.cpp`, `PSys/Rules/Heal.cpp`)

Informe: `resources.md` §4 y `visuals_sound.md` §4.4. MAGIC_TYPE 10 HEAL y 11 HEAL_PU_ONE, semilla 7 HEAL. El hechizo no
cura nada por sí mismo: **busca los objetivos y el PSys los cura uno a uno** con un evento de tipo 5 por chakra.

### Buscar objetivos y lanzar

- `GMagicHealInfo::FindTargets` 0x5FBB00 (ya en `Magic/CastRules.cpp`): radio `dummyVar` 10 (35 con PU) y tope
  `maxToHeal` 20 (100), los dos × el poder tribal si hay hechizo; espiral de `ceil(2R/10)²` celdas; cada vivo que acepta
  el efecto y `CanBeHealedByHealSpell` a menos de R pasa a ser objetivo del PSys (`AddTarget_`, vt 0x114). **No mira si
  le falta vida**: un sano también recibe su chakra (comprobado en el juego: aldeanos con vida 1 se iluminan).
- `SpellHeal::InitWithPos` 0x72D870 = `Spell::InitWithPos` y, **si devuelve 1**, `FindTargets(pos, this)`; no mira
  cuántos encontró. La comprobación de la mano es `GMagicHealInfo_vfunc12` 0x5FBD20 (vt 0x30): solo con MAGIC_TYPE 10 u
  11 y `FindTargets(pos, NULL) > 0`.
- `SpellTargets::TakeTargetObject` 0x671030 saca **el último** de la lista (`psys::Effect::TakeTarget`), así que el
  PSys consume los objetivos en orden inverso al que los encontró.

### `UR_HealSpellChakra` 0x6A0B20 (`PSys/Rules/Heal.cpp`)

Constructor 0x6A0810 (pone las marcas 6 del modificador y quita la 4: **no es creador, pero mantiene vivo el efecto
hasta que se cierra**; en openblack es `Modifier::KeepsAlive`), propiedades 0x6B1E40. Cada turno:

1. Saca objetivos de `SpellTargets` mientras haya. Los que ya tienen un chakra (lista global 0xD4ED38, `g_Chakraed`) o
   ya no existen se descartan; el resto:
   - **manda `SpellEvent{tipo 5, centro del objetivo, fuerza 1, objetivo}`**, que es lo que cura de verdad
     (`ApplyDefaultSpellEffect` → `ApplyEffect(EV, 0)` → `IncreaseLife`, y `SetPoisoned(0)` si estaba envenenado);
   - crea un átomo de punto con su `AtomData` (0x38 bytes, ctor fn_006A0920): el objetivo, su `Get2DRadius` (+0x30) y
     su `GetHeight` (+0x34), y lo mete al principio de la lista global (fn_006A0A60);
   - la posición es fn_006A0AB0: el punto del objetivo con la altura del terreno y, con `TakeCentrePos`, **media altura
     del objeto** más arriba.
2. Recorre sus átomos: el **primero** de la lista, en cuanto tiene edad > 0, suena `SoundHeal` (SOUND_SPELL_HEAL, una
   sola vez por átomo; `SoundSpacing` 0.2 se lee y no se usa). La lista del original crece **por la cabeza**
   (`AtomCollection+0x40`; fn_00674BD0, el `mov [ecx+0x40], eax` de 0x674BEE, llamada desde
   `AtomCollection::CommonInitNewAtom` 0x674C7A), así que ese primer átomo es el **más nuevo**; el vector de openblack
   crece por el final (`PSys.cpp`, `NewAtom`), así que el sonido va en el último (arreglado en la tanda 2 de
   milagros2; un átomo recién hecho tiene edad 0, así que en los dos casos el sonido espera al paso siguiente). Cada
   chakra sigue a su objetivo y, con `ScalePropObjectSize`, su escala de regla (+0x78) es el `Get2DRadius` del objetivo.
3. `fn_006A0E30` da la intensidad del chakra con la edad de su **subcolección** (el estallido del grupo siguiente):
   `t = edad / AtomAgeMaxAlpha` hasta 1 y luego `1 − (edad − AtomAgeMaxAlpha) / (AtomAgeZeroAlpha − AtomAgeMaxAlpha)`,
   recortada a 0..1 (`psys::heal::ChakraFade`). Con ella:
   - los átomos del estallido reciben alfa `int(t × MaxAlpha)` (100.46 → 100 en el pico);
   - el objetivo recibe `SetSpecularColor(int(t × SpecularColor))` (`Living` 0x417480, +0xD0), es decir el brillo
     (200, 255, 255) × t: **el aldeano se ilumina** mientras dura el chakra.
   Cuando el estallido se queda sin átomos (a los 3 s) el chakra se borra, y al destruirse su `AtomData` (fn_006A09A0)
   sale de la lista global y le quita el brillo al objetivo (`SetSpecularColor(0)`).
   `SetSpecularColor` guarda el dword entero (0x417484) y el fundido pone siempre el alfa a 0xFF (0x6A0EF5), así que
   aunque el RGB llegue a 0 el dword no es 0: los dibujos, que prueban el dword entero (fn_0051B3D0 0x51B416,
   `Animal::Draw` 0x51C4D6), siguen viendo un especular propio (que gana al tinte del veneno) hasta el
   `SetSpecularColor(0)` del destructor (0x6A0A28). openblack: `components::SpecularColour` se queda con RGB 0 y solo lo
   quita el destructor (`Heal.cpp` `ClearSpecularColour`; sesión milagros2, petición de shaders).
- Un chakra cuyo objetivo desaparece se borra (0x6A0D7D: además el átomo olvida el objetivo); si un chakra no tiene
  subcolección la regla se suelta (0x6A0E1C devuelve 0).
- El bit 4 de `Objeto+0x24` (0x6A0D8B) también termina el chakra, **sin** olvidar el objetivo, así que el destructor del
  `AtomData` le quita el brillo. Ese bit está identificado: es `GameThingWithPos::Flags` 1 << 2, el
  `UNAVAILABLE_FOR_STATE_CHANGE` de `bw1-decomp/src/Black/GameThingWithPos.h` (solo se lee invertido, en
  `IsAvailableForStateChange`), y el único sitio que lo pone es `GInterface::PlaceObjectInMagicHand` (0x5DA7C1 →
  fn_005DC330 → fn_005DC2A0 → fn_005FAFC0, el `or byte [esi+0x24], 4` de 0x5FB014): **es el aldeano que coge la mano**.
  openblack no tiene esa marca, así que usa el objeto que lleva la mano (`HandSystem::GetHeldObject`) **(aproximado)**.
- El brillo especular se dibuja en `RenderingSystem` / `vs_object`: `components::SpecularColour` va en la y de la quinta
  columna de la instancia (`lh3d_colour::PackInstanceSpecular`, 8 bits por canal, [rendering-objects.md](rendering-objects.md#los-campos-de-color-del-objeto-en-la-instancia)) y se suma al especular de la luz del terreno, como hace
  fn_0080BF10 desde fn_0080BEC0 (`Villager::Draw` fn_0051B3D0, `Animal::Draw` 0x51C4D6).

### `CreateRuleFusedSphericalExplode` 0x69F610

El estallido de cada chakra (5 sprites de `S_SpriteSheet1`, FileOffset 57, 7 cuadros, escala 5, aditivos). Cuando la
colección cumple `FuseTime` (0 en el chakra) hace `NumAtoms` átomos de golpe: dirección de `PSysRandR3` (rechazando el
vector nulo), normalizada, con `|y|` si `OnlyHemisphere`; **una sola velocidad** para todo el estallido,
`MinSpeed + rand(MaxSpeed − MinSpeed)` (0.6 en el chakra), con la y × `ScaleYSpeed`. Solo el primer átomo suena
`SoundExplode`; con `DisableParent` (1 por defecto) el átomo padre deja de dibujarse (marca 0x10). Después la regla se
suelta. Valores por defecto del constructor: NumAtoms 100, FuseTime 20, ScaleYSpeed 2, DisableParent 1.

### `UR_HealInHand` 0x6A0F40

La onda de la mano (SF_HealChakraInHand y SF_HealChakraOnHolder, `WiggleFreq` 0.6; por defecto 1): cada átomo se coloca
en `GetCurrentParentPos × sin(edad de la colección × WiggleFreq × 2π)`, y los átomos impares con el signo contrario. Es
literalmente una multiplicación de la posición del padre, no un desplazamiento.

### Vida y veneno

- `Object::IncreaseLife` 0x637870 (`Villager::IncreaseLife` 0x753460 solo la llama): si vida + cantidad > 1, la cantidad
  se recorta a `1 − vida`; con cantidad ≤ 0 no toca nada; si no, `SetLife(vida + cantidad)`. Devuelve la vida nueva. El
  efecto de curar es `Object::GetHealEffect` 0x637D80 = `EV.heal × defenceMultiplierHeal` (R9 resuelta: no hay más
  recorte que el 1).
- Con efecto de curar 1 (la fila HEAL de info.dat), un aldeano a 0,3 de vida pasa a 1 de golpe.
#### Veneno (`ECS/Life.h`, `ecs::life`)

- Envenenado: bit 1 de `Living+0xB4` (`Living::IsPoisoned` 0x416F90 vt 0x4A4 / `SetPoisoned` 0x416FA0 vt 0x69C; el
  `Object::SetPoisoned` 0x402780 de las vasijas es otra marca, `components::Pot::poisoned`). En openblack es
  `components::Poisoned`, con `ecs::life::IsPoisoned` / `SetPoisoned`, y `ApplyDefaultSpellEffect` (0x720E34) lo quita
  con HEAL o HEAL_PU_ONE.
- **Quién envenena**: no hay ningún hechizo que envenene. Las dos llamadas a `SetPoisoned(1)` sobre un vivo son
  `Villager::AddResource` 0x7564D0 (0x7564E5..0x7564F3: comida con la marca «envenenada», p. ej. un montón envenenado
  que le ponen en las manos) y `Villager::GetResourceFrom` 0x753390 (0x7533E8..0x7533FC: coger un recurso de un objeto
  cuyo `IsPoisoned` es 1 — `Pot::IsPoisoned` 0x55D4E0, `StoragePit::IsPoisonedResource` 0x733550 —, que además pasa el
  `IsSpeedUp`). Es decir, **se envenena al recibir la comida, no al comerla**. Comer solo cambia la animación:
  `Villager::EatFood` 0x75C00E y `EatFoodAtHome` 0x75C0BE eligen 0xD4 en vez de 0xA3 / 0x26 si está envenenado. En
  openblack el lado de comer es de la sesión de aldeanos: la API es `ecs::life::TakePoisonedResource`.
- **Lo que le hace el veneno**: `Villager::CheckHungry` 0x75BCC0 aplica el daño del hambre también cuando el aldeano
  **no** tiene hambre, solo por estar envenenado (0x75BD83..0x75BD9E). La cantidad (0x75BDA6..0x75BDE0) se escribe
  `max(1 − comida / hungryForFood, 1) × hungerToLifeMultiplier`, con el `max` leído en 0x75BDBB..0x75BDCA (`fcom 1.0`,
  `test ah,0x41`, `je` se queda con el valor solo si es **mayor** que 1): como la comida nunca es negativa
  (0x75BD59..0x75BD6E la recorta a 0), el primer término nunca gana y **el daño es `hungerToLifeMultiplier` fijo** por
  comprobación periódica. Portado como `ecs::life::ProcessPoison`, llamado desde `CheckHungry` (el resto de
  `CheckHungry` sigue siendo V4 de la sesión de aldeanos). Además `Villager::DoSleeping` 0x760DB6 **se salta la
  recuperación de vida al dormir** mientras está envenenado (ese estado no está portado).
- **El tinte**: fn_0051B3D0 (el ayudante de dibujo de `Living::Draw` 0x51AEEC y `Villager::Draw` 0x51BA74/0x51BAD5), en
  0x51B43D..0x51B45D: si el vivo **no** tiene color especular propio (`Living+0xD0`, el del chakra, que gana) y está
  envenenado, se dibuja con difuso 0xFFE8FFDD (0x51BB50, que el fichero de símbolos llama `Pot::GetPoisonSpecular`
  pero va en la ranura del difuso de fn_0080BEC0, la misma que usa el blanco 0xFFFFFFFF) y especular 0xFF001000
  (0x51BB60, que el fichero de símbolos llama `Object::GetFireEffect`); fn_0080BF10 **suma** el especular al color del
  objeto canal a canal con saturación (0x80BF2D..0x80BF68). En openblack son los datos `ecs::life::k_PoisonDiffuse` y
  `k_PoisonSpecular`: **dibujarlo es de la sesión de shaders** (`LH3DColor`), todavía no se ve.
  El `Pot::GetPoisonColor` 0x80BEC0 del fichero de símbolos no es un color: es el «pinta con estos dos colores y
  `AddForDrawing`» del objeto 3D.

### Sin portar / sin verificar (curar)

- **La niebla (`ParticleMistCreator`) no hace falta para curar**: SF_HealChakra solo usa `ParticlePointCreator` y
  `ParticleSpriteCreator`. Queda para el agua y el tiempo.
- El power-up (SF_HealChakraPU) añade la malla `MSH_S_HEAL_MESH` (532, la seta) con `UR_KPStretchHeight` 0x6A50C0 y
  `UR_KPMoveAtoms` 0x6A60B0 (interpolación de puntos clave `KPSplineInterpolator::EvalAtT` 0x6A7EB0, un spline cúbico
  con el factor 1/6; `MovePropAtomIndex` escala el desplazamiento por `índice / (NumAtoms − 1)`) y el sonido
  HEAL_MUSHROOM. **Ya está portado** (lo de «desensamblado, no portado» estaba desfasado, auditoría de milagros2):
  las dos reglas se registran en `PSys/Rules/KeyPoints.cpp`, el creador `ParticleMeshCreatorAnimTextured` está en
  `PSys/Creators/Mesh.cpp` y el `SoundOfCreate` en `PSys/PSys.cpp`. El chakra funciona igual en el PU (radio 35, hasta
  100 objetivos).
- La malla 532 es tipo 4 con el byte +5 = 5 (dos caras) y 192 de sus 512 triángulos miran hacia dentro, pero el original
  **no le cambia el material**: `ParticleMeshCreatorAnimTextured::CreateLH3DObject` 0x6A8D20 solo hace
  `LH3DObject::Create` y seis llamadas de su vtable, ninguna a `GJUtils::SetMaterialProperties` 0x57E220 (el cambio
  4 → 13 del escudo físico). openblack ya lee la marca de dos caras de la propia malla (`L3DSubMesh.h`, byte +5 bit 0),
  así que aquí no hay nada que arreglar (H8 cerrada).
- `CanBeHealedByHealSpell` (vt 0xB18) es `!IsDead` en `Living` (0x5EE550 → vt 0xAF4, `Living::IsDead` 0x417270) y da 0
  en **toda la clase `Dove`** (`Dove::CanBeHealedByHealSpell` 0x41EAB0 es un `xor eax,eax`), no solo en la paloma del
  hechizo: ningún pájaro se cura (cuervo, paloma, golondrina, pichón, gaviota, murciélago, los dos de hechizo y
  también el **buitre**, `class Vulture : public Dove`, cuya vtable 0x8BB7F8 +0xB18 es 0x41EAB0).
  Portado en `CastRules.cpp` con `ecs::animal_ai::IsFlyingSpecies` + el buitre aparte (IsFlyingSpecies no lo
  incluye). CitadelDove / CitadelBat: clase sin identificar, openblack no los crea (inferido: se curan). La criatura da
  1 (M8).
- Sigue inferido que los muertos no se curan: openblack no tiene `IsDead` (los estados de morir no están portados), así
  que un aldeano o animal vivo siempre pasa el filtro.

### Ganchos, pruebas y capturas

- `OPENBLACK_TEST_HURT_VILLAGERS="x,z,radio,vida[,envenenado[,turno[,curar[,repetir]]]]"`
  ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)): hiere a los aldeanos del radio,
  puede lanzar el milagro ahí mismo y escribe cada cambio de vida, veneno y brillo.
- `test_heal`: la curva del chakra (`ChakraFade`, el alfa 100 del pico y el brillo 160 a 1,2 s), que la regla mantiene
  el efecto vivo sin átomos hasta el `CloseDown`, la mecha y la velocidad única del estallido, la onda de la mano, los
  dos colores del tinte del veneno con su daño por comprobación (`Heal.poisonData`) y, con `OPENBLACK_GAME_PATH`, los
  valores reales de SF_HealChakra y SF_HealChakraInHand.
- Capturas en `dev\_audit\magic\`: `m4h_heal_chakra.png` (Land1, aldeanos de 1800, 2648 con la cámara en
  `1789,35,2639`: tres chakras encendidos, con el aldeano iluminado dentro de cada estallido) y
  `m4h_heal_trace.log` (vidas 0,300 → 1,000 y veneno curado en los tres, el brillo subiendo 40 → 106 → 160 y bajando a
  0, 18 átomos —1 punto + 5 sprites por chakra— durante 3 s y el hechizo abierto hasta sus 20 s).
- `polish_fix_curar_chakra.png` / `.log` (tanda 2 de milagros2): `OPENBLACK_TEST_HURT_VILLAGERS="1794.4,2646.6,6,0.3,1,200,1,16"`
  con `OPENBLACK_TIME_OF_DAY=12` y la cámara en `1785,33,2640,1794.4,29,2646.6`: el aldeano envenenado iluminado dentro
  de su estallido. En `polish_fix_curar_heal_poison.log` se ve el veneno haciendo daño poco a poco (vida 0,300 → 0,299
  → 0,298 … en los aldeanos que el milagro no alcanza) y curándose en cuanto les llega un chakra.

## Bosque (M4b, `Magic/Spells/SpellForest`, `Magic/Objects/MagicTree`, `ECS/Trees`)

Informe: `resources.md` §3; lo de abajo está releído en el exe (W120, `tmp_dis\miracles\impl\m4b\`) y corrige varias
cosas del informe. La escena de la diosa de los árboles (toma de la cámara) está aplazada.

- **SpellForest** (MAGIC_TYPE 13 NATURE; `GMagicForestInfo::AllocSpell` 0x5FAD90, 0xF8 bytes): +0xEC el Forest,
  +0xF0 «bosque creado» (fn_007254F0 lo pone a 0), +0xF4 el máximo de árboles (`SetMaxObjectsToCreate` 0x7256C0: -1 →
  `finalNoTrees` 18). `InitWithPos` 0x725540 es el de Spell.
  - `CanCast` 0x5FAE80 (vt 0x30): dentro del mapa, tierra, `fn_005FADF0` (ningún Abode de **la celda** del punto,
    `FindType(0)`, tiene `Get2DRadius > distancia` a su centro de fuego) y `ValidPlaceForTree` 0x725C50 (dentro,
    tierra y no `MapCoords::IsFixed`).
  - El radio de `fn_005FADF0` es la vt 0x64 de cada objeto, y **`Field::Get2DRadius` 0x528E80 es la constante 5 m**
    ([0x8AB6E4]): es la única clase que redefine el hueco (las vtables de Object, Abode, Field, Tree y Pot dan
    `Object::Get2DRadius` 0x638180 menos la de Field). Portado en `SpellForest.cpp` `NoAbodeCovers`, que antes usaba el
    radio genérico también para los campos.
  - **`IsFixed` 0x603790 → `MapCell::IsFixed` 0x601EA0 mira solo el primer objeto fijo de la celda** (MapCell +4,
    donde `Fixed::InsertMapObjectToCell` 0x52DEA0 pone el más nuevo con `SetFirstObjectFixed`) y su bit +0x24 & 2, que
    solo pone el ctor de `MultiMapFixed` 0x52E1F0 (`or byte [esi+0x24], 2` en 0x52E207). O sea: `IsFixed` = «el fijo más
    nuevo de la celda es un MultiMapFixed». Las clases que heredan de él, según bw1-decomp (`src/Black/*.h`): Abode (y
    con él Field y StoragePit), BigForest, CitadelPart (CitadelHeart, WorshipSite, CreaturePen, WorshipTotem), Feature
    (y AnimatedStatic), FishFarm,
    MobileStatic (y con él MagicTeleport y las farolas), PFootball, PrayerSite, SpellIcon y TotemStatue; `SingleMapFixed`
    (Tree, MapShield, ScriptHighlight, PrayerIcon) no lo pone, ni GFootpath ni BuildingSite (son GameThing). Un árbol está solo en su celda: una celda cuyo último fijo
    es un árbol no está «ocupada», aunque tenga un edificio debajo. openblack prueba ahora esa lista de componentes
    (`ecs::fire::traits::IsMultiMapFixed`, el rasgo común; antes solo preguntaba «no es un árbol», lo que ocupaba la celda con cualquier
    SingleMapFixed) y cuenta los árboles del propio evento. **(aproximado)** el más nuevo sigue siendo el del índice de
    creación: el grid de openblack es un `unordered_set` que se reconstruye entero (`ECS/Map.h`) y no guarda orden de
    inserción. Solo se nota con un objeto que salió del mapa y volvió sin crearse de nuevo (cogido y soltado): en el
    original vuelve a ser el más nuevo, aquí conserva su índice.
  - `SpellEvent` 0x725830: nada con el tipo 1 o si ya hay Forest; `ApplyDefaultSpellEffect` (paga costPerEvent 1;
    EffectValues de NATURE: alineamiento 1; reacción 21) y, si aplica, **todo el bosque de golpe**: N = `fn_00725790` =
    round(+0xF4 × (fuerza > 0)); paso = N > 1 ? 1/(N − 1) : 1; vueltas = N × **17/13** (el float 0x9819FC =
    1,3076923) × 2π; para i < N: f = i·paso, r = 2 + 9·√(1 − (1 − f)²), a = f·vueltas, punto = castPos (+0xC0) +
    (r cos a, r sin a) truncado a MapCoords (ftol × 6553,6); si `ValidPlaceForTree` y el bosque tiene menos de N
    (fn_00725800), `fn_00725600(punto, GetRandomTreeInfo(castPos))`.
  - `GetRandomTreeInfo` 0x725580: `Terrain::GetMaterialInfo` 0x735330 de la celda del **punto de lanzamiento** (no de
    cada árbol) y `magicTreeTypes[GameRand(4)]` (un GameRand por árbol). `GetTerrainMaterial` 0x735380: 27 (nieve) si
    `GClimate::GetSnow ≥ 27` en la esquina de la celda; si no, el tipo del segundo material de la altitud de la celda en
    su país (fn_00804CF0 → fn_00873770, como `GSoundMap`); **un 0 (fuera del mapa o sin bloque) pasa a 1**, que es
    DEEP_WATER: palmeras; `> 43` → 0. Hierba (18) y casi todos: haya, abedul, cedro, cedro; arena y agua: palmeras;
    nieve y hielo: coníferas.
  - `fn_00725600`: con el primer árbol crea el Forest (`new 0x58` → fn_005399E0(pos, creador): el jugador del
    creador, id = contador 0xBEA238++, al principio de la lista g_game +0x205BB4) y marca +0xF0. Luego
    `fn_005FD000(pos, hechizo, info, bosque, GameFloatRand(2π), 0, woodValueMultiplier 0,25 × poder tribal)` y la
    escala objetivo +0x64 = `fn_007255C0` = **1 − 0,5 × distancia/11**: 0,91 el primero (a 2 m) y 0,5 en el borde (el
    plan decía 1,0 en el centro: no hay árbol en el centro).
  - `Process` 0x7259C0 = `CoreProcess` + `ProcessTrees` 0x725A30 (su argumento no se usa): si el bosque se borró (+0xA
    **bit 0**, el de ToBeDeleted; el informe decía bit 1) → +0xEC = 0 y CloseDown; sin bosque devuelve 1 mientras haya
    PSys y 5 (se borra el hechizo) cuando no; con bosque compara N con los árboles (+0x4C + +0x54, sin signo):
    N < árboles → `fn_0053A490`: **todos** menguan `decaySpeed` 0,05 (fn_0074A3A0: escala − d; ≤ 0 → ToBeDeleted);
    si no → `fn_0053A520`: **todos** `Tree::Grow(growSpeed 0,01, 0, 0)`. El número que reciben las dos no se usa.
    Devuelve 1.
  - `CalculateCostToMaintain` 0x7259E0 = costPerGameTurn (5) + árboles × costPerEvent (1): 23 con 18.
  - `CloseDown` 0x55D1E0 = CoreCloseDown + creador = NULL. **Los árboles no se quedan** (el informe decía que sí): sin
    creador `GetSpellStrength` 0x720750 da 0, así que N = 0 y todo el bosque mengua 0,05 por turno hasta desaparecer;
    con el último árbol se borra el Forest y al turno siguiente el hechizo (los hechizos cerrados se siguen
    procesando: `ProcessSpells` solo salta los que no están `IsAvailable` 0x401810, es decir, los borrados). Un bosque
    de un jugador (`timerWhenPlayerCasting` -1) dura mientras se pague; el de un milagro de un uso (120 s) o el de un
    guion con duración se va al cerrarse. Traza real (duración 8 s): cierra en el turno 81, mengua desde ~0,4 y en el
    88 no queda ninguno; el hechizo se borra en el 89.
  - `ToBeDeleted` 0x725500: borra el bosque (si no se está borrando ya) con sus árboles.
  - `GetMaxObjectsToCreate` 0x7256F0 = min(+0xF4, bosque ? sus árboles : (+0xF0 ? 0 : 18)); lo guarda la semilla
    (`StoreChantsAndAgeFromSpell`, vt 0x550: `SpellOps::maxObjectsToCreate`). `HasEnoughChantsAndLifeForRecast`
    0x725730 = eso > 0. `AdjustSpellSeedPos` 0x725750 (vt 0x540): altitud = max(altitud, el árbol más alto,
    fn_0053A740 con vt 0x42C `GetHeight`), −5 sin bosque (+0xEC == 0). **Sí tiene llamador** (lo de antes era falso):
    el dibujo de la semilla 0x729020, `call [edx+0x540]` en **0x72906E** (ver la semilla de la mano, abajo). Es la única
    clase que la redefine: el puntero 0x725750 sale una sola vez en la imagen (vtable 0x8F4FE4 + 0x540) y las otras doce
    vtables de hechizo tienen `Spell::AdjustSpellSeedPos` 0x55CE60 (`ret 4`). openblack:
    `spell_forest::AdjustSpellSeedPos`.
- **La semilla de la mano sobre el bosque (fiel, lane «mano» de milagros2).** Hay dos semillas: el átomo `Seed.L3D` del
  PSys (cae girando y se borra al tocar tierra, ver abajo) y la SpellSeed `I_Forest` de la mano, que es la que se
  queda y se recoge.
  - Al lanzar (HAND_POSITION) la SpellSeed sale de la mano (0x16) pero sigue atada al hechizo (`deleteSeedOnceCast` 0)
    y al icono (+0x5C: solo lo borra `ClearSpellIconLink` 0x7281D0 desde `ToBeDeleted` 0x728280).
  - Cada fotograma `GGame::Process3dEngine` 0x54E023 → `Spell::DrawSpells` 0x7203F0 → `Spell::Draw` 0x720430 (vt
    0x50C) → `DrawSpellSeed` 0x721360 (vt 0x508; con +0xAC salta a) **0x729020**: si `fn_00728FC0` (no en el mapa, no
    IN_HAND ni guardada en la mano, `seedFollowsSpell` +0x120, hechizo abierto, **icono** +0x5C) y +0x60, altitud =
    `fn_006022C0(semilla, 1)` (la cima `GetTopPos` 0x638160 = altitud sobre el suelo +0x1C + `GetHeight` de los
    objetos de su celda, lista +4 y luego +0, que no son vivos ni se mueven y cuyo círculo se solapa: d² < r_obj² +
    r_semilla², 0 si ninguno), vt 0x540, y `LHMatrix::Translation` a (x, suelo + altitud, z), +0x44 = 1, +0x48 = 0 y
    `AddForDrawing(semilla)` 0x63B5D0, que manda la colisión de dibujo: la mano la ve.
  - Corregido el «(inferido) cuenta el suelo dos veces» de la auditoría: la altitud de un MapCoords es sobre el suelo
    (`GetLHPoint` 0x605C40 = `GetAltitude` + y), así que `GetTopPos` también lo es.
  - Resultado: −5 m (bajo tierra) mientras cae el átomo; al salir el bosque, en el suelo en el centro; luego sube con el
    árbol más alto (traza `seed … drawn over spell …`, altitud −5 → 0,18 → … → 16 m).
  - Recoger: `SpellSeed::ValidForPlaceInHand` 0x728580 (neutral con g_game +0x14 & 0x2000, o del jugador de la mano)
    → agarre de 225 ms `GenericPickup` 0x5D2800 (dentro de la influencia, vt 0x714 = 1) → `PlaceObjectInMagicHand`
    0x5DA6F0 → `InterfaceSetInMagicHand` 0x728810 (`StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200:
    el hechizo cierra y el bosque mengua 0,05 por turno). Con árboles guardados se puede lanzar en otro sitio.
  - openblack: `magic::seed::DrawSpells` (`Magic/Core/SpellSeed.cpp`, desde `magic::Update`), `FollowsSpell`,
    `ValidForPlaceInHand`; la mano: `HandSystem::FindObjectUnderHand` (`HandPlacement.cpp`) y `PickUpSeedOrStone`
    (`HandApplyToObject.cpp`).
  - **(aproximado):** `IsMoving` vt 0x174 de un objeto fijo se toma como «está en físicas» (openblack no guarda la
    posición del turno anterior); las listas de la celda son las de `effects::ObjectsInMapCell` (aproximado allí).
    **(inferido):** +0x44 / +0x48 del Game3DObject (alfa y banderas de dibujo; no es la escala, que es +0x50).
  - Sin portar en `Spell::DrawSpells`: fn_0064AF20 (por jugador y neutral, los seis huecos +0x34 de player +0xA48 con
    fn_0077B3B0; sin identificar), fn_00682950 (la colisión invisible de las bolas de fuego, lista g_game +0x205C9C,
    fn_00682F30) y la vt 0x108(1) del objeto +0xB0 en `Spell::Draw`. fn_00725FE0 es un `ret`; fn_0072BF50 son los
    escudos (`map_shield::DrawShields`).
  - Solo con una semilla de icono: la de un orbe de un uso o `OPENBLACK_TEST_SEED` (sin icono) ni se dibuja ni se
    recoge, como el original. `OPENBLACK_TEST_SPELL` (guion) no crea semilla.
  - Capturas (`dev\_audit\magic\`; `OPENBLACK_TEST_WORSHIP_SITE="NORSE,NATURE"`, `OPENBLACK_TEST_TOWN_SPELL="0,13"`,
    `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="NATURE,200"`, `OPENBLACK_TEST_CAST="press@26,release@26.3,..."`,
    `OPENBLACK_MOUSE_AT=0.5,0.5`): `polish_fix_mano_forest_side.png` (cámara `1772,52,2604,1790,36,2625`, 7 s después:
    la semilla sobre las copas), `polish_fix_mano_forest_hover.png` (cámara cenital `1792.4,85,2626.8,1792.4,30,2627.8`:
    la mano sobre la semilla del bosque crecido) y `polish_fix_mano_forest_picked.log` (`Hand: picked up spell seed
    2702 … InterfaceSetInMagicHand 1`, el hechizo `closed true`, `trees 18 wanted 0: decay`). Prueba:
    `test_spell_forest` `seedFollowsSpell`.
  - La espiral del átomo al caer no se pudo ver en una captura fija (inferido igual que el original por el código de
    `RotateAxis` y `Seed.L3D` descentrada).
  - `fn_00725B20` / `fn_00725BF0` (un árbol en un punto al azar del anillo 2..11 m) no tienen llamador.
- **MagicTree** (0x74 bytes; `MagicTree::MagicTree` 0x5FCF50, creado por fn_005FD000): `Tree(pos, info, bosque,
  maxScale 1,0, ángulo, escala 0)`, bandera mágica +0x5C | 2, +0x70 = multiplicador de madera, +0x6C = el jugador del
  hechizo (sin hechizo, el de g_game +0x205A5B) y `CreateReaction(this, REACT_TO_MAGIC_TREE 8, GetPlayer, 0)`.
  - `ToBeDeleted` 0x5FD070: sus reacciones, `Tree::ToBeDeleted` y, si el bosque sigue disponible y se quedó sin
    árboles, el bosque.
  - `StartOnFire` 0x5FD0D0 quita la reacción 8; `EndOnFire` 0x5FD0E0 la crea otra vez salvo si la bandera g_game
    +0x14 & 0x8000 está puesta (0x5FD0EB `test ch, 0x80` se salta el `CreateReaction`). **Esa bandera es «estoy dentro de
    `GGame::ClearMap` 0x552BB0»:** se pone al principio y se quita al final (bw1-decomp `src/Black/Game.cpp` 1296 y
    1388), y por eso los objetos que se destruyen al cambiar de tierra no crean reacciones ni avisan a su pueblo
    (`Abode.cpp` 92 y 105) ni nada parecido. openblack solo llama a `EndOnFire` desde el sistema de fuego
    (`ECS/Fire/FireEffect.cpp`, cuando se apaga un fuego), nunca mientras limpia una tierra: la condición se cumple
    siempre y no hay guarda que portar.
  - `GetWoodValueMultiplier` (vt 0x868) = +0x70; el de Tree 0x74B810 da 1. `Tree::GetWoodValue` 0x74B7B0 = vida × eso
    × woodValue × escala × [0xD1A294]; `HandSystem::DepositInStore` ya lo multiplica. `GetImpressiveType` 14.
- **Tree** (`ECS/Trees` de la sesión «arboles», `components::Tree`): el ctor 0x749E00 recibe (pos, info, bosque, maxScale → +0x64,
  ángulo, escala); si maxScale ≠ escala marca «creciendo» (+0x5E bit 0) y la cuenta +0x60 (int16) =
  GameRand(growsAfterNumGameTurns); luego `Forest::AddTree` y +0x5E | 2. `TreeArchetype::Create(forestId, pos, info,
  noEscénico, ángulo, maxSize, size)` hace lo mismo; `MagicTree` lo llama con el id de su bosque, maxSize 1 y size 0.
  - `Tree::Grow` 0x74A3F0 (cantidad, setScale, subirMax): con subirMax, maxScale = max(maxScale, escala + cantidad);
    nada si la escala ya es maxScale; si no, min(escala + cantidad, maxScale) con `SetScale` (vt 0x124) o con
    `SetJustScale` (vt 0x51C) y la matriz 3D rehecha con la escala, el ángulo Y y la posición. Devuelve lo que creció.
  - `Tree::Process` 0x74A290 (vt 0x5FC, lista de los que crecen): baja la cuenta; en 0 vuelve a
    growsAfterNumGameTurns y, si crece por debajo de maxScale, `Grow(cantidad, 0, 0)`; devuelve 1 si sigue creciendo.
    La cantidad se arma en 0x74A2D7..0x74A33E: `growthAmount` (info +0x11C) × (1 + 0,01 [0x8C5840] ×
    `rainingAcceleratorMultiplier` (info +0x130) × `GClimate::GetMaxRainingOrSnowing` 0x771600) × (1 + 0,5 [0x8AA3B4] ×
    `MapCoords::GetAlignment` 0x6057B0). Las dos consultas se hacen en `MapCoords::GetLHPoint` 0x605C40 del propio árbol
    (el suelo más su y).
  - **`MapCoords::GetAlignment` 0x6057B0 (el «alineamiento del sitio») es del terreno, no de un jugador:** empieza en 0 y
    por cada jugador que existe (orden de `GGame::GetNextPlayer` 0x5508A0) suma
    `Influence::CalculatePlayerInfluence(pos, jugador, 0, tipo 0, aliados 1)` 0x5CD170 × `GPlayer::GetAlignmentValue`
    0x64D6A0, y recorta a −1..1 (0x60580F y 0x60582C). Es decir, el bien o el mal de quien manda allí, pesado por su
    influencia. (`fn_00605850` lo pasa luego por `GetDiscreteAlignmentValue` 0x414730, que no se usa aquí.)
  - Portado (lane «bosque2» de milagros2): `ecs::TreeGrowthAmount` (`ECS/Trees.h`, con prueba en `test_spell_forest`
    `treeGrowthAmount`), la lluvia con `weather::GetMaxRainingOrSnowingAt` y el alineamiento con
    `effects::alignment::LandAlignmentAt` (nuevo en `ECS/Effects/Alignment`). Con `OPENBLACK_TREE_TRACE=1` cada turno de
    crecimiento escribe `growth <a> = <growthAmount> x rain <r> (mult <m>) x alignment <al>`. Alcanza también a los
    árboles del milagro, como en el original (su bosque es uno más de la lista).
  - `Tree::ToBeDeleted` 0x74A210: sale de su bosque (fn_0053A220) y de la lista global de árboles g_game +0x205CDC.
- **Forest** (en openblack, los ids de `ECS/Trees`: `CreateForest`, `IsInForest`, `GrowAllTrees`, `ShrinkAllTrees`,
  `TallestTreeHeight`; los árboles guardan el id en `forestId` y no hay listas; 0x58 bytes: id +0x40, siguiente +0x44, `Trees0` +0x48/+0x4C
  los crecidos, `Trees1` +0x50/+0x54 los que crecen):
  - `AddTree` 0x53A310: árbol +0x68 = bosque; a `Trees1` si crece por debajo de maxScale, si no a `Trees0`; cada
    lista ordenada por `DistanceToForest` 0x53A890 (distancia al bosque, 0 sin bosque): el nuevo va delante del primero
    que esté más lejos. `fn_0053A220` lo saca.
  - `Forest::ProcessForests` 0x539D70 (ranura 5 del turno) → `Forest::Process` 0x539DA0: un bosque vacío (con +0x38 =
    0) pone +0x34 = 2000 y lo baja cada turno; por debajo de 2 se borra. Si no, y con +0x3C ≠ 1, `Tree::Process` de
    cada árbol que crece; los que devuelven 0 pasan a `Trees0`. Luego los árboles nuevos naturales (cada 2000 +
    GameFloatRand(1000) × 0,05 × crecidos, junto a uno al azar, fn_00539FD0 / fn_0053A010, y
    `GPlayer::FUN_0064da80(14, 1)` 0x539FB7 al jugador más influyente del árbol nuevo,
    `CalculateMostInfluentialPlayer` 0x603830 en 0x539F9B, si no es neutral) los hace `ECS/Trees` también en los bosques del milagro: un árbol
    nuevo en uno de ellos hace que haya más árboles que N y el bosque entero mengua, como haría el original. +0x38 y +0x3C no se sabe qué son (0 en el ctor).
  - `ToBeDeleted` 0x539C60: `ToBeDeleted` de cada árbol de las dos listas y sale de la lista.
  - **No hay unión de bosques**: el milagro siempre hace un Forest nuevo (`CreateForest(0, punto del primer árbol)`).
  - Con `ECS/Trees`: `ShrinkAllTrees` borra con `DeleteTree` (Tree::ToBeDeleted 0x74A210) y avisa a los oyentes;
    el de los milagros (`MagicTree.cpp`, `AddTreeDeletedListener`) quita sus reacciones (0x6E4750), suelta la mano y
    apunta el bosque de un MagicTree borrado. `SpellForest::Process` borra entonces ese bosque si se quedó sin árboles
    (`DeleteForest` = Forest::ToBeDeleted 0x539C60; la última parte de MagicTree::ToBeDeleted 0x5FD070) y cierra al
    turno siguiente, como el original. `SpellForest::ToBeDeleted` borra el bosque con `DeleteForest`. El oyente
    recibe cómo se va el árbol: `Removed` (la entidad se destruye: también se van su fuego, FireEffect::ToBeDeleted
    0x72EBE0, y la mano lo suelta) o `BecameDeadTree` (`FellTree` o el árbol muerto de la mano: el DeadTree se queda la
    malla y el fuego, fn_00730960 en su ctor 0x510880, y deja de ser MagicTree). El multiplicador de madera del MagicTree
    (0,25 × poder tribal, +0x70, 0x5FD0C0) va en `Tree::woodValueMultiplier` y el DeadTree lo copia (+0x9C, 0x5108F7).
  - **El jugador del Forest no hace falta** (esto corrige la auditoría, que lo daba por el alineamiento de los árboles
    nuevos): fn_005399E0 lo guarda (0x5399F4 `GetPlayer` del creador → el ctor de GameThing fn_0046B8A0), pero
    `Forest::Process` 0x539DA0 no lo lee nunca; la estadística 14 de un árbol nuevo (0x539FB7) va al jugador más
    influyente de ese árbol y `GPlayer::FUN_0064da80` sale enseguida fuera de una partida multijugador
    (`IsMultiplayerGame` en 0x64DA90). (inferido) no se encontró otro lector. openblack sigue con `CreateForest(0, …)`.
- **El efecto SF_Forest sin la escena** (lo que se ve): la semilla `Seed.L3D` (escala 0,207) cae desde 9,4 m con
  gravedad 1,6 (máx. 3,35 m/s) girando, con SOUND_SPELL_FOREST_1 y un disco de manchas azules; al tocar tierra (turno
  41, 4,1 s) salen los 18 árboles. **Todo lo demás cuelga del átomo de la cámara** (grupo 2,
  `ParticleAnimWithCameraCreator`: `Tree_Goddess_test.anm` + `Forest.cam`, pausa 4 s, alfa de 0 a 252 entre 4,1 y
  9 s y de vuelta a 0 entre 22,1 y 23,5 s, vive 25 s): a los 3,5 s `AddSubCollectionsToAtom` añade las chispas, el
  vórtice (`Vortex.L3D`, 6) y la raíz de chispas (SOUND_SPELL_FOREST_4); a los 5,5 s las mariposas
  (SOUND_SPELL_FOREST_3; `UR_ForestPath` + `UR_Flocking` + `ParticleGoodEvilCreator` → `ParticleAnimCreator`
  S_Butterfly_Flap.anm, o murciélagos M_Bat_Flap.anm); y a los 3,5 s el mapa de luz `Forest_LMap.raw` con chispas
  permanentes. Con el creador de la cámara sin portar el átomo existe pero no dibuja ni mueve la cámara, así que se
  ven la semilla, las manchas, las chispas, el vórtice y el mapa de luz. **Sin portar:** la diosa y la cámara
  (aplazado).
- **Las mariposas y los murciélagos sí están** (esto corrige la línea vieja de «sin portar»): `UR_ForestPath` y
  `ParticleGoodEvilCreator` en `PSys/Rules/Forest.cpp`, `UR_Flocking` en `PSys/Rules/Flock.cpp` y `ParticleAnimCreator`
  en `PSys/Creators/Mesh.cpp`. **Y aletean (U7):** `ParticleAnimCreator_Butterfly` / `ParticleAnimCreator_Bats`
  (SF_Forest.txt líneas 726 y 756) traen `AnimFileName` `.\Data\SPELLS\Anims\S_Butterfly_Flap.anm` /
  `M_Bat_Flap.anm` con `PlayAnim 1`, `LoopAnim 1`, `RandomiseInitFrame 1`, `SpeedUpFactor 1`, `InitialScale 4` / 1 y
  las mallas `S_Butterfly.l3d` / `MSH_A_BAT_1`. Cada átomo toca el clip (un ciclo cada 366 ms la mariposa y cada
  800 ms el murciélago) desde un fotograma al azar, con sus huesos (`Particle3DAnim::DrawAt` 0x67A8E0,
  `GetCycleTimeFromFrame` 0x6C85F0). Detalle y lo que queda en
  [Las mallas de partículas](particles.md#las-mallas-de-partículas-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-y-la-cúpula-del-escudo).
  Antes de U7 se dibujaban como malla quieta en su pose de reposo (`polish_fix_bosque2_forest_dry.png`). Capturas del
  aleteo en `dev\_audit\sistemas\u7\`: `forest_t80/_t81/_t110.png` (la cámara de arriba, 80, 81 y 110 turnos después
  de caer la semilla) y `close_t80/_t81/_t110.png` (cámara `1782,40,2612,1790,36,2625`, entre los árboles).
- **Ganchos:** `OPENBLACK_TEST_SPELL=NATURE,x,z` (o `13`), `OPENBLACK_TEST_MAGIC_TURN=<n>` y
  `OPENBLACK_TEST_FOREST_SHOT` ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)).
  Con `OPENBLACK_SPELL_TRACE=1` cada turno escribe `SpellForest trees <n> wanted <N>: grow|decay <suma>` y el evento
  `SpellForest event 3 ...: N trees wanted, M made ..., material m`.
- **Pruebas:** `test_spell_forest` (la espiral con las constantes exactas, la cuantización a MapCoords, la escala
  objetivo, N, GetMaxObjectsToCreate, el coste y AdjustSpellSeedPos; con `OPENBLACK_GAME_PATH`, la fila NATURE: 18,
  0,1, 0,01, 0,05, 0,25, 10000 cánticos, 120/-1 s, y los `magicTreeTypes`). Capturas en `dev\_audit\magic\` (hierba al
  oeste del pueblo de Land1, 1790, 2625; cámara `1772,52,2604,1790,36,2625`): `m4b_forest_saplings.png` (3 turnos tras
  caer la semilla: los 18 brotes en espiral y el destello), `m4b_forest_saplings12.png` (12 turnos: escala 0,12),
  `m4b_forest_40.png`, `m4b_forest_grown.png` (120 turnos: cada árbol en su escala objetivo, 0,91 dentro y 0,5 en el
  borde) y `m4b_forest_decay.log` (duración 8 s: el bosque mengua y desaparece).
  La lluvia del crecimiento, con `OPENBLACK_TEST_WEATHER="1790,2625,100,100"` y `OPENBLACK_TREE_TRACE=1`:
  `polish_fix_bosque2_forest_rain.png` (lluvia sobre el bosque; la traza da
  `growth 0.0300 = 0.0100 x rain 100 (mult 2.00) x alignment 0.000`) contra `polish_fix_bosque2_forest_dry.png` (sin
  tormenta: `growth 0.0100 = 0.0100 x rain 0`, y se ven las mariposas quietas).

## Bandadas (M4c, `Magic/Spells/SpellFlock`, `PSys/Rules/Flock.cpp`)

Los milagros FLYING_FLOCK (24, palomas o murciélagos) y GROUND_FLOCK (25, lobos). Desensamblado en
`dev\tmp_dis\miracles\impl\m4c\` (`flock.asm`, `ground_tail.asm`, `spelldove.asm`, `spellwolf.asm`,
`followtargets.asm`, `flocking*.asm`) y `resources.md` §5. Los animales son animales de verdad del módulo de los
animales (`ECS/AnimalAI.h`: `CreateAnimal`, `MoveTo`, `SetStateRaw`, `SetFinalDestination`, `Destination`,
`SetAlpha`, `Remove`, `SetDeathCallback`); lo que sus clases de hechizo SpellDove / SpellBat (AnimalDove.cpp) y
SpellWolf (AnimalSpellWolf.cpp) hacen distinto vive en `SpellFlock.cpp` (componente `SpellFlockAnimal`).

### El hechizo (fiel)

- **Clases:** `SpellFlock` (SpellWithObjects, ctor 0x7231C0 / fn_00723210) y sus dos hijas: `SpellFlockFlying`
  (AllocSpell 0x723100, 0x120 bytes) y `SpellFlockGround` (AllocSpell 0x723180, 0x110 bytes). Datos
  `SpellFlockData`: +0xF4 la Flock, +0xF8 creados, +0xFC acumulador, +0x100/+0x104/+0x108 último punto de salida
  (MapCoords y altura sobre el suelo), +0x110 el PSys del lanzamiento (solo la voladora).
- **Constantes:** 12 animales por segundo (GetNumToEmitPerSecond 0x7230E0 / 0x7230F0), variación de ángulo 2,0
  (0x723550 / 0x723560), distancia `info+0x60` = 800 m (0x723530 / 0x723540), cuántos = `fistp(numberToCreate ×
  GetTribalPower)` (0x723B80 / 0x724250: 12 palomas, 14 lobos; redondeo al par más cercano), radio de caza `info+0x64` =
  45 m (fn_00723170).
- **`InitWithPos` 0x7232D0:** último punto = la mano (PSysProcessInfo +0xC) en MapCoords (ftol × 6553,6) y su altura
  sobre el suelo; `new Flock(pos, GFlockInfo 0xC5E624, jugador, 0xABA52)` (Flock::Flock 0x52F780: radio 0x50, y el
  hechizo pone la distancia de bandada = el radio, 80); luego `Spell::InitWithPos`. La voladora (0x723A80) además, si
  lanza el interfaz de este ordenador, crea SF_FlockFlyingCastGood/Evil (122 + malo) en el punto, sin hechizo
  (`GJPSysInterface::Create(NULL, ...)`), con su jugador; `Draw` 0x724100 lo avanza cada fotograma y se cierra
  (vt 0x118) cuando ya están todos los animales.
- **Bueno o malo:** alineación del jugador (`GPlayer+0x60 → +8`) `< alignmentSwitch` (−0,2) → malo: partículas 125
  FlockFlyingRainEvil (si no 124, GetParticleType 0x723A30) y **SpellBat** (GAnimalInfo 21), si no **SpellDove** (20).
  La de tierra es siempre 126 FlockGroundDust (0x724290) y **SpellWolf** (22).
- **El bucle de salida** (Flying `Process` 0x723BC0, Ground 0x7242A0), cada turno mientras creados < N:
  1. el hechizo se pone donde está el jefe (el primer miembro de la Flock, +0x40 → +8);
  2. `emit += 12 × 100 ms × 0,001` (en float); el segmento de salida va del último punto a la mano de ahora;
  3. mientras `creados < emit`: `creados++`; `f = (creados − emitAntes) / (emit − emitAntes)`; S = viejo +
     `fistp((nuevo − viejo) × f)` en x, z y la altura interpolada; el punto donde nace lleva un temblor
     `GameFloatRand(0,2) − 0,1` m en x y luego en z (el de destino no);
  4. se salta (contando igual) si el punto con temblor está fuera del mapa (`MapCoords::InBounds` 0x6042C0), si lanza
     un humano y `CalculatePlayerInfluence(pos del hechizo, jugador, 0, 0, 1) <= 0`, o si fn_00723570 falla;
  5. el animal: `fn_00419D10(S, info, sin pueblo, la Flock, edad 0)`; escala = `GetScale()` (1, 0x4247E0) × 2,8 +
     `GameFloatRand(0,2)` (aves) o × 1,5 + `GameFloatRand(0,5)` (lobos), primero el azar; `SetGameAngle(S → T)`
     (GetAngleFromXZ 0x74D240); a la lista del hechizo (delante) y, todos, `AddTarget` al PSys principal;
  6. **aves:** altura del punto = la de la mano; T a `altitudeNormal` (GAnimalInfo +0x278, 45 m). El jefe:
     `SpellEvent{2, su posición, sin movimiento}` (reacción 37 ReactToImpressiveSpell), `SetupMoveToPos(T,
     START_WANDER 0x1F)`, `SetState(0, SPECIAL_MOVE_TO_POS 0x2C)` y la Flock +0x78 = FOLLOW_FLOCK, +0x80 = 3
     (formación), +0x7C = DECIDE_WHAT_TO_DO. Los demás: `SetupMoveToPos(GetDestPos del jefe (+0x80), 0x1F)` y el mismo
     0x2C. **Así que el abanico de ±2 rad es solo el ángulo con el que nace cada ave; todas vuelan al T del jefe**;
  7. **lobos:** nacen en el suelo (altura 0, T altura 0) con `SetPlayer(lanzador)`, una nube
     `CreateSpotVisual(S, 9 MAGIC_OBJECT_CREATED, 1, 0)`; el jefe hace el `SpellEvent 2` y `fn_00420F50(S, T, 45)`; los
     demás `fn_00420F50(S, GetDestPos del jefe, 45)`.
- **fn_00723570 (el destino):** dirección d = la de la cámara (PSysProcessInfo +0x18) si lanza un humano, si no
  `castPos − mano`; y = 0; (1, 0, 0) si |d|² < 0,0001. Lado: humano → v = normaliza(S − castPos) y `cruz = v.z·d.x −
  v.x·d.z`: +1 si > 0,1, −1 si < −0,1; si no (y para la IA) +1 con creados par, −1 impar. Ángulo θ = 2 · creados ·
  lado / N; d girado θ en Y (fn_00518BF0, vector fila: `(x cos − z sin, x sin + z cos)`), puesto a la longitud
  (fn_006805F0). T = S con su celda de 10 m (la palabra alta) movida a `ftol((celda·10 + d)/10)` y la parte de dentro
  de la celda igual; mientras T esté fuera y la distancia siguiente (la mitad) pase de 10 m se repite con la mitad
  (800, 400, ..., 12,5). Devuelve `InBounds(T)`.
- **`SpellFlock::Process` 0x7233D0** (el final de los dos): si no está cerrado, para cada miembro
  `fn_006D0C20(+0x2C, +0x14, GetRadius)` busca un escudo cruzado este turno; si lo hay, `SpellEvent{4, su posición,
  objetivo = el hechizo del escudo}` (`costPerShieldCollide` 50) y **si responde 0 (el escudo aguanta) el miembro
  `SetDying` (vt+0x6A4), que en estas clases es el desvanecido (R15)**; con el escudo roto pasa; en los dos casos una
  chispa en el escudo (fn_006D0AF0). Luego `SpellWithObjects::Process` 0x721290 (su bucle 0x721040 llama a
  `Animal::ProcessBySpell` 0x417700 = `ProcessFadeOut`), y **la bandada** (su +0x14, el centro del dominio de
  `components::Flock`) se pone donde el jefe (0x7234F2..0x723519; la auditoría 4 lo corrigió: se escribía en el
  hechizo, que solo se mueve al principio de cada `Process` de clase, 0x723BE6 / 0x7242C8).
- **Coste** 0x723240: `miembros × costPerEvent (0) + costPerGameTurn` (40 / 35). **CloseDown** 0x723270 =
  SpellWithObjects 0x721300: `CoreCloseDown` y `SetDying` a cada objeto (GetSetObjectsDyingOnCloseDown 0x55CF50 = 1).
  **ToBeDeleted** 0x720FD0: CloseDown, la lista fuera; la voladora borra su PSys del lanzamiento (fn_007239B0).

### SpellDove, SpellBat y SpellWolf (lo que les toca al hechizo)

- **El desvanecido (fiel):** alfa 0..255 en un Zoomer de LH3DLib (SpellDove +0x148..+0x174, SpellWolf
  +0x168..+0x194; empieza en 255, 0x41F280 / 0x420930). `SetDying` (SpellDove 0x41F5C0, SpellWolf 0x420CF0) **nunca
  llama a Living::SetDying**: si el destino aún no es 0, `vt+0xBD4` (fn_0041F2F0 / fn_00420A20, el
  `SetDestinationWithSpeedAndTime(0, 0, t)` del Zoomer: cuártica desde el valor y la velocidad de ahora) con
  t = `GetNumTurnsToDieOver` (20, 0x41F620 / 0x420D50) × 100 ms = 2 s. `ProcessFadeOut` (0x41F4C0 / 0x420BF0) avanza
  0,1 s por turno y con el alfa exactamente 0 hace `ToBeDeleted`. «El destino» es el segundo campo del Zoomer
  (+0x14C / +0x16C; el valor es +0x148 / +0x168): una vez empezado, otro `SetDying` (escudo, CloseDown, la llegada) no
  lo reinicia (corregido en milagros2: el puerto miraba el valor y lo rearrancaba en cada llamada). El color de la malla es `fistp(alfa) << 24 |
  0xFFFFFF` y translúcida si no es 255 (SetColor 0x41F630; SpellWolf::Draw 0x51C6EC): aquí `SetAlpha(alfa / 255)` en
  el turno (el alfa solo cambia en el turno). El animal sigue moviéndose mientras se desvanece.
- **Lobo, el pasillo** (fn_00420F50, fiel): normal `(DZ, −DX)/|D|` de D = destino − inicio ((1, 0) si |D|² < 0,0001;
  la nota de resources.md decía «dirección»: es la normal), desplazamiento `−normal·inicio`, medio ancho 45 m,
  `+0x80` y `+0x148` = destino (0x421054..0x421081; resources.md decía +0x80 = inicio) y luego `SetRunToFinalDest`
  0x4209C0 (0x421084: velocidad = escala × info.speed4 × 1,1, `SetupMoveToPos(+0x148, SET_DYING)`) **en el turno del
  hechizo** (`SetupWolf`: el lobo corre desde su primer turno). Los que siguen al jefe toman su `GetDestPos` (vt 0x860 =
  `MobileWallHug` +0x80, 0x416F70; llamada en 0x724737): **mientras el jefe caza, +0x80 es la presa**
  (`SetupMobileMoveToObject` 0x60ACE3 escribe ahí su `GetWorkingPos`), así que esos lobos corren a ella y se
  desvanecen al llegar a 30 m (leído así; en Land 1 junto al pueblo casi todos se van en 2-4 s). El ctor de la clase
  (AllocSpell 0x4207F0) no usa la edad de fn_00419D10: `fn_0041FD30` recibe `grownUpAge + 1` (0x420816..0x420822: un
  adulto, que caza presas vivas) y el hambre +0xE4 = `info.hunger` (150, 0x420885..0x42088E). `IsPosOnCorridor` 0x420E10: `|normal·p + desp| <= 45` y el punto no más de 45 m
  por detrás de la esquina de la celda de 10 m del lobo a lo largo del pasillo (no del inicio, como decía resources.md).
  `SpellWolf::MoveToPos` 0x421300: `Living::MoveToPos`; si el hambre +0xE4 `>=` info+0x20C (el ctor 0x4207F0 la pone
  así: con hambre desde el principio) `ReactToAnimalFoodNeeds` (vt+0xBC0, la caza del león); y a menos de 30 m
  (0x8BF51C, `GetDistanceInMetres` 0x74CD70 en XZ) del destino final, `SetDying` (el desvanecido).
  `IsHuntingTargetValid` 0x420D60: un Living que no está en DYING/DEAD/DOWNED/BEING_EATEN (0xE, 0xF, 0x11, 0x12) ni
  tumbado (+0xB4 & 0x80), dentro del pasillo y con `IsPosValidForTurnAngle` (vt+0xB3C); además el lobo no se está
  desvaneciendo (+0x16C ≠ 0). Es vt+0xBB4: lo llaman la búsqueda de presa fn_004196D0 (0x419715) y
  `Animal::HuntingMoveToPos` 0x418DB0 (0x418E36, si no vale → vt+0xBB8). **Portado (milagros2, lane bandadas)** con
  ganchos pequeños en los archivos de «animales»: `AnimalPredators.cpp` (`IsHuntingTargetValid`, la rama SpellWolf de
  `Abandon` = `HuntingMoveToPosAbaondon` 0x420F30, `SpellWolfMoveToPos`), `AnimalAI.cpp` (el estado MOVE_TO_POS del
  SpellWolf llama a `SpellWolfMoveToPos`; `Lion::Eat` vt+0xB50 y `Animal::StartWander` vt+0xB48 también para él) y
  `AnimalAIDetail.h` (la declaración). La llegada a 30 m ya no se mira en el turno del hechizo.
- **Otros:** SpellDove/SpellBat `GetTimeToBank` 0,5 s, `ReactToAnimalNeeds` 0x24, SpellBat
  `CanBeFrighteningToCreature` = 1; SpellWolf `SetSpeed` no hace nada, `DecideWhatToDo` / `Wander` /
  `HuntingMoveToPosAbaondon` = SetRunToFinalDest (en `ECS/AnimalAI` y `AnimalPredators`, sesión «animales»).

### Las partículas (`PSys/Rules/Flock.cpp`, fiel)

- **UR_FollowTargets** 0x6A04B0 (ctor 0x6BFA20, propiedades 0x6B1DD0): banderas 2 sin 4 (espera a sus objetivos
  hasta cerrar). Si el efecto no está cerrando, un átomo nuevo por paso: con `RemoveTargetFromManager` (1) se lleva el
  último objetivo añadido (TakeTargetObject 0x671030), si no el primero sin quitarlo (fn_00671080); con
  `UseLHPointTargets` (0) y sin objetos, un punto (que no usa). El átomo guarda su objetivo, su escala de regla = la
  escala del objetivo, y `SoundCreate` suena salvo con `SoundOneOnly` (1) si no es el único átomo de la colección.
  Cada paso cada átomo se pone en su objetivo (GlobalToLocal); un objetivo que se fue se olvida y, con
  `RemoveAtomWhenTargetDies` (0), su átomo se borra. En SF_FlockFlyingRain* (SOUND_SPELL_DOVES / _BATS) y
  SF_FlockGroundDust (SOUND_SPELL_WOLVES) cuelga de cada ave o lobo el rastro de UR_WillowWisp (3 o 5 brillos).
- **UR_Flocking** 0x683580 (ctor 0x683470, propiedades 0x6ABCD0; lo usan SF_Forest, SF_Butterflies*, SF_Flies* y
  SF_CreatureSpellItch*; desensamblado en `impl\m4c\flocking*.asm`): bandada de partículas. Por colección guarda la
  velocidad V de la bandada (CollectionData 0x560EC0 +0x24, empieza en 0). Cada paso (dt el del efecto, [0xD4E0EC]):
  - la media de velocidades y de posiciones C de sus átomos; el objetivo P = el átomo padre (o el origen) en el marco de
    la colección (fn_00674B10);
  - `V = V (1 − dt K_FlockDamping) + unit(P − C) K_IdealVel f(|P − C|, IdealVelAccnType, F_InvertAccnIdealVel) dt`;
  - para cada átomo `a` que tenga otro: la suma por los demás b de `unit(b − a) f(|b − a| LocalScale, NeghbourAccnType,
    NeighbourAccnInvert)` × `K_NeighbourAccn / n`; el más cercano da la evitación `unit(cercano − a) f(|..| LocalScale,
    2, no) K_NeighbourAvoidance`; el centro la atracción `unit(C − a) f(|C − a|, AxisChosen, F_InvertAccn)
    K_CentralAttraction`; y la igualación `(media − (v_a − V_antes)) K_VelocityMatching` (× LocalScale). Aceleración =
    vecinos + igualación + atracción − evitación, recortada a `F_MaxAccn`; `v' = (v_a − V_antes)(1 − dt K_Damping) +
    acel·dt + V`, recortada a `F_MaxVel`;
  - la orientación: con `SpriteRotation` (1 por defecto) `SetAngleY(atan2(−y, x) + π/2)` de v' vista desde la cámara
    (fn_006840E0); si no, `UpdateBanking` 0x684160: `SetAngleY(−π/2) × Rx(alabeo) × Rz(−cabeceo) × SetAngleY(rumbo)`
    (producto de filas de LHMatrix), rumbo = atan2(v.z, v.x), cabeceo = atan2(v.y, |v.xz|) × ReducePitchBy, alabeo =
    atan((a.z v.x − a.x v.z) / |v.xz| / GravityForBanking) con a = (v' − v_a)/dt; el +Z local mira a la velocidad;
  - al final cada átomo avanza `pos += v·dt`.
  La ley fn_00683520 (d, tipo, invertir): x = max(d, 0,01) × ScaleModifier; tipo 0 → 1, 1 → x, 2 → x²; 1/eso salvo
  invertido (los tipos son enteros 0..2, los setters 0x6AC0D0.. no admiten otros; el archivo los escribe como FLOAT).
  Por defecto (ctor): K 1 / 1 / 0 / 1 / 0 / 0 / 0, F_MaxAccn 10, F_MaxVel 100, GravityForBanking 10, ReducePitchBy 1,
  ScaleModifier 1, tipos 0 / 2 / 1, SpriteRotation 1.
- **EventConditionAtomNearVillagers** 0x67D8E0: la posición +0x80 del átomo (en metros, tal cual) en MapCoords está en
  el mapa y alguno de los objetos de esa celda de 10 m es un aldeano. SF_FlockGroundDust la declara pero ninguna regla
  la usa.

### Pendiente / no fiel

- **Hecho en milagros2:** la caza en el pasillo, el arranque en el turno del hechizo, la llegada en el estado del lobo
  (arriba), y la vt+0x6A4 de las tres clases ya sustituye a `Living::SetDying` (`animal_ai::SetSpeciesDying`,
  `ECS/AnimalApi.cpp`; ya no hay DYING ni cadáver para ellos).
- **Ángulo de nacimiento (fiel por lectura):** `SetGameAngle` 0x60DA90 (+0x5C y `SetYAngle`) de `GetAngleFromXZ`
  0x74D240 (punto con temblor, T; 0x7245E8..0x7245FB) = `LHArcTan` 0x74D0C0 de T − S: es `detail::AngleOfMapCoords`;
  la cara dibujada es la de `FaceAngle` de los animales (su convención de `ConvertGameAngleTo3D`).
- **(aproximado)** La posición previa (+0x2C) para el escudo es la del turno anterior del hechizo (F5, sin tocar).
- **(pendiente, animales)** En openblack los lobos persiguen (HUNTING_MOVE_TO_POS 41 contra aldeanos dentro del
  pasillo) pero no llegan al salto: dan vueltas a 30-50 m de la presa. Su `SetSpeed` no hace nada (0x4209B0), así que
  corren a ~1,5 m/turno y el radio de `SetTowardsAngle` (2 × velocidad / giro, 0x418681) es ~29 m; dentro de él la
  reducción de 0x4186B4..0x4186FC casi anula el giro. Leído igual en el original (el radio y la fórmula), pero no
  comprobado contra el juego original: puede ser fiel o un fallo del movimiento de los animales.
- **(inferido)** UR_Flocking con `SpriteRotation` (moscas, picor): la matriz 0xEA1D28 se toma como la rotación
  mundo→cámara (x = v·derecha, y = v·arriba), como hace `UR_OrientSpriteWithVelocity` (Rules/Orient.cpp).
- **Duración real con una semilla de la mano (inferido por las reglas de cánticos, medido):** 5000 cánticos iniciales
  / 40 (palomas) o 35 (lobos) por turno, sin recarga: 12,5 s y 14,5 s, no los 25 s / 60 s del temporizador (el
  lanzamiento de guion es del jugador neutral, que repone: allí se cierra por el temporizador).
- Sin portar: `NeedsContinualPackets` 0x723280 (true mientras `creados < N` si lanza un humano y es el jugador del
  interfaz; si no, `Spell::NeedsContinualPackets` 0x7214C0): decide si el interfaz manda paquetes con la mano; en
  local openblack refresca `PSysProcessInfo` cada turno (`creator::UpdateSpellInfo`), así que no cambia nada (inferido).
  Load/Save tampoco.

### Ganchos, pruebas y capturas

- `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` y `GROUND_FLOCK,x,z` (los nombres del info.dat; o 24 / 25): el jugador
  neutral lanza desde 30 m sobre el punto, así que d = (1, 0) y el lado alterna; `OPENBLACK_TEST_FLOCK_SHOT=
  "<turnos>,<ruta.png>[;...]"` pide capturas esos turnos después del lanzamiento. Con `OPENBLACK_SPELL_TRACE=1` cada
  animal escribe `SpellFlockFlying|Ground <n> #<N> entity e at (x, z) +h m -> (Tx, Tz)` y cada 10 turnos cada miembro
  su posición, estado, alfa, presa (`target`, su distancia) y velocidad; `SetDying (state, final)` dice cuándo y en qué
  estado empezó el desvanecido. Para seguirlos con la cámara: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90"
  OPENBLACK_TEST_ANIMAL_SPECIES=20` (22 los lobos) `OPENBLACK_TEST_VIEW_LOCK=1`.
- `test_flock`: N con el redondeo de fistp, bueno/malo, el abanico (dirección, lado, ángulo, giro), el destino (celda,
  truncado hacia 0, las mitades), la interpolación de salida, el pasillo del lobo (normal, borde, por detrás de la
  celda) y el desvanecido (2 s, 79,7 a mitad, 0 a los 20 turnos); con `OPENBLACK_GAME_PATH`, las filas reales (12/14,
  −0,2, 800, 45, 40/35 por turno, 50 por choque, 25/60 s, reacción 37, altitudNormal 45); y de UR_Flocking la ley
  fn_00683520 y la rotación de UpdateBanking (rumbo, cabeceo reducido, alabeo).
- Capturas en `dev\_audit\magic\` (Land1; lanzamiento de guion desde 30 m sobre 1790, 2625, así que salen hacia +x):
  `m4c_doves_12.png` (12 turnos: las 12 palomas abriéndose desde el punto, con su brillo), `m4c_doves_40.png` (40
  turnos: en hilera hacia el T del jefe, con el rastro), `m4c_wolves_15.png` (los lobos naciendo con la nube de
  MAGIC_OBJECT_CREATED), `m4c_wolves_60.png` (corriendo con el polvo), `m4c_butterflies_75.png` / `_110.png` (un bosque
  en 1912, 2605 junto a los cerdos: las mariposas de UR_Flocking entre los árboles) y los registros `m4c_doves.log`
  (las 12 salidas con su T en abanico; cerrado a los 25 s, el desvanecido y el hechizo borrado a los 27,2 s) y
  `m4c_wolves.log`. De milagros2 (GROUND_FLOCK en 1750, 2625, junto al pueblo): `polish_fix_bandadas_close15.png`
  (15 turnos: los lobos se abren hacia los aldeanos del pueblo, cazando), `polish_fix_bandadas_hunt25.png` (25 turnos,
  más lejos: persiguiendo junto a las casas) y `_hunt60.png` (60 turnos: casi todos desvanecidos al llegar a la presa
  del jefe), registro `polish_fix_bandadas_close.log`.

## Bola de fuego y rayo (M5, `Magic/Objects/MagicFireBall`, `PSys/Rules/{Fireball,Lightning}`)

Informes: `destructive.md` §2-4 y §7, `visuals_sound.md` §4.1-4.2, `psys/part_render.md` §8 y §10. Todo lo de abajo está
leído en el exe (W120) salvo lo marcado UNVERIFIED o «(inf)».

El fuego que encienden (el modelo de calor, `src/ECS/Fire`, sus aldeanos y sus natives CHL) está en
[Fuego](magic.md#fuego-m5-srcecsfire).

### Bola de fuego (MAGIC_TYPE 1-3, semilla 2 FIRE)

- **`MagicFireBall`** (`Magic/Objects/MagicFireBall.cpp`, 0x682970-0x683390): un Object invisible **que no está en las
  celdas del mapa** (por eso `IS_FIRE_NEAR` no lo encuentra), creado por la regla `AttatchFireBallToAtom` y pegado a su
  átomo. Su `FireEffect` empieza en `initialTemperature · fuerza` del efecto y es lo que quema; la fila de
  `GMagicFireBallInfo` (3 filas, 0xD4E3B0) la elige el nivel de mejora. Se borra al bajar de `deletionTemperature` (el
  átomo queda marcado como desviado) o cuando su átomo desaparece.
  - **Coger**: `ValidForPlaceInHand` 0x682DD0 acepta cualquiera que no sea del propio jugador; `fn_00682EA0` crea en la
    mano una semilla FIRE nueva **ya lista** (`fn_00729900(0)`) y cargada, y la bola se va.
  - **Absorber**: `DeleteAndPutIntoSpellSeed` 0x682E10 multiplica el poder de la semilla FIRE sostenida por
    `1 + catchIncreaseFactor`.
- **Reglas del PSys** (`PSys/Rules/Fireball.cpp`): `UpdateRuleGravityWithFloor` 0x6A1880 (**R4**: el rebote usa
  `DampingHorozontalBounce` / `DampingVerticalBounce`, el arrastre del suelo y el sonido de impacto por clase de
  tamaño), `CreateWithInitialDirection` 0x69E950, `AttatchFireBallToAtom` 0x682FD0, `EventAlways`,
  `SetAtomHasBeenDeflected` 0x6A26C0, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`,
  `UR_Trail`, y las condiciones de desviación, de agua cercana y de vapor.
  - `CreateWithInitialDirection` 0x69E950, lanzamiento no humano (guion, ordenador, criatura): la nueva solución a 30°
    se aplica si `|v|² > 0,01` (`fcomp qword [0x8C7620]`, el double 0,01, en 0x69EC60) y `v.y / sqrt(vx² + vz²) >
    tan(0,52370351552963257)` (double [0x9375F0], `fptan` 0x69EC77), todo en la FPU. El port comparaba con 89129 (los 4
    bytes bajos del double leídos como float, fallo del desensamblador ya corregido), así que casi nunca la aplicaba
    (corregido 2026-10-02, `Fireball.cpp`). El tiempo nuevo (0x69EC92..0x69ECC1) es `sqrt((Δy − D·tan)·(−2/g))`, con el radicando a 0,1 si sale negativo (Δ [esp+0x64] 0x69EB1B, D [esp+0x38] 0x69EB3F; releído en la auditoría de fuego2).
- **Revisión de las constantes double** (2026-10-02; todas las direcciones de .rdata citadas en `src/Magic`, `src/PSys`,
  `src/ECS/Fire`, `src/ECS/Weather`, `src/ECS/Effects` y `src/Worship` que el exe lee con `qword ptr`): mal solo
  [0x8C7620] (arriba) y, en el límite, [0x900C70] de `MapShield` (0,30000001192092896 = 0,3f ensanchado, no 0,3). Bien:
  [0x8C79D8] (1e-4f ensanchado: gestos, `PSys/Utility.cpp`), [0x8D45D8] (2π float ensanchado: MapShield, Forest, Storm,
  SpellSeedGraphic), [0x9361E8] (−π/2 float), [0x9003C0] (0,99e30), [0x900AE8] (1,1), [0x980518] (1/30), [0x8AB260]
  (0,5), [0x8CF7D8] / [0x9375E8] (0,6 / 0,3 del tamaño del sonido; `Audio/SpellSounds.cpp` los compara en float, lo que
  solo difiere con una fracción exactamente 0,6f o 0,3f). Lista completa en `tmp_dis\miracles\polish\fuego2_fix.md`.

### Rayo (MAGIC_TYPE 4-6, semilla 6 LIGHTNING_BOLT; `PSys/Rules/Lightning.cpp`)

`UR_Lightning` 0x6914C0 es una regla de creación que mantiene una estructura de horquillas mientras se canaliza. Por
colección guarda un `UR_Lightning_CollectionData` (0xC0 bytes; aquí una entrada con clave en la ranura de la regla, como
las bolas de fuego, para que nada quede colgando).

- **Origen y dirección.** Con `CastingFromHand` el origen es la posición del gesto (`GetCurrentGesturePosn` 0x673600 =
  `PSysProcessInfo` +0x0C); si no, el átomo padre. El rumbo del cono es **fn_00673650 = `atan2(cameraForward.z,
  cameraForward.x)`**, es decir el frente de la cámara (el informe decía «el rumbo de la mano»; verificado ahora: el
  `PSysProcessInfo` empieza en manager+0x24, así que manager+0x3C/+0x44 son `cameraForward.x/z`).
- **Objetivos** (`fn_00690F50`, tres modos):
  - desde la mano (`fn_006901E0`): espiral de `4·ceil(R/10)²` celdas de 10 m desde el origen con R = `SearchRadius` (60
    en el rayo base); vale el objeto disponible que no sea semilla de hechizo (fn_00690090) y cuya dirección horizontal
    cumpla `dot(normalize(dx,0,dz), (cos rumbo, sin rumbo)) > cos(SplitAngle)` — un cono de semiángulo 0,2 rad ≈ 11,5°.
    Para hasta `MaxLightningObjects`;
  - si salen menos de `MinLightningObjects`, rellena con **puntos de suelo**: ángulo `rumbo + rand(pi/4)` (verificado: es
    `+rand(0, pi/4)`, no centrado; la constante es 0x3F490FDB), distancia `rand(0,6·R)`, y = terreno + 2 (0x8AB478);
  - `TakeTargetsFromManager` toma los `SpellTargets`; el modo del padre (fn_00690880) usa `DefaultSearchRadius` 20.
  - Una criatura se salta cuando `creature+0x12B0 < 0,5` (fn_0047ACC0, UNVERIFIED qué significa): no portado.
- **Estructura** (`CreateForkStructure` 0x691190): borra los átomos de la colección, crea **un** átomo y le cuelga
  `2 · nº de objetivos` subcolecciones del `ForkGroup` (0x69119B..0x6911A6), cada una con `MaxJointsPerFork` átomos de
  `ParticleChainCreator`. A cada horquilla le quita la interpolación (`and byte [edi+0x38], 0xFD` en 0x6912A1; también
  0x691B42 y 0x692B1D): **fiel** (`Collection::flags`, bit 2; por defecto 3 en el ctor 0x675CA2). fn_00679920 solo
  interpola entre los dos últimos pasos con ese bit (0x67999E); sin él dibuja el PSR actual, así que el rayo cambia de
  forma de golpe en cada paso (10 Hz), como el original. Con `CastingFromHand` y la mano propia (fn_00691B80) cada
  articulación de cada horquilla recibe un `DrawOffsetLT` (0x69130C..0x69134C): **fiel**, ver abajo. Al rebuscar
  objetivos (fn_00691390) la estructura **no** se rehace: si hay más objetivos que horquillas, la recursión se para
  (0x692786).
- **Orden de cada paso** (`ActualModify` 0x6914C0, fiel): origen y rumbo; la primera vez, búsqueda y estructura; el
  choque con otro rayo (fn_006916B0, 0x6915E5); rebuscar (fn_00691390, 0x6915EE); el sonido; **todas** las horquillas
  se sueltan de la raíz en **cada** paso, esté o no el gestor en estado 2 (0x691651..0x691681; el port decía que fuera
  del estado 2 se quedaban como estaban: era un error, ahora no se dibujan); y solo en estado 2 `UpdateForkStructure`.
- **Cada paso** (`UpdateForkStructure` 0x691BB0):
  - la escala de las horquillas es `ForkScale · FP_ForkScale` (+0xAC);
  - el campo +0x18 de cada objetivo («enfriamiento») baja de uno en uno (0x691C26..0x691C31; con signo, `jle`). Se
    escribe al buscar (`PSysRand(ftol(AverageLightmapLife / ([0xD01A38] · 0,001)))`, 0x6910C9) y en cada golpe
    (0x692935), pero **nadie lo lee para saltarse un objetivo**. Es solo un dato. El port lo usaba como filtro y eso era la causa principal del «va a saltos»
    (corregido);
  - sin choque con otro rayo elige cada objetivo activo con probabilidad `min(+0x68, (N+1)/2) / N` hasta llegar a ese
    número (0x691C5B..0x691CF3). Si el rayo está enlazado con otro (data +0x20 o +0x24, el choque de
    fn_006916B0 / fn_00691AD0) elige **uno**: el primero activo desde `PSysRand(N)` (0x691CF5..0x691D56), **fiel**;
  - antes de la recursión, `PSysRand(2 · horquillas)` elige la colección de +0xA4 (0x691D98..0x691DC3), que nadie usa:
    el port solo hace la tirada;
  - si no se elige ninguno, ese paso no hay horquillas (0x691D59..0x691D6E);
  - rebusca (fn_00691390) cuando `RenewSearchEvery > 0` y la edad desde la última búsqueda (+0x5C, puesta a 0 en
    0x690F66) la supera, o con `RenewTargetsOnMove` si el origen se ha movido más de `RenewTargetsOnMoveFrac · R` desde
    +0x3C. fn_006916B0 no es «rebuscar»: es el choque de dos rayos (lista 0xD4EC60), ver abajo.
- **Choque de dos rayos** (fn_006916B0 / fn_00691AD0, **fiel** salvo el brillo, ver el final):
  - los datos de cada colección van a una lista global, el más nuevo delante (0xD4EC60, cuenta 0xD4EC64; ctor
    0x68FED0..0x68FEEE), con el turno de creación en +0xB0 (g_game +0x205A40, 0x68FEA5) y `CastingFromHand` en +0x59
    (0x68FECA). El dtor fn_0068FF50 los saca y deshace los enlaces;
  - cada paso, un rayo lanzado desde la mano y no enlazado (+0x59, +0x20 = 0) pone +0xB4 a 0 y busca en la lista un
    rayo **más viejo** (+0xB0 menor; del mismo turno no vale) lanzado desde la mano, con objetivos (+0x74) y sin enlace
    o enlazado con él. Con d1 = su centroide (+0x48) − su origen y d2 = su centroide − este origen, el punto de choque
    es `T = centroide − 0,7 · 0,5 · (normalize(d1) + (cos h, 0, sin h)) · min(|d1|, |d2|)`, h el rumbo de este rayo
    (0x6918E8..0x6919F3). Se enlazan si los rumbos van hacia el mismo lado (`cos h' cos h + sin h' sin h > 0`,
    0x6918AF..0x6918E2), `|T − origen| < SearchRadius` (fn_006901D0) y `normalize(T − origen)` está a menos de
    1,0367 rad de h (doble 0x936D90, 0x691A47..0x691A72). Si no, el viejo que estuviera enlazado se desengancha;
  - enlazar (fn_00691AD0(viejo, nuevo, CommonGlowGroup)): viejo +0x20 = nuevo, nuevo +0x24 = viejo, nuevo +0xB4 = T, y
    si el viejo no tenía enlace, un `CommonGlowGroup` en la última articulación de su horquilla 0 (fn_00691B50) sin
    interpolación (0x691B42). Al desenganchar, esa articulación pierde sus subcolecciones (fn_00673B20);
  - en fn_00691F30, prof 0: el nuevo (+0x24) lleva el tronco a su +0xB4 y ahí **para** (ni ramas ni golpe,
    0x692110..0x692136); el viejo (+0x20) lleva el tronco al +0xB4 del nuevo y sigue con **una** horquilla más, prof 1,
    desde allí hasta su objetivo con la escala × this +0x60 = **3,0** (solo el ctor 0x690183 la pone; no es una
    propiedad), así que opaca y gruesa (0x69213B..0x692172, 0x6927EC..0x6928EA). Su golpe tiene fuerza 2 y el mismo
    evento va también al hechizo del nuevo (fn_00690070, 0x692AA0..0x692AB9);
  - `CommonGlowGroup` (el brillo de escala 5 de SF_LightningBolt) **solo** se usa aquí (0x691AAF es su única lectura):
    el port lo ponía en la punta de cada horquilla (inferido, sin base), quitado;
  - (aproximado) el original toca los átomos del otro rayo en el paso del que busca; el port lo deja para el siguiente
    paso del rayo afectado (`ApplyGlow`), así que el brillo puede llegar o irse un turno tarde. Un dato cuya colección
    deja de dar pasos (ninguno este turno ni el anterior, o 10 s de reloj) sale de la lista con la semántica del dtor.
- **Búsqueda** (fiel): espiral `GUtils::Spiral` 0x74D7E0 con `dir = count = 1` (0x6902A7..0x6902BB): primero actualiza
  (`--count == 0 → dir++, count = dir/2`) y luego devuelve `tabla[dir & 3]`, así que el primer paso es −x. fn_006901E0
  recorre `4·ceil(R/10)²` celdas (0x6902A4) y fn_00690880 (tormenta) solo `ceil(R/10)²` (0x690906, sin ×4). Un objeto
  solo cuenta en la celda donde está su posición (fn_00604F40, 0x6903AE / 0x6909B4), así que un objeto de varias celdas
  no entra dos veces.
- **Árbol de horquillas** (`fn_00691F30(prof, data, horquilla, origen, lista, escala)`, desde 0x691DE6 con la
  horquilla 0, prof 0, escala 1 y la siguiente horquilla libre +0xA0 = 1), **fiel**:
  - centroide = suma de las puntas de los objetivos activos de la lista / tamaño de la lista (0x692040..0x69210C);
  - con 2 o más objetivos la horquilla para en `origen + (centroide − origen)·(0,2 + rand(0,4))` (0x69217C..0x6921F2);
    con uno llega a su punta;
  - articulación i: `origen + (corte − origen)·i/(n−1)`; las interiores se desplazan `(rand(2·RandomFrac) −
    RandomFrac)·longitud` **en X y Z** (0x6924D5..0x692511; la wiki decía «Y y Z»: era un error);
  - escala de `S/(prof+1)` en la raíz a `S/(prof+2)` en la punta, `S = escala · ForkScale · FP_ForkScale`
    (0x692366..0x69239E, 0x692515..0x692545): el tronco va de S a S/2;
  - alfa `128 + PSysRand(127)` en todas las horquillas; 255 solo si la escala de la rama es > 1, que solo pasa en el
    choque de dos rayos (×3,0 de +0x60) (0x6923FF..0x692418, 0x692548..0x69257C);
  - con 2 o más objetivos los reparte por el signo de `dot((punta − corte).xz, (centroide − origen).xz)`: > 0 a la
    primera lista, el resto a la segunda (0x6925B9..0x6926F4). Si una sale vacía, se queda el último de la otra
    (0x6926FA..0x69276A). Luego recurre con prof+1 desde la última articulación, primero con una lista y luego con la
    otra, cada una con la siguiente horquilla libre (0x69276D..0x6927DA, en profundidad). Se para si no quedan
    horquillas. Salen `2N−1` horquillas para N objetivos golpeados;
  - con un objetivo, golpe (0x6928F9..): +0x18 = `PSysRand(AverageLightmapLife/dt)`; **fn_00691E80** crea el átomo de
    `LightMapGroup` (`PCreatorLightMapAtom`) en el centroide de la lista, que es la punta. fn_00691ED0 (no es el mapa
    de luz) es el aviso de «impresionante» (`GetImpressiveIntensity` -> fn_00692FA0), una vez por objetivo cuando
    +0x5C > 0,2 (TODO). Luego viene el `SpellEventInfo` tipo 3 en la punta (fn_00691E00) con **fuerza 1**. Es 2 solo si
    el rayo está enlazado con otro (data +0x20, 0x6929DE..0x6929EE). La wiki decía «2 cuando el rayo sale de un objeto
    padre»: era un error, corregido en el código. El mismo evento va también al gestor del rayo enlazado (0x692AA0,
    fn_00690070), **fiel**.
  - Al entrar, la horquilla se vuelve a enganchar a la raíz (0x691F6D..0x691F7D) **antes** de las pruebas: si luego
    sale sin colocarla (le falta el `DrawOffsetLT`, 0x692007, o el terreno, 0x692262) se dibuja con las articulaciones
    del último paso que la colocó. El port hace lo mismo.
  - Escudo en el tramo (0x692268..0x692361), **fiel**: `fn_006D0BC0(corte, 2,5)`; dentro de uno, el punto de entrada
    del tramo desde el origen (vt 0xFC `FindIntersect`, margen 2,5), un `SpellEventInfo` 4 ahí {sin movimiento, fuerza 1
    (2 enlazado), sin prueba de escudos, objetivo el hechizo del escudo (fn_006D0B10)} al hechizo del rayo; si responde
    0, la horquilla acaba en ese punto (ni ramas ni golpe). La chispa en el escudo (fn_006D0AF0) va siempre.
  - `NumTexturesToTile` (this +0x48, ctor −1), **fiel**: si no es −1, la cadena de la horquilla se corta en
    `max(1, ftol(NTT · longitud / data+0x60))` repeticiones (0x6923D0..0x6923FC; `Collection::chainTextures`).
    data +0x60 **sí** tiene escritor: fn_00690F50 la pone a la distancia del origen al centroide de **todos** los
    objetivos (+0x48 = suma de puntas / N, 0x6910BD..0x691177); el 1,0 del ctor (0x68FE7F) solo vale hasta la primera
    búsqueda (la wiki decía que no había otro escritor: era un error). Lo usan SF_LightningStrike /
    SF_LightningStormPush (15).
  - La posición de la punta es `fn_00691E00`: el objeto en su celda de mapa, altura `GetAltitude + (+0x1c) +
    GetHeight`; un punto de suelo, él mismo.
  - Sin hechizo (rayo de guion o de clima) el original aplica los `EffectValues` estáticos de la info 0xCC9704 en la
    punta: **UNVERIFIED** qué fila de `GEffectInfo` es; no portado.
- **Corte con el terreno** (**fiel**, lane «rayo3» de milagros2; `LandIslandInterface::RayCast`, API común en
  `3D/LandIslandInterface.h` + `3D/Implementations/LandIsland.cpp`, revisada por la sesión sistemas). En fn_00691F30
  0x69220C..0x692262, tras elegir el punto de corte y antes de los escudos:
  - `LH3DIsland::RayCast` fn_00802550(origen, corte, &x, &z) pasa los dos puntos a celdas (x, z · 0,1 [0x8AC404],
    y / 0,67 [0xC3720C]) y llama a `RayCastInternal` fn_00802680. Si acierta, el punto en metros (× 10 [0x8AB414],
    globales 0xE9CD80 / 0xE9CD84).
  - `RayCastInternal` lanza un **rayo**, no un tramo: mueve el final a `final + t·(final − inicio)`, con t el primer
    borde del mapa (0 o 512) en x o en z, o sea, hasta el borde más un tramo (0x802683..0x802765). La guarda
    `t == 1e20` compara el float 1e20 (0x60AD78EC) con el double 1e20 (0x9A2BE0), que no son iguales: el final se mueve
    **siempre**. Luego lo recorta a 0,1 .. 511,9 (Cohen-Sutherland, primero x y luego z; tras recortar en z, el código de
    ese extremo pasa a 0 sin volver a mirar x) y lo recorre celda a celda con las cuatro variantes del DDA
    (0x802C80..0x803078: pendientes `dx/dz`, `1/(dx/dz)`, `dy/dz`, `dy/dx`; se cruza el borde en z si la x de ese cruce
    sigue en la columna, si no el de x). Una sola columna o una sola fila prueban cada celda con el tramo entero.
  - La prueba de celda fn_0083AE80 lee las cuatro esquinas del propio bloque (celdas +4, +0xC, +0x8C, +0x94, también
    la fila del borde; `GetCellCorners`). Si los dos extremos quedan bajo la esquina más baja o sobre la más alta, no hay
    choque. Si no, prueba el plano de los dos triángulos (0,0)(1,0)(0,1) y (1,1)(1,0)(0,1):
    - u, v dentro de −0,1 .. 1,1 [0x8C9B2C / 0x8AB230];
    - `u + v ≤ 1,1` en el primero y `≥ 0,9` [0x8C5844] en el segundo;
    - el punto delante del inicio;
    - un denominador menor que 1e-4 (qword 0x8C79D8) en el primer triángulo acaba la prueba sin mirar el segundo.
  - Sin choque, un rayo que baja (fin.y ≤ inicio.y, |dy| ≥ 0,0001 [0x8BF518]) da su cruce con y = 0. Solo cuenta si está
    a ≤ 7500 m (`dx² + dz² ≤ 5,625e7` [0x9A2BD4]) de la cámara (`g_camera` 0xEA1DB8; 0x8025D9..0x802672).
  - Si el terreno queda más cerca del origen (en x z) que el corte (`fcompp; test ah, 1`), sale de **toda** la función
    (0x692262 → 0x692ABE). Esa horquilla no se recoloca: ya se volvió a enganchar al entrar (0x691F6D) y se dibuja como
    la dejó el paso anterior. Tampoco hay ramas, golpe ni prueba de escudos.
  - Los otros siete llamadores del exe, listados en el comentario de `RayCast` para quien los porte: `GCamera::Update`
    0x442406, fn_0044EF60 0x44F046, `CameraModeNew3::Update` 0x45DA4A, `GLandscape::Draw` 0x5E4848, fn_005E5620
    0x5E5660, fn_00800C30 0x800D79 y fn_0086BD00 0x86BF1C.
  - **(aproximado)**: aritmética en float donde el original guarda valores x87 extendidos entre los `fstp`; solo cambia
    algo justo en el borde de una celda o de un triángulo.
  - **(guarda del port)**: un tope de 4·512 + 4 pasos en el recorrido.
  - Con `OPENBLACK_SPELL_TRACE` sale la traza «fork at depth N ... cut by the land at (x, z)».
- **`IsAvailable`** (fiel, rayo3): `SearchAround` (fn_006901E0 / fn_00690880) pregunta `IsAvailable` (vt +0x2C,
  0x69038E / 0x690997) antes de la celda propia y de fn_00690090. Es `GameThing::IsAvailable` 0x401810 (no se está
  borrando) y, para un aldeano, `Villager::IsAvailable` 0x751D50 (no está DYING, `ecs::villager::IsAvailable`). El modo
  del gestor (fn_00690C70, sin leer) no lo usa en el port.
- **Enfriamiento** (rayo3): `PSysRand` 0x6729E0 es la función de [0xD4E0BC] (`GameRand` 0x672AF0 o `LocalRand`
  0x672B40, según fn_00673340):
  - con n = 0 da 0 sin tirar (0x510693 / 0x6DE574);
  - si no, `LHRand % n` **sin signo** (`div` 0x7DB62B);
  - el `ftol` de `LightmapSteps` le llega tal cual (0x691091 / 0x69292F). Un turno de 0 ms da +inf, el ftol da
    0x80000000 y la tirada sale en 0 .. 2³¹−1;
  - el comentario anterior («sin enfriamiento») era *(inferido)* y estaba mal;
  - el port lo hace con `PSysRand(effect, n)` en `Lightning.cpp`. **(aproximado)** sale del generador float del efecto,
    la convención de este PSys.
- **`DrawOffsetLT`** (**fiel**; `Atom::drawOffset` en `PSys.h`, sumado en `Effect::Collect`/`CollectChains`). Cuando se
  lanza desde la mano propia (fn_00691B80 = `CastingFromHand` y `NetUnsafeIsMyInterfaceCasting` 0x673540: el +0x44 del
  hechizo, 1 sin hechizo), cada paso llama a `SetRefPos` 0x6C7600 (vt 0x100) en cada articulación de la horquilla con
  `(origen del paso = data +0x30, peso w − w·i/(n−1))`, w = 1 en el **tronco** (prof 0) y **0** en las ramas
  (0x691FC7..0x69203E), con el peso recortado a 0..1 (un NaN da 0). Cada fotograma, `GetOffset` 0x6C7690 da
  `(posición de la mano de mi interfaz ahora, GInterface +0x3A0 = CHand, +0x78, − origen del paso) · peso`, que
  fn_00679920 suma a la posición dibujada, interpolada o no (átomo +0x124, 0x679B69..0x679BBF): el arranque del rayo
  va pegado a la mano entre pasos y la punta no se mueve. En el port la mano es la del `HandSystem` (la misma de la
  que sale `PSysProcessInfo` +0x0C). `DrawOffsetDecay::ProcessList` 0x68F680 no tiene que ver: es el reloj de la otra
  subclase (`DrawOffsetDecay`). Otras reglas que crean `DrawOffsetLT` y aún no lo usan: `UR_FollowCastPosn` 0x69FEAD
  (el sprite del origen de SF_LightningBolt, `Rules/HandFollow.cpp`) y `UR_HandSprinkle` 0x6A046B.
- `UR_LightningStrike` 0x6937A0 (SF_LightningStrike / SF_LightningSingleStrike, el rayo de guion y de clima) solo crea un
  átomo con sus `NextGroups` —donde vive el `UR_Lightning` del golpe— y su `SOUND_SPELL_LIGHTNING`, una vez.
- **Números por evento** (la tabla de `destructive.md` §4.2, confirmados en el registro): PU0 burn 800, crush 0,0006,
  hit 0,0017, radio 1 m; PU1 1400/0,0011/0,003; PU2 2000/0,0018/0,0075.
- `LightningForkFlicker` 0x6B24D0 / 0x6A1000: **sin uso**. Ninguno de los 132 ficheros de hechizo lo usa.
- **Color de la cinta** (fiel, `Creators/Chain.cpp`, `Graphics/RendererChain.cpp`). Valores por defecto del creador:
  `FrameHeight` 64, `FrameWidth` 32 y `NumTexturesForWholeChain` −1 (ctor 0x6AA739..0x6AA747; con −1, `CreateChain`
  pone n−1 en 0x6AA8DF). Con ellos, fn_006C8920 pone la **U a lo ancho** `[(cuadro+FileOffset)·W, +W]/256`. El cuadro
  es `FrameOfHead` en la última repetición, `FrameOfTail` en la primera y 0 en las demás. La **V va a lo largo**:
  `H/256 · tramo local / tramos de la repetición`. El rayo usa así la tira 0 de `S_lightning` (núcleo blanco con halo
  cian) repetida 4 veces. Los vértices son `articulación ± lado·escala` (semiancho = escala, 0x67BA3B..0x67BB0D). Los
  extremos de tramos vecinos se juntan en su punto medio (0x67BD2D..0x67BE82). El desplazamiento de V
  (0x67BE91..0x67BED5, con `fmod` por `H/256`) es 0 porque chain +0x4C = 0 (inferido: no se ha buscado otro escritor).

Las cintas (`ParticleChainCreator`) y los mapas de luz (`ParticleLightMapCreator`) del rayo están en
[Cadenas y mapas de luz](particles.md#cadenas-y-mapas-de-luz-psyscreatorschainlightmapcpp-graphicsrendererchaincpp).

### Pruebas y capturas

- `test_lightning`: las clases registradas, las propiedades de `ParticleChainCreator` y `ParticleLightMapCreator`, la UV
  por tramo y, con `OPENBLACK_GAME_PATH`, los ficheros reales `SF_LightningBolt`, `SF_LightningBoltPUTwo` y
  `SF_LightningStrike` (grupos, contadores, `SplitAngle`, `ForkScale`, el tamaño del `.raw` del mapa de luz). Desde
  «rayo2»: `drawOffsetLT` (el recorte del peso y `GetOffset`), `trunkJointsFollowTheHand` (cada articulación del tronco
  con el origen del paso y peso 1 − i/(n−1)), `chainTexturesOverride` (`NumTexturesToTile`) y `twoBoltsClash` (dos
  efectos: el nuevo para en el punto de choque, el viejo sigue con una horquilla opaca de escala ×3 y su golpe de fuerza
  2 llega a los dos hechizos). Desde «rayo3»: `landCutsTheFork` (con tierra a la altura 30 —20,1 m— entre la mano a
  30 m y el punto de suelo, la horquilla se corta y no hay golpe; con tierra a 0, sí). `test_land_raycast` (7 pruebas)
  cubre `RayCast` / `RayCastCells`: la ladera, el rayo que pasa de su punto final, por encima, la caída sobre llano, el
  y = 0 cerca o lejos de la cámara, el recorrido diagonal y el recorte fuera del mapa.
- Capturas en `dev\_audit\magic\`:
  - `polish_fix_rayo3_cliff.png` (+ `.log`; `OPENBLACK_SPELL_TRACE=1 OPENBLACK_TEST_MAGIC_TURN=150
    OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,2100,2835,10,300" OPENBLACK_TEST_CAST="shot@17"`, cámara
    `2075,150,2770,2118,62,2842`, `--mod game.skip-intro=off`):
    - es el rayo de guion al pie del acantilado al este de (2100, 2835), donde el suelo sube de 46 m a 70 m en 10 m;
    - el tronco sale del origen (74,9 m) y acaba en la pared, con el mapa de luz encima;
    - el registro da las horquillas de profundidad 0 y 1 «cut by the land at (2115, 2840)», antes de sus puntos de corte
      en (2131, 60, 2843).

  - `polish_fix_rayo2_hand_t25.0.png` / `_t25.05.png` (+ `.log`; `OPENBLACK_TEST_SEED=LIGHTNING_BOLT`,
    `OPENBLACK_TEST_CAST="press@22,release@28,shot@25"`, `OPENBLACK_TEST_CAST_PATH`, `--mod game.skip-intro=off`): el
    rayo lanzado de verdad desde la mano, con el arranque en la mano.
  - `polish_fix_rayo2_clash.png` (+ `.log`; además `OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,1800,2628,10,300"`, mano en
    (1784, 2598), cámara `1745,85,2560,1805,35,2640`): dos rayos enlazados; el de la mano llega al punto de choque, donde
    está el brillo `CommonGlowGroup`, y desde ahí sale la horquilla gruesa hacia el objetivo. El registro da `link 2` /
    `linked by 1` y los dos hechizos reciben el mismo evento con burn 1600 (fuerza 2).
  - `m5_bolt.png` (`OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,1818.6,2628.4,10,300"`): dos horquillas con su cinta
    `S_lightning` desde el origen (a 30 m, que es de donde lanza `SPELL_AT_POS`) hasta las puntas, con los brillos del
    `CommonGlowGroup` y un árbol alcanzado ardiendo. El registro da `Lightning: 3 targets (3 objects), 2 of 8 forks
    struck ... heading 0.00 rad, fork scale 2.00`, `PSys chains: 2 ribbons ... 10 joints` y los eventos tipo 3 con burn
    800 / crush 0,0006 / hit 0,0017 y radio 1. (Captura anterior al arreglo «polish rayo»: entonces el enfriamiento
    se usaba como filtro y muchos fotogramas no tenían ninguna horquilla; ver las capturas `polish_fix_rayo_*`. Los
    brillos de las puntas eran el `CommonGlowGroup` puesto por el port en cada punta, que el original solo pone en el
    choque de dos rayos: ya no salen.)
  - `m5_fireball.png` (`OPENBLACK_TEST_SPELL="1,1826.8,2641.4,10,300"`, MAGIC_TYPE 1): las bolas caen sobre el pueblo;
    el registro muestra el fuego de la propia `MagicFireBall` (Tc 2000, capacidad 18,8) y luego los fuegos nuevos del
    granero (objeto 52, Tc 180, capacidad 4000), de la casa 29 y de los aldeanos 30, 44 y 49, que huyen en estado 219.

## Escudos (M6-shield, `Magic/Spells/SpellShield`, `Magic/Objects/MapShield`, `PSys/Rules/Shield`)

Informe: `dev\tmp_dis\miracles\protect_creature.md` §1; lo nuevo, verificado en `runblack.exe` al portarlo, está en
`dev\tmp_dis\miracles\impl\m6s\` (spark, spin, vapour, EP, tracer). MAGIC_TYPE SHIELD (19) y PHYSICAL_SHIELD (20) usan
la misma clase `SpellShield` (SpellWithObjects, 0x10C bytes), que hace un objeto `MapShield`: `MagicShield` (invisible;
la cúpula es el PSys del hechizo, SF_DefenseSphere) o `PhysicalShield` (la malla sólida MSH_S_SOLID_SHIELD 554).

### El hechizo (`SpellShield.cpp`)

- `InitWithPos` 0x72B5F0, en orden: el radio (castData +0) se recorta **primero por arriba y luego por abajo**
  (`maxRadius` 1000 si no es menor, luego `minRadius` 5 si no es menor) y se reescribe en castData; `Spell::InitWithPos`
  (la magnitud queda en el radio); la reacción REACT_TO_MAGIC_SHIELD (13) del jugador del hechizo con radio r + 30
  (reaction +0x3C, `reactions::SetRadius`); la ciudad más cercana a menos de 250 m (`MapCoords::GetNearestTown`
  0x6020E0, todas las ciudades de todos los jugadores y del neutral); **un anillo anti de radio = la magnitud por cada
  otro jugador activo** (`GGame::GetNextActivePlayer` 0x5508D0: los siete huecos con +0x8E0 ≠ 0; aquí los jugadores
  que la tierra creó); y `MapShield::Create` 0x72BE20 en la lista de objetos. Si `Spell::InitWithPos` falla, el
  original hace todo lo demás igualmente (y luego lo borra).
- `Process` 0x72B750: suelta la reacción de golpe (+0xF4) cuando ya no está, y `SpellWithObjects::Process`.
- Coste 0x72B7F0: `costPerGameTurn × (magnitud / radiusForNormalCost)²` (20 y 22 × (r/30)²; con r = 40, 35,6 y 39,1
  por turno); `divideCostsByTribalPower` = 1 lo divide en `PayFor`. Temporizadores: jugador -1 (dura mientras se paga),
  criatura y ordenador 20 s, un uso 40 s.
- `CloseDown` 0x72B840 = `SpellWithObjects::CloseDown` 0x721300: `CoreCloseDown` y, como
  `GetSetObjectsDyingOnCloseDown` (0x55CF50) da 1, `SetDying` (vt 0x6A4) en cada objeto. `ToBeDeleted` 0x72B500: fuera
  de la lista 0xDA07F0, se van sus reacciones, +0xF8 = 0, `SpellWithObjects::ToBeDeleted` 0x720FD0 (CloseDown y la
  lista vacía) y el `ToBeDeleted` de cada anillo.
- `UpdateStruckReaction` 0x72B780 (vt 0x51C): la reacción 35 la primera vez; después solo refresca su turno
  (reaction +0x2C, `reactions::Stamp`). `SetUpDestroyedReaction` 0x72B7C0 (vt 0x520): quita las 13 que empezó,
  +0xF8 = 0 y crea la 36. En la base `Spell` las dos están vacías (0x55CE10 / 0x55CE20). `SpellOps` tiene ahora
  `updateStruckReaction` / `setUpDestroyedReaction` y `SpellHitSpell` (fn_00720B70) las llama en el hechizo golpeado.
- `IsUnder` 0x72BD20: `dist(p, castPos) < GetRadius() − margen` (x, z). `fn_0072BA00` (para `SPELL_AT_POINT`): el primer
  escudo disponible cuya magnitud cubre el punto; su máscara se prueba como `(bit | máscara) != 0`, así que no filtra.
- Aviso para las demás clases: en un `Spells/*.cpp`, una función del espacio anónimo llamada `CloseDown` y registrada
  como `ops.closeDown = CloseDown` dentro de `openblack::magic::Register...()` **resuelve a `magic::CloseDown`** (el
  despacho), que se llama a sí mismo sin fin: el juego se cuelga al cerrarse el hechizo. Aquí se llama `ShieldCloseDown`.

### El objeto (`MapShield.cpp`)

- Lista `g_game +0x205CA4`, el más nuevo primero; `MapShield` ctor 0x72C070 (`FixedObject(pos, info, 0, 1)`).
- **MagicShield** (fn_0072C250): `SetScale(0.017 × r)`; `Draw`, `DrawShield` y `ProcessShield` vacíos; `SetDying`
  0x72C320 lo borra al momento (3); `IsEffectReceiver` 0; no interactúa con las físicas.
- **PhysicalShield** (fn_0072C9F0 tras los ceros y unos de fn_0072CB70):
  - `creationTurn` = el turno; **`SetScale(1.0)` y la altura `shieldHeight + raiseWithScale × 1.0` con el 1.0 que
    aún tiene `finalScale`**; `startSpin = clamp(Spell +0x98, ±3)` — **R14: Spell +0x98 es el `curl` del
    PSysProcessInfo del hechizo** (+0x64 + 0x34; el curl del guion o de la interfaz); `endSpin = ±0.15` con el signo del
    inicial (0 cuenta como +); `startScale = 0.017 r × 0.01`; `finalScale = 0.017 r` y `SetScale(finalScale)`.
  - fn_0072CD40: la matriz y escala actuales y previas, **dos `ProcessShield` para cebarlo** y el efecto
    SF_PhysicalShieldFX (PARTICLE_TYPE 0x43) en `(x, suelo + altura, z)` con magnitud 1, luego `SetPlayer`,
    `AddTarget(el escudo)` y `SetMagnitude(radio)`. El escudo lo avanza él mismo cada turno con un PSysProcessInfo de
    ceros, fuerza 1 y activo (vt 0x100 en `ProcessShield`).
  - `ProcessShield` 0x72D190 (desde `Spell::ProcessSpells` por fn_0072BF80, antes de los mantenimientos):
    - `t` = (turno − creación) × 0,1 s. Antes de 0,5 s, **A = 1 − t/0,5: el escudo nace a tamaño completo y se encoge
      mientras no se dibuja**, y el giro no avanza. Después, `u = t − 0,5`: A = x + x² − x³ con x = u/1,5 (1 pasado
      1,5 s) y el giro `startSpin + (endSpin − startSpin) × B`, B = y + y² − y³ con y = u/6.
    - Muriendo: `dieTime += dt`; pasado 1,5 × 1,5 = 2,25 s se borra; si no, A × (1 − clamp(dieTime/1,5)).
    - escala = start + (final − start) × A; `bob = fmod(bob + 1,3 dt, 2π)`.
    - Altura (+0x1C) = `shieldHeight + raiseWithScale × finalScale + (sin(bob) + 1) × bobMagnitude × escala × 0,5`:
      **el bamboleo va con la escala nueva** (el informe decía A); con los datos 0, -2 y 3 el escudo está entre -1,36 y
      +0,68 m respecto del suelo a tamaño completo.
    - `SetScale(escala)` solo si cambió respecto del turno anterior y **difiere más de 0,3 (double 0x900C70) de
      `GetScale`**: la escala de las colisiones va a saltos detrás de la dibujada (0,68 → 0,276 al encogerse, luego
      0,597 al crecer, para r = 40).
    - La matriz: identidad × escala girada por el ángulo en Y (filas 0 y 2), en `(x, suelo + altura, z)`.
  - `DrawShield` 0x72CED0 (cada fotograma, por fn_0072BF50): escala y matriz interpoladas entre el turno anterior y
    este por la fracción del turno; alfa (+0x70) = `max(40, min(1, fuerza) × 255)`; **solo se dibuja pasados 0,5 s**, con
    `SetGlobalAlpha(1)` y el tinte blanco (se ve el alfa de su propia textura: en openblack el componente `Alpha` = 1, o
    0 antes de 0,5 s). El alfa del PSys (+0x14 → +0x6C, `Effect::SetGlobalAlpha`) es el del escudo y, muriendo, × el
    recorte de dieTime/1,5 **que el código hace al revés**: 1 hasta 1,5 s y 0 después.
  - **Material de la malla (`PhysicalShield::CallVirtualFunctionsForCreation` 0x72CCB0, portado en milagros2):**
    `fn_0057E220(obj3D vt+0xF8, 5, 0xD)` (0x72CCCD) y `(…, 4, 0xD)` (0x72CCE5); vt+0xF8 de la vtable 0x9A32A0 es
    fn_007F9E70 = `[this+0x7C]`, el LH3DMesh. fn_0057E220 recorre **todas** las submallas (+0xC / +0x10) y sus
    primitivas (+4 / +8) y cambia solo el dword +0 (tipo) si vale `from` (0x57E252..0x57E256): ni el byte +5 (caras,
    WRAP) ni ALPHAREF. En MSH_S_SOLID_SHIELD (554) la submalla 1 (capa interior, AlphaTextured 4) pasa a
    AlphaTexturedAlphaAdditiveNz 13 = modo fn_0082ECD0 (SRCALPHA / ONE, sin prueba de alfa, **sin escribir Z**); ninguna
    primitiva es de tipo 5. La 0 (exterior, TexturedChroma 9, ALPHAREF 200) no cambia: con la tabla 0xC387C8 va al modo
    15 (fn_0082E470, SA/ISA, alfa ≥ 200, escribe Z). Cambia la malla compartida, para todos los escudos físicos, como
    el original. En openblack: `L3DMesh::ReplaceMaterialType` / `L3DSubMesh::ReplaceMaterialType` (tipo y campos de
    `k_MaterialTypeLut[to]`; `twoSided`, `wrap`, `uvOffset` y `alphaCutoutThreshold` se quedan) desde
    `map_shield::Create`. Antes la capa interior era SA/ISA y escribía Z también con alfa 0: agujeros naranja turbio,
    red doble y trozos de la pared del fondo que faltaban con cortes rectos de triángulo. Ahora los agujeros suman su
    brillo rosado y la pared del fondo se ve igual en todos (`polish_fix_escudo_phys_a/_b.png` frente a
    `polish_escudo_phys_a/_b.png`). El orden de dibujo de las submallas dentro del objeto (primero la 0) sigue
    **(inferido)**; con la interior aditiva y sin Z apenas importa.
  - `SetDying` 0x72D170: muriendo y sin hechizo; se desvanece solo (encoge en 1,5 s y se borra a los 2,25 s).
  - Físicas: `InteractsWithPhysicsObjects` 1, `GetAlwaysRemainsInPhysicsInternalSystem` 1 (queda en el sistema aunque
    nada se mueva cerca: `PhysicsObjects` lo añade en cada `BeginTurn`), constantes de la fila 10, peso
    `GMapShieldInfo.weight` = 50000 × escala³ (`Object::GetWeight`). La malla de colisión es la submalla física de 554
    (28 vértices), a la escala del objeto.
  - `ReactToPhysicsImpact` 0x72D610: si quien le dio (`GetGameObjectWhoHitMe`, el que golpea) está disponible,
    `PhysicallyDestroysAbodes` y el hechizo tiene fuerza: `SpellEvent 5` en su MapCoords (x, z, su altura sobre el
    suelo) sin objetivo, `PayFor(|v| × masa × chantCostPerImpactMomentum (25) × 0,0001, forzado)` con la velocidad y
    masa del que golpea, `Town::UpdateAggressor` (no portado) y la reacción de golpe o de destrucción.
- `IsPointDefinietlyWithinShieldVolume` (vt 0x870): MagicShield 0x72B850 es una esfera 3D de radio la magnitud del
  hechizo; PhysicalShield 0x72B8E0 un cono de su `Get2DRadius` R: `d² < R²` (x, z) y **la altura del punto sobre el
  suelo (MapCoords +8, R14: relativa)** menor que `H (1 − d/R)`, con H = `GetHeight` 0x638120 (malla +0x28 × escala ×
  2). `fn_0072B990` (`Reaction::ApplyReactionToLivingObjectsAtSquare`): un vivo a menos del 2D radius de un escudo
  ignora una reacción cuyo origen no está dentro de él → `map_shield::IsReactionBlockedByShield(vivo, origen)`, para
  los repartos de reacciones de los aldeanos (el de fuego tiene su `TODO(M6)` en `VillagerFire.cpp`).

### Las reacciones de los aldeanos (`ECS/Systems/Implementations/VillagerShield`)

Portado en la tanda 2 de «milagros2» (leído entero en el exe). La reacción 13 la crea el hechizo al lanzarse y se
reparte **una sola vez** (`SpreadReaction`), así que solo tienen ocasión los aldeanos que están cerca en ese momento.

- **Arreglo del reparto**: el iniciador de las reacciones 13 / 35 / 36 es el **propio hechizo**, y en openblack una
  entidad `Spell` no tiene `Transform`, así que `reactions::SpreadReaction` cortaba antes de empezar y **ninguna
  reacción de escudo llegaba a nadie**. `Reactions.cpp` lee ahora la posición del iniciador con `MapPosOf`: el
  `Transform` si lo tiene y, si no, `components::Spell::position` (+0x14, que es lo que devuelve `Reaction::GetPos`
  0x6E45C0).
- `Villager::ReactToMagicShieldPriority` 0x765BB0: 0 si el iniciador no es un `SpellShield` (0x765BCD) o no está
  disponible (vt 0x2C, 0x765BDD); **sin pueblo devuelve ya la prioridad** de ReactionInfo[13] (+0x10, 0xD4FBD4,
  0x765C08 → 0x765C48); con pueblo, 0 si `TownDesire::GetDesireSignificanceToVillager(pueblo +0x34,
  TOWN_DESIRE_FOR_PROTECTION 3)` 0x746660 es 0 (`test ah, 0x40`) o si `turno − pueblo +0xEB0` (el turno del último
  agresor) pasa de `GVillagerInfo` +0x364 (`numGameTurnsAfterAggressionInterestedInShield`). La distancia que calcula en
  0x765BEF **se descarta** (`fstp st(0)`): no influye.
- `Villager::SetupReactToMagicShield` 0x765C60: `AddReaction(reacción, 168)` (vt +0x990 → `Living::AddReaction`
  0x5F0F30: guarda la reacción y apila el estado final) y +0xBC = el hechizo; si **no** está ya bajo el escudo
  (`SpellShield::IsUnder(su posición, 0,2 R)` 0x72BD20, 0x765CC4) camina con `SetupMoveToWithHug` a
  `centro + GetPosFromAngle(rumbo centro→él ± π/8, 0,8 R (1 − aleatorio³))` con estado final 168 —es decir se mete
  debajo, sesgado hacia el borde—; y en los dos casos deja el punto de mira (+0x10C, `JustWholeMapXZ`) a **1 m de sí
  mismo en ese mismo rumbo**, o sea mirando hacia fuera de la cúpula (0x765D8B..0x765DEF).
- `Villager::AmazedByMagicShieldReaction` 0x765E00 (fila 168 de la tabla de estados; su salida es el thunk 0x5B0100 =
  `Villager::ExitReaction` 0x7527A0): con pueblo, el hechizo todavía disponible y el deseo de protección > 0
  (`test ah, 0x41`), `LookAtPos(+0x10C, 0)` un paso por turno y, cuando ya mira, 1 vez de cada 4 (`GameRand(4)`) un
  punto nuevo; el clip del estado (`AnimFn::AmazedByShield` = `Villager::AmazedByShieldAnimation` 0x4240C0) se mantiene
  `(GameFloatRand(10) + 20) × 1000 / duración` vueltas (`IsReadyForNewAnimation` 0x5EC960) y luego se vuelve a sortear,
  salvo si es el 286 INTO_POINTING, que tras una vuelta pasa al 395 TALKING_AND_POINTING (0x765F55..0x765F8D). Si falta
  el pueblo, el escudo o el deseo: `SetupWaitForCounter(ftol(GameRand(60) + 20), 163)` 0x76B060 (0x765FC0), con lo que
  también queda portada la fila 57 WAIT_FOR_COUNTER (`Living::WaitForCounter` 0x5EC310).
- `Town::UpdateAggressor` 0x73C9B0: portado **solo el registro** (+0xEAC el jugador agresor y +0xEB0 el turno,
  0x73CA82 / 0x73CA98; `components::Town::aggressor` / `aggressorTurn`), que escribe el impacto del escudo físico
  (0x72D74F..0x72D788: `EffectValues(2, 0, el que golpea, 1.0, PhysicsObject::GetPlayer 0x647460)` y valor 0). Sin
  portar: los huecos de agresión por jugador (`fn_0073E0F0` sobre pueblo + n × 0x80 + 0x9F4, con el valor + GTownInfo
  +0xAC cuando el hueco está a 0 y × el peso +0xEB4 / +0xEB8, que luego decae × 0,9), el `TownAttackSFX` 0x71B7C0 y el
  mimetismo de la criatura (0x73CAAA..0x73CB29).
- `MapShield::CreatureMustAvoid` 0x72C170 (vt 0x614): portada la condición (no controlado por script, +0x24 & 0x400, y
  jugador distinto del del escudo; si el escudo ya no tiene hechizo, +0x60 = 0 en 0x72C155, su jugador es el de
  `GameThing::GetPlayer` 0x570130 = el del interfaz, g_game +0x205A5B, el PLAYER_ONE de openblack, no NULL); como openblack no tiene criatura, su jugador entra por parámetro y **nadie la llama
  todavía** (en el original la usa el camino de la criatura).
- `GetImpressiveValue` **sigue sin portar** porque no hay nada que lo pida (el sistema de impresionar / creencia no
  está): `SpellShield::GetImpressiveValue` 0x72BA80 da 0 para un aldeano cuyo pueblo tiene como agresor (+0xEAC) al
  jugador del escudo si la reacción es la 13 o la 35, y si no `Spell::GetImpressiveValue` 0x721630, × 4 cuando es la
  36 (0x72BAED); `PhysicalShield::GetImpressiveValue` 0x72D7F0 es igual con la 13 y de base
  `Object::GetImpressiveValue` 0x639860, sin el × 4.
- Lo que no se ha portado del reparto: cambiar la reacción que ya sigue un aldeano por otra que puntué más
  (0x6E4134, `reactions::MaySwitch`), igual que en el fuego y el teletransporte; y las reacciones 35 (golpe) y 36
  (destrucción), que ningún vivo atiende aún.
- Las dos entradas que openblack no tiene (el deseo de protección del pueblo y, mientras nada más haga de agresor, el
  turno del agresor) dejan la reacción **inalcanzable para un aldeano con pueblo**; para verla en el juego está el
  gancho `OPENBLACK_TEST_SHIELD_REACTION=1`, que da las dos por buenas (capturas
  `polish_fix_escudo2_react_magic.png` / `_phys.png`: 8 aldeanos pasan a 168 en el turno del lanzamiento, uno de ellos
  entra andando).

### Las partículas (`PSys/Rules/Shield.cpp`)

- **Registro `DefensiveShield`** (lista 0xD4EE48, `PSysShield.cpp`): `UR_AddDefensiveSphere` 0x6A2A60 crea la primera
  vez una `DefensiveSphere` (fn_006D0B20) en `GetCurrentParentPos` (el origen del efecto, en SF_DefenseSphere) con el
  radio del proveedor (magnitud × 1,11062); después solo sigue el radio. Se borra con su colección (el `~Effect`).
- **API para las bolas de fuego (lane M5)**, en `PSys/Rules/Shield.h`, espacio `psys::shields`:
  `DoAnyShieldDeflections(effect, atom, oldGlobal)` es `UpdateRuleGravityWithFloor::DoAnyShieldDeflections` 0x6A1FA0.
  El original lo llama **al final de la actualización de cada átomo** (0x6A1F48) solo con `CheckShieldDeflections`
  (+0x72), con `oldGlobal` = la posición global del átomo **al empezar su actualización** (0x6A1903). Margen =
  1,25 × baseScale × ruleScale; si el átomo cruzó hacia dentro de una esfera: la intersección (`FindIntersect`
  0x57CCD0, el radio + margen), una chispa (`AddImpactTarget`, fn_006D0AF0), y un `SpellEvent 4 {impacto, el último
  movimiento global, 1, target = el hechizo del escudo}` al propio efecto del átomo; con 0 el átomo queda en el
  impacto (`GlobalToLocal`) y su velocidad rebotada (`v −= 2(v·n)n`, 0x57CFD0) y devuelve true; con 1 pasa. En el
  hechizo, `ApplyDefaultSpellEffect` → `SpellHitSpell`: el escudo paga fuerza × `costPerShieldCollide` (FIRE 600/800/
  1000) y la bola un evento; si el escudo sigue con fuerza, reacción de golpe y 0 (rebota). También hay
  `FindShieldContainingPoint`, `FindShieldCrossedInto`, `IsPointInShield` (estricto `< (r + m)²`),
  `HasCrossedIntoShield` y `SpellOf`. `UR_CloudMoverNew` (0x6D46D9) también llama a la función.
- `ApplyDefaultSpellEffect` con `event +0x20` (`checkShields`, 0x720D19): con una esfera que contenga el punto (margen =
  el radio del efecto) se manda a sí mismo un evento 4 con él mismo de objetivo; con 0 se corta el evento, si no, chispa
  en el punto. Portado tal cual (la intención no está clara).
- `SpellTargets` guarda también **puntos** (12 bytes, +0x14): `Effect::AddTargetPoint` / `TakeTargetPoint`
  (fn_00670F00 toma primero un punto y si no un objeto, su posición).
- `UpdateRuleShieldSpark` 0x6A2BF0 (una AtomCreateRule): por cada impacto, mientras haya menos de
  `MaxNumAtomsForCollection`, un átomo en el impacto con su `SoundSpark`, tres ángulos al azar y un número de ondas
  2..5; los de más de `SparkLife` se borran. `ModifySubCollection` 0x6A2F90 dibuja con sus N átomos **un arco desde el
  centro de la esfera hasta el impacto**: el átomo i (de N a 1) en u = i/N a u R a lo largo de la dirección del impacto,
  desviado R × WiggleAmpl × sin(k u π) sin(4t + a3) y sin((k+1) u π) sin(2t + a3); escala 0,04 R (1,5 + 0,4 (1 +
  sin(a3 − 6t))), alfa (1 − t/SparkLife) × 510 entre 0 y 255.
- `UR_InitialSpin` 0x69E490: cuando el efecto tiene jugador, giro = curl (manager +0x58 = PSysProcessInfo +0x34) ×
  ScaleAngularVelocity entre ±MaxAngularVelocity, que se apaga en TimeToFade; gira el átomo en su Y.
- `UR_VapourEndEffect` 0x6A39E0: el parche de superficie se pone en su padre (el trazador de la esfera), orientado la
  primera vez con su Y hacia fuera y después girado lo que se movió el padre (eje último × actual, ángulo acos);
  escala = ScaleFactor, **alfa = edad × 30** (plena a los 8,5 s).
- **La cúpula girando (milagros2, «efecto muelle» y «se cruzan los alfa»)**: el original interpola cada fotograma
  **toda la matriz 3×4** del átomo (fn_00679C30, desde fn_00679920 0x6799F9), giro incluido; openblack dibujaba la
  posición interpolada con el giro del turno nuevo y cada 0,1 s los casquetes se torcían de golpe (el muelle). Ya está
  interpolado (`Effect::CollectCollection`, PSys.cpp). Comprobado con `OPENBLACK_TEST_SHIELD_FRAMES` (SHIELD r 40, curl
  5, 2,5 s): de una fracción 0,94 a la 0,07 del turno siguiente solo se deslizan los anillos de alfa
  (`polish_fix_escudo_spirit_diff_boundary.png`); dentro del turno (0,07 → 0,94) la cúpula se mueve entera y suave
  (`_diff_inturn.png`). Lo que queda al girar es del original: 15 casquetes aditivos (modo 13, sin escribir Z, prueba
  de Z solo contra el mundo, dos caras, orden por parche que en aditivo no cambia nada) que se solapan 2-3 veces, con
  la curvatura que no coincide con la esfera y anillos de alfa DXT3 de 4 bits; al moverse cambian las sumas donde se
  solapan. La luz por vértice (fn_00858BA0) sigue **(aproximado)** y la fracción del original va de 0 a 5
  (0x6799A8..0x6799DF) frente a 0..1 en openblack (solo importa si un fotograma tarda más de un turno).
- `SetCollectionAlpha` 0x6A2720: alfa de la colección = clamp(proveedor, 0, 255) (en la cúpula, la fuerza × 255, 40..255).
- `UR_AtomsAtEPTarget` 0x69A960: toma un objetivo objeto la primera vez y hace un átomo por punto extra de su malla (5
  si no tiene; MSH_S_SOLID_SHIELD no tiene, así que los cinco van a la posición del objeto); si el objeto se va, borra
  sus átomos.
- `CheckShieldDeflections` 0x6A2570 (regla de átomo, ningún archivo la usa) portada salvo `MoveToBaseGroup`.
- **`UR_SphereSurfaceTracer` 0x6A32B0 corregido** (PSys.cpp): la propiedad del radio es `ScaleSphereRadius` (no
  `SphereRadiusFP`, que no existe: la cúpula salía de 1 m), los ángulos con `fmod 2π`, el alfa `Alpha × ScaleAlpha`
  y, fuera de jerarquía, + la posición del padre.

La corrección de las jerarquías del PSys que necesita la cúpula está en
[Corrección en el núcleo del PSys: las jerarquías](particles.md#corrección-en-el-núcleo-del-psys-las-jerarquías).

### Ganchos y capturas

- `OPENBLACK_TEST_SPELL` admite `curl` (el séptimo valor); `OPENBLACK_TEST_SHIELD_SHOT` captura por turnos
  ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)).
- `test_shield`: recorte, coste, curvas, ayudantes de esfera, registro y desvío con un efecto real, marco de jerarquía y,
  con `OPENBLACK_GAME_PATH`, las filas reales (5 / 1000 / 30, 25, 0 / -2 / 3, 20 y 22, peso 50000) y
  `physicalShieldMaterialTypes` (carga la 554 de AllMeshes.g3d con bgfx Noop: tras (5, 13) y (4, 13) la submalla 1 es
  tipo 13 aditiva sin Z, dos caras y WRAP; la 0 sigue tipo 9 con umbral 200/255 y Z).
- `OPENBLACK_TEST_SHIELD_FRAMES="<turnos>,<n>,<prefijo>[@<lento>]"`: n capturas en fotogramas seguidos desde esos
  turnos tras el primer escudo (`<prefijo>_<i>.png`; el log da el turno y su fracción). Cada captura para el fotograma
  ~0,45 s a 1600×900: con `-W 640 -H 360` salen dos por turno (fracción ~0,07 y ~0,94). `@<lento>` multiplica la
  duración del turno; el PSys interpola con la fracción del juego (`game_clock::TurnFraction`, g_game +0x205D64), así
  que también va lento. La fracción propia de MapShield sigue con el reloj de pared (pendiente:
  [engine-math.md](engine-math.md#reloj-del-juego-1)).
- Capturas en `dev\_audit\magic\`: `m6s_shield_dome.png` (SHIELD r 40 en el almacén de Land1, desde arriba: la cúpula
  translúcida de parches MSH_S_SPELLBALLSURFACE02), `m6s_shield_dome_side.png`, `m6s_dome_t10/t30/t100.png` (1, 3 y
  10 s); `m6s_phys_t03/t07/t10/t13/t25.png` (PHYSICAL_SHIELD r 40: oculto a 0,3 s, 0,7, 1,0 (A 0,41), 1,3 (0,67) y
  2,5 s, completo) y `m6s_physical_full.png`; `m6s_physical_rock_slow.log` (una roca a 12 m/s rebota: dos golpes,
  momentos 8191 y 16115, paga 20,5 y 40,3 cánticos; el evento 5 cae en la roca), `m6s_physical_rock.log` (a 33 m/s
  la detecta, 166,6 cánticos, pero **atraviesa** la cáscara fina: el contacto de `PhysOb` no la para; sin verificar si
  el original la para), `m6s_physical_close2.log` (duración 3 s: se cierra en el turno 31, encoge en 1,5 s y se borra
  en el 54 con el hechizo) y `m6s_magic_close2.log` (el PSys se apaga 2,2 s y el hechizo se borra en el 54).

### Sin portar / UNVERIFIED

- (Portado, arriba) las reacciones de los aldeanos al escudo y el predicado `CreatureMustAvoid`. Siguen fuera: la ruta
  de la criatura que lo usa (0x54AF60), las reacciones 35 (golpe) y 36 (destrucción) —nadie las atiende—,
  `GetImpressiveValue` 0x72BA80 / 0x72D7F0 (sin sistema de impresionar que lo pida) y todo `Town::UpdateAggressor`
  menos su registro. Pendiente de la sesión «towns»: el deseo de protección del pueblo
  (`TownDesire::GetDesireSignificanceToVillager` 0x746660 necesita las tres tablas de deseos, TownDesire +0x90 / +0xD4 /
  +0x118, que no están) y que alguna otra agresión (daño, fuego, edificios) escriba el agresor; hasta entonces solo
  reaccionan los aldeanos **sin pueblo** (o con el gancho `OPENBLACK_TEST_SHIELD_REACTION=1`).
- Los escudos no están en la rejilla de objetos del mapa: `ApplyEffectToMapPos` no los alcanza (el físico admitiría
  efectos sin quemar, `IsEffectReceiver` 0x72CC80; el mágico ninguno).
- `CallVirtualFunctionsForCreation` 0x72CCB0: la base `SingleMapFixed::CallVirtualFunctionsForCreation` 0x52E880 y
  los enlaces de caminos (vt 0x78 / 0x80 / 0x88 / 0x98 / 0x1E8 sobre [esi+0x40], fn_00644DF0 si (obj+0xA & 1) == 0).
  El cambio de material `fn_0057E220` sí está portado (arriba).
- El dibujo de los parches de la cúpula es el de `Creators/Mesh.cpp`: **resuelto en M6b** (color del jugador con mezcla
  0,5, aditivo por el `MeshChangeMaterialProps` del ctor, y `DrawCutByPlane` no recorta una malla estática); ver
  [Las mallas de partículas](particles.md#las-mallas-de-partículas-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-y-la-cúpula-del-escudo). `OrientToSurface` del trazador (lo usan
  SF_DefenseSphereInHand/OnHolder) y `MoveToBaseGroup` (ya en el núcleo, de la lane de la tormenta) siguen sin usarse
  aquí.
- Los puntos extra de las mallas no se cargan (`UR_AtomsAtEPTarget` usa la posición del objeto, exacto para 554).
- `Get2DRadius` / `GetHeight` salen de la caja de la malla. **Igual que el original** (ya comprobado del todo en la
  auditoría de suposiciones): `LH3DMesh::ComputeBoundingBox` 0x8081B0 recorre todas las submallas (+0xC / +0x10, la
  física de 554 incluida) al cargar (la cabecera la trae a cero) con `LH3DSubMesh::ComputeBoundingBox` 0x87FB20 →
  `LH3DPrimitive::ComputeBoundingBox` 0x807F30 (el primer punto sale el máximo y el segundo el mínimo, 0x807F97..), y
  +0x24 / +0x28 / +0x2C acaban siendo **media extensión**: (max − min) y luego × 0,5 (0x80833A / 0x808349 / 0x808355;
  +0x30 es su módulo). Por eso `Object::Get2DRadius` 0x638180 = escala × max(media X, media Z) y `GetHeight` 0x638120 =
  escala × media Y × 2 (`fadd st, st` 0x63813D) = la altura entera, que es justo lo que hace openblack con
  `GetBoundingBox().Size()` (× 0,5 en el radio, tal cual en la altura).
- (Corregido) la bola de fuego ya llama a `DoAnyShieldDeflections` (`PSys/Rules/Fireball.cpp`).
- Las dos claves de orden iguales (el escudo físico y su SF_PhysicalShieldFX en el mismo punto) con `std::sort` no
  estable: qué hace el Z-sorter con los empates sigue sin leerse (0x82F280 resultó ser el bucle de callbacks de
  `FinishFrame`, no el orden; hay que leer 0x82F460). Dueño: render común.
- La roca rápida (33 m/s) que atraviesa la cáscara del escudo físico **no está demostrado que sea una desviación**: el
  original usa el mismo modelo que openblack porta —20 subpasos de 5 ms por turno (`GameTurnUpdate` 0x646046), contacto
  por vértice contra las caras del otro cuerpo (`CollideVertices` fn_007FDA60 con el segmento centro→vértice,
  fn_007FBF80) y muelles de `PhysicsConstants.txt` fila 10 (`GetPhysicsConstantsType` 0x72D7E0)—, y la rama de malla de
  fn_007FDD60 (el otro cuerpo con +0x168, fn_008683C0) no la usa ningún cuerpo de openblack. El golpe **sí se detecta**
  (paga cánticos), pero el muelle no frena 33 m/s en los dos subpasos que dura el contacto. Falta una captura del
  original para comparar; no se ha tocado nada de Physics.
- En el camino de los objetos con `Alpha` (`Renderer.cpp`), una primitiva Standard con `depthWrite` = false (tipos 6,
  7, 8) sigue escribiendo Z y no se cambia de tabla de modos (0xC387C8) por primitiva; al 554 ya no le afecta.
- `Primitive::modulateAlpha` (α = textura × difuso de los modos 3, 5, 6, 8, 10..13, 15, 16) **no lo lee nadie**:
  `fs_object.sc` multiplica siempre el alfa por el del objeto. Tras el cambio de material la capa interior del 554 es
  del tipo 13, que sí es × difuso, así que para el escudo coincide con el original (auditoría de suposiciones).
  La prueba de alfa de la capa exterior sí es fiel: el original escala ALPHAREF por el alfa del objeto (modo 15,
  0x82E579..0x82E5AE: `al = material+4`, `fild`, `fimul` por el byte de alfa, `fmul 1/255`, `fsub 5`) y el sombreador
  compara el alfa de la textura sin escalar contra el umbral − 5/255, que es lo mismo.

## Teletransporte (M6t, `src/Magic/Objects/MagicTeleport`, `src/ECS/Systems/Implementations/VillagerTeleport`)

Informe: `dev\tmp_dis\miracles\protect_creature.md` §2 y las funciones releídas en `impl\m6t`. TELEPORT es
MAGIC_TYPE 12; su clase es `SpellTeleport` (una `SpellWithObjects`, 0xF4). Cada lanzamiento deja una **piedra de
teletransporte invisible** (`MagicTeleport`, una MobileStatic → MultiMapFixed) con una piscina de partículas
(ParticleType 73 `SF_TeleportVortex`). Las piedras de un jugador forman una lista (GPlayer +0xA58 cabeza, +0xA5C
cuenta; aquí `PlayerMagic::teleportStones`). Un vivo que se mueve y entra o se deja caer en una piedra sale por la
piedra del mismo jugador que más lo acerca a donde va.

### El hechizo y la piedra

- `SpellTeleport::InitWithPos` 0x5FBEB0: `MagicTeleport::Create(pos, this)`, la piedra entra en la lista de objetos
  del hechizo (+0xEC/+0xF0) y luego `Spell::InitWithPos`. El hechizo **no tiene tipo de partícula** (el efecto es de la
  piedra), así que InitWithPos manda `SpellEvent 11` (`particleType == 0`) y no crea PSys propio.
- `GMagicTeleportInfo` vt+0x30 (CanCast en pos) 0x5FBE50: falla si hay un MultiMapFixed (edificio, campo, otra
  piedra...) a menos de `fn_005FCCA0` = **6 m** (`fn_00604C30` con el predicado AsMultiMapFixed); si no,
  `GMagicInfo::CanCast` = 1. En openblack `cast_rules::CanCastAt` lo llama `teleport::AnyMultiMapFixedNear`, con el
  rasgo común `ecs::fire::traits::IsMultiMapFixed` (bit 2 de +0x24, ctor 0x52E207; = `AsMultiMapFixed` vt 0x678). Como
  `SPELL_AT_POS` no comprueba nada (creador neutral, bandera a 0), el guion planta la piedra igual (el gancho lo
  confirma: «CanCastAt(A) now false» pero la piedra se crea).
- `MagicTeleport::Create` 0x5FC1F0: `new MagicTeleport(pos, spell)` (ctor 0x5FC130: `MobileStatic(pos, 0xD3B614,...)`),
  `CallVirtualFunctionsForCreation` 0x5FC260 (crea el PSys 73 en la pos del mundo y `SetPlayer`), y
  `SetScale(GetScale()·0.01)`. La piedra **no dibuja malla**; `Draw` 0x5FCCC0 solo manda una colisión de mano
  invisible de radio 3 mientras el hechizo tenga semilla (+0xAC; ver «La mano y las piedras»), y avanza y dibuja el PSys con el tiempo del fotograma
  (por eso el vórtice va en `SetPerFrame`).
- Coste y temporizadores (efecto 12): costToCreate 5000, initialChants 2000, costPerGameTurn 1, costPerEvent 1,
  `divideCostsByTribalPower = 1`, `costPerKilometer = 200` (GMagicTeleportInfo +0x58). Temporizador del jugador **-1**
  (las piedras persisten), criatura/CP 25 s, un uso 120 s.
- **Cuánto vive una piscina (no es un fallo):** al no tener temporizador, la cierran los cánticos. Una semilla de orbe
  de dispensador le da `initialChants` 2000 y el hechizo gasta 1 por turno, o sea unos **200 s** (traza: 2000 → 1814 en
  186 turnos), salvo que un jugador siga pagando (`CostToMaintain`). Cada salto útil le **devuelve** cánticos
  (`PayFor` con coste negativo, R13), así que una piscina que se usa dura más. Igual que en el original.
- `MagicTeleport::ToBeDeleted` 0x5FC310: quita reacciones, borra el PSys, se desenlaza de la lista del jugador y libera
  la lista de viajeros. `SpellWithObjects::CloseDown` 0x721300 pone a morir cada objeto (`SetDying` 0x4027A0 =
  `ToBeDeleted(0)`), así que cerrar el hechizo borra la piedra. En el código la función se llama `TeleportCloseDown`:
  con el nombre `CloseDown`, dentro de `magic::RegisterTeleportSpell` se resolvía a `magic::CloseDown` (el despacho)
  y el juego se colgaba al cerrarse el hechizo (revisión «review3a»; comprobado con `OPENBLACK_TEST_SPELL=TELEPORT,…,3`:
  se cierra y se borra en el turno 31).

### Quién usa una piedra

- `MagicTeleport::ShouldLivingThingReact` 0x5FC590: el vivo se mueve (IsMoving vt 0x174) y hay otra piedra T del mismo
  jugador con `1.2 · (|vivo−esta| + |T−destino|) < |vivo−destino|` (0x8C6C98 = 1.2), con `dest = GetFinalDestPos`
  (vt 0x884) y `FastDistance` 0x74CE10 (punto fijo, 6553,6 por metro: `max + min/2`).
  - `Object::IsMoving` 0x402710 (leído): la posición actual (`GameThingWithPos::Pos` +0x14 x, +0x18 z) no es la de
    `Object::coords` (+0x2C, +0x30), que es la del turno anterior; es decir, **se movió en el último turno**, en
    cualquier estado. **(aproximado)** openblack no guarda la posición del turno anterior: `villager_teleport::IsMoving`
    mira que tenga una etiqueta de movimiento que no sea ARRIVED y velocidad > 0 (antes solo valía el estado
    MOVE_TO_POS, así que un aldeano que iba al lugar de culto o a otra reacción no contaba).
  - `Villager::GetFinalDestPos` 0x756AD0 → `Living::GetFinalDestPos` 0x5EC1E0 (leído): si tiene sendero y nodo
    (+0xC8/+0xCC), el último nodo no oculto del sendero (`GFootpath::GetEndNonHiddenNode` 0x535120 con la bandera
    (+0xB4 >> 3) & 1); si no, `MobileWallHug::GetDestPos` (vt 0x860, 0x416F70) = **la meta +0x80, se mueva o no**.
    openblack usa siempre la meta del WallHug (fiel); **(pendiente)** la rama del sendero, porque ningún `Living` de
    openblack anda sobre un `components::Footpath`. Nota: un aldeano que nunca ha andado tiene meta (0, 0) (como el
    +0x80 del original tras `SetToZero`), así que un salto forzado sobre él se calcula contra (0, 0).
- `Villager::ReactToTeleportPriority` 0x766200 = `(ShouldLivingThingReact ? 0xFF : 0) & prioridad de la reacción 20`
  (la fila REACT_TO_TELEPORT de `ReactionInfo`). `SetupReactToTeleport` 0x766250 registra el destino en la piedra
  (`GetFinalDestPos` vt 0x884 en 0x766297 y `fn_005FC6A0` en 0x7662A5), pone +0xBC = la piedra y entra en
  `GO_TOWARDS_TELEPORT_REACTION` (201) o en su gemela rápida 251 (0x766380 es un `jmp` a la de 201, así que solo cambia
  la fila de la tabla de estados: animación e índice de velocidad). **La condición, leída (0x7662B2..0x7662CE):**
  `0xC9 + 0x32 · (velocidad > umbral)`, con la velocidad propia del aldeano (`MobileWallHug` +0x5A, el u16 que
  `GetSpeedInMetres` 0x60C070 convierte a metros) y el umbral `GMobileWallHugInfo` +0x10C, que es
  `speedGroup.speed2` del info.dat (el mismo campo que lee `Living::FleeFromPredatorPriority` en 0x5F15ED);
  `setle` sobre el u16, así que 251 solo si es **estrictamente** más rápido. Portado en
  `villager_teleport::SetupReactToTeleport` (un aldeano que ya corría al lugar de culto sale con 251: ver la prueba de
  abajo). `GoToTeleportReaction` 0x7662F0: al llegar (`AreWeThere`) pasa a
  `TELEPORT_REACTION` (202), si no `SetupMoveToWithHug(piedra)`. `TeleportReaction` 0x7663F0 llama
  `DoTeleport(vivo, false)` y `StopReactingAndSetState`.
- La reacción se **reparte una sola vez**, al crear la piedra (`Reaction::CreateReaction` con marca 0; `ProcessReactions`
  no la vuelve a repartir, su bandera 0xD00DD4 nunca se pone). Por eso solo reaccionan los aldeanos que ya se están
  moviendo en ese instante (comprobado: los aldeanos que deambulan por el pueblo reaccionan y saltan).
- Soltar con la mano: `fn_005FC4B0` exige que el jugador del aldeano sea el de la piedra y `teleportCount != 1`.
  `fn_005FC4F0`, en su orden exacto (leído): `SetTopState(FLYING 10)` 0x5FC4FD, `fn_005DA0C0` (la interfaz lo deja en el
  MapCoords de la piedra) 0x5FC517, `SetTopState(LANDED 11)` 0x5FC51E, `DecideWhatToDo` (vt 0x8C8) 0x5FC52C,
  `GetFinalDestPos` (vt 0x884) 0x5FC549 → `fn_005FC6A0`, y `DoTeleport(forzado)`; devuelve 1 si saltó, si no 0x17.
  `villager_teleport::LandAt` ya pasa por FLYING y LANDED (los dos estados existen en la tabla de openblack: 10
  `VillagerCarried`, 11 `VillagerLanded`). **(aproximado)** el `DecideWhatToDo` de openblack solo cambia el estado, así
  que la meta que se registra después es la que el aldeano traía.

### La mano y las piedras (fiel, lane «mano» de milagros2; era el «no funciona» del usuario)

Antes la mano no podía hacer nada con una piedra. Ahora:

- **La mano ve la piedra.** `MagicTeleport::Draw` 0x5FCCC0: mientras el hechizo (+0x9C) tenga semilla (+0xAC,
  0x5FCD03), `SendInvisibleDrawCollision(piedra, (x, GetAltitude + y, z), 3.0)` (0x5FCD18). 0x519960 proyecta el
  centro (`ProjectPoint` 0x819390; nada detrás del plano cercano 0xE839E0), mide el radio en pantalla de un punto a 3 m
  y, si el ratón cae dentro del círculo, `SendObjectDrawCollision(piedra, profundidad)`. openblack:
  `HandSystem::PickObjectAlongRay` (`HandPlacement.cpp`) prueba una esfera de 3 m en cada piedra de
  `teleport::HandCollisionStones()`: el rayo pasa a menos de 3 m del centro en el plano de su profundidad.
  **(aproximado):** la distancia con la que compite es la longitud del rayo hasta ese plano (como las mallas de
  openblack), no la w proyectada. Una piedra de guion (`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_TELEPORT`) no tiene
  semilla y la mano no la ve, como el original.
- **Pulsar con un aldeano en la mano sobre la piedra = aplicarlo** (`HandApplyToObject.cpp`,
  `HandSystem::HeldActionPressedOnObject`). `ActionPressedHolding` 0x5D1560: objetivo = `GetCreatureToGiveTo` (sin
  criatura) o el objeto bajo la mano; fuera de la influencia (`[this+0x48]` == 0) y `InterfaceMustBeInInfluenceForInteraction`
  (vt 0x714, 0x4028A0 = 1) va a la rama sin objetivo (0x5D15D6..0x5D15E9); luego `ValidToApplyThisToObject` (vt 0x71C,
  0x5D1607). Para un aldeano `ApplyOnlyAfterRecSystem` y `ValidForLockedApplyProcess` son 0, así que
  `SendApplyToObject` 0x5D30D0 (0x5D1684): comprueba que la mano sostiene algo (status +0x90 = el contador de la mano,
  **no** la influencia como decía la verificación), repite la validez (0x5D3126), paquete 0x11 (0x5D32A7) y estado
  0x12. El paquete (0x5DA1A0) repite las comprobaciones y llama a vt 0x720 (0x5DA251): `Villager::ApplyThisToObject`
  0x752C40 → piedra (0x752FC2) → `ValidToApplyVillagerDirectlyToTeleport` 0x752FEA → `fn_005FC4F0` 0x752FF8, que saca
  al aldeano de la mano con fn_005DA0C0 (`RemoveFirstFromHand` fn_005CED60 y el MapCoords de la piedra) y salta.
  `HandleApplyResult` fn_005DA100: 5 → nada; 3 → consumido; si no es 1 y el objeto ya no está en la mano, se devuelve
  tal cual (el caso del aldeano, 1 o 0x17); 0x16 → `RemoveFirstFromHand`; 0x17 → fn_005DA0C0 en la posición del
  objetivo; 0x18 → fn_005DC1E0. fn_006E47C0(obj, 1) (marca +0x30 en la lista g_game +0x205BDC) sin identificar ni
  portar. **Sin portar:** la rama del tótem (`WorshipTotem`, el sacrificio de 0x752C40..0x752FB0): con un tótem la
  pulsación sigue armando la suelta; la ayuda `HelpProfile` 6/7.
- **Coger la piscina.** `MagicTeleport::ValidForPlaceInHand` 0x5FC440 = la vt 0x6FC de la semilla del hechizo (0 sin
  semilla); `InterfaceSetInMagicHand` 0x5FC470 = `GInterfaceStatus::PlaceObjectInMagicHand(semilla)` 0x5DC870 y
  devuelve 0. La semilla vuelve a la mano (`StoreChantsAndAgeFromSpell`, `ClearSpellLink`: el hechizo cierra y
  `SpellWithObjects::CloseDown` 0x721300 borra la piedra). openblack: `HandSystem::PickUpSeedOrStone`, con el agarre de
  225 ms y la influencia de `GenericPickup` 0x5D2800. Como todo objeto que no es árbol ni bosque, suena
  `SoundTag::Create(MapCoords +0x14, 10 G_PickUpObject, modo 3, 3D, InGame)` 0x71EB60 (0x5D2881..0x5D28B7; añadido en
  la auditoría de la lane). **(aproximado)** para la semilla: openblack solo guarda el punto dibujado, no el MapCoords
  propio que 0x729020 no mueve.
- **Lo que hace falta para usarla:** dos piedras del jugador (`teleportCount != 1`) y aldeanos de un pueblo suyo
  (`fn_005FC4B0`), dentro de su influencia. Con una sola piedra no se puede aplicar.
- Gancho `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1,0,hand"`: solo planta B (guion); A se lanza con la mano
  (`OPENBLACK_TEST_SEED=TELEPORT`, `OPENBLACK_TEST_MAGIC_TURN=200` y una pulsación de `OPENBLACK_TEST_CAST` con el ratón
  en x0,z0); diez turnos después pone en la mano al aldeano del pueblo de PLAYER_ONE más cercano a A, y las
  pulsaciones siguientes de `OPENBLACK_TEST_CAST` (reales, por `HandSystem::Update`) lo aplican y luego cogen la
  piscina. Cada 10 turnos escribe las piedras, su semilla y lo que hay en la mano.
- Prueba (`polish_fix_mano_teleport.log`, `polish_fix_mano_teleport_hover.png`; `OPENBLACK_TEST_TELEPORT="1812.6,2654.7,1760,2625,0,hand"`,
  `OPENBLACK_TEST_TELEPORT_TURN=150`, `OPENBLACK_CAMERA_LOCK=1800,75,2600,1812,30,2652`, `OPENBLACK_MOUSE_AT=0.5,0.5`,
  `OPENBLACK_TEST_CAST="press@24,release@24.3,shot@29.5,press@30,release@30.3,press@36,release@36.6"`): la piedra A
  3148419 sale de la semilla 2697; el aldeano 48 en la mano (válido); la pulsación: `villager 48 from stone 3148419 …
  to stone 2099844 … (forced)`, `Hand: applied 48 to object 3148419: result 0x1`, y el aldeano aparece en B; la
  pulsación larga: `stone 3148419 deleted`, `Hand: picked up … seed 2697 (InterfaceSetInMagicHand 1)`, la semilla en
  la mano y una sola piedra.

### `DoTeleport` 0x5FC790 y el coste (R13)

Entre las otras piedras del jugador elige la de mayor ahorro `s = dist(dest, vivo) − dist(dest, T)` (metros,
`GetDistance` 0x74CD50); umbral 0 (o −1e6 si es forzado). Si hay hechizo: `SpellEvent{2, pos de la piedra}` y
**`PayFor(−s·costPerKilometer·0.001, true)`** (`fn_005FBF10`). `PayFor` 0x720990 → `fn_00720FC0` hace `chants −= coste`
sin recorte, dividido por `max(poderTribal,1)` con `divideCostsByTribalPower`. **Lectura literal (R13 resuelta): un
salto útil (s > 0) le da cánticos al hechizo; solo un salto forzado hacia atrás (s < 0) cuesta.** Comprobado: un salto
forzado de −30 m cobra 6 (=30·200·0,001); los saltos naturales de +50..60 m tienen ahorro positivo. Luego
`CreateSpotVisual(SPOT_VISUAL 14 VILLAGER_TELEPORT = SF_TeleportVillager, 1,0)` en los dos extremos y
`Living::MoveByTeleport` 0x5EC340: sonido `G_SpellTeleportEnergiseGo` (InGame 39) donde estaba, `..Arrive` (InGame 38)
donde llega, y `MoveMapObject`. Los dos son `SoundTag::Create(MapCoords, muestra, track 0, modo 2, loops 0, +0x40 0,
3D 1, AUDIO_SFX_BANK_TYPE **1 = InGame**, retardo 0)` 0x71EB60 (0x5EC342..0x5EC372; el banco es 1, no 2 como decía el
comentario viejo): ya van por `audio::tags::CreateAtMapCoords`, no por un emisor directo.

- `GPlayer::Process` 0x6496BC → `fn_005FCC70`/`fn_005FCBA0`: cada turno, por cada piedra disponible, se caen de la
  lista de viajeros los que ya no existen o no siguen la reacción de la piedra (`teleport::ProcessPlayers`, ranura 3
  del turno).
### Los aldeanos van al lugar de culto por las piedras (fiel, lane «teleport2» de milagros2)

`Villager::CheckWorshipActivity` 0x76BAE0 → `Villager::CanIGetToTheWorshipSite` 0x76BC20 → `GPlayer::fn_0064D6B0`
(= `teleport::FindRouteStone`), todo releído:

- `CanIGetToTheWorshipSite(MagicTeleport*& out)`: sin pueblo (vt 0x48) o sin lugar de culto (vt 0x30C) devuelve 1;
  si `GetDistanceInMetres` 0x74CD70 (plana: `GetDistance` 0x74CCB0 solo usa los dos primeros dwords del MapCoords) del
  aldeano al lugar es `<=` `maxDistanceThatVillagersWillGoToWorship` (la info del pueblo +0x148, 500 m en Land 1)
  devuelve 1 sin piedra (0x76BC5E..0x76BC69); si es mayor, pide el jugador (vt 0x1C; sin jugador también 1) y llama a
  `fn_0064D6B0(pos del aldeano, pos del lugar, maxDist)`: 0 → no puede ir (0x76BCA3), si no `out` = la piedra y 1.
- `fn_0064D6B0` recorre la lista de piedras del jugador (+0xA58, la más nueva primero) y guarda **por separado** la
  distancia mínima al origen (d1, con la piedra que la da) y al destino (d2), las dos empezando en `maxDist`
  (0x64D6C5/0x64D6C9); al final devuelve esa piedra si `d1 + d2 < maxDist` (0x64D720..0x64D731, estrictamente). Una
  sola piedra puede servir para los dos extremos. Pruebas nuevas en `test_teleport` (`RouteStoneBothEndsAreLookedForApart`).
- Con piedra, `CheckWorshipActivity` (0x76BB99..0x76BC07) hace: `SetReactionDoneWhen(REACT_TO_TELEPORT)` 0x6E44A0 (la
  ficha de la reacción se marca con el turno; si no la tiene, la **añade** al final y, con 3 ya, quita la más vieja,
  0x6E4507..0x6E4540; sin el olvido de 1800 turnos de `fn_006E4340`. Portada aparte en `VillagerWorship.cpp`
  `SetReactionDoneWhen`: `RefreshRecord` de `StopReacting` solo actualiza y no servía), el aldeano sale de la lista de reactores de la reacción que seguía
  (0x76BBAF..0x76BBF2; openblack no guarda esa lista en una `Reaction`, y `SetupReactToTeleport` le cambia +0x94 / +0xBC
  de todas formas) y `StartReacting(REACT_TO_TELEPORT, piedra, la reacción de la piedra +0x94)` (vt 0x994 =
  `Living::StartReacting` 0x6E4590, que despacha por tipo a `Villager::SetupReactToTeleport`). Es decir: primero
  `GotoWorshipSiteForWorship` pone el estado 59 y la meta (el punto de llegada del lugar), y encima se apila
  201/251 hacia la piedra; al saltar, `StopReactingAndSetState` → `PopFromPrevious` devuelve el estado 58
  (`GOTO_WORSHIP_SITE_FOR_WORSHIP`, el estado de reanudación de 59 en el info.dat), cuya **propia función de estado es
  0x76BCC0 = `GotoWorshipSiteForWorship`** (`VillagerOriginalFns.h`), que vuelve a poner 59 y a andar: el aldeano sigue
  hacia el lugar desde la piedra por la que ha salido. Esa fila 58 no tenía función en openblack (el aldeano se quedaba
  quieto): ahora es `villager_worship::GotoWorshipSiteForWorshipState`.
- El ahorro del salto es positivo (va hacia el lugar), así que **el salto le devuelve cánticos al hechizo** (R13).
- Gancho `OPENBLACK_TEST_TELEPORT="x0,z0,0,0,0,worship"` (con `OPENBLACK_TEST_WORSHIP_SITE="NORSE"`): pone la piedra A
  lejos del lugar (x0,z0, o un punto de tierra a 1,15 × maxDist que busca él), la piedra B a 20 m del lugar, mueve al
  aldeano de PLAYER_ONE más cercano 80 m más allá de A (así queda fuera de alcance) y llama a `CheckWorshipActivity`.
  Prueba (`polish_fix_teleport2_worship_walk.log`): el aldeano 27, a 655 m del lugar, da `CheckWorshipActivity -> true`,
  «reacts to stone 2698 … state **251**», anda 80 m hasta A, salta a B («saving 530.87 m», `PayFor(-106.17)`: el hechizo
  gana cánticos), aparece en estado **59** junto a B, anda hasta el punto de llegada y pasa a **60** (baila).
  Capturas: `polish_fix_teleport2_worship_toA.png` (el aldeano andando hacia la piedra A, a 18 m) y
  `polish_fix_teleport2_worship_toSite.png` (recién salido de B, al lado de la ciudadela del lugar de culto).
- `Creature::MoveByTeleport` 0x479F70 y `Creature::ReactToTeleportPriority` 0x4F3A10 / `SetupReactToTeleport` 0x4F3A60
  siguen **pendientes** (no hay criatura).

### SF_TeleportVortex y ZR_SurfRevol (`src/PSys/Rules/SurfRevol`, `src/Graphics/RendererSurfRevol.cpp`)

- El fichero `SF_TeleportVortex` (descomprimido de `Data\Spells\ZSpellFiles\SF_TeleportVortex_txt.zzz`, verificado):
  `ZR_SurfRevol` FunctionIndex 0 (disco plano), textura `S_TileLandscape.raw` (256×256, con alfa
  `S_TileLandscapeA.raw`), NumU 12 × NumV 5, SpeedU 0 / SpeedV 0,213717, `AlphaFadeIn`/`Out` 0,4, `FadeAlphas` 1,
  `ChangeSpecColor` 1, `MaxUVChange` 1, `MaxVertexChange` 0,35, `HeightAboveLandscape` 0,5,
  `DoRaiseAboveLandscape` 1, `RaiseAboveLandscapeRadius` 30, `MaterialUpdateZBuffer` **0**,
  `MaterialUseTextureAlpha` 1, `UseAdditiveAlpha` 0, `MaterialSetDoubleSided` **0** y `UseLighting` **1**; escala 5 por
  `UR_ChangeScale` sobre el átomo padre (`InitialScale` 2); bucle de sonido `SOUND_SPELL_TELEPORT_POOL` en el
  `SoundOfCreate` del `CreateRuleAnAtom` del grupo 0 (LOOPING 1, SOFTRELEASE 1).
  **(pendiente)** `UseLighting 1` y `MaterialSetDoubleSided 0`: openblack dibuja la piscina sin luz y siempre a dos
  caras (`RendererSurfRevol.cpp`), por lo que se ve más tenue que en el original. `UseLighting` (+0x88) pide las
  normales de la malla (`fn_006C9340`) y una luz de D3D que el programa `WorldQuad` no tiene, y el culling pide saber el
  sentido de los triángulos: es trabajo del renderizador de partículas (lane m7/sistemas), no de esta lane, y hacerlo a
  medias dejaría la piscina invisible desde arriba. La `S_TileLandscape.raw` instalada es de 2021 (parche), así que el
  tono tampoco es comparable.
- `ZR_SurfRevol::ModifyAtomCollection` 0x686370 (props 0x6B2E80): crea un `RenderParticleGJMeshRotatingUV` (ctor
  0x6C8B60) con una malla de revolución construida en `fn_006858F0`. El perfil sale de FunctionIndex (tabla 0x6868CC):
  0 `TestDisk` (r=t, y=0), 1 `TestFunnel` (r=t, y=3(√t−1)), 2 `TestFunnelSpout` (r=1,5t, y=3(√2t−1)), 3
  `TestFunnelParab` (r=t, y=3(t²−1)); cualquier otro → disco. Vértice (i,j): `u=i/(NumU−1)`, `t=j/(NumV−1)`,
  `(r(t)cos2πu, y(t), r(t)sin2πu)`, uv `(u,t)`. Con `FadeAlphas`, la fila con `t<AlphaFadeIn` va en RGB `255·t/fadeIn`
  (alfa 255) y con `t>1−AlphaFadeOut` en alfa que baja a 0 (RGB 255); con `ChangeSpecColor` el especular es el color del
  jugador × `(1−ese factor)` (`GetPlayer3DColor` 0x64B590 → tabla 0xBFF0B8 con `GetRemapedPlayer`; el neutral es
  0xFF000000). `fn_00685F00` escala las UV por `TextureWidth/256, TextureHeight/256`; `fn_00685F40` retuerce las UV
  `u += (1−t)²·MaxUVChange`; `fn_00685FC0` gira cada fila `t·MaxVertexChange` sobre Y; con `DoRaiseAboveLandscape`
  (`fn_00686980`) la malla se corta por las celdas y las diagonales del terreno (`fn_00686D90`) y cada vértice sube
  al terreno bajo él menos el del centro, una vez al crearla (`land_morph`, ver
  [rendering-objects.md](rendering-objects.md#mallas-pegadas-al-suelo-land_morph)).
  `RenderParticleGJMeshRotatingUV::GameUpdate` 0x6C8BC0 desplaza las UV (SpeedU/V) dentro de la baldosa; `DrawAt`
  0x67CBA0 dibuja en modo 6 (color = textura×difuso + especular, alfa = textura×difuso).
  - **Tamaño (fiel, verificado):** la malla tiene radio 1 y solo pasa por la matriz dibujada del átomo
    (`RenderParticleGJMesh::DrawAt` 0x67C150 multiplica cada vértice por `DrawData+4`, la PSR de atom+0xD0). Esa PSR
    ya lleva la jerarquía: `PostUpdateAtoms` fn_00673EA0 hace `cur = PSR del padre × local` (fn_00673DB0, fn_007FAE60) y
    `escala = +0x74 × +0x78 × la del padre`. El átomo del disco nace con +0x74 = 1 (ctor `AtomCore` 0x673830) ×
    `Scale` 1 (0x68649E) y es hijo (Hierarchies[2] = 1) del átomo del grupo 2. Radio = 1 × `InitialScale` del grupo 2 ×
    `UR_ChangeScale`: **6 m en `SF_SpellDispenserVortex`** (InitialScale 6, sin UR_ChangeScale; magnitud 1,0 en
    `SpellDispenser::CallVirtualFunctionsForCreation` 0x722840) y **10 m en `SF_TeleportVortex`** (2 × 5).
    `fn_00686980` (DoRaiseAboveLandscape) pasa la malla al mundo con esa misma matriz (fn_00673E40) y la devuelve con
    su inversa (0x686D6C), así que no cambia el tamaño. Error corregido (2026-10-01): el puerto multiplicaba otra vez
    por la escala del padre (36 m de radio el dispensador, 100 m la piedra de teletransporte).
  - openblack: la regla `ZR_SurfRevol` guarda la malla en un `SurfRevolCreator` (tipo `Other`) por átomo; el radio es
    `atom.scale` (que `Effect::PostUpdate` ya multiplica por el padre en una jerarquía). `RendererSurfRevol.cpp` la dibuja con el programa `WorldQuad` en dos pasadas (la textura mezclada y el
    especular sumado con una textura blanca 1×1). La `S_TileLandscape.raw` de esta instalación está fechada en 2021
    (reemplazada por un parche/mod). **Lo comparte la lane m7** para los discos de los dispensadores.
- `SF_TeleportInHand` y `SF_TeleportOnHolder` (sprites) son de M2/M7; `UR_FollowLocalHand` **ya está portada**
  (`src/PSys/Rules/HandFollow.cpp`, registrada en `PSysRegistry.h`: la nota vieja de que faltaba era falsa), aunque su
  efecto en la mano no se ha comprobado en juego. El destello del aldeano SV 14 `SF_TeleportVillager` ya funciona
  (`psys::manager::CreateSpotVisual`).

### Pruebas y capturas

- `test_teleport`: `FastDistance`, la regla del desvío (1,2), `ChooseTarget`, el signo del coste (R13: 500 m útiles
  devuelven 100 cánticos, −20 m forzados cuestan 4) con `Spell::PayFor` real, `FindRouteStone` (dos pruebas: la básica y
  `RouteStoneBothEndsAreLookedForApart`, los dos mínimos por separado y el `<` estricto), los cuatro perfiles de
  `ZR_SurfRevol` y la malla del vórtice (12×5, fundidos, triángulos, torsión de UV y vértices, colores de jugador).
- Gancho `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,jugador[,modo]]"` (`OPENBLACK_TEST_TELEPORT_TURN=<n>`,
  `OPENBLACK_TELEPORT_TRACE=1`): planta dos piedras (como `SPELL_AT_POS`) y, en modo `walk`, hace andar al aldeano más
  cercano hacia B; en `drop`, lo suelta forzado sobre A; `none` solo las piedras; `hand`, el camino real de la mano;
  `worship`, el viaje al lugar de culto por dos piedras (ver arriba).
- Capturas en `dev\_audit\magic\` (**ninguna de las de abajo sigue en la carpeta**: se borraron en limpiezas
  posteriores y aquí quedan solo como registro de lo que se comprobó; las de las lanes «mano» y «teleport2» de
  milagros2, citadas arriba, sí están):
  - `teleport_discs.png`: las dos piscinas `S_TileLandscape` translúcidas sobre el suelo del pueblo de Land1;
  - `disc_dispenser_before.png` / `disc_dispenser_after.png` (`OPENBLACK_TEST_DISPENSER=NORSE_ABODE_SPELL_DISPENSER,1812,2652,1`,
    `OPENBLACK_CAMERA_LOCK=1800,75,2600,1812,30,2652`, `-n 8000 --screenshot-frame 7800`): el disco de estrellas del
    dispensador de 36 m de radio (antes) y de 6 m, al pie de la máquina (después); `disc_teleport_after.png`
    (`OPENBLACK_TEST_TELEPORT=1790,2640,1835,2660,7,none`, misma cámara): las dos piscinas de 10 m;
  - `teleport_react.log`: los aldeanos 47/80/33 reaccionan a la piedra A, andan hasta ella y saltan a B (ahorros
    +50..60 m);
  - `teleport_jump.log`: un salto forzado (soltar) con `PayFor(6, forzado)` y SV 14 en los dos extremos.

## Tormenta, tormenta eléctrica y tornado (M6-storm, `Magic/Spells/SpellStormAndTornado`, `PSys/Rules/Storm`, `ECS/Weather/{LightningFlash,StormClouds}`)

Informes: `destructive.md` §6 y `visuals_sound.md` §4.9; lo leído de nuevo en `runblack.exe` para este port (y lo que
corrige a los informes) está en `dev\tmp_dis\miracles\impl\m6st\` (`tornado.txt`, `stormcast.txt`, `drawclouds.txt` y
`props.py`, que saca los desplazamientos de las propiedades de los `DefineProperties`). Los tres MAGIC_TYPE (16 STORM,
17 STORM_PU1, 18 STORM_PU2) son la misma clase y lanzan el mismo `SF_LightningStormPush`; lo que cambia es el nivel de
mejora que leen sus reglas (−1, 0, 1: el derivado de la semilla, R3 sin verificar) y las filas de efecto y de
`GMagicStormAndTornadoInfo`.

### El hechizo (`SpellStormAndTornado.cpp`, 0xF8 bytes, vtable 0x9847DC)

- Constructor 0x72D9C0: `Spell(tipo, creador)`, +0xEC (el PSys del remolino) y +0xF0 (la reacción de agua) a 0, y entra
  **el primero** en la lista 0xDA07F8 (cuenta 0xDA07FC).
- `InitWithPos` 0x72DAA0: el radio (castData +0) se recorta a [minRadius 20, maxRadius 1000] y se escribe de vuelta
  (`min(r, max)` y luego `max(r, min)`); `Spell::InitWithPos` (el PSys principal, magnitud = radio); después un segundo
  PSys **sin hechizo**, `GJPSysInterface::Create(NULL, 106 SF_StormCast, el LHPoint de la posición, PSysProcessInfo
  +0x24, 1.0, 0)`, cuya magnitud pasa a ser la del hechizo (vt 0x11C).
- `Process` 0x72DB90: olvida la reacción +0xF0 si ya no está; si hay remolino, un `PSysProcessInfo` de ceros con fuerza 1
  y activo que rellena el creador (`UpdateSpellInfo`, vt 0x5C), fuerza 1 otra vez, y un paso del remolino; sin creador,
  o cuando el remolino acaba (5), se borra. Después `Spell::Process`.
- `CalculateCostToMaintain` 0x72DB50 = el de `Spell` × (radio / radiusForNormalCost 40)². `GetRadius`/`Get2DRadius`
  (vt 0x60/0x64) = la magnitud. `CloseDown` es el de `Spell`. `ToBeDeleted` 0x72DA20: sale de la lista,
  `Spell::ToBeDeleted` y borra el remolino.
- fn_0072DCC0, desde el enfriamiento por lluvia del fuego (fn_0072EFB0 0x72F2D9, con la posición +0x14 del objeto que
  arde): el primer hechizo de tormenta sin reacción de agua cuyo radio pase la distancia en x, z a su +0x14 recibe
  `REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE` (34) de su jugador, sellada. Conectado en `ECS/Fire/FireEffect.cpp`.
- Filas (info.dat): `rainAmount` 50 / 100 / **0**: el tornado **no llueve**. Coste por turno 20/25/30 × (r/40)², por
  evento 0/2/10, temporizadores 40 s.

### Los núcleos y las nubes (`UR_CloudMoverNew` 0x6D41C0, `UR_CloudGather` 0x6D4A70)

`SF_LightningStormPush` crea 5 núcleos invisibles (grupo 0, `CreateRuleSphere`), cada uno con su colección de nubes
(grupo 2).

- **`UR_CloudMoverNew`** (ctor 0x6D4150: DelayBeforeMove 0, WindDamping 0,06, WindMagnification 60): el primer paso pone
  los núcleos en el origen del PSys; hasta `DelayBeforeMove` (5 s en el fichero) no hace nada; después, por núcleo: el
  viento `fn_00771B10(p, suavizado)` (0 si lanza el guion), `T = viento × WindMagnification × 0,1`, `v += WindDamping ×
  (T − v) × dt`, `p += v × dt`. La posición del hechizo (+0x14) sigue al núcleo. Con nivel 1 los núcleos rebotan en los
  escudos (`DoAnyShieldDeflections`). 0x6D452C..0x6D4601 miden la pendiente y escalan una **copia** de la velocidad que
  nadie lee: en W120 la pendiente no hace nada.
- **`UR_CloudGather`** (ctor 0x6D4700; las propiedades por desplazamiento en `props.py`; `TornadoGroup` no está en el
  fichero: 10):
  - Primer paso: relámpagos si el nivel ≠ −1; lluvia sí; el rumbo = `GetCurrentHeading` 0x673660 =
    `(cameraForward.x, 0, cameraForward.z)` **sin normalizar**; solo la colección del **primer núcleo** (índice 0 en su
    colección, fn_00673CA0) registra la tormenta, y con nivel 1 le añade al núcleo el grupo del tornado. Ritmo
    `NumAtoms / TimeToForm`; giro ±1 al azar; **el primer rayo** a los `rand(0,5; 1) × SwitchLife + LightningDelay`
    (3 y 8 s en el fichero: entre 9,5 y 11 s). El informe tenía el retraso como intervalo: es al revés.
  - Emite mientras `cuenta < debidas` (la primera nube sale en el primer paso) y haya menos de `NumAtoms`.
  - Cada nube (fn_006D4880): radio `(rand(0,7; 1) + 0,7) × R` (1,4..1,7 R, R = el proveedor Radius = 1,2 × magnitud),
    ángulo al azar, edad 0, altura extra `rand(HeightVaryAmount)`. Pasado `TimeToForm` vuelve a empezar: las nubes
    **giran hacia el centro** en `TimeToForm` s (`r = (1 − f) × radio × (1 + 0,3 cos(edad × 0,1 + θ)) × escala de la
    colección`, con `f = edad / TimeToForm`), con velocidad angular `±MaxAngularSpeed × (1 − (2f − 1)²)`.
  - Su aspecto: crecer = `f / FracToMaxSize` y luego baja; color gris `MaxColor + (MinColor − MaxColor) f`, alfa
    `MinAlpha + (MaxAlpha − MinAlpha) crecer`, escala `(MinScale + (MaxScale − MinScale) crecer) × CloudScale`, y la
    proporción `MaxCloudRatio + (MinCloudRatio − MaxCloudRatio) f` va al estirado del átomo, que el creador de niebla
    usa como su k (`TakeRatioFromMatrix`). La colección, en sus primeros 10 s, multiplica la proporción y el radio por
    `c + (1 − c) × suavizado(t)` (`CloudRatioMaxCollection` 1, `CollectionRadiusInitialScale` 2).
  - Altura: `baseScale × altura extra × escala + CloudHeight`, más la altura media del suelo en una rejilla de 3 × 3
    puntos a 0,33 R del núcleo (con el tornado, la del origen del PSys, que es la base del tornado).
  - Se dibujan como `LH3DMist` con el creador de niebla de la lane del agua (`PSys/Creators/Mist.cpp`): corregido ahí
    que la k tome el estirado y que el color se multiplique por el color base de la tabla de luz [0xFA26A4]
    (`LandLightTable::Current().GetRawBase()`, la del fotograma anterior). El mapa de sombra `S_SMClouds16` **se
    estampa** en el suelo (0x67A7BE..0x67A8C1 → fn_006CA280 → fn_0086CFF0 modo 2 → fn_00878C70): `mist_atoms::SubmitFrame`
    llama a `land_light::AddStamp` (sesión sistemas, U5) en `(x + 10, 0, z + 10)`, centrado, alfa del átomo / 255.
- **La tormenta registrada** (fn_006D5730, sobre los valores de fn_0083F3F0): interior `max(R, 60)`, exterior
  `max(2,5 R, interior + 20, 80)` (los tres `fcomp; test ah, 0x41; je` se quedan con el valor solo si es mayor: el
  informe, el gancho `OPENBLACK_TEST_WEATHER` y esta wiki los tenían como mínimos; corregido); fundido
  `0,5 × TimeToForm` (4 s), vida 1e9 s, fuerza 1, **0 nubes** (`DrawClouds` no dibuja nada para el milagro),
  elevación = el proveedor CloudHeight (1,5 × magnitud); 20 grados, lluvia `min(ftol(fuerza × rainAmount) como byte,
  100)` (así `fuerza 3 × 100 = 300 → 44`), nublado 80, nieve 0; viento = rumbo × `fuerza × lerp(WindMinSpeed,
  WindMaxSpeed, clamp((mag − MagMin) / (MagMax − MagMin)))`, cada componente recortado a ±128 y redondeado a un byte
  (128 da −128). La posición **no** se pone al crearla: `fn_006D5950` la copia del núcleo cada paso, y como el destino
  del `GWeather` se queda en (0, 0, 0), `GWeather::Update` la acerca 0,1 m por turno hacia el origen del mapa antes de
  que la vuelva a poner (fiel, despreciable). Al cerrar se marca para borrar (fn_006D4950); si alguien la borra (un
  `KILL_STORMS_IN_AREA`), el paso siguiente registra otra.
- **Rayos** (0x6D52A0): con relámpagos y nubes medio formadas (las de `f > 0,5`, la lista estática 0xD4EE88, que cada
  colección vacía al empezar), el siguiente en `rand(0,5; 1) × SwitchLife / max(poder tribal, 1)`; sale de una nube al
  azar: la primera vez `AddSubCollection(LightningGroup)` sobre ella; las siguientes la misma colección **se mueve** a
  la nube nueva (fn_00674A30) y, al pasar `rand(0,5; 1) × LightningLife`, se suelta de la nube y se queda sin actualizar
  hasta el siguiente. El rayo es el `UR_Lightning` de M5 en su modo del padre (radio 1 × magnitud): eventos tipo 3 con
  la fila de la tormenta (STORM_PU1: burn 10000, hit 0,0045 → incendia al primer golpe). El trueno es `SoundLightning`
  en la nube, con tamaño al azar (< 0,33 → 3, < 0,66 → 2, si no 1) y banderas |= 0x22.
- **El brillo de la nube** (fiel, sesión milagros2): durante `SpecLife` (0,5 s) la nube del rayo recibe en su +0x90 un
  especular azulado (0x6D5205..0x6D5283: `v = ftol((1 − t / SpecLife) × 255)`, ARGB `(255v, 200v, 200v, 255v) >> 8`;
  pasado `SpecLife`, y en fn_006D4880 0x6D48E5, `0xFF000000`). **Sí se dibuja** (la wiki decía lo contrario):
  `RenderParticle` copia +0x90 al DrawData +0xC (0x679BF4) → `RenderParticleMist::DrawAt` 0x67A6C4/0x67A6D6 lo empuja
  a `SetColour` 0x7F9770 (+0x4C color, **+0x50 especular**) → la rama de efecto de fn_007FA300 (0x7FA3B1..0x7FA5AF) no
  toca +0x50 (la normal lo pisa con la neblina en 0x7FA6DD) → fn_0080DB30 0x80DEF5 lo pone en [0xE9FE2C] → el
  especular de cada vértice (p. ej. fn_0084D2D0 0x84D645: vértice +0x14). `D3DRS_SPECULARENABLE` (0x1D) se pone una
  vez al abrir el dispositivo (fn_0082C8F0 0x82CC1E..0x82CC5F): 1, salvo en una Voodoo ([0xEA9E9C], `CheckDescForVoodoo`);
  ningún otro código la cambia (búsqueda de `push 0x1d` y de todas las llamadas a `SetRenderState` 0x412940), y el modo 6 (fn_0082DF10) tampoco. Así
  que la nube se aclara hacia blanco azulado (color = textura × difuso + especular; el alfa no cambia) y se apaga en
  0,5 s. openblack: `Atom::specular` (+0x90) y `DrawAtom::specular` (`PSys/PSys.h`), escrito por `CloudGather`
  (`Storm.cpp`), pasado por `mist_atoms::SubmitFrame` a `MistDesc::specular` (`Graphics/Mists.h`) y sumado en la rama
  de efecto de `Renderer::DrawMist` (`u_cloudSpecular`, `fs_cloud.sc`). El `SpecColorR/G/B` del creador
  (`ParticleCreator::DefineProperties` 0x6B3562..0x6B359B, +0x24/+0x28/+0x2C, 0 en el ctor 0x6A91C4..0x6A91CA) ya se
  lee (`psys::ReadCreatorProperties`) y `Effect::NewAtom` lo pone en +0x90 como fn_006A85E0 0x6A8748..0x6A875B:
  `(R << 16) | (G << 8) | B`, alfa 0; es 0 en todos los ficheros volcados.
  - **El alfa del especular no es niebla** (cerrado, milagros2): en D3D7 el alfa del especular de un vértice TL es el
    factor de niebla de vértice solo con `D3DRS_FOGENABLE` (0x1C). El juego nunca la pone: no hay ningún `push 0x1c`
    ante `SetRenderState` 0x412940 ni ante `IDirect3DDevice7::SetRenderState` (vt +0x50) en todo `.text` (barrido de
    bytes; tampoco `FOGTABLEMODE` 0x23 ni `FOGVERTEXMODE` 0x8C), y el valor por defecto de D3D7 es FALSE. La neblina del
    motor es por software (luz y especular de cada vértice, fn_007FEB30). Así que el alfa 0xFF / `(255v) >> 8` del +0x90
    no cambia nada en el dibujo y el port, que solo usa el RGB, es fiel. La rama de efecto de fn_007FA300 solo cambia la
    luz (fn_0081E1F0 = `SetLight(0)` del dispositivo, vt +0x48, con la posición (0, 500000, 0) y vuelta,
    0x7FA4F2..0x7FA590) y el [0xC39264] (0xD2 → 0x5A).

### El tornado (`UR_Tornado` 0x6D18B0; ctor 0x6D1680)

Solo con nivel 1 (STORM_PU2): el grupo 10 bajo el primer núcleo, y bajo él el grupo 11 con la regla.

- `UpdateBaseAndTopPoints` 0x6D1C60: el fundido de cierre `1 − (t − cierre) / FadeOutTime`; el alfa de las colecciones
  `min(edad / FadeInTime, fundido)`; la cima sigue al padre (la raíz del tornado, que `UR_FollowParent` deja en el
  núcleo) pasado `DelayBeforeMove`; **la base vaga alrededor de la cima** `(n(t) + n(2t)/2, 0, n(1,3t) + n(2,6t)/2) ×
  TornadoScale × TopMoveAmp` con `t = edad × TopMoveFreq` (`VSNoise1To1`), en el suelo; el origen del PSys pasa a ser la
  base; la cima a `TornadoScale × TopHeight` sobre ella; velocidades por paso. **Cada paso** mientras no cierra:
  `SpellEvent` tipo 2 en la base con su velocidad (fila 18: hit 0,001, empujar 0,001, radio 1).
- El embudo: radio `(BaseRadius + (TopRadius − BaseRadius) h²) × TornadoScale` (fn_006D2790; el informe sospechaba
  otro exponente: es 2); su eje a la altura h, `lerp(base, cima)` con la ganancia de Schlick `gain(FunnelBendParameter,
  h)` en x, z (fn_006D2910; `bias(b, x) = x^(ln b / ln 0,5)`, 0xD4EEA0 = 1 / ln 0,5) más un serpenteo de
  `TornadoScale × WiggleAmplitude` con fase `h × WiggleCount × π`.
- Los átomos que vuelan (`UpdateFlyingAtoms` 0x6D31F0): giran a `lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) ×
  velocidad propia (0,5..1,5)` (más despacio fuera de la pared: × `r / (ρ + 0,1)`), el radio se acerca a la pared con
  `dρ/dt = −0,5 (ρ − r)` integrado con punto medio (fn_006D28B0), suben hacia `lerp(base, cima, mezcla)` con k 0,1
  (objetos reales) o 0,3, y se suma la velocidad del embudo a esa altura. Los objetos reales (estado 0) y los de adorno
  (1) se van al grupo 12 (`GroupToMoveToOnceDone`: gravedad 10, chocan con el suelo, 15 s) cuando pasan de 0,9 de la
  altura o al cerrar; los de adorno empiezan con gravedad y se mezclan en el vórtice en `PretendBlendTime`.
- **Coger** (fn_006D21B0): una vez cada tres turnos de juego, pasado `FadeInTime`, **un objeto por vez**; alcance
  `(r(0) + r(1)) × clamp(poder tribal, 1, 5)`; espiral de `(ceil(R/10) + 2)²` celdas (GUtils::Spiral 0x74D7E0 con
  dirección 1 y cuenta 1, 0x6D22E9: −x, −z, +x, +x...; la auditoría 4 corrigió el recorrido, que era el simétrico); cada objeto con objeto 3D cuya
  celda propia sea esa y a menos de su radio 2D + alcance. Si cabe entero (`CanBecomeAPhysicsObject`, `2 r(0) > radio`,
  `r(1) > radio`; para las vasijas y montones, el flag de su `GPotInfo`, Pot 0x66E8F0) se le pregunta al hechizo
  (`SpellEvent` 7, `CanBeDestroyedBySpell`); si es un montón (`IsPileResource`), se le quitan `lerp(150, 650,
  clamp(TornadoScale))` (redondeado) como **un montón nuevo del tipo de la mano** (vt 0x870 `GetHandPotInfoType`:
  HAND_FOOD 12 / HAND_WOOD 11; de un montón del almacén se quita del almacén), escalado `rand(0,7; 1,2) ×
  clamp(TornadoScale, 0,2, 1)`, y vuela ese. Una criatura: fn_00477060 (no hay criaturas).
- Lo que lleva (`RenderParticleGameObject`, 0x6C9E60): el átomo copia la matriz del objeto (fn_00674150) y el objeto
  dibuja la del átomo (`DrawAt` 0x67B170). Cuando el átomo muere (choca con el suelo en el grupo 12, o a los 15 s), el
  destructor 0x6C9FC0: **un vivo** se deja en su sitio y muere (`DestroyedByEffect`: el aldeano muere), **cualquier otra
  cosa se borra** (`ToBeDeleted`): el tornado destruye lo que se lleva.
- Polvo (`UpdateDebrisAtoms` 0x6D2AF0): `DebrisEmitRate` por segundo, como mucho 50, del color de polvo del material del
  suelo (`GTerrainMaterialInfo.tornadoDustColorRGB`, el segundo material de la altitud en su país); adornos
  (fn_006D2E70): `PretendEmitRate × TornadoScale` por segundo, solo en tierra seca y con el tornado del todo visible
  (alfa > 250): pollos y matorrales. Las mallas del embudo (`S_TornadoNonFade.l3d`, fn_006D2A40) en la base, girando
  `(1 + 0,13 i) × MeshThetaDot`.
- No portado (no se usa): fn_006D1AD0, los sprites del embudo (`NumAtomsToCreate` = 0 en el único fichero). Leídas por
  ningún código de W120: `PretendHeight`, `MaxSearchDistance*`, `LocalSearchDistance`, `PauseBeforeAffectsGameObjects`,
  `UseTornadoStrength`, `K1_Accn`, `ScaleWhipUp`.

### El remolino (`UR_StormCast` 0x6D59B0, SF_StormCast)

30 átomos en anillo alrededor del padre (el grupo 0 tiene jerarquía y escala = la magnitud): radio que se encoge de
`MaxRadius` a `MinRadius` a `RadiusDot` y, pasado `DispersalAge`, crece y se desvanece en `FadeOutTime`; giro
`lerp(ThetaDotMinRadius, ThetaDotMaxRadius, radio normalizado) × (1 ± ThetaDotSpread)`; a `InitHeight × escala` sobre el
suelo; el padre avanza por el rumbo **escalado a 20** (+0x64 del ctor, sin propiedad), acelerando de 0 a 1 entre
`AccnStartTime` y `AccnEndTime`.

### El destello y las nubes de las tormentas registradas (`ECS/Weather/LightningFlash`, `StormClouds`)

- **El destello** (el objeto de 0x24 bytes en GWeather +0x70): `fn_00837290(punto, radio exterior, intensidad)` lo
  enciende en los relámpagos de `GWeather::Update` (0,5 los de horquilla 0x83FBF0, 1,0 los de sábana 0x83FC5D);
  `fn_008372D0` lo envejece cada turno y lo apaga pasados 0,8 s; `fn_00837200` cada fotograma (desde
  `LH3DAtmos::Update3D`, solo las tormentas no marcadas): `s = 1 − edad`, f1 = s, f3 = s³, los dos 0,1 entre 0,2 y 0,5 s,
  × intensidad. El sello de luz del terreno `fn_0086CFF0(f3)` (el mapa radial de 64 × 64 0xED92F0, `(32 − d) × 9`) **no
  se dibuja** (pendiente, sin textura de luz dinámica). **`weather::LightningFlashAtCamera(cámara)`** es [0xFA2768] de
  `Update3D` 0x83587C..0x835903: la tormenta más cercana en x, z (las no marcadas; la primera de las iguales), «dentro»
  si `d² < ((interior + exterior)/2)²` de su descriptor, y `ftol(clamp(f1, 0, 1) × 255)`. **Rareza fiel**: la marca de
  dentro no se borra cuando aparece otra más cercana de la que se está fuera, que entonces da su destello.
- **Las nubes** (`GWeather::DrawClouds` 0x83FC90, para todas las tormentas, marcadas o no, desde fn_0083F8B0 cada
  fotograma): hasta 16 cúpulas (`numClouds` se recorta en el propio descriptor), una nueva por fotograma: k
  `Random(2,5; 5)`, tamaño `(exterior + interior) × 2 Random(0,01; 0,015)`, en `(±1, Random(−10, 10) + elevación, ±1)`;
  cada 400 **fotogramas** un destino nuevo `(±1, Random(0, 20) + elevación, ±1)` al que van en pasos de 0,0025;
  posición `(exterior + interior)/2 × (x, z)` desde la posición dibujada, y la altura sobre el suelo; color = el base de
  la tabla de luz con cada byte × (1 − negrura/2), alfa `fundido × 0,75 × alfa base`, y la neblina por distancia de
  fn_007FEB30; se dibujan por encima de alfa 5. Van a `mists::Submit` (la rama de efecto). La sombra de la tormenta en el
  suelo (`fn_0086CFF0` modo 2 con el mapa 0xEE9D3C, fuerza `(negrura + 0,7) × fundido`) se estampa con
  `land_light::AddStamp` (`StormClouds.cpp`, sesión sistemas); con 0 nubes (el milagro) `DrawClouds` sale antes
  (0x83FC9E `jle 0x8400CB`), así que la tormenta del milagro no pone esa sombra, solo la de sus nubes de partículas.
  `Random` 0x81D180 es el `rand()` del CRT (aproximado: aquí un generador propio del mismo tipo). El milagro no tiene
  estas nubes (0); las climáticas y las de los objetos de tiempo sí (8 por defecto).
- **Nublado en la cámara** (`Clouds::WeatherOvercastAtCamera`, de mapa): `GCamera::Update` 0x4426BA, el byte de nublado
  de `LH3DAtmos::GetWeatherSmooth(cámara, 1)` × 0,01 → [0xD1A26C], que `DrawSky` 0x5E2215 copia a [0xFA2754] para la
  tabla de luz. Puede pasar de 1 (un byte de hasta 127).
  Comprobado para el milagro (milagros2): el nublado 80 de fn_006D5730 (+0x4B) llega a la cámara por
  `storms::CalcAtmos` (lleno dentro del radio interior, 72 m con radio 60; traza `overcast 80`) y `LandLightTable::Build`
  lo aplica como el original: tope del color base `ftol(255 − 96 × 0,8)` = 178 (0x869ADB) y la neblina hacia la de
  tormenta (color `(c >> 3) + 32`, k 48, cerca 15, lejos 350; 0x869DB1..0x869F37). En la tarde de Land 1 el cambio se
  nota poco (`polish_fix_tormenta_overcast90.png`): lo que más oscurece en el original es la sombra de las nubes, que ya
  se estampa (ver abajo).

La alineación del cielo (`fn_0064AC30`, `alignment::GetInterfaceAlignment`), que también cambia las nubes, está en
[La alineación del cielo](magic.md#la-alineación-del-cielo-alignmentgetinterfacealignment).

### Inferido, aproximado y pendiente (tormenta)

- (sin verificar, R3) El nivel de mejora por la semilla: −1 / 0 / 1.
- (inferido) Lo que hace `InitialisePhysics` con un aldeano o animal que se lleva el tornado: aquí sale de las físicas y
  pasa al estado de la mano (IN_HAND, `animal_ai::PlaceInHand`). El bit 0x10 de Object +0x25 que mira fn_006D2140 no se
  comprueba. La distancia de `GetDistanceInMetres` se toma en x, z.
- (aproximado) El sitio donde muere un vivo soltado: donde se vio su átomo por última vez (el turno o el fotograma). La
  escala del objeto llevado es uniforme (la del eje Y de su matriz), como en el original.
- (aproximado) Quitar recursos de un montón suelto: la cantidad y el hundimiento (como la mano); el montón vacío no se
  borra. Los montones se buscan también fuera de la rejilla del mapa (openblack no la llena con las vasijas).
- (aproximado) La colección del tornado se actualiza el mismo paso en que se crea (el original la enlaza en la cabeza de
  la lista y entra el paso siguiente): `Effect::UpdateCollection` recorre ahora por índice.
- (aproximado) Las nubes de las tormentas registradas cuentan fotogramas como el original, pero openblack dibuja más
  fotogramas por segundo; su contador del atlas avanza siempre (no solo en pantalla); la base de la tabla de luz es la
  del fotograma anterior.
- (hecho con `land_light::Stamp`, sesión sistemas U5; comprobado en el juego por milagros2 «pulido3») El sello de luz
  del destello (`LightningFlash.cpp`), la sombra de la tormenta registrada (`StormClouds.cpp`) y **la sombra de las
  nubes del milagro** (`Mist.cpp`): `RenderParticleMist::DrawAt` 0x67A7BE..0x67A8C1 mete por nube un registro en la lista 0xD4EDB8
  (posición `(x, 0, z)`, cuadro 0, alfa = byte de alfa del DrawData × 1/255 recortado a [0, 1]); `PSysLightMaps::AddDrawing`
  0x6CA6E0 → fn_006CA280 lo estampa con fn_0086CFF0 en modo 2 (sombra: bpp 1 porque `IsShadowMap`,
  `ParticleMistCreator::GetBitmap` 0x6AA540), 16 × 16 celdas de 10 m; fn_0086D360 → fn_0086D060 → **fn_00878C70**:
  bilineal, `v = 255 − (255 − texel) × fuerza / 255` con suelo 0x30, y `min(v, byte +3)` en el color por vértice de las
  celdas del LandBlock (bit 4 de +0x920; ClearLight fn_0086D460); la luz del rayo es el modo 1 (fn_00878780).
  En el juego (`polish_fix_pulido3_*`, de día): el suelo bajo la tormenta queda más oscuro que sin ella
  (`polish_fix_pulido3_ctrl_t80.png`), con manchas oscuras de bordes marcados que se mueven con las nubes, y cada rayo
  enciende el suelo (y los árboles, que toman la luz de su celda) en un parche azulado. Sin el original al lado no se
  puede afirmar si la intensidad es la misma (el suelo 0x30 deja manchas casi negras). Pendiente (de terreno, no de
  Storm/Mist): el orden fila/columna del texel en `land_light::ApplyStamp` frente a fn_00878C70; y siguen sin portar
  las criaturas (fn_00477060) y fn_006D1AD0.
- (aproximado) El contador del atlas de cada nube (`Atom::mistCounter`, el +0x84 de su `LH3DMist`, que el ctor 0x7F9560
  empieza en `ftol(Random(0, 16)) & 15`, 0x7F95DC..0x7F95FB): el `Random` 0x81D180 con un generador propio, y avanza en
  cada fotograma aunque la nube no esté en pantalla (el original solo avanza las que `LH3DMist::AddDrawing` 0x7FA7F0 ve).
  Antes era un contador global para todas: ya no laten a la vez.
- **El embudo del tornado**: `ParticleMeshCreatorAnimTextured` **sí tiene** la propiedad `DrawWithLandscapeColor`
  (corregido en la auditoría: la tanda de pulido decía que no): su `DefineProperties` 0x6B3970, tras las de
  `ParticleBaseMeshCreator` 0x6B37A0 y las suyas, la lee como última propiedad en su +0x84 (0x6B3AEF..0x6B3AFD; el ctor
  0x6A8BB0 la deja a 0 en 0x6A8BF6), y su `CreateParticle` 0x6A8F0D..0x6A8F20 la pasa al bit 1 del +0x24 (el que mira
  `Particle3DObj::DrawAt` 0x67A00C para fn_0080BEC0). El `DrawWithLandscapeColor=1` del fichero vale: el embudo va
  multiplicado por la luz del suelo, como antes de la tanda (`Creators/Mesh.cpp`). `UseGlobalAlpha` (+0x5A) es el bit 0 del +0x24
  (0x6A8E04..0x6A8E17) → vt 0x48 `SetGlobalAlpha(1)` (0x67A216..0x67A227), la tabla 0xC387C8, que deja igual el modo 6
  del material (tipo 4 → 6 en 0x57E120: `SRCALPHA/INVSRCALPHA`, alfa = textura × difuso). Que se vea poco **es fiel en
  buena parte**: la textura de la malla (piel 0x16AA, 256 × 256 ARGB4444 dentro del .l3d) es blanca con alfa 1/15..12/15
  (media ≈ 0,45), × `ColorA` 60/255 → ≈ 0,1 por capa; son 2 mallas de dos caras (`CreateRuleSphere_TornadoMesh`
  `NumAtoms=2`). `ParticleMeshCreator::CreateParticle` 0x6A8B00 nunca pone el bit 0 (`UseGlobalAlpha`). Revisado en
  «rayo3» y ya portado: ver [Las mallas de partículas](#las-mallas-de-partículas-y-la-cúpula-del-escudo). Comprobado de nuevo de día con la luz del suelo
  (`polish_fix_pulido3_tornado150.png` / `230.png`, milagros2 «pulido3»): el embudo se ve como una columna gris
  translúcida con lo que arrastra dentro; tenue pero visible, como dan los datos.

### Ganchos, pruebas y capturas (tormenta)

- Ganchos: `OPENBLACK_TEST_SPELL=STORM,x,z,60` (y `STORM_PU1`, `STORM_PU2`), `OPENBLACK_TEST_STORM_SHOT`,
  `OPENBLACK_TEST_STORM_STRIKE_SHOT`, `OPENBLACK_TEST_STORM_PILE`, `OPENBLACK_TEST_STORM_CLOUDS` y
  `OPENBLACK_STORM_TRACE` en [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración).
- `test_storm`: el recorte y el coste del hechizo, el descriptor de fn_006D5730 (radios, viento, lluvia y sus rarezas),
  las rampas de las nubes, el embudo (radio, bias/ganancia, RK2), una colección de nubes que registra su tormenta y la
  marca al cerrar, el destello (curva, mapa, cámara y la marca que se queda), el color de las nubes registradas, la
  alineación de la interfaz y, con el juego, `SF_LightningStormPush` / `SF_StormCast` (sus clases registradas; el
  remolino solo, que acaba a los 4,4 s).
- Capturas en `dev\_audit\magic\` (Land1, lanzado en el turno 200 sobre el almacén):
  - `m6st_clouds_10/40/80.png` (STORM desde arriba): las nubes se juntan sobre el pueblo; la traza da lluvia 50 y
    nublado 80 en el centro;
  - `m6st_gather_15.png`, `m6st_gather_45.png`, `m6st_rain_90.png` (STORM_PU1 de cerca, con el árbol 953 ardiendo desde
    el turno 195): rayas de lluvia; el fuego pasa de 25 llamas a 2 en 4 s y echa vapor (lluvia 100: se enfría dos veces
    más rápido, `1 + 0,01 × lluvia`);
  - `m6st_strike_3/7/12.png` (los rayos n.º 3, 7 y 12): horquillas desde las nubes; con burn 10000 arden el almacén,
    casas y árboles;
  - `m6st_tornado_85..160.png` y `m6st_pile_82..120.png` (STORM_PU2, con un montón de comida de 400): el polvo, árboles,
    vallas y aldeanos girando; la traza da `tornado takes pile/pot ...` con trozos del montón y del almacén;
  - `m6st_puffs.png`, `m6st_puffs_high.png` (`OPENBLACK_TEST_STORM_CLOUDS="1818,2628,60,8,0.5,160"`): las cúpulas de una
    tormenta registrada vistas desde dentro y desde arriba.
  - Tanda de pulido (milagros2): `polish_fix_tormenta_side_strike2.png` (STORM_PU1 desde el lado, rayo n.º 2: el disco
    oscuro y los rayos continuos), `polish_fix_tormenta_side_glow101.png` y `polish_fix_tormenta_game_glow101.png` (el
    turno siguiente al rayo, `OPENBLACK_TEST_STORM_SHOT=101`: las nubes del rayo encendidas por el especular; el
    especular se escribe en el paso siguiente al rayo, así que la captura del turno del rayo aún no lo tiene),
    `polish_fix_tormenta_game120.png` (cámara de juego bajo la tormenta: nube oscura y lluvia),
    `polish_fix_tormenta_overcast90.png` (STORM con nublado 80 en la cámara) y `polish_fix_tormenta_tornado150.png`
    (el embudo sin la luz del suelo, con el cambio que la auditoría deshizo). `test_storm` `cloudMistAtoms`: el contador propio de cada nube y el especular en
    los átomos de dibujo; `creatorSpecColour`: `SpecColorR/G/B` del creador.
  - Tanda «pulido3» (de día, `--mod game.skip-intro=off`, lanzado en el turno 200): `polish_fix_pulido3_pu1_t80.png`
    (STORM_PU1 sobre el almacén, cámara de juego, 80 turnos: suelo a la sombra) frente a
    `polish_fix_pulido3_ctrl_t80.png` (la misma toma con la tormenta lejos), `polish_fix_pulido3_pu1_strike3.png` (rayo
    n.º 3: la luz del rayo en el suelo), `polish_fix_pulido3_storm_high90.png` (STORM desde 260 m: la masa parda de nubes
    sobre el pueblo), `polish_fix_pulido3_tornado150.png` / `230.png` (STORM_PU2 con un montón de 400: el embudo y las
    sombras de las nubes en la ladera).

## Explosión de rayo y clases de PSys que faltaban (M6b, `PSys/Rules/{Explosion,KeyPoints,Orient,Forest}.cpp`)

La explosión de rayo (MAGIC_TYPE 7-9 EXPLOSION_ONE, EXPLOSION_ONE_PU_ONE y _PU_TWO; semilla BEAM_EXPLOSION) es un
`Spell` simple (`SpellGeneral.cpp`, clase General): todo lo que hace sale de los eventos de `UR_Explosion`. Lanza los
tipos de partícula 11 / 12 / 13 (`SF_BeamExplosionSingle` / `Many` / `Loads`). Desensamblado en
`dev\tmp_dis\miracles\impl\m6b\` (`explosion_*.asm`, `moveatom.asm`, `meshdraw.asm`, `forestpath.asm`).

### Los archivos (fiel)

- Grupo 8 (creado al empezar): un átomo de control con `SetPSysCloseDown` 0x6A26D0 tras 6 s
  (`EventConditionCollectionDelay`; 13,9 s en Loads), que cierra el efecto (`PSysManager::SetState(1)` 0x672FF0). En
  Many/Loads un `SpreadingDiskEmitter` reparte 6 (Many, radio 30→50) o 6 + 50 (Loads, 25→40 y 20→60) puntos de
  explosión más.
- Grupo 7: el padre de cada explosión (muere a los 16 s); grupo 6: la colección de `UR_Explosion`, sin átomos.
- Valores de `UR_Explosion` en los tres archivos: `InitialDelay` 0,4, `TimeToDoEventsFor` 5, `SmokeDelay` 1,2,
  `BlastSpeed` 50, `SpreadSpeed` 20, `MaxDistance` 15, `MaxObjectsToDelete` 15, `MaxObjectsToExplode` 15; `BeamDelay`
  no aparece (0, el del ctor). Valores por defecto del ctor 0x67E090 (en el port desde la fusión con el agua):
  MaxObjectsToDelete / ToExplode 20, MaxDistance 100, BlastSpeed 10, SpreadSpeed 10, TimeToDoEventsFor 5,
  InitialDelay 3,5, SmokeDelay 3, BeamDelay 0.
- **Arreglado (fiel):** `SpreadingDiskEmitter` (MAC 0x6A6610, DefineProperties 0x6AFA90) estaba registrado como un
  `DiskEmitter` normal, que lee `Radius` (0): todas las explosiones de Many y Loads caían en el mismo punto. El original
  emite **varios átomos por paso** (bucle con `ShouldEmit`) y mueve cada uno `(cos θ r, Height, sin θ r)` con
  r = PSysFloatRand(StartRadius, StopRadius) y θ = PSysFloatRand(2π). Sus `StartTime` / `StopTime` (+0x5C / +0x60) son
  propiedades que la clase **no lee**. (`DiskEmitter` 0x6A64D0 sí coincidía: un átomo por paso, dirección al azar en el
  disco con radio PSysFloatRand(Radius) y Height en y. `ShouldEmit` 0x6A63A0 también: periodo 1/EmissionFreq con
  `Randomise` × (0,5 + rand(0,5)), `MaxTotalAtomsToEmit` y `MaxAtoms`.)

### `UR_Explosion` (R5: InitCollection 0x67E200, ModifyAtomCollection 0x67ECE0, actualización 0x67E900)

- Propiedades (DefineProperties 0x6B0B90, una `AtomCreateRule`): +0x2C MaxObjectsToDelete, +0x30 MaxObjectsToExplode,
  +0x34 MaxDistance, +0x38 BlastSpeed, +0x3C SpreadSpeed, +0x40 TimeToDoEventsFor, +0x44 InitialDelay, +0x48 SmokeDelay,
  +0x4C BeamDelay. Datos por colección (0x58 bytes, ctor fn_0067E140): centro +0x24, «sin empezar» +0x20, «queda
  objetivo» +0x21, anillo +0x30, lista {turno, objeto} +0x34, explotados +0x48, borrados +0x4C, humo / rayo / parado
  +0x50 / +0x51 / +0x52 y el contenedor del rayo +0x54.
- **Cada paso** (MAC 0x67ECE0): centro = `GetCurrentParentPos` con y = la altura del suelo. Si no está parado ni
  cerrándose: con la edad de la colección > InitialDelay, `InitCollection` (una vez). Mientras edad < InitialDelay +
  TimeToDoEventsFor, **un `SpellEvent 2` en el centro por paso** (velocidad 0, fuerza 1, sin escudos): el evento por
  defecto del hechizo, burn / crush / hit del efecto (BEAM 200 / 0,01 / 0,01, PU1 400, PU2 800 / 0,02 / 0,02) en su
  radio (5, 5, 10) con `ApplyEffectToMapPos`, pagando `costPerEvent` (10 cánticos) cada vez: unos 50 eventos. Con edad >
  BeamDelay, el spot visual 36 `BEAM_EXPLOSION_FX` en el centro (escala 1, 60 turnos); con edad > SmokeDelay, el 23
  SMOKE en tierra seca o el 22 STEAM en el agua (`MapCoords::IsDryLand` 0x603620), **magnitud 8** durante 4 s.
  - **Turnos, no segundos** (rayo3, fiel): `CreateSpotVisualWithSpecifiedDuration` 0x63E580 recibe turnos. Son los 60
    de 0x67EE92 y, para el humo, una copia en línea de `NumGameTicksPerSecond`: `(1000 div [0xD01A38]) · 4`, ftol,
    `and 0xFFFF` (0x67EEF8..0x67EF2C; no llama a 0x711630).
  - El port los pasa por `manager::CreateSpotVisualTurns`. Antes pasaba segundos y `CreateSpotVisual` los volvía a
    convertir: con un turno distinto de 100 ms la cuenta no salía igual.
  - Los dos efectos salen en `MapCoords(centro)` 0x603160. El efecto del contenedor arranca en ese MapCoords como punto:
    fn_0063E410 0x63E418..0x63E47B, `GetAltitude` + la altitud.
  - Captura `polish_fix_rayo3_smoke_t40.png` / `_t80.png` (+ `.log`; `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,1825,2632"`,
    `OPENBLACK_TEST_MAGIC_TURN=300`, `OPENBLACK_TEST_EXPLOSION_SHOT` a los turnos 40 y 80, cámara
    `1770,75,2575,1825,35,2632`, `--mod game.skip-intro=off`):
    - el registro da `spot visual 36 (SF_BeamExplosionFX) ... for 60 turns` y `spot visual 23 (SF_Smoke) ... for 40 turns`;
    - en t40 el humo aún sube del centro;
    - en t80 ya no queda.

  Parado o cerrándose: se cierra el rayo (`GParticleContainer::CloseDown`
  0x63E370) y se vacía la lista. Después, siempre, la actualización 0x67E900.
- **El símbolo `RecursiveUpdateForkStructure@UR_Lightning` 0x67E900 está mal puesto**: es la actualización de
  `UR_Explosion` (sin recursión). Cada paso el anillo crece `SpreadSpeed × dt` ([0xD4E0EC]); los objetivos que ya no
  están disponibles salen; con explotados ≥ MaxObjectsToExplode **y** borrados ≥ MaxObjectsToDelete se vacía la lista.
  Recorre la lista y **trata como mucho un objeto por paso**: el primero con objeto 3D (+0x40) que el anillo alcance en
  3D (`|p − centro|² ≤ (GetRadius + anillo)²`) recibe, si está dentro de un escudo con margen 2 (fn_006D0BC0), una chispa
  (fn_006D0AF0) y un evento 4 al escudo (con 0 se salva); después `GetActualObjectToEffect` (vt 0x5D8) y la pregunta
  `SpellEvent 7` (CanDestroy). Con 1: si no es criatura y quedan, explotados + 1 y la malla en pedazos (fn_00681260,
  ver «Los pedazos» abajo); si quedan borrados, borrados + 1 y `DestroyedByBeam` (vt 0x500) si [0xC029EC] (1). Conteste lo que conteste,
  el objeto sale de la lista; el que contesta 1 corta el recorrido de ese paso.
- **InitCollection 0x67E200**: margen = el radio del efecto del hechizo (`GMagicEffectInfo` +0x2C = archivo 0x1C), 5 sin
  hechizo. Dentro de un escudo (fn_006D0BC0 con ese margen): el punto donde un rayo desde 200 m más arriba corta la
  esfera (vt 0xFC FindIntersect), chispa y evento 4 en el centro al hechizo del escudo; **con 0 la explosión se para del
  todo** (+0x52). Luego: en tierra seca una marca (fn_008251C0, `ecs/GroundMarks`, ver
  [rendering-objects.md](rendering-objects.md#mallas-pegadas-al-suelo-land_morph)); en el agua **tres anillos** (crecimiento 5,
  7 y 10; edad 0, ángulo 0, aspecto 1, ritmo 1, celda 0x30, blanco; +0x24 = 1,0 sin identificar; una sola
  implementación, `psys::water_rings::AddExplosionRings` de `PSys/PSysWaterRings`, de la lane del agua). Los objetivos: r =
  MaxDistance × el poder tribal del hechizo entre 1 y 5; las `ceil((r + 20) / 10)²` celdas de la espiral
  (`GUtils::Spiral` 0x74D7E0) desde la del centro; de cada celda, los objetos móviles y fijos disponibles **cuya celda
  propia es esa** (fn_00604F40) y a menos de `Get2DRadius + r` en x/z (`GetDistanceInMetres` 0x74CD70, una hipotenusa).
  Anillo = 0. Por último cinco rocas `MSH_Z_SPELLROCK01` (567) en el centro ± 4 m que se rompen en pedazos (ver «Los
  pedazos» abajo).
- **El cráter** (`ecs::ground_marks`, `src/ECS/GroundMarks`, de la sesión sistemas; 0x67E35C..0x67E395, solo si `MapCoords::IsDryLand` 0x67E353, es decir, si no salen
  los anillos). La clase se llama `RootsPile` (símbolos del Mac: `__ct__9RootsPileFRC7LHPointffl`,
  `DrawAll__9RootsPileFv`). **No es `TemporaryShadow`**: esa es fn_00825090 (lista 0xEB99FC, una sombra dinámica).
  - `new RootsPile(centro, PSysFloatRand(2π), [0x9357D4] = 8, malla 0x251)` = fn_008251C0 → fn_00825240. El centro es
    el mismo punto que se pregunta con `IsDryLand` (0x67E347 y 0x67E392 empujan el mismo `LHPoint`): el origen del efecto, no se
    baja al suelo. Lo que lo amolda es `UpdateMelting`, que guarda por vértice
    −(altura bajo el vértice − altura bajo el origen) / escala (0x81695B y 0x816A59..0x816A77), igual que el
    `MorphWithTerrain` de openblack.
  - La malla es `MeshPack[0x251]` = 593 `TreeRootsPile`, el mismo montón de tierra del árbol arrancado. La guarda
    [0xEB9A04] el primer `RootsPile` que se crea, y los siguientes usan esa aunque pidan otra. Los dos llamadores piden
    0x251.
  - `LH3DObject::Create(1)` 0x80B4D0: un objeto **morfable** (vtable 0x9A2E34). Con `UpdateMelting` (vt 0x1E8, una vez)
    se amolda al terreno: en una ladera el hoyo se dobla con ella.
  - `SetPosition` 0x423140 (vt 0x20): el origen en el centro, girado el ángulo alrededor de Y, escala 8 en los tres
    ejes. vt 0x58(1) pone la marca 0x20 solo con el detalle Light [0xC38224]; vt 0x40(0) quita la 0x10.
  - Vida +8 = 0x3A98 = **15000 ms**, y un `SmokyStuff::Create(centro, 1, 1,0, −1)` (0x8252EB). Es el polvo marrón del
    modo 1: velocidad de 1,5 × tamaño (0x823DA7), 0x68503D, se va en 1,5 s.
  - **`RootsPile::DrawAll` fn_00825350**, cada fotograma desde fn_005E5CD0 (0x5E6197, justo antes del humo
    fn_00824140), con `g_game_time_inc` en ms. Con ≤ 1000 ms el alfa del color es `ftol(ms × 0,255)` ([0x9A2BA8]) y
    se llama a `SetGlobalAlpha(1)` (vt 0x48); después ms −= g_game_time_inc. Con ≤ 0, fn_00825300 lo saca de la lista y
    borra el objeto; si no, se dibuja como un objeto normal (`AddDrawing` 0x815A70, luz de la tierra fn_00801C90). No
    se hunde ni cambia de escala: dura 14 s entero y se desvanece en el último segundo.
  - `ClearAllStuff` 0x82AEFD los borra al cambiar de mapa.
  - En openblack es `ecs::ground_marks` (U3 de sistemas; ver
    [rendering-objects.md](rendering-objects.md#mallas-pegadas-al-suelo-land_morph)): una entidad `Transform` + `Mesh` +
    `MorphWithTerrain` (una vez, `Melting::Snapshot`) (+ `Alpha` en el último segundo), con el `SmokyStuff` de modo 1.
    **(pendiente)** las dos marcas de dibujo del `LH3DObject` no tienen equivalente: la 0x20 del detalle Light
    (fn_008168C0) y la 0x10 apagada (fn_007F97A0). Sombra estática no echa en ninguno de los dos (`CastsStaticShadow`
    pide `Fixed` / `MobileStatic` / …, y la entidad no lo es). El alfa del último segundo va como `Alpha` (la pasada
    translúcida del renderizador), que es lo que hace `SetGlobalAlpha` (marca 0x80, fn_007F9D60) en el original.
    **(aproximado)** la luz de la tierra de fn_00801C90 sobre el color del montón la hace el alumbrado normal de mallas.
  - El montón del árbol arrancado (fn_0074BD20 → fn_008251F0, escala `(M+0x24 + M+0x2C) × escala × 0,3`) usa la misma
    `ground_marks::Create`, y con ella el mismo polvo de modo 1.
- **`Object::CanBeDestroyedBySpell`** (vt 0x778, 0x639960; responde al evento 7, 0x720DBD, que devuelve `== 1`):
  `IsEffectReceiver(NULL)` y no la marca +0x25 & 0x40, y si está en un guion (vt 0x448 con g_game +0x25005C → +0x45E8
  y +0x45EC) solo para un hechizo con +0x25 & 4. Dicen 0: `Creature` 0x47B1E0, `Field` 0x529FF0 y `CitadelPart`
  0x4695D0 (corazón de la ciudadela, partes, corral, lugar de culto, tótem de culto). **(inferido)** openblack no lleva
  la marca 0x40 ni los objetos de guion: se toman a 0.
- **`Object::DestroyedByBeam`** (vt 0x500, 0x63AB20) = `ToBeDeleted(0)`: árboles y árboles muertos con `DeleteTree`,
  animales con `animal_ai::Remove`, el resto como el `DestroyedByEffect` genérico. `Abode::DestroyedByBeam` 0x402CB0
  (todas las clases de Abode, el almacén, el dispensador, el tótem): `ReduceLife(GetLife(0))`. **(aproximado)** los
  aldeanos se van con `life::Kill` (el `Villager::ToBeDeleted` no está portado).
- **Qué le pasa al edificio en el original** (leído para el arreglo; corrige lo de «ruina / fantasma» de la auditoría):
  - `Abode::ReduceLife` 0x405D90 → `MultiMapFixed::ReduceLife` 0x52F5E0. Un edificio construido (`IsBuilt`, vt 0x890)
    usa `Object::ReduceLife`, así que la vida queda en 0. Uno a medio construir pierde porcentaje construido
    (fn_0052EDD0).
  - Con vida < 1, cada habitante (+0xA0, siguiente +0xE4) hace `SetStateWhenTappedOnAbode` 0x752B80. Si cruza
    `GetPercentRepairedForNonFunctional` (vt 0x894): `StopBeingFunctional` (vt 0x918) y, si
    `CausesTownEmergencyIfDamaged`, `Town::SetInStateOfEmergency`.
  - Con ciudad y sin obra (+0x74), `Town::AddBuildingSite` 0x73B8E0. Con obra, +0x640 = 1,1 × vida − 0,1.
  - Dibujo: con la obra, `IsDrawBuilding` 0x52F0C0 = 1. Sin `DestructionMesh` (+0x90, la de las rocas),
    `GetPercentRepairedFromWhenDamaged` 0x52F010 da 0,98 × vida (0x52F09C) y `GetPercentForDrawBuilding` 0x52EFD0
    = min(construido, eso) = 0.
  - `Abode::Draw` 0x515F70 → `MultiMapFixed::Draw` 0x518090 → `DrawBuilding` 0x517F90: a 0 no dibuja nada (0x517FEF),
    solo el fuego si arde. **El edificio desaparece de la vista** y queda como obra hasta que lo reparen. Sin ciudad
    no hay obra y se sigue dibujando entero.
  - **(pendiente, lane mapas)** las ciudades de openblack no tienen obras (`Town::AddBuildingSite`, la reparación). Por
    eso aquí la vida baja a 0 y el edificio sigue dibujado entero, como con el fuego.

### El rayo que se ve (SF_BeamExplosionFX, PT 138)

- `UR_MoveAtom::ModifyAtomCore` 0x6A5E50 (DefineProperties 0x6AE240: +0x20 StartTime, +0x24 StopTime, +0x28
  MoveSmoothly, +0x2C..+0x34 Start, +0x38..+0x40 Stop): entre StartTime y StopTime (incluidos) t = (edad − inicio) /
  (fin − inicio), 1 en el paso que llega al final, `t²(3 − 2t)` con MoveSmoothly; la posición local = inicio + (fin −
  inicio) t. La columna `MSH_S_BLAST_CENTRE` baja de 120 m al suelo en 0,4 s.
- `UR_ChangeScaleXYZ::ModifyAtomCore` 0x6A5240 (DefineProperties 0x6ADE60): ruleScale = el XZ interpolado, stretch =
  Y / XZ (0 si XZ ≤ 0,0001, [0x8BF518]); pasado StopTime, solo el primer paso escribe los valores finales. Los cuatro
  conos `MSH_S_BLAST_CONE` se abren de 0 a 12 en XZ con Y 8.
- La columna lleva `FaceCamera` (abajo). El mapa de luz `S_BeamSingleLightMap.raw`, el sonido `LASERBEAM_1` y el
  `AddSoundToAtom` de `LASERBEAM_EXPLODE` con temblor de cámara ya estaban portados.

### Las mallas de partículas y la cúpula del escudo

El dibujo de las mallas del PSys (`ParticleMeshCreator`, `UsePlayerColor`, `FaceCamera`, `DrawCutByPlane`), que hace
visible la cúpula del escudo y el rayo, está en [Las mallas de partículas](particles.md#las-mallas-de-partículas-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-y-la-cúpula-del-escudo).

Revisado en la lane «rayo3» de milagros2 (`Creators/Mesh.{h,cpp}`, el camino de `mesh_atoms::Instance` en
`RenderingSystem.cpp` y la rama PSys de `vs_object`):

- **Materiales** (fiel). Cada creador reescribe los materiales de su malla con `GJUtils::SetMaterialProperties` fn_0057E1D0
  la primera vez que la busca, en su primera partícula. El port lo hace en la primera `InitAtom`, con
  `L3DMesh::SetMaterialProperties`. Según el creador:
  - `ParticleMeshCreator`: fn_006A8A40 0x6A8A5F..0x6A8A6E con la malla del paquete y `MeshChangeMaterialProps`;
  - `ParticleMeshCreatorAnimTextured`: fn_006A8CC0 0x6A8CDF..0x6A8CEE;
  - el fichero de malla: `PGetSharedMesh` 0x57DF18 al cargarlo;
  - `ParticleAnimCreator`: fn_006A95E0 0x6A9602 con +0x93 = 1, o fn_0057D420 0x57D433 con el fichero, siempre.

  `MaterialProperties` = +0x55 aditivo, +0x56 Z, +0x57 dos caras, +0x58 cambiar, +0x59 alfa (sin propiedad, 1 en los
  ctors 0x6A897D / 0x6A8BD0; del animado, +0x90..+0x94). Las mallas del paquete cambian para todos sus usuarios, como en
  el original. La semilla del bosque (Seed.L3D, tipo 2) queda en 8 `TexturedAlphaNz`, que mezcla con el alfa.
  **(aproximado)** si dos creadores cargan el mismo fichero con propiedades distintas, aquí aplica cada uno las suyas; el
  original guardaba las del primero.
- **`UseGlobalAlpha`** (fiel). Es el bit 0 del +0x24 de la partícula. `Particle3DObj::DrawAt` 0x67A216..0x67A227 /
  `Particle3DAnim::DrawAt` 0x67A9D6 lo pasan a `SetGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 del +4 del objeto). Con
  él, el dibujo usa la tabla 0xC387C8 (fn_0080DB30 0x80DEED..0x80DF09), que lleva los modos opacos 0, 2, 4, 9 y 17 a
  sus versiones con alfa. Sin él usa la 0xC38728: el alfa del color solo se nota en los modos que mezclan. Quién lo pone:
  - `ParticleMeshCreator`: **nunca**. Su propiedad +0x5A se lee (0x6B3902), pero `CreateParticle` 0x6A8B00 no la usa, y
    el ctor 0x6C7A23 borra el bit;
  - `AnimTextured`: sí, desde +0x5A (0x6A8E04..0x6A8E17);
  - `ParticleAnimCreator`: sí, desde +0xA5 (ctor 1 en 0x6A93CA, 0x6A983A..0x6A9840; 0 mientras mezcla dos mallas en
    0x67A9A4, cosa que no hace ningún fichero).

  En el port, `Instance::globalAlpha`:
  - solo las aditivas y las de alfa global con alfa < 1 van con las mallas que se desvanecen (tabla 0xC387C8);
  - las demás van con su malla, y el alfa (1 − [0][3]) solo lo toman sus primitivas que mezclan;
  - los modos 12 y 13 son iguales en las dos tablas, así que las aditivas no cambian;
  - en los ficheros, la única `ParticleMeshCreator` no aditiva es la semilla de SF_Forest, y su modo 8 también es igual
    en las dos tablas.
- **Especular de la DrawData** (+0xC, el +0x90 del átomo; pedido de la sesión shaders). `Particle3DObj::DrawAt` se lo
  da al objeto junto al color: fn_0080BEC0(color, especular) 0x67A012, que fn_0080BF10 suma al especular del suelo, o
  `SetColour` vt 0x2C 0x67A023 (obj +0x50, fn_007F9770).
  - `mesh_atoms::Instance::specular` lo lleva. Va en la w de la cuarta columna como 3e6 + 7 bits por canal, como
    `components::SpecularColour`, y la rama PSys de `vs_object` ya no lo pone a 0.
  - **(aproximado)**: se pierde el bit bajo de cada canal.
  - Con `DrawWithLandscapeColor` esa w lleva el color, así que el especular no se manda. Ningún fichero da especular a
    una malla (`SpecColorR/G/B` 0).

### Las otras clases que faltaban

- `UR_KPStretchHeight` 0x6A50C0 y `UR_KPMoveAtoms` 0x6A60B0 (el champiñón del curar potenciado, SF_HealChakraPU):
  curvas `KPSplineInterpolator` (`PSys/Rules/KeyPoints.cpp`). El setter de la propiedad (0x6AE170) copia los pares (t,
  valor) y fn_005B3760 calcula las segundas derivadas (el `spline()` de Numerical Recipes) con **pendiente 0 en los
  extremos**, porque el ctor de cada regla pone la marca del array (+8 = 1; sin ella 1e30, natural); `EvalAtT` 0x6A7EB0
  es `splint` (bisección, h²/6), sin recortar fuera de las claves. StretchHeight: stretch = curva(edad del átomo) entre
  StartTime y StopTime (ctor 0 y 5). MoveAtoms: posición = `GetCurrentParentPos` + (0, curva(edad), 0), × índice / (n −
  1) con `MovePropAtomIndex` (el índice 0 es el átomo más nuevo, la cabeza de la lista del original).
- `UR_OrientSpriteWithVelocity` 0x69A790 (las llamas de SF_FireBallInHand; `PSys/Rules/Orient.cpp`): la primera vez v =
  la velocidad y k = −10 ln(1 − SmoothFactor); cada paso v += (velocidad − v)(1 − e^(−k dt)); u = −v + (0,
  ProportionDefault, 0) en el marco de la cámara y `SetAngleY(atan2(−u_y, u_x) + π/2)` 0x674360. **(inferido)** la
  matriz 0xEA1D28 se toma como la rotación mundo → cámara (x = u · derecha, y = u · arriba).
- `UR_ForestPath` 0x6A3770 (`PSys/Rules/Forest.cpp`): con la edad de la colección, r = RadiusSpline y h = HeightSpline
  (las mismas curvas KP); por átomo tres ángulos al azar (2π) la primera vez, θ = fmod(edad · ThetaSpeed + θ0, 2π), φ
  igual con PhiSpeed; p = SphereRadius · r (· ScaleSphereRadius) · (cos θ cos φ SX, sin φ SY, sin θ cos φ SZ), +
  GetCurrentParentPos fuera de jerarquía, y + h en y; la velocidad = movimiento / dt.
- `ParticleGoodEvilCreator` 0x6AAA00 (DefineProperties 0x6B40C0, ctor 0x6AA990: `AlignmentSwitch` −0,5): el creador malo
  si el efecto tiene jugador con alineamiento (GPlayer +0x60 → +0x08) < AlignmentSwitch, si no el bueno
  (`Creator::Resolve`, PSys.h). Las mariposas siguen quietas hasta que la lane m4c porte `UR_Flocking`.

### Los pedazos (EXPLODE_OBJECT, `PSys/Rules/ExplodeObject.cpp`; E1 / E2 de la auditoría, fiel)

- **Quién los pide.** `fn_00681260(objeto, origen, velocidad, 6,0f, 0)` (0x67EC86; también `GScript::DeleteObject`
  0x6F93EF / 0x6F9460, que no se usan aquí) toma la matriz del mundo (vt 0x63C) y el objeto 3D (+0x40);
  `fn_006812B0(objeto 3D, matriz, origen, velocidad, 6,0f, marca)` toma su malla (vt 0xF8; sin malla no hace nada) y
  mete {malla, matriz (LHMatrix, 0x30), origen, velocidad, 6,0} (0x48 bytes) al final de la cola 0xD4E320, o de la
  0xD4E308 con marca ≠ 0. El 6,0 (un float, no un entero) no lo lee nadie. Los cuatro llamadores pasan marca 0: la cola
  0xD4E308 y `UR_ExplodeObject2` (0x681560 → fn_0067FFB0, RandomFactor +0x2C 0,3, MaxDepth +0x30 4) no hacen nada
  nunca (registrada, sin portar fn_0067FFB0). En openblack la matriz es la del `Transform` (posición, rotación × escala)
  y la malla la del componente `Mesh` leída de `AllMeshes.g3d` (`explode_object::PackMesh`): una malla que no es del
  paquete (p. ej. un edificio ya roto, `BuildingDamage`) no se rompe (se escribe en la traza).
- **Las cinco rocas** (0x67E79E..0x67E88E, al final de `InitCollection`): `LH3DObject::Create(0)` con `MeshPack[0x237]`
  (0 si el paquete es más corto); `SetPosition` 0x423140 en centro + (rand(−4, 4), 0, rand(−4, 4)) — el rand de **z**
  primero (0x67E7E3), luego el de x —, escala `PSysFloatRand(0,8, 1,2)` ([0x8C4A04] / [0x8C6C98] × [0x9357DC] = 1) y
  después el ángulo `PSysFloatRand(2π)`; filas de la matriz X = (cos, 0, sin) s, Y = (0, s, 0), Z = (−sin, 0, cos) s;
  `fn_006812B0(roca, su matriz, centro − 5 m (fn_0067E8C0), BlastSpeed, 6,0, 0)` y la roca se borra (vt 4): de las rocas
  solo existen los pedazos.
- **Quién vacía la cola.** `PSysGlobal::InitializeOneTimeOnly` 0x68F750 solo crea el singleton `PSysUtilityPSys`
  (0xD4E0E8). El efecto lo hace `fn_006718E0` cuando +0x10 es NULL: `PSysInterface::Create(NULL, 23, (0, 0, 0), (0, 0,
  0), 1,0, 0)`, desde fn_006717F0, que `PSysGlobal::GameLoopEnd` 0x68F5B0 → fn_006721B0 llama **una vez por turno** (paso
  11 de `GGame::ProcessTurn`, después de los hechizos): lo procesa (vt 0x100) y, si devuelve 5 (acabado: `MaxSpellAge`
  25 de `SF_ExplodeObject`), lo borra (fn_006718C0) y el turno siguiente se hace otro. `OnClearMap` 0x68F820 vacía las
  dos colas (fn_006811D0, fn_00681200). En openblack: `explode_object::GameLoopEnd` desde `magic::ProcessTurnEnd` y
  `Clear` desde `magic::OnLoadMap`.
- **`UR_ExplodeObject`** (vtable 0x938A98; ctor 0x6BECA0: +0x10 |= 6, una regla que crea; DefineProperties 0x6B0C70:
  `RandomFactor` +0x30 = 0,3 y `MaxTrigsPerFrag` +0x2C = 15; el archivo pone RandomFactor 0,889381 y no da
  MaxTrigsPerFrag). `ModifyAtomCollection` 0x6814E0 saca las entradas **desde la última** y llama a
  `ExplodeMesh(colección, NextGroups +0x20, malla, matriz, MaxTrigsPerFrag, origen, velocidad)`.
- **`ExplodeMesh` 0x6807B0** (corrige lo que se dijo antes: no son vértices sino **aristas**):
  - Solo las submallas con la marca 0x20000000 (el bit 0 de la máscara de LOD, bits 29..31) y **sin los bits de estado**
    0x3F0 (bits 4..9: andamios, lápidas). Cada primitiva por separado.
  - Las tres aristas de cada triángulo (fn_0067DF00: (t0, t1), (t1, t2), (t2, t0)); fn_0067DFD0 guarda primero el
    extremo de menor (z + y) + x (en el empate, el segundo) y la longitud² ((dz² + dy²) + dx²).
  - `qsort` (0x7C7E64, el de la CRT de MSVC 6) por longitud con un comparador que **nunca da 0** (0x682550: 1 si
    a ≥ b); de cada racha de aristas iguales seguidas (los seis floats, fn_00460640) se queda una.
  - Por arista única, la lista de triángulos que la tienen (búsqueda fn_00682580: `bsearch` con 0x682770, y si falla
    un recorrido entero por la primera igual).
  - Los pedazos: desde el primer triángulo sin usar, un **camino**: el triángulo entra (fn_0057D630), y el siguiente es
    el primer triángulo sin usar que comparte una de sus aristas (esquinas 0, 1, 2; la lista en su orden); sigue
    mientras encuentre y el pedazo tenga ≤ MaxTrigsPerFrag, así que un pedazo tiene **hasta 16** triángulos. Cuando el
    último no tiene vecino libre el pedazo acaba. El siguiente empieza en el primer triángulo libre desde el de ahora.
  - Cada pedazo es un átomo (`AtomCore::Create` + fn_00674DD0 con NextGroups) con un `RenderParticleGJMesh` (ctor
    0x6C8A90, +0x21 = 1) cuyo `GJMesh` (0x94 bytes, ctor 0x67FF20) tiene 3 vértices nuevos por triángulo (posición por
    la matriz, uv y la **normal tal cual, sin girar**) y el material de la primitiva (+0 = la primitiva).
  - Posición +0x80 = el centroide (suma × 1/n) en el mundo, vértices menos el centroide, rotación la identidad.
  - Velocidad +0x34 (0x680F34..0x6810AC): d = centroide − origen llevado a `velocidad`; r = `PSysRandR3` llevado a
    |d| × RandomFactor; v = (d.x + r.x, d.y + **r.y × 0,5**, d.z + r.z): el 0,5 ([0x8AA3B4]) solo va en la Y.
- **Después**, las reglas del archivo (ya portadas): gravedad 3,345 con suelo, giro (`AppearanceRuleTumble`), alfa
  255→0 de 0 a 3 s, escala 1→0 de 1 a 5 s, y fuera a los 6 s.
- **El dibujo, `RenderParticleGJMesh::DrawAt` 0x67C150**: color = el de la `DrawData` × la luz de la tierra bajo la
  posición dibujada (+0x21; fn_00801C90, byte a byte, alfa incluido; fn_00801C90 da dos salidas: en su 2.º argumento
  la tabla [luminosidad] interpolada, 0x8020F0, y en el 3.º los colores propios de las celdas, dword +0 | 0xFF000000,
  interpolados igual, 0x801F81; DrawAt 0x67C184 solo usa la primera, la otra va a [ebp − 0x40] y nadie la lee; el 255 ×
  255 >> 8 deja el alfa opaco en 254), igual para todos los vértices (el GJMesh no tiene
  colores: +0x24 ≠ vértices); luego la luz del modelo ([0xC029C0] = 1): la luz [0xEA9E90] por la inversa de la matriz
  dibujada, normalizada, I = fistp(255 n·l), f = I < 0 ? 90 : 90 + ((255 − 90) I >> 8), RGB × f >> 8; la segunda luz
  [0xD4EC08] es 0. Con alfa de la `DrawData` ≠ 255 se dibuja con la tabla de modos con alfa (0xC387C8).
  `Draw3DWorldTriangle` 0x81C090 con el material de la primitiva: **sin bruma ni especular**.
  - En openblack cada pedazo es una malla generada (`L3DSubMesh::LoadGenerated`, con las texturas propias de la malla de
    origen: `L3DMesh::SetSkinSource`) dibujada como átomo de malla (`mesh_atoms::Collect`, `PSys/Creators/Mesh.cpp`):
    la matriz PSR, el color ya multiplicado por la luz de la tierra en la CPU (`explode_object::LandLight`, la
    interpolación entera de fn_00801C90 0x801CB8..0x8020F7: celdas ftol(x × 0,1), pesos ftol(frac × 256), por canal
    a + ((b − a) w >> 8), fuera del mapa o sin bloque table[255]) en la tercera columna, que es la rama de `vs_object`
    sin bruma ni especular con la luz del modelo encima; translúcido cuando el alfa ≠ 255. La malla se borra con el
    átomo.
  - **(aproximado)** la luz de la tierra de la CPU no lleva el tope de las sombras de las nubes que el port aplica en la
    GPU a los demás objetos; tampoco la ruta alternativa [0xEA9EB4] → fn_007ACD90 (no leída). Las operaciones en coma
    flotante son de 32 bits (no se imita la x87). En la última fila / columna del mapa el original lee la celda 17 del
    bloque (+0x88 / +0x90); el port toma la última celda.
  - **(aproximado)** como los demás átomos de malla, los translúcidos van con las mallas que se desvanecen y no dentro
    del objeto Z del efecto.
- Capturas (`dev\_audit\magic\`, `OPENBLACK_TEST_MAGIC_TURN=300 OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,1825,2632"
  OPENBLACK_CAMERA_FLY="1790,58,2592,1825,30,2632"`, `--mod game.skip-intro=off`, `-n 14000`):
  `polish_fix_beam2_house_t6.png` (los pedazos del almacén y de las rocas saltando), `_t14.png` (los 490 pedazos del
  árbol 952, hojas con su textura), `_t40.png` (ya desvanecidos: alfa a 0 a los 3 s), `_t80.png` (el después: el
  almacén sigue en pie, E4); traza en `polish_fix_beam2_house_end.log` (`ExplodeObject: mesh ... in N pieces`: 70 el
  almacén, 20 cada roca, 490 el árbol).

### Sin portar / pendiente

- `UR_ExplodeObject2` → fn_0067FFB0 (no la llama nadie: la cola 0xD4E308 queda vacía).
- E5 (el aldeano con `life::Kill`, **(aproximado)**, lane mapas) y E7 (la marca +0x25 & 0x40 y los objetos de guion,
  **(inferido)**): ver `DestroyedByBeam` y `CanBeDestroyedBySpell` arriba; E6, la ciudadela, abajo.
- El edificio «destruido»: ver «Qué le pasa al edificio en el original» arriba (falta la obra de la ciudad, lane
  mapas).
- `GetActualObjectToEffect` de la ciudadela (CitadelHeart 0x468C30, CitadelPart 0x469780).
- `ER_EmitFromParentAtom` y `CreateRule_GameObjectRef` (SF_SparklesFromObject, SF_ButterfliesOnObject, criaturas): no
  los usa ningún milagro del jugador.

### Ganchos, pruebas y capturas

- `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` lanza la explosión (`EXPLOSION_ONE_PU_ONE` / `_PU_TWO` para Many /
  Loads); `OPENBLACK_TEST_EXPLOSION_SHOT="<turnos>,<ruta.png>[;...]"` pide capturas esos turnos después de empezar la
  primera explosión ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)). Con
  `OPENBLACK_SPELL_TRACE=1`: `Explosion: started ...` con sus objetivos (distancia, radio, clase), `Explosion: ground
  mark ...` con su ángulo, cada objeto destruido (anillo, explotados, borrados), las rocas en cola y cada malla rota
  (`ExplodeObject: mesh ... in N pieces, from ... at ...`).
- `test_explosion`: ChangeScaleXYZ, MoveAtom, la cadencia de los eventos 2 y el cierre de un archivo como Single, el
  tinte del jugador, FaceCamera, las curvas KP, el creador bueno / malo y el polvo `SmokyStuff` de
  modo 1 a 1,5 × tamaño (la vida y el alfa de la marca los prueba `ground_marks`); los pedazos: el camino por aristas
  (16 + 4 en una tira de 20, trozos sueltos, MaxTrigsPerFrag 0), la velocidad (la Y del azar a la mitad) y la cola
  vaciada en átomos en el centroide (solo LOD 0 sin estado).
- Capturas del cráter (`polish_fix_beam_open_t10/_t40/_t110/_t142.png` + `polish_fix_beam_open_end.log`): BEAM_EXPLOSION
  en (1850, 2612), en la ladera junto al almacén de Land 1, cámara `1822,55,2585,1850,28,2612`.
  - t10: la columna dentro del hoyo.
  - t40: el hoyo oscuro amoldado a la ladera, con el humo.
  - t110: el hoyo solo, 10 s después.
  - t142: desvaneciéndose en su último segundo.
  - A 150 turnos ya no está.
- Capturas en `dev\_audit\magic\`: `m6b_beam3_t6/_t9/_t20.png` (BEAM_EXPLOSION junto al almacén de Land1: la columna
  con los conos, el resplandor del suelo y, al final, los tres árboles que ya no están; `m6b_beam3.log`: los tres árboles de la
  lista destruidos con el anillo a 6, 14 y 16 m (en otra pasada también un aldeano a 12 m) y el evento 2 por turno con
  burn 200); `m6b_beampu2_t12/_t40/_t80.png`
  (BEAM_EXPLOSION_PU2 = SF_BeamExplosionLoads en el pueblo: decenas de columnas repartidas por el
  `SpreadingDiskEmitter`, humo y una casa ardiendo; 39 objetos destruidos); `m6b_dome4_t30.png` (la cúpula SHIELD de
  radio 40 sobre el almacén, lanzada por el jugador neutral para que la fuerza no baje: ahora se ve como una burbuja
  aditiva clara, frente al parche apenas visible de `m6s_shield_dome.png`); `m6b_forest_t80/_t110.png` (las mariposas del bosque como mallas quietas en
  sus cinco grupos, moviéndose con `UR_ForestPath`); `m6b_healpu.png` (HEAL_PU_ONE: el champiñón de las curvas KP sobre
  el almacén); `m6b_fire_hand.png` (las llamas de la bola de fuego en la mano con `UR_OrientSpriteWithVelocity`).

## Milagros de la criatura (M8, pendiente)

Las 16 filas de criatura de info.dat son los MAGIC_TYPE 26..41 (el último bloque de
[Tablas de info.dat](magic.md#tablas-de-infodat-m0-srcmagicmagictables)). Su clase de hechizo **no está portada**: corren como un `Spell` simple. Lo que ya
se sabe y espera a M8:

- Reglas de lanzamiento de criatura: vt 0x30 0x5FA7E0 = 0 y vt 0x2C 0x5FA7F0 ([Reglas de lanzamiento](magic.md#reglas-de-lanzamiento-magiccastrules));
  `IsCreatureCastFromAbove` 0x5FB7E0.
- Los deseos de la criatura al lanzar (fn_00721730, al principio de `Spell::InitWithPos` 0x71FE50) y el robo de
  hechizos por la criatura (culto).
- El mimetismo (`ConsiderMakingCreatureMimicPlayer`, en agua y bosque) y la criatura en la explosión, el rayo y el
  tornado (`Creature` 0x47B1E0 `CanBeDestroyedBySpell` = 0, `creature+0x12B0`, fn_00477060).

## Pendiente

Lo que falta de cada milagro, en su sección:

- Agua: [Sin portar / pendiente (agua)](#sin-portar--pendiente-agua)
- Curar: [Sin portar / sin verificar (curar)](#sin-portar--sin-verificar-curar)
- Bandadas: [Pendiente / no fiel](#pendiente--no-fiel)
- Escudos: [Sin portar / UNVERIFIED](#sin-portar--unverified)
- Tormenta, tormenta eléctrica y tornado: [Inferido, aproximado y pendiente (tormenta)](#inferido-aproximado-y-pendiente-tormenta)
- Explosión de rayo y clases de PSys que faltaban: [Sin portar / pendiente](#sin-portar--pendiente)
- Comida y madera: la voz `GGuidance::ResourceDropSFX` 0x71B570 (falta el canal de guía y los campos del pueblo), el
  desvío de la madera de un almacén en obras (`StoragePit` +0x74 = `BuildingSite`, inalcanzable sin obras) y el signo de
  la inclinación de `HandStateGrain`, que está al revés en `HandPlacement.cpp` (lane de la mano). Los vt 0x78/0x80 de
  `MagicFood::CallVirtualFunctionsForCreation` ya están identificados y se cumplen
  ([Comida y madera](miracles.md#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)).
- Bosque: la diosa y la cámara (aplazado; el aleteo de las mariposas y los murciélagos ya está, U7)
  ([Bosque](miracles.md#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)).
- Rayo: `LightningForkFlicker` 0x6B24D0, `NumTexturesToTile`, el árbol de horquillas recursivo, `DrawOffsetLT` y los EffectValues del rayo sin hechizo ([Rayo](miracles.md#rayo-magic_type-4-6-semilla-6-lightning_bolt-psysruleslightningcpp)).
- Teletransporte: la iluminación de la piscina (`UseLighting` 1, `MaterialSetDoubleSided` 0 de `SF_TeleportVortex`) y la criatura ([Teletransporte](miracles.md#teletransporte-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)).
- [Milagros de la criatura](miracles.md#milagros-de-la-criatura-m8-pendiente).

## Ganchos de prueba

Todos los `OPENBLACK_*` están en [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración).
Por milagro:

- Agua: [Pruebas y capturas](#pruebas-y-capturas)
- Curar: [Ganchos, pruebas y capturas](#ganchos-pruebas-y-capturas)
- Bandadas: [Ganchos, pruebas y capturas](#ganchos-pruebas-y-capturas-1)
- Bola de fuego y rayo: [Pruebas y capturas](#pruebas-y-capturas-1)
- Escudos: [Ganchos y capturas](#ganchos-y-capturas)
- Teletransporte: [Pruebas y capturas](#pruebas-y-capturas-2)
- Tormenta, tormenta eléctrica y tornado: [Ganchos, pruebas y capturas (tormenta)](#ganchos-pruebas-y-capturas-tormenta)
- Explosión de rayo y clases de PSys que faltaban: [Ganchos, pruebas y capturas](#ganchos-pruebas-y-capturas-2)
- Comida y madera: la viñeta «Pruebas» de [Comida y madera](miracles.md#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource).
- Bosque: las viñetas «Ganchos» y «Pruebas» de [Bosque](miracles.md#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees).

## Fuentes

- `dev\tmp_dis\miracles\`: `PLAN.md`, `resources.md` (comida, madera, agua, bosque, curar, bandadas),
  `destructive.md` (fuego, rayo, tormenta), `protect_creature.md` (escudos, teletransporte), `visuals_sound.md`,
  `psys\part_render.md`, y el desensamblado propio de cada lane en `impl\` (`m4a`, `m4b`, `m4c`, `m6b`, `m6s`, `m6st`, `m6t`).
- `dev\_audit\magic\`: las capturas y registros citados.
