# Animales: IA, estados y clips

Investigación con direcciones: `C:\Users\diewgarc\dev\tmp_dis\animals\grazing_ai.md` (IA de los herbívoros),
`hand_death.md` (mano, vuelo, aterrizaje, muerte), `predator_ai.md` (depredadores), `hunting.md` (la caza),
`flee.md` (la huida), `birds_ai.md` y `birds_draw.md` (las aves) y `openblack_plumbing.md` (qué reutiliza openblack). Scripts en la
misma carpeta (`dumpanimals.py` valores de info.dat, `vtd.py` vtables, `a.txt` desensamblado de Animal.cpp).
bw1-decomp solo tiene stubs vacíos de Animal*.cpp: todo sale de runblack.exe.

En openblack: `ECS/AnimalAI.*` (estados por turno, herbívoros, mano, muerte), `ECS/AnimalPredators.cpp` (depredadores
y caza), `ECS/AnimalFlee.cpp` (huida), `ECS/AnimalBirds.cpp` (aves), `ECS/AnimalAIDetail.h` (lo que comparten), `ECS/AnimalAnimations.*` (clip por
estado y especie),
`components::AnimalBrain` (los campos de Living / MobileWallHug / Animal que usa la IA), `Flock::leaderTurns`.

## Turno (`Animal::ProcessState` 0x417EE0)

Cada turno, después de los aldeanos: TurnsSinceStateChange + 1; si el estado tiene la marca de necesidades
(info.dat `animalStateTable.field0xa4`) `ProcessNeeds` (hambre, sueño, cría +1 hasta su máximo de info.dat; el líder
cuenta `leaderTurns`); luego la función del estado (`g_AnimalStateTable` 0xD12108). No hay clips de entrada / salida
ni cambio de velocidad por estado (`Animal::SetStateSpeed` vacío). `SetTopState`: filtro de salida (en la mano solo
FLYING, LANDED o muerte; volando solo IN_HAND, LANDED o muerte), estado, contador a 0 y el clip del estado.
`PlayAnimThenSetState`: WAIT_FOR_ANIMATION con el clip sin cambiar hasta que turnos × 100 ms ≥ su duración.

## Herbívoros (oveja, tortuga, vaca, caballo, cerdo)

Una sola clase (constructor 0x41D0B0, vtable "Cow"); cambian solo los clips. Ciclo:

- **DECIDE_WHAT_TO_DO** (`Cow::DecideWhatToDo` 0x41D1B0): criar si toca; el líder (primer miembro de la bandada) lleva
  la manada a un punto al azar del dominio cada `stayTime` turnos (vaca 200, resto 1000) o si salió del dominio; un
  miembro a más de `flockDistance` del líder vuelve a su lado; si no, las necesidades; si no, START_WANDER.
- **WANDER** (`Cow::Wander` 0x41D280): en línea recta a `step` por turno y solo cambia de rumbo al entrar en otra celda
  de 10 m (sin wall-hug ni agua). El rumbo nuevo (`SetNewWander` 0x41A3F0) suma, con un presupuesto de la velocidad
  por el eje mayor (`fn_0041A5B0`), 0,9 × velocidad hacia el líder si está lejos, la bandada (`fn_0041AD70`: 1/5 hacia el
  centro de los demás, el vecino más cercano por ejes, **otra vez** el vector de cohesión, 3/5 del paso del vecino; la
  distancia se compara con MapCoords crudos, así que casi siempre atrae) y un giro al azar de ±turnAngle/2.
- **Hambre** (50 turnos, caballo 100): `LookForGrazePos` recorre en espiral (dominio/10)² celdas desde la suya, delante
  (±viewAngle/2 = ±90°), ni la suya ni la de otro miembro (o su destino), sin agua ni objeto fijo; MOVE_TO_POS allí →
  **START_TO_EAT** (clip de bajar la cabeza una vez) → **EAT** 20..34 veces (GameRand(15) + 20), cada vez uno de los dos
  clips de comer al azar → **FINISH_EATING** (levantar la cabeza) → DECIDE.
- **Sueño**: contador puro, sin día / noche. Lleno (1000) → SEEK_SLEEP al sitio de dormir (la celda del centro del
  dominio al nacer) → SLEEPS de pie (−2 por turno, +1 de ProcessNeeds: ~100 s).
- **Cría** (3000 turnos): solo si la bandada tiene menos miembros de los que tuvo (`maxMembers`): GIVES_BIRTH crea uno
  de edad 1 en la misma bandada.
