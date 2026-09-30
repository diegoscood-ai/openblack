# Animales: IA, estados y clips

Investigación con direcciones: `C:\Users\diewgarc\dev\tmp_dis\animals\grazing_ai.md` (IA de los herbívoros),
`hand_death.md` (mano, vuelo, aterrizaje, muerte) y `openblack_plumbing.md` (qué reutiliza openblack). Scripts en la
misma carpeta (`dumpanimals.py` valores de info.dat, `vtd.py` vtables, `a.txt` desensamblado de Animal.cpp).
bw1-decomp solo tiene stubs vacíos de Animal*.cpp: todo sale de runblack.exe.

En openblack: `ECS/AnimalAI.*` (estados por turno), `ECS/AnimalAnimations.*` (clip por estado y especie),
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
- Sin hacer: IA de los depredadores (cazar, acechar; ahora se quedan quietos en DECIDE), vuelo de las aves (siguen sin
  crearse), fusión de bandadas tras aterrizar, crecer con la edad, reacciones (huir de la mano, de depredadores),
  pastores, el humo del cadáver, animal lanzado a un almacén de comida → comida.

## Ganchos de prueba

`OPENBLACK_ANIMAL_TRACE=1` (cambios de estado, landType y cada 50 turnos cuántos hay en cada estado),
`OPENBLACK_TEST_VIEW_ANIMAL="n[,distancia[,ángulo[,cada]]]"`, `OPENBLACK_TEST_THROW_ANIMAL="n,turno[,vx,vy,vz]"`,
`OPENBLACK_TEST_KILL_ANIMAL="n,turno"`. `dev\shot_animal.sh <nombre> <fotogramas> <captura> [VAR=valor...]` lanza una
copia privada en `dev\animales_run`.
