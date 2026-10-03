# Aldeanos: datos, máquina de estados y velocidad

El aldeano de runblack.exe (W120) tal como lo porta la sesión "aldeanos" (rama `local/aldeanos`, hito V1: el núcleo de
la máquina de estados). Los clips (qué clip por estado, transiciones, tamaño) siguen en [animation.md](animation.md);
aquí van los datos del aldeano, el turno, los cambios de estado y las reglas de velocidad. Investigación con
direcciones: `C:\Users\diewgarc\dev\documentacion\aldeanos\` (`V1_spec.md`, `PLAN.md`, `core.md`, `team_apis.md`, volcados en
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
- `test/test_villager_core.cpp`: 20 casos (ver [Pruebas](#pruebas));
- V4: `ECS/Villager/VillagerHome.*`, `VillagerFood.*`, `VillagerAge.*`, `VillagerResources.*`, `ECS/Town/AbodeVillagers.*`,
  `ECS/Town/TownVillagers.*`; `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp` (V4,
  ver [Casa, comida, sueño, sin techo y edad (V4)](#casa-comida-sueño-sin-techo-y-edad-v4)).

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

Villager +0xE0 (escritores directos: `documentacion\aldeanos\v1\scanE0.py`): 0x1 tras un toque en su casa
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
2. validate (+0x80) del TOP (+0x8C, 0x74FF91) y del FINAL crudo (+0x8D, 0x74FFD9), solo si la fila tiene; el
   resultado no se usa. Las filas de reacción (201, 202, 251, 214-218, 220, 6-30, 140-196... todas las de validate
   original 0x756A00 en `VillagerOriginalFns.h`) llaman a `villager_reactions::ReactionValidate` 0x756A00 desde
   `LivingActionSystem::VillagerCallValidate` (fusión de V2, 2026-10-01): sin objeto de reacción (+0xBC), o no
   disponible, o en la mano si la reacción lo pide → `PopFromPrevious` 0x751E50. Por eso `ReactToFire` 0x765870 solo
   devuelve 0 (sin cambiar de estado) cuando el objeto no es un `Object` o no tiene fuego, y `GoToTeleportReaction`
   0x7662F0 no comprueba nada (openblack solo devuelve 0 si no guarda piedra, en vez de leer un nulo); las salidas
   propias (inferido) que hacían ese trabajo ya no están.
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

Spec completa: `dev\documentacion\aldeanos\V2_spec.md`. Código: `Villager/VillagerDecide.{h,cpp}`, `Villager/VillagerHome.{h,cpp}`,
`Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`; tests `test/test_villager_decide.cpp` (13 casos).

- **163 DECIDE_WHAT_TO_DO** (0x7515C0, devuelve 1): emergencia del pueblo (Town::IsInStateOfEmergency 0x747970, +0xF1C
  `Town::emergencyStartTurn`, nadie lo escribe aún: TODO(Milagros)) → 242; discípulo / seguidor (DiscipleDecideWhatToDo
  neutro, V14); `SetTopState(163)`; niño → ChildDecideWhatToDo (CheckChild, reparto del pueblo neutro, guardería neutra,
  → 114); CheckNeededForSomething (sin techo: neutro V4 → CheckNeededForSpecial: **culto de Milagros**, cívico (V3:
  calcula el trigger y borra `flags & 1`; el reparto, desde V3, en [Deseos del pueblo y reparto (V3)](#deseos-del-pueblo-y-reparto-v3)), deseos propios con umbral 0,3) → CheckTakeResourcesToStoragePit
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
- **36 GO_HOME** = DoGoingHome(37, 238): con casa, anda a la puerta con FINAL 37 (V4: 37 la hace entrar). Sin casa: la
  tienda o 130 (V4). La regla "herido → 36" de CheckEveryTime está encendida en el juego.
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

## Deseos del pueblo y reparto (V3)

Spec completa: `dev\documentacion\aldeanos\V3_spec.md` (desensamblados `town_dis\desire.txt`, `desire_fns.txt`, `rep.txt`;
`v3\emu_dtab_all.py` saca la tabla, `v3\emu_qsort.py` ejecuta el `_qsort` del exe). Código: `Town/TownDesire.{h,cpp}`
(tabla, funciones, Process, órdenes, reparto, API de lectura, guiones), `Town/TownProcess.{h,cpp}` (Town::Process y el
bucle de jugadores), `Town/TownStats.{h,cpp}`, `Villager/VillagerSatisfy.{h,cpp}` (los CheckSatisfy), `Components/Town.h`
(`TownDesire`, `DesireSort`, `TownStats` y los campos nuevos del pueblo); tests `test/test_town_desire.cpp` (16 casos) y
un caso nuevo en `test/test_villager_decide.cpp`.

- **Orden en el turno** (milagros2, con el OK de asistente): `town_process::ProcessPlayers` se llama una vez por turno
  desde `magic::ProcessTurnStart` (Magic/MagicLoop.cpp, ranura 3 de GGame::ProcessTurn). Va después de
  `InfluenceRing::ProcessRings` (0x54E63C), así que Town::Process ve los anillos de este turno. Después vienen los viajeros
  del teletransporte (fn_005FCC70, 0x6496BC) y el alineamiento (0x6496C5), y luego los bailes, GlobalGameLists, los
  bosques y, al final, Living (0x54E65B). **(aproximado)**: GPlayer::Process 0x6494E0 hace pueblos, teletransporte y
  alineamiento jugador a jugador; aquí cada paso recorre todos los jugadores antes de pasar al siguiente.

- **Tabla** (fiel): 0xDA32C8 + d·0x68, rellena por crt_xc 0x744BD0: nombre, función (+0x10), Amount/Desired (+0x20/+0x30,
  solo 5, 6, 7; solo los lee la traza 0x745EC0), CheckSatisfy del aldeano (+0x40), modificación (+0x50), niños (+0x60:
  2, 3, 4, 15, 16) y +0x64 (sin lector). Info por deseo 0xDA2930 + d·0x90 (+0x18 trigger, +0x58 TribeMultiplier[9]).
  Nombres para `TOWN_DESIRE_BOOST`: fn_747270 (`_stricmp`).
- **Funciones** (fiel, x87 en double, ver supuestos): Food = `Town::CalculateDesireForFood` 0x747F00 (thunk 0x747340):
  `1 − (comida + 1e-4)/(5·Σ foodReqiredForDinner + 1e-4)`, con el aviso `HelpSpritesLowOnFood(min(v,2) − 0,9)`
  (0x747FA0) si v ≥ 0,95 y el pueblo es del jugador local; Wood 0x747FF0 con S = min(R5+R6+R9+R12, 3), a = (artesanos +
  0,001)/(adultos + 0,001) + S, **k = max(abodes/10, 1)**, B = 500k, C = 5000k y el aviso LowOnWood (0x7481BC,
  min(v,2) − 1); Abodes 0x748210 (max(a, c)⁴·(1 − R9)(1 − D6)); Civic 0x748330 (PopulationWhenNeeded de GAbodeInfo::Find
  0x405B30, la primera coincidencia); For_Children 0x748430 (alineamiento de milagros2 y TribalPower[4] de
  `PlayerMagic`, 1,0); To_Build 0x748640 y Repair_Town 0x7486B0 (Abode::GetDesireToBeRepaired 0x406970 con la vida
  `ecs::life`); Playtime 0x7487B0 (0,1 si D0, D1, D5, D6, D9 < trigger y turno > 4000); Relaxation 0x7488C0 y Sleep
  0x748960 (`sky_type::At` y `EveningRamp` sobre la hora visual del reloj de `Game`; Sleep llega a 6,25 de noche).
  Protection 0x7488A0 / Mercy 0x7488B0 leen Town +0xEC0 / +0xEBC, que valen 0 hasta las agresiones (**pendiente**);
  For_Wonder 0x748740 = 0 sin `GetBeliefInPlayer` (**pendiente**, milagros2); Supply_Worship, For_Rain, For_Sun,
  Suppy_Workshop = 0 (literal).
- **TownDesire::Process** 0x745AE0 (fiel): +0x164, los 17 en orden 0..16 (un deseo que lee otro de índice mayor ve el
  del turno anterior), `CallDesireFunction` 0x745D80 (bruto +0x168 = f·TribeMultiplier sin recortar; deseo +0x118 =
  clamp(bruto·modificación, −1, 1)), modificaciones 0x746490 / 0x7462A0 (150 = GVillagerInfo[10] maxFoodCarried) /
  0x746350 (250) / 0x746400, los dos órdenes y, cada 50 turnos, `(2R0 + R1 + max(R3,R4) + max(R5,R6))/5` con
  `HelpSpritesVillagerUnhappy` (0x745C8A) si pasa de 0,6 y es del jugador local. La estadística del jugador
  (GPlayer +0xA44) es **pendiente**.
- **Órdenes** (fiel): orden 1 (+0x278, valor GetDesire, +0 = refuerzos A + guion) y orden 2 (+0x344, GetRawDesire, +0 =
  refuerzo A) con el `_qsort` de la CRT VC6 0x7C7E64 portado literal (CUTOFF 8, `_shortsort` 0x7C7FB8, pivote al medio;
  no es estable: con todo a 0 queda `8 1 2 3 4 5 6 7 0 9 … 16`). Los tests comparan con `emu_qsort.py`.
- **Reparto** `CheckVillagerNeededForTownDesire` 0x745FF0 (fiel): trigger 0 → 0,001; t = min(trigger + info +0x18, 1);
  las entradas sin CheckSatisfy y, para un niño, las sin +0x60 se saltan sin cortar; corta (0) en la primera elegible con
  `TempMod(k)·valor ≤ t` y devuelve 1 cuando un CheckSatisfy da 1. **Rareza conservada**: `TempMod` se pide con el índice
  del bucle k, no con el del deseo. Lo llama fn_7581A0 (`villager::CheckNeededForTownDesire`, VillagerDecide.cpp).
- **CheckSatisfy** (V3): Sleep es el de V2; Playtime 0 y Relaxation 0 (literal: sin fútbol, criatura ni artefactos);
  Food (V8), Wood (V9), Abodes / Civic (V6/V7), Supply_Worship (milagros2), To_Build (V7), Repair (V11) y Workshop
  devuelven 0 (**pendiente**). Consecuencia: en V3 solo Sleep produce conducta.
- **Town::Process** 0x747380 (`town_process::ProcessTown`): TownStats del turno, +0x5E4 = 0, TownDesire::Process, cada
  10 turnos `worship::percentage::GetWorshipersNeeded(1, 0)` / `AdjustWorshipersWorshipping(n, 1, 0)` de milagros2
  (fn_7489F0; antes nadie lo llamaba), el pulso +0x5E8/+0x5EC y la cuenta atrás +0xF20; los demás pasos con TODO y su
  dirección (solares V6, casas V4, artefactos, banderas, agresiones, reparaciones V11, emergencia, criatura, vasijas
  V5, misioneros, creencia, alineamiento por deseos, Shuffle V4). `town_process::ProcessPlayers` recorre
  `map_cells::ForEachTown` y se llama en `Game::GameLogicLoop` entre los tiburones y los PuzzleGames, antes de los
  aldeanos (GPlayer::ProcessPlayers 0x54E641 va antes que Living::ProcessLiving 0x54E65B). La influencia (+0x5C8, pasos
  3-5 del original: 0x7473A0 / 0x7473AD / 0x7473BD) no se llama aquí: la calcula cada turno `influence::ProcessTowns` de
  milagros2 desde su propio gancho (`influence::ProcessTurn`, dentro de `magic::ProcessTurn`).
- **Guion** (fiel): CHL `SET_TOWN_DESIRE_BOOST` (341) = GScript::SetTownDesireBoost 0x6FE650 (pueblo, d < 17, −1 ≤ v ≤ 1 →
  +0xD4[d] = v y reordena solo el orden 1; "Thing not valid!" / "Invalid Params"); CHL `GET_DESIRE` (234) = 0x6FCCA0
  (d inválido → "Invalid desire" y 0 **sin sacar el objeto**; si no, GetRawDesire); el comando de mapa
  `TOWN_DESIRE_BOOST` 0x7179EC escribe +0xD4 sin reordenar ni comprobar el rango (Land2.txt: "Abodes" /
  "Civic_Buildings" −0,75).
- **API para otras sesiones** (`ECS/Town/TownDesire.h`): `GetDesire` / `GetRawDesire`, `GetSortedDesires` (+0x278),
  `GetSortedRawDesires` (+0x344 = Town +0x37C valor / +0x380 tipo, lo que lee CheckTownDesiresSFX 0x71B130), `GetField`
  (+0x90 / +0xD4 / +0x118 / +0x168), `GetDesireSignificanceToVillager` 0x746660, `GetMostDesired` 0x745E50,
  `GetMostSignificantRawDesire` 0x745EA0, `CalculateDesireForFood` (con aviso) / `FoodDesireValue` (sin aviso),
  `SetBoost`, `AlignmentTurns` (+0x410, solo milagros2). Rellenar `desireTowns` / `townResourceNeeds` de audio queda
  para audio (**pendiente**, P-2).

### Desviaciones y efectos visibles hasta V4

- De noche Sleep es el primero (bruto hasta 6,25, deseo 1) y CheckSatisfySleep manda a 36; V4 los mete en casa (37 → 38
  → 119 → 120, ver [V4](#casa-comida-sueño-sin-techo-y-edad-v4)). De día el reparto corta en Relaxation/Playtime
  (CheckSatisfy 0) y todo sigue como en V2.
- El culto de milagros2 recibe ahora, cada 10 turnos, el ajuste de adoradores de Town::Process (fn_7489F0).
- **(aproximado)** TownStats se recalcula al empezar Town::Process desde las entidades (el original suma al añadir y
  quitar); mismos recuentos, las sumas de float en otro orden. **(aproximado hasta V6)** todas las casas del guion
  cuentan como funcionales, y `Town::storagePit` (+0x30) / `Town::creche` (+0x744) los pone la creación por guion
  (AbodeArchetype) en vez de StoragePit / Creche::MakeFunctional (el último almacén gana; la primera guardería).
- **(aproximado)** las cadenas x87 (80 bits) se calculan en double y se guardan en float donde el original hace
  `fstp dword`.
- **(aproximado)** la influencia del turno (milagros2, `magic::ProcessTurn`) se calcula después de los deseos y no dentro
  de cada Town::Process (los deseos no la leen).
- **(aproximado)** `SET_TOWN_DESIRE_BOOST` con d negativo no escribe (el original escribe fuera del array).
- **(inferido)** +0x90 vale 0 en partida nueva (solo lo escribe Load); TRIBE_TYPE = el enum `Tribe` para
  TribeMultiplier; "jugador local" = PLAYER_ONE; la lista +0x770 está vacía; fn_555240 (paso 23) no necesita llamada.
- No portado (sin conducta): la suma de control de red [0xDA2770], la traza de depuración 0x7457C0 (la sustituye
  `OPENBLACK_TOWN_TRACE`) y las funciones sin llamadas (0x745E80, 0x745FA0, 0x7461E0, 0x746220, 0x7465F0, 0x7466B0,
  0x7468E0).

## Casa, comida, sueño, sin techo y edad (V4)

Spec completa: `dev\documentacion\aldeanos\V4_spec.md` (desensamblados en `v4\`: `home.txt`, `homeless.txt`, `food.txt`,
`age.txt`, `abode.txt`, `town.txt`, `misc*.txt`, `helpers.txt`, `scanline.txt`; valores de los tests en `v4\v4calc.py`).
Código: `Villager/VillagerHome.{h,cpp}` (36/37/38, 119/120/121, 129, 130, 234, 238, la tienda, las mudanzas),
`Villager/VillagerFood.{h,cpp}` (CheckHungry, cantidades, 117/118/212, 33/34/35), `Villager/VillagerAge.{h,cpp}` (crecer,
escala, vejez, embarazo), `Villager/VillagerResources.{h,cpp}` (lo que lleva y coge: la mitad de comida de V5),
`Town/AbodeVillagers.{h,cpp}` (la lista de la casa, PresentAtHome, la puntuación, Abode::Process, las mudanzas del
Shuffle), `Town/TownVillagers.{h,cpp}` (sin techo, vagabundos, AddVillagerToTown, FindAbodeWithSpaceInTown, UseFood,
Shuffle); tests `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp`.

- **La noche** (fiel): Sleep (16) arriba → reparto → CheckSatisfySleep 0x761490 → 36 → puerta → **37** ArrivesHome
  0x760930 → `Villager::ArriveHome` 0x751FA0 (bit 4 de +0xE0, `Abode::presentAtHome` +0xB6 `inc`, malla oculta por el
  clip −4) → **38** AtHome 0x760B10 = HomeDecideWhatToDo 0x75FEA0 → CheckSatisfySleep dentro → CheckWhenGoingToBed 0x760B60
  (devuelve 1 salvo que muera de viejo; una vez por estancia, bit 0x2000) → **119** GotoBedAtHome 0x760B30 → **120**
  SleepingAtHome 0x760D70 (contador RestAtHomeTime 100; sin pueblo no cuenta) → DoSleeping 0x760DB0 cada 100 turnos
  (+0,05 de vida salvo envenenado; sigue mientras Sleep es el primero del orden 1 o la vida < 0,7: de 0,4 a 0,70000005 en
  6 ciclos). De día DoSleeping da 0 → 38 → reparto u ocio → la salida **ExitAtHome** 0x761B40 de 35..38 y 118..121 hace
  LeaveHome (0x751FD0: bits 4 y 0x2000 fuera, `presentAtHome` `dec`) si el estado siguiente no se queda en casa (fila de
  info.dat, fichero 0xC0). 121 WakeUpAtHome 0x760E50 = GoHome (ningún código lo pone).
- **37** (fiel): no ha llegado (AreWeThere(puerta, 0)) → otra vez a la puerta con FINAL 37 (literal, también desde 249);
  construida y reparada (vida ≥ 1, IsRepaired 0x4016A0) → dentro; herida (< 0,3): casa funcional → dentro, si no tienda
  (238); con hambre (food < 0,5 estricto): `SetTopState(163)` si no es funcional y dentro en el mismo turno (literal);
  si no SetupBuildingObject 0x758530 (neutro: V7/V11) y dentro. Sin casa → 129 y 0.
- **38** (fiel): emergencia → 119; CheckNeedsAtHome 0x760110 (la embarazada se queda; umbral
  `0,9·max(GetLifeDesireFromLife(0,7), POWER(0,5))` = 0,7875, el mayor, no el menor; 0,9 para el discípulo que ignora
  necesidades; el niño pasa por CheckChildActivity = ChildDecideWhatToDo, siempre 1); el discípulo; CheckNeededForSomething
  (también el culto de milagros2 cada turno); HomeNothingToDo 0x75FFB0 (dentro, GameRand(4) == 0 → 119 con contador 0).
- **Sin casa** (fiel): DoGoingHome 0x760280 sin pueblo → 130; a más de 100 m de su pueblo → paseo a 10..35 m de él, de su
  lado (FINAL el TOP); cerca → GetTentPos 0x7604F0 → 238, o un paseo de 10..30 m. La tienda: el árbol más cercano en 50 m
  (fn_00604AF0 con IsTree) si fn_0074C650 le encuentra sitio (2 m del árbol, al otro lado del ocupante; un aldeano en 238 o
  un MultiMapFixed a menos de 4 m cuenta; dos = lleno, y no se prueba otro árbol); si no, 3 intentos: celda libre
  (`Collide & 0x19 == 0`) y ningún aldeano en 238 a menos de 5 m en las 9 celdas de una espiral que **mueve el punto**: la
  tienda queda en (−20 m, +10 m) del sitio probado (rareza literal). **238** SleepInTent 0x761AE0, **129** HomelessStart
  0x761320, **130** VagrantStart 0x76A8D0 (un pueblo de su tribu a menos de 200 m → AddVillagerToTown y 163; herido →
  tienda; si no un paseo de 10..30 m hacia delante).
- **Comer** (fiel; P-1: los 33/34 del almacén entran en V4): **CheckHungry** 0x75BCC0 (lote = turnos·9e-5 entre
  TribalPower[3] (player +0x74) y por la velocidad si pasa de 1 y se mueve; daño 0,001 con hambre (food < 0,5,
  estricto: IsHungry usa ≤) o veneno: el `max(…, 1)` deja el factor en 1; interrupciones 0xD0 / 0xD4 de la fila del
  estado final; vida 0 → STARVING, o CHANT desde el culto); la cantidad GetAmountOfFoodToEat 0x75BC20 =
  `ftol((1 − 0,3·clamp(deseo de Food del pueblo))·(float)(POWER(food)·85))` (74 con food 0,5);
  **ChangeStateToFindFoodToEat** 0x75B990 (necesita 0 → 117, o 118 dentro; su casa funcional con bastante → 36 / 118; el
  almacén —el del pueblo o, si no hay, su casa— funcional con bastante → 33; sin almacén funcional → al punto de entrega
  con FINAL 34; si lleva algo, lo come; si no 0); **117 / 118** EatFoodHeld 0x75BF20 (`comido/aComer·1,2 + food`,
  recortado a [0, 1], NaN → 0; Town::UseFood 0x73B5E0 suma a `Town::foodUsed` +0x6F8); **GetFoodFromHome 0x75C040 coge
  dos veces** (GetResourceFrom ya hace PickupResource: la casa pierde n y el aldeano gana 2n, rareza literal); **34**
  ArrivesAtStoragePitForResource 0x7698D0 (coge min(lo que necesita, lo que hay) y vuelve a la puerta con FINAL 163;
  luego come lo que lleva); **212** ShowPoisoned 0x75B940; **35** ArrivesAtHomeWithFood 0x769B30 (de la ama de casa,
  V14). La casa resta su comida con DoResourceRemoving 0x404F60 (CallDesireFunction del pueblo antes,
  `town_desire::CallDesireFunctionNow`).
- **Edad** (fiel): CheckChildGrownUp 0x751050 a los 13 → bit 8 fuera, edad 18, ChildToAdult de la casa (o del pueblo) y
  ChildBecomesAdult 0x757F10 (madre 0, CheckNeedNewAbode, **234** GoHomeAndChange 0x761810); la malla de adulto llega en la
  salida de 234 (ExitGoHomeAndChange 0x761980 → ChangeTribeIfRequired 0x7618C0 → ChangeInfo 0x761A00), no en SetAge. Si
  no, reescala cada 375 turnos (solo los niños cuyo check cae en esos turnos: mcd(9, 375) = 3, **(inferido)**). Vejez
  CheckDeathFromOldAge 0x760CA0 (en el check periódico, ≈ cada 800 turnos, y en CheckWhenGoingToBed): edad > 60,
  `n = ftol(r³·40)` (el cubo, no el cuadrado), `GameRand(n)`, muere si edad + d > 100: nadie antes de los 63.
  WomanSpecial 0x752240 (la cuenta atrás del embarazo) es literal; el parto es V14. La escala del constructor y de
  SetScaleForAge usa ahora el GameFloatRand sincronizado (V1 usaba el generador de openblack: cambia el orden de las
  tiradas del constructor).
- **Casa y pueblo** (fiel): `Abode::inhabitants` es la lista ordenada +0xA0 (la cabeza, la más reciente: decide quién se
  muda en el Shuffle y la pareja a la hora de dormir), `maleFemale` +0xA8 / +0xAC, `adultCount` / `adultMaleCount` /
  `childCount` +0xB4 / +0xB5 / +0xB7, `emptyTimer` +0xB0; `Town::homelessVillagers` +0x768, ordenada. AddVillagerToAbode
  0x404060, RemoveAliveVillagerFromAbode 0x404340 (dentro → 163; su salida hace el LeaveHome; la pareja no se toca),
  RemoveDeletedVillagerFromAbode 0x404220 (borra las dos parejas), RemoveAllVillagersFromAbode 0x404560 (la casa
  destruida, Buildings.cpp → HomeDeleted → MakeHomeless), la puntuación 0x404B40, FindAbodeWithSpaceInTown 0x73B370 (la
  más nueva gana los empates), AddVillagerToTown 0x73A090 (CheckAddWorshipSite de milagros2 con el primer aldeano),
  CheckNeedNewAbode 0x757F90 (con percentTooCrowded 0,5, un adulto solo en una casa de 2 ya es «demasiado»: se muda si
  hay algo mejor o queda sin techo, literal). Town::Process paso 4 (Abode::Process 0x404440: una casa vacía y construida
  pierde 0,0001 de vida cada 1001 turnos procesados, en float) y paso 24 (ShuffleVillagersAroundAbodes 0x741540 con el
  `_qsort` de VC6, un movimiento por llamada).
- **Creación por guion** (fiel; P-6): CREATE_VILLAGER_POS ("AALN", 0x715A4C) crea el aldeano en el segundo argumento y
  busca el pueblo con FindTownWithID de la ranura entera 0. LHScriptX::ScanLine 0x7E7540 solo escribe la ranura de un
  argumento 'N' (atol), así que vale el id del último comando con un 'N' primero (en las tierras, el CREATE_ABODE o
  CREATE_TOWN de justo antes: `lhscriptx::Script::IntSlot`); sin ese pueblo, el más cercano a la posición del aldeano
  (fn_00552FF0). Luego AddVillagerToTown elige la casa. Se quitó la regla de openblack «la casa a 1 m² de la posición
  del guion».
- **APIs** para otras sesiones: `villager::IsAtHome`, `IsReachable` (0x756460: disponible, no en casa, TOP ≠ 236; la usa
  AnimalPredators en vez de su prueba de los estados 13..18), `LeaveHome`; `abode_villagers::VillagersOf`,
  `PresentAtHome`, `RemoveAllVillagersFromAbode`; `town_villagers::Homeless`, `AddVillagerToTown`;
  `town_desire::CallDesireFunctionNow`.
- **(inferido)**: TribalPower[3] vale 1,0 (nadie lo escribe); la lista de vagabundos está vacía (sus escritores son
  V12/V14); el ocupante no aldeano de fn_0074C650 (+0x24 & 2) es un MultiMapFixed (+0x24 & 4 de IsReachable e
  IsAvailableForStateChange es «en la mano», PlaceObjectInMagicHand 0x5FB014: `fire::traits::InHand`); 115 solo por
  guion; IsInScript de una casa (+0x24 & 0x200) vale 0; IsTree = el componente Tree.
- **(aproximado)**: la velocidad +0x5A sale de `WallHug::speed`; IsMoving = el último paso del WallHug no es cero;
  TownStats recalculadas en cada Town::Process (V3) con `males` / `females`, y AddVillagerToTown, Town::RemoveVillager y
  ChildToAdult tocan al momento adultos, niños y sexos; la influencia de las casas va en el gancho de milagros2, no en el
  paso 4; Abode::ReduceLife 0x405D90 no tiene punto de entrada (se usa Object::ReduceLife de `ecs::life`); el Kill
  provisional hace LeaveHome antes (hasta V12); los tipos de las firmas de openblack hacen de las cadenas de tipos del exe
  para las ranuras enteras; sin la vasija temporal (V5), el punto de entrega es la posición del aldeano.
- **Pendiente**: CheckGetPregnantAtHome neutro y sin partos (V14, P-2: una embarazada sin parto se quedaría en casa para
  siempre); la vasija temporal (V5); SetupBuildingObject al llegar (V7/V11); el baile en DoGoingHome; SetVillagerDisciple
  en 234 y HousewifeStartsGivingBirth (V14); Town::RemoveVillager solo con listas y cuentas (V12); las filas 248-250 del
  culto (`DoGoingHome(249, 250)`, ArrivesHome, SleepInTent, ExitAtHome) esperan el visto bueno de milagros2; la mano que
  coge a un aldeano de dentro (P-9); las ventanas de noche siguen con `inhabitants` hasta que sistemas aplique
  `presentAtHome` (Abode::Draw 0x515F78); las capturas en el juego (V4_spec §13).

## Llevar recursos y el almacén (V5)

**Pendiente** (2026-10-03: la investigación se paró por decisión del usuario antes de escribir la especificación; no hay
código de V5). Lo que ya existe de V4: `Villager/VillagerResources.{h,cpp}` con los estados 33/34 (comer del almacén).
Falta: coger y soltar recursos (PickupResource / Drop*), las capacidades por tipo de aldeano, la vasija temporal
(Town::GetTemporaryResourceStorePotOrPos 0x73E900), llevar comida y madera al almacén (StoragePit::AddResource 0x732F60,
Abode::DoResourceAdding 0x404DF0), CheckSatisfyFoodDesire 0x759F30, el objeto llevado (SetStateCarriedObject
0x7501A0), CreateDroppedResource 0x750940 y la reacción 9. Volcados para retomarlo: `dev\documentacion\aldeanos\v5\README.md`.

## Culto: vuelta a casa

CheckVillagerGoBackToTownFromWorship 0x76BEC0 (fichero de Milagros) devuelve el código de SetTopState(248) == 1
(0x76BF59..0x76BF6D), no "TOP == 248": si antes pausa (239 con FINAL 248) también devuelve 1 y el aldeano ya se ha
ido. Antes, tras una pausa, ProcessInWorship seguía con un aldeano que ya no estaba en el sitio (otra vez en la cola de
vuelta, un adorador más pedido, daño del canto de más y una entrada vieja al frente de la cola).

## Muerte (provisional hasta V12)

`villager::VillagerDead` marca al aldeano, escribe `Villager <n> died (<motivo>)` y `FlushDeaths` lo mata al final del
turno (`ecs::life::Kill`). El original lo deja vivo (SetDying → 13) y sigue llamando a CallState; aquí ya no
**(aproximado hasta V12)**. V4: antes del Kill, `LeaveHome` (si estaba dentro), para que `presentAtHome` no se quede
alto (en el original lo haría la salida del estado hacia 13).

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
- `OPENBLACK_TOWN_TRACE=1[,<cada>][,raw]` (V3): por pueblo, cada `<cada>` turnos (50) y siempre que cambie el primero del
  orden 1: `town <id> turn <t> pop <p>: [16 Sleep 1.000 raw 6.250] [15 Relaxation 0.100] …` (los 17, orden 1) y
  `avg <a>` en los turnos de 50; con `,raw` también el orden 2.
- `OPENBLACK_VILLAGER_TRACE` (V3): el reparto escribe `civic: t=<t> k=<k> d=<d> v=<v> tmp=<m> -> skip(child)|skip(nocs)|
  cut|cs=0|cs=1`.
- `OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<pueblo>]"` (V3): en el turno 2, SetBoost como SET_TOWN_DESIRE_BOOST (reordena
  el orden 1) en todos los pueblos o en el de ese id.
