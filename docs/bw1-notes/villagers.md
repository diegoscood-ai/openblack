# Aldeanos: datos, máquina de estados y velocidad

El aldeano de runblack.exe (W120) tal como lo porta la sesión "aldeanos" (rama `local/aldeanos`, hito V1: el núcleo de
la máquina de estados). Los clips (qué clip por estado, transiciones, tamaño) siguen en [animation.md](animation.md);
aquí van los datos del aldeano, el turno, los cambios de estado y las reglas de velocidad. Investigación con
direcciones: `C:\Users\diewgarc\dev\tmp_dis\aldeanos\` (`V1_spec.md`, `PLAN.md`, `core.md`, `team_apis.md`, volcados en
`core\` y `v1\`).

En openblack:
- `ECS/Villager/VillagerCore.{h,cpp}`: el turno (ProcessState, CheckEveryTime), los cambios de estado (SetTopState,
  SetCurrentAndDestinationState, SetState, salidas y entradas), la creación (constructor), CREATED (85), la pausa (239),
  SetupMoveToWithHug, la muerte provisional;
- `ECS/Villager/VillagerStateInfo.h`: nombres de los campos de la tabla de estados de info.dat;
- `ECS/Villager/VillagerStateTable.h`: la fila de la tabla de funciones (estado, entrada, salida, validate...);
- `ECS/Villager/VillagerOriginalFns.h`: las direcciones de las funciones de cada fila del original (para los avisos);
- `ECS/Villager/VillagerDebugHooks.cpp`: la traza y los ganchos de prueba;
- `ECS/Systems/Implementations/LivingActionSystem.cpp`: la tabla (`k_VillagerStateTable`) y el bucle del turno;
- `ECS/Components/Villager.h` (los campos), `LivingAction.h` (los tres estados y los contadores), `Town.h`
  (`TownDesire`), `ECS/Villager/VillagerAge.h` (edad), `ECS/Archetypes/VillagerArchetype.cpp` (la creación),
  `ECS/VillagerSpeed.*` (velocidad), `ECS/VillagerAnimations.*` (clips de los cambios de estado);
- `test/test_villager_core.cpp`: 20 casos (ver [Pruebas](#pruebas)).

## Campos (Villager.h)

| offset | campo | notas |
|---|---|---|
| Object +0x48 | `life` | 0..1 (`ecs::life`) |
| Living +0xA0 | `birthTurn` | Living::SetAge 0x5ED2C0: `turno − edad·1500`; la edad es Living::GetAge 0x5ECAF0 = `(turno − birthTurn) / 1500` sin signo (`villager::GetAge`). 1500 = GGameInfo +0xC (0xD01A04) |
| +0xE0 | `flags` | ver [Flags](#flags) |
| +0xE8 | `food` | lo que tiene en la tripa; el constructor lo deja en [0,5; 1,1) |
| +0xEC | `lastCheckTurn` | turno del último check periódico (lo reinicia CheckHungry 0x75BEDF) |
| +0xF0 | `foodSpeedUp` | IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430 |
| +0xF2 | `discipleType` | g_DiscipleInfos 0x99A1F8 |
| +0xF4 / +0xF6 | `resourceHeld` | comida y madera que lleva |
| +0xF8 | `pregnancy` | turnos que faltan (0 = no) |
| +0x100 | `mother` | |
| +0x118 | `targetThing` | TargetThing (bw1-decomp `Villager.h`) |
| +0x11C | — | unión Football* / TradeTown / WanderArea (bw1-decomp `Villager.h`). V1 no la añade: ningún código de V1 la lee |
| +0x128 / +0x12C | `abode` / `town` | |

`lifeStage` es un espejo de `flags & 0x8` (lo leen DetailMeshes, el dibujo y los sonidos) y `sex` es un espejo de
GVillagerInfo +0x1F8 (`IsWoman` 0x752620 y SetSpeed leen el de info). `task` no es del original.

Faltan del original (llegan con su hito): `next` +0xE4 (lista de la casa, V4), `building_site` +0xFC (V6),
`LastPlayerToInteract` +0x104 (V12), +0x108 / +0x10C / +0x110, `fire_effect` +0x114 (Milagros), +0x124.

### Flags

Villager +0xE0 (escritores directos: `tmp_dis\aldeanos\v1\scanE0.py`): 0x1 tras un toque en su casa
(SetupAfterTapOnAbode), 0x2 en el sitio de culto (AddVillagerToWorshipSite / RemoveVillagerFromWorshipSite), 0x4 dentro
de casa (ArriveHome), 0x8 niño (SetAge; CheckChildGrownUp lo borra), 0x10 de camino al culto o a un fuego, 0x20 en la
mano (InterfaceSetInMagicHand; EndPhysics / Landed lo borran), 0x80 fútbol / guion, 0x200 / 0x400 discípulo /
seguidor, 0x800 / 0x1000 clip de entrada / salida (en openblack `SkeletalAnimation::transitionFlags`), 0x2000 yendo a
dormir (CheckWhenGoingToBed). En V1 solo se escribe 0x8 (SetAge); 0x2, 0x4 y 0x200 se leen. El 0x2 de culto lo
escribe Milagros en `WorshipVillager::atSite` (AddVillagerToWorshipSite 0x76C3F0 / RemoveVillagerFromWorshipSite
0x76C440, team_apis.md (C)); CheckEveryTime (0x7504DC) lee `flags & 2` **o** `atSite` hasta que pase a `flags`. 0x4 y
0x200 hoy no los pone nadie.

## Tabla de estados de info.dat (VillagerStateInfo.h)

`GVillagerStateTableInfo::Infos` 0xDB9E68, 0x114 bytes por estado; el desensamblado lee cada campo en el offset del
fichero + 0x10. Los campos de `InfoConstants.h` conservan sus nombres `field0x..` (otras sesiones los usan);
`state_info::` les da nombre: `Clip` (0x00), `ServedDesire` / `ServedDesireAmount` (0x04 / 0x08, AdjustTownModifier),
`IsFinal` (0x0C), `NotStoredAsPrevious` (0x10), `IsMoving` (0x14), `ResumeState` (0x20), `SpeedGroup` (0x24),
`CanPauseForASecond` (0xC4), `NoGoHomeWhenHurt` (0xD8), `DoPeriodicChecks` (0xE4), `GoHomeWhenHurt` (0xE8),
`NoOutOfClip` (0xF0), `LifeDrainPerTurn` (0xF8)... (lista completa en el fichero).

## La tabla de funciones (VillagerStateTable.h)

La del original está en 0xD09198, 0x90 bytes por estado: +0x00 estado, +0x10 entrada, +0x20 salida, +0x30/+0x40
save/load, +0x50, +0x60 (función de clip), +0x70 (clip de transición), +0x80 validate. En openblack:
- la **salida** devuelve **1 = puede salir** y recibe el estado siguiente; la **entrada** recibe (final de antes,
  estado nuevo) y devuelve **1** (aceptada), **0x23** (aceptada; la función fijó los estados) u otro valor (rechazada);
- una ranura vacía es "sin función": vale 1, como en el original;
- un estado sin portar (`k_TodoEntry`) no hace nada (devuelve 0) y avisa **una vez por estado** con la dirección del
  original; si el original tiene entrada, salida o validate en una fila que openblack deja vacía, avisa una vez
  (`TODO: Unimplemented entry function of ... (0x...): taken as 1`);
- las salidas de culto de Milagros (58, 59, 60, 213) conservan su convención vieja (false = puede salir) y la fila las
  adapta;
- las de fuego están en sus filas, como las rellena `_$E32` (ver [Fuego en la tabla](#fuego-en-la-tabla));
- 16 DROWNING: EnterDrowning 0x767410 y ExitDrowning 0x767420 solo aceptan (`mov eax, 1; ret`); la función de estado
  (Villager::Drowning 0x76A780) llega con agua.

### Fuego en la tabla

El inicializador estático `_$E32` (0x5AA2D9..0x5AA767) pone en +0x10 / +0x20 de cada fila:

| fila | estado | entrada (+0x10) | salida (+0x20) | dónde en `_$E32` |
|---|---|---|---|---|
| 215 | REACT_TO_FIRE | — | ExitReaction 0x7527A0 (el thunk 0x5B0100 = `jmp [vt +0x910]`) | — |
| 216 | PUT_OUT_FIRE_BY_BEATING | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA2D9 / 0x5AA2EC |
| 217 | PUT_OUT_FIRE_WITH_WATER | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA421 / 0x5AA434 |
| 218 | GET_WATER_TO_PUT_OUT_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA4BE / 0x5AA4CB |
| 219 | ON_FIRE | EnterOnFire 0x75AF30 | ExitOnFire 0x75AF80 | 0x5AA611 / 0x5AA642 |
| 220 | MOVE_AROUND_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA75D / 0x5AA767 |

Lo que devuelven (leído en el desensamblado):
- **EnterPutOutFire(final, s)**: 1 si `IsStateEntryFunctionSameAs(final, s)` 0x7524D0 (las dos filas tienen la misma
  entrada: de 216/217/218/220 a otra de ellas); si no, con fuego (+0x114) vivo (fn_0075AD90; si ya no existe, +0x114 =
  0), su vt +0x2C y una reacción (+0x94) no apagada (Reaction +0x34, la pone ShutDown 0x6E4723): **0** si ya está en
  la lista de bomberos de la raíz (0x75AE75), y si no lo añade (AddFireman 0x7309A0) y da **1**. En cualquier otro caso
  da **0** y, si el final de antes es reactivo (tabla +0xB8, 0xDB9F30), StopReacting (vt +0x998). Un 0 es 0x2F y
  Villager::SetTopState entra en 163.
- **ExitPutOutFire(s)**: siempre **1** (0x75AECB y 0x75AF20). Si la salida no es "la misma" (vt +0x96C,
  0x752530: otra fila con ExitPutOutFire, o un estado no final) sale de la lista de bomberos y de la de camino al
  culto del pueblo (0x73E360); si no estaba en la lista, solo +0x114 = 0 y vuelve sin ExitReaction; si no, ExitReaction
  0x7527A0 (acaba la reacción salvo que `s` sea reactivo).
- **EnterOnFire(final, s)**: **1** sin fuego o con su vt +0x2C a 0; **0** si ya está en la lista (0x75AF78); si no lo
  añade y **1**.
- **ExitOnFire(s)**: siempre **1**; sale de la lista si está y **+0x114 = 0 siempre** (no mira `s`).

Los cambios de estado de `VillagerFire.cpp` y `VillagerTeleport.cpp` son ya los del núcleo: `villager::SetTopState`
(con la tirada de pausa, la salida de TOP y del final y la entrada, una vez cada una, y los códigos 1 / 0x2E / 0x2F;
los dos pasan por `villager_reactions::SetTopState`, que además acaba el paseo de openblack),
`villager::SetState(2, s)` para el estado guardado (vt +0x938: salta los de tabla +0x10 y ajusta el pueblo) y
`villager::SetupMoveToWithHug`. Ya no hay SetTopState local, ni `CallEntry` / `CallExit`, ni
`villager_fire::CallFinalStateExit`; las salidas de culto (58, 59, 60, 213) corren por sus filas. Además:
- SetupMoveAroundFire 0x75A770 solo guarda destino y estado siguiente si SetTopState(220) da 1 (0x75A783).
- PopFromPrevious 0x751E50 (`villager_reactions::PopFromPrevious`, uno solo para fuego y teletransporte):
  SetTopState del estado de reanudación de lo guardado (Infos +0x30 0xDB9E98 = fichero 0x20); si da 0x2E, TOP = 163 en
  bruto (LivingAction::SetState 0x5ECC90) y después PREVIOUS = 0 en bruto. **Sin nada guardado** es la fila 0, cuya
  reanudación es **0** en info.dat: SetTopState(0) = INVALID_STATE, como el original (Living::InvalidState 0x5EC1D0
  devuelve 0 cada turno). Antes se inventaba 163.
- MoveAroundFire 0x75A7E0 al llegar: PopFromPrevious (0x75A815) y PREVIOUS = 163 **en bruto** (0x75A81A..0x75A827,
  LivingAction::SetState 0x5ECC90 con ecx = +0x8C: sin el salto de la tabla +0x10 ni el pueblo), no Villager::SetState.
- ResetStateAfterReacting 0x751E10 (vt +0x9A0): PopFromPrevious y, si el estado final es reactivo (fichero 0xB8),
  SetTopState(163). StopReactingAndSetState 0x5F11C0 (vt +0x99C): eso y luego StopReacting si sigue reaccionando. Lo
  usan ReactToFire (0x765A48, ya bombero) y TeleportReaction (0x76642F, tras el salto; antes iba al revés: primero
  StopReacting y luego PopFromPrevious).
- **(aproximado)** La salida de MOVE_TO_POS (ExitMoveToPos 0x5EDDA0) no está portada: estos SetTopState quitan las
  marcas de paseo de openblack (salvo con 0x2E), como hacía el SetTopState local.

Diferencias con antes (comprobadas en partida, ver abajo): las entradas y salidas corren también cuando el estado final
no cambia, como en el original. Al replanificar o llegar (SetupMoveToWithHug / SetTopStateToFinal) un bombero en 220
pasa por ExitPutOutFire(220) y EnterPutOutFire(220, 220), que no cambian nada; un aldeano en 219 pasa por ExitOnFire,
que **borra +0x114**: tras su primer paseo deja de huir del fuego ajeno y solo sigue en 219 mientras arde él (en el
original igual: OnFire 0x75B1E0 llama a SetupMoveToWithHug 0x5F2890 en 0x75B39E).

**La pausa 239 no sale en fuego ni teletransporte.** CanPauseForASecond 0x752120 mira la fila del estado **de
destino** (Infos +0xD4 en memoria, 0xDB9F3C = fichero 0xC4), que es 0 en 163, 201, 202 y 215..220: esos SetTopState
no tiran la pausa. Solo puede salir cuando PopFromPrevious vuelve a un estado que pausa (un oficio con 0xC4 = 1) o en
248 (culto).

### Salidas de las reacciones (ExitReaction, ExitReactToTeleport)

- **ExitReaction 0x7527A0** (vt +0x910; las filas guardan el thunk 0x5B0100 = `jmp [vt +0x910]`, 45 filas en `_$E32`):
  CircleHugInfo::Reset(+0x70) 0x60A9F0 (en openblack quita `WallHugObjectReference`, **aproximado**), y si `s` no es
  reactivo (IsReactiveState en línea, Infos +0xC8 0xDB9F30 = fichero 0xB8) StopReacting (vt +0x998). Devuelve 1.
  En la tabla: 215 y las filas sin portar 6-9, 203 y 214 (`TodoWithExitReaction`); las demás filas con ese thunk
  (12, 19-22, 25, 26, 30, 140-168, 194-196, 205-208, 227, 231, 235-237) siguen vacías porque nadie entra en ellas.
  ExitPutOutFire la llama al final (0x75AF19).
- **Villager::StopReacting 0x7637D0** → **Living::StopReacting 0x5F1140**: con TOP 203 y bailando, RemoveFromDance(1)
  (sin portar: 203 no tiene función de estado); con reacción (+0x94): fuera de la lista de la reacción,
  `fn_005F0FE0(tipo)` = el registro del tipo recibe el turno (`reactions::RefreshRecord`), +0x94 = 0; +0xBC = 0 siempre.
  En openblack `villager_reactions::StopReacting` llama a `villager_fire::StopReacting` (reacción 0, objeto nulo,
  RefreshRecord de REACT_TO_FIRE) y `villager_teleport::StopReacting` (borra su estado, RefreshRecord de
  REACT_TO_TELEPORT). EnterPutOutFire (0x75AE55) también la usa.
- **ExitReactToTeleport 0x766390** (salida de 201, 202 y 251): si no `IsStateExitFunctionSameAs(s)` (vt +0x96C
  0x752530), sale de la lista de camino al culto de su pueblo (GetTown vt +0x48 → 0x73E360) y +0xE0 &= ~0x10 (también
  sin pueblo); luego ExitReaction(s) y su resultado.
- **IsStateExitFunctionSameAs 0x752530** (`villager::IsStateExitFunctionSameAs`): la salida de la fila de
  GetFinalState y la de `s` son la misma (los punteros de 16 bytes enteros; aquí las direcciones de
  `VillagerOriginalFns.h`, dos vacías cuentan como iguales) → 1; si no, `s` final (0xDB9E84) → 0, si no 1. ExitPutOutFire
  (0x75AE95) también la usa ya, en vez de la lista a mano.

## Creación (Villager::Create 0x74FBE0 y el constructor 0x74F950)

`Villager::Create` tira primero `GameRand(10) <= 1` (0x74FBF0) para intentar un SpecialVillager (TODO V14: no hay, así
que siempre sale uno normal). Después, el constructor (`villager::Construct`):
1. Living::Living (vida = info.life) y SetToZero 0x74FB20 (todos los campos nuevos a 0).
2. SetAge 0x7528C0: niño si `edad < grownUpAge` (`flags |= 8`), si no `edad = max(edad, 18)` y `flags &= ~8`; mallas y
   escala (gasta FloatRand); `birthTurn = turno − edad·1500`.
3. `foodSpeedUp = 0` (0x74FA02); mujer (info +0x1F8 == 1): `pregnancy = 0` (0x74FA08).
4. food (0x74FA18..0x74FA67): `min(1, GameFloatRand(0,6) + hungryForFood)` es una macro que evalúa dos veces: si la
   primera tirada da < 1 se tira otra vez y se guarda la segunda sin recortar.
5. lastCheckTurn (0x74FA6D..0x74FACD): `turno − (GameRand(processChecksEvery) < turno ? GameRand(...) : turno)`. La
   segunda tirada puede pasar del turno: en el turno 6 con una tirada de 7 queda 0xFFFFFFFF (literal; la resta sin signo
   de GetGameTurnsSinceLastChecked lo deja bien).
6. Contador de estado Object +0x58 (`turnsUntilStateChange`) = GameRand(500) + 1 (0x74FAB0).
7. **Regla del agua** (0x74FADC..0x74FAF5): `SetState(TOP, IsWater(pos) ? 16 DROWNING : 85 CREATED)`, el SetState
   exacto, sin entrada, clips ni velocidad, y sin llamar a nada del agua. `IsWater` = MapCoords::IsWater 0x6035B0
   (`pot_resource::IsWater`; con la fusión de agua, `sea_cells::IsWater`).
8. `++g_game+0x205A54` y SetSkeleton 0x7562C0: TODO(V12).

Después (como antes de V1) el arquetipo pone la casa y el pueblo (en el original, CallVirtualFunctionsForCreation y
AddVillagerToAbode: V4) y la velocidad de 85 **(aproximado)**.

En Land2 el guion crea tres aldeanos en mar abierto (1578, 2220) en el turno 6: nacen en 16 DROWNING, como en el
original; hasta que llegue la función de estado de agua se quedan quietos en el agua.

## El turno (Villager::ProcessState 0x74FF70)

`LivingActionSystem::Update`: primero los ganchos de prueba, luego para cada aldeano (orden del registro)
`ProcessReaction` 0x5F1270 (no-op: TODO Milagros M-5) y `ProcessState`; al final `FlushDeaths`.
**(aproximado)** el original lleva aldeanos y animales en una sola lista (g_game +0x205BBC) y el paso del camino
(Living::MoveToPos 0x5EC270) va dentro de la función de estado; openblack mueve a todos antes (PathfindingSystem) y
procesa los animales después.

ProcessState:
1. `++turnsSinceStateChange` (+0x90) y ProcessFoodSpeedup 0x753430 (`foodSpeedUp != 0 && turno % 10 == 0` → −1).
2. validate (+0x80) del TOP y del FINAL crudo; el resultado no se usa.
3. Si suena un clip de entrada / salida (flag 0x800): espera a que acabe (IsReadyForNewAnimation 0x5EC960 →
   FinishedIntoOutOfAnimation 0x750060) y no hace nada más ese turno.
4. CheckEveryTime 0x750410 y CallState 0x7521D0 (la función del TOP).

CheckEveryTime:
- controlado por un guion (+0x25 & 4; lo escribe GameThingWithPos::SetControlledByScript 0x402240, en openblack
  `ecs::script_held`) → nada;
- desgaste: si el TOP es móvil, la vida baja `LifeDrainPerTurn` del TOP y desde ahí se mira la fila del **FINAL
  crudo**; si no, baja la del estado final (GetFinalState) y se mira la fila del TOP (rareza literal);
- si la fila tiene checks (`DoPeriodicChecks`): vida == 0 exacto → VillagerDead(CHANT si el final es 248-250 o
  flags & 2 / `WorshipVillager::atSite`, si no EXHAUSTION); si han pasado **más de** `processChecksEvery` (8) turnos → el check periódico, cada 9
  turnos: CheckDeathFromOldAge cuando `(turno + UniqueId) % 800 < t`; herido (`vida < 0,3`, fuera de casa, la fila
  permite ir a casa, no derribado, y en 19/20 solo con `food > hungryForFood`) → SetTopState(36 GO_HOME) (0x7505C3).
  **Hasta V4 la regla se evalúa pero no se aplica** (TODO(V4); `SetGoHomeEnabledForTests` la enciende en las pruebas):
  la fila 36 no tiene función de estado (GoHome 0x760270 → DoGoingHome 0x760280: casa, sendero, ARRIVES_HOME) y el
  aldeano herido se quedaría quieto para siempre (la auditoría vio 9 congelados en Land1 con `LIFE=0.25`); la traza
  escribe `hurt (life ...): GO_HOME skipped until V4`;
  CheckChildGrownUp, WomanSpecial y CheckHungry (que reinicia `lastCheckTurn`);
- si no: el check del discípulo (flags & 0x200, tipo que ignora necesidades según g_DiscipleInfos +0xC, FINAL crudo
  221, pueblo con +0x5E8) → 163. Town +0x5E8 aún no existe (0).

## Cambios de estado

Códigos: 1 hecho, 0x2E la salida rechazó (no cambia nada), 0x2F la entrada rechazó.

- **Villager::SetTopState(s)** 0x752010: si `CanPauseForASecond(s)` (TOP ≠ 239, fila con pausa, sin guion):
  `x = 1 − vida` (vida·0,5 si está envenenado), y si `GameFloatRand(1) − 0,5·x³ < pauseForASecondChance (0,01)` →
  SetupPauseForASecond(s) 0x76B090 = SetCurrentAndDestinationState(239, s). Si no, Living::SetTopState; si da 0x2F,
  CallEntryStateFunction(163).
- **Living::SetTopState(s)** 0x5F28E0: salida(s) → 0x2E; `out` = CallOutofAnimationFunction(s) (0x5F2900); entrada(s)
  → 0x2F; SetStateSpeed (0x5F291B, sin condición); si `out ≠ −1`, SetAnim(out); si no, SetStateAnim 0x5ECB10 y
  CallIntoAnimationFunction(s) (0x5F2947).
- **Living::SetCurrentAndDestinationState(c, d)** 0x5F2980: igual, pero la salida (0x5F298B), el clip de salida
  (0x5F299C) y el clip de entrada (0x5F29EB) reciben **`d`**; solo la entrada doble (vt +0x908) recibe los dos.
- **CallExitStateFunction(s)** 0x752320: la salida del TOP y, si el estado final es otro, también la suya; 1 solo si las
  dos dan 1.
- **CallEntryStateFunction(s)** 0x7523D0: `entry[s](final, s)`; 1 → SetState(0, s). La doble 0x752440: la de `c`, luego
  `entry[d](final de antes, d)`; 1 → SetState(1, d).
- **Villager::SetState(i, s)** 0x753690: PREVIOUS no guarda un estado con 0x10; el estado viejo de cualquier índice,
  si es final, sale de los modificadores del pueblo, y el nuevo entra (también en PREVIOUS: rareza literal); fijar TOP
  borra antes FINAL (con su ajuste) y pone +0x90 a 0. No toca el contador +0x58, ni clips, ni velocidad.
- **AdjustTownModifier** 0x753560: `town.desire.doingNow[d] ±= cantidad` y `doingNowCount[d] ±= 1` (floats de
  TownDesire, town +0x510 / +0x554).
- **SetupMoveToWithHug(pos, final)** 0x5F2890: SetCurrentAndDestinationState(GLivingInfo +0x124 `moveState`, final)
  (0x5F2894..0x5F28AF; las 63 filas de aldeano de info.dat tienen 1 MOVE_TO_POS) y, solo si da 1, el paseo
  (WallHug). Lo usan fuego y teletransporte (`VillagerMove.h` reenvía a VillagerCore).
- **Llegada de MOVE_TO_POS** (Living::MoveToPos 0x5EC270): MobileWallHug::MoveTo == 0xA → SetTopStateToFinal
  (0x5EC28E) = Villager::SetTopState(FINAL): tirada de pausa, salida de MOVE_TO_POS (ExitMoveToPos 0x5EDDA0,
  CircleHugInfo::Reset, sin portar: vale 1) y salida / entrada del FINAL. MoveTo 0x60AF20 solo da 0xA en ARRIVED
  (0x60AFC0, si AreWeThere(0)) y en FINAL_STEP (0x60AF6C), y las dos ponen antes el objeto en el destino (Pos = destino,
  MoveMapObject vt +0x55C); si no devuelve 0 / 1 / 6 / 7 y Living::MoveToPos no hace nada: **no hay paseo
  "abandonado"**. En openblack (`WallHugMoveToResult`): llega cuando tiene la marca FinalStep (o Arrived) y ya está en
  su destino (PathfindingSystem la pone con AreWeThere y lo coloca en el destino al turno siguiente, como el original
  da 0xA un turno después de que STEP_THROUGH ponga FINAL_STEP). Antes un paseo que el PathfindingSystem abandonaba
  (sin marcas) contaba como llegada donde estuviera: así los aldeanos del teletransporte daban vueltas 1 ⇄ 201 sin llegar
  nunca a la piedra.
- **Los casos sin portar del PathfindingSystem** (destino dentro del círculo que rodea, TODO #864, y el paso de un
  círculo a otro, TODO #865; en el original MoveToCircleHugCircleSquareSweep<0/1> 0x614C40 / 0x6159F0): `AbandonMove`
  ya no suelta el paseo, sigue en STEP_THROUGH (0x60B02A: recto al destino, sin obstáculos) **(aproximado)** y el
  aldeano llega por AreWeThere. Además el ARRIVED del PathfindingSystem estaba al revés (salía de ARRIVED justo cuando
  había llegado; 0x60AFC0 sale cuando **no** ha llegado).
- El paseo ocioso inventado (radio 40, FINAL 209) **ya no existe** (V2): 163 es `Villager::DecideWhatToDo` 0x7515C0
  (ver "Decidir qué hacer y el ocio (V2)"). Solo las herramientas de depuración ("Move To Point") preparan paseos con
  FINAL 0, que al llegar vuelven a 163 por el camino de compatibilidad (el original haría SetTopState(0)).
- CallOutofAnimationFunction 0x756620 / CallIntoAnimationFunction 0x756590: ver [animation.md](animation.md)
  (`VillagerCallOutOfAnimation`, `VillagerApplyStateClips`).

### Camino de compatibilidad (`LivingActionSystem::VillagerSetState`)

| llamada | qué hace |
|---|---|
| TOP, `skipTransition = false` (culto) | `villager::SetTopState` exacto |
| TOP, `skipTransition = true` (mano, física, animales, la llegada de un paseo con FINAL 0, LANDED, Gui) | si es el mismo estado, nada; si no, `villager::SetState(0, s)` exacto + clips y velocidad. **(aproximado)**: el original pasa por SetTopState con EnterInHand / ExitInHand..., no portadas |
| FINAL / PREVIOUS | `villager::SetState(i, s)` exacto |

## Estados 85 y 239

- **85 CREATED** (Villager::VillagerCreated 0x753DD0): `v = +0x58; +0x58 = v − 1; si v == 0 → +0x58 = 0 y
  SetTopState(163)`. Pasa a 163 en la llamada número contador + 1 (1..501 turnos).
- **239 PAUSE_FOR_A_SECOND** (0x76B0B0): cuando acaba el clip (IsReadyForNewAnimation(1)), SetTopStateToFinal 0x5ECA80
  = SetTopState(FINAL): corren otra vez la salida y la entrada de `s`; no vuelve a pausar (TOP == 239). Mientras dura,
  GetFinalState() = s.

## Velocidad (Villager::SetStateSpeed 0x753760)

Se llama sin condición en los dos SetTopState; los saltos son suyos: no cambia nada si el aldeano está controlado por
un guion (GameThingWithPos +0x25 & 4, 0x753766) o si baila (Living::IsDancing 0x5ECC10, 0x753772: el DanceGroup de
Living +0xD8; en openblack `WorshipVillager::dancing` o TOP == IN_DANCE, **(aproximado)**). Así el aldeano que entra
en 60 WORSHIPPING_AT_WORSHIP_SITE tras FindDanceGroup conserva la velocidad. Después toma el estado final y SetSpeed
0x750ED0 (fórmulas en [animation.md](animation.md)). La edad es GetAge (sin signo) y se compara **sin signo** con
grownUpAge (0x750F26, `jae`) y oldAge (0x750F87, `jbe`); las diferencias se cargan como qword sin signo, ×0,2×0,1,
máximo 0,4. El adulto resta
`GetDesireForFood()·0,1` (0x750FDB, POWER 0x75BB60 = `1 − min(food, 1)³`), `vida·0,1` y 0,2 si es mujer.

## Decidir qué hacer y el ocio (V2)

Spec completa: `dev\tmp_dis\aldeanos\V2_spec.md`. Código: `Villager/VillagerDecide.{h,cpp}`, `Villager/VillagerHome.{h,cpp}`,
`Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`; tests `test/test_villager_decide.cpp` (13 casos).

- **163 DECIDE_WHAT_TO_DO** (0x7515C0, devuelve 1): emergencia del pueblo (Town::IsInStateOfEmergency 0x747970, +0xF1C
  `Town::emergencyStartTurn`, nadie lo escribe aún: TODO(Milagros)) → 242; discípulo / seguidor (DiscipleDecideWhatToDo
  neutro, V14); `SetTopState(163)`; niño → ChildDecideWhatToDo (CheckChild, reparto del pueblo neutro, guardería neutra,
  → 114); CheckNeededForSomething (sin techo: neutro V4 → CheckNeededForSpecial: **culto de Milagros**, cívico (V3:
  calcula el trigger y borra `flags & 1`, reparto 0), deseos propios con umbral 0,3) → CheckTakeResourcesToStoragePit
  (→ 31) → SetupNothingToDo. El culto ya no va al principio de 163: está en su sitio (0x760013), antes de la rama ociosa
  y también se mira desde 246.
- Deseos: comida = 1 − min(food, 1)³, vida = 1 − ((vida − min(0,3, vida)) / 0,7)²; el mayor primero, comparaciones
  estrictas con 0. CheckSatisfySleep 0x761490 no mira la hora (con casa → 36). ChangeStateToFindFoodToEat neutro (V4).
- **SetupNothingToDo** 0x753B50: GameRand(9), tabla 0x753C64 = 0,1,1,1,2,2,2,2,2. Rama 0: casa funcional → 36; si no
  GameRand(100) < 10 → 36, si no cae a la 1. Rama 1: con casa → 245; si no cae a la 2. Rama 2: con pueblo, anda a
  GetChillOutPos (punto de reunión + R..10R, ±22,5° de su lado, R = 0,1·GTownInfo +0x140) con FINAL 246; si no 36.
  Siempre devuelve 1.
- **209** devuelve 1 (solo lo ponen guiones). **245** GoAndChilloutOutsideHome 0x76B3F0 y **252** GoAndChilloutInTown
  0x76B590 → GetMeToMyChillOutPos 0x76B610 (lejos: anda a GetPosOutside(3, R/2, R/2) de la puerta; cerca y libre
  (CheckForClearArea con 1,2·radio): LookAtPos un paso y 246; ocupado: FindClearArea(5, 1)). **246** SitAndChillout
  0x76B4E0: entrada 500 turnos (+0x394), luego un chequeo cada 101 llamadas (+0x396 = 100): emergencia, CheckNeededFor
  Something, GameRand(10) == 0 → SetupNothingToDo sin pasar por 163. Clip SitDown: el bit 0x800 antes del clip actual.
- **36 GO_HOME** (parcial) = DoGoingHome(37, 238): con casa, anda a la puerta con FINAL 37 (37 ARRIVES_HOME es V4: el
  aldeano queda quieto en la puerta). Sin casa: nada (tienda / vagabundo V4). La regla "herido → 36" de CheckEveryTime
  está encendida en el juego.
- **114 CHILD_FOLLOWS_MOTHER** 0x7578C0: CheckChild, reparto, guardería; si no, anda a la madre (o a la casa) + 5 m en
  un ángulo al azar (GameFloatRand(2π), VillagerChild.cpp 0x39) si el punto es navegable; sin madre ni casa,
  CheckNeedNewAbode (neutro V4). La fila 114 lleva +0x50 AlwaysReactToTownEmergency (0xD0D208 = 0x5AC990), como 36 y
  209.
- Pueblo: GetCongregationPos 0x7408B0 con caché `Town::congregationPos` (+0xF10; también la escribe
  SET_TOWN_CONGREGATION_POS); media de las casas que no son campos (con < 3, más los planos) y FindClearArea(130, 3, 10,
  BlocksTownClearArea); si no, base + 10..20 m. La lista de casas (+0x754) va de la más nueva a la más vieja
  (AddStructureToTown inserta en cabeza, 0x7399C3..0x7399CF); la de planos (+0x9A8) de la más vieja a la más nueva
  (AddPlanned añade al final, 0x73D08A..0x73D0AD). La altura del resultado es la del último leído (0x7409C1..0x7409DB).
  SET_TOWN_CONGREGATION_POS: GetScriptPos 0x718250 → MapCoords::Set 0x603280 (x, z; altura 0, o el tercer campo sin
  escalar si lo hay, 0x6032E4); el desplazamiento 0xD99724 es nulo al cargar una tierra (LoadMapFeatures 0x7180FE) y
  solo lo pone el vórtice (fn_0076FA50).

### Desviaciones y efectos visibles hasta V4

- Aldeanos en 37 ARRIVES_HOME (sin función) y en 36 sin casa (DoGoingHome devuelve 1 sin hacer nada) quedan quietos
  para siempre; el número crece con la partida (Land1: 4 → 5 → 6 en 37 en los turnos 400 / 800 / 1200, más 3 en 36 sin
  casa). Aceptado (P-1).
- **Niños sin casa quedan quietos en 114** hasta V4: sin madre (los creados al empezar el mapa no tienen, la madre es
  nula cuando la edad < grownUpAge) y sin casa, ChildFollowsMother llama a CheckNeedNewAbode 0x757F90 (neutro) y
  devuelve 1; si tienen hambre, CheckChild → GoHome → la rama sin casa de DoGoingHome 0x760310 (neutra) devuelve 1. Antes
  de V2 paseaban (paseo inventado). Land1: 5–6 niños en 114 quietos (12, 13 en (1748,4, 2679,6) desde su creación). El
  trace lo dice una vez por aldeano: `child 114: no mother, no abode -> CheckNeedNewAbode …` y
  `home: no abode -> DoGoingHome's homeless branch …`. Land1 (2026-10-01, `_mapa_runudit_v2_fix.log`, 367 turnos): niños 60, 12 y 67 por
  CheckNeedNewAbode, aldeanos 1992 y 1991 por la rama sin casa de DoGoingHome.
