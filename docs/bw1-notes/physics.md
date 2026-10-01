# Físicas: objetos lanzados, choques, daño y rocas que se parten

Código: `src/ECS/Physics/` (`PhysOb` = sólido rígido, `PhysicsObjects` = gestor por turnos), `src/ECS/Rocks.*`,
`src/ECS/Components/Life.h`, y la parte de la mano en `HandPhysics.cpp`. Informes completos con pseudo-C++ y
direcciones en `C:\Users\diewgarc\dev\tmp_dis\physics\` (`physob.md`, `physicsobject.md`, `physob_bodies.md`,
`rock_split.md`). No se usa Bullet para esto (openblack solo lo usa para lanzar rayos).

- [Motor (PhysOb)](#motor-physob-0x7fb7300x7fe7b0)
- [Cuerpos](#cuerpos)
- [Gestor](#gestor-physicsobjectgameturnupdate-0x644fc0)
- [Edificios que se rompen](#edificios-que-se-rompen-abodereacttophysicsimpact-0x406240-fragmesh-0x7f6f00)
- [Sonidos, polvo y aspecto de los golpes](#sonidos-polvo-y-aspecto-de-los-golpes)
- [El agua en los golpes y al soltar](#el-agua-en-los-golpes-y-al-soltar)
- [Rocas que se parten](#rocas-que-se-parten-rocksplitintwo-0x6e7560)
- [Pendiente](#pendiente), [Ganchos de prueba](#ganchos-de-prueba), [Fuentes](#fuentes)

Estado: el motor y lo que se describe aquí es **fiel** (portado del original) salvo lo marcado y lo que está en
[Pendiente](#pendiente). Todo lo del agua que no es física (qué celda es agua, el golpe contra el agua, hundirse y
ahogarse) está en [water.md](water.md).

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
  y un animal hundido se borra (ver [water.md](water.md#hundirse-ahogarse-y-borrarse)).

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

## El agua en los golpes y al soltar

- En [water.md](water.md): las [celdas de agua (SeaCells)](water.md#celdas-de-agua-seacells), el
  [golpe contra el agua, la onda al cabecear y los recursos que caen al mar](water.md#golpes-y-objetos-que-caen-al-agua)
  y [hundirse, ahogarse y borrarse](water.md#hundirse-ahogarse-y-borrarse) (`HasSunk`, estado DROWNING 16,
  `ToBeDeleted`). Aquí queda el soltar desde la mano, que decide si el objeto aterriza o se queda en física (también
  sobre el agua).
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

## Rocas que se parten (Rock::SplitInTwo 0x6E7560)

- Una roca con radio 2D > 3,6 no se puede coger: **pulsar sobre ella la golpea al momento**. Una roca que sí se puede
  coger se golpea con un clic corto (< 225 ms). Condición: altura > 0,7. Sin contador de golpes.
- Salen dos rocas del mismo tipo, escala × 0,7935 (∛½: la mitad de volumen), solo el ángulo Y, en
  `Pos ± (cos a, 0, sin a)·0,7935·R2D` con `a` al azar; la original se borra y las mitades entran en física (caen o
  siguen volando con su velocidad). Sonido G_RockTap_01..04 en rotación, en la mano.
- Los impactos fuertes también las parten (ver daño).

## Pendiente

- Nieve sobre la FragMesh (necesita el clima: tormentas de nieve y el mapa de nieve 128×128) y carbonizado/brillo por
  fuego (necesita el sistema de fuego); el color 0,75 de la tapa del "a medio construir".
- Edificios: reparación por los aldeanos (sitio de construcción, madera), golpes de la criatura, alineamiento y agresor
  del pueblo, que salgan los habitantes al bajar de 0,75, edificios a medio construir (−0,2 por golpe).
- Aldeanos y animales al aterrizar: las tres posturas del original y los cadáveres (hoy se levantan o desaparecen).
- Muerte de aldeanos completa (`VillagerDead` 0x7506C0) y el mimetismo de la criatura al hundirse algo que soltó el
  jugador: en [water.md](water.md#pendiente).
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

## Ganchos de prueba

`OPENBLACK_TEST_PHYSICS="x,z,altura,vx,vy,vz[,escala[,n]]"`, `OPENBLACK_TEST_HIT_VILLAGER="velocidad[,escala[,índice]]"`,
`OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz"`, `OPENBLACK_TEST_HIT_ABODE="velocidad[,escala[,índice[,n]]]"` (n rocas contra una casa, una cada 1,5 s), `OPENBLACK_HAND_TEST_SPLIT="x,z,escala,rondas"`,
`OPENBLACK_PHYSICS_TRACE=1` (posición, velocidad, contactos, G, densidad y radio de cada cuerpo por turno),
`OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (tipo = `villager|animal|tree|pot|rock`: lo crea a esa altura sobre el
punto y lo mete en física sin velocidad; escribe la celda, la densidad y `GET_LAND_HEIGHT` ahí y en la tierra de
referencia, y traza el contador del aldeano que se ahoga cada 100 turnos),
`OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"` (la mano sujeta una roca 0, una vasija de 300 de comida 1 o de madera 2,
el primer aldeano 3, el primer árbol 4 o el primer animal 5, y lo **suelta suave** en (x, z) tras esos segundos de juego; el log dice si acabó en física).

## Fuentes

- `C:\Users\diewgarc\dev\tmp_dis\physics\`: `physob.md`, `physicsobject.md`, `physob_bodies.md`, `rock_split.md`,
  `fragmesh.md`, `abode_damage.md`, `collision_sounds.md` (tabla completa en `snd/full_matrix.md`) y
  `building_visuals.md`.
- `bw1-decomp` `src/Black/Object.cpp:447` (`Object::InitialisePhysicsFromHand`, emparejado).
- `tmp_dis/agua/re/scan_tree5c.py` (barrido de la bandera `Tree`+0x5C).