- `OPENBLACK_TEST_VILLAGER_AGE="<edad>[,<n>]"` (V4): en el turno 2, solo Living::SetAge (el turno de nacimiento), sin
  mallas ni bits (12,99 → 13 y la vejez). `OPENBLACK_TEST_HOMELESS=<n>` (V4): en el turno 2, MakeHomeless del aldeano n.
- `OPENBLACK_VILLAGER_TRACE` (V4) añade `home 36: …` (to the door / no abode -> far / tent / wander / vagrant 130),
  `home 37: not there|arrive (present <n>)|tent|hungry 163+arrive|repair TODO(V7)`, `home 38: emergency|needs(t=…)|
  disciple|something|nothing r4=<r>`, `exit-home <s> -> <next> (stay|leave, present <n>)`, `sleep 120: life <l> ->
  keep|wake`, `tent: tree|spiral try|fail`, `food: …`, `eat: …`, `home-food: took <m> held <h>`, `age: grown|rescale|
  old age r= n= d= -> die|live`, `homeless: into abode|list`, `abode: moves|too crowded`, `vagrant 130: …` y, cada 100
  turnos, `home: town <id> inside <n> asleep <m> tents <k> homeless <h> vagrants <v>`. `OPENBLACK_TOWN_TRACE`:
  `shuffle: <casa> -> <casa> (swap|take …) = <r>`.

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