- Los que quedan en 37 o en 36 sin casa no vuelven a CheckNeededForSomething: el culto de Milagros
  (CheckNeededForWorship) no puede reclutarlos y la reserva de adoradores baja con la partida.
- Otros caminos acaban también en 37: un aldeano que sobrevive a ser comido queda con vida 0,05 (AnimalPredators
  ProcessDownedVillagers → LANDED → 163) y la regla "herido → 36" lo lleva a la puerta y a 37; igual los heridos por
  fuego o por `OPENBLACK_TEST_HURT_VILLAGERS`.
- Ganchos nuevos: `OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]"`, `OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]"` (fuerza
  la próxima GameRand(9)), `OPENBLACK_TEST_VILLAGER_SHOT="<turno>,<png>[;...]"` (captura en ese turno). El trace añade
  `decide: …`, `chill 245/252: …`, `sit 246: check -> …`, `home 36: …`, `child 114: …` y `congregation town …`.

Comprobado (2026-10-01): Land1, 2532 turnos, todos los aldeanos: 62 SetupNothingToDo (r = 0..8: 11, 3, 10, 8, 6, 4, 7, 5,
8) → 36 × 11, 245 × 18, 246 × 33; 245: 20 "lejos", 17 "cerca y libre", 11 "ocupado" (por su casa, ver supuesto 21); 246:
274 "otra vez" y 26 "nada"; un único aviso de 37 ARRIVES_HOME; ningún 163/209/245/246 sin función. Punto de reunión de
los pueblos 0 (1789,3, 2681,3) y 4 (2479,1, 2542,7) por la media. Land2 con `OPENBLACK_TEST_WORSHIP="1,0.5"`, 3335
turnos: r = 0..8 repartidas (30..45 cada una), 1734 chequeos de 246 "otra vez" y 185 "nada", 10 pueblos con punto de
reunión, el culto sigue (11 adoradores en 59/60).

