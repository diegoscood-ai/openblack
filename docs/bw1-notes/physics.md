# Físicas: objetos lanzados, choques, daño y rocas que se parten

Código: `src/ECS/Physics/` (`PhysOb` = sólido rígido, `PhysicsObjects` = gestor por turnos), `src/ECS/Rocks.*`,
`src/ECS/Components/Life.h`, y la parte de la mano en `HandPhysics.cpp`. Informes completos con pseudo-C++ y
direcciones en `C:\Users\diewgarc\dev\tmp_dis\physics\` (`physob.md`, `physicsobject.md`, `physob_bodies.md`,
`rock_split.md`). No se usa Bullet para esto (openblack solo lo usa para lanzar rayos).

## Motor (PhysOb, 0x7FB730..0x7FE7B0)

- **Contactos por penalización** sobre una nube de vértices (masas puntuales): cada vértice bajo el suelo o dentro de
  otro cuerpo recibe un muelle normal y un ancla de rozamiento de Coulomb. Sin impulsos ni coeficiente de rebote: el
  rebote sale del muelle y la amortiguación de probar los vértices **0,06 s por delante** (`x + v·0,06`).
- **dt 0,005 s, 20 subpasos por turno de 0,1 s**, Euler semiimplícito. Gravedad 9,81; velocidad máx. 124 (la misma del
  lanzamiento de la mano); ω máx. 3π. Sin amortiguación lineal; la angular conserva `d4` por segundo.
- El original acumula el par como F×r y gira las filas al revés; las dos inversiones se cancelan. El port usa r×F y
  columnas (mismo movimiento). El tensor de inercia conserva el fallo del original (`I[1][2] = −xz`).
- **Suelo**: `GetAltitude` por vértice y la **normal plana del triángulo exacto** (`LH3DIsland::GetNormal` 0x803630,
  alturas crudas sin el aplanado del borde del mar). openblack normaliza exacto; el original usa una tabla de 1024.
- **Mar**: si el suelo bajo el centro es < 0,0001, el centro está por debajo del radio y la celda no tiene tierra:
  flotación `frac·m·g/densidad`, arrastre ×100, y **ningún contacto con el fondo**. La densidad sube 6,67e-5 por
  subpaso sumergido (se empapa); por encima de 1 se hunde. Por debajo de −4R el objeto se borra.
- **Reposo**: umbral 1 (4 si ya reposa), crece tras 15000 cuentas; el contador empieza en −escala·media altura·1000
  (0x7FB7D6) y suma 5 por subpaso.
- `Data\PhysicsConstants.txt`: versión 3, 24 filas × 6 (densidad, k de contacto/masa, k de penetración/masa, µ,
  fracción angular conservada por segundo, arrastre), cada columna recortada a su rango. Filas por clase: 0 casas,
  3 rocas, 4/5 vasijas, 6 árboles, 7 aldeanos, 8 animales, 14–20 juguetes, 21–23 setas (tabla en `physob.md`).

## Cuerpos

- Masa = `escala³ · info.weight` (Object::GetWeight 0x638480), mínimo 0,01. Casas 2000 estáticas.
- Por defecto, los vértices y triángulos de las submallas `isPhysics` (bit 13) o, si no hay, las de LOD 0.
  **El `AllMeshes.g3d` modificado del usuario no trae submallas de física en las rocas**, así que se usa la malla de
  dibujo (40–109 vértices; el arte original tiene 12–24).
- Árboles (`SetUpPhysObAsATree` 0x63A230): 16 puntos en anillos a lo largo del tronco y 24 caras; centro de masa a
  0,4 H (vivos) o 0,5 H (muertos). Aldeanos y animales (0x5EFF40 / 0x5F04E0): caja de 12 puntos y 20 caras, centro a
  media altura, arrastre ×2.

## Gestor (PhysicsObject::GameTurnUpdate 0x644FC0)

- Cada turno, cada cuerpo que se mueve mira la caja `|v.xz|·0,1 + R` y añade como **obstáculos en reposo** los objetos
  que interactúan: rocas y estáticos, objetos móviles, aldeanos, animales, árboles muertos, vasijas de la mano, casas y
  almacenes. **Los árboles en pie no interactúan** (lo lanzado los atraviesa); tampoco campos, bosques ni montones.
- Choque objeto–objeto: rayo del centro de A a cada vértice contra los triángulos de B; las fuerzas van a los dos. Un
  obstáculo que no consigue quedarse quieto (umbral 4) pasa a volar: así se derriba a un aldeano o se empuja una roca.
- Al final del turno: `impacto = |ΣF|·0,05` (fuerza media), G = impacto / (m·g) (≈1 apoyado), y
  `ReactToPhysicsImpact` en los dos cuerpos. El daño se atribuye al jugador que lanzó lo que golpea.
- **Daño**: aldeanos y animales, si G > 2, pierden `(G−2)·0,03` de vida (× defenceMultiplierCrush; 1 en aldeanos).
  Rocas (no golpeadas por otra roca), si G > 4 y altura > 0,7: vida −(G−4)·0,005, y se parten por debajo de 0,01.
  Árbol o árbol muerto que golpea un almacén: se convierte en madera. Casas: ver abajo.
- **Fin del vuelo**: un árbol lanzado acaba como DeadTree en la postura en que quedó; una vasija de la mano en tierra
  pasa a montón; aldeanos y animales se levantan; un aldeano que acaba en una celda con agua pasa a DROWNING (60 s)
  y un animal hundido se borra (ver más abajo).

## Edificios que se rompen (Abode::ReactToPhysicsImpact 0x406240, FragMesh 0x7F6F00..)

Código: `src/ECS/Physics/Buildings.*`, `FragMesh.*`, componentes `BuildingDamage` y `Fragment`
(`src/ECS/Components/Fragment.h`), mallas generadas en `src/3D/L3DMeshGenerated.cpp`. Informes:
`tmp_dis/physics/fragmesh.md` y `abode_damage.md`.

- Solo rompen edificios las rocas (fila 3) y el juguete de fila 20, con `p = |v|·masa` del que golpea:
  **p > 2000** rompe; 1000–2000 y 300–1000 solo suenan (`editor.sad` 431–436 y 437–442).
- Al primer golpe el edificio se copia en triángulos en coordenadas de mundo (submallas LOD 0). Un **cilindro
  infinito** por la posición de la roca, en la dirección de `0,3·v` y de radio `R + 0,7`, decide:
  - triángulos con los 3 vértices dentro se rompen;
  - los que tienen 1–2 se parten por la mitad del lado más largo (hasta 3 veces según su tamaño);
  - los pequeños van por mayoría.
- Los rotos de cada primitiva vuelan como un trozo con la velocidad del golpe (giro ±1). Sus partes sueltas y todo lo
  que queda del edificio sin tocar el suelo (y < suelo + 0,1) caen como trozos quietos (giro ±2). Los triángulos
  sueltos desaparecen. Grupos por lados compartidos (tolerancia 0,01).
- La **vida del edificio** pasa a ser la fracción de triángulos que quedan; bajo 0,75 deja de funcionar (pendiente:
  que salgan los aldeanos y la emergencia del pueblo); a 0 desaparece: sus aldeanos se quedan sin casa y un almacén
  pierde sus montones. Suena el derrumbe (`editor.sad` 443–447).
- El edificio dañado se dibuja con su FragMesh: cada triángulo plano, con cara trasera 0,45 detrás y una pared en
  cada borde abierto (malla generada propia). Su cuerpo de física sigue con la malla intacta; si la misma roca
  vuelve a darle, dejan de chocar (la atraviesa en el tercer contacto).
- **Trozos**: cuerpo = sus vértices distintos y una copia de cada uno 0,45 detrás (sin caras: nada choca con ellos),
  alrededor de su origen; el ×2 del original (0x76F2DB) va al **arrastre**, no a la inercia; el contador de reposo usa
  la media altura de la malla de la roca de info.dat. Se crean dentro de `FragMesh::Impact`, antes de leer lo que queda
  (así un golpe que solo parte triángulos también suelta trozos). Un edificio olvida la roca que lo golpeó cuando esta
  se para o se coge, o cuando su cuerpo se rehace (fn_646D60, Abode::SetUpPhysOb).
- **Trozos**: fila 11, masa `30·área`, solo chocan con el suelo, no se pueden coger, duran 100 turnos por triángulo
  (sin fundido). Los finísimos (área < 0,4 R²) se borran al crearse. Uno grande (área > 9) que cae mientras el
  edificio sigue en pie se queda como escombro del edificio.
- Rareza del original: los escombros vuelven a contar como triángulos del edificio, y si tras un golpe el recuento
  llega a 1 la FragMesh se borra y la casa se dibuja entera. Se deja así a propósito (el usuario lo prefiere como el original, 2026-09-29).
- Pendiente: el polvo de cada trozo (una partícula por vértice), la reparación por los aldeanos (el dibujo
  "a medio construir" sobre los escombros), golpes de la criatura, alineamiento y agresor del pueblo, edificios en
  construcción (−0,2 por golpe).

## Sonidos, polvo y aspecto de los golpes

Código: `src/ECS/Physics/CollisionSounds.*`, `Dust.*`, `PartialBuild.*`. Informes `tmp_dis/physics/collision_sounds.md`
(tabla completa en `snd/full_matrix.md`) y `building_visuals.md`.

- **Sonido de choque** (`AttemptToAddSoundEvent` 0x6464F0), una vez por turno en cada cuerpo despierto con algo que lo
  golpeó o `F > 0,5·m·g`: tipo de colisión de cada lado (info `collideSound`; trozo = BUSH; DeadTree malla 406 =
  HOLLOW_WOOD; sin objeto = GROUND, o WATER en el mar), nivel por `g = impacto / (peso de info sin escalar · 9,81)`
  (3 si < 1,25, 1 si > 3, si no 2), y la muestra de `editor.sad` de la tabla del original, en 3D sobre el objeto. Una
  pareja no vuelve a sonar hasta dos turnos después. Una roca contra un edificio: suena el edificio.
- **Edificios**: golpe medio 423–425 y flojo 426–430 (`G_Rock_V_Ground_M/S`), derrumbe 398–406 (`G_Crash_Abode`), en 3D.
  (Corrige 431–447, que salían de leer la tabla del banco con una columna de desfase.)
- **Polvo**: al caer al suelo, 6 bocanadas de `data\blobs.raw` (filas 2–3), color 0x50806040, tamaño `min(2R, 5)`,
  ±2 m/s, vida 1 s de juego, crecen en 0,125 s y encogen hasta 0; en el mar color 0x28C8F0F4 + chapoteo + anillo;
  en celdas someras, anillo + polvo. Cada trozo de edificio suelta una por vértice (0x80706050, tamaño 2).
- **Silbido** (G_ROCKPAST, `InGame.sad` 69–73): un cuerpo que entra a más de 20 m/s en la esfera de 10 m de la cámara.
- **Edificio golpeado a medio construir**: sobre su FragMesh se dibuja el modelo intacto recortado a
  `pos.y + pct·alto` (pct = `(vida − s)/(1 − s)`, s = 1,1·vida − 0,1 al golpear: 1/11), con pared interior a 0,35 (0,2 si
  el material es de dos caras), tapa en el corte y el andamio (la submalla de mayor status) saliendo de la tierra; nada
  si el corte queda por debajo de 0,2. Sin reparación por los aldeanos, se queda así.
- **Sombras**: los trozos no proyectan; el edificio roto mantiene la sombra estática de su modelo intacto.
- **Pendiente** (dependen de sistemas que aún no existen): nieve sobre la FragMesh (tormentas del clima, mapa de nieve)
  y carbonizado/brillo por fuego; el color 0,75 de la tapa.

## Predicados de celda del mar (`src/ECS/SeaCells.{h,cpp}`)

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

## El agua en los golpes y al soltar

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
- **Soltar (suave o lanzando): `Object::InitialisePhysicsFromHand` 0x636F00** (código emparejado en bw1-decomp
  `src/Black/Object.cpp:447`), portado entero en `HandSystem::InitialisePhysicsFromHand` (HandHolding.cpp). Todo
  soltar pasa por aquí: el paquete 0x12 llama a `ApplyThisToMapCoord` (árbol sobre almacén de madera → el almacén se
  lo queda, 0x74BFD0) y luego `ThrowObjectFromHand(status, 0)` 0x6385E0 con la **velocidad del muelle** (no cero):
  1. `PhysicsObject::AddObject(obj, v, 0, NULL, status)`; `lanzado = v.x² + v.z² > 4` (> 1 si lo tira una criatura).
  2. `AdjustToGroundLevel(lanzado, !IsAnyKindOfTree)` 0x7FCB80 (sin lanzar: baja el cuerpo hasta que su vértice más
     bajo toca el suelo, alineado a la normal salvo los árboles; lanzado: solo lo saca del suelo), `ZeroForces`,
     **`RaiseUntilNotIntersecting`** 0x644800 y la bandera FROM_HAND (4).
  3. `aterriza` si no se lanzó, **no hubo que subirlo** (la y del cuerpo no cambió; excepción: aldeano de un jugador
     del ordenador) y `IsDryLand || altitud de la celda redondeada (fistp) > 1`. `Living` y `Fence` además necesitan
     normal.y ≥ 0,7 (0x6372A6). `IsFence` 0x609110 = `MobileStatic` con malla 0x38 (valla americana) o 0x51/0x52
     (vallas celtas).
  4. Aterriza → bandera LANDED (8). `Living`, `Fence` o árbol (sin fuego) en `IsLand` salen de la física **en el
     acto** con `RemoveObject(obj, 1, 1)`: toman la pose del cuerpo y corre su `EndPhysics` (aldeano: LANDED o se
     ahoga; animal: LANDED; árbol: replantado, ver objetos). Un árbol **inclinado** en la mano (|x| o |z| de
     `GetYXZ` > 0,2) o con `dont_replant` se queda en física **sin** LANDED. Todo lo demás (rocas, estatuas, vasijas
     lanzadas…) **sigue en física con LANDED**, apoyado y alineado, hasta que se para solo (~1 s una roca en llano).
  5. No aterriza (el mar, fuera del mapa, subido encima de algo, lanzado) → sigue volando o cae;
     `Villager::CreateDroppedResource` 0x750940 (suelta el tronco que lleva: pendiente, openblack no lleva madera),
     `Reaction::CreateReaction(obj, 9 REACT_TO_FLYING_OBJECT)` (pendiente, sin reacciones) y
     `Creature::CheckAllCreaturesForCatching` (pendiente, criatura).
  - **Vasija de la mano** (`Pot::InitialisePhysicsFromHand` 0x66DF00): con |v|² ≤ 5 (los tres ejes) no hay física:
    `StartMultiPutdown` (partículas), `Pot::AddResourceToPos` (se funde con montones y almacenes, **se pierde en el
    agua**), `GoolooGooloo` y `ToBeDeleted`; más rápida vuela como cualquier objeto.
  - `RaiseUntilNotIntersecting` 0x644800: mete como cuerpos en reposo (fn_00644DF0) los objetos de las celdas bajo
    el cuadrado C ± R que `InteractsWithPhysicsObjects`, y sube el cuerpo lo que diga
    max(fn_007FDD60(él, otro, (0,−1,0)), fn_007FDD60(otro, él, (0,1,0))) con cada cuerpo cuyas esferas se solapan,
    repitiendo mientras alguno empuje > 0,001. fn_007FDD60 = el mayor `dot(q − hit, dir)` de los vértices q cuyo
    rayo hacia atrás (fn_007FC310, t < 0 sin límite, normal sin normalizar con umbral −0,0001) encuentra una cara
    del otro. Un aldeano no se sube sobre algo empujado por un `Living` (bandera 2, `Object::PushObject` 0x6396BA).
  - `RemoveObject(obj, 1, 1)` 0x646A00: ángulos y posición del cuerpo, `EndPhysics`, y si LANDED y `IsLand`,
    `DropSfx` (vt+0x794: 0 salvo `Tree::DropSfx` 0x74BC60 = G_PlantTree_01 + tick % 3, que openblack toca al
    replantar); `Tree::EndPhysics` 0x74B830 solo replanta con LANDED en tierra y sin fuego, si no, árbol muerto.
  - Gancho: `OPENBLACK_HAND_TEST_DROP` (tipos 4 árbol, 5 animal añadidos). Comprobado en Land1 (1788,4; 2710):
    roca → en física con LANDED y en reposo a los 0,9 s; árbol → replantado; aldeano y animal → fuera de la física
    en el acto; vasija → montón; aldeano en el mar (1464; 2016) → se hunde y DROWNING 600 turnos; roca sobre un
    edificio en (1780,4; 2713,3) → subida a y 38,3, sin aterrizar.
- **Recursos que caen al mar** (`Pot::AddResourceToPos` 0x66F270): fuera del mapa no hace nada, y lo que sobra tras
  fundirse con montones/almacenes **se pierde** si la celda es de agua (0x66F42D): ni montón nuevo ni sonido.
  openblack: una sola regla, `ecs::pot_resource::AddResourceToPos` (`ECS/PotResource.cpp`, exacta a 0x66F270, ver
  [magic.md](magic.md)); la mano (`HandResources.cpp`) ya no tiene regla propia y `pot_resource::IsWater` llama a
  `ecs::sea_cells::IsWater` (única copia de `MapCoords::IsWater` 0x6035B0).

## Hundirse, ahogarse y borrarse (`src/ECS/VillagerDrowning.{h,cpp}`, `src/ECS/ToBeDeleted.{h,cpp}`)

- **`HasSunk`** (vt +0x7B8), preguntado en cada subpaso desde 0x645A01 cuando el cuerpo está despierto, su centro está
  por debajo de `R/2` y su **densidad > 1**; si dice que sí, el cuerpo se para (v = 0, L = 0) y se ejecuta `EndPhysics`
  como si hubiera quedado en reposo (código 2):
  - `Object::HasSunk` 0x637470 → **no**: rocas, árboles, vasijas, montones y trozos siguen bajando hasta `T.y < −4R`
    (código 4) y ahí se **borran** con el `ToBeDeleted(0)` de su clase. Tiempos medidos: roca al momento, aldeano ~4
    turnos, vasija de ofrenda ~64, animal ~75, objeto normal ~150, árbol ~194, vasija ~298, balón ~525.
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
  [water-queries.md](water-queries.md#3-el-agua-en-los-guiones-chl).
- **Pendiente (`TODO(villager-death)`)**: `VillagerDead` 0x7506C0 de verdad (alineamiento por motivo, contadores del
  pueblo, textos de guía, madera/comida que suelta) y el estado DEAD (`Villager::Dead` 0x76A5E0): borra el fuego,
  `CreateSmokyStuff` y, **solo fuera del agua**, `fn_00828790` = un registro de 12 bytes (lista 0xEB9A7C) con un
  `LH3DObject` nuevo de la malla del aldeano (la de niño, `GVillagerInfo`+0x204, por debajo de la edad +0x138) que
  toca el clip del **alma** P_DEAD1/2_GOTO_HEAVEN o _HELL (244/245 o 247/248: la pareja según el clip "M_P_DEAD1",
  cielo o infierno al 50 %); luego la malla del aldeano pasa a ser la 0x1FF `PersonSkeletonMale` ([0xDCB164]). En el
  agua: humo y esqueleto, sin alma. Hoy, al llegar el contador a 0, el aldeano se borra con `ToBeDeleted`.

## Rocas que se parten (Rock::SplitInTwo 0x6E7560)

- Una roca con radio 2D > 3,6 no se puede coger: **pulsar sobre ella la golpea al momento**. Una roca que sí se puede
  coger se golpea con un clic corto (< 225 ms). Condición: altura > 0,7. Sin contador de golpes.
- Salen dos rocas del mismo tipo, escala × 0,7935 (∛½: la mitad de volumen), solo el ángulo Y, en
  `Pos ± (cos a, 0, sin a)·0,7935·R2D` con `a` al azar; la original se borra y las mitades entran en física (caen o
  siguen volando con su velocidad). Sonido G_RockTap_01..04 en rotación, en la mano.
- Los impactos fuertes también las parten (ver daño).

## Ganchos de prueba

`OPENBLACK_TEST_PHYSICS="x,z,altura,vx,vy,vz[,escala[,n]]"`, `OPENBLACK_TEST_HIT_VILLAGER="velocidad[,escala[,índice]]"`,
`OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz"`, `OPENBLACK_TEST_HIT_ABODE="velocidad[,escala[,índice[,n]]]"` (n rocas contra una casa, una cada 1,5 s), `OPENBLACK_HAND_TEST_SPLIT="x,z,escala,rondas"`,
`OPENBLACK_PHYSICS_TRACE=1` (posición, velocidad, contactos, G, densidad y radio de cada cuerpo por turno),
`OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (tipo = `villager|animal|tree|pot|rock`: lo crea a esa altura sobre el
punto y lo mete en física sin velocidad; escribe la celda, la densidad y `GET_LAND_HEIGHT` ahí y en la tierra de
referencia, y traza el contador del aldeano que se ahoga cada 100 turnos),
`OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"` (la mano sujeta una roca 0, una vasija de 300 de comida 1 o de madera 2,
el primer aldeano 3, el primer árbol 4 o el primer animal 5, y lo **suelta suave** en (x, z) tras esos segundos de juego; el log dice si acabó en física).