Con `ReactionValidate` conectada (fusión de V2, 2026-10-01, `_scratch\mapa\p_fire.log`, `p_tele3.log`, con una
traza temporal en `VillagerCallValidate` que no se queda en el código): fuego (`OPENBLACK_TEST_FIRE=
"1785.2,2652.6,450,abode,60"`, 785 turnos) 12 aldeanos 85/1/114 → 215 → 220 ⇄ 216 y vuelta a 163 con
`StopReacting` al apagarse; `ReactionValidate` corre cada turno para el TOP (1080 veces en 216, 54 en 220, 13 en 215)
y para el FINAL (410 en 220, 3 en 215), sin ningún pop (el objeto, la casa, sigue disponible; al apagarse
`ExitPutOutFire` → 163 hace `StopReacting` antes de que se valide un estado de reacción sin objeto). Teletransporte
(`OPENBLACK_TEST_TELEPORT="1715,2595,1760,2640,7,walk"`, `_TURN=550`): 1 → 201 → (20 turnos andando, FINAL 201
validado cada turno) → 202 → salto de 63,6 m → 163 y `PopFromPrevious stored 245 -> resume 163`. **Ojo con el
gancho `walk`**: desde V2 su paseo con FINAL 163 no admite reacciones (`IsAvailableForReaction` 0x763390: el +0xEC
de 163 es 0), así que el aldeano del gancho no reacciona; la prueba se hizo con FINAL 245 cambiado a mano en
`TeleportDebugHooks.cpp` (sin guardar). `OPENBLACK_TEST_MAP_CYCLE` sobre Land1-5, Greek God, TwoGods y Kapa's Land1
sin cuelgues.