## Culto: vuelta a casa

CheckVillagerGoBackToTownFromWorship 0x76BEC0 (fichero de Milagros) devuelve el código de SetTopState(248) == 1
(0x76BF59..0x76BF6D), no "TOP == 248": si antes pausa (239 con FINAL 248) también devuelve 1 y el aldeano ya se ha
ido. Antes, tras una pausa, ProcessInWorship seguía con un aldeano que ya no estaba en el sitio (otra vez en la cola de
vuelta, un adorador más pedido, daño del canto de más y una entrada vieja al frente de la cola).

## Muerte (provisional hasta V12)

`villager::VillagerDead` marca al aldeano, escribe `Villager <n> died (<motivo>)` y `FlushDeaths` lo mata al final del
turno (`ecs::life::Kill`). El original lo deja vivo (SetDying → 13) y sigue llamando a CallState; aquí ya no
**(aproximado hasta V12)**.

## Ganchos de prueba

- `OPENBLACK_VILLAGER_TRACE=1` (o `=<n>`, índice de creación): líneas `Villager trace:` con la creación (posición,
  edad, food, lastCheckTurn, contador, 85 o 16), cada `SetState`, `SetTopState a → b = código`,
  `SetCurrentAndDestinationState`, `pause 239 → s (rand, umbral)`, `AdjustTownModifier`, cada check periódico, un
  resumen cada 100 turnos y cada llamada a las entradas y salidas de fuego (`EnterPutOutFire(final, s) = r`,
  `ExitPutOutFire(final, s) = 1`, `EnterOnFire`, `ExitOnFire`), `ExitReaction(s) reactive r`,
  `ExitReactToTeleport(s) same r`, `StopReacting`, `PopFromPrevious stored a -> resume b = código`; el resumen lleva la
  posición, el destino y la marca de paseo (L / O / E / S / F / A / -).