- Velocidad: siempre `speedDefault` (vaca, oveja, cerdo 0,75 m/s; caballo 1,5; tortuga 0,25), así que siempre el clip
  de andar. Giro en MOVE_TO_POS: `Animal::SetTowardsAngle` 0x418560, como mucho turnAngle por turno (vaca 34 = 6°).

## Depredadores (león, tigre, leopardo, lobo)

Tigre y leopardo usan el código del león (el tigre pone la guarida en un bosque); el lobo tiene el suyo. Pasean, duermen,
crían y aterrizan con las funciones de los herbívoros.

- **Felinos** (`Lion::DecideWhatToDo` 0x41FE70): criar; después de las 22:00 (hora visual) a dormir con el contador
  lleno; el líder mueve la manada (stayTime león 200); si no, hambre (león 12000 turnos, tigre 9000, leopardo 1200),
  luego sueño; si no, a pasear a 1,5 m/s.
- **Lobos** (0x4216B0): a la guarida (un cuadrado del tamaño de la manada alrededor del centro del dominio: el bosque
  grande, bosque o árbol más cercano) y tumbados en ella (HIDE_IN_LAIR, clip de dormir). Hambrientos (120 turnos) solo
  salen después de las 23:00: hacia la bandada más cercana no más fuerte a 1,2 m (en la práctica la suya: el líder caza
  donde está) o hacia el pueblo más cercano.
- **Presa** (`fn_004196D0`): la primera de una espiral de 64 celdas (±40 m): otra especie, en el suelo, viva, con carne
  (todos menos la tortuga), fuera de sus círculos de giro; un cachorro solo una presa derribada. Se persigue desde la
  siguiente comprobación, a 8 m/s. En el original los **aldeanos también son presa**; openblack aún no, porque sus
  aldeanos no tienen los estados de derribado / comido.
- **Persecución** (HUNTING_MOVE_TO_POS 41, 0x418DB0): acecha a 1,25 m/s (clip de acecho) de 100 a 50 m, esprinta a
  10 m/s por debajo de 50 m; salta cuando está a zancada del salto × escala × 0,5 (~2,3 m) y la presa está a ±22,5° de
  su rumbo; abandona a los 20 s (chaseTime) o a más de 100 m.
- **Salto** (TARGET_POUNCE 40, 0x419010; clip POUNCE_HI, el lobo POUNCE): a 1 m o menos la presa cae (vida 0,05,
  DOWNED); al terminar la zancada del salto: si cayó, `FinishPouncing` (hambre 0, se pone junto a la presa a comer);
  si no, vuelve a perseguirla.
- **La presa**: DOWNED (clip de caer) → BEING_EATEN (tumbada) 300 turnos → DEAD, cadáver 50 turnos.
- **Comer**: START_TO_EAT → EAT 15..24 ciclos; si la presa desaparece vuelve a decidir (`Lion::Eat` 0x41FE40; también
  al final de la comida, así que el clip de levantarse no llega a verse).

## Aves (cuervo, paloma, golondrina, paloma bravía, gaviota, murciélago)

Todas son la clase Dove (constructor 0x41DCF0); solo cambian los clips y los valores de info.dat. Nacen a
`altitudeNormal` sobre el suelo (20 o 40 m), a 8-10 m/s.

- **Líder** (`Dove::DecideWhatToDo` 0x41DE40 → `StartWander` 0x41DF50 → SPECIAL_MOVE_TO_POS 44): un tramo hasta un
  punto al azar a 80 m (domainRadius) **de donde está** (la bandada deriva por la isla), a su altura ± altitudeVariance
  dentro de altitudeNormal + [altitudeMin, altitudeMax]; otro tramo al llegar o cada stayTime (100 turnos).
- **Seguidores** (FOLLOW_FLOCK 45): un punto a 10 m del líder, luego un hueco de formación (`fn_0041E890`, leído
  literalmente) y MOVE_TO_POS en 2D a altura constante sobre el suelo; vuelta a empezar.
- **Altura** (`Animal::MoveTo3D` 0x418AA0): en vuelo mantienen su altura absoluta (no siguen las colinas) y suben o
  bajan como mucho altitudeMovementChange por turno (0,2-0,6 m), nunca a menos de 2 m del suelo.
- **Alabeo**: al girar se inclinan ±0,5 rad en 2 s (el Zoomer, `Animal::SetTowardsAngle`) y vuelven a nivel en 2 s;
  `Dove::Draw` 0x41F680 gira la matriz dibujada alrededor de su eje de avance (en openblack, en `MobileDrawing`).
