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

## Huida (reacción 28)

Cada depredador crea al nacer la reacción "huir del depredador" y la reparte **una sola vez**: a los animales que en
ese momento estén a 25 m o menos (5 × 5 celdas; distancia = media distancia Manhattan), si su especie tiene
`isFleeingFromPredator`, no están muertos / derribados / esperando un clip y el depredador no acecha (a su speed2 o
menos pasa inadvertido); un depredador solo huye de uno más fuerte. `Reaction::ProcessReactions`, que la repartiría cada
turno, depende de un interruptor de depuración que el juego no activa nunca: en el original los herbívoros casi nunca
huyen. Huir: estado 49 a su velocidad de huida (vaca, oveja, cerdo 4 m/s; caballo 9) ~10 m hacia un lado de la
trayectoria del depredador; luego estado 6 (otra vez si viene hacia él o está a menos de 30 m) o 30 (quieto
mirándolo); termina a los ~80-88 turnos o a más de 75 m, y vuelve a DECIDE.

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

## Diferencias y pendiente

- MOVE_TO_POS va recto con el giro limitado: el wall-hug alrededor de obstáculos no está portado para animales, y tras
  1000 turnos sin llegar se da por llegado (seguro de openblack). `Collide(collideType)` se aproxima con el agua y la
  huella de los objetos fijos [supuesto]. El orden de la espiral de pastoreo y el sitio al azar para dormir son
  supuestos.
- La guarida del tigre es el árbol de bosque más cercano (el original puntúa los bosques con una sigmoide de la
  distancia y toma el primer árbol del mejor). Los animales de script (flags 0x400 / 0x4000, bandada +0x5C) se tratan
  como normales.
- Aves: el valor inicial de flock+0x78 (antes del primer tramo del líder) se supone FOLLOW_FLOCK; tras cada clip
  completo en FOLLOW_FLOCK se vuelve a tirar la moneda y se reinicia la cuenta [supuesto]; el seguimiento del miembro
  siguiente (modo 2) y el aterrizaje no se portan (no se usan).
- Sin hacer: aldeanos como presa, fusión de bandadas tras aterrizar, crecer con
  la edad, las demás reacciones (huir de la mano...), pastores, el humo del cadáver, animal lanzado a un almacén de
  comida → comida.

## Ganchos de prueba

`OPENBLACK_ANIMAL_TRACE=1` (cambios de estado, landType y cada 50 turnos cuántos hay en cada estado),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distancia[,ángulo[,cada]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turno[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turno"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n cuenta solo esa especie),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (esa especie con hambre en el turno 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turno>`
(los depredadores reparten otra vez su reacción de huida), `OPENBLACK_TEST_VIEW_LOCK=1` (la cámara se coloca cada turno
junto al animal, para las aves). Land2 (`-s Land2.txt`) tiene leones, tigres y lobos. `dev\shot_animal.sh <nombre> <fotogramas> <captura> [VAR=valor...]` lanza una
copia privada en `dev\animales_run`.
