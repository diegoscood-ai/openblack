# Animales: IA, estados y clips

Toda la IA de los animales portada de runblack.exe (sesión "animales", 2026-09-30; commits 06fec160 herbívoros, mano y
muerte · 2d629816 depredadores y huida · e3a9d81f aves · 333ad5ae API para los hechizos · 2894cfd9 aldeanos como presa,
bandadas, edad, humo, reacciones de comida y objeto volador · f992a01a auditoría completa contra el original). bw1-decomp
solo tiene stubs vacíos de Animal*.cpp: todo sale del ejecutable.

Investigación con direcciones, en `C:\Users\diewgarc\dev\tmp_dis\animals\`: `grazing_ai.md` (herbívoros),
`hand_death.md` (mano, vuelo, aterrizaje, muerte), `predator_ai.md` (depredadores), `hunting.md` (caza), `flee.md`
(huida), `birds_ai.md` / `birds_draw.md` (aves), `misc.md` (bandadas, edad, humo), `villager_prey.md` (aldeanos
cazados), `reactions.md` (todas las reacciones), `wallhug.md` (moverse), `old_age.md` (vejez), `audit.md` y
`audit_r1..r4.md` (la auditoría), `openblack_plumbing.md`. Scripts en la misma carpeta (`dumpanimals.py` valores de
info.dat, `vtd.py` vtables, `a.txt` desensamblado de Animal.cpp).

En openblack: `ECS/AnimalAI.*` (turno, herbívoros, mano, muerte, estados de movimiento), `ECS/AnimalPredators.cpp`
(depredadores y caza), `ECS/AnimalLairs.cpp` (guaridas), `ECS/AnimalBirds.cpp` (aves), `ECS/AnimalFlee.cpp` (las
reacciones), `ECS/AnimalAIDetail.h` (lo
que comparten), `ECS/AnimalAnimations.*` (clip por estado y especie), `ECS/AnimalApi.cpp` (funciones para hechizos y
scripts, en `AnimalAI.h`), `ECS/AnimalDebugHooks.cpp` (ganchos de prueba), `ECS/SmokyStuff.*` (humo del cadáver),
`components::AnimalBrain` (los campos de Living / MobileWallHug / Animal que usa la IA), `components::Flock`,
`ECS/ScriptHeld.*` + `components::ScriptHeld` / `CannotBeEaten` (lo que retienen los scripts y sus marcas) y
`ECS/AnimalScript.cpp` (el animal que un script suelta).

**Ovejas de Land1:** `Land1.txt` no crea ninguna. Las 9 ovejas las crea el desafío "The Lost Flock" (`challenge.chl`,
script `TheLostFlock`), que `LandControl1` lanza solo tras la intro `FollowUs` y `CitadelGuide`; en openblack la intro
no termina todavía, así que no aparecen. Land2 sí las crea en el mapa.

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
  de andar. Giro en MOVE_TO_POS: `Animal::SetTowardsAngle` 0x418560, como mucho turnAngle por turno (vaca 34 = 6°);
  con el destino dentro de su círculo de giro (R = 2 × velocidad / turnAngle en radianes) gira |dif| − turnAngle × d / R,
  casi del todo al estar cerca (0x4186B4..0x4186FC), así que no se queda dando vueltas al destino.

## Depredadores (león, tigre, leopardo, lobo)

Tigre y leopardo usan el código del león (el tigre pone la guarida en un bosque); el lobo tiene el suyo. Pasean, duermen,
crían y aterrizan con las funciones de los herbívoros.

- **Felinos** (`Lion::DecideWhatToDo` 0x41FE70): criar; después de las 22:00 (hora visual) a dormir con el contador
  lleno; el líder mueve la manada (stayTime león 200); si no, hambre (león 12000 turnos, tigre 9000, leopardo 1200),
  luego sueño; si no, a pasear a 1,5 m/s.
- **Guaridas** (`ECS/AnimalLairs.cpp`; audit_r4.md §1.8, lairs.md). La lista de bosques (g_game +0x205BB4) va del más
  nuevo al más viejo; el primero se toma sin puntuar y los demás puntúan con `fn_0053AD00` =
  `SigmoidThreshold(-0,9, -(distancia MapCoords / 1000))` (la sigmoide del número de árboles se calcula y se descarta),
  cambiando solo si puntúan estrictamente más. Todo bosque a más de 1000 MapCoords (0,15 m) puntúa 0,1144, así que en la
  práctica **la guarida es el segundo bosque más nuevo** (con uno solo, ese), en su árbol crecido más cercano al centro.
  - Tigre (`Tiger::CalculeLairPos` 0x421470): esa regla; sin bosques o sin árbol crecido, donde está. Land5: el tigre en
    (2841, 2918), junto al bosque 19, pone la guarida en el bosque 18 (1358, 3577), a 1600 m.
  - Lobo (`Wolf::CalculeLairPos` 0x421730): el bosque grande más cercano; si no hay, la regla del tigre; si el bosque
    elegido no tiene árbol crecido o no hay bosques, el árbol más cercano (todos); si no, donde está.
  - León y leopardo (`Lion::CalculeLairPos` 0x420010): donde está. Land2 no tiene bosques: el tigre se queda donde está.
- **Lobos** (0x4216B0): a la guarida (un cuadrado del tamaño de la manada alrededor del centro del dominio, ver Guaridas)
  y tumbados en ella (HIDE_IN_LAIR, clip de dormir). Hambrientos (120 turnos) solo
  salen después de las 23:00: hacia la bandada más cercana no más fuerte a 1,2 m (en la práctica la suya: el líder caza
  donde está) o hacia el pueblo más cercano.
- **Presa** (`fn_004196D0`): la primera de una espiral de 64 celdas (±40 m): otra especie, en el suelo, viva, con carne
  (todos menos la tortuga), fuera de sus círculos de giro; un cachorro solo una presa derribada. Se persigue desde la
  siguiente comprobación, a 8 m/s. En el original los **aldeanos también son presa**; openblack aún no, porque sus
  aldeanos no tienen los estados de derribado / comido. Nunca es presa lo que no se puede comer (+0x25 & 0x40) ni lo
  controlado por un script (+0x24 & 0x400) salvo para un cazador que está en un script (ver Scripts y marcas).
- **Persecución** (HUNTING_MOVE_TO_POS 41, 0x418DB0): acecha a 1,25 m/s (clip de acecho) de 100 a 50 m, esprinta a
  10 m/s por debajo de 50 m; salta cuando está a zancada del salto × escala × 0,5 (~2,3 m) y la presa está a ±22,5° de
  su rumbo; abandona a los 20 s (chaseTime) o a más de 100 m.
- **Salto** (TARGET_POUNCE 40, 0x419010; clip POUNCE_HI, el lobo POUNCE): a 1 m o menos la presa cae (vida 0,05,
  DOWNED); al terminar la zancada del salto: si cayó, `FinishPouncing` (hambre 0, se pone junto a la presa a comer);
  si no, vuelve a perseguirla.
- **La presa**: DOWNED (clip de caer) → BEING_EATEN (tumbada) 300 turnos → DEAD, cadáver 50 turnos; la que no se puede
  comer (+0x25 & 0x40, 0x5EC4E0) pasa a LANDED y sigue viva.
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
turnAngle). Al paso que queda a una zancada pasa a FINAL_STEP y llega (0xA) al turno siguiente. El rodeo de obstáculos
(LINEAR → ORBIT → EXIT_CIRCLE) solo lo usa `SetupMoveToWithHug` (0x5F2890): ir a la comida (19) y volver a huir (6);
portado en `ECS/AnimalWallHug.*` (investigación `wallhug_circle.md`):

- **Círculos** (`ObjectCircleIterator`, siempre los de la celda de 10 m del punto que se barre): los de cada objeto fijo
  de la celda con datos de choque (no los campos, que el iterador salta; los bosques no tienen; un árbol solo su tronco
  de 0,3 m y solo en su celda; un objeto de varias celdas en cada celda cuyo círculo de 7,1 m toca). El de un edificio o
  elemento es el de su caja (centro, el mayor semieje × escala, mínimo 1 m); si la caja es más de 1,4 veces más larga que
  ancha, una fila de int(largo / ancho) + 1 círculos del semieje corto. Después, las celdas de agua (o fuera del mapa) de
  la celda y sus 8 vecinas, cada una un círculo de 7,2 m en su centro.
- **LINEAR** (`MoveToCircleHugLinearSquareSweep` 0x60CA50): el círculo más cercano en el que entra el rayo del paso
  (la entrada real, dot − √disc; detrás de −0,2 m no cuenta); TurnsToObj = distancia / largo del paso (0xFF: ninguno o a más
  de 255 turnos). Anda recto, y **solo re-apunta** (`InitStepsXZ`, con el giro limitado de la especie) y barre otra vez
  al cambiar de celda. Al acabarse la cuenta empieza a orbitar por el lado por el que pasa el paso (CW / CCW).
- **ORBIT** (0x60B0E4 / 0x60B40E): gira velocidad / radio × 0,0497 + 1 cada turno. `MoveToCircleHugCircleSquareSweep`
  (0x6159F0 CW, 0x614C40 CCW) busca a lo largo de la órbita el primer corte con otro círculo de la celda (o el destino si
  está dentro del círculo, 0,1° antes), TurnsToObj = el arco / (1,5 × velocidad), y pone el rumbo tangente (hacia el
  centro ± 90°, menos 64 por radio de más sobre 0,9 radios). A menos de un turno: el destino → STEP_THROUGH directo; otro
  círculo → pasa a él y barre otra vez (3 niveles). Sale (EXIT_CIRCLE, recto hacia fuera desde el centro) cuando está más
  cerca del destino que al empezar a orbitar, con el destino del lado de dentro y delante; fuera del círculo vuelve a
  LINEAR_CW / CCW.
- Consecuencia del original: como LINEAR solo re-apunta al cambiar de celda y con turnAngle (6° la vaca), un animal cuyo
  rumbo no apunta ya al destino (o que sale de una órbita hacia fuera) se aleja y no llega [el código; no visto en el
  juego original]; si el destino queda dentro del círculo rodeado, llega por el STEP_THROUGH final. Con números: la oveja
  gira como mucho 64 (11°) por re-apunte (`Animal::SetTowardsAngle` 0x418560; el giro casi entero solo dentro de su
  círculo de giro, R = 2 × 0,075 m / 0,196 = 0,77 m) y anda 0,075 m por turno, así que re-apunta una vez cada ~133 turnos;
  en Land2 con una pila al lado (`OPENBLACK_TEST_FOOD_PILE`) de 25 ovejas hambrientas solo 2 llegan a comer, y la
  reacción de comida dura 2000 turnos. Comprobado contra el código 2026-10-01 (MoveTo 0x60B095, MoveToCircleHug
  0x60D800, InitStepsXZ 0x60BFA0, SetupMobileMoveToPos 0x60ABC0, AreWeThere 0x60AD60, Living::MoveToPos 0x5EC270,
  ProcessReaction 0x5F1270: nada más re-apunta).

`GUtils::Spiral` (dir 1, cuenta 1: (−1, 0), (0, −1), (+1, 0) × 2, (0, +1) × 2...)
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
En openblack el reparto, los registros, la puntuación y la regla de cambio son comunes a todos los vivos
(`ECS/Effects/Reactions`, `components::ReactionRecords`); lo propio de los animales (prioridades 28/7/9, StartReacting,
turnos de 28, estados 49/6/30/19/20) es su manejador en `ECS/AnimalFlee.cpp`. Los aldeanos reciben las mismas
reacciones en el mismo reparto (fuego y teletransporte portados).

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
  bandada se centra donde cayó → INTERACT_DECIDE → a pasear. **No hay estado de
  ahogarse** en los animales (`Animal::EndPhysics` no tiene rama de agua): en el mar el animal
  nunca se para, su densidad sube y a los **~75 turnos (7,5 s)** pasa de 1 y `Living::HasSunk` 0x5ED370 lo mata y lo
  borra (`SetDying`, estado 15, `ToBeDeleted(0)`) — vivo o cadáver. En una celda somera con agua de altitud ≥ 2
  aterriza **vivo** (a diferencia del aldeano, que se ahoga); ver
  [water.md](water.md#hundirse-ahogarse-y-borrarse). Soltar suave
  sobre el mar ya lo mete en física como el original; en tierra openblack sigue colocándolo de pie.
- Muerte (`Living::SetDying` 0x5EC390, nada mientras vuela): DYING (clip de caer) → DEAD (tumbado según landType; los
  depredadores con el clip de dormir) 600 turnos (nunca si lo controla un script) → desaparece (el humo `CreateSmokyStuff` aún no). Un cadáver lanzado
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
  cogieron, con el radio y la distancia de la vieja, sin pueblo y máximo 0; la vieja, si queda vacía, se borra, salvo
  una bandada de script (+0x25 & 4: suelta la referencia del animal y se queda aunque esté vacía, 0x419B75).
- **Fusión** (`Animal::LookForFlocksInSpiral` 0x41A690): solo tras aterrizar (±80 m), porque `flocksCanMerge` es 0 en
  todas; la más grande se queda con todos si son de la misma especie y la suma no pasa de `maxFlockSize`; una bandada
  con pueblo no se fusiona; dos bandadas de script no se fusionan y una de script ajena se queda siempre con todos
  (0x41A837, fn_005302A0 0x5302B4). Rareza: el máximo de la que se queda suma el de la otra una vez por miembro, recortado.
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
0x76B380). Los conduce la IA de animales (`components::DownedVillager`). El que no se puede comer (+0x25 & 0x40) se
levanta (LANDED) en vez de morir; openblack lo pone en LANDED al acabar los 300 turnos, sin esperar al clip (aproximado).

## Scripts y marcas (`dev\tmp_dis\animals\script_flags.md`)

- **Retenido por un script** (`ECS/ScriptHeld.*`, fiel): los objetos de los scripts del original son huecos de
  `ScriptManage` (511, 0xD967F8) con su cuenta de referencias; en openblack el hueco es `components::ScriptHeld` de la
  entidad. Cada variable del LHVM que guarda un objeto le da una referencia (callbacks de `LHVM::Initialise` en
  `Game.cpp`, `ADD_REFERENCE` / `REMOVE_REFERENCE`; el ScriptLibraryR.dll original llama a esos dos nativos en cada POP
  de un objeto y al parar una tarea); con la primera queda `IsInScript` (+0x24 & 0x200) y, si lo creó un
  script (`CREATE`, `CREATE_WITH_ANGLE_AND_SCALE`: `AddScriptGameThing(thing, 1)`), **controlado por el script** (+0x24
  & 0x400). Tras cada `LookIn` (GScript::Process 0x6EB6DB → fn_0070D480) lo que ya no tiene referencias se suelta:
  pierde las dos marcas y un animal pasa por fn_0041AA00 (bandada propia si no tiene; muerto → SetDying, que da otros
  600 turnos al cadáver; vivo → INTERACT_DECIDE_WHAT_TO_DO, en openblack un turno después: aproximado).
- **Controlado por el script**: no es presa salvo para un cazador que esté en un script (HuntingMoveToPos 0x418DD7,
  fn_00419340, fn_004196D0), no toma reacciones (IsAvailableForReaction 0x5F120C), **su cadáver no caduca** (Living::Dead
  0x5EC41E) y su bandada es de script (mano y fusión, arriba).
- **Lobo en un script**: el líder pone la guarida donde está (Wolf::CalculeLairPos 0x421774) y caza desde ella a
  cualquier hora sin mirar el hambre (Wolf::HideInLair 0x421A2E).
- **No se puede comer** (+0x25 & 0x40, `components::CannotBeEaten`): lo pone el vórtice de tierra a tierra a todo lo que
  sale de él (fn_005FE3B0 0x5FE5DD) y los objetos de los puzles; no es presa y, si ya estaba derribado, sobrevive.

## Manchas y malla de los animales

(Movido de rendering.md.)

### Manchas (hechas)

Informe: `tmp_dis\render\animal_notes.txt`, datos `animal_ebone_dump.txt`.
- En `fn_00812170`: si no es humano, con `IsHumanShadowed` (flag 0x4000000, `SetHumanShadowed(1)` en el Create de cada
  especie; 0 mientras la criatura lo sostiene), y > 0,2 y malla con `ContainsEBone` → `fn_0081FFF0(obj, normal, ebone)`.
- Bloque EBone (836 bytes) tras los de huella (tamaño en +8), UV2, nombre y métricas extra: `u32 tamaño; float m[16][12];
  int32 hueso[16]`. Se usan las posiciones de m[0..3] en el espacio de su hueso: P = objeto × hueso × pos, y = suelo + 0,2.
  Par (0, 1) siempre, par (2, 3) si hueso[2] ≠ −1 (todos los cuadrúpedos: 4 quads). Aves y murciélagos no tienen EBone.
- **Rareza del original**: el primer quad de cada par recibe V = D (construye D + (P1 − P0)/2 pero pasa &D); el segundo
  D + (P0 − P1)/2.
- openblack: `L3DFile::GetEBone`, `L3DMesh::GetBlobPoints`, bucle de animales en `Renderer::DrawHumanShadows`.

### Creación: malla y escala

- `CREATE_ANIMAL` (24, "ANNN": tipo, rebaño, pueblo) y `CREATE_NEW_ANIMAL` (25, "ANNNN": + edad) → `fn_00419D10`
  (rebaños y clases en objects-and-resources.md). Malla: `Object::CallVirtualFunctionsForCreation` 0x636BE0 da al
  LH3DObject `GetDetailMesh(2, 1, 0)` (info +0x1FC + 4k: alta, std, baja) y el LOD es siempre 1: **la std** (también
  `GetMesh`); openblack usaba la alta. Escala (`InitialiseScale` 0x417B20): jóvenes
  ageToScale[edad − 1] + FloatRand(0,75·(ageToScale[edad + 1] − s)); adultos 1,05 − FloatRand(0,1). Sin ángulo inicial.
- Land1 crea 116 (palomas 40, gaviotas 22, golondrinas 14, caballos 12, vacas 10, cerdos 7, tortugas 6, murciélagos 5);
  openblack los crea todos, con la IA y los clips de esta página.
- openblack: `components::Animal`, `AnimalArchetype`.

## Diferencias y pendiente

- El rodeo (`ECS/AnimalWallHug.*`): el orden de los objetos de una celda es el del registro de openblack, no la lista de
  la celda (el barrido de la órbita decide por cotas, así que un empate o un orden distinto puede elegir otro corte); la
  pertenencia a la celda se calcula (círculo de 7,1 m; árbol en su celda) en vez de leer la lista del mapa; un círculo
  cuyo objeto se borra se suelta [inferred]; la contabilidad compartida `g_CircleHugStateInfo` / `DoWallHuggerLookahead`
  (0x609A50, un aldeano fantasma que simula el camino) no se porta; arcos y rumbos en metros, no en MapCoords enteros.
  Dos comprobaciones de openblack que el original no hace (lee `GetObjectPtr()` sin mirar): sin círculo al empezar la
  órbita o el barrido, solo posible si su objeto se borró [inferred]. `+0x76` se pone a 0 siempre al preparar el paseo
  (el original solo con entrada en `g_CircleHugStateInfo`; nadie lo lee antes de reescribirlo) y el campo `+0x78`
  (1 / 0x10) no se lleva [inferred: sin identificar, no se lee en el camino de los animales].
  El port de openblack para aldeanos (`PathfindingSystem`) tiene fallos propios (wallhug.md §7).
- Guaridas (`ECS/AnimalLairs.cpp`): `GUtils::GetDistance` se porta tal cual (`hypotenuse` 0x74F680 con la raíz
  inversa aproximada de la tabla de 1024 entradas 0xDA5A10, ~0,1 %), pero sobre las posiciones float de openblack
  truncadas a MapCoords. La búsqueda de agua del tigre no se porta (no cambia el resultado). `flock +0x5C`
  (sin guarida) no se porta porque el original solo lo pone a 0 (predator_ai.md). Los empates de distancia de bosques
  grandes y árboles eligen el objeto más nuevo por el índice de creación, como las listas con inserción en cabeza del
  original. El orden de los árboles crecidos de un bosque (`GrownTreesByDistance`, de "arboles") usa la distancia 3D al
  centro; el original (`DistanceToForest` 0x53A890 = `GetDistanceInMetres`) la mide solo en x / z: en laderas puede
  cambiar qué árbol es la guarida.
- Aldeano comido: desaparece (sin cadáver, alineamiento, avisos del pueblo ni duelo de los vecinos); los aldeanos no
  huyen de los depredadores ni toman reacciones.
- Scripts (script_flags.md): el vórtice no existe en openblack, así que nada tiene aún la marca «no se puede comer»
  (necesita Milagros: `script_held::SetCannotBeEaten` en lo que sale del vórtice); no hay bandadas de script
  (FLOCK_CREATE / FLOCK_ATTACH sin implementar: ni DisbandId ni sus referencias); al soltar un aldeano no se porta
  `Villager::ReleaseFromScript`; `SetScriptState(0x20)` del animal soltado no se porta (fn_0041AA00 lo pisa al
  momento); el motivo de muerte SACRIFICE (7), que borra un cadáver aunque esté controlado, no se lleva. Los buscadores
  del CHL de openblack (CALL, GET_...) no llaman a `AddScriptGameThing`: el hueco se crea con la primera referencia
  [approximated]. Al soltar un animal, «en la física o en la mano» (+0x24 & 0x44) es en openblack el componente de
  física o los estados IN_HAND / FLYING [approximated], y la marca g_game +0x14 & 0x8000 (scripts parados: con ella
  `GScript::Process` no hace `LookIn`, 0x6EB6C7) no existe en openblack. Las referencias del LHVM son fieles: el
  ScriptLibraryR.dll original llama a los nativos ADD_REFERENCE / REMOVE_REFERENCE desde POP (0x10008BC0: el objeto
  nuevo y el viejo de la variable) y al parar una tarea (0x10006604); los parámetros de una tarea las reciben por
  el POP de su prólogo (script_flags.md §6).
- Fusión de bandadas (0x41A690 / 0x41A790): el original no fusiona una bandada con pastor (Flock +0x30, el aldeano de
  `VillagerBecomesShepherd` 0x768C1C: la propia o la ajena) ni dos de distinto jugador (`GetPlayer`); openblack no tiene
  pastores ni jugador de bandada, así que solo mira el pueblo de la propia [approximated].
- Pastores, la prioridad de líder de FLOCK_ATTACH, el aterrizaje de las aves (inalcanzable en el juego: info.sleep 0), el orden
  exacto de las listas de cada celda del mapa y el turno intercalado con los aldeanos.
- Los números aleatorios son los de openblack (mt19937), no el GameRand del original.
- Dejar un objeto suave con la mano lo coloca al momento; el original lo suelta en la física (mano, Tareas.txt).

## Ganchos de prueba

`OPENBLACK_ANIMAL_TRACE=1` (cambios de estado, landType y cada 50 turnos cuántos hay en cada estado),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distancia[,ángulo[,cada]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turno[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turno"`, `OPENBLACK_TEST_ANIMAL_SPECIES=<AnimalInfo>` (n cuenta solo esa especie),
`OPENBLACK_TEST_HUNGRY=<AnimalInfo>` (esa especie con hambre en el turno 1), `OPENBLACK_TEST_SPREAD_REACTIONS=<turno>`
(los depredadores reparten otra vez su reacción de huida), `OPENBLACK_TEST_HUNT_VILLAGER="<especie>,<turno>"`,
`OPENBLACK_TEST_FOOD_PILE="<especie>,<turno>"`, `OPENBLACK_TEST_FOOD_BEHIND="<especie>,<turno>[,<tipo>[,<m>]]"` (el
primero de esa especie, con hambre, a 8 m delante del edificio (0; no un campo, que el iterador de círculos salta) o
elemento (2) de 2..8 m de radio o del árbol (1) más
cercano y una pila de comida m (6) detrás; su recorrido cada 2 turnos; con la traza, cada cambio del rodeo),
`OPENBLACK_TEST_SMOKE=<n>`, `OPENBLACK_TEST_CORPSE_TURNS=<n>`, `OPENBLACK_TEST_LAIRS=<turno>` (lista los bosques y
cada líder depredador recalcula su guarida; con la traza, la guarida elegida y la regla),
`OPENBLACK_TEST_SCRIPT_HELD="n,turno[,suelta]"` (el n-ésimo animal queda retenido por un script como si lo hubiera
creado un CREATE; en `suelta` pierde la referencia; cada 25 turnos su estado, contador y marcas),
`OPENBLACK_TEST_CANNOT_BE_EATEN=<turno>` (ese turno todos los animales y aldeanos reciben la marca del vórtice; con
`HUNT_VILLAGER` antes del derribo no hay caza, después el aldeano se levanta),
`OPENBLACK_TEST_VIEW_LOCK=1` (la cámara se coloca cada turno
junto al animal, para las aves). Land2 (`-s Land2.txt`) tiene leones, tigres y lobos. `dev\tools\animales_shot.sh <nombre> <fotogramas> <captura> [VAR=valor...]` lanza una
copia privada en `dev\animales_run`.