- **Nunca se posan** en el juego original: info.sleep es 0 en todas, así que LAND_AT_POS / SLEEPS no se alcanzan. No
  tienen hambre, cría ni día / noche (los murciélagos vuelan de día).
- **Clips**: el de moverse (y el de decidir) es una moneda entre batir alas y planear en cada cambio de estado y en cada
  SetSpeed (la golondrina entre tres, el murciélago siempre bate); su SetAnim nunca reinicia el clip. Tabla en
  `ECS/AnimalAnimations.cpp` (`BirdClip`).
- No se pueden coger ni golpear (playerCanPickUp 0) y no son presa (a más de 2 m). Muertas caen con la física a la
  velocidad del vuelo (`Dove::Dying` 0x41F1B0) y quedan en el suelo como cadáver.

## Moverse (MobileWallHug, `dev\tmp_dis\animals\wallhug.md`)

`Living::SetupMoveToPos` (0x5F2830) usa la versión de un argumento de `SetupMobileMoveToPos`: **STEP_THROUGH**, un paseo
recto sin rodear obstáculos que en los animales re-apunta cada turno con su `SetTowardsAngle` (giro limitado por
turnAngle). Al paso que queda a una zancada pasa a FINAL_STEP y llega (0xA) al turno siguiente; como en el original, un
destino dentro de su círculo de giro puede rodearse para siempre. El rodeo de obstáculos (LINEAR → ORBIT →
EXIT_CIRCLE) solo lo usa `SetupMoveToWithHug`: ir a la comida (19) y volver a huir (6); **ese rodeo no está portado
para animales** (van rectos también ahí). `GUtils::Spiral` (dir 1, cuenta 1: (−1, 0), (0, −1), (+1, 0) × 2, (0, +1) × 2...)
en todas las espirales; `Collide(1)` es solo agua (el bit `hasWater` de la celda de terreno) o fuera del mapa,
`Collide(0)` nada; `CalcRandomPos` (0x5ED080): dos puntos al azar, cada uno con una espiral de 25 celdas, aceptando si
está en el mapa, sin choque, fuera de los círculos de giro y (aves) sobre un bloque de terreno; si no, el centro si está
fuera de sus círculos de giro, y si no, su propia posición. Ángulos con las tablas trunc(65536 cos) y `LHArcTan`.
Rumbo inicial 0 (+x). La lista de la bandada está en orden inverso de creación: el líder es el primero que entró; la
formación de las aves cuenta desde el más nuevo. Los animales se procesan del más nuevo al más viejo.

## Reacciones