## Pendiente

- Nieve sobre la FragMesh (necesita el clima: tormentas de nieve y el mapa de nieve 128×128) y carbonizado/brillo por
  fuego (necesita el sistema de fuego); el color 0,75 de la tapa del "a medio construir".
- Edificios: reparación por los aldeanos (sitio de construcción, madera), golpes de la criatura, alineamiento y agresor
  del pueblo, que salgan los habitantes al bajar de 0,75, edificios a medio construir (−0,2 por golpe).
- Aldeanos y animales al aterrizar: las tres posturas del original y los cadáveres (hoy se levantan o desaparecen).
- Muerte de aldeanos completa (`VillagerDead` 0x7506C0) y el mimetismo de la criatura al hundirse algo que soltó el jugador.
- De soltar: la velocidad angular de la mano (`ThrowAngularVelocity`, 0 en openblack), el sonido de discípulo
  (`MakeDiscipleSFX`) y `SetVillagerDisciple`, la reacción 9, el tronco del aldeano y lo de la criatura (atrapar,
  imitar, juguetes). La rama de malla de fn_007FDD60 (fn_008683C0) no hace falta: ningún cuerpo de openblack la usa.
  La bandera `Tree`+0x5C & 2 de `Tree::EndPhysics` (0x74B882: solo `Fixed::EndPhysics`, ni replantar ni árbol
  muerto) la pone únicamente el constructor de `MagicTree` (0x5FCF8D, `or byte [esi+0x5C], 2`; barrido de todo
  `.text` en `tmp_dis/agua/re/scan_tree5c.py`): son los árboles del milagro del bosque, que este árbol no tiene.
- La normal del terreno sin la cuantización del original.
- Tooltip "Golpear para Romper" (0xEF7) y comprobar la influencia del jugador antes de golpear una roca.
- Mod **better physics** (pedido por el usuario, desactivado por defecto): trozos de edificio con malla de colisión y que
  se puedan agarrar; el motor sigue fiel al original.