- `OPENBLACK_TEST_VILLAGER_LIFE="<vida>[,<n>]"`: en el turno 2 fija la vida de todos o del aldeano n.
- `OPENBLACK_TEST_VILLAGER_STATE="<estado>[,<n>]"`: en el turno 2 llama a `villager::SetTopState` y escribe el código.
- `OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z"`: en el turno 2 crea una celta (Housewife, 25 años) ahí.
- `OPENBLACK_TEST_VILLAGER_POISONED=<n>`: en el turno 2 envenena al aldeano n.

Comprobado (2026-10-01): Land1, 58 aldeanos, todos 85 → 163 con código 1, checks cada 9 turnos, desgaste 2e-6 por turno
al andar; con `LIFE=0.2` y `STATE=246`, 14 de 55 pausan (239 → 246; se esperaba ~27 %) y en el siguiente check van a
36 (antes de aplazar GO_HOME a V4); con `LIFE=0`, `died (EXHAUSTION)` sin cuelgue; `OPENBLACK_TEST_THROW_VILLAGER`:
85 → 10 → 11 → 163. Land2, `OPENBLACK_TEST_WORSHIP="1,0.5"`: llegan a 59 y 60; tres aldeanos nacen en el mar en 16.

Tras la auditoría (2026-10-01): Land1 con `LIFE=0.25`, 731 turnos: ningún aldeano entra en 36 (3066 líneas `GO_HOME
skipped`), siguen andando y llegando (35 llegadas por el puente a 163); Land1 normal, 338 turnos, sin cambios. Land2,
`WORSHIP="1,0.5"` y `LIFE=0.25`, 2452 turnos: 25 salidas 60 → 248 directas y 8 tras pausa (239 → 248), repartidas
entre 21 aldeanos hasta el final (antes, desde el turno 2333 solo salía uno). Land2,
`OPENBLACK_TEST_HUNT_VILLAGER="0,3"` (turno 3, antes de que el guion cree los aldeanos del mar, así el primero del
registro está en tierra): 85 → 17 DOWNED → 18 BEING_EATEN → comido.