## Pruebas

`test/test_villager_core.cpp` (tabla falsa en el Locator y tiradas guionizadas con `villager::SetRandForTests`):
food con una y dos tiradas, lastCheckTurn, contador y regla del agua, orden de azar del constructor, 85, 0x2E por TOP y
por FINAL, 0x2F (→ 163), 0x23, fijar TOP borra FINAL con el pueblo, la regla de PREVIOUS, AdjustTownModifier, la
pausa (con y sin veneno, sin tirada en 239 o sin la marca), 239 → FINAL, check cada 9 turnos, desgaste, EXHAUSTION /
CHANT (también con `WorshipVillager::atSite`), herido → 36 (encendido desde V2; apagado en un caso; 19 con comida,
derribado), SetupMoveToWithHug con `moveState` conserva FINAL (y 0x2F sin paseo), POWER.

`test/test_villager_food.cpp` (V4): el lote de hambre (0,79919), el daño estricto, las interrupciones (0xD0 / 0xD4,
el discípulo, STARVING / CHANT), las cantidades de `v4calc.py` (74, 65, 63, 54, 83, 85), ChangeStateToFindFoodToEat
(117 / 118 / 36 / 33 / lo que lleva / 0), EatFoodHeld (1,0 y 0,9864865, NaN → 0), la doble cogida de GetFoodFromHome,
117 / 118 / 212 y 34. `test/test_villager_home.cpp`: 37, ExitAtHome con PresentAtHome, HomeDecideWhatToDo (0,7875,
GameRand(4)), embarazo y niño, dormir (6 ciclos de 0,4 a 0,70000005), CheckWhenGoingToBed una vez por estancia, la
tienda (árbol, el otro lado, lleno → (−20 m, +10 m)), DoGoingHome sin casa, la puntuación y FindAbodeWithSpaceInTown,
la lista de la casa, CheckNeedNewAbode → 129 → 36, 130, 238, 234, el Shuffle y Abode::Process (1001 turnos).
`test/test_villager_age.cpp`: la capa pura (63 años, r³), SetScaleForAge con GameFloatRand guionizado, el niño de 13
(18 años, cuentas, 234), la vejez y WomanSpecial. En `test_villager_decide.cpp` cambian dos casos de V2 (un aldeano con
hambre come al momento, 117; uno sin casa ni pueblo va a 130).