`Reaction::ProcessReactions`, que las repartiría cada turno, depende de un interruptor de depuración que el juego no
activa nunca, así que **cada reacción se ofrece una sola vez**, al crearse (`Reaction::CreateReaction` → `SpreadReaction`
0x6E3E10: (maxReactionDistance × 0,2)² celdas de `GUtils::Spiral` dentro del radio; `ApplyReactionToLivingObjectsAtSquare`
0x6E3F90 para cada animal: disponible (`IsAvailableForReaction`), distancia = media distancia Manhattan, puntuación
min(255, prioridad × (1 + 0,5 × howImportantIsDistance × (max − d) / max)), y sus **registros** (3 como mucho, {tipo,
turno}: no vuelve a tomar un tipo antes de NumGameTurnsBeforeReactingAgain; caducan a los 1800 turnos). Un animal que ya
reacciona solo cambia si la nueva puntúa más y la actual dura ya 10 s (1 s si era coger con la mano). Cada tipo acaba
por su propio número de turnos (`Living::ProcessReaction` 0x5F1270), si desaparece el iniciador (y entonces vuelve al
estado guardado: DECIDE, o el estado 0 si venía de moverse, y allí se queda como el original) o si desaparece la
reacción (sigue en su estado). Salir a un estado que no es de reacción (la mano, morir, una necesidad) suelta la
reacción sin cambiar el estado (`Animal::ExitReaction` 0x41B170). Los animales reaccionan a objeto (0), mirar (1),
hechizo (3), criatura (6), comida (7), fuego (10), árbol cayendo (27, que nadie crea) y depredador (28); los
depredadores también a objeto volador (9). Tortuga y aves no reaccionan. **Ningún animal reacciona a la mano**.

- **Depredador (28):** lo crea cada depredador al nacer (fn_0041FD30). Huyen los que tengan `isFleeingFromPredator` a
  25 m, si el depredador no acecha (a su speed2 o menos pasa inadvertido); un depredador solo de uno más fuerte. Huir:
  estado 49 a su velocidad de huida (vaca, oveja, cerdo 4 m/s; caballo 9) ~10 m hacia un lado de su trayectoria; luego
  6 (otra vez si viene hacia él o está a menos de 30 m) o 30 (quieto mirándolo); ~80-88 turnos o a más de 75 m.
- **Comida (7, `Pot::SetupReaction` 0x66D660):** las pilas de comida del mapa (CREATE_POT) al cargar, y una vasija que la
  mano deja; se quita al cogerla o vaciarla. Los herbívoros **hambrientos** a 35 m van al borde de la pila (la suma de
  los dos radios), le quitan 50 (la pila encoge; vacía desaparece) y su hambre queda a 0. Las pilas que hace la mano
  (MagicFood) tienen foodType 0: ningún animal va a ellas, tampoco en el original.
- **Objeto volador (9, `Object::InitialisePhysicsFromHand` 0x637412):** lo que la mano lanza hace huir a los
  depredadores a 25 m si 2 × su velocidad pasa de su distancia; la reacción se quita al aterrizar el objeto.
- Sin portar (necesitan sistemas que openblack no tiene): hechizos (0, 3), artefactos del pueblo (1), criatura (6),
  fuego (10).

## Mano, vuelo y muerte

- Coger: `GAnimalInfo.playerCanPickUp` (todos los de tierra salvo los de los puzles). Al cogerlo deja su bandada por
  una propia (`SeperateLivingIntoNewFlock`) → IN_HAND. Soltar o lanzar: física → FLYING (clip THROWN).
- En reposo (`Animal::EndPhysics` 0x5F0D80): landType por la fila derecha del cuerpo (y > 0,5 de lado derecho, < −0,5
  izquierdo, si no de pie); vivo → LANDED (clip de levantarse según landType; los depredadores, el de despertar) → la
  bandada se centra donde cayó → INTERACT_DECIDE → a pasear. **No hay ahogamiento** de animales (solo se borra un
  cadáver hundido). openblack deja un animal suavemente de pie (el original lo suelta en la física).
- Muerte (`Living::SetDying` 0x5EC390, nada mientras vuela): DYING (clip de caer) → DEAD (tumbado según landType; los
  depredadores con el clip de dormir) 600 turnos → desaparece (el humo `CreateSmokyStuff` aún no). Un cadáver lanzado
  vuelve a DEAD con otros 600.

## Clips por especie (AnimalAnimation.cpp 0x41C0E0..)

| especie | andar | quieto | comer | bajar / subir cabeza | mano | lanzado | aterriza lt 0/1/2 | muerto lt 1/otro | cae |
|---|---|---|---|---|---|---|---|---|---|
| vaca | 45 (41 corre) | 42 | 36/35 | 37 / 44 | 38 | 43 | 42/40/39 | 30/29 | 31 |
| oveja | 145 (141) | 142 | 133/132 | 136 / 140 | 137 | 143 | 142/139/138 | 131/130 | 134 |
| cerdo | 128 (125) | 126 | 117/116 | 120 / 124 | 121 | 127 | 126/123/122 | 115/114 | 118 |
| caballo | 61 / trote 59 / 56 | 57 | 49/48 | 52 / 60 | 53 | 58 | 57/55/54 | 47/46 | 46 |
| tortuga | 172 | 171 en todo | | | | | | | |

Umbrales: `speedThreshold` entrada 2 (vaca, oveja y cerdo) y 3 (caballo). Depredadores (león, tigre, leopardo, lobo):
acecho bajo speedDefault, andar, correr sobre la entrada 6..9; en la tabla de `ECS/AnimalAnimations.cpp`. El clip
avanza con el terreno recorrido mientras se mueve (`Object::IsMoving`) y con el tiempo si no.

## Bandadas, edad y humo

- **Coger un animal** (`Flock::SeperateLivingIntoNewFlock` 0x52FE10): siempre pasa a una bandada propia donde lo
  cogieron, con el radio y la distancia de la vieja, sin pueblo y máximo 0; la vieja, si queda vacía, se borra.
- **Fusión** (`Animal::LookForFlocksInSpiral` 0x41A690): solo tras aterrizar (±80 m), porque `flocksCanMerge` es 0 en
  todas; la más grande se queda con todos si son de la misma especie y la suma no pasa de `maxFlockSize`; una bandada
  con pueblo no se fusiona. Rareza: el máximo de la que se queda suma el de la otra una vez por miembro, recortado.
- **Edad** (`Living::GetAge` 0x5ECAF0): 1500 turnos por año desde su turno de nacimiento. Los jóvenes crecen cada 375
  turnos (`escala += aleatorio(0,75 × (ageToScale[edad + 1] − escala))`) hasta `grownUpAge`; un adulto nace a 0,9 y dos
  tiradas lo dejan en (0,95, 1,05]. **Los animales no mueren de viejos**: comprobado en todo el ejecutable
  (`dev\tmp_dis\animals\old_age.md`): `oldAge` / `retirementAge` solo los lee `Villager::CheckDeathFromOldAge`
  (0x760CA0) y el motivo de muerte OLD_AGE (9) solo es de aldeanos.
- **Nacer** (GIVES_BIRTH): el recién nacido decide al momento, antes de que la madre vuelva a pasear.
- **Animal lanzado a un almacén de comida:** se convierte en comida (su foodValue: vaca y caballo 1200, oveja 800,
  león y tigre 900, lobo 700, cerdo 290) y desaparece (`Animal::ReactToPhysicsImpact` 0x41BC10).
- **Humo del cadáver** (`Object::CreateSmokyStuff` 0x63A810, `ECS/SmokyStuff.*`): 15 sprites de `Data\Textures\smoke.raw`,
  cada uno con dirección al azar a 0,3..1 × tamaño, gris con opacidad vida × 100 / 255, 3 s, de 0,5 a 1,5 × tamaño.
- **En la mano** los animales tienen el agarre de los aldeanos (radio 2D, bajada 0,65). El landType se lee de la matriz
  del cuerpo al empezar el turno.

## Aldeanos cazados

Los aldeanos son presa como los animales (tipo 2, con carne) si están fuera de casa. Derribados, su salud queda en 5 %
y pasan a DOWNED (clip `P_ATTACKED_BY_LION`), luego BEING_EATEN 300 turnos (`P_DYING`) y mueren (`Villager::BeingEaten`
0x76B380). Los conduce la IA de animales (`components::DownedVillager`).

## Diferencias y pendiente

- El rodeo de obstáculos de ir a la comida y volver a huir (`SetupMoveToWithHug`): no portado para animales. El port de
  openblack para aldeanos (`PathfindingSystem`) tiene fallos propios (wallhug.md §7).
- Guaridas del tigre y el lobo: el original toma el segundo bosque más nuevo (todos los bosques a más de 0,15 m puntúan
  igual) y su árbol crecido más cercano al centro (audit_r4.md); openblack aún no tiene la lista de bosques en ese
  orden, así que se usa el árbol de bosque más cercano.
- Aldeano comido: desaparece (sin cadáver, alineamiento, avisos del pueblo ni duelo de los vecinos); los aldeanos no
  huyen de los depredadores ni toman reacciones.
- Pastores, las ramas `IsInScript` y las marcas de script (0x400 / 0x4000 / +0x25 & 0x40: presa que sobrevive), la
  prioridad de líder de FLOCK_ATTACH, el aterrizaje de las aves (inalcanzable en el juego: info.sleep 0), el orden
  exacto de las listas de cada celda del mapa y el turno intercalado con los aldeanos.
- Los números aleatorios son los de openblack (mt19937), no el GameRand del original.
- Dejar un objeto suave con la mano lo coloca al momento; el original lo suelta en la física (mano, Tareas.txt).

## Ganchos de prueba

`OPENBLACK_ANIMAL_TRACE=1` (cambios de estado, landType y cada 50 turnos cuántos hay en cada estado),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distancia[,ángulo[,cada]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turno[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turno"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n cuenta solo esa especie),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (esa especie con hambre en el turno 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turno>`
(los depredadores reparten otra vez su reacción de huida), `OPENBLACK_TEST_HUNT_VILLAGER="<especie>,<turno>"`,
`OPENBLACK_TEST_FOOD_PILE="<especie>,<turno>"`, `OPENBLACK_TEST_SMOKE=<n>`, `OPENBLACK_TEST_CORPSE_TURNS=<n>`,
`OPENBLACK_TEST_VIEW_LOCK=1` (la cámara se coloca cada turno
junto al animal, para las aves). Land2 (`-s Land2.txt`) tiene leones, tigres y lobos. `dev\shot_animal.sh <nombre> <fotogramas> <captura> [VAR=valor...]` lanza una
copia privada en `dev\animales_run`.