Fuego y teletransporte por el núcleo (2026-10-01): Land1, `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 650
turnos: los mismos estados que antes (85/1 → 215 → 220 ⇄ 216, 163/1 → 219) y una entrada y una salida por cambio (3213
`EnterPutOutFire(216, 220) = 1` para 3213 cambios 216 → 220, 3221 para 220 → 216, 11 `(215, 220) = 1`), más las de
replanificar y llegar (22 `ExitPutOutFire(220, 220)` / `EnterPutOutFire(220, 220)`, 6 `ExitOnFire(219, 219)` /
`EnterOnFire(219, 219)`), ningún 0x2E ni 0x2F; antes faltaban esas últimas y ExitOnFire no se llamaba nunca. El vaivén
216 ⇄ 220 de cada turno ya estaba antes. Teletransporte (`OPENBLACK_TEST_TELEPORT="1785,2655,1830,2660,7,walk"`,
`_TURN=300`): 1 ⇄ 201 como antes y, en una de las pasadas, 201 → 202 → salto (ahorro 26 m) → `SetTopState 202 → 163 =
0x1`.

Revisión de Milagros (2026-10-01, `_scratch\mapa\tele3.log`, `fire1.log`, `cycle2.log`): teletransporte igual que
arriba, 1365 turnos: los 6 aldeanos que reaccionan hacen **un** paseo a la piedra (1 con FINAL 201, unos 70 turnos),
llegan (1 → 201 → 202), saltan (ahorro 44-45 m) y `PopFromPrevious stored 209 -> resume 163`; se acabó el vaivén
1 ⇄ 201. Los aldeanos siguen andando y llegando (208 paseos acabados, mediana 170 turnos; ninguno en MOVE_TO_POS sin
moverse entre dos resúmenes), 18 casos sin portar del PathfindingSystem siguen en STEP_THROUGH. Fuego
(`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 747 turnos): los mismos estados (85 → 215 → 220 ⇄ 216 → 163,
163 → 219), `ExitReaction` en cada salida de 215..220 y 12 `StopReacting` al apagarse; ningún `pause 239` ni
`Stuck in an invalid state`. `OPENBLACK_TEST_MAP_CYCLE` sobre Land1-5 sin cuelgues.

