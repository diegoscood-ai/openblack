# Árboles y bosques

Todo lo de los árboles: arrancar y el tirón, qué se puede coger, soltar y replantar, la madera (info.dat y la API para
los oficios de aldeano), las búsquedas de árboles y bosques, el crecimiento, el dibujado, el fuego propio del árbol y el
sacrificio. Todo es **fiel** (leído en runblack.exe o en info.dat) salvo lo marcado **(inferido)**, **(aproximado)**,
[supuesto], *desviación* o **pendiente**. Código de openblack: `src/ECS/Trees.{h,cpp}`, `HandTrees.cpp`.

- [Arrancar y coger](#arrancar-y-coger)
  - [Tirón](#tirón-handstatetug-enter-0x5b7df0--update-0x5b8070-en-handtreescpp)
  - [Reglas de coger y BigForest](#reglas-de-coger-y-bigforest)
- [Soltar y replantar](#soltar-y-replantar)
- [Alineación](#alineación)
- [Madera y tabla GTreeInfo](#madera-y-tabla-gtreeinfo)
- [Árboles para los oficios de aldeano](#árboles-para-los-oficios-de-aldeano-api-srcecstreesh-para-la-sesión-de-aldeanos)
- [Búsquedas de árboles y bosques](#búsquedas-de-árboles-y-bosques-para-los-aldeanos-informe-tmp_distrees2villager_queriesmd)
- [Crecimiento y bosques](#crecimiento-treeprocess-0x74a290-treegrow-0x74a3f0)
- [Dibujado](#dibujado)
- [Fuego](#fuego)
- [Sacrificio](#sacrificio)
- [Pendiente](#pendiente) · [Ganchos de prueba](#ganchos-de-prueba) · [Fuentes](#fuentes)

## Arrancar y coger

- Arrancar: el tirón empieza **al pulsar** (StartGrab 0x5D1740 llama a `CHand::PickUp(obj, 1)` en el momento; el umbral
  de 225 ms es solo para los demás objetos); el árbol se inclina hacia la mano y sale cuando la mano se ha movido más de
  peso/1000 m en horizontal desde donde lo agarró (detalle y diferencia con el original en
  [Tirón](#tirón-handstatetug-enter-0x5b7df0--update-0x5b8070-en-handtreescpp)). Sonido TreeBreak, montón de raíces
  (malla 593, 15 s) y raíces colgando (malla 592).
- El montón de raíces (el cráter) es un `LH3DObject::Create(1)`, **morfable** (fn_00825240 → UpdateMelting vt+0x1E8 una
  vez al crearlo): se amolda al terreno como los campos y almacenes (`MorphWithTerrain` en `HandSystem::Uproot`).

### Tirón (`HandStateTug` Enter 0x5B7DF0 / Update 0x5B8070, en HandTrees.cpp)

- **Original**: al empezar, el ancla es la base del
  árbol y el plano de arrastre pasa por ella con la normal del terreno, a la altura de la mano vista a la distancia del
  ancla. Tras 0,13 s (el fundido del cambio de estado), cada fotograma la mano va al corte del rayo del ratón con ese
  plano; el agarre está en `base + arriba × bajada` (bajada = 0,1 × altura, mínimo 3,2 × escala de la mano × 0,3 al
  empezar); un muelle `F = 1000 × (mano − agarre)` (tope 600000) lo inclina con par `(r × F)/1000` y rozamiento
  cuadrático 4 alrededor de la base, y el tronco se estira hasta ×1,3 (Zoomer 0,3 s). Sale cuando `|F| > GetWeight`
  (escala³ × peso de info.dat): agarrado lejos del punto de agarre, sale enseguida. Soltado antes, vuelve a su postura.
  Al final de cada Update (también en los primeros 0,13 s) la mano se coloca en el agarre del tronco estirado
  (`CHand+0x78 = matriz × (0, bajada, 0)`); solo se dibuja ahí, el siguiente Update la vuelve a poner en el plano.
  **Consecuencia (2026-09-30, por confirmar con el original)**: como la mano antes de pulsar está sobre el rayo del
  ratón, el plano queda a la altura a la que ese rayo cruza el eje del árbol, así que el primer tirón es esa altura menos
  la bajada: una haya de escala 1 (18 m, agarre a 1,8 m, peso 1000) solo se inclina si se pulsa a menos de ~1 m del
  agarre (de 0,8 a 2,8 m sobre la base); pulsada en la copa sale a los 0,13 s. El openblack de antes de las físicas
  medía solo la distancia horizontal del cursor a la base y se inclinaba pulsara donde pulsara.
  **Lo que hace openblack (2026-09-30, a petición del usuario, que recuerda el original así)**: no se usa el muelle
  literal (además el estirado ×1,3 hacía que el árbol subiera y bajara). Al pulsar se guarda el punto agarrado y su
  distancia en el rayo del ratón; el tirón es cuánto se ha movido en horizontal la mano (el rayo a esa distancia) desde
  entonces. El árbol se inclina hacia ella hasta 0,25 rad y sale cuando pasa de peso/1000 m (escala³ × peso de
  info.dat). Agarrado en cualquier sitio y sin mover el ratón, no sale. Si algún día se puede probar el original, se
  puede comprobar con `tmp_dis\trees2\tugwatch.py` (lee la memoria de runblack.exe: plano, mano, agarre, estado).
  Gancho: `OPENBLACK_TEST_TUG="x,z,espera,mantener"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`, el cursor se mueve 0,5 s
  después), trazas con `OPENBLACK_HAND_TRACE=1`.

### Reglas de coger y BigForest

- **Reglas de coger**: `Tree::ValidForPlaceInHand` = 1 e `IsTuggable` = 1 para los 22 tipos, a cualquier escala (arbustos,
  setos, palmeras, bosquecillos, dentro o fuera de pueblos). Solo lo impiden la bandera 0x2000 (partidas guardadas y
  puzles), estar fuera de la influencia o una selección bloqueada; entonces va por el camino de "tocar", que para
  árboles no hace nada. `BigForest` (portado): no se tira; al agarrar (225 ms) `InterfaceSetInMagicHand` 0x4393C0
  hace `RemoveResource(WOOD, 350)` (madera del Conifer) y pone en la mano un Conifer nuevo (escala 1, ángulo 0).
  `RemoveResource` 0x4390D0: la madera del bosque (+0x84; al crearlo woodValue × escala, `Create` 0x438EC0) baja 350 y
  **solo** cuando su madera se aleja más de 250 de vida × escala × woodValue se reescala a madera/woodValue y planta en el
  borde; sin madera suficiente da lo que queda y el bosque se borra (detalle en
  [Búsquedas de árboles y bosques](#búsquedas-de-árboles-y-bosques-para-los-aldeanos-informe-tmp_distrees2villager_queriesmd)).
  `AddTreeAround` 0x439220: hasta 10 ángulos al azar a su radio; en tierra y sin objeto de la celda con distancia + radio
  menor de 4, un Pine de su bosque (+0x80), escala 0,05, ángulo al azar y tamaño máximo 0,75 + azar(0,5). openblack:
  `HandSystem::TakeTreeFromForest` (HandTrees.cpp), `BigForest::wood`, gancho `OPENBLACK_HAND_TEST_FOREST=1` (Land1:
  15000 → 14650, escala 0,977). DeadTree/FelledTree: se cogen sin tirón. Arrancar: `G_TREEBREAK` + 1 empujón de
  alineación malvada (`GAlignment::Update`); replantar, bueno ([Alineación](#alineación)).

## Soltar y replantar

- **Soltar** (`Object::InitialisePhysicsFromHand` 0x636F00 + `Tree::EndPhysics` 0x74B830): el árbol cuenta como «dejado
  con cuidado» (bandera 8 LANDED del objeto físico) si no se lanza y no hubo que subirlo (`IsDryLand` o altitud de la
  celda > 1). La prueba de la normal (y < 0,7 ⇒ no aterriza) es **solo** para seres vivos y vallas, no para árboles
  (bw1-decomp `src/Black/Object.cpp:539`; la versión anterior de esta nota la aplicaba también a los árboles). Un árbol
  LANDED sin `FireEffect` sobre `IsLand` sale de la física en el acto si llega casi derecho (los ángulos x y z de su
  matriz YXZ ≤ 0,2 rad ≈ 11,5°; un árbol en la mano toma el «arriba» de la mano, que sigue la superficie, así que en una
  ladera va inclinado) y `Tree::EndPhysics` lo replanta; inclinado (o `dont_replant`) sigue en física sin LANDED, cae y
  acaba como árbol muerto. Caliente o ardiendo, o LANDED en una celda de agua: sigue en física con LANDED y al pararse
  `Tree::EndPhysics` lo hace árbol muerto (conserva su fuego). Todo pasa por las físicas
  (`HandSystem::InitialisePhysicsFromHand`, `HandPhysics.cpp`). «Derecho» = `LHMatrix::GetYXZ` 0x7FAB30 de su matriz
  con |x| ≤ 0,2 y |z| ≤ 0,2 rad (x = asin(fila2.y), z = atan2(−fila0.y, fila1.y), comprobado emulando). Soltado sobre el mar (ni `IsDryLand` ni altitud de la celda > 1)
  **no aterriza**: flota ~19 s hasta hundirse (ver [physics.md](physics.md#el-agua-en-los-golpes-y-al-soltar)). El sonido
  (`Tree::DropSfx` 0x74BC60, G_PLANTTREE + tick%3) lo lanza `PhysicsObject::RemoveObject` 0x646B44 en **todo** soltado
  con cuidado que acabe en tierra, replantado o no. Lanzado = árbol muerto siempre.
- **Bosque al replantar** (0x74B8BF): espiral por las celdas del mapa hasta 25 + 10 m; por cada objeto fijo
  `d = distancia − su radio 2D`. Un objeto de un pueblo (o parte del templo) a menos de 25 m ⇒ el árbol es «de pueblo»
  (bit 1 de +0x5E = `isNonScenic`, ¡se pone a **1** dentro del pueblo!) y se une al bosque **del pueblo**, que gana a
  cualquier otro; si no, hereda el bosque del árbol con bosque más cercano (sin límite propio, solo los 35 m de la
  búsqueda); sin ninguno y fuera de pueblo, crea un bosque nuevo. Efectos: humo blanco `SmokyStuff` en el suelo (en
  openblack, el polvo del agarre), `SPOT_VISUAL_FOREST_CREATED` (0x2C) **siempre que no sea en pueblo**,
  `StartImmersion(0x2E)` y mímica de criatura (sin portar) y alineación buena (ver [Alineación](#alineación)).
  El original saca el bosque del pueblo de una lista que el pueblo guarda (Town +0x608): el último bosque escénico de
  la lista (detalle en «Replantar en un pueblo», en
  [Búsquedas](#búsquedas-de-árboles-y-bosques-para-los-aldeanos-informe-tmp_distrees2villager_queriesmd)); en un pueblo
  sin bosque escénico se queda el bosque del árbol más cercano y, sin ninguno, el árbol queda **sin bosque**. openblack
  ya lo hace igual (`ecs::TownForestId` recorre esa lista, `HandTrees.cpp`). *Antes* openblack no modelaba la lista y
  el primer árbol plantado en un pueblo creaba su bosque.

## Alineación

Arrancar un árbol con la mano es un acto **malo** (`Tree::InterfaceSetInMagicHand`) y replantarlo (`Tree::EndPhysics`)
o el árbol que planta el agua (`Tree::ApplyWaterSpell`) son **buenos**: ±`treePullPutAlignmentChange` por
`GAlignment::Update` 0x4145A0. El valor (`GAlignment`, GPlayer +0x60), el peso por la alineación actual, el ritmo por
turno, los guiones y lo que falta están en
[magic.md](magic.md#alineación-del-jugador-galignment-gplayer-0x60-srcecseffectsalignment-componentsplayeralignment)
(`src/ECS/Effects/Alignment.*`, `components::PlayerAlignment`).

## Madera y tabla GTreeInfo

- Sobre un almacén = madera `woodValue·escala·GLandBalance[5]` (`Tree::GetDefaultResource` 0x74B7A0, × vida). Un **árbol
  muerto** da menos: `DeadTree::GetDefaultResource` 0x511330 = `woodValue·escala` sin vida ni balance de tierra.
- Tabla GTreeInfo (info.dat, runtime = registro + 0x10, paso 0x140): madera 800 Oak (roble), 700 Beech/Cedar/Copse
  (haya, cedro, bosquecillo), 500 Birch/Olive (abedul, olivo), 400 Cypress (ciprés), 350 Conifer/Pine (conífera, pino),
  300 palmeras, 100 setos, 15 arbustos; peso 1000 (arbustos 20, setos 100);
  capacidad calorífica 1000 (arbustos 100, setos 200); sacrificio 400/500/1000 (Oak)/250/350/100/200/110; temperatura
  de combustión 110 para todos.

## Árboles para los oficios de aldeano (API `src/ECS/Trees.h`, para la sesión de aldeanos)

- **Borrar** (`DeleteTree` = `Tree::ToBeDeleted` 0x74A210 / `DeadTree::ToBeDeleted` 0x510C90): fuera del bosque y de las
  físicas, avisa a los oyentes (`AddTreeDeletedListener`: fuego, reacciones, mano) y se borra. `DeleteForest` =
  `Forest::ToBeDeleted` 0x539C60: borra cada árbol de sus dos listas y sale de la lista de bosques (también el bosque
  vacío a los 2000 turnos). `ShrinkAllTrees` usa `DeleteTree` para el árbol que llegaría a 0 (fn_0074A3A0).
  El borrado genérico `ecs::ToBeDeleted` (`src/ECS/ToBeDeleted.cpp`; lo usan las físicas, p. ej. código 4 = hundido
  en el mar) también manda Tree y DeadTree a `DeleteTree` (b6cbcc74).
- **Madera**: `TreeWoodValue` = `Tree::GetWoodValue` 0x74B7B0 (vida × 1 × woodValue × escala × GLandBalance[5]) o
  `DeadTree::GetWoodValue` 0x511AD0 (vida × woodValue × escala³: el original eleva la escala al cubo ahí); `TreeWood` =
  `GetDefaultResource(WOOD)`: `Tree` 0x74B7A0 = (int)GetWoodValue, `DeadTree` 0x511330 = (int)(woodValue × 1 × escala), lo
  que recibe un almacén (`DepositInStore` lo usa).
- **Quitar madera a un tronco** (`RemoveWood` = `DeadTree::RemoveResource` 0x511370): si le quedan ≤ n, se borra y da lo
  que tenía; si no, **encoge**: `escala = (madera − n)/(woodValue × multiplicador 1)`. Su recurso es su
  `GetDefaultResource` (`Object::GetResource` 0x639520: el suyo si el tipo coincide, 0 si no). Comprobado:
  haya muerta de escala 1, 700 → quitar 100 → 600, escala 0,857.
- **Tipo de tronco al cargarlo** (`TreeCarriedType`): `Tree::GetCarriedTreeType` 0x55D900 = `carriedType` de info.dat;
  `DeadTree::GetCarriedTreeType` 0x511A20 = 0-3 si su malla es uno de los 4 troncos de `CarriedObject::Init` 0x462600
  (MeshPack 406, 347, 348, 349), si no el `carriedType` de su árbol (haya = 3, madera dura).
- **Talar** (`FellTree` = `FelledTree::Create` 0x5116A0, que solo llama `Villager::ForesterChopsTree` 0x75FAC0): el árbol
  pasa a `DeadTree` + `FelledTree` con su malla (sin soltar las raíces: esa bandera solo la pone `Tree::EndPhysics`) y
  entra en las físicas lanzado por el leñador: `k = 0,4 × altura × 0,5`, `a = atan2(x, −z)` de la dirección
  leñador→árbol (fn_007FAA50; 0 si mide menos de √0,001), velocidad `(sin a, 0, −cos a)·k` (a lo largo de esa
  dirección), giro `0,4·(cos a, 0, sin a)` rad/s **en espacio del cuerpo** (`PhysicsObject::AddObject` 0x6443A0 hace
  `L = Σ (w·I)_i · fila_i` con la matriz del árbol, con su giro Y): en mundo `R·(cos a, 0, sin a)·0,4`, así que cómo cae
  depende de la orientación del árbol. En openblack el eje va **negado**: `PhysOb::Integrate` 0x7FE260 gira las filas con
  `R(ŵ, ángulo)`, que en el `PhysOb` diestro de openblack es girar −ángulo (`tmp_dis\physics\physob.md`, «Sign
  convention»). Después `Villager::ForesterChopsTree` borra el árbol (`ToBeDeleted`): en openblack es la misma entidad,
  así que se avisa a los oyentes de borrado y el fuego pasa al tronco (fn_00730960). Luego `PhysOb::AdjustToGroundLevel(false, true)`.
  **Sin portar**: `flags |= 2` y `+0x1A4 = 2` del objeto físico (sin identificar), `RaiseUntilNotIntersecting` 0x644800 y
  las dos reacciones 0x0C («aquí hay madera»: una del constructor de DeadTree 0x510957 y otra de `FelledTree::Create`
  0x511889; `FelledTree::EndPhysics` 0x511970 no añade la del posarse). `FelledTree::Draw`
  0x511990 añade el tronco al dibujo dos veces sin fuego (falta un `return` en el original): sin efecto visible.
  Gancho `OPENBLACK_TEST_FELL="x,z"`.

## Búsquedas de árboles y bosques para los aldeanos (informe `tmp_dis\trees2\villager_queries.md`)

- **Árboles de una celda** (`TreesInCell`; `MapCoords::FindType(6)` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0): el
  tipo 6 (`OBJECT_TYPE_FOREST_TREE`) va en la lista de **fijos** de la celda (MapCell +4), y `Fixed::InsertMapObjectToCell`
  0x52DEA0 mete cada objeto **en cabeza**: el primero es el último insertado. openblack no tiene listas por celda:
  `Tree::mapInsertion` guarda ese orden (al crear el árbol y al replantarlo, `InsertMapObject` en `Fixed::EndPhysics`).
- **Buscar árbol para talar** (`FindTreeNearVillager` = `Villager::FindTreeNearVillager` 0x75FD00): las 9 celdas de
  alrededor en el orden de `GUtils::Spiral` (0x74D7E0, tabla 0xDA59FC, empezando con dir 1 y pasos 1: (0,0) (−1,0)
  (−1,−1) (0,−1) (1,−1) (1,0) (1,1) (0,1) (−1,1)), en cada una **solo el primer árbol** que no sea
  INDESTRUCTIBLE (bit 0x4000 de +0x24: solo lo ponen los objetos de puzle y `LandscapeVortexOut`; ningún árbol en una
  partida normal); el más cercano por `Dist2D(aldeano, posición de trabajo)` desde 99999. Sin más reglas: ni distancia
  máxima, ni el bit «de pueblo» (+0x5E & 2), ni tamaño, ni bosque. El original devuelve 0/1/10 (10 = ya lo toca,
  `IsTouching`): eso lo decide el lado del aldeano.
- **Posición de trabajo** (`TreeWorkingPos` = `Tree::GetWorkingPos` 0x74C040): la del árbol más, hacia el aldeano,
  `Get2DRadius(aldeano) + 0,9` (0x8C5844). El radio es el **del aldeano**. `Object2DRadius` = `Object::Get2DRadius`
  0x638180 = escala × max(semiejes x, z de su caja).
- **Bosques** (Forest, 0x58 bytes): +0x34 cuenta atrás de vacío, +0x38 BigForest, +0x3C = 1 bosque «de pueblo»
  (escénico), +0x40 id. Un **BigForest** tiene su Forest (su ctor 0x438CE0 lo crea en +0x80 y le pone +0x38): ahora en
  openblack también, así que el Conifer que da al cogerlo y el Pine que planta en su borde son de ese bosque.
  `Forest::Process`: solo cuenta como vacío sin BigForest y sin árboles; **un bosque escénico no se procesa** (sus árboles
  no crecen y no planta).
- **Bosque escénico del pueblo** (`MakeScenicForest` = `Town::MakeScenicForest` 0x741B40): toma los árboles a menos de
  250 + 10 m del centro del pueblo que no tienen bosque, o cuyo bosque es escénico y están más cerca del centro del
  pueblo que del de ese bosque (distancias 2D); si el pueblo no tenía, lo crea en el centro **solo si hay algún árbol**. Las
  celdas son las de la espiral de `GUtils::Spiral` desde la celda del centro, que para en la primera celda a más de R
  (1369 celdas, radio de Chebyshev 18: no todo el disco de 260 m).
- **Lista de bosques del pueblo** (`AssignForestsToTown` = `Town::AssignForestsToTown` 0x73EB00, Town +0x608): se vacía y se
  llena con cada bosque cuyo punto más cercano (el borde de su BigForest o su centro, fn_0053ADB0) está a menos de
  `GTownInfo::maxDistanceForTownForest` (250, +0x164) del almacén (o del punto temporal) y que tiene madera
  (`ForestWood` = fn_0053B280: la de su BigForest más la de cada árbol). La llama `Town::AsssignTownFeature` 0x73EAC0 (para
  cada pueblo, tras `MakeScenicForest`) y `Scaffold::BuildBuilding`; no se toca al crear o replantar árboles.
  El borde es `Object::GetNearestEdgeToPos` 0x636DA0 (vt+0x83C de BigForest): pos + GetPosFromAngle(ángulo hacia
  `pos`, Get2DRadius).
- **Replantar en un pueblo** (`TownForestId`): el árbol se une al **último bosque escénico** de la lista del pueblo
  (`Tree::EndPhysics` 0x74BA2B); si no hay ninguno se queda con el bosque del árbol más cercano ya encontrado, y sin
  ninguno de los dos, **sin bosque** (antes openblack creaba uno).
- **Bosque más cercano** (`FindNearestForestToPos` = `Town::FindNearestForestToPos` 0x73EC10): en la lista del pueblo, el
  de punto más cercano (0 si se está dentro del radio del BigForest) a menos de 250; gana uno no escénico, el escénico solo
  si no hay otro. `FindForest(pos, max, soloVacíos)` = fn_0053A1A0: por la lista global, el de **centro** más cercano
  (vacío = sin árboles y sin BigForest). `ForestCentreTree` = `Forest::GetForestCentreTree` 0x53ABF0.
- **BigForest para los leñadores**: `BigForestArrivePos` = `GetArrivePos` 0x439360 (su posición más, hacia el aldeano,
  0,5 × su radio 2D); los árboles en la mano o en vuelo no están en ninguna celda (en el original salen del mapa);
  `BigForestRemoveWood` = `RemoveResource` 0x4390D0: pide n / vida; si no llega, da lo que tiene y
  se borra; si llega, resta y **solo cuando** su madera se aleja más de 250,0 (0x8C6210) de vida × escala × woodValue se
  reescala y planta un Pine en el borde (`AddTreeAround` 0x439220: tamaño 0,05, máximo **0,75** (0x8AC3F8) + azar(0,5);
  antes openblack ponía 0,5). La mano usa lo mismo (350 por árbol, así que siempre reescala). Ganchos
  `OPENBLACK_TEST_TREE_QUERIES="x,z"` y `OPENBLACK_HAND_TEST_FOREST=1`.
- **Quién llama a `MakeScenicForest` y `AssignForestsToTown`**: en el original, `Town::AsssignTownFeature` (carga del mapa) y
  los edificios terminados; en openblack, de momento nadie (la sesión de aldeanos lo enganchará; solo el gancho
  `OPENBLACK_TEST_TREE_QUERIES`): hasta entonces los pueblos no tienen lista de bosques y un árbol replantado en un pueblo
  queda sin bosque. **Pendiente**.

## Crecimiento (`Tree::Process` 0x74A290, `Tree::Grow` 0x74A3F0)

- Solo crecen los árboles **de un bosque**: en el original únicamente `Forest::Process` 0x539DA0 recorre sus árboles, así
  que los árboles sueltos del guion (todos los de Land 1 y Land 2, que llevan bosque −1) no crecen nunca. Los de Land 3
  (65), Land 4 (82) y Land 5 (164) sí.
- Un árbol nace «creciendo» (bit 0 de +0x5E) solo si su `maxSize` es distinto del tamaño con el que se crea, y su
  contador (+0x60) arranca en un turno al azar de [0, growTurns) (ctor 0x749E00).
- Cada `growTurns` turnos (10 en los 22 tipos, o sea 1 s): `amt = growAmt · (1 + 0,01·rainMultiplier·lluvia) ·
  (1 + 0,5·alineación del terreno)`, y `escala = min(escala + amt, maxSize)`. `growAmt` 0,01 (0,02 conífera/pino, 0,005
  roble/olivo/palmera). Al llegar al máximo deja de crecer. `SetScale` es virtual y rehace la colisión: el círculo de
  obstáculo sigue al tamaño.
- openblack: `src/ECS/Trees.cpp` (`ProcessTreesTurn`, `GrowTree`), llamado desde el turno del mundo (`src/Magic/MagicLoop.cpp`). **Sin
  clima ni alineación de terreno todavía** (el clima ya existe en `src/ECS/Weather`, pero `GrowTree` aún no lo lee):
  lluvia 0 y alineación 0, así que `amt = growAmt`. Ganchos
  `OPENBLACK_TEST_TREE_GROWTH="x,z"` (dos brotes, uno con bosque y otro sin), `OPENBLACK_TREE_TRACE=1` (cada paso
  de crecimiento y el brillo) y `OPENBLACK_TEST_REPLANT="x,z,gradosDeInclinación"` (suelta un árbol ahí y dice si se
  replanta, cae o queda muerto).
- **Bosques** (`Forest`, ctor 0x539BD0; `ECS/Trees.cpp`): un bosque es un objeto con centro e id (CREATE_FOREST, o
  `new Forest(pos, 0)` al replantar fuera de todo bosque; id 0 = el siguiente libre). CREATE_TREE y CREATE_NEW_TREE
  buscan el id del guion en la lista de bosques y, si no existe, el árbol **no tiene bosque** (0x7162BE): los ids 0-6
  de Land5 y el −1 de Land1/Land2 quedan sin bosque. Cada turno (`Forest::Process` 0x539DA0): un bosque vacío espera
  2000 turnos y se borra; si no, crecen sus árboles y puede plantar uno nuevo: `r = 2000 + azar(1000)`,
  `f = min(1, 0,05·crecidos)`, `T` = turnos desde el último árbol que plantó cualquier bosque (global 0xCD04C8),
  `c` = intentos del bosque (+1 por turno); si `c·f·T/300 > r`, planta junto a uno de los `azar(n/2+1)` crecidos más
  cercanos a su centro (`Forest::CreateNewTree` 0x539FD0) y `c` vuelve a 0. **Plantar junto a un árbol**
  (fn_0053A010): 32 ángulos desde uno al azar (2π/32 entre ellos) × 5 radios (entero 5-9 al azar, luego `(r+2) % 10`),
  el primer sitio libre; el árbol nuevo es del tipo del padre, tamaño 0,1, máximo `0,8 + azar(0,4)` y ángulo al azar.
  «Libre» (fn_0074C180) = sin objeto fijo (círculo de 0,5) y en tierra; el original lee `(collide & 8) == 0 || IsWater`,
  la parte del agua parece invertida y se ha tomado como «no en agua» [supuesto]. Con un bosque de 20 árboles crecidos
  sale más o menos un árbol nuevo en el mundo cada minuto y medio (comprobado en Land3: el bosque 19 plantó uno).
- **Agua sobre un árbol** (`Tree::ApplyWaterSpell` 0x74C390, `ecs::ApplyWaterSpell`, para la sesión de milagros): uno
  que crece crece `waterMultiplier·growAmt`; con el subtipo de hechizo 0x17 también uno adulto, la mitad por
  `GetDistanceModifier(tamaño, 3)` (= `SigmoidThreshold(0,5, 1 − min(tamaño,3)/3)`, tabla de 41 pasos en 0xC23284), por
  encima de su máximo. Uno adulto de un bosque regado sin 0x17, pasados 40 turnos del último árbol del mundo, planta
  otro a su lado (el llamador da la alineación buena y la estadística 0xE). Suena 0x78 + tick%9 (`G_TreeGrow`).

## Dibujado

- **Mecido** (el mismo viento que los campos maduros, tabla `T0` y `Tree::PreDraw` 0x74A7C0 en
  [objects-and-resources.md](objects-and-resources.md#campos-field-informe-tmp_disfieldfield_notestxt)): los árboles
  (`Tree::Draw` 0x74B016) usan la misma tabla con factor 1: x de la columna 1 = 0, z = escala × T0[i], `i` = bits 2-5 de
  +0x5C; portado en RenderingSystem salvo con el árbol inclinado por la mano. La curva junto a lo que pasa cerca (bits
  6-9 de +0x5C, tabla 0xD19A48) ya está portada para la mano y los objetos físicos (ver «Curvado», abajo); falta solo la
  criatura (ranura 2), que aún no existe.
- **Ranura de viento**: `round(yAngle·16/2π) & 15` (0x74A0E7), guardada al crear el árbol, así que los árboles orientados
  igual se mecen juntos (`components::Tree::windSlot`; antes openblack usaba un hash de la entidad).
- **Brillo por cámara** (`Tree::PreDraw` 0x74A883 → global 0xC22FA0, leído solo por código de árboles):
  `d = normalize(foco de la cámara − posición de la luz)`, `v = normalize_xz(dirección de vista)`,
  `b = dot < 0 ? 200 : 200 + 55·dot`, y `Tree::Draw` 0x74B077 multiplica cada canal RGB del color del árbol por `b/256`
  (0,781 … 0,996). **Rareza del original**: LH3D tiene una sola luz puntual y de día su único `setter` es código muerto,
  así que la luz se queda en el origen del mapa (0,0,0); los árboles se oscurecen un 22 % cuando la cámara mira hacia esa
  esquina. Al ocaso y de noche (tipo de cielo > 0; `fn_005E5830`, que coloca la luz después de `Tree::PreDraw`, así
  que los árboles usan la del fotograma anterior) la luz va a 3 unidades de la **mano** hacia la cámara, con la mano
  subida al menos a 10 sobre el terreno: entonces `dot ≈ cos(inclinación de la cámara)` y los árboles se ven más claros
  (Land1, cámara típica: 200 a mediodía, 242 a las 20 h).
- **Color propio del árbol** (`fn_00802120` en `Tree::Draw`): es la misma luz bilineal de las 4 celdas bajo el origen
  que usan los demás objetos (`fn_00801C90`, mismas tablas 0xEDD90C y celdas +3/+0xB/+0x88/+0x90), solo que en entero
  (fracción de MapCoords >> 8) en vez de float; la neblina (`fn_007FEB30`) es la de todos los modelos. openblack ya lo
  hace igual en `vs_object`: no hay nada propio que portar.
  openblack: `ecs::TreeBrightness()` en `ECS/Trees.cpp`, aplicado como color propio en la w de la cuarta columna de la
  instancia (igual que el tinte de los campos), `RenderingSystem.cpp`.
- **Sonido ambiente de hojas** (0x74B111): los árboles de más de 10 de alto con la cámara a ≤ 10 en x y z (y < 18 en y)
  suenan ~1 vez por segundo (`LocalRand(1000/msFotograma) == 1`): fila `{*,*,20,*,70}` de `editor.sad` =
  `G_TreeRustle_01..11` + `G_TreeCreak_01/02`. openblack: `ecs::UpdateTrees` + `AnimationSounds::PlayFromTable`.
- **Curvado junto a lo que pasa cerca** (`Tree::Draw` 0x74AB8B, `fn_005DF1B0`, tabla 0xD19A48): cada fotograma se apuntan
  en la tabla el objeto que lleva la mano (ranura 1: todo objeto en la mano se «dibuja en la mano»), los objetos físicos
  en vuelo (ranuras 3-13 por turno, `fn_00646FE0`) y la criatura del jugador (ranura 2; aún no hay), cada uno con su
  posición y un radio = escala × la semidiagonal de su malla (LH3DMesh +0x30). Cada fuente marca los árboles de las 3 × 3
  celdas de 10 m a su alrededor (gana la última). Un árbol marcado se curva si su copa no está por debajo de la fuente
  (base + altura ≥ y de la fuente) y la distancia horizontal `d` es menor que el radio `r`:
  `ángulo = 0,471239 · (1 − ((r − 0,75)·d/r + 0,75)/r)` (27° como mucho), alrededor del eje horizontal perpendicular a la
  dirección fuente→árbol, la copa **alejándose** de la fuente; solo la matriz dibujada, y en ese fotograma sin vaivén.
  Sonido al empezar a curvarse: clave `{c, *, *, 10, 75}` de `editor.sad` con `c = 3` si la curva es < 0,3 (sin
  muestras), 2 si < 0,67, 1 si no: `G_Crash_Tree_M_01..08` («rubbing trees»). openblack: `ecs::UpdateTrees`
  (`UpdateTreeBends`, `Tree::bendAngle/bendDirection`) y `RenderingSystem.cpp`. Comprobado con una roca en la mano
  (`OPENBLACK_HAND_TEST_HOLD=1.5`): los árboles cercanos se apartan y suenan `G_Crash_Tree_M_05/07`.

## Fuego

El modelo de calor común a todos los objetos (`FireEffect`, `SpreadEffect.cpp`, `src/ECS/Fire`) está en
[magic.md](magic.md#fuego-m5-srcecsfire); la bola de fuego y el rayo, en
[miracles.md](miracles.md#bola-de-fuego-y-rayo-m5-magicobjectsmagicfireball-psysrulesfireballlightning). Aquí, lo que
toca al árbol (valores de la tabla GTreeInfo, arriba):

- **Fuego** (`SpreadEffect.cpp` / FireEffect en Object+0x44; turno 0,1 s; **fiel**, portado en `src/ECS/Fire`):
  temperatura T, Tc = max(110, 40); arde si T ≥ Tc. Ardiendo T += 0,1·T/(2Tc) hasta 2Tc; enfriando (T ≤ anterior)
  T −= (T + 10 − amb)·4·H·R·0,1·k/capacidad (k = 50 sobre agua con y < 2, 1 + 0,01·lluvia). Daño: vida −=
  (T − Tc)/Tc·0,001 por turno (muere en ~100 s; magic.md lo escribe `(T − Tc)/(2·Tc − Tc) · defenceMultiplierBurn ·
  0,1`, con defenceMultiplierBurn = 0,01 en los 22 tipos de árbol y en Tree Logs (info.dat,
  `tmp_dis\miracles\infodump\info_dump.txt`) y Tmax = 2·Tc: es la misma fórmula); carbonizado con vida < 0,6. A vida 0 el árbol desaparece. Contagio: cada turno busca en R + 10 m, R =
  1,25·radio2D·clamp((T − 0,8Tc)/1,2Tc); calor q = min(10·dT, 0,5·(Ts − amb)·cap_s) → el objetivo gana q/cap_t (los
  arbustos prenden ~10× antes). Un árbol ardiendo se puede coger y sigue ardiendo; sostenido sobre algo que arde, o
  lanzado, prende lo que toca; al caer se vuelve DeadTree ardiendo. Sin rayos ni fuego aleatorio. Visual: color ×
  max(50, 255 − (1 − vida)·2550)/256 (casi negro al perder un 8 %), modo 230 + calor·25/255, escala × 5·vida por
  debajo de 0,2; llamas `FireGraphic` (sprites `S_Fire.raw`, humo `S_SpriteSheet3.raw`, luz `S_LMFireBall.raw`),
  2 llamas por árbol de 0,2·alto; sonido de fuego en bucle.

## Sacrificio

- **Sacrificio** (aplazado por el usuario): soltar un árbol apuntando al **WorshipTotem** (CitadelPart del sitio de
  culto; `ValidToApplyThisToObject` 0x74BD50): v = sacrificeValue·vida·(0,5 + 0,5·vida) al maná del sitio
  (+0xF0) y al total (+0xF4), fantasma del árbol (`GoolooGooloo`), `G_SACRIFICE_01`, número flotante rojo "%3.0f".
  Aldeanos ×1,25; comida, rocas y vasijas no. **Pendiente**: el sacrificio no está portado (`GET_SACRIFICE_TOTAL` da 0
  en `CHLApi.cpp`). Los sitios de culto que necesita ya existen (`src/Worship`; `CREATE_WORSHIP_SITE` llama a
  `magic::script::CreateWorshipSite`, ver
  [magic.md](magic.md#culto-de-dónde-salen-los-milagros-m7-srcworship-ecssystemsimplementationsvillagerworship)).

## Pendiente

- Tirón: comprobar con el original la altura del plano (la «Consecuencia» del Tirón) con `tmp_dis\trees2\tugwatch.py`;
  openblack usa la distancia horizontal de la mano.
- Replantar: `StartImmersion(0x2E)` y la mímica de la criatura.
- `MakeScenicForest` y `AssignForestsToTown` sin llamador (carga del mapa y edificios terminados): los pueblos no tienen
  lista de bosques.
- Talar: `flags |= 2` y `+0x1A4 = 2` del objeto físico, `RaiseUntilNotIntersecting` 0x644800 y las dos reacciones 0x0C.
- Crecimiento y campos: la lluvia y la alineación del terreno (`MapCoords::GetAlignment`).
- Curvado: la criatura del jugador (ranura 2).
- Sacrificio (aplazado por el usuario).

## Ganchos de prueba

- `OPENBLACK_TEST_TUG="x,z,espera,mantener"` (+ `OPENBLACK_TEST_TUG_MOUSE2="x,y"`) y `OPENBLACK_HAND_TRACE=1`: el tirón.
- `OPENBLACK_TEST_REPLANT="x,z,gradosDeInclinación"`: suelta un árbol y dice si se replanta, cae o queda muerto.
- `OPENBLACK_TEST_FELL="x,z"`: talar.
- `OPENBLACK_TEST_TREE_QUERIES="x,z"`: búsquedas de árboles y bosques, `MakeScenicForest`, `AssignForestsToTown`.
- `OPENBLACK_HAND_TEST_FOREST=1`: coger de un BigForest.
- `OPENBLACK_TEST_TREE_GROWTH="x,z"` y `OPENBLACK_TREE_TRACE=1`: crecimiento y brillo.
- `OPENBLACK_HAND_TEST_HOLD=1.5`: curvado de los árboles junto a lo que lleva la mano.
- Fuego: los ganchos de [magic.md](magic.md#ganchos-de-prueba) (`OPENBLACK_TEST_FIRE`).

## Fuentes

- `C:\Users\diewgarc\dev\tmp_dis\trees2\`: `pick_rules.txt`, `treeinfo.txt`, `fire_notes.txt`, `totem_notes.txt`,
  `villager_queries.md`, `tugwatch.py`.
- `C:\Users\diewgarc\dev\tmp_dis\physics\physob.md` («Sign convention»): el giro del árbol talado.
- `C:\Users\diewgarc\dev\tmp_dis\field\draw_colour_sway_notes.txt`: la tabla del mecido.
- bw1-decomp `src/Black/Object.cpp:539`: la prueba de la normal al soltar (solo seres vivos y vallas).
