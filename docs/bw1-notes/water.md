# El agua en el juego

Todo lo del agua que no es el dibujo del mar: qué celda es agua, las consultas de agua y la máscara `LandAvoid`, el
agua en los guiones, los golpes y las caídas al agua, hundirse y ahogarse, los anillos, los tiburones, el puzle de los
peces, el barco de los misioneros, el decorado fijo (cascada, arca) y qué suena en el agua. El mar, la costa y los
ríos se dibujan en [rendering.md](rendering.md); los reflejos y los cortes bajo el agua, en
[rendering-objects.md](rendering-objects.md).

- [Celdas de agua (SeaCells)](#celdas-de-agua-seacells)
- [Consultas de agua](#consultas-de-agua-gutils-gstream-abode)
- [Máscara `LandAvoid` de la criatura](#máscara-landavoid-de-la-criatura)
- [El agua en los guiones (CHL)](#el-agua-en-los-guiones-chl)
- [Golpes y objetos que caen al agua](#golpes-y-objetos-que-caen-al-agua)
- [Hundirse, ahogarse y borrarse](#hundirse-ahogarse-y-borrarse)
- [Anillos de agua](#anillos-de-agua)
- [Tiburones (clase `Whale`)](#tiburones-clase-whale)
- [Puzle de los peces](#puzle-de-los-peces)
- [Barco de los misioneros (PetitNavire)](#barco-de-los-misioneros-petitnavire)
- [Decorado fijo: cascada de Land 3, arca y dinosaurio de Land 4](#decorado-fijo-por-tierra-cascada-de-land-3-arca-y-dinosaurio-de-land-4)
- [Audio del agua](#audio-del-agua)
- [El agua en otras páginas](#el-agua-en-otras-páginas)
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

## Celdas de agua (SeaCells)

Toda la tierra/agua del original se decide con cinco predicados sobre la **celda de terreno** (8 bytes; byte +4 =
altitud cruda, byte +6 = propiedades del LND: 0x10 `hasWater`, 0x20 `coastLine`). Antes cada módulo tenía su regla
(banderas, altura ≤ 0, borde recortado); ahora hay un solo módulo con la tabla exacta, **incluido el borde del mapa**:

| Función de openblack | Original | Regla | Sin celda (fuera del mapa o sin bloque) |
|---|---|---|---|
| `sea_cells::IsWater` | `MapCoords::IsWater` 0x6035B0 | `propiedades & 0x10` | **agua** (1) |
| `sea_cells::IsLand` | `MapCoords::IsLand` 0x603720 | `!(propiedades & 0x10)` | 0 |
| `sea_cells::IsDryLand` | `MapCoords::IsDryLand` 0x603620 | **altitud ≥ 4**, no mira las banderas | 0 |
| `sea_cells::IsCoastal` | `MapCoords::IsCoastal` 0x6036A0 | `!(propiedades & 0x10) && (propiedades & 0x20)` | 0 |
| `sea_cells::InBounds` | `MapCoords::InBounds` 0x6042C0 | la celda cae dentro del mapa de juego (comparación sin signo) | — |
| `sea_cells::CollideLandscape` | parte de terreno de `MapCell::Collide` 0x601BD0 | 0x10 fuera del mapa (fn_00601E00), si no 1 agua / 2 tierra | 0x10 / 1 |
| `sea_cells::GetSurfaceType` | `GSoundMap::GetSurfaceType` 0x71D8E0 | 6 sin celda, 7 si `!IsLand`, si no el `surfaceSound` del material (3 si no es 1..8) | 6 |

- La celda de un punto es `MapCoords` = `ftol(mundo·6553,6) >> 16` (10 unidades por celda, truncado; una x negativa
  sale del mapa por el word alto sin signo, no se recorta a 0). El golpe contra el agua y la regla de soltar usan
  además la celda **redondeada al más cercano** (`fistp` en el original, `std::lrint`), media celda de diferencia.
- Los bits 0x04 (campo), 0x08 (fijo) y 0x20 (árbol) de `MapCell::Collide` salen de los objetos de la celda del mapa,
  que openblack todavía no lista: `CollideLandscape` solo da la parte de terreno.
- Consumidores hoy: `CollisionSounds` (golpe contra el agua), `AnimationSounds` (superficie de los clips),
  `HandSystem::IsLand` (y con él los peces, los árboles y las vasijas de la mano), `HandHolding` (soltar suave),
  `ecs::pot_resource::IsWater` (recursos perdidos en el mar), `FishShoals::IsOkToCreateFishFarmAt`, `WaterQueries` y `LandAvoid`.
- **Piscifactorías**: `GFishFarmInfo::IsOkToCreateAtPos` 0x52D100 = `IsCoastal` **y** que no haya ya una granja en esa
  celda (`MapCoords::FindType(0x21)` 0x6045C0). Ni pueblo, ni profundidad, ni distancia. Hoy solo el guion crea
  granjas, así que la regla está portada (`IsOkToCreateFishFarmAt`) pero sin usar.
- Prueba unitaria: `test/test_sea_cells.cpp` (isla de dos bloques hecha a mano + `Land1.lnd` real si está instalado:
  mar abierto 1464,2016; orilla de altitud 1 en 1485,2015; tierra seca de altitud 44 en 1788,4/2710).

## Consultas de agua (`GUtils`, `GStream`, `Abode`)

Las posiciones del original son `MapCoords`: x y z en 16.16 de celda (mundo × 6553,6 truncado, 10 unidades de mundo
por celda) e y = altura **sobre el suelo**, no altitud del mundo. El puerto mantiene ese detalle dentro del módulo y
expone `glm::vec3` en unidades de mundo.

| Función | Dirección | Qué hace |
|---|---|---|
| `GUtils::GetDistanceInMetres` | 0x74CD70 | distancia xz de dos `MapCoords`: `hypotenuse(int,int)` 0x74F680 (16.16) × 10/65536 |
| `hypotenuse` / raíz inversa | 0x74F680 / 0x74F620 | raíz inversa por tabla de 1024 mantisas (`crt_xc_fn_atexitCleanupReg_Utils_0074F580`): ~10 bits, error ≈ 0,1 % |
| `GUtils::FindNearestCoastalTo` | 0x74E2E0 | espiral cuadrada de celdas desde el punto hasta encontrar una celda **dentro del mapa de juego** y `MapCoords::IsCoastal`, o hasta que la espiral se aleja más que el radio (máx. 999999 pasos) |
| `GUtils::Spiral` | 0x74D7E0 | el paso de la espiral: tabla 0xDA59FC = (+1,0), (0,+1), (−1,0), (0,−1), con `dir/2` pasos por tramo; suma **al word alto**, así que la fracción de celda del punto de partida no se pierde (`operator+=` 0x605470) |
| `GStream::FindNearestPosTo` | 0x733D30 | el punto de río (`CREATE_STREAM_POINT`) más cercano en xz, **estrictamente** más cerca que el radio; la y del resultado es la altitud del punto menos el suelo allí |
| `GUtils::FindNearestDrinkingWater` | 0x74E3A0 | primero el río; si lo hay, busca además una costa que no esté más lejos **del punto de partida** que ese río (si la encuentra, gana la costa) y devuelve "encontrado" en cualquier caso; sin río, la costa dentro del radio |
| `Abode::FindNearestDrinkingWater` | 0x407020 | lo anterior desde la casa a su caché: bit 0 de +0x7C = encontrado, +0x80 = la posición (**solo cambia si encuentra algo**) |
| `Abode::GetNearestWaterPos` | 0x405FC0 | lee esa caché (falso si el bit está a 0) |

Radios del original: **200** al crear la casa (`Abode::Abode` 0x4013E0), **400** el pastor cuando la casa aún no tiene
agua (`Villager::ShepherdMoveFlockToWater` 0x768CC0), **500** el tigre (`Tiger::CalculeLairPos` 0x4214DD).

Detalles que importan:

- La espiral **para en cuanto se aleja del radio**, no recorre el cuadrado completo: por eso no siempre devuelve la
  costa estrictamente más cercana, sino la primera de la espiral.
- `IsCoastal` (0x6036A0) es **tierra** en la línea de costa (`!hasWater && coastLine`), no agua: el agua potable es una
  celda de tierra junto al mar, y el bebedor se acerca a ella. Los predicados están en `ECS/SeaCells`.
- `Tiger::CalculeLairPos` (0x421470) llama a `FindNearestDrinkingWater(pos, 500)` **para cada bosque** pero no usa el
  resultado para colocar la guarida: la respuesta solo fija la distancia que compara con la puntuación del bosque
  (`fn_0053AD00`, `SigmoidThreshold` de árboles y distancia), y esa distancia no se lee después. La guarida sigue
  siendo el bosque mejor puntuado, así que `ECS/AnimalPredators.cpp` no cambia (solo lo anota).

## Máscara `LandAvoid` de la criatura

`LandAvoid` (0xD559B0) son 512×512 bytes `[z][x]`, uno por celda del terreno, que se construyen **una vez por paisaje**
en `GLandscape::Open` (0x5E5541) con `ValidateLandAvoid` (0x6E7BA0) y `FloodAnalyse` (0x6E7FA0, `RoutePlan.cpp`):

1. Para cada celda, las altitudes de sus **cuatro esquinas** (`(x,z)`, `(x,z+1)`, `(x+1,z)`, `(x+1,z+1)`; 0 fuera del
   mapa o sin bloque) × 0,67: si `max − min > 10` → **1** (demasiado inclinada); si las cuatro son 0 → **1** (mar
   profundo); si no → **4** (candidata andable).
2. La semilla del flood es la última celda 4 de la última tirada de celdas 4 que termina en un 1 (por filas: el
   contador se reinicia en cada fila; una tirada que llega al final de la fila no cuenta). En Land1 sale **(191, 362)**.
3. `FloodAnalyse` inunda los 4 vecinos ((−1,0), (0,−1), (+1,0), (0,+1)): las 4 alcanzadas pasan a **0** y las 1
   vecinas del flood a 5. Al final 1 → **2**, 4 → **2** (no alcanzables) y 5 → **1**.
4. Las celdas 0 cuyo terreno tiene el bit de agua (o no tiene bloque) pasan a **6**: agua por la que se puede andar.

Valores finales: **0** tierra alcanzable, **6** agua andable, **1** celda a evitar pegada a la zona alcanzable, **2**
inalcanzable. `fn_00483890(pos, r)` (r = 7,1 en `fn_00483850`; 7,05 en `fn_00483870`, giro) acepta la posición si su
celda es 0 o 6 y ningún centro de celda `(10i+5, 10j+5)` del 3×3 vecino que no sea 0/6 queda a menos de r.

La criatura, por tanto, **vadea** hasta donde las cuatro esquinas de la celda tienen altitud 0 y no nada (en
`ctrspec27.txt` no hay animación de nadar). Los lagos de Land1 (altitud 1 con agua) también salen **6**: se puede
entrar en ellos.

En openblack: `land_avoid::Validate(island)` se llama al cargar el paisaje (`Game::LoadLandscape`), `At(x, z)` da el
valor y `IsPosValid(pos, radio)` es `fn_00483890`. Recuento en Land1: 12 949 celdas 0, **1866** celdas 6 (el anillo de
agua somera y los lagos), 3671 celdas 1 y 243 658 celdas 2.

**Gancho** `OPENBLACK_DUMP_LAND_AVOID=1` (o `=<fichero>.png`) vuelca la máscara al cargar el paisaje: un píxel por
celda, x a la derecha y z hacia abajo, **verde** 0, **azul** 6, **rojo** 1, **gris** 2 (magenta = valor imposible), y
escribe en el log la semilla y los recuentos. Comprobado con los puntos de Land1: `(146,201)` mar abierto → rojo (mar
profundo junto a la zona alcanzable), `(147-148,201)` costa somera → azul, `(149,201)` y `(178,271)` tierra → verde,
los lagos `(213,241)` y `(216,309)` → azul.

## El agua en los guiones (CHL)

- **`GET_LAND_HEIGHT`** (`GScript::GetLandHeight` 0x6FB1F0): celda `(int)(x·0,1)`, `(int)(z·0,1)` (truncado hacia 0, así
  que −10 < x < 0 sigue siendo la celda 0); fuera de 0..511, sin bloque o **altitud 0** devuelve **−10,0**
  (0xC1200000, "está en el mar"); si no, la altura interpolada (`LH3DIsland::GetAltitude`). openblack devolvía siempre
  la altura: ahora `sea_cells::ScriptLandHeight` hace la regla completa (`CHLApi.cpp`, `GET_LAND_HEIGHT` 151). Lo usan
  los retos `Baywatch` (Land2) y `LostBrother` (Land1). Comprobado: mar abierto de Land1 (1464, 2016) → −10, tierra seca
  (1788,4; 2710) → 28,917.
- **`GET_PROPERTY`** (`GScript::GetProperty` 0x70DAE0, tabla de saltos 0x70E78C sobre `propiedad − 1`): de momento están
  las dos del agua, las demás siguen sin implementar.
  - `FLYING` (5, 0x70DCF8) = bit 6 (0x40) de `Object+0x24` = **el objeto tiene un `PhysicsObject`** (también los
    obstáculos en reposo, igual que `Object::IsActuallyInTheAir` 0x639410).
  - `DROWNING` (6, 0x70DD0A) = la virtual `IsDrowning` (vt +0x17C): `Villager` 0x756B30 = estado **16** DROWNING;
    `Object` 0x63A780 = tiene `PhysicsObject` **y** el centro de masas (po +0xCC) está por debajo de y = 0;
    `GameThingWithPos` 0x4052D0 = 0. En openblack es `ecs::IsDrowning` (`ECS/VillagerDrowning`). La usan los retos
    `CreatureSavingPeopleDrowningMan` y `PiperSetFree` (Land1) y `ThrowBlokeMain` / `EndOfFlyingNutter` (Land3).
  - Con una cosa que ya no existe el original escribe "Thing no longer valid" y apila un **float 0**; openblack hace lo
    mismo (sale a menudo en el log porque los guiones referencian objetos que openblack aún no crea).
- **`SET_PROPERTY`** (0x70F380 → fn_0070E820, tabla 0x70F2BC): `FLYING`, `DROWNING` y `MOVING` van al caso de error
  ("Cannot Set Property %d") y **no cambian nada**; portado así.
- **Enum arreglado**: `ObjectPropertyType` de `src/ScriptHeaders/ScriptEnums.h` juntaba dos entradas en
  `InHandGrabypeSpeed`, así que desde `IN_HAND_GRAB` (10) todo iba desplazado un valor (`SPEED` 11 … `BUILT_PERCENTAGE`
  22, `ZPOS` 25). Ahora son `InHandGrab` y `Speed` como en `bw1-decomp include/chlasm/ScriptEnums.h`. Nadie usaba los
  números antes (comprobado con grep), así que el arreglo no rompe nada.

## Golpes y objetos que caen al agua

- **Golpe contra el agua** (`AttemptToAddSoundEvent` 0x6465B7): `!IsDryLand` (altitud < 4) → **anillo** en (x; 0,1; z)
  de crecimiento 2R, ritmo 1/R, celda **0x3F**, 0xFFFFFFFF; además, si en la celda redondeada no hay celda o la
  altitud es < 3 → tipo de colisión WATER, polvo de espuma 0x28C8F0F4 y `fn_0074F2D0` (que **solo** pone la bandera
  global de chapoteo que asusta a los peces: no hay efecto de fichero de hechizo, las 6 "partículas" de espuma son el
  polvo normal de `fn_00845C20`). Altitud 3 = orilla: anillo + polvo marrón con el sonido del suelo.
  - Las 6 partículas (0x646776..0x646854; openblack `ECS/Physics/Dust`): en (x, `GetAltitude`, z), velocidad
    (rand(201) − 100)·0,02 por eje, tamaño min(2R, 5), tipo 4 de las "liquid particles" (`fn_00845D30`, cupo 0x400;
    `fn_00846010`: tipo 4 = 1 s sin gravedad, tipo 0 = 3 s, otros 2 s; se quitan cuando la edad **pasa** de la vida,
    antes de moverse). El color pasa antes por `fn_004ED180`: k = clamp(ftol(nieve en el punto), 0, 255) con la
    rejilla `SnowCover` [0xEDC344] (128×128, 40 unidades por celda, bilineal, `fn_0086CA80`) y cada canal
    c += floor((base − c)·k/256) hacia el color base de la luz [0xFA26A4], alfa igual. Sin nieve k = 0 y el color no
    cambia; aquí no hay `SnowCover` (clima de openblack-magic), así que no se aplica.
- **Onda al cabecear en el agua** (0x645A5E): mismo anillo pero **celda 0x30** (la del chapoteo de la mano), no 0x3F.
- **Soltar sobre el agua** (`Object::InitialisePhysicsFromHand` 0x636F00, portado entero; el algoritmo está en
  [physics.md](physics.md#el-agua-en-los-golpes-y-al-soltar)): solo "aterriza" con `IsDryLand` o con la altitud de la
  celda redondeada (fistp) > 1, así que sobre el mar el objeto se queda en física y flota o se hunde
  ([abajo](#hundirse-ahogarse-y-borrarse)); una vasija de la mano soltada despacio pierde su recurso (siguiente punto).
  La flotación (densidad que sube 6,67e-5 por subpaso, arrastre ×100, borrado por debajo de −4R) está en
  [physics.md](physics.md#motor-physob-0x7fb7300x7fe7b0). **fiel**
- **Recursos que caen al mar** (`Pot::AddResourceToPos` 0x66F270): fuera del mapa no hace nada, y lo que sobra tras
  fundirse con montones/almacenes **se pierde** si la celda es de agua (0x66F42D): ni montón nuevo ni sonido.
  openblack: una sola regla, `ecs::pot_resource::AddResourceToPos` (`ECS/PotResource.cpp`, exacta a 0x66F270, ver
  [magic.md](magic.md)); la mano (`HandResources.cpp`) ya no tiene regla propia y `pot_resource::IsWater` llama a
  `ecs::sea_cells::IsWater` (única copia de `MapCoords::IsWater` 0x6035B0).

## Hundirse, ahogarse y borrarse

Código: `src/ECS/VillagerDrowning.{h,cpp}` y `src/ECS/ToBeDeleted.{h,cpp}`. **Fiel** salvo lo que se marca
pendiente (la muerte completa del aldeano, ver [Pendiente](#pendiente)).

- **`HasSunk`** (vt +0x7B8), preguntado en cada subpaso desde 0x645A01 cuando el cuerpo está despierto, su centro está
  por debajo de `R/2` y su **densidad > 1**; si dice que sí, el cuerpo se para (v = 0, L = 0) y se ejecuta `EndPhysics`
  como si hubiera quedado en reposo (código 2):
  - `Object::HasSunk` 0x637470 → **no**: rocas, árboles, vasijas, montones y trozos siguen bajando hasta `T.y < −4R`
    (código 4) y ahí se **borran** con el `ToBeDeleted(0)` de su clase. Tiempos medidos: roca al momento, aldeano ~4
    turnos, vasija de ofrenda ~64, animal ~75, objeto normal ~150, árbol ~194, vasija ~298, balón ~525.
    Mientras bajan se dibujan enteros con su Draw normal, después de la tierra: la parte bajo y = 0 queda tapada por la
    Z de las celdas dibujadas (que la escriben aunque sean transparentes) y se ve sobre las celdas de mar abierto 0x02
    (que no se dibujan). Un árbol que se hunde junto a esas celdas sale **cortado en rectángulos**, también en el
    original ([rendering.md](rendering.md#costa)); el usuario lo recuerda así (2026-10-01): un árbol o una roca
    lanzados al mar se veían cortados.
  - `Living::HasSunk` 0x5ED370 (animales) → `SetDying`, estado LIVING_DEAD 15 y `ToBeDeleted(0)`: el animal desaparece.
  - `Villager::HasSunk` 0x750AB0 → `stateCounter = GVillagerInfo::drowningTime` (**600** turnos = 60 s) y estado
    **DROWNING (16)**. (Si el aldeano ya estaba muerto: estado DYING 14 con `dyingTimeWithoutGraveyard`, rama que
    openblack no alcanza porque un aldeano sin vida se quita al momento.)
  - Falta en los dos `Living`: avisar a la criatura para que aprenda del jugador que lo soltó
    (`ConsiderMakingCreatureMimicPlayer`, `DETECTED_PLAYER_ACTION_THROW_IN_THE_SEA` 0x15) — depende de la criatura.
- **`Villager::EndPhysics` 0x5F0A60, rama del agua** (0x5F0BAF): todo aldeano que **acaba la física en una celda con el
  bit de agua** se ahoga, también en la orilla somera; no hay LANDED. Con vida > 0 → DROWNING con `drowningTime`
  (y `lastPlayerToInteract` = quien lo lanzó, pendiente); si no, `VillagerDead(6 PLAYER_INTERACTION_DROWN)`. Se llega
  aquí por `HasSunk` (lo normal en el mar) o al pararse con contactos en una celda de agua de altitud ≥ 2; en Land1 **no
  hay ninguna celda de agua con altitud > 1** (comprobado recorriendo el mapa en `test_sea_cells`), así que en esa isla
  siempre se llega por `HasSunk`. openblack usaba `po.body.inWater` (la física) en vez de `IsWater(Pos)` (la celda).
- **Estado DROWNING (16)**, `Villager::Drowning` 0x76A780, una vez por turno: `--stateCounter` y, a 0, muerte con
  motivo 6. Clip por defecto del estado **252 `P_DROWNING`** (bucle de 2233 ms) y sus eventos de sonido: 30 ms y 1590 ms
  chapoteo de nadar 157 (`editor.sad` 559-562), 257 ms voz de ahogarse 134 (hombre 563-570, mujer 571-578, el niño no
  grita) — ya suenan porque `AnimationSounds` resuelve la superficie 7 (agua) del clip.
  `EnterDrowning` 0x767410 / `ExitDrowning` 0x767420 devuelven 1 (no hacen nada).
  - **Corre cada turno**: `GGame::ProcessTurn` → `Living::ProcessLiving` 0x5EC810 → `ProcessState` (vt +0x620,
    `Villager::ProcessState` 0x74FF70) → `CallState` 0x7521D0. `GVillagerInfo::processChecksEvery` (+0x2DC) solo
    espacia el bloque periódico de `CheckEveryTime` (0x750518: vejez, hambre…), no la función de estado.
  - **Bit 0x4000 de `Flags` +0x24 = INDESTRUCTIBLE**: lo ponen y quitan `SET_INDESTRUCTABLE` (`GScript::
    SetIndestructable` 0x6FDE20, objetos que no son contenedores de guion), los puzles (PuzzleGame, HanoiBlock,
    PuzzlePig) y `GameOSFile::LoadInstance`. `Drowning` 0x76A783 fija el contador en 10 antes de restarle: **un
    aldeano indestructible no se ahoga nunca**. openblack: componente `Indestructible` y `SET_INDESTRUCTABLE` en
    `CHLApi.cpp`.
  - **`lastPlayerToInteract` (+0x104)** = `PhysicsObject::GetPlayer` 0x647460 (el jugador del `GInterfaceStatus` de
    po+0x24: la mano que lo soltó o lanzó, heredado por lo que golpea; 0 sin físicas), puesto en la rama del agua de
    `EndPhysics`. Solo lo lee el `VillagerDead` de `Drowning` (el `GetPlayerWhoLastDroppedMe` del aldeano es el de
    `GameThing`, que devuelve NULL, 0x4018B0). openblack aún no tiene jugadores donde guardarlo.
  - **Rescatar con la mano**: cogerlo lo pone IN_HAND (la función de ahogarse deja de correr) y soltarlo en tierra seca
    lo saca de la física en el acto → `Villager::EndPhysics` sin `IsWater` → LANDED. Tras LANDED solo vuelve al
    estado anterior (+0x8E) si tiene la bandera 0x400 (controlado por guion) o si ese estado tiene +0x104 en su
    `GVillagerStateTableInfo` (fichero +0xF4): solo `InScript` (4) e `In Script Dance` (5); DROWNING no. Queda
    rescatado.
- **Clips de morir en el agua**: `DyingAnimation` 0x423770 → **283 `P_INTO_DEAD_DROWNED`** si `IsWater(Pos)`, y
  `DeadAnimation` 0x4237A0 → **249 `P_DEAD_DROWNED`** (`VillagerAnimations`); en tierra 253 y 243.
- **`ToBeDeleted`** común (`ECS/ToBeDeleted`): la limpieza de cada clase antes de quitar la entidad — aldeano
  (`Villager::DeleteDependancys` 0x74FD60: casa y lista de sin techo), animal (`Animal::DeleteDependancys` 0x417BA0:
  la IA lo olvida), y luego fuera de la física y del registro. Lo usan el borrado a −4R (antes `registry.Destroy`
  directo), el hundimiento de animales y la muerte del ahogado.
- **Guiones**: `GET_PROPERTY(DROWNING)` (y `FLYING`) ya responden con `ecs::IsDrowning`; ver
  [El agua en los guiones](#el-agua-en-los-guiones-chl).

## Anillos de agua

- **Fiel** (`fn_005E5100`, tras la tierra y antes de los modelos): por anillo, edad += (int)(ms de
  juego · ritmo), fuera a 700; media anchura max(edad·crecimiento/700, 0,0001) (z × aspecto, +0x28); alfa (int)((255 − 0,364286·
  (edad % 700))·A) >> 8, RGB del color; giro en Y, celda & 63 de la hoja 8×8 de `smoke.raw`/`smokea.raw`, modo 13
  (SRCALPHA/ONE, sin Z); deriva con el viento si +0x1C. Chapoteo de la mano: (x, 0,2, z), crecimiento 7, ángulo al azar,
  celda 0x30, 0xB0 + tabla de luz[255]. Objeto físico en el agua (0x6466D2): (x, 0,1, z), crecimiento 2·radio, ritmo
  1/radio, celda 0x3F, blanco. openblack: `ecs/WaterRings`, `Renderer::DrawWaterRings`; gancho `OPENBLACK_TEST_SPLASH="x,z"`
  (un chapoteo por segundo; los anillos solo avanzan con el juego en marcha). El color +0x34 se fija **al crear** el
  anillo (tabla[255] de ese fotograma para la mano y la cascada, tabla[200] para la lluvia): `AddWaterRing` resuelve
  `seaLight` una vez y el dibujo usa +0x34 tal cual, así un anillo hecho al anochecer o en un relámpago no cambia.
  Anillos de partículas (`PSys/PSysWaterRings`, llamados por las reglas de
  [particles.md](particles.md#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua)): explosión
  (`UR_Explosion` 0x67E347: en agua o altitud < 4 tres anillos de crecimiento 5, 7 y 10, celda 0x30, blanco, en
  (x, altitud, z); en tierra seca el chamuscado 0x251) y onda de partícula (`fn_006A1630`: solo en agua y a más de 2
  (rule+0x40) en xz de la última, crecimiento 4·radio del átomo, en el suelo).
- Cupo del original: 1024 anillos de 0x38 bytes en 0xEAB7C8. Los crean el chapoteo de la mano, los objetos que caen
  al agua ([arriba](#golpes-y-objetos-que-caen-al-agua)), la estela de los [tiburones](#tiburones-clase-whale), el
  pie de la [cascada](#decorado-fijo-por-tierra-cascada-de-land-3-arca-y-dinosaurio-de-land-4), los peces del
  [puzle](#puzle-de-los-peces), la lluvia y las partículas; los nadadores (SuperVillagers) aún no existen en openblack.
  El agua que asusta a los peces: [rendering-objects.md](rendering-objects.md#bancos-de-peces-de-las-piscifactorías).

## Tiburones (clase `Whale`)

- **Fiel** (clase `Whale`, Whale.cpp; hecho en W12, `src/ECS/Sharks.{h,cpp}`,
  `ECS/Archetypes/SharkArchetype.*`, `ECS/Components/Shark.h`). `CREATE(Whale = 26, 5000, pos)` → `Whale::Create`
  0x774C50 (`GMobileObjectInfo[24]`; la malla 370 de info.dat no se usa) → `CallVirtualFunctionsForCreation` 0x774CA0:
  **escala ×2**, malla 31 `MSH_SHARK_BONED`, clip 129 `ANM_SHARK_BONED_SWIM` en bucle, +0x6C (rumbo) = 0. Sin IA: por
  turno `Whale::Process` 0x775280 solo copia Pos en +0x2C (lo mueve el `WALK_PATH` del guion por el foco de las
  pistas `Track21`/`Track20` de `camera.edt`: [camera-tracks.md](camera-tracks.md), `ECS/MobileWalkPaths.*`). Por fotograma `fn_00774E30` (desde `GLandscape::Draw` 0x5E4B26, antes del mar):
  tiempo del clip += ms; rumbo = `LH3DMath::GetYAngle` 0x841290 = atan2(dz, dx) en [0, 2π) de Pos − +0x2C (el
  anterior si no se movió); posición interpolada con la fracción del turno (cada extremo en GetAltitude + relY);
  parte de abajo en 0xFF303070; estela `fn_00775170`; la parte de arriba es `Whale::Draw` 0x774E10 (tabla[255], plano
  por defecto). Sin sombra, sin reflejo, no se coge.
  Estela: un temporizador **global** 0xDCB984 para todos los tiburones (cada uno le suma los ms del fotograma): si
  pasa de 50, `%= 50` y un anillo en (p.x, 0, p.z), crecimiento 10, aspecto 0,5, ritmo 0,5, celda 0x31, 0x90FFFFFF,
  ángulo = rumbo, sin escribir +0x1C. p = la posición de `EBone.matrices[0]` por la matriz del hueso `EBone.bones[0]`
  (0x77507D, `fn_007FAE60`; en la malla 31 el hueso 0 y (−0,079, −0,003, 0,304)): `L3DMesh::GetEBonePoint0`.
  El temporizador suma ms enteros (`g_game+0x250540`, 0x775265: la diferencia de dos lecturas enteras del reloj de
  juego, `GGame::Loop` 0x54D374); openblack lo lleva igual (reloj en double y ms enteros por fotograma, sin perder
  tiempo a muchos fps). Espacio de `[0xC37D9C]`: el que acaba de llenar el `DrawCutByPlane` del propio tiburón
  (`fn_00811C70` → `fn_00839980`/`fn_00839BC0` mezclan los fotogramas del clip y `fn_00839F10` multiplica cada hueso
  por su padre y la raíz por la matriz de mundo del objeto, +0x14): **espacio de mundo**, sin cámara. openblack:
  modelo × pose (huesos en espacio de modelo) × punto, el mismo producto. Captura `_audit/agua/re_shark.png`. Gancho `OPENBLACK_TEST_SHARK=1` (los dos tiburones de
  `FollowUs` con sus `WALK_PATH`; captura `_audit/agua/paths_sharks1.png`).
  Captura `_audit/agua/w12_shark_noon.png`: aleta y cola claras sobre el agua, cuerpo azul oscuro a través del mar,
  anillos blancos saliendo del lomo hacia la cola.

## Puzle de los peces

Land 4, `PuzzleGame` 14: hay que meter 30 peces a la vez en la red durante 0,5 s. **Fiel**; faltan el pescador y el
pergamino. Los bancos de peces normales (piscifactorías: dibujo, susto, pesca, reserva) están en
[rendering-objects.md](rendering-objects.md#bancos-de-peces-de-las-piscifactorías); su creación desde el guion, en
[map-loading.md](map-loading.md#piscifactorías-create_fish_farm--create_town_fish_farm).

### La red, el cebo y los bancos

- **Fiel** (Land 4, `PuzzleGame` 14, `fn_006D7480` rama 0x6D7FCD): cebo `{pos, radio 11, need 30,
  500 ms}` + red `FishPlot` (ctor 0x829A30: `Data\MISC\Fishplot.l3d`, un flotador estático con luz dinámica, dibujado
  en 7 puntos `pos + 11·(cos(i·2π/7), 0, sin(i·2π/7))`, fase 0, cierre 1) + 2 bancos de 15 (rango 7) en `pos + (±12, 0,
  12)` con `+0x5C = cebo`. `fn_00824B90` cada fotograma: `inside = 0` en todos los cebos; por banco, `n =
  fn_00824DA0` (0 y el banco ya no se mueve ni se dibuja si el cebo está `done`; si no, los peces visibles con
  `dx² + dz² < r²` tras moverse, 0x824AB8), y si tiene cebo: la red bajo el agua (`fn_00829BC0`), `inside += n` y, con
  `inside ≥ 30` y sin `done`, `timer += g_game_time_inc`; a 500 ms `done = 1`, la red se cierra y **cada uno de los 15
  peces de cada banco de ese cebo** suelta un anillo (su posición, crecimiento 2, ritmo 1, +0x24/+0x28 = 1, celda
  0x30, blanco; +0x1C sin escribir). Al final, el cebo con `inside < 30` vuelve el temporizador a 0 (0x824D2E): los 30
  tienen que estar dentro a la vez 0,5 s seguidos. `fn_00829BC0` (dt = ms·0,001): si se cierra y `k ≠ 0`, `k =
  max(k − 2·dt, 0)`, radio `1 + 10·k` y se recolocan los puntos; `fase += 2·dt`; `SetClipPlane(0, −1, 0, 0)`, por
  flotador `SetPosition((x, y + 0,5·cos(i² + fase), z), 0, 1)` + `DrawCutByPlane` (vt+0x11C, fn_0080C050), y
  `SetClipPlane(0, 1, 0, 0)`. Como se llama **una vez por banco**, con los dos bancos la fase avanza 4/s y la red se
  cierra en 0,25 s, no en 0,5 (se replica). Encima del agua `fn_00829B50` (desde `fn_00824D60`, 0x5E6296): los mismos
  flotadores con el plano por defecto. Los bancos del puzle no se pescan (`fn_00824B10` salta `+0x5C ≠ 0`).
  - openblack: `components::FishBait` / `FishPlot` y `FishShoal::bait` (FishFarm.h), `ecs/FishPuzzle`
    (`CreateFishPuzzle`: los bancos son `FishFarm` de reserva llena sin `Transform`), la regla en
    `ecs::UpdateFishShoals`, `Renderer::DrawFishPlots` (`RendererFishPlot.cpp`; instancias propias, modo de corte de
    `vs_object`): la parte de abajo en la pasada de reflejo, espejada, tras los peces; la de arriba en la principal
    tras los anillos. La red se dibuja una vez por fotograma (el original la dibuja dos veces, una por banco, con la
    fase de cada llamada: no se ve). El color del corte es el `+0x4C` por defecto, 0xFFFFFFFF (ctor de
    `LH3DMeshedObject` 0x8164F7): el ctor de `FishPlot` no llama a `SetColour` (vt+0x2C); `UseDynamicLighting`
    (vt+0x58, `fn_008168C0`) solo pone el bit 0x20 de +4, y los únicos que escriben +0x4C son los dibujos de vt+0x100,
    +0x110, +0x130 y +0x154, por los que la red no pasa (`fn_00829B50`/`fn_00829BC0` solo llaman a vt+0x20 y
    vt+0x11C). El lado del guion (`CREATE_WITH_ANGLE_AND_SCALE` 32/14 y `PLAYED`) está hecho: ver
    [abajo](#el-lado-del-guion-puzzlegame-14-y-played). Faltan el pescador y el pergamino.
    Gancho `OPENBLACK_TEST_FISH_PUZZLE` ([Ganchos de prueba](#ganchos-de-prueba)).

### El lado del guion (`PuzzleGame` 14 y `PLAYED`)

- `CREATE_WITH_ANGLE_AND_SCALE(32, 14, PuzzlePos, …)` → `fn_006D6680` (puzzlegame.cpp, 0x588 bytes, lista
  g_game+0x205D14). No hace nada hasta que `GlobalGameLists::Process` 0x591449 llama cada turno a `fn_006D7480`:
  0x6D74C3 nada si +0x3C; si la prueba de "jugado" (`fn_006D66E0`) da 1, +0x3C = 1 y fuera; si no, el paso del tipo. El
  tipo 14 (0x6D7FCD) crea la primera vez el cebo en `ConvertToLHPoint(+0x14)` 0x6041C0 (altitud + y del guion), su red
  y los dos bancos (ver [arriba](#la-red-el-cebo-y-los-bancos)).
- `PLAYED` (64) = `GScript::Played` 0x6F9DC0: criatura → su plan (0x6F9DF4); Living → `IsScriptAnimationComplete` o su
  estado; tiempo → +0x78 == 0; **PuzzleGame** (vt+0x498) → `fn_006D66E0`, switch 0x6D6C94 sobre tipo − 1: el 14
  (0x6D6A32) da 0 sin cebo, 1 si `cebo+0x18` (done) —y pone +0x3C = 1—, 0 si no. Objeto perdido o "Thing not living"
  → 1 (openblack: todo lo que no es aldeano, animal, criatura ni puzle da 1 con "Thing not living"; los Living y la
  criatura siguen sin portar). El `done` lo pone `fn_00824B90` cuando los 30 peces llevan 500 ms dentro del radio 11 (los bancos solo cuentan
  a menos de 300 de la cámara, como se dibujan).
- `PuzzleGame::ToBeDeleted` 0x6D6FF0: borra el cebo con su `FishPlot` (fn_00829B20) y los dos bancos (fuera de la lista
  0xEB99F4, con sus 15 peces).
- openblack: `components::PuzzleGame`, `src/ECS/PuzzleGames.{h,cpp}` (creación, turno, `PLAYED`, limpieza de los
  borrados), `CreateScriptObject` tipo 32 y `PLAYED` en `CHLApi`. Solo el tipo 14; los demás puzles (Hanoi,
  laberintos, tótems, ajedrez...) no están portados y su `PLAYED` da 0.

## Barco de los misioneros (PetitNavire)

`PLAY_JC_SPECIAL(6)` en Land 1 (`TheMissionaries`): la botadura del arca de los misioneros y su travesía. **Fiel**,
con las diferencias que se dicen al final. El reflejo del casco se dibuja como los demás reflejos
([rendering-objects.md](rendering-objects.md#reflejos-de-objetos-y-sombra-de-la-mano-sobre-objetos)); el porcentaje
de construcción del arca en el dique (`BUILT_PERCENTAGE`) está en
[map-loading.md](map-loading.md#porcentaje-de-construcción-de-un-feature-built_percentage-propiedad-chl-22).

Scripts de RE en `tmp_dis\agua\re\` (`emu_navire_pre.py`, `emu_navire_post.py`: Unicorn con objetos LH3D falsos que
registran cada llamada; `rd.py`; `chlfn.py` da la función GScript de un opcode CHL, tabla 0xC0DB98 + 0x90·opcode).
- **CHL**: `PLAY_JC_SPECIAL` (326) = `GScript::PlayJCSpecial` 0x708ED0, tabla 0x708F74 sobre el valor entero (0..15):
  0, 1, 2, 4, 5, 6 → `fn_005DF9C0(n)`; 3 → un objeto de 0x2C de ScriptGFX (0x828DB0); 14/15 → [0x9CD384] = 1/0. En
  `fn_005DF9C0` el caso 6 (0x5DFBF8) es `new PetitNavire(0)` (0x68 bytes). `IS_PLAYING_JC_SPECIAL` (327) 0x708FC0 saca
  un **float** (ftol) y devuelve 1 salvo con 13, que da [0xD19C94] (solo lo pone la intro de la mano, `fn_005DF640`
  0x5DF807; sin portar → 0).
- **Un solo barco** [0xD19CB4]; el ctor 0x5E1020 libera el que haya (`fn_005E13C0`: suelta Boat1/Boat2 y los objetos y
  pone el global a 0). Constantes (inicializadores `crt_xc_fn_JCMisc_005DFED0/005DFF00`): dique [0xD19A08] =
  (1881,0833; 8,1316; 3154,1094), salida en el mar [0xD199F8] = (1456,54; 0; 3263,06).
- **Objetos**: casco MSH_O_ARK (339) estático, `SetPosition(dique, 0, 1)`; marinero MSH_P_NORS_SAILOR (504) animado con
  ANM_P_PUSH_OBJECT (346). Animaciones +0x08..+0x20 = 346, 332 OVERWORKED1, 235 CROWD_WON_2, 333 OVERWORKED2, 406
  TITANIC, 378 SITTING_SWINGING_LEGS, 359 SCRUBBS. Modo 0: sombra dinámica propia (`fn_008745A0`, holder+4 = 1 y
  ShadowInfo+0xC = **0**: también cae sobre objetos). Modo 1: vaca MSH_A_COW_1 (16) con ANM_A_COW_EAT_2 (36), montón de
  grano MSH_S_GRAIN_PILE (533) y `LH3DSprite::Create(5)` con el material de humo [0xEA1ABC] (`smoke.raw`, **modo 6**,
  0x80BC7D), +0x14 = 3,92699 (5π/4), bandera 0x40 (plano en XZ), celda 0x31; fases +0x50[i] = i·1200 ms.
- **Pistas del casco**: `Data\MISC\boat1.anm` ("beach04", 158 fotogramas, 15833 ms, banderas 0x501 = en bucle) y
  `boat2.anm` ("beach_sailing", 44 fotogramas, 4466 ms, 0x501). Una sola matriz por fotograma con **determinante −1**;
  `fn_0083AC70` interpola los 12 floats (como `LH3DAnim::GetPose`) y la compone con la matriz padre M (`fn_007FAFF0`:
  pista·M). Después `RotateY(π/2)` 0x5198F0 y `fn_007FAE60(diag(−1, 1, 1))`, que **multiplican por delante** (espacio
  local): casco = espejo·RotY(π/2)·pista(t)·M, determinante +1. En glm: `M · pista · eulerAngleY(−π/2) · scale(−1, 1, 1)`.
- **PreDraw 0x5DFF20** (desde `GLandscape::Draw` 0x5E490F, antes del mar), con dt = `g_game_time_inc` entero:
  - Modo 0: +0x34 += dt; +0x64 = +0x24; si `!+0x48 || +0x34 > 3000`, +0x24 += dt. Si +0x24 > 15833 − 400 se borra,
    hace `new PetitNavire(1)` y **vuelve**: ese fotograma el barco nuevo no tiene PreDraw y su PostDraw lo dibuja una vez
    en el dique sin girar. Si no: M = Translate(dique); y del casco += `GetAltitude(casco.xz)` − `GetAltitude(dique.xz)`
    (0x5E00EB..0x5E0154); `fn_00874850` (sombra); +0x4C = 0xFF303070 y `DrawUnderWater` (vt+0x118, el reflejo);
    `fn_00801C90` le devuelve la luz de tierra.
  - Modo 1: +0x24 = (+0x24 + dt) % 4466 (en bucle; si no, min(…, dur − 1)); +0x34 += dt; a los 60000 ms se borra. M =
    RotY(π/4) (0x92B210) en (1456,54; 0; 3263,06) + (−k, 0, −k)·0,005·+0x34, con k = `InverseSquareRoot(2)` 0x841170
    (tabla 0xEEA394 más un paso de Newton = 0,70710659): **5 u/s** hacia −x −z, 300 unidades en total. Casco como
    arriba (sin corrección de altura ni sombra), 0xFF303070 y `DrawUnderWater`.
  - **Resuelta la duda B4**: no hay "dos partes" ni dos dibujos por fotograma. 0x5E0100-0x5E0190 (modo 0) y
    0x5E0380-0x5E03EE (modo 1) son ramas **excluyentes** (0x5E0195: `cmp +0x30, 1`); **las dos** montan el espejo
    diag(−1, 1, 1) (0x5E00C1-0x5E00D9 y 0x5E0350-0x5E03B6) y el π/2 es el `RotateY` del casco. Cada fotograma hay un
    solo `DrawUnderWater`, en 0xFF303070.
- **PostDraw 0x5E03F0** (desde `fn_005E5CD0` 0x5E6250, después de `fn_00824140`):
  - Modo 0, sonidos 2D (`GAudio::PlaySoundEffect` 0x429E30, banco GGlobal+0x3BC = `Scriptsfx.sad`, opciones +0xBC = 2,
    es decir +0x50 = modo 2, dueño 0, is3D 0; en openblack por `sample_play`, uno de los 16 canales)
    cuando el tiempo del casco cruza el umbral (+0x64 < umbral < +0x24): 100 → 62 `MissionaryBoatCreak_01`, 1500 → 61
    `MissionaryBoatSlide_01`, 3900 → 60 `MissionaryBoatSplash_01`.
  - Modo 0, cada 200 ms (+0x38 += ftol(dt); > 200 → acción y +0x38 = 0): si 3900 < t < 6500, 2 `SmokyStuff::Create`
    (casco + (r2 − 10, **7**, r1), modo 0, tamaño 7, 0xFEFFFFFF); si 1130 < t ≤ 3900, 2 × (casco + (r2, 0, r1), 0, 5,
    0xFFB88C38, arena), con r1 = Random(−20, 20) y r2 = Random(−2, 2) en ese orden (corrige el informe: el ±20 va en z y
    el −10 / +7 en x / y). Después el casco (vt+0x100).
  - Modo 0, marineros (el mismo objeto dibujado 5 veces): si t > 850: si +0x48, +0x34 = 0; animación +0x0C + (i % 3)·4
    y +0x48 = 0. Posición (dique.x + {5,2; 5,3; 5,5; 5; 5}[i], suelo, dique.z + (i − 2,5)·3 + {1; −0,7; 0; 0,4; −0,2}[i]
    + 10) (0xBF2B1C, 0xBF2B44), ángulo −π/2, fotograma ({5, 500, 1500, 455, 2000}[i] + +0x34) % duración (0xBF2B30),
    color = luz de tierra en su sitio.
  - Modo 1, estela (0x5E0785): fase = (fase + dt) % 6000, t = fase/6000; posición = casco·(0, 0, 100t − 15) con
    **y = 0,2**; media anchura +0xC = 30t + 10, aspecto +0x10 = 0,5; alfa = ftol((1 − f)·255) con f = (t − u)/(1 − u)
    si t ≥ u (si no, u) y u = [0xD19CB8]: **nadie escribe ese float** (solo lecturas en 0x5E086E..0x5E0893, ningún
    inicializador), así que u = 0 y f = t; solo se dibuja si f > 0,2 (doble 0,2 en 0x8C7C68).
  - Modo 1, cubierta (0x5E08E7..0x5E1015, emulado): la matriz de cada uno es L·casco (`fn_007FAFF0` y copia a
    obj+0x14), L = RotY(a)·escala + t en el marco del casco; color = el +0x4C del casco. Vaca (a = −1,
    t = (−1,778; 10,78; 1,83), fotograma +0x34 % dur) y otra vez (a = −0,7, t = (−1,778; 10,78; 3,83), +0x34 + 1255);
    grano (escala 0,26, t = (−5,708; 10,854; 3,199)); el objeto marinero con MSH_P_NORS_F_A_1 (498) y TITANIC en
    (−0,14; 13,213; −19,657) (+0x34), con 504 y TITANIC en (−0,14; 13,213; −18,9) (+0x34 + 500), SITTING girado π en
    (−6,25; 11,424; −7,227) (+0x34) y en (−5,25; 11,424; −7,227) (+0x34 + 2345), SCRUBBS girado π en
    (5,881; 10,741; 0,174) (+0x34).
- **SmokyStuff** (0xCC bytes, lista 0xEB99CC): `Create` 0x823C90(pos, modo, tamaño, color) = 15 sprites de humo modo
  6 que miran a la cámara; cada uno en pos + (c, b, a) con a, b, c = Random(−tam, tam), giro Random(0, 2π), celda 0x10,
  velocidad norm(e, tam, d)·Random(0,3; 1)·tam (modo 0). `fn_00824140` (0x5E619C, dt = ms·0,001) → `fn_00823F70`:
  vida −= dt/3 (modo 0), nada si vida ≤ 0; color (vida·100)<<24 | 0x808080 con el rgb del argumento; giro ±5·vida + v.x;
  media anchura ((1 − vida)·2 + 1)·tam/2; pos += v·dt; celda (int)(vida·15); se libera con vida < 0. `Random` 0x81D180
  = a + (b − a)·rand()/32768 (stdcall). Billboard de `LH3DSprite::Draw` 0x84071D: x local → (cos, −sin) en pantalla,
  y local → (sin, cos).
- openblack: `src/ECS/PetitNavire.{h,cpp}` (estado, entidades, PreDraw y PostDraw en `Update` con el tiempo entero y el
  resto guardado), `src/ECS/SmokyStuff.{h,cpp}` (el mismo módulo que el humo del cadáver de
  [animals.md](animals.md), `Object::CreateSmokyStuff` 0x63A810), `components::DynamicShadow` (la sombra del casco entra en
  `graphics::PhysicsShadows`), `Renderer::DrawBoatReflection` / `DrawBoatSprites` (`RendererBoat.cpp`; el reflejo en
  0x303070 usa el modo 2 de `vs_object` con el rgb empaquetado cuando z > 1). Diferencias que quedan: los sprites se
  dibujan después de los modelos transparentes en vez de ordenados con ellos; la cubierta toma la luz de tierra de su
  propio sitio (no la del casco); la sombra del casco solo cae en tierra (`PhysicsShadows` no se dibuja sobre objetos);
  el modo ≠ 0 de `SmokyStuff::Create` (0x823DA7) no tiene llamadas aquí y no está portado.

## Decorado fijo por tierra: cascada de Land 3, arca y dinosaurio de Land 4

Informes: `tmp_dis\agua\sealife_features.md` §2.6 y §3, `tmp_dis\agua\audio.md` §6. **Fiel** (hecho), con las
diferencias que se dicen en el punto de openblack.

- `GWaterfall` (`CREATE_WATERFALL`, comando 68 de Land, 0x7175D1; ctor 0x734130, vtable 0x8EC14C) es un objeto
  **vacío**: `CallVirtualFunctionsForCreation` 0x7341B0 = `ret 4`, no dibuja ni suena, y ninguna tierra lo usa.
- `DesignedWaterFall` 0x5E3770 (LandFeature.cpp), cada fotograma desde fn_005E5CD0 (0x5E5CDA), según el número de
  tierra (`SET_LAND_NUMBER`, g_game+0x205A08). Al cambiar de número (última tierra en 0xBF34E4, 74 al arrancar) borra
  los dos objetos (0xD1A31C / 0xD1A324), el `ScriptMarker` 0xD1A32C, su `SoundTag` 0xD1A330 (ToBeDeleted: el bucle
  termina la pasada) y suelta las mallas. No son objetos de juego: `LH3DObject` sueltos (sin sombra, sin celda).
  - **Land 3**: `Data\MISC\waterfall3.l3d` (`LH3DObject::Create(0)`, estático) en (3059,23; **0**; 3145,33), ángulo
    4,7, escala 1, luz dinámica, +0x10 = 1. Cada fotograma `V = V − 0,5·dt` y `V −= (int)V` (queda en −1..0) con
    `SetUVOffset(0, V)` (vt+0xE8 fn_007F9B70: +0x68/+0x6C y bandera 0x400); dt = `g_game_time_inc`·0,001. Cada 0,7 s
    de juego (temporizador 0xD1A338, se reinicia aunque el cupo esté lleno) un anillo en (3018,8; 0,2; 3130,15):
    crecimiento 30, +0x24 = 1, aspecto 1, ritmo 0,3, celda 0x30, ángulo 0, color `0x80 << 24 | tabla[255].rgb`; el
    campo de deriva +0x1C no se escribe. `SoundTag::Create(marcador, 12 G_WaterFlow, false, modo 2, bucles −1, 0,
    3D, InGame, 0)` (0x5E3921).
  - **Land 4**: `Data\MISC\arche.l3d` (Create(1)) en (3538, altitud, 2129), ángulo 8,9728, escala 1,1, +0x10 = 10,
    con el mismo SoundTag de `G_WaterFlow` en (3538, 0, 2129); `Data\MISC\dinosaur.l3d` en (2690, altitud, 2590)
    (≈ 138,7: tierra alta), ángulo 1,57, escala 1, +0x10 = 10, con su huella de terreno (fn_0081E9E0, la lista de
    huellas de los edificios). `dinosaur.l3d` es la única de las tres con `ContainsLandscapeFeature` (0x8000);
    `waterfall3.l3d` trae un bloque de huella pero sin esa bandera y nadie la estampa.
  - El marcador se crea con `MapCoords(LHPoint)` (0x603340 guarda y − altitud) y `Get3DSoundPos` 0x56FE20 le vuelve a
    sumar la altitud: el sonido sale en y = 0 exacto. `G_WaterFlow` (InGame 12; el banco lo describe como "Citadel
    waterfall", pero solo lo usan esta cascada y el arca): volumen 50, bucle, min 60, **max 110**, escala 6, modo 2.
- openblack: `ECS/DesignedScenery` (`designed_scenery::Update` cada fotograma tras los anillos, `OnLoadMap` antes del
  reset del registro): entidades `Transform` + `Mesh` (mallas `misc/<fichero>` cargadas al usarse), `UvScroll` en la
  cascada, `AddWaterRing` con `seaLight`. La huella del dinosaurio sale sola en la pasada de huellas (malla con
  `ContainsLandscapeFeature`). Sonido: `Audio/SoundTags` (ver [Audio del agua](#audio-del-agua)). El +0x10 de los objetos (float 1 / 10) es el factor k de
  distancia del LOD de `fn_00815A70` (vt+0x100): D = min((k + 1)·[0xC37EA0]·[0xC3813C], 100000) (0xC3813C lo mueve
  `LevelOfDetail`), LOD 1/2/3/4 por debajo de 23,33·D / 66,67·D / 86,67·D / más allá (`g_last_distance` 0xEA1AF4) y
  cada submalla se dibuja si los bits 29..31 de sus banderas contienen el LOD: las tres mallas los tienen todos
  (0xE0000800), así que k no cambia nada en pantalla y no se guarda. Diferencias: la luz dinámica es la de todos los objetos; al cargar
  un mapa el registro se borra, así que el bucle se corta en seco en vez de acabar la pasada.
- **Qué se desplaza (hecho)**: la bandera 0x400 no la lee nadie. El `Draw` estático 0x80DB30 lee U y V con vt+0x8 /
  vt+0xC (0x80DE86) y los deja en 0xECA62C / 0xECA630 (0xECA628 = 1 si no son los dos 0); el envío de triángulos por
  defecto (`[0xC386EC]` = `LH3DRender::DrawTriangle` 0x82F810) los suma a las UV **salvo si el byte +5 del material
  tiene el bit 0x10** (0x82F8CC). En `waterfall3.l3d` la roca (submalla 0, `Textured`, +5 = 0x14) lo tiene y el agua
  (submalla 1, `TexturedChroma`, 0x04) no: **solo corre el agua**. (Otras rutas, `fn_0082FD70` de `fn_00812170` /
  `fn_00817930` y `fn_00884750`, lo suman siempre; el estático no pasa por ellas.) En `AllMeshes.g3d` solo las
  mallas `S_PHILE*` tienen ese bit, ninguna pila de comida. openblack: `L3DSubMesh::Primitive::uvOffset` y
  `u_window.w` en `vs_object`; captura `_audit/agua/re_waterfall_diff.png` (diferencia de dos fotogramas: solo cambia
  la lámina de agua, y las palmas por el viento).
- **Anillos del pie**: se dibujan con `LH3DSprite::Draw` en modo 13 (`fn_0082ECD0`: mezcla SRCALPHA/ONE, sin prueba
  de alfa, ZWRITEENABLE 0 y sin tocar ZFUNC): con prueba de Z contra la tierra, como en openblack. Los anillos del pie quedan casi enterrados: el suelo está a 0 en el punto y
  sube a 1,7 en 7 unidades, así que con la prueba de Z solo se ve un brillo tenue.

## Audio del agua

Qué suena con el agua y cuándo. El motor (canales, modos, distancias, bancos) está en [audio.md](audio.md); el
ambiente y la mano en el agua están, de momento, en
[objects-and-resources.md](objects-and-resources.md#sonidos-informe-tmp_dissoundnotestxt) («Mano en el agua / agarrar
tierra» y «Ambiente (atmos)»). Todo **fiel**.

- **Mano en el agua**: al empezar a agarrar el terreno sobre una celda de agua (`StartLandscapeGrip` fn_005D1AB0), un
  `G_HandInWater_01..10` (InGame 99 + contador) 3D en (x; 0,2; z), uno a la vez (grupo de clones 4, modo 3), con el
  anillo del chapoteo y el susto de los peces; no suena en pausa ni con algo en la mano.
- **Golpes contra el agua**: con altitud < 3 en la celda redondeada el tipo de colisión es WATER y suena la muestra de
  `editor.sad` de la tabla de choques ([physics.md](physics.md#sonidos-polvo-y-aspecto-de-los-golpes)); `G_BigSplash`
  (modo 2) no se repite mientras suene el mismo sample del mismo objeto.
- **Ahogarse**: los eventos del clip 252 `P_DROWNING` (chapoteo 157 y voz 134,
  [arriba](#hundirse-ahogarse-y-borrarse)).
- **Barco de los misioneros**: tres sonidos 2D de `Scriptsfx.sad` en la botadura (62, 61, 60;
  [arriba](#barco-de-los-misioneros-petitnavire)).
- **Cascada de Land 3 y arca de Land 4**: `G_WaterFlow` (InGame 12) en bucle por una `SoundTag`
  ([arriba](#decorado-fijo-por-tierra-cascada-de-land-3-arca-y-dinosaurio-de-land-4)).
- **Partículas que caen al agua**: el rebote y la onda de `UpdateRuleGravityWithFloor`
  ([particles.md](particles.md#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua)).
- **Ambiente**: por el tipo ATMOS de las celdas cercanas a la cámara: 1 SEA `ocean.sad`, 3 COASTAL `shore.sad`, 2
  STILL_FRESH_WATER `lake.sad` (y 9 RUNNING_WATER `stream.sad`, que ningún mapa base usa); el mar calla a menos de 20
  de una celda COASTAL. Los ríos no tienen sonido propio (`ATMOS_TYPE_RUNNING_WATER` sin analizar, ver
  [rendering.md](rendering.md#ríos)).

El motor de las etiquetas de sonido que usan la cascada y el arca (no está en [audio.md](audio.md)):

- **SoundTags** (`Audio/SoundTags`, SoundTag.cpp 0x71E300..0x71ED90): etiqueta {cosa o punto fijo, desplazamiento,
  muestra, bucle, activa}. `ProcessTurn` = `SoundTag::ProcessSoundTags` 0x71E5F0 una vez por turno (GGame::EndTurn):
  si la cosa ya no existe → ToBeDeleted; si está activa, `fn_0071E680` llama a `GAudio::PlaySoundEffect` 0x42A100 con
  **la propia etiqueta como dueño** del canal (+0x20), el punto (`Get3DSoundPos` de la cosa), el desplazamiento +0x1C,
  muestra +0x28, seguir +0x30 (solo con cosa; `false` en las del decorado), modo +0x38, bucles +0x3C, is3D +0x44 y su
  banco (`SoundTag::Set` 0x71E4F0); 0x429E30 solo la arranca con la cámara a ≤ maxDist (.sad +0x26C) del punto y el
  modo 2 deja el canal que ya suena. En openblack va por `sample_play` (dueño `Owner::Tag`), así cuenta en los 16
  canales de LHaudio; `IsPlaying` = `LHSampleIsPlaying` (fn_0042A2D0) y `ReleaseLoop` = 0x42A310. `SetActive(0)` corta
  en seco (LHSampleStop); `Delete` suelta el bucle (LHSampleReleaseLoop) y la etiqueta muere al terminar la pasada.
  `Clear` en `Game::LoadMap` antes del reset del registro (los emisores son entidades). Es la generalización de la
  idea de `LanternSounds` de la rama principal (farolas, muestra 0x93); esa todavía no usa este módulo.

## El agua en otras páginas

- Dibujo del mar (filas, ondulación, deriva, lo que hay bajo el mar), la costa y su alfa, los ríos y el brillo de la
  mano de noche sobre el agua: [rendering.md](rendering.md#mar-skyraw--skyaraw),
  [Costa](rendering.md#costa), [Ríos](rendering.md#ríos), [Cielo](rendering.md#cielo-sol-luna-y-nubes-original).
- Reflejos de la mano, los objetos, la criatura y los barcos; los cortes bajo el agua (`DrawCutByPlane`); los bancos
  de peces de las piscifactorías: [rendering-objects.md](rendering-objects.md#reflejos-de-objetos-y-sombra-de-la-mano-sobre-objetos),
  [DrawCutByPlane](rendering-objects.md#cortar-por-el-plano-del-agua-drawcutbyplane),
  [bancos de peces](rendering-objects.md#bancos-de-peces-de-las-piscifactorías).
- Creación de las piscifactorías desde el guion:
  [map-loading.md](map-loading.md#piscifactorías-create_fish_farm--create_town_fish_farm).
- Flotación y hundimiento en el motor de física: [physics.md](physics.md#motor-physob-0x7fb7300x7fe7b0); soltar desde
  la mano: [physics.md](physics.md#el-agua-en-los-golpes-y-al-soltar).
- Recorridos de los tiburones (`WALK_PATH`, pistas de `camera.edt`): [camera-tracks.md](camera-tracks.md).
- Partículas sobre el agua (rebote, ondas, vapor de la explosión): [particles.md](particles.md#el-psys-en-el-mundo-formato-paso-dibujo-y-reglas-del-agua).
- Animales en el mar: [animals.md](animals.md#mano-vuelo-y-muerte); dejar algo sobre el mar con la mano:
  [hand-and-interface.md](hand-and-interface.md).
- Mods: `water.living` y el agua de `world.foliage` en [mod-library.md](mod-library.md#waterliving).

## Pendiente

- Consultas de agua y `LandAvoid`: portadas y probadas, pero sin consumidor hasta que haya trabajos de los aldeanos y
  criatura ([arriba](#consultas-de-agua-gutils-gstream-abode)).
- `GET_PROPERTY`: solo `FLYING` y `DROWNING`; las demás propiedades siguen sin implementar.
- Espuma de los golpes contra el agua: el color hacia la luz base según la nieve (`SnowCover`) no se aplica.
- Hundirse: avisar a la criatura para que imite al jugador que soltó al aldeano o al animal
  (`ConsiderMakingCreatureMimicPlayer`); `lastPlayerToInteract` sin jugadores donde guardarlo.
- Anillos: no hay nadadores (SuperVillagers con `M_P_Swim2`).
- Puzle de los peces: el pescador y el pergamino.
- Barco: los sprites ordenados con los transparentes, la luz de la cubierta, la sombra del casco sobre objetos y el
  modo ≠ 0 de `SmokyStuff::Create`.
- **Muerte del aldeano (`TODO(villager-death)`)**: `VillagerDead` 0x7506C0 de verdad (alineamiento por motivo, contadores del
  pueblo, textos de guía, madera/comida que suelta) y el estado DEAD (`Villager::Dead` 0x76A5E0): borra el fuego,
  `CreateSmokyStuff` y, **solo fuera del agua**, `fn_00828790` = un registro de 12 bytes (lista 0xEB9A7C) con un
  `LH3DObject` nuevo de la malla del aldeano (la de niño, `GVillagerInfo`+0x204, por debajo de la edad +0x138) que
  toca el clip del **alma** P_DEAD1/2_GOTO_HEAVEN o _HELL (244/245 o 247/248: la pareja según el clip "M_P_DEAD1",
  cielo o infierno al 50 %); luego la malla del aldeano pasa a ser la 0x1FF `PersonSkeletonMale` ([0xDCB164]). En el
  agua: humo y esqueleto, sin alma. Hoy, al llegar el contador a 0, el aldeano se borra con `ToBeDeleted`.

## Ganchos de prueba

Todos en [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración) (y los de física en
[physics.md](physics.md#ganchos-de-prueba)):

- `OPENBLACK_DUMP_LAND_AVOID=1` (o `=<fichero>.png`): la máscara `LandAvoid` (colores [arriba](#máscara-landavoid-de-la-criatura)).
- `OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (+ `OPENBLACK_PHYSICS_TRACE=1`): un objeto en el agua que flota, se hunde o
  se ahoga; con `OPENBLACK_TEST_CUT=1` además su parte bajo el agua se dibuja cortada.
- `OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"`: la mano suelta suave algo en (x, z), p. ej. un aldeano en el mar
  (1464; 2016).
- `OPENBLACK_TEST_SPLASH="x,z"`: un chapoteo de la mano por segundo (anillos, sonido, susto de los peces).
- `OPENBLACK_TEST_SHARK=1` (o `="pista,cámara[,adelante[,desde[,hasta]]]"`) y `OPENBLACK_WALK_PATH_TRACE=1`: los tiburones.
- `OPENBLACK_TEST_FISH_PUZZLE="x,z[,dentro]"` (+ `OPENBLACK_HAND_TRACE=1`): el puzle de los peces.
- `OPENBLACK_TEST_JC_SPECIAL="6[,modo[,fotogramas[,ms]]]"` y `OPENBLACK_BOAT_TRACE=1`: el barco de los misioneros.
- `OPENBLACK_SCENERY_TRACE=1` y `OPENBLACK_SOUND_TAG_TRACE=1`: el decorado fijo de Land 3/4 y sus etiquetas de sonido.
- `OPENBLACK_AUDIO_TRACE=1` (canales) y `OPENBLACK_ATMOS_TRACE=<n>` (ambiente).
- Tests: `test_sea_cells`, `test_water_queries` y `test_psys_water`.

`test_water_queries` (Land1 real, ver [openblack-internals.md](openblack-internals.md#tests-y-datos-de-prueba)):
máscara coherente con los predicados de celda, costa más cercana (y que la espiral conserva la fracción de celda),
punto de río más cercano y su y, agua potable no más lejos que el río, caché de la casa y distancia con la raíz inversa
del original.

## Fuentes

- `dev\tmp_dis\agua\`: `sealife_features.md` (tiburones, puzle, decorado), `audio.md` (ambiente, cascada),
  `sea_render.md`; scripts de RE en `re\` (`emu_navire_pre.py`, `emu_navire_post.py`, `rd.py`, `chlfn.py`,
  `scan_tree5c.py`) y `re\NOTES.md`.
- `dev\tmp_dis\physics\` (`physob.md`, `physicsobject.md`, `collision_sounds.md`): flotación, golpes y hundimiento.
- `dev\tmp_dis\fish\fish_notes.txt` (susto y pesca), `dev\tmp_dis\render\cut_notes.txt` y `objshadow_notes.txt`.
- `bw1-decomp` (`src/Black/Object.cpp`, `include/chlasm/ScriptEnums.h`).
- Capturas en `_audit/agua/` (`re_shark.png`, `paths_sharks1.png`, `w12_shark_noon.png`, `re_waterfall_diff.png`).