## Pruebas

`test/test_villager_core.cpp` (tabla falsa en el Locator y tiradas guionizadas con `villager::SetRandForTests`):
food con una y dos tiradas, lastCheckTurn, contador y regla del agua, orden de azar del constructor, 85, 0x2E por TOP y
por FINAL, 0x2F (→ 163), 0x23, fijar TOP borra FINAL con el pueblo, la regla de PREVIOUS, AdjustTownModifier, la
pausa (con y sin veneno, sin tirada en 239 o sin la marca), 239 → FINAL, check cada 9 turnos, desgaste, EXHAUSTION /
CHANT (también con `WorshipVillager::atSite`), herido → 36 (encendido desde V2; apagado en un caso; 19 con comida,
derribado), SetupMoveToWithHug con `moveState` conserva FINAL (y 0x2F sin paseo), POWER.

## Supuestos (inferido / aproximado)

1. **(aproximado)** "Bailando" (Living +0xD8, el DanceGroup) se aproxima con `WorshipVillager::dancing` (lo ponen
   AddDancer / FindDanceGroup y lo quitan ExitAtWorshipSite / RemoveVillagerFromWorshipSite) o `TOP == IN_DANCE`
   (VillagerSpeed.cpp).
2. **(aproximado)** GRand: las tiradas usan el mt19937 de openblack, no la secuencia de GRand; solo se respeta el orden.
3. **(aproximado)** El arquetipo pone la velocidad de 85 CREATED al crear el aldeano; en el original la pone el primer
   SetTopState. No se ve: CREATED no anda.