## Supuestos (inferido / aproximado)

1. **(aproximado)** "Bailando" (Living +0xD8, el DanceGroup) se aproxima con `WorshipVillager::dancing` (lo ponen
   AddDancer / FindDanceGroup y lo quitan ExitAtWorshipSite / RemoveVillagerFromWorshipSite) o `TOP == IN_DANCE`
   (VillagerSpeed.cpp).
2. GRand: `villager::GameRand/GameFloatRand` reenvían a `game_random` (LHRand sobre la semilla sincronizada,
   engine-math.md «Números aleatorios»). **(aproximado)** la secuencia no es la de una partida del original: otros
   sistemas que tiran del mismo flujo (animales, árboles…) siguen con el generador de openblack o no están portados.
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
    puerta. (V4: 37 ARRIVES_HOME ya está portado.)
17. Neutros hasta su hito (no inventan conducta): ProcessReaction (Milagros M-5), Town +0x5E8 (V3), SpecialVillager
    (V14), contador de aldeanos y esqueleto (V12), DROWNING 16 (agua). (V4: CheckHungry, CheckChildGrownUp, WomanSpecial y
    CheckDeathFromOldAge ya están.)
18. Neutros de V2 (devuelven 0 / no hacen nada, con TODO y dirección) (V4: CheckHomelessMoveIntoAbode,
    ChangeStateToFindFoodToEat, CheckWhenGoingToBed, CheckNeedNewAbode, la rama sin casa de DoGoingHome y ExitAtHome
    ya están);
    (V3: TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 ya está, deja 0 o 1 en eax, 0x7460EB / 0x7460F7);
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