4. **(aproximado)** UniqueId (UniqueKeyHeap::GetUniqueIdFromAddress 0x7E19A0) es el índice de creación (`object_index`,
   +0x3C): decide en qué check periódico se mira la vejez, y el factor de SetSpeed.
5. **(inferido)** +0x11C: el tipo de la unión sale de bw1-decomp; no se porta hasta que un trabajo lo lea.
6. El turno de la partida es `Game::GetTurn` (g_game +0x205A40); sin Game (tests) vale 0 o el de `SetTurnForTests`.
7. **(aproximado)** Orden del turno: aldeanos y animales en dos pasadas, el movimiento de todos antes que la lógica, y
   los aldeanos en el orden del registro (no el de la lista g_game +0x205BBC).
8. **(aproximado hasta V12)** VillagerDead mata al final del turno y la función de estado ya no corre tras la muerte;
   el jugador de VillagerDead (GetPlayer, vt +0x1C) no se pasa (NEUTRAL).
9. **(aproximado)** Los cambios de TOP de la mano, la física, los animales, LANDED y el Gui no pasan por salidas ni
   entradas (sus Enter/Exit del original no están portadas). Los de fuego y teletransporte ya sí (núcleo).
10. (V2: el paseo ocioso inventado se ha quitado.) Un paseo con FINAL 0 (solo herramientas de depuración) vuelve a 163
    por el camino de compatibilidad (el original haría SetTopState(0)). Con FINAL distinto de 0 la llegada es la
    exacta (SetTopStateToFinal, 0x5EC28E).
11. **(aproximado)** La salida de MOVE_TO_POS / MOVE_TO_OBJECT (ExitMoveToPos 0x5EDDA0: CircleHugInfo::Reset y
    +0x60 = 0) no está portada: vale 1 (lo que devuelve) y avisa una vez.
12. **(aproximado)** AdjustTownModifier ignora un deseo fuera de 0..16 (info.dat no tiene ninguno).
13. **(aproximado)** Un tipo de discípulo fuera de la tabla g_DiscipleInfos (13 filas) no ignora necesidades.
14. **(aproximado)** Sin recursos de clips (los tests), IsReadyForNewAnimation da "acabado".
15. **(aproximado)** El bit 0x2 de +0xE0 (en el sitio de culto) se lee de `flags` o de `WorshipVillager::atSite`
    (Milagros) hasta que pase a `flags`.
16. (V2: ya no es supuesto.) La regla "herido → 36 GO_HOME" de CheckEveryTime (0x7505C3) está encendida: 36 anda a la
    puerta. **(aproximado hasta V4)** Al llegar, 37 ARRIVES_HOME no está portado: el aldeano queda quieto en la puerta.
17. Neutros hasta su hito (no inventan conducta): CheckHungry (solo el reinicio de lastCheckTurn; la comida no baja),
    CheckChildGrownUp, WomanSpecial, CheckDeathFromOldAge (V4), ProcessReaction (Milagros M-5), Town +0x5E8 (V3),
    SpecialVillager (V14), contador de aldeanos y esqueleto (V12), DROWNING 16 (agua).
18. Neutros de V2 (devuelven 0 / no hacen nada, con TODO y dirección): CheckHomelessMoveIntoAbode 0x761360,
    ChangeStateToFindFoodToEat 0x75B990, CheckWhenGoingToBed 0x760B60, CheckNeedNewAbode 0x757F90 (V4);
    TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 (V3: devuelve 0; qué deja en eax está sin leer, P-11);
    DiscipleDecideWhatToDo 0x751720, IsMotherAlive 0x757F40 (deja la madre), ChildGotoCreche 0x7579F0, RemoveFromDance
    (V14); la rama sin casa de DoGoingHome (tienda 238 / 130, V4); Town +0xF1C (lo escribirá ProcessTownEmergency,
    Milagros); ExitAtHome 0x761B40 (V4, vale 1).
19. **(aproximado, P-5)** La puerta (Game3DObject::GetDoorPosition 0x63AFE0 por LH3D vt +0x1C4, sin símbolos): el
    punto de puerta del L3D (`L3DMesh::GetDoorPos`) por rotación·escala del Transform de la casa; sin puerta, la
    posición de la casa (literal, 0x52E3A4).
20. **(inferido)** Abode IsAvailable (+0xA & 1) = la entidad es válida; IsBuilt (+0x58 & 2, +0x5C ≥ 1) = sí para todas
    las casas (no hay obras hasta V6) **(aproximado hasta V6)**; el GAbodeInfo de la casa es el de su número y malla.
21. **(aproximado)** CheckForClearArea: openblack no tiene listas de objetos por celda; se toman las entidades de cada
    celda (`effects::ObjectsInMapCell`) y su radio `Object2DRadius`: mismo conjunto, otro orden (el resultado es sí /
    no). Con el radio de las casas de openblack (≈5,4 m) y la puerta a ≈1,5 m del centro, muchos sitios de 245 caen
    "ocupados por su casa" y FindClearArea los aparta: los aldeanos se sientan algo más lejos de la puerta que en el
    original (efecto de 19).
22. **(aproximado)** La lista de casas del pueblo (+0x754) va de la más nueva a la más vieja (AddStructureToTown
    0x7399C3..0x7399CF inserta en cabeza); openblack la ordena por el índice de creación (+0x3C) de mayor a menor, como
    si cada casa entrara en su pueblo al crearse. Decide la altura de GetCongregationPos (la del último leído: la más
    vieja), la base del respaldo con una casa y qué casas quedan en el anillo con más de 100. Los planos (+0x9A8) van
    en orden de llegada (AddPlanned 0x73D08A..0x73D0AD añade al final), como `plannedAbodes`.
23. **(aproximado, P-9)** SetupMoveToOnFootpath 0x5EDD20: GFootpathLink::UseFootpathIfNecessary 0x5362E0 no está
    portado: siempre el paseo directo (SetupMoveToWithHug), que es lo literal sin enlace de camino.
24. **(aproximado)** LookAtPos 0x5EC550: el ángulo Living +0x5C es `WallHug::yAngle` (radianes, redondeado a 2048avos).
    Toma dos argumentos (ret 8); con un modo distinto de 0 / 1 / 2 el paso es el propio modo (0x5EC57A).
25. **(aproximado)** 114: la madre "disponible" (vt +0x2C) = entidad válida; IsNavigable (Collide & 2 y no & 8): el
    Collide de openblack solo sabe tierra / agua (el bit 8 nunca está).
26. **(aproximado)** SET_TOWN_CONGREGATION_POS: el analizador de guiones de openblack (Script.cpp GetParameter) solo hace
    vector de una cadena de dos campos (y su y es la altura del terreno, no un tercer campo): la altura de la caché es 0,
    lo literal para dos campos. Una cadena de tres campos (MapCoords::Set 0x6032E4 la leería sin escalar) no llega al
    comando en openblack. Ninguno de los guiones del juego usa tres campos.

Ya no son supuestos: el escritor de +0x24 & 0x400 (controlado por guion) es GameThingWithPos::SetControlledByScript
0x402240 (`ecs::script_held`; también el vórtice, fn_005FE3B0 0x5FE474); el estado de andar de SetupMoveToWithHug es
GLivingInfo +0x124 (`moveState`).
