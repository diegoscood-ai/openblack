# Milagros (sistema de magia)

Plan e informes: `dev\tmp_dis\miracles\` (`PLAN.md` y los informes que cita: `core.md`, `casting.md`, `sources.md`,
`destructive.md`, `resources.md`, `protect_creature.md`, `visuals_sound.md`). Direcciones W120. Esta página recoge lo
verificado en `runblack.exe` al portarlo, por hitos.

## Tablas de info.dat (M0, `src/Magic/MagicTables`)

- `GMagicInfo*` por MAGIC_TYPE en `0xD37D10` (42), `GMagicEffectInfo[42]` en `0xCC6630` (0x11C en memoria),
  `GSpellSeedInfo[30]` en `0xD9D678` (0x190). En memoria cada registro va 0x10 bytes detrás del archivo (vtable y
  cabecera): offset exe = offset archivo + 0x10.
- `load_variables` crea un objeto por registro, una clase por sección, en orden de MAGIC_TYPE. Las secciones en el
  orden del archivo dan exactamente 0..41: general 10 (0-9), heal 2, teleport 1, forest 1, food 2, storm/tornado 3,
  shield 2, wood 1, water 2, flock flying 1, flock ground 1, creature 16. Comprobado con el info.dat real: el campo
  `magicType` de cada registro coincide con su posición (`test_magic_tables`, `realInfoDat`).
- `GetMagicInfoAs<T>` devuelve el registro con su clase (`GMagicResourceInfo` vale para food y wood,
  `GMagicRadiusSpellInfo` para storm y shield).
- `timerWhen{OneShot,PlayerCasting,CreatureCasting,ComputerPlayerCasting}` son **float** (segundos, -1 = sin
  límite; los getters 0x5FB7A0..0x5FB7D0 hacen `fld`). MAGIC_TYPE 0 lleva el entero 10 ahí (basura). Tormenta: 40 s.
- `GWorshipSiteInfo::chantsToReserveForMaintaining` es float en el código pero el archivo trae un entero: se lee como
  ~7e-43 (fallo del original, se conserva).
- Nombres de la cola de `GSpellSeedInfo` (archivo 0xF0..0x17C): `selectionGesture` (1 SPIRAL / 2 INVERSE_SPIRAL),
  `gesture`, `gestureStage2` (0), `sizingGesture` (4 CIRCLE en STORM, SHIELD, PHYSICAL_SHIELD), `castType`
  (SPELL_CAST_TYPE), `isKeptInHand`, `castOnObject`, `seedFollowsSpell`, `magicTypes[4]` (sin PU, PU 0, 1, 2),
  `powerUpGestures[3]`, `mesh`, `scale`, `holdLoweringMultiplier`, `holdRadius`, `holdYRotate`, `holdType`,
  `attachInHandEffectToBone`, `deleteSeedOnceCast`, `holderParticle`, `exists`, `iconIndex`, `tooltip`. Sin significado
  aún: 0x10C, 0x138 (igual a la escala salvo en las bandadas), 0x150/0x154, 0x15C (0.1), 0x160 (1), 0x168, 0x178,
  0x17C (1 en FIRE, LIGHTNING_BOLT, HEAL, WEAK, STRONG).

### Ayudantes portados

| Función | Dirección | Detalle |
|---|---|---|
| `GMagicInfo::GetInfoFromText` | 0x5FB3B0 | stricmp con el `debugString` del efecto ("STORM_PU2"); 42 = no encontrado |
| `IsMaintainedSpell` | 0x5FB810 | FOREST (13), SHIELD, PHYSICAL_SHIELD (19, 20) |
| `GetChantsRequiredToCreate` | 0x5FB830 | `costToCreate` (FIRE 3500, STORM 8000); `GScript::GetManaForSpell` usa lo mismo (0x5FB800) |
| `IsCreatureCastFromAbove` | 0x5FB7E0 | `== 1` |
| `IsInAggressiveRange` | 0x5FB840 | 1.0 si min ≤ d ≤ max, si no 0.0 |
| `GMagicEffectInfo::GetTribalPower` | 0x5FB6A0 | producto de `TribalPower[t]` del jugador (GPlayer+0x68) en las tribus marcadas; <0 → 0.5, >100 → 100, ≤0.5 → 0.5; sin jugador 1 |
| `GetTribalPowerTribe` | 0x5FB710 | la primera tribu marcada con poder > 1, si no -1 |
| `GSpellSeedInfo::GetPowerUpFromMagicType` | 0x72AF70 | -1 para `magicTypes[0]`, 0/1/2 para `[1..3]`, -1 si no está. Rareza: con `[3] = 0`, MAGIC_TYPE NONE da 2 |
| `fn_0072AFA0` | | niveles = 1 + `powerUpGestures` no nulos |
| `GetMagicTypeFromPULevel` | 0x72AFC0 | -1 → `[0]`, pu → `[pu+1]` (sin comprobar límites) |
| `GetMagicInfoFromPULevel` | 0x72AFE0 | el de ese nivel; si su tipo es 0, el base |
| `fn_0072B010` | | el gesto de power-up del tipo y su nivel (el tipo base: 0 y -1) |
| `SpellSeedIsOfMagicType` | 0x72B060 | cualquiera de `magicTypes[0..3]` (NONE casa con la primera semilla con un 0: STORM) |
| `GetFirstSpellSeedForMagicType` | 0x72B090 | -1 si ninguna (no 30) |
| `fn_0072B100` | | `fn_0072B010` sobre la primera semilla del tipo |
| `fn_0072B0D0` | | la primera semilla con `exists` e `iconIndex` igual; si no -1 |
| `fn_0072B170` | | semilla por nombre (stricmp con el `debugString`: "HEAL"); 30 = ninguna |
| `fn_0072B1C0` | | la primera semilla del tipo; 30 = ninguna |

- `GMagicInfo::powerupType` vale -1 en todos los registros y nadie lo escribe. `MagicTables::GetPowerUpLevel` da el
  nivel derivado de la semilla (`GetPowerUpFromMagicType` en la primera semilla del tipo: TORNADO 1, rayo de tormenta
  0).
  **Sin verificar** (PLAN R3).

## Tipos de partícula (M0, `src/PSys/ParticleTypes`)

`fn_0068EA00` rellena una vez la tabla de 150 nombres `0xD4EBB0` con `.\Data\Spells\ZSpellFiles\<nombre>.txt`; 121
tienen archivo y el resto queda a NULL (NONE, TORNADO, FIREWORK, FOOD_IN_HAND...). Hay nombres guardados a través de un
registro (`mov eax, str; mov [ecx+off], eax`), que la tabla antigua `psys\pt_table.md` perdía: FOOD y FOOD_POISONED →
SF_Food, HEAL y HEAL_FX → SF_HealChakra, LANDSCAPE_VORTEX_OUT_BEFORE → SF_LandscapeVortexInBefore (sic). Reconstruida
con `tmp_dis\miracles\ptnames.py`.

## Registro de clases de PSys (M0, `src/PSys/PSysRegistry`)

Nombre de clase → fábrica para los modificadores (reglas, emisores) y los creadores. `PSys.cpp` registra las clases que
ya había (`RegisterCoreModifiers`); cada archivo nuevo de reglas o creadores (`src/PSys/Rules/`, `src/PSys/Creators/`)
tendrá su `RegisterXxx()`, llamado desde la lista explícita `RegisterAll()` de `PSysRegistry.cpp`. Lo no registrado
sigue como "not ported yet" (sin efecto). Los creadores registrados derivan de `Creator` y llaman antes a
`ReadCreatorProperties`.

## Vida de los objetos (M0, `src/ECS/Life`)

- Vida en 0..1 (Object+0x48). Los aldeanos la guardan ahora como float (`Villager::life`, antes un porcentaje entero
  que perdía los cambios pequeños de fuego o del cántico); rocas y animales en `components::Life`.
- `Living::Living` 0x5EBEC0 empieza con `SetLife(GLivingInfo::life)`; `Object::Object` 0x636520 con 1.0.
- `Object::ReduceLife` 0x637810: si la vida es menor que la cantidad, 0; si no, vida − cantidad. Devuelve la nueva.
  Aldeanos y animales no la redefinen. No mata: los estados de muerte no están portados, así que el choque físico
  (`HurtByImpact`) mata al llegar a 0 como antes.
- `Object::IncreaseLife` 0x637870 (y `Villager::IncreaseLife` 0x753460, que la llama): hasta 1.
- `Villager::SetLife` 0x756B40 cuenta en el pueblo (Town+0x714) los aldeanos por debajo de 0.7 de vida
  (fn_00756BC0 / fn_00756BD0); el `Town` de openblack aún no tiene esa cuenta. `Object::SetLife` 0x63A140 no deja bajar
  de 0.01 a los objetos con la marca 0x40 (o 0x200 en cierto estado de la interfaz): sin verificar cuáles, no portado.

## Núcleo de los hechizos (M1, `src/Magic/Core`, `src/Magic/Spells`, `src/ECS/Effects`)

Cada hechizo es una entidad con `components::Spell` (el `Spell` de 0xEC bytes); las funciones virtuales del original son
una tabla `SpellOps` por clase (`SpellClass`, la clase de `GMagicInfo` que lo reserva con vt 0x34), cada clase en su
archivo de `Spells/`. Una clase sin registrar corre como `Spell` normal (M1 registra General y Heal).

### Ciclo de vida (Spell.cpp 0x71FB40..)

- Constructor 0x71FB40: sin creador no pone ni creador ni jugador; con él, `player = creator->GetPlayer()` (o el
  jugador neutral), entra **al principio** de la lista (`g_game+0x205BC4`), `+0x48 = IsCreature`, `+0x4C` = el jugador
  del creador (o del SpellIcon) tiene +0x8E0 == 1.
- `GMagicInfo::CastAtPos` fn_005FB490: sin creador usa el jugador neutral; `AllocSpell`, `InitWithPos`; si no devuelve 1
  el hechizo se borra. fn_005FB520 hace lo mismo con `InitWithObject` si el flag `castOnObject` que lee vale 1. Ese flag
  lo lee en `GetSpellSeedInfo(spellSeedType = -1) + 0x118` = 0xD9D600, dentro de GSpellIconInfo[1] (R2 sin resolver);
  aquí se usa el `castOnObject` de la primera semilla de ese tipo de magia.
- `Spell::InitWithPos` 0x71FE50, en orden: deseos de la criatura (fn_00721730, M8), `originalCastPos`, estadística del
  jugador, `SetChants(castData.chants)` (+0x38 = +0x3C), `maxObjects`, `duration`, pos y castPos, copia del
  PSysProcessInfo, `dir = info+0x24`, magnitud (`castData.magnitude`, 40 sin castData), `PSysInterface::Create` en
  `(x, altura del suelo + pos.y, z)`, el registro del jugador +0xDC {castPos, magia (+0xC), turno (+0x10)}. Con PSys:
  `psys->SetPlayer`. **Sin PSys y con `particleType != 0` devuelve 0**: el hechizo no se lanza, y tampoco hay
  reacción. Sin PSys y con `particleType == 0`: `SpellEvent{11}`. Después, la reacción `createReactionOnCast`.
- `Spell::ProcessSpells` 0x720300, una vez por turno: decaimiento de la rejilla de hechizos, lugares de culto
  (fn_0072BF80, M6/M7), iconos (fn_00727350, M7), `GPlayer::ProcessSpellIcons` (M7), **primero todos los
  `ProcessMaintainRequest`**, y luego, por hechizo, `ProcessSpellSeed` (vt 0x500) y `Process` (vt 0x528); un 5 lo
  borra.
- `ProcessMaintainRequest` 0x7204D0 (siempre 1): edad += 0,1 s; `edad > duración` (con duración ≥ 0) → CloseDown;
  un creador que no es funcional → CloseDown y creador = NULL; `enabled = 1`; `creator->UpdateSpellInfo`; los hechizos
  de mano (castType IN_HAND) mueven castPos a la mano. Si está abierto, **la fuerza se lee antes de pagar el turno**
  (`psInfo.power` es la fuerza previa), paga el turno y marca la rejilla. Si está cerrado: `enabled = 0`, `power = 0`.
- `CoreProcess` 0x720660: si está abierto, `Recharge` y, con `power <= 0`, CloseDown. Luego un paso del PSys con el
  PSysProcessInfo; si devuelve 5, se van sus reacciones y el PSys. `Process` 0x720710 devuelve 5 cuando ya no hay PSys.
- `CoreCloseDown` 0x720160: `closedDown = 1` y CloseDown del PSys (vt 0x118).
- `ToBeDeleted` 0x71FD90: sale de la lista, borra el PSys y las reacciones, **borra también la semilla enlazada**
  (vt 0xC sobre +0xAC) y luego CloseDown.

### Cánticos (`Magic/Core/Chants`, 0x720750..0x720A90)

Verificado instrucción a instrucción:
- Nivel de seguridad 0x720880: los mantenidos (FOREST, SHIELD, PHYSICAL_SHIELD) → `initialChants`; los demás
  `max(min(coste/turno × (1000/ms por turno) × 5, initialChants), costPerEvent)`.
- Fuerza 0x720750: sin creador, 0. Con `S > 0`, `chants/S` recortado a 0..1; con `S <= 0`, 1 si quedan cánticos. Luego
  × `GetTribalPower` (0x7216F0, del jugador del hechizo) × `+0x8C` de la semilla × `+0xE4`.
- `PayFor(coste, forzado)` 0x720990: sin creador, 0; gratis (+0x5C), 1. Con `divideCostsByTribalPower == 1`,
  coste / max(poder tribal, 1). Resta el coste; si queda por debajo del nivel y `isSpellRecharged`, el creador repone
  todo el déficit (forzado) o como mucho el coste. Devuelve la fuerza.
- fn_00720830 (pagar el turno): **con coste 0 devuelve 1 sin llamar a PayFor**. `PayForOneEvent` 0x720A90 paga
  `costPerEvent` (sin creador devuelve 0 sin crear el punto del camino de maná). `Recharge` fn_00720910 repone el
  déficit completo.
- Quién paga (`MaintainSpell`, vt 0x58): `GPlayer` 0x64C430 lo da todo **solo si es el jugador neutral**; si no, 0, así
  que los hechizos de una semilla sin icono viven de sus cánticos iniciales. `GameThing` 0x56FED0 lo da todo. El icono
  de culto y la criatura son M7 y M8.
- Rayo lanzado por un jugador (traza real): 5000 cánticos, −50 por turno, fuerza 1 hasta bajar de 2500 (turno 50),
  0,98 en el 51 y 0,8 en el 60. Se cierra en el turno 61 (la edad es una suma de floats de 0,1 y en el 60 aún no pasa
  de 6,0) y el PSys termina en ese mismo turno.

### Eventos y efectos (`SpellEvent`, `ECS/Effects`)

- `Spell::SpellEvent` 0x720F40 ignora los tipos 1 y 11. `ApplyDefaultSpellEffect` 0x720C30:
  - si está cerrado, nada;
  - el hechizo se mueve al evento (y 0);
  - EffectValues del efecto × la fuerza que devuelve `PayForOneEvent` (con 0 no se aplica y devuelve 0), × poder
    tribal × `event.strength`;
  - tipo 4: `SpellHitSpell` con el objetivo; si el otro no cae, devuelve 0;
  - tipo 7 (después de pagar el evento): con objetivo, `CanBeDestroyedBySpell == 1`, sin reacción ni dirección; **sin
    objetivo no aplica nada pero sigue a la reacción y devuelve 1** (0x720DB4 salta a 0x720EBC);
  - tipo 5 con objetivo: si lo acepta, `ApplyEffect` (y el de curar quita el veneno);
  - los demás (y el 5 sin objetivo): `ApplyEffectToMapPos` en la posición del hechizo;
  - al final, la reacción `createReactionOnEvent` y `+0x2C = event.velocity`.
- Con `checkShields` busca un escudo (fn_006D0BC0) y se manda **a sí mismo** un evento de tipo 4 con él mismo como
  objetivo (0x720D84). Pendiente con los escudos (M6).
- `SpellHitSpell` fn_00720B70: coste = fuerza propia × `costPerShieldCollide`. Si el otro tiene fuerza 0 → 1. El otro
  paga forzado y este paga un evento. Si el otro queda sin fuerza y este con fuerza → `SetUpDestroyedReaction` y 1; si
  no, `UpdateStruckReaction` y 0.
- `EffectValues` (0x40 bytes): +0x08 los 7 números (quemar, aplastar, golpear, curar, empujar, alineamiento, creencia),
  +0x24 el radio, +0x28 quién lo aplica (el creador del hechizo), +0x3C el jugador. `*=` (0x525720) solo escala los 7
  números. `IsDestructive` 0x5258C0: quemar, aplastar, golpear o empujar > 0.
- `ApplyEffectToMapPos` 0x525100: celdas de pos ± R; cada objeto disponible que acepta el efecto, con
  `dist(pos, centro de fuego) ≤ R + radio de fuego` y `|alt(pos) + pos.y − (alt(obj) + obj.y)| ≤ altura + R`. Sin
  atenuación. En `Object` el centro de fuego es la posición (0x639AA0) y el radio es `Get2DRadius` (0x639AC0 → vt
  0x64).
- `Object::ApplyEffect` 0x637980 (los aldeanos no lo redefinen):
  - daño = aplastar y golpear positivos × los multiplicadores de defensa (0x637D00; antes pasa el calor al fuego, M5);
    curación = curar × su multiplicador;
  - curar → `IncreaseLife`, daño → `ReduceLife`; si la vida pasa a 0 → `DestroyedByEffect` (un aldeano muere);
  - aplastar > 0,01 en algo que se puede aplastar y sin reacción propia → REACT_TO_OBJECT_CRUSHED (18), iniciada por
    quien lo aplica (o el propio objeto) y con el jugador **del objeto** (vt 0x1C; en un aldeano, el dueño de su
    ciudad);
  - devuelve `(1 − vida0)/curación + vida0/daño`.
- `FireEffect::ConvertTemperatureToDamage` 0x72EEC0: 0 por debajo de Tc; si no, `(T − Tc)/Tc ×
  defenceMultiplierBurn × 0,1`.
- `GAlignment::Update` 0x414410 (R7 resuelta): nada si la vida no cambió. `K = |Δvida| +
  GPlayerInfo.applyEffectAlignmentChangeAddition`: el jugador 0 guarda en +0x64 el puntero a `GPlayerInfo` 0xD47988,
  y +0x1C en memoria es el archivo +0x0C. Para aplastar, golpear, curar y empujar, `pendiente += f(v ×
  GAlignmentInfo[i][col] × K)`; para quemar, lo mismo con `ConvertTemperatureToDamage`. **fn_00414660 compara el
  signo del cambio con el de A** (0 cuenta como positivo): mismo signo `v(1 − |A|/2)`, signo contrario `v(1 + |A|/2)`
  (leído de nuevo en 0x414660..0x4146AD: `je 0x414696` si A ≥ 0; en cada rama `jne` si v < 0; corregido el
  2026-09-30, la primera lectura de M1 decía que solo miraba el signo de v).
- Reacciones (`ECS/Effects/Reactions`): solo los datos. `CreateReaction` 0x6E3D70 crea el objeto de 0x44 bytes y lo
  reparte por las celdas (`SpreadReaction` 0x6E3E10, radio `GetRadius` × potencia, con `GameRand` para los vivos). El
  reparto y las respuestas de los aldeanos no están portados: R8 queda para la IA de los aldeanos.

### Reglas de lanzamiento (`Magic/CastRules`)

- En la vtable de `GMagicInfo` los símbolos tienen los nombres cambiados:
  - **vt 0x30 es la comprobación en una posición**: base 0x5FB420 = 1; curar 0x5FBD20 = `FindTargets`; recursos
    0x5FBA00 = tierra; criatura 0x5FA7E0 = 0; bosque 0x5FAE80; teletransporte 0x5FBE50;
  - **vt 0x2C es la de un objeto**: base 0x5FB430 = vt 0x30 en su posición; recursos 0x5FAC00; bosque 0x42D8E0 = 0;
    criatura 0x5FA7F0.
- La regla fn_005FB5D0: dentro del mapa (celda de 10 m < tamaño) y, según `castRuleType`: 0 siempre, 1 tierra, 2
  influencia `> 0`, 3 las dos.
- `GMagicHealInfo::FindTargets` 0x5FBB00: R = `dummyVar` (10 / 35) y máximo `maxToHeal` (20 / 100), ambos × el poder
  tribal si hay hechizo. Recorre en espiral `ceil(2R/10)²` celdas (GUtils::Spiral 0x74D7E0, tabla +x, +z, −x, −z) y
  cuenta sus objetos móviles vivos que aceptan el efecto y `CanBeHealedByHealSpell`, a menos de R. **No mira si les
  falta vida**: vale cualquier vivo. Con hechizo, cada uno pasa a ser un objetivo de su PSys.
- **`SPELL_AT_POS` no comprueba nada**: el creador es el neutral y la bandera de comprobación va a 0. Además
  `SpellHeal::InitWithPos` 0x72D870 no mira cuántos encontró. Así que una curación del guion se lanza aunque no haya
  nadie (el PLAN esperaba que fallase). Solo la mano pregunta (`SpellSeed::CanCast` 0x729150: la regla y luego vt 0x30).

### Semillas y milagros de un uso (`SpellSeed`, `OneOffSpellSeed`)

- **fn_00729900 está al revés de lo que dice el PLAN §4.1.1**: con 0 → `+0x90 = 1` (lista); con otro valor →
  `+0x90 = 0, +0x94 = 0`. Los iconos de culto pasan 1 (la semilla espera `delayBeforeSeedActive` = 1,5 s) y
  `CreateSpellIntoHand` pasa 0: **una semilla de un uso está lista en cuanto llega a la mano**. Además
  `InterfaceSetInMagicHand` 0x728810 ya pone +0x90 = 1.
- `CreateSpellIntoHand` 0x72A730: con la mano libre, busca el icono de culto del jugador para esa semilla
  (`GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40, que pide un icono al que se le pueda pedir el hechizo **con
  cánticos disponibles** en su lugar) y, si lo hay, crea la semilla de ese icono (fn_007282A0: su creador es el icono,
  `worship::icon::CreateSeed`); si no, la semilla suelta (fn_00728300). La marca «alguna vez activada»; `+0x72 = 1`; la
  carga gratis con todo su coste; la pone en la mano, lista. En Land1 no hay iconos, así que siempre sale suelta.
- `InterfaceSetInMagicHand` 0x728810: `SetPowerUp` del nivel actual. Sin cánticos para relanzar (o con el bit 1 de
  +0x54) la semilla se borra (3); si no, limpia +0x98, +0x70 y +0x94 y queda lista.
- `ProcessInHand` 0x729930 (cada turno en la mano): +0x94++; lista cuando `turnos × 0,1 > 1,5`; si su hechizo se cerró,
  la semilla se borra.
- También portados: `StoreChantsAndAgeFromSpell` 0x728780, `ClearSpellLink` 0x728200 (si el hechizo sigue atado a esta
  semilla, CloseDown del hechizo; si no, solo de su PSys), `ProcessFromSpell` 0x728F70 (siempre 1) y `Cast` 0x729520.
  El lanzamiento desde la mano es M2.
- `OneOffSpellSeed::Create` 0x72A2F0: semilla 0..29; `MobileObject(pos, info 0xD39F3C, 0, 0, escala 1)`. La bola se
  dibuja siempre a escala 1 y +0x6C guarda la escala que se pasará a la semilla.
  - Malla compartida `.\data\spells\meshes\O_Bibble_up.l3d`: una cúpula de 0 a 4,5 m sobre el suelo, con UV en 0..0,25
    (un atlas de 4×4).
  - `UpdateFrame` 0x72A570: `fase = fmod(fase + ms × 18 × 0,001, 16)`, cuadro = int(fase), desplazamiento
    `u = (cuadro % 4)/4`, `v = (cuadro / 4)/4` (vt 0xE8 recibe (u, v)). En openblack el desplazamiento va en
    `UvScroll {u, v}` y el sombreador suma `u` en cuartos.
  - **La bola es translúcida por el alfa del objeto, no por la textura.** La malla tiene una submalla física
    (`Smooth`, no se dibuja) y la visible, una primitiva `AlphaTextured` (modo 4, dos caras) cuya piel ARGB4444 es casi
    opaca (alfa 13-15 de 15 o 0). `Draw` 0x518E90 tiñe el objeto con `0x96FFFFFF` (byte de [0xBE8E8C];
    fn_0080BF10 multiplica el difuso: alfa 0xFF × 0x96 >> 8 = 0x95) y llama a `SetGlobalAlpha(1)` (LH3DObject vt 0x48,
    bit 0x80 de las banderas). Con ese bit el objeto usa la tabla de modos alternativa 0xC387C8: el modo 4 pasa a ser
    el 5 (`SRCALPHA/INVSRCALPHA`, alfa = textura × difuso, escribe Z). Opacidad ≈ 0,58 × la de la textura.
    - Para ordenarla en el Z-sorter, `Draw` adelanta su posición hacia la cámara su radio (vt 0x60) y luego la
      restaura. Así la bola se pinta después de la semilla de dentro. `DrawSpellGraphic` recibe como alfa el byte alto
      del difuso (0x95). openblack: `components::Alpha` = 149/255 en `OneOffSpellSeedArchetype` (pasada `MainBlended`).
      El adelanto por el radio no está hecho (M7 lo necesitará para la semilla).
  - **La bola mira siempre a la cámara.** `Draw` llama cada fotograma a fn_00518720, activa mientras el byte
    [0xBE8E8D] valga 1 (lo vale). Esta gira la matriz 3D del objeto alrededor del centro `c` de la caja de la malla
    (`LH3DMesh::ComputeBoundingBox` 0x8081B0 al cargar, todas las submallas; aquí (0; 2,23; 0)):
    - `D = normalize(centro − cámara)` y `U = normalize(Y − (Y·D)·D)` (Gram-Schmidt con (0, 1, 0), estático 0xCC62D0);
    - monta la matriz con filas (U×D, −D, U), la invierte (fn_007FB3F0), la escala por +0x44 y pone la posición en
      `centro − M·c`. Así el +Y de la malla apunta a la cámara.
    - La submalla visible es solo el casquete de arriba (y de 2,18 a 4,46, radio 2,28), así que se ve una burbuja
      redonda desde cualquier lado. La física es una esfera entera de 0 a 4,37 y no cambia al girar.
    - Solo se mueve la matriz del objeto 3D, no la posición del objeto. La animación 4×4 no depende del giro.
    - `Draw` no dibuja nada si +0x70 (la SpellSeedGraphic) es 0. `CallVirtualFunctionsForCreation` 0x72A450 la crea
      (salvo con la bandera de objeto 0x100, que una bola nueva no tiene): `SpellSeedGraphic::Create(pos, semilla, el
      jugador local, 1, pu)` y `SetAutoUpdate(0)`; `ToBeDeleted` la borra. En openblack lo hacen
      `OneOffSpellSeedArchetype` y `one_off::InterfaceTap` (captura `review2_orb_graphic.png`: la semilla de FUEGO y su
      efecto dentro de la bola del dispensador).
    - openblack: `one_off::UpdateFrames` calcula `OneOffSpellSeed::facing` y `facingOffset`, y `RenderingSystem`
      dibuja la bola con ellos. `Transform` no cambia: el dispensador compara su posición y la física usa la esfera.
      Capturas `orb_face_low.png` (de lado) y `orb_face_top.png` (desde arriba).
- `InterfaceTap` 0x72A640: `CreateSpellIntoHand`, inmersión 0xE, muestra 0x6D (`G_SpellBubblePop_04`) y la bola se
  borra (3).
- Guion del mapa (fn_00715150):
  - caso 83, `CREATE_ONE_SHOT_SPELL(pos, semilla)` → Create(pos, la semilla por nombre, −1, 1);
  - caso 84, `CREATE_ONE_SHOT_SPELL_PU(pos, magia)` → la primera semilla de esa magia y su nivel
    (`GetPowerUpFromMagicType`).

### Guion CHL (`Magic/Script/CHLSpells.cpp`)

- `SPELL_AT_POS` 0x70C190 saca curl, duración, radio, desde, destino y magia. `CastSpellAtPos` 0x70BD60 arma castData
  {radio, initialChants, duración, −1} y el PSysProcessInfo {+0x0C desde, +0x18 destino − desde, +0x24 dir (0),
  potencia 1, +0x34 curl, activo}. `SPELL_AT_THING` 0x70BFA0: con un Object, lanzamiento sobre el objeto.
- `SPELL_AT_POINT` 0x70C560 no lanza nada: devuelve el primer hechizo de esa magia a menos del radio (fn_007217A0). Para
  los escudos llama a fn_0072BA00 (M6).
- `SET_PLAYER_MAGIC` 0x70C6C0 (jugador, magia, activar) → `SetMagicTypeEnabled` (el contador de quién la tiene).
  `HAS_PLAYER_MAGIC` 0x70C750 → «alguna vez activada», y **1 si el jugador no existe**. Los jugadores del guion son
  n − 1 (0 = el neutral; `ConvertScriptPlayerToGamePlayer` 0x6EB9A0).
- `PLAYER_SPELL_CAST_TIME` 0x70C9A0: segundos desde el último lanzamiento (FLT_MAX sin jugador).
  `PLAYER_SPELL_LAST_CAST` 0x70CA50: su magia. `GET_LAST_SPELL_CAST_POS` 0x70CAB0: su punto. `GET_MANA_FOR_SPELL`
  0x70CD40: `costToCreate`.

### PSys enlazado al hechizo (`PSys/SpellLink.h`)

- El hechizo es dueño de su efecto y lo avanza él mismo con su PSysProcessInfo (`manager::StartForSpell` /
  `ProcessForSpell`); `ProcessTurn` no lo toca.
- `StrengthFloatProvider` = `info.power`; `EventConditionTrueWhenEnabled` = `info.enabled`.
- `LandscapeCollide` con `SendEvent` manda el evento 3 (posición global, movimiento del paso, fuerza 1) antes de borrar
  el átomo. El evento 1 se manda al empezar (fn_00673070).
- (inf) Mientras una clase de PSys no está portada, el efecto de un hechizo la cuenta como creadora hasta que se cierra.
  Si no, un efecto sin clases portadas acabaría en el primer turno y cortaría el ciclo del hechizo.

### Rejilla de hechizos (`SpellGrid`)

`u8[64][64]` en 0xD9C370 (celdas de 80 m). `MarkSpellGrid` fn_00721570 pone 0xFF donde hay un hechizo abierto;
fn_007215C0 la hace bajar 0x20 por turno cuando `g_game+0x205A28 == 1` (bandera sin identificar; aquí siempre).

### Orden en el turno (`Magic/MagicLoop.cpp`)

`GGame::ProcessTurn` 0x54E5C0 llama, en este orden: atmósfera (1), anillos de influencia (2), jugadores (3), danzas
(4), bosques (5), los vivos, fuego (6), reacciones (7), `Spell::ProcessSpells` (8), los contenedores de partículas
(9), la física (10), los sonidos del PSys (11), `GScript::Process`, el clima (12), `CHand::GameTurnUpdate` (13) y las
recompensas (14). En openblack `Game.cpp` llama a `magic::ProcessTurn` (1..8) tras `livingActionSystem`, luego corre el
bloque de scripts, que acaba con `psys::manager::ProcessTurn` (9), y después `magic::ProcessTurnEnd` (11..14). La única
diferencia de orden es que los scripts van antes de la 9 y no entre la 11 y la 12. Los PSys de los hechizos no los
avanza el gestor: los avanza su hechizo en la 8.

### Ganchos y trazas

`OPENBLACK_TEST_SPELL`, `OPENBLACK_TEST_SEED`, `OPENBLACK_TEST_ONESHOT` y `OPENBLACK_SPELL_TRACE` están en
[openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración). Capturas y registros en
`dev\_audit\magic\`:
- `m1_oneshot_orb.png` y `m1_oneshot_orb_close.png`: la bola junto al almacén de Land1;
- `orb_translucent_open.png` (bola en 1790, 2625) y `orb_translucent.png` (en el almacén): la bola translúcida;
- `m1_heal_trace.log`: curar en el almacén; 0 objetivos, se lanza igual, se cierra a los 20 s y se borra en el turno 200;
- `m1_lightning_player.log`: la tabla del rayo de arriba;
- `m1_seed_hand.log` y `m1_oneshot_tap.log`: una semilla de FIRE en la mano con 3500 cánticos, lista.

## Sonido de las partículas (lane S, `src/Audio/SpellSounds`, `src/PSys/Rules/Sound.cpp`)

Los bucles y golpes de los milagros (piscina del teletransporte, tornado, escudo, rayos, bolas de fuego...) los suenan
los átomos del PSys. Informe: `visuals_sound.md` §3; lo de abajo está verificado en el exe y en `LHaudiodllR.dll`
(`tmp_dis\sound\dlldis.py`).

- **Propiedad `SOUND_ACTION`** (`SoundActionProperty::ReadProperty` 0x585A70, `src/PSys/SoundAction`):
  `<SOUND_*|NO_SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b`. El nombre se busca en `Data\SoundAction.h`, que
  el juego lee al vuelo (fn_00585590, `LHParseFile::FindEnumVal` 0x7BE530); desconocido o NO_SOUND → -1. Guarda
  LOOPING (bit 0), SOFTRELEASE (bit 2) y USESURFACE (bit 3); ONLYONE se lee y se tira. El código pone 0x22 (bit 1
  "retraso por distancia", bit 5 "altura del suelo") en los truenos.
- **PSysSoundAction** (0x18): acción, luego las ranuras que se pasan al banco (superficie +4, tamaño +8, alineamiento
  +0xC), FadeStep +0x10 y las marcas +0x14. Valores por defecto (ctor en línea, p. ej. CreateRuleAnAtom 0x69F350):
  superficie 1, **tamaño 2**, alineamiento 2, FadeStep 0. Corrección al informe: sin SoundRadiusFP el tamaño es 2, no 0
  (el resultado es el mismo en las filas "*").
- **`AtomCore::StartSound` 0x6745D0**: posición global del átomo (con 0x20, altura del terreno); con USESURFACE,
  `GSoundMap::GetSurfaceType`; alineamiento del jugador dueño (`GetDiscreteAlignmentValue` 0..6 → 1,1,2,2,2,3,3; aún
  sin enlace al jugador: se queda la ranura, ninguna fila de spells.sad lo mira). Crea un `PSysSound` (0x40, ctor
  fn_006D0F70) al principio de la lista del átomo (+0x2C) y en la global 0xD4EE70. Con el bit 1 no suena: +0x34 =
  distancia a la cámara / **347**. Si no, `GAudio::SamplePlayAnimEffect` 0x42A4B0 (modo 0) con los atributos
  {tamaño, alineamiento, 1, superficie, acción} sobre spells.sad.
- **`GSoundMap::GetSurfaceType` 0x71D8E0** (`src/Audio/SoundMap`): 6 fuera de la cuadrícula de 512 celdas o sin bloque;
  7 si la celda no es tierra (`MapCoords::IsLand` 0x603720: bit 0x10 de las propiedades de la celda, `hasWater`); si
  no, `surfaceSound` del material (1..8, otro → 3). Los sonidos de animación usan ahora esta misma función (antes
  "altura ≤ 0 → 7").
- **Banco** (`LHSamplePlayAnimEffect` 0x100146F0 del DLL): `LHFindAttribRow` (fila más exacta, `src/Audio/AnimEffectBank`,
  compartido con los sonidos de animación), una muestra al azar de la lista y **no suena si la cámara está más lejos
  que el maxDist de la muestra** (S_TeleportPool: 170). El modo de reproducción de la muestra (+0x274, bit 0x400) 2 =
  no hace nada si ya suena para ese objeto; así el bucle se puede "re-emitir" cada turno sin duplicarse. El argumento de
  modo: **0 tocar, 1 `LHSampleStop`, 2 `LHSampleReleaseLoop`** (resuelve la duda de `visuals_sound.md` §7.3).
  `GAudio::SamplePlayAnimEffect` además filtra por modos de ayuda/cine y el interior de la ciudadela: no portado.
- **Vida del PSysSound** (fn_006D11A0, desde `PSysGlobal::GameLoopEnd` 0x68F5B0 una vez por turno, con la duración
  del turno):
  - átomo vivo, LOOPING: si la cámara está a menos de **1200**, se re-emite (modo 0).
  - átomo vivo, retrasado: si +0x34 > 0 se le resta el turno; al pasar de 0 (estrictamente negativo) suena. Un retraso
    que caiga justo en 0 no suena nunca (se conserva).
  - átomo muerto (StopSound 0x674500 pone +0x38 = 0; el destructor del átomo hace lo mismo): si aún suena, con FadeStep
    baja el volumen `vol − FadeStep` (0..127, no menos de 0) cada turno, y una vez (+0x3C) manda el modo 2 con
    SOFTRELEASE (el bucle acaba su vuelta) o 1 sin él (corte). Cuando ya no suena, se borra.
  - `PSysSound::Get3DSoundPos` 0x6D1000: la posición dibujada del átomo (+0xF4), con 0x20 a ras de suelo.
- **Reglas** (`Rules/Sound.cpp`): `StartStopSoundOnCondition` 0x69DC40 (SoundCondition cierta o ninguna → suena si el
  átomo no tiene ya esa acción; falsa → la para con FadeStep), `AddSoundToAtom` 0x69DCA0 (una vez por átomo al llegar a
  Delay y con la condición; StopOtherSoundsFirst; el temblor de cámara `LH3DCameraChecker::Create` no está portado),
  `RemoveSoundFromAtom` 0x69DDD0 (una vez: para esa acción con FadeStep).
- **Quién suena al crear**: CreateRuleAnAtom (SoundOfCreate; con SoundRadiusFP: < Small 200 → 3, < Medium 500 → 2, si
  no 1), CreateRuleSphere (solo el primer átomo), EmitterRuleConical y UR_WillowWisp (SoundEmission en cada átomo).
  Para las lanes de bola de fuego: `SizeFromThrow` (> 0.6 → 1, > 0.3 → 2, si no 3) y `SizeFromImpactSpeed` de
  UpdateRuleGravityWithFloor fn_006A1630 (< Medium → 3, < Large → 2, si no 1; antes exige alfa ≥ MinAlpha, |v| ≥ Small
  y que el átomo no tenga ya esa acción).
- En openblack: `audio::spell_sounds::ProcessTurn` va en la ranura 11, dentro de `magic::ProcessTurnEnd`
  (`MagicLoop.cpp`), que `Game.cpp` llama justo después de `psys::manager::ProcessTurn` (la ranura 9, al final del
  bloque de scripts). Así el sonido ve los átomos de este turno, como en el original. El único cambio de orden que
  queda es que `GScript::Process` va antes de la ranura 9 y no entre la 11 y la 12. El cargador de bancos de
  `Game.cpp` dejaba de leer un
  .sad en la primera muestra vacía: spells.sad tiene una vacía en la 31, así que faltaban la 32..88 (piscina del
  teletransporte, rayos, fuegos artificiales...). Ahora la salta y sigue.
- Prueba: `OPENBLACK_PSYS_SOUND_TRACE=1 OPENBLACK_TEST_PSYS="SF_TeleportVortex,1478,2129,0,1,5"` en Land1 (cámara
  inicial a 157 del punto, dentro de los 170 de S_TeleportPool): "start SOUND_SPELL_TELEPORT_POOL (76) ... ->
  spells.sad/39 (S_TeleportPool.wav) looping", a los 5 s "release loop" y medio segundo después "deleted" (acaba su
  vuelta). `test_spell_sounds` comprueba el enum, las marcas, los tamaños y, con `OPENBLACK_GAME_PATH`, las filas
  reales de spells.sad.
- Sin portar: el alineamiento del dueño, el temblor de cámara, los filtros de estado de juego, las repeticiones finitas
  (loop > 0 se toca una vez) y el tope global de distancia del DLL (+0x44 del sistema de audio).

## Influencia (M1i, `src/ECS/Influence`)

Investigación completa en `dev\tmp_dis\miracles\influence.md`. Todas las distancias son en x,z
(`GetDistanceInMetres` 0x74CD70).

- **Consulta.** `Influence::CalculatePlayerInfluence(pos, jugador, 0, tipo, aliados)` 0x5CD170 devuelve de -1 a 1; "en
  la influencia" es `> 0`.
  - Sin jugador da 0.
  - Con la marca de partida 0x2000 (el valor de registro "GatheringFlag", `start_system` 0x6433B1) da 1 en todas
    partes. En openblack es `OPENBLACK_INFLUENCE_EVERYWHERE`.
  - Luego mira la influencia virtual (`SET_VIRTUAL_INFLUENCE`, sin portar; es la única que lee `tipo`) y después
    `CalculatePlayerRawInfluence`.
  - Si esa da ≤ 0 y se piden aliados, devuelve la del primer aliado con influencia (`IsAllied` y +0x950 > 0.1). Si no
    hay aliado, 0.
  - Quién llama y con qué argumentos:
    - las reglas de lanzamiento 2 y 3 (fn_005FB5D0): aliados = 1;
    - la mano, `m_InInfluence` (GInterface+0x48, fn_005D1120): con la posición de la mano, tipo 1 y aliados = 1;
    - `GInterfaceStatus::Process` 0x5DC558: suelta el objeto bloqueado (coger por tandas) fuera de la influencia. Ya
      está en `HandResources.cpp`.
- **`CalculatePlayerRawInfluence`** 0x5CD230:
  - suma la ciudadela, las ciudades del jugador (GPlayer+0xA50) y los anillos, y la deja entre -1 y 1;
  - un anillo anti del mismo jugador que cubra el punto devuelve 0;
  - un anillo pegado a un objeto que está en la mano no cuenta;
  - `CameraExclusion::InsideInclusion` siempre es cierta en una partida normal (solo la usa el campo de fuerza de
    cámara de una partida guardada).
- **Ciudadela y ciudades: todo o nada.** Aportan su radio si el punto está dentro, así que la suma pasa de 1 y se queda
  en 1. Para la ciudadela, dentro es `r > d` (fn_004630F0); para la ciudad, `d < r` (fn_007479E0).
  - **Ciudadela.** `Citadel::GetInfluence` 0x464090 = `playerInfluenceMultiplier × Citadel+0x6C`.
    - +0x6C se fija una vez, al crear el primer CitadelHeart (0x4649B0): `M2 × (tierra ? storyInfluence[tierra-1] :
      influence)` de GCitadelHeartInfo, es decir 125, o 750/450/250/450/450 en las tierras 1 a 5.
    - M2 vale 1 con `CREATE_CITADEL` y la escala del plano con la ciudadela planeada. En Land1 la construye el guion
      del desafío: `BUILD_BUILDING(1915.05, 2508.89, 1.0)` → `ForceBuildingOfPlannedAtPos` →
      `CreatePlannedNoFixedCheck`.
    - Resultado: **750 m en Land1**, 450 en Land2 y 250 en Land3 (por eso en Land3 se empieza con tan poca).
  - **Ciudad.** `Town::Process` 0x747380 recalcula el radio (+0x5C8) cada turno:
    - la base es `Town::GetBaseInfluence` 0x73FD40: la `influence` de GTownInfo (25), o su `storyInfluence[tierra-1]`
      (25/25/25/50/25);
    - a eso se suma, cada `processAbodeEvery` (1) turnos, el `GetInfluence` de cada edificio de la ciudad, salvo si
      Town+0x5F8 (último argumento del constructor, 0 en `CREATE_TOWN`; no es `SET_TOWN_UNINHABITABLE`, que escribe
      +0x5F4);
    - el total se multiplica por `townInfluenceMultiplier`;
    - solo cuentan las ciudades del propio jugador: una NEUTRAL solo cuenta para el jugador neutral.
  - **Edificio.** `Abode::GetInfluence` 0x4072A0 =
    `% construido × escala × vida × GAbodeInfo::influence × (adultos +0xB4 + niños +0xB7 + 1)`
    (`MultiMapFixed::GetInfluence` 0x52ECA0 por ese factor).
    - Valores de `influence`: casas 5, tótem y centro del pueblo 90, almacén 45, taller y dispensador 25, cementerio
      30, guardería y campo de fútbol 20, maravilla 150, campo 5.
    - Los campos también son Abode.
    - `GAbodeInfo::Find` de openblack devolvería los registros sin tribu del final (arca 1, tótem 120), así que el
      registro se busca por la malla.
  - **Globales de la tierra.**
    - Los multiplicadores valen 1 por defecto (GGame::Init). Los cambian `SET_TOWN_INFLUENCE_MULTIPLIER` (caso 96:
      Land3 0.5, Land4 0.8, Land5 0.6) y `SET_PLAYER_INFLUENCE_MULTIPLIER` (caso 97).
    - `SET_LAND_NUMBER` escribe g_game+0x205A08.
    - La tierra 6 lee el float que va detrás del array de historia.
- **Anillos** (`InfluenceRing`, 0x44 bytes; lista g_game+0x205C4C, el más nuevo primero):
  - Campos: posición, objeto seguido (+0x28), jugador (+0x34), radio (+0x38) y anti (+0x3C).
  - Aportan `Influence::CalculateInfluenceOnRange(d, r)` 0x5CD560, con GInfluenceInfo 0.4 / 0.2 / 0.2:
    - 1 hasta 0.4·r;
    - de 0.8 a 0 hasta 0.6·r;
    - de **0.2** a 0 hasta r. El 0.2 es un double en 0x8C7C68, y el salto de 0 a 0.2 en 0.6·r se conserva.
  - `ProcessRings` 0x5CDB90: el anillo sigue a su objeto, y si el objeto desaparece se borra con él.
  - `IsInAntiInfluence` 0x5CD490: el punto está dentro de un anillo anti de ese jugador (`d ≤ r`).
- **Guiones.**
  - `CREATE_INFLUENCE_RING(pos, jugador, radio, anti)` (caso 59).
  - CHL `INFLUENCE_OBJECT` (60) e `INFLUENCE_POSITION` (61): en la pila van anti, jugador (índice de juego, sin
    convertir), radio y objeto o posición; devuelven el anillo.
  - CHL `GET_INFLUENCE` (62): en la pila van posición, `raw` y jugador (de guion: 0 = el local, n = n − 1). Aliados =
    `raw == 0`.
  - El guion de LandT abre un anillo de 1000 m en (2185.6, 2409.5).
- **Dibujo** (`GGame::Update3DInfluence` 0x555280, cada 10 turnos si ha cambiado algo más de 0.01):
  - un círculo por ciudadela y por ciudad, con el color del jugador (los anillos no se dibujan);
  - solo con la opción `WorldRoom::ShowInfluence`;
  - no está portado.
- **Sin portar:**
  - la influencia virtual;
  - los aliados (openblack no tiene alianzas);
  - la regla de multijugador (sin ciudadela, 0);
  - el dibujo;
  - `CalculateMostInfluentialPlayer` y sus ayudantes 0x5CD4F0 / 0x5CD600 / 0x5CD6C0.
- **Diferencia heredada.** openblack crea el templo de `CREATE_PLANNED_CITADEL` ya construido. El original le da la
  influencia cuando el guion de Land1 lo construye, a los pocos segundos.

## Tiempo y clima (M6a, `src/ECS/Weather`)

Dos capas, como en el original: **LH3DAtmos** (el motor 3D) guarda una rejilla del tiempo que hacen las *tormentas*
registradas, y **GClimate** (el juego) le suma la temperatura y el viento de los *climas* y crea tormentas naturales.
Todas las consultas del resto del juego pasan por `GClimate::ComputeWeather`.

- `Weather.h` / `WeatherQueries.cpp`: las consultas (la cabecera pequeña que incluyen las demás lanes).
- `WeatherInfo.h`: la estructura de 8 bytes y su aritmética de bytes.
- `Atmos.{h,cpp}`: LH3DAtmos, la rejilla de 128 × 128 celdas de 40 m.
- `Storms.{h,cpp}`: LH3DStorm / GWeather, los volúmenes registrados.
- `Climate.{h,cpp}`: GClimate, `ComputeWeather`, `ProcessAll`, las tormentas naturales.
- `Calendar.{h,cpp}`: la parte de fecha de GGameInfo (día del año, mes, estación).
- `WeatherThing.{h,cpp}`: los objetos de tiempo del CHL.
- `Rain.{h,cpp}` + `Graphics/RendererRain.cpp`: la lluvia dibujada.
- `WeatherLoop.{h,cpp}`: las llamadas desde el turno; `WeatherDebugHooks.cpp`: los ganchos de prueba.
- Guiones: `Magic/Script/MapScriptWeather.cpp` (comandos del mapa) y `Magic/Script/CHLWeather.cpp` (nativas CHL).

### `WeatherInfo` (8 bytes, `LH3DAtmos`)

Se devuelve en `edx:eax`, y la misma disposición es una celda de la rejilla, la cola de un LH3DStorm (+0x48) y el
resultado de `ComputeWeather`: `temperature` (grados), `rain` (0..100), `snow`, `overcast`, `windX`, `windZ`
(× 1/8 = m/s), `snowCover` y `stamp` (el fotograma en que se calculó la celda).

La aritmética es de bytes con signo, y el original mezcla dos formas: la temperatura se **envuelve** (`add cl, al`,
`WrapAdd`) y el resto se **recorta** a -128..127 (`ClampAdd`). La interpolación es `a + ((b - a) × w >> 8)` con
desplazamiento aritmético (`LerpByte`).

### LH3DAtmos: la rejilla de 40 m (`Atmos.cpp`)

- 128 × 128 celdas de 40 m (0xEDC350) = 5120 m, justo el mapa. Fuera de la rejilla se usa el *tiempo ambiente*
  (0xEDC348), que en una partida es todo 0 (solo lo escriben `LHInetWeather` y la partida guardada).
- Una celda se recalcula **por demanda** (`fn_00834EE0` / `fn_00834E20`) cuando su `stamp` no es el fotograma actual
  (0xEDC340): parte del tiempo ambiente y le suma cada tormenta registrada en la **esquina** de la celda
  (`ix × 40, 0, iz × 40`), no en el punto pedido. De ahí que la lluvia salga en escalones de 40 m.
- `LH3DAtmos::UpdateGame` 0x8356E0 (la primera llamada de `GGame::ProcessTurn`, con la hora visual y 0,1 s) actualiza
  las tormentas y avanza el fotograma; al desbordar 256 limpia todos los sellos y vuelve a 1. La posición del sol que
  también calcula (0xEDD378: 4000, cos(t·π/12) × 1100 − 150, sin × 800) es para la iluminación, aquí no se usa.
- `LH3DAtmos::GetWeather` 0x834F80: la celda de (x, z) y luego la altura: **por encima de 50 m la temperatura baja
  0,075 por metro**, y por encima de 200 m es un −11 fijo (`add al, 0xF5`).
- `LH3DAtmos::GetWeatherSmooth` 0x835180: bilineal entre las cuatro celdas en pasos de 1/256; por encima de 200 m
  tiende al tiempo ambiente con peso `(altura − 200) / 4` (tope 256) y después aplica la misma bajada por altura. Es la
  que usan la cámara (nubes) y `GetWindAt(p, true)`; todos los getters de GClimate piden la no suavizada.
- `SnowCover` (0xEDC344, la nieve acumulada en el suelo, `fn_0086CB80 × 0,5`) **no está portada**: `snowCover` queda 0.

### Tormentas: LH3DStorm y GWeather (`Storms.cpp`)

El descriptor LH3DStorm (0x50 bytes, constructor `fn_0083F3F0`) trae por defecto radio interior 100, exterior 300,
fundido 10 s, vida 100 s, fuerza 1, 8 nubes, negrura 0,5, elevación 160, velocidad de caída 1, sin relámpagos, y como
tiempo 10 grados / lluvia 100 / nublado 100 / viento (10, 0). El objeto GWeather (0x3C0, lista 0xEEA37C, el más nuevo
primero) lo copia y añade edad, destino, velocidad, contador de borrado, temporizadores de relámpago, posición de
dibujo, radios y fundido.

- `GWeather::Update` 0x83F900 (desde `fn_0083F840`, una vez por turno con 0,1 s):
  - al pasar `lifeTime` se marca para borrar;
  - el **fundido** sube de 0 a `strength` en `fadeInTime` y baja igual al final, y el **radio interior** lo acompaña
    (el exterior no);
  - se mueve hacia su destino a `speed` m/s en x,z;
  - los relámpagos, ya fundida: `forkTimer` y `sheetTimer` cuentan atrás y al llegar a 0 se recargan con
    `min + rand(max − min)`. El de horquilla crea un PSys de rayo (0xEEA384) y el de sábana suena el trueno (0xEEA388);
    aquí son dos *callbacks* (`SetForkCallback` / `SetSheetCallback`) que otra lane rellenará. El destello del terreno
    (`fn_00837290`) no está portado.
- **Borrado en dos turnos**: `fn_0083F7B0` solo marca (+0x94 = 1); el contador sube en cada `UpdateAll` y la tormenta
  se destruye cuando pasa de 2. Mientras está marcada ya no aporta nada a la rejilla. `GClimate::ToBeDeleted` 0x7713E0
  sí las destruye al instante.
- `GWeather::CalcAtmos` 0x8400E0: si el punto está en la caja y el círculo del radio **exterior**, el peso es 1 dentro
  del interior y baja linealmente a 0 en el exterior; `w = peso × fundido × 256`. Con `w == 0` no hace nada. La
  temperatura **tiende** a la de la tormenta (`LerpByte`) y lluvia, nieve, nublado y viento se **suman**
  (`ClampAdd(x, (valor × w) >> 8)`).
- `fn_0083F750` (`KILL_STORMS_IN_AREA`) marca las tormentas cuya distancia 2D al punto sea menor que radio + su radio
  exterior.

### GClimate: los climas (`Climate.cpp`)

Lista `g_game+0x205CF4`, el más nuevo primero; el **clima del mundo** (id 0, `g_game+0x250534`) está en (2560, 2560)
con radio 5120 y tipo `WORLD`. `ComputeWeather` lo crea si falta, así que en la práctica siempre existe.

- Los crea el guion del mapa con `CREATE_WEATHER_CLIMATE(id, info, "x,z", r1, r2)` (`fn_00771300`, caso 60): id 0 hace
  el del mundo (sustituyendo al anterior e ignorando el resto de argumentos), otro id uno local con los radios
  ordenados y `maxStorms = int(r2 × 0,001 + 1)`. `CREATE_WEATHER_CLIMATE_RAIN/TEMP/WIND` fijan después el resto.
  **Land1 tiene cuatro** (líneas 2270..2285 de `Scripts\Land1.txt`): el del mundo con 10,8 grados y viento (24, 0), dos
  cálidos de 37,8 grados en (2157, 2425) y (1740, 3145), y uno **muy frío de −35,2** en (2701, 2568) con viento (40, 0).
- `GClimateInfo` (7 filas de info.dat) da por estación `rainMin/Max`, `tempMin/Max` y `windMin/Max`. La estación sale de
  `GGameInfo::GetSeason`: 0 primavera (día 79), 1 verano (171), 2 otoño (263), 3 invierno.
- **Temperatura** (`fn_00773ED0` / `fn_00773F40`, cada turno): el objetivo es
  `factorHora[hora] × factorMes[mes] × (max − min) + min`, con las tablas 0xC249D8 (0,5 a las 0 h, 1,5 a las 16 h) y
  0xC249A4 (1,0 en julio, 0,1 en enero; **febrero es 0**, como en el exe). La temperatura se acerca al objetivo en
  `|int(objetivo)| × 0,1` grados por turno, así que **nunca converge: oscila** alrededor (en Land1 el guion guarda 10,8
  con objetivo 12).
- **Lluvia** (`fn_00773D60`, una vez por día de juego): si ya está lloviendo solo cuenta los días; si no, el deseo crece
  con dos tiradas de `rainMax × 0,02` y se recorta a 1. Los términos de mes, hora y naturaleza de `GClimateRainInfo`
  valen 0 porque esa tabla **no está en info.dat** (0xDCB8D0 queda a ceros).
- **Viento** (`fn_00774AA0`, por día): de la franja de la estación en la dirección `windAngle`. El peso es
  `int(exp(−((deseo − 0,5) × 5)²))`, que solo vale 1 con un deseo de exactamente 0,5, así que en la práctica es siempre
  `min + max`.
- **Un día de juego** son 36000 turnos por año / 365,25 = **98,56 turnos** (9,9 s). La tierra empieza el 5 de mayo de
  1998 a las 18:05:30 (`GGameInfo::GGameInfo` 0x557730), es decir en el día 125,75 del año.
- `GClimate::ComputeWeather` 0x771640:
  1. la rejilla en el punto (suavizada o no);
  2. la temperatura y el viento **del clima del mundo** como base;
  3. **más cada clima de la lista, el del mundo incluido otra vez**, con el peso del radio (1 dentro del interior, 0 en
     el exterior). No es un descuido de la copia: el original lee 0x250534 y luego recorre la lista completa, así que
     **el clima del mundo cuenta doble**. Por eso Land1 da 20 grados en el llano (9 + 9) y no 10.
  4. el resultado se suma con recorte a los bytes de la rejilla.
- `GClimate::ProcessAll` 0x771BE0 (turno, después de los guiones) y `fn_00772330` por clima: actualiza la temperatura;
  sus tormentas derivan con el viento de la rejilla (× 0,01 m por turno); en un día nuevo, una tormenta que se salió de
  su clima (o, la del mundo, que entró en cualquier clima) salta al tramo de desvanecimiento y el deseo vuelve a 1; y si
  ya ha llovido bastante esta estación también se desvanece. Luego, en día nuevo, lluvia y viento, y con deseo 1 una
  tormenta nueva.
- `GClimate::CreateStorm` 0x772E00: `FindWhereToCreateStorm` 0x772BE0 elige el sitio (el del mundo, una celda de 10 m al
  azar de las 512, reintentando hasta 20 veces mientras caiga en otro clima y fuera de la isla; uno local, a
  `r² × radioInterior` en una dirección al azar). El tamaño es `rand(1000)` (mundo) o la distancia al centro, recortado
  a 160..900; el exterior es `int(tamaño × 1,1)`; la vida `(rainMax × 100 − díasLloviendo) × 10` s con un mínimo de 20;
  la elevación 500. La negrura es `exp(−((t − 30) / 15)²)`, y por encima de 30 grados es una tormenta eléctrica con los
  relámpagos del clima. **Por debajo de 0 grados es nieve pura**; por encima, la parte de nieve es `exp(−(t × 0,2)²)`
  (todo lluvia pasados unos 10 grados) y el resto lluvia.
- `PAUSE_UNPAUSE_CLIMATE_SYSTEM` y `PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM` (0xC24759 / 0xC24758) apagan la
  actualización y la creación.

### Consultas (`Weather.h`)

Todas sobre `ComputeWeather` en un LHPoint (x, z en metros y la **altura absoluta** en y, que es la que baja la
temperatura):

| Consulta | Original | Nota |
|---|---|---|
| `GetMaxRainingOrSnowingAt` | 0x771600 | `max(lluvia, nieve)`; el fuego se enfría con `1 + 0,01 ×` esto |
| `GetRainAt` / `GetSnowAt` | 0x771570 / 0x7715B0 | 0 mientras no haya clima del mundo |
| `IsRainingAt` / `IsSnowingAt` / `IsSnowCoveredAt` | 0x7714B0 / 0x7714F0 / 0x771530 | el byte > 0 |
| `GetTemperatureAt` | `GClimate::GetTemp` 0x771A80 | **no** es la temperatura ambiente del fuego, que es la constante 24,7 de `MapCoords::GetTemperature` 0x605CC0 |
| `GetWindXAt` / `GetWindZAt` | fn_00771AB0 / fn_00771AE0 | los bytes |
| `GetWindAt` | fn_00771B10 | `(vientoX/8, 0, vientoZ/8)`; lo leen el fuego, `UR_CloudMoverNew` y el rebote de las bolas de fuego |

Los cinco primeros comprueban antes que exista el clima del mundo (devuelven 0 / falso si no); los demás lo crean.

### Objetos de tiempo del CHL (`WeatherThing.cpp`)

`CREATE` de un `SCRIPT_OBJECT_TYPE_WEATHER_THING` (tipo 15) crea un WeatherThing con una tormenta hecha de
`GWeatherInfo[subtipo]` (radio interior 100, exterior 300, vida 100 s, nubes a 500, y la temperatura, humedad, nieve,
nublado y viento de la fila como bytes). Guarda una **copia** del descriptor; las nativas la editan y `UpdateStats`
0x774370 la vuelca a la tormenta (recargando los temporizadores de relámpago): `CHANGE_WEATHER_PROPERTIES` (123),
`CHANGE_LIGHTNING_PROPERTIES` (124), `CHANGE_TIME_FADE_PROPERTIES` (125), `CHANGE_CLOUD_PROPERTIES` (126).
`WeatherThing::ProcessWeatherThings` 0x7741A0 corre cada turno: si su tormenta ya no está la olvida, con
`SetAffectedByWind` la tormenta deriva con el viento de `ComputeWeather` × 0,01, y el objeto se mueve con ella.

`CREATE_WEATHER_STORM` del guion del mapa (caso 0x717328) crea una tormenta de un clima. Su tercera cadena
(`"nublado,nieve,temp,lluvia,vientoX,vientoZ"`) se lee con `%d` **sobre los bytes** del descriptor, 4 bytes por valor,
así que cada valor pisa los tres siguientes: solo sobreviven temperatura, lluvia y el viento (nieve y nublado acaban en
0 o −1). Ninguna tierra original lo usa.

### La lluvia dibujada (`Rain.cpp`, `Graphics/RendererRain.cpp`)

`LH3DAtmos` tiene un objeto de lluvia (`fn_00833DA0`) con **128 rayas** de 0x1C bytes. Cada raya (`fn_00833D10`) es una
línea de `(x, −50, z)` a `(x + dx, elevación, z + dz)` alrededor del centro de la baldosa, con `x, z` en ±80, la
inclinación `dx, dz` en ±15, un desplazamiento de textura 0..1 y una velocidad de 0,1..0,2 vueltas por segundo. La u va
del desplazamiento a desplazamiento + 1 (la textura se repite a lo ancho) y la v es fija, 0x3F010000 = 0,50390625, la
fila 129 de `Data\Textures\atmos.raw`, que es una hilera de trazos blancos.

- `LH3DAtmos::Update3D` 0x8357A0 (por fotograma): la tormenta **más cercana a la cámara** fija la elevación y la
  velocidad de caída (160 y 1 sin tormenta); las dos se acercan un 0,3 del camino por fotograma y se recortan a
  40..640 m y 0,3..5. Después, *si se dibujó en el fotograma anterior* (0xEDC300), las rayas avanzan: la fase sube 2,4
  por segundo y al desbordar la raya se coloca de nuevo.
- `LH3DAtmos::Render3D` 0x836250: por cada bloque de tierra, dos muestras de la rejilla (el centro del bloque y el
  centro + 40) y el mayor de sus bytes de lluvia y nieve; si pasa de 5, encola un objeto en el Z-sorter con los tres
  bytes bajos de su dato de usuario = `(x/80, z/80, valor × 88 / 100)` (`fn_008341B0`). Al vaciar el Z-sorter
  (`fn_0082F280`) el callback `fn_00833F80` los desempaqueta y llama a `fn_00834370(x, z, 0, 128, alfa)`. Por eso la
  baldosa es una sola por bloque, centrada en su origen + 80.
- `fn_00834370`: nada a más de 400 m de la cámara; entre 100 y 400 el alfa y el número de rayas se multiplican por
  `1 − (d − 100) / 300`. El alfa de abajo es ese, y el de arriba `alfa / ((2d/400 + 1) × 5)`, así que la raya se
  desvanece hacia la nube. Cada raya además se atenúa con su fase: por debajo de 0,05, `fase × 20`; por encima de 0,95,
  `(1 − fase) × 20`. El material es `AtmosMaterial` en modo 6 (alfa) con prueba de Z y sin escribir Z.
- Con el valor por encima de 0x2C el original también llama a `g_water_drop_cb` para las gotas que salpican el suelo:
  **no está portado**. Tampoco la nieve dibujada (`fn_00834120` → `fn_00834BF0`) ni el destello del relámpago.
- En openblack las rayas son líneas de un píxel con el programa `WorldQuad` (`atmos.raw` + `atmosa.raw`) en la pasada
  `MainBlended`.

### Orden en el turno (`WeatherLoop.cpp`)

`Magic/MagicLoop.cpp` llama, en las ranuras de `GGame::ProcessTurn` 0x54E5C0: `ProcessTurnStart` en la 1
(`LH3DAtmos::UpdateGame`), `ProcessTurnEnd` en la 12 (`ProcessWeatherThings` y `GClimate::ProcessAll`), y `UpdateFrame`
cada fotograma desde `magic::Update` (las rayas de lluvia). `OnLoadMap` lo deja todo vacío.

### Sin portar / UNVERIFIED

- `SnowCover` (la nieve acumulada, 0xEDC344) y la nieve dibujada.
- El destello de los relámpagos (`fn_00837290`) y la luz de tormenta del terreno (0xFA2768); los *callbacks* de rayo y
  trueno están puestos pero vacíos (los rellenará la lane del rayo).
- Las gotas en el suelo (`g_water_drop_cb`).
- La influencia virtual del tiempo, `LHInetWeather` y el tiempo ambiente de la partida guardada.
- `GClimate+0x84` ("relámpago aunque haga menos de 30 grados"): **no se ha encontrado quién lo escribe**, se asume 0.
- El campo de distancia por bloque (+0x9BC) que usa `Render3D` para descartar bloques: aquí se usa la distancia 2D a la
  cámara con el mismo umbral (400 + 160), y el corte real sigue siendo el de 400 m de `fn_00834370`.

### Ganchos

`OPENBLACK_TEST_WEATHER="x,z,radio[,lluvia[,fundido[,temperatura]]]"`, `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` y
`OPENBLACK_WEATHER_TRACE=1`; ver
[openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración).

## Comida y madera (M3, `Magic/Spells/SpellResource`, `Magic/Objects/Magic{Food,Wood}`, `ECS/PotResource`)

Informe: `resources.md` §1. Lo de abajo está leído en el exe (W120) salvo lo marcado.

- **SpellResource** (MAGIC_TYPE 14 FOOD, 15 FOOD_PU1, 21 WOOD; `GMagicResourceInfo::AllocSpell` 0x5FAC20, +0xEC «primer
  evento hecho» a 0 en fn_00724C80; sin Process propio).
  - `SpellEvent` 0x724D80: primero el evento por defecto (EffectValues de FOOD/WOOD: solo alineamiento 1, reacción 21);
    luego, salvo el evento 1 o si está cerrado, n = `resourceAmountFirstEvent` la primera vez y
    `resourceAmountPerEvent` después; `PayFor(costPerUnit × n)` (el resultado no se mira); fuera del mapa, en agua
    (`IsDryLand` 0x603620: byte de altitud ≥ 4) o con fuerza 0 se paga y no cae nada; si no,
    `Pot::AddResourceToPos(pos, IS, tipo, ftol(poderTribal × n), 0, x)`.
  - Comida: 200 la primera vez (1400 cánticos), luego 18 (126); FOOD_PU1 20; madera 500 (1500), luego 20 (60).
  - `HasEnoughChantsAndLifeForRecast` 0x724C90 = costPerUnit × primero ≤ cánticos.
  - **El campo `poisoned` de FOOD_PU1 no envenena**: fn_00724CE0 empuja `(pos, IS, FOOD, n, 0, poisoned == 1)` y en
    `AddResourceToPos` el 5.º argumento ([esp+0x60]) va a `SetPoisoned` (vt 0x69C, con `IsPoisoned` vt 0x4A4) y el 6.º
    ([esp+0x64]) a `SetSpeedUp` (vt 0x864, con vt 0x4A8). La comida PU hace pilas «speed-up» (chispas
    PILEFOOD_SPEEDUP, 46). `resources.md` §1.4 tenía los dos argumentos al revés.
- **`Pot::AddResourceToPos` 0x66F270** (`ECS/PotResource.cpp`, lo usan la mano al soltar una vasija y los granos):
  - fuera del mapa, 0; recorre 9 celdas de 10 m con `GUtils::Spiral` 0x74D7E0 (la de pos y sus 8 vecinas), en cada una
    la lista fija (+4) y luego la móvil (+0), mientras quede algo;
  - acepta un objeto si `IsResourceStore(tipo)` (vt 0x680) o `IsPot` (vt 0x4B4) y `GetResourceType` (vt 0x690) == tipo,
    y `IsCloseToEqual(pos, GetDefaultFireCentrePos, Get2DRadius × GetRadiusMultiplierForApplyingPotToPos)` (vasija 2,
    almacén/objeto 1,2); el primero que cumple toma lo que acepta (`AddResource(tipo, resto, IS, envenenado, &pos, 0)`,
    vt 0x9C), no el más cercano;
  - lo que sobra, si pos no es agua, hace una pila nueva (fn_005FA8B0) con el sonido de pila fn_0066D1A0 (< 200:
    `77 + t % 6` / `92 + t % 6`; si no `75 + (t & 1)` / `86 + t % 6`), `SetPoisoned` y `SetSpeedUp`. La voz de guía
    `GGuidance::ResourceDropSFX` no está portada.
  - **R16, el radio 2D de la pila:** `Object::Get2DRadius` 0x638180 = escala × max(+0x24, +0x2C) de la malla, que
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 llena con la media extensión (max − min)/2 de todas las submallas;
    `PileFood::Get2DRadius` 0x66F180 lo multiplica por `GetProportionRaised` (vt 0x86C). En Land1: MagicFood de 200
    → 1,95 m (acepta a 3,9 m), crece con la pila (acepta a 4,14 m con 218); MagicWood de 500 → 2,04 m (acepta a 4,07 m).
- **Pilas** (`Magic/Objects`): MagicFood 0x5FA9F0 (PotInfo 10, MSH_S_GRAIN_PILE, escala 0,3, dueño +0xBC) y MagicWood
  0x600E20 (PotInfo 9, MSH_B_WOOD_01, escala 0,7, dueño +0xB4); sin Process ni caducidad. Los vt 0x78(0)/0x80(0) de
  `MagicFood::CallVirtualFunctionsForCreation` 0x5FAAB0 no están portados (UNVERIFIED qué son).
- **Efecto SF_Food / SF_Wood** (`PSys/Rules/Sprinkle.cpp`, `PSys/Creators/Mesh.cpp`):
  - `UR_HandSprinkle` 0x6A0220: un átomo fuente en la posición del gesto (como mucho 58 m sobre el suelo); en el primer
    paso, si lanza la interfaz de este ordenador, arranca `HandStateGrain` (fn_005B2F70). Velocidad
    (0, Δy/dt − 20 si lanza un humano, 0).
  - Los granos los emite `UR_WillowWisp` con el hechizo *habilitado*: max(dt × MaxAtoms / DieAge,
    movido / 2,5 m) por paso, o sea **18 por segundo aunque la mano esté quieta** (el plan decía que una mano quieta no
    suelta nada: no es así). Cada grano que toca tierra (`LandscapeCollide SendEvent 1`) es un `SpellEvent 3`.
  - `AppearanceRuleTumble` 0x6A6200 (troncos) y `ParticleMeshCreator` 0x6A8B00 / `…AnimTextured` 0x6A8DA0 (troncos
    MSH_I_OFFERING_WOOD, cono de lluvia): los átomos de malla se dibujan como instancias con los objetos.
  - Arreglo del lector de ficheros de hechizo: las ARRAY pueden llevar flotantes (`KeyPoints` de UR_HandSprinkle); antes
    la lectura entera se paraba allí y SF_Food, SF_Wood (y cualquier fichero así) no cargaban.
- **HandStateGrain** (`HandGrain.cpp`, estado 8 de la mano): `fn_005B3000` guarda (ClampHand, TotalTime, altura,
  ángulo, bucle) y la posición de la mano (+0x1EC); `fn_005B2DA0` en cada turno: t += dt / TotalTime, spline cúbica
  natural de fn_005B3760 por (0,0) (0,2,1) (0,8,1) (1,0) (el ctor pasa 1e30 en los dos extremos porque +0x148 = 1,
  0x5B2CF2; el pico vale 1,61); altura = v × 10 m, inclinación =
  v × 1,07 rad (pico a t = 2 s: 16,1 m y 1,73 rad). Con ClampHand (comida y madera) `HandStateHolding::Update`
  0x5B3FD4 (vt 0x1C) pone la
  posición requerida de la mano en el punto donde empezó, y suma la altura (vt 0x18). El sentido del giro de la
  inclinación es UNVERIFIED. `Spell::CoreCloseDown` 0x720160 la para.
  - **Con ClampHand la mano no se mueve mientras cae el grano**, así que todos los granos caen en el mismo punto y
    hacen una sola pila que crece (el plan esperaba una línea de 20 m: no la hay con `ClampHand 1`, y el agua, que
    tiene `ClampHand 0`, sí sigue a la mano). El punto es el que tenía la mano al empezar el chorro, y el bucle no
    lo vuelve a tomar.
- **Pruebas:** `test_food_wood` (spline, bucle, costes, sonidos, `GetProportionRaised`, la emisión del sprinkle y la
  lectura de KeyPoints; con `OPENBLACK_GAME_PATH` las filas reales). Capturas en `dev\_audit\magic\`:
  `m3_food_pile.png` (`OPENBLACK_TEST_SPELL=FOOD,1808,2626`: la columna de grano y la pila; el registro da 200 → pila
  nueva, luego +18 por grano, 1400 y 126 cánticos), `m3_wood_pile.png` (WOOD: troncos que caen girando sobre una
  MSH_B_WOOD_01 de 500 + 20·n), `m3_food_store.log` (junto al almacén: todo entra en el almacén), `m3_foodpu_pile.png`
  (FOOD_PU1: 200 y luego +20 (140 cánticos), la pila nueva con SetSpeedUp y su visual 46 SF_SparklesFromObject) y,
  con la mano (`OPENBLACK_TEST_SEED=FOOD` + `OPENBLACK_TEST_CAST=press@2,release@20` de la lane M2),
  `m3_hand_raise_730.png` (la mano alzada y volcada, el chorro de grano y la pila junto al almacén) con
  `m3_hand_raise.log`, donde `Grain trace` da el pico 16,14 m / 1,731 rad a t = 0,50 de los 4 s.

## Fuego, bola de fuego y rayo (M5, `src/ECS/Fire`, `Magic/Objects/MagicFireBall`, `PSys/Rules/{Fireball,Lightning}`)

Informes: `destructive.md` §2-4 y §7, `visuals_sound.md` §4.1-4.2, `psys/part_render.md` §8 y §10. Todo lo de abajo está
leído en el exe (W120) salvo lo marcado UNVERIFIED o «(inf)».

### El modelo de calor (`FireEffect`, `SpreadEffect.cpp` 0x72E940-0x7310F0)

Cada objeto más caliente que el aire lleva un `FireEffect` (0x50 bytes, tipo de guardado 0x29). Hay una lista global, la
más nueva primero, y `FireEffect::ProcessList` 0x730760 la recorre una vez por turno (0,1 s; hueco 6 de
`GGame::ProcessTurn`).

- **Valores del objeto** (`GObjectInfo` +0xB0 heatCapacity, +0xB4 combustionTemperature, +0x80.. multiplicadores de
  defensa; `FireObjectTraits.cpp`): `Tc = max(combustionTemperature, 40)` (fn_00730180), `Tmax = 2·Tc` (fn_007301B0),
  capacidad `max(heatCapacity, 1)` (fn_007301D0), ambiente `MapCoords::GetTemperature` 0x605CC0 = **24,7 en todo el
  mapa** (fld 0x930080).
- **Arde** cuando `T >= Tc` (`IsOnFire` 0x730360); reacciona con `T >= 100` o `T >= Tc`
  (`IsAboveReactionTemperature` 0x730380). La fracción de fuego (0x7303E0) es `(T - 0,8·Tc)/(2·Tc - 0,8·Tc)` limitada a
  `2·vida` y a 0..1; el radio del fuego es `1,25 ·` el radio del objeto `·` la fracción (0x72FF10) y la altura de la
  llama `1,25 · altura · (T - Tamb)/(2·Tc - Tamb)` (fn_0072FF70).
- **Por turno** (`fn_0072F5B0`, dentro de ProcessList):
  - en agua enfría 50 veces más rápido; con lluvia o nieve el multiplicador es `rainCoolingMultiplier·lluvia + 1`;
  - si le dieron calor este turno: `T += 0,1·T/(2·Tc)` con techo `2·Tc`;
  - si no: `T -= (T + 10 - Tamb)·(4·altura·radio)·0,1·multiplicador/capacidad` (el «área» 4·H·r);
  - ardiendo: daño `(T - Tc)/(2·Tc - Tc) · defenceMultiplierBurn · 0,1` a la vida (0x72EEC0), y **carbonizado** +0,04 por
    turno mientras la vida < 0,6, con tope `(0,6 - vida)/0,6`; al enfriarse baja 0,02 por turno;
  - al morir: `DestroyedByEffect` (una criatura no se destruye);
  - **propagación**: espiral de celdas de 10 m mientras la celda esté a `radio del fuego + 10 m` del centro del fuego;
    cada objeto de esas celdas recibe calor (`fn_0072F980`). El viento (fn_00771B10) **se calcula y se descarta**: no
    mueve la búsqueda. Un objeto en la mano solo propaga dentro de la influencia de quien lo sostiene (0x730860);
  - la reacción `REACT_TO_FIRE` (10) se crea al pasar la temperatura de reacción y se borra al bajar; en la mano es
    `REACT_TO_BURNING_OBJECT_IN_HAND` (33), `FireEffect::StartedMoving` 0x730A60;
  - **grupos**: cada fuego nace raíz de su grupo (+0x40 raíz, +0x44 siguiente); `AddToMyFireGroup` 0x72FBE0 encadena el
    fuego nuevo justo detrás del que lo encendió, y una raíz que deja de arder pasa el grupo al primer miembro que arde.
    La lista de bomberos (+0x48) la guarda solo la raíz.
- **Sonido** (`FireSound.cpp`): solo los **2** fuegos más cercanos a la cámara con fracción > 0,1 suenan (tabla de 2
  ranuras 0xDA09CC), un `G_Fire` en bucle sobre el objeto.
- **Visual** (`FireGraphic.cpp`, `PSysBase` 0xD0, fn_00731160/1560/2200): llamas `S_Fire.raw` en modo 13 naranja
  0xFF713C con celda `int(fmod(-25·edad, 32) + 32)`, vapor blanco aditivo y humo gris `S_SpriteSheet3` (celda
  `int(fmod(25·edad, 32))`) en rachas de 30 turnos; tinte del árbol ardiendo (fn_0074B3A0), gris del carbonizado
  (fn_00730570, `255 - int(c·255)·175/256`, 80 al carbonizarse del todo) y brillo `GetFireEffectCharingColor` 0x730480.
  Falta el mapa de luz `S_LMFireBall` del objeto ardiendo (bit 4).
- **Aldeanos** (`VillagerFire.cpp`, `VillagerFireman.cpp` 0x75A3D0-0x75B460 y `ReactToFire` 0x765870): estados 215
  `REACT_TO_FIRE`, 216 `PUT_OUT_FIRE_BY_BEATING`, 219 `ON_FIRE` y 220 `MOVE_AROUND_FIRE`. Los de agua (217, 218) **en
  W120 se rinden en el acto** (`DECIDE_WHAT_TO_DO`), así que nadie acarrea agua. Un aldeano que apaga no recibe calor
  (fn_0072F980). `SetupOnFire` 0x75B170 guarda el estado y el destino anteriores y pasa a `ON_FIRE` con el fuego que lo
  calienta. **R10** (la decisión de 0x765870) queda leída en `ReactToFire`: el aldeano busca el fuego del grupo más
  cercano a él que esté por encima de la temperatura de reacción (`fn_00730070`).

### Bola de fuego (MAGIC_TYPE 1-3, semilla 2 FIRE)

- **`MagicFireBall`** (`Magic/Objects/MagicFireBall.cpp`, 0x682970-0x683390): un Object invisible **que no está en las
  celdas del mapa** (por eso `IS_FIRE_NEAR` no lo encuentra), creado por la regla `AttatchFireBallToAtom` y pegado a su
  átomo. Su `FireEffect` empieza en `initialTemperature · fuerza` del efecto y es lo que quema; la fila de
  `GMagicFireBallInfo` (3 filas, 0xD4E3B0) la elige el nivel de mejora. Se borra al bajar de `deletionTemperature` (el
  átomo queda marcado como desviado) o cuando su átomo desaparece.
  - **Coger**: `ValidForPlaceInHand` 0x682DD0 acepta cualquiera que no sea del propio jugador; `fn_00682EA0` crea en la
    mano una semilla FIRE nueva **ya lista** (`fn_00729900(0)`) y cargada, y la bola se va.
  - **Absorber**: `DeleteAndPutIntoSpellSeed` 0x682E10 multiplica el poder de la semilla FIRE sostenida por
    `1 + catchIncreaseFactor`.
- **Reglas del PSys** (`PSys/Rules/Fireball.cpp`): `UpdateRuleGravityWithFloor` 0x6A1880 (**R4**: el rebote usa
  `DampingHorozontalBounce` / `DampingVerticalBounce`, el arrastre del suelo y el sonido de impacto por clase de
  tamaño), `CreateWithInitialDirection` 0x69E950, `AttatchFireBallToAtom` 0x682FD0, `EventAlways`,
  `SetAtomHasBeenDeflected` 0x6A26C0, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`,
  `UR_Trail`, y las condiciones de desviación, de agua cercana y de vapor.

### Rayo (MAGIC_TYPE 4-6, semilla 6 LIGHTNING_BOLT; `PSys/Rules/Lightning.cpp`)

`UR_Lightning` 0x6914C0 es una regla de creación que mantiene una estructura de horquillas mientras se canaliza. Por
colección guarda un `UR_Lightning_CollectionData` (0xC0 bytes; aquí una entrada con clave en la ranura de la regla, como
las bolas de fuego, para que nada quede colgando).

- **Origen y dirección.** Con `CastingFromHand` el origen es la posición del gesto (`GetCurrentGesturePosn` 0x673600 =
  `PSysProcessInfo` +0x0C); si no, el átomo padre. El rumbo del cono es **fn_00673650 = `atan2(cameraForward.z,
  cameraForward.x)`**, es decir el frente de la cámara (el informe decía «el rumbo de la mano»; verificado ahora: el
  `PSysProcessInfo` empieza en manager+0x24, así que manager+0x3C/+0x44 son `cameraForward.x/z`).
- **Objetivos** (`fn_00690F50`, tres modos):
  - desde la mano (`fn_006901E0`): espiral de `4·ceil(R/10)²` celdas de 10 m desde el origen con R = `SearchRadius` (60
    en el rayo base); vale el objeto disponible que no sea semilla de hechizo (fn_00690090) y cuya dirección horizontal
    cumpla `dot(normalize(dx,0,dz), (cos rumbo, sin rumbo)) > cos(SplitAngle)` — un cono de semiángulo 0,2 rad ≈ 11,5°.
    Para hasta `MaxLightningObjects`;
  - si salen menos de `MinLightningObjects`, rellena con **puntos de suelo**: ángulo `rumbo + rand(pi/4)` (verificado: es
    `+rand(0, pi/4)`, no centrado; la constante es 0x3F490FDB), distancia `rand(0,6·R)`, y = terreno + 2 (0x8AB478);
  - `TakeTargetsFromManager` toma los `SpellTargets`; el modo del padre (fn_00690880) usa `DefaultSearchRadius` 20.
  - Una criatura se salta cuando `creature+0x12B0 < 0,5` (fn_0047ACC0, UNVERIFIED qué significa): no portado.
- **Estructura** (`CreateForkStructure` 0x691190): borra los átomos de la colección, crea **un** átomo y le cuelga
  `2 · nº de objetivos` subcolecciones del `ForkGroup`, cada una con `MaxJointsPerFork` átomos de
  `ParticleChainCreator`. Con `CastingFromHand` cada átomo recibe un `DrawOffsetLT` (0x69131C; no portado).
- **Cada paso** (`UpdateForkStructure` 0x691BB0):
  - la escala de las horquillas es `ForkScale · FP_ForkScale`;
  - los enfriamientos (+0x18) de cada objetivo bajan de uno en uno;
  - si los objetivos no se han renovado, elige cada objetivo con probabilidad `min(AtOnce, (N+1)/2) / N` y como mucho
    esos; en el modo de padre o de manager elige **uno** con una rotación aleatoria de la lista;
  - los objetivos se rebuscan cuando el origen se ha movido más de `RenewTargetsOnMoveFrac · SearchRadius` o cada
    `RenewSearchEvery` s (fn_006916B0 / fn_00691390).
- **Geometría de la horquilla** (el bucle de `fn_00691F30` en 0x69244D): las articulaciones van del origen a la punta,
  las interiores desplazadas `±RandomFrac · longitud` **solo en Y y Z** (dos `PSysFloatRand`, la X no se desplaza), la
  escala interpola con `ForkScale` y el alfa es 255 en la horquilla que llega a un objetivo y `128 + rand(127)` en las
  demás (el parpadeo). La recursión del original construye un árbol de horquillas con desvíos de `SplitAngle`; aquí cada
  objetivo recibe **dos** horquillas, la principal hasta la punta y una que se desvía hacia un punto a
  `tan(SplitAngle) · distancia` (**simplificación**: el árbol recursivo completo y el suavizado por puntos medios de
  `fn_0067B3F0` no están portados).
- **La punta** (0x692941): enfriamiento del objetivo `rand(AverageLightmapLife / dt)` pasos; **fn_00691E80** crea un
  átomo de `LightMapGroup` con `PCreatorLightMapAtom` en la punta (el informe se lo atribuía a fn_00691ED0: no, esa es
  el aviso de «impresionante», `GetImpressiveIntensity` -> fn_00692FA0, y solo se lanza una vez cuando la colección pasa
  de 0,2 s); `SpellEventInfo` tipo 3 (`Landed`) en la punta con fuerza 2 cuando el rayo sale de un objeto padre y 1
  desde la mano. La posición de la punta es `fn_00691E00`: el objeto en su celda de mapa, altura `GetAltitude + (+0x1c)
  + GetHeight`; un punto de suelo, él mismo.
  - Sin hechizo (rayo de guion o de clima) el original aplica los `EffectValues` estáticos de la info 0xCC9704 en la
    punta: **UNVERIFIED** qué fila de `GEffectInfo` es; no portado.
  - El escudo (fn_006D0BC0 con radio 2,5) que cubra un tramo corta la horquilla y recibe un evento tipo 4: **M6**.
- `UR_LightningStrike` 0x6937A0 (SF_LightningStrike / SF_LightningSingleStrike, el rayo de guion y de clima) solo crea un
  átomo con sus `NextGroups` —donde vive el `UR_Lightning` del golpe— y su `SOUND_SPELL_LIGHTNING`, una vez.
- **Números por evento** (la tabla de `destructive.md` §4.2, confirmados en el registro): PU0 burn 800, crush 0,0006,
  hit 0,0017, radio 1 m; PU1 1400/0,0011/0,003; PU2 2000/0,0018/0,0075.
- No portado aún: `LightningForkFlicker` 0x6B24D0 (`FlickerFreq`, sin leer) y `NumTexturesToTile`.

### Cadenas y mapas de luz (`PSys/Creators/{Chain,LightMap}.cpp`, `Graphics/RendererChain.cpp`)

- **`ParticleChainCreator`** 0x6AA900: los átomos de una colección son las articulaciones de **una** cinta. El dibujo
  por colección (fn_0067B3F0) la orienta a la cámara: en cada articulación el vector lateral es
  `normalize(cross(dirección del tramo, dirección a la cámara)) · escala`, y la U recorre la cadena repitiendo la
  textura `NumTexturesForWholeChain` veces (`S_Lightning.raw`, 4 veces en el rayo). Para esto el `Effect` tiene un
  `CollectChains` nuevo que devuelve las colecciones de tipo cadena con sus articulaciones **en orden** (el `Collect`
  normal las aplana). Se dibuja después de los sprites ordenados, en `MainBlended`.
  - No portado: `UseDynamicLighting` (color × `clamp(0,6 + 0,4·(n·L))`), el desplazamiento vertical de la UV con el
    tiempo y el suavizado por puntos medios.
  - `OPENBLACK_PSYS_CHAIN_TRACE=1` escribe por fotograma cuántas cintas hay, con cuántas articulaciones, su textura y de
    dónde a dónde van: sirve para separar «no se dibuja» de «no hay ninguna en ese fotograma».
- **`ParticleLightMapCreator`** 0x6A9D80: `GJBitmap::LoadBitmapFromFile(nombre, Pitch, 3, NumFramesInFile,
  NumFramesInUse)` son fotogramas cuadrados de `Pitch × Pitch` apilados, RGB (3 B/px) o grises (1 B/px) — el del rayo,
  `S_lightning_lightmap_with_border.raw`, son 1200 B = 5·5·16·3. El original los mete en la lista 0xD4EDB8 y
  `PSysLightMaps::AddDrawing` 0x6CA6E0 los **estampa** (fn_0086CFF0) en la textura de luz dinámica del terreno.
  - **Desviación (no es un mod, es una aproximación de dibujado):** el port no tiene esa textura dinámica, así que cada
    fotograma se escala a 32×32 en un atlas de 8×8 y el átomo se dibuja como un cuadrado aditivo plano **sobre el
    suelo** (`SetHorozontal`), con la altura del terreno bajo la punta. La luz no sigue la pendiente ni tiñe los objetos
    que están encima. `ShiftX`/`ShiftZ` (≈10, que compensan el +10 del estampado) no se usan.
- Arreglo de paso: `TextureBaseName` resuelve el nombre del `TextureFileName` con **las mayúsculas que el fichero tiene
  en `Data\Textures`** (los ficheros de hechizo escriben `S_Lightning.raw` y el fichero es `S_lightning.raw`), así que
  ahora encuentran su textura también los sprites que antes no se dibujaban.

### Natives CHL (`Magic/Script/CHLFire.cpp`)

170 `IS_ON_FIRE` 0x6FB4C0, 171 `IS_FIRE_NEAR` 0x6F7910 (`FindNearForScript` con el predicado 0x6F7100; una bola de fuego
no está en las celdas, así que no la encuentra), 174 `SET_TEMPERATURE` 0x6FB840, 175 `SET_ON_FIRE` 0x6FB780, 321
`SET_HURT_BY_FIRE` 0x6FDF40 y 426 `SET_SET_ON_FIRE` 0x6FDEE0 (los dos últimos, bits 2 y 3 de Object +0x0A).

### Pruebas y capturas

- `test_lightning`: las clases registradas, las propiedades de `ParticleChainCreator` y `ParticleLightMapCreator`, la UV
  por tramo y, con `OPENBLACK_GAME_PATH`, los ficheros reales `SF_LightningBolt`, `SF_LightningBoltPUTwo` y
  `SF_LightningStrike` (grupos, contadores, `SplitAngle`, `ForkScale`, el tamaño del `.raw` del mapa de luz).
- Capturas en `dev\_audit\magic\`:
  - `m5_tree_fire.png` (`OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"`): el árbol ardiendo, carbonizado y con
    llamas, y aldeanos alrededor; el registro muestra la propagación al aldeano 30, que huye en estado 219 y muere, y de
    ahí al objeto 52. `m5_gfx.log` tiene la traza del gráfico (2 llamas vivas de las 2 que permite el árbol, escala 0,25).
  - `m5_bolt.png` (`OPENBLACK_TEST_SPELL="LIGHTNING_BOLT,1818.6,2628.4,10,300"`): dos horquillas con su cinta
    `S_lightning` desde el origen (a 30 m, que es de donde lanza `SPELL_AT_POS`) hasta las puntas, con los brillos del
    `CommonGlowGroup` y un árbol alcanzado ardiendo. El registro da `Lightning: 3 targets (3 objects), 2 of 8 forks
    struck ... heading 0.00 rad, fork scale 2.00`, `PSys chains: 2 ribbons ... 10 joints` y los eventos tipo 3 con burn
    800 / crush 0,0006 / hit 0,0017 y radio 1. El rayo **parpadea**: una horquilla solo se dibuja el turno en que su
    objetivo es golpeado, y cada objetivo queda en enfriamiento `rand(AverageLightmapLife/dt)` turnos, así que muchos
    fotogramas no tienen ninguna.
  - `m5_fireball.png` (`OPENBLACK_TEST_SPELL="1,1826.8,2641.4,10,300"`, MAGIC_TYPE 1): las bolas caen sobre el pueblo;
    el registro muestra el fuego de la propia `MagicFireBall` (Tc 2000, capacidad 18,8) y luego los fuegos nuevos del
    granero (objeto 52, Tc 180, capacidad 4000), de la casa 29 y de los aldeanos 30, 44 y 49, que huyen en estado 219.

## Culto: de dónde salen los milagros (M7, `src/Worship`, `ECS/Systems/Implementations/VillagerWorship`)

Investigación: `dev\tmp_dis\miracles\sources.md` (§1-§8). La cadena del original es: una **ciudad** guarda tipos de
magia → su **centro del pueblo** enseña un icono por semilla → la **ciudadela** del jugador tiene un **lugar de culto**
por tribu, con un icono por semilla → los **aldeanos** bailan allí y llenan su **batería** de poder de oración → al
tocar un icono este se **carga** y la semilla aparece en la mano. Aparte están los **dispensadores** de milagros de un
uso y las **luciérnagas**.

### Estructura y datos

- `GWorshipSiteInfo[9]`, uno por tribu (`GTribeInfo.worshipSiteInfo`): `chantsPerVillager` 3 (celta 4, tibetano 5),
  `maxDancersVisible` 20, `chantsToFillBattery` 9000, `eachVillagerAddToFillBattery` 300, `prayerSiteDistance` 44,
  `radiusFromCitadel` 37.5, `artifactPowerupMultiplier` 1e-5, y la malla del altar por tribu (101
  `BuildingCitadelNorseAltar`, 93 indio, 94 azteca, 95 celta, 98 africano/egipcio, 99 griego, 100 japonés, 103
  tibetano).
  - **Fallo conservado:** `chantsToReserveForMaintaining` está en el archivo como el entero **500** y el ejecutable lo
    lee con `fld` (`fn_0077A950`), así que vale ~7e-43 ≈ 0: la reserva para mantener hechizos no existe en la práctica.
- `GSpellIconInfo[2]`: [0] "Spell Icon" (lugar de culto), [1] "TownSpell Icon" (centro del pueblo). Las dos usan la
  malla **203** `BuildingVillageCentreSpellHand` y `gatheringChantAddPerGameTurn` 61.
- **Puntos especiales** (las métricas extra del L3D; `Game3DObject::GetSpecialPos` 0x63B040 / 0x63B0B0 = la matriz de
  la métrica por la del objeto; `src/Worship/SpecialPoints.cpp`):
  - la malla `b_worship.l3d` del lugar de culto tiene **16**: 7 escondite, 8 centro del baile y tótem, 9 llegada,
    **10..15 los seis huecos de icono**;
  - la malla del centro del pueblo (p. ej. 179 `BuildingNorseVillageCentre`) tiene **14**: **0..5 los seis huecos de
    icono** (todos a y = 2,781, en corro) y 6 el tótem (y = 4,613, en el centro);
  - la malla 203 del icono tiene 1: el punto donde flota el `SpellSeedGraphic` (+1 en y).
- Coste de carga = `GMagicEffectInfo.costToCreate` (FUEGO 3500 / PU1 7000 / PU2 10000, RAYO 5000/7500/10000,
  CURAR 6000/9000, COMIDA 7000/10000, MADERA 7000, AGUA 5000/7000, NATURALEZA 13000, RAYO EN HAZ 16000/32000/60000...).

### La ciudadela y sus seis huecos (`Worship/Citadel.cpp`)

`CitadelWorship` va en la entidad del templo (`components::Temple`), que openblack crea en `CitadelArchetype`. Seis
huecos (`sites[6]`); el ángulo del hueco *n* es **el ángulo del corazón + n × 2π/7** (`Citadel::GetWorshipSiteAngle`
0x463610) y el lugar se coloca a `radiusFromCitadel` del origen de la ciudadela.

- `Citadel::AddTown` 0x463130 → `FindOrCreateWorshipSite` 0x4631D0 / 0x463220 → `FindTribeWorshipSite` 0x463190 o
  `RequestANewWorshipSite` 0x4633F0 (el hueco libre más cercano a la ciudad más próxima de esa tribu, si no a la
  ciudadela).
- `CitadelHeart::CreateBuiltWorshipSite` 0x465110 es el `CREATE_WORSHIP_SITE` del guion: crea el lugar de esa tribu
  **sin comprobar la ciudad** y le añade las ciudades del jugador de esa tribu. La posición y el número de sitio que
  trae el guion **no se usan**.
- `GPlayer::PostLoadCleanup` 0x64AB90 (justo después del guion de la tierra; en openblack, en el primer turno): por
  cada jugador con ciudadela, cada una de sus ciudades sin lugar de culto → `Citadel::AddTown`.
- `Town::IsAllowedToCreateWorshipSite` 0x740BB0: **nunca en la tierra 1**, ni si el guion lo prohíbe
  (`SET_CAN_BUILD_WORSHIPSITE`), ni sin población. Por eso en Land1 solo hay dispensadores y luciérnagas.

### La batería y el turno del lugar (`Worship/WorshipSite.cpp`)

`WorshipSite::ProcessSpellIcons` 0x77B4D0, una vez por turno desde `Citadel::ProcessSpellIcons` 0x463920 (que sale de
`GPlayer::ProcessSpellIcons` 0x64AEE0, dentro de `Spell::ProcessSpells`):

1. **Tensión** (+0x114) = `(pedido − capacidad) / capacidad`, con capacidad = `N × chantsPerVillager × poder tribal[2]`
   (`fn_0077E060`). Sin capacidad, 1 si se pidió algo y 0 si no.
2. Si la tensión **no** es positiva, los iconos que se están cargando se reparten lo que sobra:
   `min(disponible, necesitado) / cuántos` a cada uno (`fn_0077CBC0` resta la reserva de mantenimiento, ~0 por el fallo
   de arriba). Lo que cada icono acepta se cobra al lugar.
3. `WorshipSpellIcon::Process` de cada icono.
4. **Fin de turno** `fn_0077B6A0`: `k = min(1, usado/capacidad + empuje)` con
   `empuje = max(0,2; 0,5 − batería/máximo × 0,5)` (0 si sale ≤ 0); producido = `capacidad × k`;
   `chantDamage` = producido / N (lo que cuesta de vida a cada bailarín); `batería -= usado − producido` (nunca menos
   de 0); `disponible = batería + capacidad`. Esa `k` es también la intensidad del baile (`fn_0077B8D0` →
   `fn_0050C340`).
   - Cada 1000 turnos los artefactos del lugar darían un extra; openblack no tiene artefactos (informe R12).
- `UseChants` 0x77BBB0 apunta lo pedido, cobra como mucho lo disponible y suma a la estadística del jugador.
  `MaintainSpell` 0x77BC50 y `fn_0077CC50` son las variantes de los trucos (cánticos infinitos, mantenimiento gratis).
- `MaxBattery` = `chantsToFillBattery + N × eachVillagerAddToFillBattery` (9000 sin bailarines).
- **Tensión visual** `fn_0077B3B0` (por fotograma): `fase = fmod(fase + (5 + 5·clamp(tensión,0,1))·dt, 2π)`,
  `pulso = (cos fase + 1)/2`.
- El **baile** real sale de su `.DAN` (`GDanceInfo[19 + hueco]`, `GroupBehaviour::CalculateDancePosition` 0x597F20). No
  está portado: los bailarines se reparten en un anillo de 6 m alrededor del punto 8, a 256/N cada uno (la parte de
  anillo de esa función). **UNVERIFIED**: la forma exacta del baile.

### Los iconos y la carga (`Worship/WorshipSpellIcon.cpp`, `Worship/TownCentreSpellIcon.cpp`)

- `WorshipSpellIcon::Create` 0x77F2B0 pone la malla 203 en el hueco 10..15 con la escala y el ángulo del lugar, y su
  `SpellSeedGraphic` encima (`SpellIcon::Create3DSpellObject` 0x726210). `UpdateGraphicsWithPULevels` 0x77F320 muestra
  el nivel de mejora más alto que el jugador tiene habilitado, con alfa 0,5.
- `TownCentre::AddSpell` 0x744050 crea un icono por semilla en el primer hueco libre 0..5 del centro del pueblo;
  `TownCentre::MakeFunctional` 0x743E80 lo hace para toda la magia que la ciudad ya tenía y luego llama a
  `WorshipSite::AddTownSpells`. Cada icono del pueblo pide al lugar de culto un icono de su semilla
  (`fn_0073D1C0` → `WorshipSite::AddSpellIconIfNecessary` 0x77C9E0); al quitarlo, el del lugar solo desaparece si
  ninguna otra ciudad del lugar tiene esa semilla (`fn_0077CAA0`).
- **Tocar** (`SpellIcon::InterfaceTap` 0x726430 → `WorshipSpellIcon::ActualInterfaceTap` 0x77F880): si ya está lleno, la
  semilla a la mano; si se está cargando, se cancela; si no, empieza a cargarse. Un icono del centro del pueblo reenvía
  el toque al icono del lugar de culto de su misma semilla (`TownSpellIcon::GetWorshipSpellIcon` 0x748F30). El sonido
  del toque es `G_ClickOnSpell_01` a un tono de {100, 115, 130, 145, 155, 175} % según el hueco (`fn_00726490`).
- **Carga** `StartCharge` 0x77FA00 / `ValidForStartCharge` 0x77FAB0 / `fn_0077FB40` (paquete 0x25). Al llenarse
  (`GetChantNeeded` ≤ 0): si ya hay semilla en la mano se le sube el nivel de mejora; si no,
  `PutFullyChargedPowerUpSeedInHand` 0x77F8F0 la pone en la mano **ya lista** (`fn_00729900(1)`, la corrección de M1) y
  suena la voz del milagro (`PlayFullyChargedSoundFX` 0x77F4E0, banco `SpellDialogue.sad`).
- **Fallo conservado de `AddToChantStore` 0x77FDA0:** por debajo del requisito devuelve lo que ha metido; por encima
  deja el almacén en el requisito y devuelve el **exceso** `x − (requisito − almacén)`, y es ese exceso lo que se le
  cobra al lugar de culto.
- Devolver la semilla: `CancelCharge` 0x77F9A0 y `ReturnAllChantsToWorshipSite` 0x77FD60 devuelven el almacén a la
  batería; `SpellSeed::ApplyToWorshipSite` 0x7289C0 / 0x728B30 / 0x729A80 devuelve los cánticos de la semilla al lugar
  de su icono (soltarla en el suelo del lugar, dársela al tótem o a un icono, o sacudirla de la mano). Si se le da a un
  icono **de otra semilla** del mismo jugador, ese icono entrega su semilla cargada (el intercambio). Un dispensador,
  un `WorshipTotem` y cualquier icono son "puntos de devolución" (`IsSpellSeedReturnPoint`), así que
  `SpellSeed::CanCast(objeto)` 0x729190 les deja dar la semilla aunque la magia no se pueda lanzar sobre objetos.
- Con la marca de partida 0x2000 (`OPENBLACK_INFLUENCE_EVERYWHERE`) los iconos neutrales se cargan solos a
  `gatheringChantAddPerGameTurn` (61) por turno.
- El **anillo de carga** (malla 561 `MSH_S_PULSE_IN`, `TChargingData::Draw` 0x7267A0) usa la fracción
  `almacén/requisito` (1 con semilla en la mano), mostrada como `(f+0,2)/1,2`, y al llenarse pulsa con
  `alfa = 255·(0,1 + 0,5·(sin(4π t)+1)/2)`.

### El porcentaje de culto y los aldeanos (`Worship/WorshipPercentage.cpp`, `VillagerWorship.cpp`)

- `Town::SetWorshipPercentage` 0x73C060 (arrastrar el tótem, `TotemStatue::NetworkUnfriendlyLockedSelect` 0x7386A0:
  `pct = clamp(pct + dy × 0,1; 0; 1)`): 0 sin lugar de culto; si no, se guarda, se le pasa al tótem
  (`TotemStatue::SetWorshipPercentage` 0x738270, que lo sube 8 m con un *Zoomer* de 5,2 s) y se manda a los aldeanos
  que falten.
- `Town::GetWorshipersNeeded` 0x73C860: `objetivo = pct > 0 ? max(1; int(población × pct + 0,5)) : 0`;
  `resultado = objetivo − (adorando + en camino) + los que piden volver a casa`.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0: dos pasadas (la segunda acepta también los marcados 0x200); para
  mandar, los aldeanos disponibles **más lejos** del centro del baile primero
  (`fn_0073C590` = `GetDistanceModifier(distancia; distancia del centro a la ciudad + 100) × vida²`, que crece con la
  distancia); para retirar, los que están o van al lugar, los más lejanos primero (estado 163).
- Estados del aldeano (tabla de `LivingActionSystem.cpp`): **59** llega al lugar (0x76BE00; a 10 m del punto 9 entra al
  baile si `N < maxDancersVisible`, si no al escondite), **60** bailando (0x76C680), **213** escondido (0x76C5E0) y
  **248** vuelve a casa (0x761B70). Salidas `ExitMoveToWorshipSite` 0x76C170 y `ExitAtWorshipSite` 0x76C1F0. El 58 del
  original es la marcha por el camino (`SetupMoveToOnFootpath`); openblack camina con el WallHug dentro del 59, así que
  el 58 no se usa. `Villager::CheckNeededForWorship` 0x76BA60 entra desde `DECIDE_WHAT_TO_DO`.
  - **Ojo:** el `k_VillagerStateStrings` de openblack se equivoca en los índices 248..254 (dice `RESTART_MEETING`...);
    el enum `VillagerStates` sí coincide con el original y es el que indexa la tabla.
- `Villager::ProcessInWorship` 0x76C890 cada turno: `CheckVillagerGoBackToTownFromWorship` 0x76BEC0,
  `CheckRequestGoHome` 0x76C8D0 (con vida < `damageThresholdToGoHome` 0,3 se apunta en la cola, ordenada por el deseo
  de vida `GetLifeDesireFromLife` 0x75BBC0) y `ReduceVillagerLifeByChant` 0x76C800
  (`vida -= chantDamage × chantLifeRate`, 5e-6; al llegar a 0 muere con motivo 4 y lo cuenta
  `GET_TOWN_WORSHIP_DEATHS`).
- `Villager::CanIGetToTheWorshipSite` 0x76BC20: dentro de `maxDistanceThatVillagersWillGoToWorship` (500).
- Sin portar: comer en el lugar (estado 241, hace falta el estómago del aldeano) y llevar suministros (estados 42-46).

### Dispensadores y luciérnagas (`Worship/SpellDispenser.cpp`, `Worship/FireFlyReward.cpp`)

- `SpellDispenser` es un Abode con su magia y su periodo. `SpellDispenser::Process` 0x722A70: mientras su orbe siga
  existiendo y tocándolo, espera; si no, cada `periodo` turnos crea otro (`CreateOneOffSpellSeed` 0x722B80 →
  `OneOffSpellSeed::Create` en su posición + 1,2 × su altura, visual de sitio 9). El periodo por defecto es
  `timeEachMobileObjectTakesToProduce` = **300** turnos; `SET_MAGIC_PROPERTIES` 0x70CC30 y `SET_TIMER_TIME` 0x711280 lo
  cambian en segundos (× turnos por segundo) y un periodo 0 lo desactiva. Darle una semilla no lanzada lo convierte en
  un orbe allí, perdiendo sus cánticos (`fn_00728C50`, visual 0x1B).
- En **Land1** los dispensadores no salen del guion de la tierra, sino del guion del desafío
  (`GiveSpellDispenserReward`: `CREATE_WITH_ANGLE_AND_SCALE(SPELL_DISPENSER)`, `SET_MAGIC_PROPERTIES`, `SET_ACTIVE`,
  `SET_TIMER_TIME`). `CREATE_SPELL_DISPENSER` solo aparece en Land3, Land5 y los patios de recreo.
- **Luciérnagas** (`FireFly.cpp` 0x52B5A0..0x52B790): al coger con la mano un objeto sobre el que dormía una luciérnaga
  (`fn_0052B600`, desde `GInterface::PlaceObjectInMagicHand` 0x5DA6F0) se sortea un milagro de un uso con las
  probabilidades de `FIRE_FLY_SPELL_REWARD_PROB`, que **solo usa Land1.txt** (CURAR 20; FUEGO, RAYO, NATURALEZA,
  COMIDA, MADERA y AGUA 1 cada uno): `r = GameFloatRand(total)`, el primer milagro cuya suma acumulada llega a `r`, su
  primera semilla y nivel, y un orbe si esa semilla existe (`GSpellSeedInfo.exists`).

### Guion (`Magic/Script/CHLWorship.cpp`, la parte de culto de `MapScriptMagic.cpp`)

- Comandos del mapa: `CREATE_TOWN_SPELL` / `CREATE_TOWN_CENTRE_SPELL_ICON` (10 y 12, el mismo manejador),
  `CREATE_NEW_TOWN_SPELL` (11), `CREATE_SPELL_ICON` (13, no hace nada ni en el original),
  `CREATE_PLANNED_SPELL_ICON` (14, solo el tipo de magia de la ciudad), `CREATE_WORSHIP_SITE` (19),
  `FIRE_FLY_SPELL_REWARD_PROB` (88) y `CREATE_SPELL_DISPENSER` (90).
- Natives CHL: 330 `IS_SPELL_CHARGING` 0x70CB80, 331 `IS_THAT_SPELL_CHARGING` 0x70CBD0, 355 `GAME_SET_MANA` 0x6FE800,
  356 `SET_MAGIC_PROPERTIES` 0x70CC30, 376 `SET_CAN_BUILD_WORSHIPSITE` 0x6FEC40, 386 `SET_MAGIC_IN_OBJECT` 0x6FF0B0,
  410 `GET_TOWN_WORSHIP_DEATHS` 0x6FF640, 422 `GET_MANA` 0x6FE8C0, 423 `CLEAR_PLAYER_SPELL_CHARGING` 0x70CD80 y
  453 `GET_SPELL_ICON_IN_TEMPLE` 0x6F3590; más las ramas de dispensador de `SET_ACTIVE` (255) y `SET_TIMER_TIME` (145)
  y los tipos de `CREATE` 30 `ONE_SHOT_SPELL`, 31 `ONE_SHOT_SPELL_IN_HAND` y 36 `SPELL_DISPENSER` (`GScript`
  0x6F1010).

### Selección por gesto

Los iconos que ve el sistema de selección de M2 se registran con `gestures::SetIconProvider`
(`Worship/GestureIconProvider.cpp`): los del jugador de la interfaz, en el orden de las listas de sus seis lugares
(`GPlayer` 0x64BAB0..0x64BF40). `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 elige, entre los iconos válidos de esa
semilla, el del lugar con más cánticos disponibles.

### Capturas

En `dev\_audit\magic\` (Land2 con `-s Land2.txt`, Land1 con `-s Land1.txt`; los ganchos están en
[openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)):

- `m7_land2_site.png`: la ciudadela de PLAYER_TWO con sus dos lugares de culto (el norso de
  `CREATE_WORSHIP_SITE` en el hueco 5 y el griego que `PostLoadCleanup` añade por la ciudad 2 en el hueco 0), los dos en
  el origen de la ciudadela y girados a su hueco, con su caldero de comida y su altar.
- `m7_land2_icons.png`: los cuatro iconos de hechizo del lugar (FUEGO, NATURALEZA, COMIDA, MADERA de la ciudad 1) en los
  puntos 10..13 de la malla `b_worship`, con su `SpellSeedGraphic` y su efecto encima.
- `m7_land2_dance.png` (`OPENBLACK_TEST_WORSHIP="1,0.5"`): 11 de los 22 aldeanos de la ciudad 1 en el lugar de culto. El
  registro da `site 149 icons 4 N 11 C 33.0 k 0.223 strain -1.000 battery 6824 / 12300 available 6857 damage 0.67`:
  capacidad 11 × 3, máximo 9000 + 11 × 300 y el daño por bailarín exactamente como el original.
- `m7_land2_totem.png` (lo mismo): el tótem de la ciudad 1 subido sobre su plinto (8 × 0,5 = 4 m) y los iconos del centro
  del pueblo a su alrededor.
- `m7_land2_charge.png` (`OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE;..."`, `OPENBLACK_TEST_MANA=40`, `OPENBLACK_TEST_TAP_ICON="FIRE,5"`): el anillo de
  carga encendido sobre el icono de FUEGO mientras se llena a 40 cánticos por turno.
- `m7_land2_seed_hand.png` / su registro (con `OPENBLACK_TEST_MANA=20000`): con la batería llena el icono se llena en un
  turno y `Worship: seed 3093 of icon 3086 in the hand with 7000 chants` (COMIDA cuesta 7000).
- `m7_land1_dispenser.png` (`OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1826,2670,WOOD"`,
  `OPENBLACK_TEST_FIREFLY_REWARD="1846,2670,3"`): el dispensador de milagros de Land1 con su orbe de MADERA y tres
  recompensas de luciérnaga. El total de probabilidades es 26 (CURAR 20 + seis de 1), como en Land1.txt, y salieron
  CURAR, AGUA y COMIDA.

Fallo de dibujo conocido: la malla `b_worship` del lugar de culto (una `ContainsLandscapeFeature`) sale **negra**,
porque openblack no le pone la textura del terreno que usa. No es del sistema de culto.

### Diferencias con el original y lo que falta

- Los lugares de culto de openblack nacen **construidos**: no hay obras ni `BuildingSite`, así que
  `CREATE_PLANNED_WORSHIP_SITE` no hace nada y una ciudadela planeada recibe igual sus seis huecos.
- **El arrastre del tótem con la mano no está conectado** (`percentage::TotemTown` está listo para ello): el porcentaje
  se prueba con `OPENBLACK_TEST_WORSHIP`.
- No está portado: el baile real de los `.DAN`, los artefactos del lugar, el sprite del camino de maná
  (`CreateManaPathSprite` 0x77B2C0, del lado del lanzamiento), los suministros al lugar, los cofres de recompensa (M7b)
  y el robo de hechizos por la criatura (con M8).
- **Sin verificar:** el tipo de jugador 3 que no puede tener lugar de culto; el bit 0x200 del aldeano que la segunda
  pasada de `AdjustWorshipersWorshipping` acepta; `maxDistanceForVillagersToGoToTheWorshipsite` (1000) y
  `minLifeForVillagersToGoToTheWorshipsite` (0,4), que no se leen en las funciones portadas; el contador
  `WorshipSpellIcon +0x114` (nadie lo activa en el ejecutable).


## Lanzar desde la mano, gestos y efectos de la mano (M2, `src/Magic/Gestures`, `src/Magic/Hand`, `HandSpellSeed.cpp`)

Informes: `casting.md` (§2-5) y `visuals_sound.md` (§1.4, §4.13). Lo de abajo está leído en el exe; lo que no, se dice.

### Gestos: el búfer y el reconocedor (`GestureBuffer`, `GestureMatch`, `GestureTemplates`)

- **Entrada** (`GestureInput.cpp`): la muestra la da el ratón **sin botón**. Es el mensaje 0 de CMouse, cada 28 ms de
  eventos de ratón (`fn_005CEAD0`), con el punto del terreno bajo el cursor, o el de la última muestra si está fuera.
  No hay muestras en pausa ni durante los 0,4 s que siguen a un reconocimiento. Si la cámara cambió de posición en el
  fotograma (`GCamera::IsMoving`), **el búfer se borra** en cada `ProcessPowerUpSystem`.
- `GestureSystem::AddSample` 0x57BBC0: 80 muestras en anillo.
  - Una muestra quieta se compara con la de **dos mensajes antes** (0x57BC3A: head − 2, porque la cabeza aún no ha
    avanzado). 70 seguidas así borran el búfer, y esa muestra lo empieza de nuevo: es la 72.ª muestra quieta.
  - `ProcessNewSample` 0x57C3F0 busca las esquinas sobre la marcha:
    - esquina = giro ≥ π/8·¾ (`FindCorner` 0x57BFE0);
    - fusión o rechazo por longitud (`MergeOrReject` 0x57C200; `LongEnough` fn_0057C630: 12 px, o entre 4 y 12 si la
      caja reciente mide menos de 50 px);
    - rumbo y octante de la salida (`UpdateHeading` 0x57C710). El octante redondea un ,5 exacto hacia abajo
      (fn_00578700).
- `Gestures.jty` (`GestureSystemDataList::Load` 0x579AF0): 81 plantillas de 0x65C bytes.
- `MatchGesture` 0x579F10 → `Match` 0x57A050: primero `MatchForward` 0x57A1A0 y, si la plantilla lo permite,
  `MatchMirror` 0x57A3E0 (giros negados, error sin envolver). Solo se comparan tres cosas:
  - la secuencia de giros (un giro menor que T1 = 21π/128 se puede absorber; error máximo T2 = 3π/16);
  - la primera dirección;
  - la clase de aspecto (0,15 / 4, fn_00579FA0).
- Paquete (fn_0057A5E0), en modo 2 (el de todas las plantillas):
  - el punto es el del terreno bajo el centro de la caja de las muestras encajadas;
  - el tamaño es 1,05 × la media anchura de esa caja en el mundo, a esa distancia.

  Es el `size` del círculo de la tormenta y de los escudos (la magnitud del lanzamiento).
- Los gestos de milagro del jugador (selección SPIRAL) son **14**, no 12: FORK_DOWN, CYRILLIC_L, VERTICAL_SCRIBBLE,
  S_SHAPE, FORK_RIGHT, FORK_LEFT, FORK_UP, HEART, THREE, W_SHAPE, SQUARE_SPIRAL, INVERSE_SQUARE_SPIRAL, HOUSE y STAR.
  Cada uno reconoce su trazo y ninguno de los otros 13 (`test_gestures`, `realData`).

### Qué se busca y cuándo (`PowerUpSystem.cpp`, `GInterface::ProcessPowerUpSystem` 0x5CF300)

- Corre al final de cada `InterfaceActionProcess`: una vez por fotograma (`ProcessFrameInputs`) y otra por turno
  (`GInterface::Process`, con el tiempo del último fotograma). Así queda resuelta R6. Siguen sin encontrarse quién
  pone el bit 0x02 de m_Buttons y quién lee el tope de 40 s de la repetición.
- Orden de cada llamada:
  1. el borrado por la cámara, la espera de 0,4 s y la caducidad del círculo pendiente (5 s);
  2. **el círculo**: con la acción pulsada (m_Buttons 0x200) y una semilla con `sizingGesture` (CIRCLE: tormenta,
     escudo y escudo físico), el círculo guarda posición y tamaño;
  3. con una semilla **de icono** que carga (`HoldingChargingSeed` fn_005CEF50), los gestos de power-up de la semilla
     (fn_005D0000); si ya tiene un power-up, SCRIBBLE lo quita (paquete 0x6A). Si no hay tal semilla, la etapa de la
     selección abierta (`SelectionStage` 0x5CFAE0, tope `selectionSystemTimeOut` = 30 s);
  4. **SCRIBBLE cancela**:
     - sacude lo que haya en la mano, si está en la influencia y es `ValidToShakeFromHand`
       (`DoRemoveFromHandVisual` + `ForceDropHeld`; una semilla vuelve a su lugar de culto o se borra);
     - o, con la mano vacía, anula la carga del icono más cargado (paquete 0x1E);
  5. con la mano libre, SPIRAL / INVERSE_SPIRAL abren la selección si hay un icono pedible de esa categoría
     (`OpenSelection` 0x5CF010);
  6. R_SHAPE repite el último milagro (paquete 0x26), si el jugador puede.
- **Las semillas de un uso** no tienen icono, así que con ellas no hay gestos de power-up (solo con las de un icono de
  culto, M7). SCRIBBLE sí las sacude.
- **API para M7** (`PowerUpSystem.h`): `gestures::SetIconProvider(IconProvider*)`. Sin proveedor la selección no se
  abre nunca; `Worship/GestureIconProvider.cpp` registra el suyo. El proveedor responde:
  - `AnyRequestableIconOfCategory` (fn_0064BE40), `ForEachRequestableIcon` (el recorrido de OpenSelection) e
    `IconValidForRequest` (fn_0064BEC0);
  - `RequestSpell` (paquete 0x25), `CanRepeat` / `RepeatLast` (0x26) y `CancelMostChargedIcon` (0x1E);
  - `AnyIconChargingForHand` / `MaxChargeFraction` (las bandas de carga de PHandFX);
  - `PowerUpAvailable` / `SetPowerUpCharge` (0x6A).
- No están portados:
  - la ayuda (`HelpProfile::Trigger` 0xE..0x17): con `OPENBLACK_GESTURE_TRACE=1` sus eventos van al registro;
  - la inmersión (force feedback 3, 8, 9, 10);
  - los iconos de gesto del HUD (`DisplayGesture` fn_0068ABA0, `S_Gesture0/1.raw`, R17 sin leer). La tabla
    `LookingFor` sí se rellena.

### Lanzar desde la mano (`HandSpellSeed.cpp`)

- `ActionPressedHolding` 0x5D1560 con una semilla: sobre un objeto válido (en la influencia) aplica al objeto; si no,
  al suelo bajo la mano, que debe estar en la influencia del jugador. Según el `castType`:
  - **HAND_GESTURE** (tormenta, fuego, escudos, bandadas): arma al pulsar si allí se puede lanzar
    (`ValidToApplyThisToMapCoord` 0x728720 = lista y `CanCast`); si no, `FailApply`. Armar
    (`BeginApplyOnRelease` fn_005D2730) reinicia el búfer con la muestra actual y arranca el bucle IN_GAME 3
    `G_HandGesture_02`. Lanza al soltar (estados 8/9, 0x5D48D0).
  - **HAND_POSITION** (bosque, curar, teletransporte, rayo destructor): lanza al pulsar (`DropOnMapCoord` fn_005D1850).
  - **IN_HAND** (comida, madera, agua, rayo): estado 10/11. Mientras se mantiene, un apply por turno
    (0x5D4C10 / 0x5D4D00); al soltar, `ApplyUnlockProcess` 0x728EB0.
- `SendApplyToMapCoord` 0x5D3340:
  - un paquete por turno (`m_ApplySentTurn`);
  - con un círculo pendiente, el punto y el gesto son los del círculo;
  - **fn_00729AF0: una semilla con `sizingGesture` necesita ese gesto en el paquete**; si no, `FailApply`;
  - un nivel de power-up en carga va con el lanzamiento;
  - luego `SpellSeed::ApplyThisToMapCoord` 0x728E20 (la magnitud es el tamaño del gesto) y el resultado
    (fn_005DA100): la semilla se queda en la mano si el hechizo se mantiene en ella; si no, sale (0x16) con la visual
    SUCEED_CAST (3).
- `FailApply` fn_005D18F0: la visual 4 (`SF_FailedApply`) en el punto y `G_SpellCastFailure`.
- Parámetros de sujeción (0x728640..0x728680): MAGIC hasta que la semilla está lista (`Cwiggle` a media longitud) y
  luego su `holdType`; radio `holdRadius × escala`, más `holdLoweringMultiplier`. La malla de la semilla solo se dibuja
  en la mano con `isSpellSeedDrawnInHand`: fuego, rayo, curar y tormenta son solo su efecto en la mano.

### La mano (`HandMagicFX.cpp`: PHandFX y el efecto en la mano)

- **Efecto en la mano** (CHand fn_0046E7B0 / `DrawSpellInHand` 0x46E680):
  - es el `particleTypeInHand` del nivel, y `SetPowerUp` lo vuelve a crear;
  - se avanza cada fotograma con `max(1, g_game_time_inc)` ms, fuerza = la del PSys de la semilla, magnitud = la escala
    de la mano, y solo con la semilla lista;
  - `UR_FollowLocalHand` 0x69A6A0 y `UR_FollowCastPosn` 0x69FE30 (`Rules/HandFollow.cpp`) lo llevan a la mano;
  - los efectos que avanzan por fotograma se dibujan donde los dejó el último paso (`manager::SetPerFrame`), sin
    interpolar por turno.
- **PHandFX** (ctor 0x68CB10, `Draw` 0x68D0C0, `Band::Draw` 0x68D6D0): bandas `Power_Up_Band.L3d` de escala 10 en el
  hueso raíz, a 10 + 40·índice, girando a (1 + 0,2·índice)·12 rad/s.
  - Permanentes: nivel de power-up + 1 (máximo 5); alfa 20→130 en 0,85 s, con lerp de matrices.
  - Temporales: 5 al ganar un nivel, 0,1 s entre ellas; alfa 20→120, con slerp.
  - De carga: duración lerp(3,5; 1; c), una cada lerp(6; 0,3; c) s.
  - Llegan volando desde 4 m delante de la cámara, a media escala (la matriz 0xEA1CF8 es la de la cámara, inf).
  - `AddSpellToHandVisuals` suena `G_SpellPowerUpBand`; el sacudido, `G_ShakeHand_01` y una banda que se va.
- **El brillo de la mano** (una segunda pasada con `S_Hand_Flow` aditivo, color del jugador, alfa 0,8, atlas 8×4 a
  −20 cuadros/s) se calcula (`hand_fx::GetGlow`) pero **no se dibuja**: hace falta un sombreador de malla con huesos y
  dos texturas (color y `S_Hand_Flowa`).

### Efectos de utilidad (`PSys/Utility.cpp`, PSysUtilityPSys 0xD4E0E8)

- **La estela** (PT 48 `SF_GestureChain`) está activa cuando el juego espera un gesto: semilla de icono cargando,
  semilla con m_Held & 8, selección abierta con la mano libre o semilla con círculo. Va en la mano, con magnitud
  `escala de la mano × f(distancia)` ({0, 50, 500, 1500} → {0,2; 1; 1; 1,5}). Sus reglas `ZR_ChainGesture` 0x68A080
  (emisión fn_0068A330) y `CreateRuleMakeChain` 0x69FD10 están en `PSys/Rules/Gesture.cpp`, y la cinta se dibuja con
  el `ParticleChainCreator` de M5 (`Graphics/RendererChain.cpp`).
- **La selección** (PT 28 `SF_SpellSelection`), mientras está abierta.
- **El gesto reconocido** (`fn_00689790` desde `Success(1)`; PT 35 `SF_Gesture`; `UR_GesturingRecognised`
  0x6884F0 / 0x688910):
  - El registro (0x48 bytes, lista 0xD4EB10) lleva el trazo (los puntos de terreno de todo el búfer) y la forma ideal
    del gesto (`PathSymbol<n>.cam`, o la del círculo) puesta sobre la caja de píxeles de lo encajado:
    - la caja conserva el centro y divide sus medias medidas por las de la forma (fn_0068C140);
    - cada punto va al terreno bajo su píxel, a su altitud, o a 400 m por el rayo (fn_00689F20);
    - si en el suelo la forma sale **más del doble de honda que de ancha** (ejes de la cámara en horizontal), se aplasta
      en vertical ×0,75 y se repite, 15 veces como mucho;
    - la ideal se remuestrea a tantos puntos como tiene el trazo.
  - La regla toma un registro por paso: un átomo (PCreator) y **IN_GAME 36 `G_SpellGestureRecognise`**. En su
    subcolección pone `NumAtoms` (234) sprites del color del jugador, con escala × (longitud de la ideal / 100):
    - la ideal se acerca a la cámara hasta subir esa escala (como mucho a media distancia);
    - cada sprite va del trazo a la ideal (t sobre `TimeToIdeal`, mezclado con smoothstep por `InterpGain`) y se
      enciende desde los extremos (alfa `t × MaxAlpha`);
    - tiembla con ruido de valor de fase barajada, que se apaga tras `DispersalTime`. El ruido es `Noise::VSNoise1To1`
      0x590BB0: la red de Ebert, tabla de permutación 0xBEFDBC y spline de Catmull-Rom 0x590010 (`PSys/Noise.cpp`);
    - la colección pulsa de `CollectionAlphaPulse` a 0 entre 2,4 y 4,5 s, y el átomo muere a `DieAge` (7 s).
  - No están portados:
    - el dibujo de la `LightSheet` de LH3D (50 puntos en la ideal, altura escala × 9, alfa 1 − (2f − 1)²); los datos
      sí están;
    - el pulso de color de la mano (vt 0x2C del objeto de la mano, sin identificar).
  - En el original los valores de la red de ruido son aleatorios en cada partida (1 − GameFloatRand(2)); aquí salen de
    una semilla fija (inf).
- Sin portar: alimentar una bola de fuego en vuelo con una semilla de fuego en la mano (el principio de
  `ProcessPowerUpSystem`); necesita que el cursor pueda señalar la MagicFireBall.

### Ganchos, pruebas y capturas

- Ganchos, en [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración):
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_CAST_PATH`, `OPENBLACK_TEST_THROW_VEL`, `OPENBLACK_TEST_SHOT_PATH`,
  `OPENBLACK_TEST_GESTURE` y `OPENBLACK_GESTURE_TRACE`.
- **Para las otras lanes**:
  - `OPENBLACK_TEST_SEED` + `OPENBLACK_TEST_CAST` lanza por el camino real de la mano;
  - `_CAST_PATH` arrastra la mano durante la primera pulsación (comida, madera, agua);
  - `_THROW_VEL` es la velocidad de la mano que recibe el hechizo (la bola de fuego).
- `test_gestures`:
  - octantes y redondeo de ,5; esquinas de un cuadrado; el borrado tras las muestras quietas;
  - plantillas sintéticas (reconoce la suya y no las otras; el espejo, solo con `allowReverse`);
  - la selección con un icono falso: SPIRAL abre y FORK_RIGHT pide la semilla 4;
  - con `OPENBLACK_GAME_PATH`, `Gestures.jty` (81 × 1628), los 14 gestos del jugador, y CIRCLE y STAR en espejo con
    `reversed`.

## Teletransporte (M6t, `src/Magic/Objects/MagicTeleport`, `src/ECS/Systems/Implementations/VillagerTeleport`)

Informe: `dev\tmp_dis\miracles\protect_creature.md` §2 y las funciones releídas en `impl\m6t`. TELEPORT es
MAGIC_TYPE 12; su clase es `SpellTeleport` (una `SpellWithObjects`, 0xF4). Cada lanzamiento deja una **piedra de
teletransporte invisible** (`MagicTeleport`, una MobileStatic → MultiMapFixed) con una piscina de partículas
(ParticleType 73 `SF_TeleportVortex`). Las piedras de un jugador forman una lista (GPlayer +0xA58 cabeza, +0xA5C
cuenta; aquí `PlayerMagic::teleportStones`). Un vivo que se mueve y entra o se deja caer en una piedra sale por la
piedra del mismo jugador que más lo acerca a donde va.

### El hechizo y la piedra

- `SpellTeleport::InitWithPos` 0x5FBEB0: `MagicTeleport::Create(pos, this)`, la piedra entra en la lista de objetos
  del hechizo (+0xEC/+0xF0) y luego `Spell::InitWithPos`. El hechizo **no tiene tipo de partícula** (el efecto es de la
  piedra), así que InitWithPos manda `SpellEvent 11` (`particleType == 0`) y no crea PSys propio.
- `GMagicTeleportInfo` vt+0x30 (CanCast en pos) 0x5FBE50: falla si hay un MultiMapFixed (edificio, campo, otra
  piedra...) a menos de `fn_005FCCA0` = **6 m** (`fn_00604C30` con el predicado AsMultiMapFixed); si no,
  `GMagicInfo::CanCast` = 1. En openblack `cast_rules::CanCastAt` lo llama `teleport::AnyMultiMapFixedNear`. Como
  `SPELL_AT_POS` no comprueba nada (creador neutral, bandera a 0), el guion planta la piedra igual (el gancho lo
  confirma: «CanCastAt(A) now false» pero la piedra se crea).
- `MagicTeleport::Create` 0x5FC1F0: `new MagicTeleport(pos, spell)` (ctor 0x5FC130: `MobileStatic(pos, 0xD3B614,...)`),
  `CallVirtualFunctionsForCreation` 0x5FC260 (crea el PSys 73 en la pos del mundo y `SetPlayer`), y
  `SetScale(GetScale()·0.01)`. La piedra **no dibuja malla**; `Draw` 0x5FCCC0 solo manda una colisión de mano
  invisible de radio 3 mientras el hechizo tenga icono/semilla, y avanza y dibuja el PSys con el tiempo del fotograma
  (por eso el vórtice va en `SetPerFrame`).
- Coste y temporizadores (efecto 12): costToCreate 5000, initialChants 2000, costPerGameTurn 1, costPerEvent 1,
  `divideCostsByTribalPower = 1`, `costPerKilometer = 200` (GMagicTeleportInfo +0x58). Temporizador del jugador **-1**
  (las piedras persisten), criatura/CP 25 s, un uso 120 s.
- `MagicTeleport::ToBeDeleted` 0x5FC310: quita reacciones, borra el PSys, se desenlaza de la lista del jugador y libera
  la lista de viajeros. `SpellWithObjects::CloseDown` 0x721300 pone a morir cada objeto (`SetDying` 0x4027A0 =
  `ToBeDeleted(0)`), así que cerrar el hechizo borra la piedra. En el código la función se llama `TeleportCloseDown`:
  con el nombre `CloseDown`, dentro de `magic::RegisterTeleportSpell` se resolvía a `magic::CloseDown` (el despacho)
  y el juego se colgaba al cerrarse el hechizo (revisión «review3a»; comprobado con `OPENBLACK_TEST_SPELL=TELEPORT,…,3`:
  se cierra y se borra en el turno 31).

### Quién usa una piedra

- `MagicTeleport::ShouldLivingThingReact` 0x5FC590: el vivo se mueve (IsMoving vt 0x174) y hay otra piedra T del mismo
  jugador con `1.2 · (|vivo−esta| + |T−destino|) < |vivo−destino|` (0x8C6C98 = 1.2), con `dest = GetFinalDestPos`
  (vt 0x884) y `FastDistance` 0x74CE10 (punto fijo, 6553,6 por metro: `max + min/2`).
- `Villager::ReactToTeleportPriority` 0x766200 = `(ShouldLivingThingReact ? 0xFF : 0) & prioridad de la reacción 20`
  (la fila REACT_TO_TELEPORT de `ReactionInfo`). `SetupReactToTeleport` 0x766250 registra el destino en la piedra
  (`fn_005FC6A0`), pone +0xBC = la piedra y entra en `GO_TOWARDS_TELEPORT_REACTION` (201; su gemela rápida 251
  0x766380 es un `jmp` a la de 201). `GoToTeleportReaction` 0x7662F0: al llegar (`AreWeThere`) pasa a
  `TELEPORT_REACTION` (202), si no `SetupMoveToWithHug(piedra)`. `TeleportReaction` 0x7663F0 llama
  `DoTeleport(vivo, false)` y `StopReactingAndSetState`.
- La reacción se **reparte una sola vez**, al crear la piedra (`Reaction::CreateReaction` con marca 0; `ProcessReactions`
  no la vuelve a repartir, su bandera 0xD00DD4 nunca se pone). Por eso solo reaccionan los aldeanos que ya se están
  moviendo en ese instante (comprobado: los aldeanos que deambulan por el pueblo reaccionan y saltan).
- Soltar con la mano: `fn_005FC4B0` exige que el jugador del aldeano sea el de la piedra y `teleportCount != 1`.
  `fn_005FC4F0` deja al aldeano en la piedra (FLYING→LANDED→DecideWhatToDo), registra su destino y hace un `DoTeleport`
  **forzado**; devuelve 1 o 0x17.

### `DoTeleport` 0x5FC790 y el coste (R13)

Entre las otras piedras del jugador elige la de mayor ahorro `s = dist(dest, vivo) − dist(dest, T)` (metros,
`GetDistance` 0x74CD50); umbral 0 (o −1e6 si es forzado). Si hay hechizo: `SpellEvent{2, pos de la piedra}` y
**`PayFor(−s·costPerKilometer·0.001, true)`** (`fn_005FBF10`). `PayFor` 0x720990 → `fn_00720FC0` hace `chants −= coste`
sin recorte, dividido por `max(poderTribal,1)` con `divideCostsByTribalPower`. **Lectura literal (R13 resuelta): un
salto útil (s > 0) le da cánticos al hechizo; solo un salto forzado hacia atrás (s < 0) cuesta.** Comprobado: un salto
forzado de −30 m cobra 6 (=30·200·0,001); los saltos naturales de +50..60 m tienen ahorro positivo. Luego
`CreateSpotVisual(SPOT_VISUAL 14 VILLAGER_TELEPORT = SF_TeleportVillager, 1,0)` en los dos extremos y
`Living::MoveByTeleport` 0x5EC340: sonido `G_SpellTeleportEnergiseGo` (InGame 39) donde estaba, `..Arrive` (InGame 38)
donde llega, y `MoveMapObject`.

- `GPlayer::Process` 0x6496BC → `fn_005FCC70`/`fn_005FCBA0`: cada turno, por cada piedra disponible, se caen de la
  lista de viajeros los que ya no existen o no siguen la reacción de la piedra (`teleport::ProcessPlayers`, ranura 3
  del turno).
- La ruta al lugar de culto por piedras (`Villager::CanIGetToTheWorshipSite` 0x76BC20 → `GPlayer::fn_0064D6B0`) está
  portada como `teleport::FindRouteStone` (la piedra más cercana al inicio si `d1 + d2 < maxDist`), pero aún no la usa
  nadie (es M7).

### SF_TeleportVortex y ZR_SurfRevol (`src/PSys/Rules/SurfRevol`, `src/Graphics/RendererSurfRevol.cpp`)

- El fichero `SF_TeleportVortex`: `ZR_SurfRevol` FunctionIndex 0 (disco plano), textura `S_TileLandscape.raw` (256×256,
  con alfa `S_TileLandscapeA.raw`), NumU 12 × NumV 5, SpeedV 0,2137, alfa de entrada/salida 0,4, iluminado; escala 5 por
  `UR_ChangeScale` sobre el átomo padre; bucle de sonido `TELEPORT_POOL` (lane S).
- `ZR_SurfRevol::ModifyAtomCollection` 0x686370 (props 0x6B2E80): crea un `RenderParticleGJMeshRotatingUV` (ctor
  0x6C8B60) con una malla de revolución construida en `fn_006858F0`. El perfil sale de FunctionIndex (tabla 0x6868CC):
  0 `TestDisk` (r=t, y=0), 1 `TestFunnel` (r=t, y=3(√t−1)), 2 `TestFunnelSpout` (r=1,5t, y=3(√2t−1)), 3
  `TestFunnelParab` (r=t, y=3(t²−1)); cualquier otro → disco. Vértice (i,j): `u=i/(NumU−1)`, `t=j/(NumV−1)`,
  `(r(t)cos2πu, y(t), r(t)sin2πu)`, uv `(u,t)`. Con `FadeAlphas`, la fila con `t<AlphaFadeIn` va en RGB `255·t/fadeIn`
  (alfa 255) y con `t>1−AlphaFadeOut` en alfa que baja a 0 (RGB 255); con `ChangeSpecColor` el especular es el color del
  jugador × `(1−ese factor)` (`GetPlayer3DColor` 0x64B590 → tabla 0xBFF0B8 con `GetRemapedPlayer`; el neutral es
  0xFF000000). `fn_00685F00` escala las UV por `TextureWidth/256, TextureHeight/256`; `fn_00685F40` retuerce las UV
  `u += (1−t)²·MaxUVChange`; `fn_00685FC0` gira cada fila `t·MaxVertexChange` sobre Y; con `DoRaiseAboveLandscape`
  (`fn_00686980`) cada vértice sube al terreno bajo él menos el del centro (aquí al dibujar).
  `RenderParticleGJMeshRotatingUV::GameUpdate` 0x6C8BC0 desplaza las UV (SpeedU/V) dentro de la baldosa; `DrawAt`
  0x67CBA0 dibuja en modo 6 (color = textura×difuso + especular, alfa = textura×difuso).
  - openblack: la regla `ZR_SurfRevol` guarda la malla en un `SurfRevolCreator` (tipo `Other`) por átomo; el disco
    hereda la escala del átomo padre (UR_ChangeScale = 5, **sin verificar** cuál escala aplica el original, es la
    documentada). `RendererSurfRevol.cpp` la dibuja con el programa `WorldQuad` en dos pasadas (la textura mezclada y el
    especular sumado con una textura blanca 1×1). La `S_TileLandscape.raw` de esta instalación está fechada en 2021
    (reemplazada por un parche/mod). **Lo comparte la lane m7** para los discos de los dispensadores.
- `SF_TeleportInHand` (sprite, le falta `UR_FollowLocalHand`) y `SF_TeleportOnHolder` son de M2/M7; el destello del
  aldeano SV 14 `SF_TeleportVillager` ya funciona (`psys::manager::CreateSpotVisual`).

### Pruebas y capturas

- `test_teleport`: `FastDistance`, la regla del desvío (1,2), `ChooseTarget`, el signo del coste (R13: 500 m útiles
  devuelven 100 cánticos, −20 m forzados cuestan 4) con `Spell::PayFor` real, `FindRouteStone`, los cuatro perfiles de
  `ZR_SurfRevol` y la malla del vórtice (12×5, fundidos, triángulos, torsión de UV y vértices, colores de jugador).
- Gancho `OPENBLACK_TEST_TELEPORT="x0,z0,x1,z1[,jugador[,modo]]"` (`OPENBLACK_TEST_TELEPORT_TURN=<n>`,
  `OPENBLACK_TELEPORT_TRACE=1`): planta dos piedras (como `SPELL_AT_POS`) y, en modo `walk`, hace andar al aldeano más
  cercano hacia B; en `drop`, lo suelta forzado sobre A; `none` solo las piedras.
- Capturas en `dev\_audit\magic\`:
  - `teleport_discs.png`: las dos piscinas `S_TileLandscape` translúcidas sobre el suelo del pueblo de Land1;
  - `teleport_react.log`: los aldeanos 47/80/33 reaccionan a la piedra A, andan hasta ella y saltan a B (ahorros
    +50..60 m);
  - `teleport_jump.log`: un salto forzado (soltar) con `PayFor(6, forzado)` y SV 14 en los dos extremos.

## Bosque (M4b, `Magic/Spells/SpellForest`, `Magic/Objects/MagicTree`, `ECS/Forests`, `ECS/TreeGrowth`)

Informe: `resources.md` §3; lo de abajo está releído en el exe (W120, `tmp_dis\miracles\impl\m4b\`) y corrige varias
cosas del informe. La escena de la diosa de los árboles (toma de la cámara) está aplazada.

- **SpellForest** (MAGIC_TYPE 13 NATURE; `GMagicForestInfo::AllocSpell` 0x5FAD90, 0xF8 bytes): +0xEC el Forest,
  +0xF0 «bosque creado» (fn_007254F0 lo pone a 0), +0xF4 el máximo de árboles (`SetMaxObjectsToCreate` 0x7256C0: -1 →
  `finalNoTrees` 18). `InitWithPos` 0x725540 es el de Spell.
  - `CanCast` 0x5FAE80 (vt 0x30): dentro del mapa, tierra, `fn_005FADF0` (ningún Abode de **la celda** del punto,
    `FindType(0)`, tiene `Get2DRadius > distancia` a su centro de fuego) y `ValidPlaceForTree` 0x725C50 (dentro,
    tierra y no `MapCoords::IsFixed`).
  - **`IsFixed` 0x603790 → `MapCell::IsFixed` 0x601EA0 mira solo el primer objeto fijo de la celda** (MapCell +4,
    donde `Fixed::InsertMapObjectToCell` 0x52DEA0 pone el más nuevo con `SetFirstObjectFixed`) y su bit +0x24 & 2, que
    solo pone el ctor de `MultiMapFixed` 0x52E1F0 (edificios, campos, features, estáticos, bosques grandes...). Un árbol
    es `SingleMapFixed` (solo en su celda) y no lo tiene: una celda cuyo último fijo es un árbol no está «ocupada»,
    aunque tenga un edificio debajo. openblack toma el más nuevo por el índice de creación (inf: un árbol replantado
    vuelve a ser el más nuevo de su celda y openblack no lo sabe) y cuenta los árboles del propio evento.
  - `SpellEvent` 0x725830: nada con el tipo 1 o si ya hay Forest; `ApplyDefaultSpellEffect` (paga costPerEvent 1;
    EffectValues de NATURE: alineamiento 1; reacción 21) y, si aplica, **todo el bosque de golpe**: N = `fn_00725790` =
    round(+0xF4 × (fuerza > 0)); paso = N > 1 ? 1/(N − 1) : 1; vueltas = N × **17/13** (el float 0x9819FC =
    1,3076923) × 2π; para i < N: f = i·paso, r = 2 + 9·√(1 − (1 − f)²), a = f·vueltas, punto = castPos (+0xC0) +
    (r cos a, r sin a) truncado a MapCoords (ftol × 6553,6); si `ValidPlaceForTree` y el bosque tiene menos de N
    (fn_00725800), `fn_00725600(punto, GetRandomTreeInfo(castPos))`.
  - `GetRandomTreeInfo` 0x725580: `Terrain::GetMaterialInfo` 0x735330 de la celda del **punto de lanzamiento** (no de
    cada árbol) y `magicTreeTypes[GameRand(4)]` (un GameRand por árbol). `GetTerrainMaterial` 0x735380: 27 (nieve) si
    `GClimate::GetSnow ≥ 27` en la esquina de la celda; si no, el tipo del segundo material de la altitud de la celda en
    su país (fn_00804CF0 → fn_00873770, como `GSoundMap`); **un 0 (fuera del mapa o sin bloque) pasa a 1**, que es
    DEEP_WATER: palmeras; `> 43` → 0. Hierba (18) y casi todos: haya, abedul, cedro, cedro; arena y agua: palmeras;
    nieve y hielo: coníferas.
  - `fn_00725600`: con el primer árbol crea el Forest (`new 0x58` → fn_005399E0(pos, creador): el jugador del
    creador, id = contador 0xBEA238++, al principio de la lista g_game +0x205BB4) y marca +0xF0. Luego
    `fn_005FD000(pos, hechizo, info, bosque, GameFloatRand(2π), 0, woodValueMultiplier 0,25 × poder tribal)` y la
    escala objetivo +0x64 = `fn_007255C0` = **1 − 0,5 × distancia/11**: 0,91 el primero (a 2 m) y 0,5 en el borde (el
    plan decía 1,0 en el centro: no hay árbol en el centro).
  - `Process` 0x7259C0 = `CoreProcess` + `ProcessTrees` 0x725A30 (su argumento no se usa): si el bosque se borró (+0xA
    **bit 0**, el de ToBeDeleted; el informe decía bit 1) → +0xEC = 0 y CloseDown; sin bosque devuelve 1 mientras haya
    PSys y 5 (se borra el hechizo) cuando no; con bosque compara N con los árboles (+0x4C + +0x54, sin signo):
    N < árboles → `fn_0053A490`: **todos** menguan `decaySpeed` 0,05 (fn_0074A3A0: escala − d; ≤ 0 → ToBeDeleted);
    si no → `fn_0053A520`: **todos** `Tree::Grow(growSpeed 0,01, 0, 0)`. El número que reciben las dos no se usa.
    Devuelve 1.
  - `CalculateCostToMaintain` 0x7259E0 = costPerGameTurn (5) + árboles × costPerEvent (1): 23 con 18.
  - `CloseDown` 0x55D1E0 = CoreCloseDown + creador = NULL. **Los árboles no se quedan** (el informe decía que sí): sin
    creador `GetSpellStrength` 0x720750 da 0, así que N = 0 y todo el bosque mengua 0,05 por turno hasta desaparecer;
    con el último árbol se borra el Forest y al turno siguiente el hechizo (los hechizos cerrados se siguen
    procesando: `ProcessSpells` solo salta los que no están `IsAvailable` 0x401810, es decir, los borrados). Un bosque
    de un jugador (`timerWhenPlayerCasting` -1) dura mientras se pague; el de un milagro de un uso (120 s) o el de un
    guion con duración se va al cerrarse. Traza real (duración 8 s): cierra en el turno 81, mengua desde ~0,4 y en el
    88 no queda ninguno; el hechizo se borra en el 89.
  - `ToBeDeleted` 0x725500: borra el bosque (si no se está borrando ya) con sus árboles.
  - `GetMaxObjectsToCreate` 0x7256F0 = min(+0xF4, bosque ? sus árboles : (+0xF0 ? 0 : 18)); lo guarda la semilla
    (`StoreChantsAndAgeFromSpell`, vt 0x550: `SpellOps::maxObjectsToCreate`). `HasEnoughChantsAndLifeForRecast`
    0x725730 = eso > 0. `AdjustSpellSeedPos` 0x725750: altitud = max(altitud, el árbol más alto, fn_0053A740 con
    vt 0x42C `GetHeight`), −5 sin bosque; no encontré quién la llame (ningún `call [reg+0x540]` es de un hechizo).
  - `fn_00725B20` / `fn_00725BF0` (un árbol en un punto al azar del anillo 2..11 m) no tienen llamador.
- **MagicTree** (0x74 bytes; `MagicTree::MagicTree` 0x5FCF50, creado por fn_005FD000): `Tree(pos, info, bosque,
  maxScale 1,0, ángulo, escala 0)`, bandera mágica +0x5C | 2, +0x70 = multiplicador de madera, +0x6C = el jugador del
  hechizo (sin hechizo, el de g_game +0x205A5B) y `CreateReaction(this, REACT_TO_MAGIC_TREE 8, GetPlayer, 0)`.
  - `ToBeDeleted` 0x5FD070: sus reacciones, `Tree::ToBeDeleted` y, si el bosque sigue disponible y se quedó sin
    árboles, el bosque.
  - `StartOnFire` 0x5FD0D0 quita la reacción 8; `EndOnFire` 0x5FD0E0 la crea otra vez salvo con la bandera g_game
    +0x14 & 0x8000 (sin identificar: se toma como apagada).
  - `GetWoodValueMultiplier` (vt 0x868) = +0x70; el de Tree 0x74B810 da 1. `Tree::GetWoodValue` 0x74B7B0 = vida × eso
    × woodValue × escala × [0xD1A294]; `HandSystem::DepositInStore` ya lo multiplica. `GetImpressiveType` 14.
- **Tree** (`ECS/TreeGrowth`, `components::Tree`): el ctor 0x749E00 recibe (pos, info, bosque, maxScale → +0x64,
  ángulo, escala); si maxScale ≠ escala marca «creciendo» (+0x5E bit 0) y la cuenta +0x60 (int16) =
  GameRand(growsAfterNumGameTurns); luego `Forest::AddTree` y +0x5E | 2. `TreeArchetype::Create` hace ahora lo mismo.
  - `Tree::Grow` 0x74A3F0 (cantidad, setScale, subirMax): con subirMax, maxScale = max(maxScale, escala + cantidad);
    nada si la escala ya es maxScale; si no, min(escala + cantidad, maxScale) con `SetScale` (vt 0x124) o con
    `SetJustScale` (vt 0x51C) y la matriz 3D rehecha con la escala, el ángulo Y y la posición. Devuelve lo que creció.
  - `Tree::Process` 0x74A290 (vt 0x5FC, lista de los que crecen): baja la cuenta; en 0 vuelve a
    growsAfterNumGameTurns y, si crece por debajo de maxScale, `Grow(growthAmount × (1 + GetMaxRainingOrSnowing × 0,01
    × rainingAcceleratorMultiplier) × (1 + alineamiento del sitio × 0,5), 0, 0)`; devuelve 1 si sigue creciendo.
    **El crecimiento natural está aplazado** (no se llama a Grow); la cuenta y el paso a la lista de crecidos, sí.
  - `Tree::ToBeDeleted` 0x74A210: sale de su bosque (fn_0053A220) y de la lista global de árboles g_game +0x205CDC.
- **Forest** (`ECS/Forests`, `components::ForestTrees`; 0x58 bytes: id +0x40, siguiente +0x44, `Trees0` +0x48/+0x4C
  los crecidos, `Trees1` +0x50/+0x54 los que crecen):
  - `AddTree` 0x53A310: árbol +0x68 = bosque; a `Trees1` si crece por debajo de maxScale, si no a `Trees0`; cada
    lista ordenada por `DistanceToForest` 0x53A890 (distancia al bosque, 0 sin bosque): el nuevo va delante del primero
    que esté más lejos. `fn_0053A220` lo saca.
  - `Forest::ProcessForests` 0x539D70 (ranura 5 del turno) → `Forest::Process` 0x539DA0: un bosque vacío (con +0x38 =
    0) pone +0x34 = 2000 y lo baja cada turno; por debajo de 2 se borra. Si no, y con +0x3C ≠ 1, `Tree::Process` de
    cada árbol que crece; los que devuelven 0 pasan a `Trees0`. Luego los árboles nuevos naturales (cada 2000 +
    GameFloatRand(1000) × 0,05 × crecidos, junto a uno al azar, fn_00539FD0 / fn_0053A010, y FUN_0064da80(14, 1) al
    jugador más influyente) están aplazados. +0x38 y +0x3C no se sabe qué son (0 en el ctor).
  - `ToBeDeleted` 0x539C60: `ToBeDeleted` de cada árbol de las dos listas y sale de la lista.
  - **No hay unión de bosques**: el milagro siempre hace un Forest nuevo. Los bosques de los guiones siguen siendo los
    números `forestId` de openblack (el id nuevo es uno más que el mayor en uso, inf).
  - En openblack, los árboles que otros sistemas quitan sin `ToBeDeleted` (quemados, al almacén, árbol muerto) salen
    de las listas en el siguiente acceso, con sus reacciones, y el bosque se va con el último (inf).
- **El efecto SF_Forest sin la escena** (lo que se ve): la semilla `Seed.L3D` (escala 0,207) cae desde 9,4 m con
  gravedad 1,6 (máx. 3,35 m/s) girando, con SOUND_SPELL_FOREST_1 y un disco de manchas azules; al tocar tierra (turno
  41, 4,1 s) salen los 18 árboles. **Todo lo demás cuelga del átomo de la cámara** (grupo 2,
  `ParticleAnimWithCameraCreator`: `Tree_Goddess_test.anm` + `Forest.cam`, pausa 4 s, alfa de 0 a 252 entre 4,1 y
  9 s y de vuelta a 0 entre 22,1 y 23,5 s, vive 25 s): a los 3,5 s `AddSubCollectionsToAtom` añade las chispas, el
  vórtice (`Vortex.L3D`, 6) y la raíz de chispas (SOUND_SPELL_FOREST_4); a los 5,5 s las mariposas
  (SOUND_SPELL_FOREST_3; `UR_ForestPath` + `UR_Flocking` + `ParticleGoodEvilCreator` → `ParticleAnimCreator`
  S_Butterfly_Flap.anm, o murciélagos M_Bat_Flap.anm); y a los 3,5 s el mapa de luz `Forest_LMap.raw` con chispas
  permanentes. Con el creador de la cámara sin portar el átomo existe pero no dibuja ni mueve la cámara, así que se
  ven la semilla, las manchas, las chispas, el vórtice y el mapa de luz. **Sin portar:** la diosa y la cámara
  (aplazado) y las mariposas (esas cuatro clases; `ParticleAnimCreator` es una malla animada .anm dentro del PSys).
- **Ganchos:** `OPENBLACK_TEST_SPELL=NATURE,x,z` (o `13`), `OPENBLACK_TEST_MAGIC_TURN=<n>` y
  `OPENBLACK_TEST_FOREST_SHOT` ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)).
  Con `OPENBLACK_SPELL_TRACE=1` cada turno escribe `SpellForest trees <n> wanted <N>: grow|decay <suma>` y el evento
  `SpellForest event 3 ...: N trees wanted, M made ..., material m`.
- **Pruebas:** `test_spell_forest` (la espiral con las constantes exactas, la cuantización a MapCoords, la escala
  objetivo, N, GetMaxObjectsToCreate, el coste y AdjustSpellSeedPos; con `OPENBLACK_GAME_PATH`, la fila NATURE: 18,
  0,1, 0,01, 0,05, 0,25, 10000 cánticos, 120/-1 s, y los `magicTreeTypes`). Capturas en `dev\_audit\magic\` (hierba al
  oeste del pueblo de Land1, 1790, 2625; cámara `1772,52,2604,1790,36,2625`): `m4b_forest_saplings.png` (3 turnos tras
  caer la semilla: los 18 brotes en espiral y el destello), `m4b_forest_saplings12.png` (12 turnos: escala 0,12),
  `m4b_forest_40.png`, `m4b_forest_grown.png` (120 turnos: cada árbol en su escala objetivo, 0,91 dentro y 0,5 en el
  borde) y `m4b_forest_decay.log` (duración 8 s: el bosque mengua y desaparece).

## Curar (M4 «m4h», `Magic/Spells/SpellHeal.cpp`, `PSys/Rules/Heal.cpp`)

Informe: `resources.md` §4 y `visuals_sound.md` §4.4. MAGIC_TYPE 10 HEAL y 11 HEAL_PU_ONE, semilla 7 HEAL. El hechizo no
cura nada por sí mismo: **busca los objetivos y el PSys los cura uno a uno** con un evento de tipo 5 por chakra.

### Buscar objetivos y lanzar

- `GMagicHealInfo::FindTargets` 0x5FBB00 (ya en `Magic/CastRules.cpp`): radio `dummyVar` 10 (35 con PU) y tope
  `maxToHeal` 20 (100), los dos × el poder tribal si hay hechizo; espiral de `ceil(2R/10)²` celdas; cada vivo que acepta
  el efecto y `CanBeHealedByHealSpell` a menos de R pasa a ser objetivo del PSys (`AddTarget_`, vt 0x114). **No mira si
  le falta vida**: un sano también recibe su chakra (comprobado en el juego: aldeanos con vida 1 se iluminan).
- `SpellHeal::InitWithPos` 0x72D870 = `Spell::InitWithPos` y, **si devuelve 1**, `FindTargets(pos, this)`; no mira
  cuántos encontró. La comprobación de la mano es `GMagicHealInfo_vfunc12` 0x5FBD20 (vt 0x30): solo con MAGIC_TYPE 10 u
  11 y `FindTargets(pos, NULL) > 0`.
- `SpellTargets::TakeTargetObject` 0x671030 saca **el último** de la lista (`psys::Effect::TakeTarget`), así que el
  PSys consume los objetivos en orden inverso al que los encontró.

### `UR_HealSpellChakra` 0x6A0B20 (`PSys/Rules/Heal.cpp`)

Constructor 0x6A0810 (pone las marcas 6 del modificador y quita la 4: **no es creador, pero mantiene vivo el efecto
hasta que se cierra**; en openblack es `Modifier::KeepsAlive`), propiedades 0x6B1E40. Cada turno:

1. Saca objetivos de `SpellTargets` mientras haya. Los que ya tienen un chakra (lista global 0xD4ED38, `g_Chakraed`) o
   ya no existen se descartan; el resto:
   - **manda `SpellEvent{tipo 5, centro del objetivo, fuerza 1, objetivo}`**, que es lo que cura de verdad
     (`ApplyDefaultSpellEffect` → `ApplyEffect(EV, 0)` → `IncreaseLife`, y `SetPoisoned(0)` si estaba envenenado);
   - crea un átomo de punto con su `AtomData` (0x38 bytes, ctor fn_006A0920): el objetivo, su `Get2DRadius` (+0x30) y
     su `GetHeight` (+0x34), y lo mete al principio de la lista global (fn_006A0A60);
   - la posición es fn_006A0AB0: el punto del objetivo con la altura del terreno y, con `TakeCentrePos`, **media altura
     del objeto** más arriba.
2. Recorre sus átomos: el **primero** de la lista, en cuanto tiene edad > 0, suena `SoundHeal` (SOUND_SPELL_HEAL, una
   sola vez por átomo; `SoundSpacing` 0.2 se lee y no se usa). Cada chakra sigue a su objetivo y, con
   `ScalePropObjectSize`, su escala de regla (+0x78) es el `Get2DRadius` del objetivo.
3. `fn_006A0E30` da la intensidad del chakra con la edad de su **subcolección** (el estallido del grupo siguiente):
   `t = edad / AtomAgeMaxAlpha` hasta 1 y luego `1 − (edad − AtomAgeMaxAlpha) / (AtomAgeZeroAlpha − AtomAgeMaxAlpha)`,
   recortada a 0..1 (`psys::heal::ChakraFade`). Con ella:
   - los átomos del estallido reciben alfa `int(t × MaxAlpha)` (100.46 → 100 en el pico);
   - el objetivo recibe `SetSpecularColor(int(t × SpecularColor))` (`Living` 0x417480, +0xD0), es decir el brillo
     (200, 255, 255) × t: **el aldeano se ilumina** mientras dura el chakra.
   Cuando el estallido se queda sin átomos (a los 3 s) el chakra se borra, y al destruirse su `AtomData` (fn_006A09A0)
   sale de la lista global y le quita el brillo al objetivo (`SetSpecularColor(0)`).
- Un chakra cuyo objetivo desaparece se borra; si un chakra no tiene subcolección la regla se suelta (0x6A0E1C
  devuelve 0). El bit 4 de `Objeto+0x24` (sin identificar) también termina el chakra en el original: no portado.
- El brillo especular se dibuja en `RenderingSystem` / `vs_object`: `components::SpecularColour` va empaquetado en la w
  de la cuarta columna de la instancia (3e6 + 7 bits por canal) y se suma al especular de la luz del terreno, como hace
  fn_0080BF10 desde fn_0080BEC0 (`Villager::Draw` fn_0051B3D0, `Animal::Draw` 0x51C4D6).

### `CreateRuleFusedSphericalExplode` 0x69F610

El estallido de cada chakra (5 sprites de `S_SpriteSheet1`, FileOffset 57, 7 cuadros, escala 5, aditivos). Cuando la
colección cumple `FuseTime` (0 en el chakra) hace `NumAtoms` átomos de golpe: dirección de `PSysRandR3` (rechazando el
vector nulo), normalizada, con `|y|` si `OnlyHemisphere`; **una sola velocidad** para todo el estallido,
`MinSpeed + rand(MaxSpeed − MinSpeed)` (0.6 en el chakra), con la y × `ScaleYSpeed`. Solo el primer átomo suena
`SoundExplode`; con `DisableParent` (1 por defecto) el átomo padre deja de dibujarse (marca 0x10). Después la regla se
suelta. Valores por defecto del constructor: NumAtoms 100, FuseTime 20, ScaleYSpeed 2, DisableParent 1.

### `UR_HealInHand` 0x6A0F40

La onda de la mano (SF_HealChakraInHand y SF_HealChakraOnHolder, `WiggleFreq` 0.6; por defecto 1): cada átomo se coloca
en `GetCurrentParentPos × sin(edad de la colección × WiggleFreq × 2π)`, y los átomos impares con el signo contrario. Es
literalmente una multiplicación de la posición del padre, no un desplazamiento.

### Vida y veneno

- `Object::IncreaseLife` 0x637870 (`Villager::IncreaseLife` 0x753460 solo la llama): si vida + cantidad > 1, la cantidad
  se recorta a `1 − vida`; con cantidad ≤ 0 no toca nada; si no, `SetLife(vida + cantidad)`. Devuelve la vida nueva. El
  efecto de curar es `Object::GetHealEffect` 0x637D80 = `EV.heal × defenceMultiplierHeal` (R9 resuelta: no hay más
  recorte que el 1).
- Con efecto de curar 1 (la fila HEAL de info.dat), un aldeano a 0,3 de vida pasa a 1 de golpe.
- Envenenado: bit 1 de `Living+0xB4` (`Living::IsPoisoned` 0x416F90 / `SetPoisoned` 0x416FA0). En openblack es
  `components::Poisoned`, y `ApplyDefaultSpellEffect` lo quita con HEAL o HEAL_PU_ONE. Quién envenena (comer comida
  envenenada) aún no está portado; el tinte del aldeano envenenado (difuso 0xFFE8FFDD y especular 0xFF001000,
  `Pot::GetPoisonColor` / `GetPoisonSpecular` 0x51BB50, fn_0051B3D0) tampoco.

### Sin portar / sin verificar (curar)

- **La niebla (`ParticleMistCreator`) no hace falta para curar**: SF_HealChakra solo usa `ParticlePointCreator` y
  `ParticleSpriteCreator`. Queda para el agua y el tiempo.
- El power-up (SF_HealChakraPU) añade la malla `MSH_S_HEAL_MESH` (532) con `UR_KPStretchHeight` 0x6A50C0 y
  `UR_KPMoveAtoms` 0x6A60B0 (interpolación de puntos clave `KPSplineInterpolator::EvalAtT` 0x6A7EB0, un spline cúbico
  con el factor 1/6; `MovePropAtomIndex` escala el desplazamiento por `índice / (NumAtoms − 1)`) y el sonido
  HEAL_MUSHROOM: **desensamblado, no portado**. El chakra sí funciona en el PU (radio 35, hasta 100 objetivos).
- El orden del sonido: el original toca `SoundHeal` en el primer átomo de **su** lista, que va del más nuevo al más
  viejo; en openblack la lista va al contrario, así que suena en el chakra más antiguo (inf).
- `CanBeHealedByHealSpell` (vt 0xB18) da 1 en la criatura y 0 en la paloma (M4c/M8).

### Ganchos, pruebas y capturas

- `OPENBLACK_TEST_HURT_VILLAGERS="x,z,radio,vida[,envenenado[,turno[,curar[,repetir]]]]"`
  ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)): hiere a los aldeanos del radio,
  puede lanzar el milagro ahí mismo y escribe cada cambio de vida, veneno y brillo.
- `test_heal`: la curva del chakra (`ChakraFade`, el alfa 100 del pico y el brillo 160 a 1,2 s), que la regla mantiene
  el efecto vivo sin átomos hasta el `CloseDown`, la mecha y la velocidad única del estallido, la onda de la mano y,
  con `OPENBLACK_GAME_PATH`, los valores reales de SF_HealChakra y SF_HealChakraInHand.
- Capturas en `dev\_audit\magic\`: `m4h_heal_chakra.png` (Land1, aldeanos de 1800, 2648 con la cámara en
  `1789,35,2639`: tres chakras encendidos, con el aldeano iluminado dentro de cada estallido) y
  `m4h_heal_trace.log` (vidas 0,300 → 1,000 y veneno curado en los tres, el brillo subiendo 40 → 106 → 160 y bajando a
  0, 18 átomos —1 punto + 5 sprites por chakra— durante 3 s y el hechizo abierto hasta sus 20 s).

## Escudos (M6-shield, `Magic/Spells/SpellShield`, `Magic/Objects/MapShield`, `PSys/Rules/Shield`)

Informe: `dev\tmp_dis\miracles\protect_creature.md` §1; lo nuevo, verificado en `runblack.exe` al portarlo, está en
`dev\tmp_dis\miracles\impl\m6s\` (spark, spin, vapour, EP, tracer). MAGIC_TYPE SHIELD (19) y PHYSICAL_SHIELD (20) usan
la misma clase `SpellShield` (SpellWithObjects, 0x10C bytes), que hace un objeto `MapShield`: `MagicShield` (invisible;
la cúpula es el PSys del hechizo, SF_DefenseSphere) o `PhysicalShield` (la malla sólida MSH_S_SOLID_SHIELD 554).

### El hechizo (`SpellShield.cpp`)

- `InitWithPos` 0x72B5F0, en orden: el radio (castData +0) se recorta **primero por arriba y luego por abajo**
  (`maxRadius` 1000 si no es menor, luego `minRadius` 5 si no es menor) y se reescribe en castData; `Spell::InitWithPos`
  (la magnitud queda en el radio); la reacción REACT_TO_MAGIC_SHIELD (13) del jugador del hechizo con radio r + 30
  (reaction +0x3C, `reactions::SetRadius`); la ciudad más cercana a menos de 250 m (`MapCoords::GetNearestTown`
  0x6020E0, todas las ciudades de todos los jugadores y del neutral); **un anillo anti de radio = la magnitud por cada
  otro jugador activo** (`GGame::GetNextActivePlayer` 0x5508D0: los siete huecos con +0x8E0 ≠ 0; aquí los jugadores
  que la tierra creó); y `MapShield::Create` 0x72BE20 en la lista de objetos. Si `Spell::InitWithPos` falla, el
  original hace todo lo demás igualmente (y luego lo borra).
- `Process` 0x72B750: suelta la reacción de golpe (+0xF4) cuando ya no está, y `SpellWithObjects::Process`.
- Coste 0x72B7F0: `costPerGameTurn × (magnitud / radiusForNormalCost)²` (20 y 22 × (r/30)²; con r = 40, 35,6 y 39,1
  por turno); `divideCostsByTribalPower` = 1 lo divide en `PayFor`. Temporizadores: jugador -1 (dura mientras se paga),
  criatura y ordenador 20 s, un uso 40 s.
- `CloseDown` 0x72B840 = `SpellWithObjects::CloseDown` 0x721300: `CoreCloseDown` y, como
  `GetSetObjectsDyingOnCloseDown` (0x55CF50) da 1, `SetDying` (vt 0x6A4) en cada objeto. `ToBeDeleted` 0x72B500: fuera
  de la lista 0xDA07F0, se van sus reacciones, +0xF8 = 0, `SpellWithObjects::ToBeDeleted` 0x720FD0 (CloseDown y la
  lista vacía) y el `ToBeDeleted` de cada anillo.
- `UpdateStruckReaction` 0x72B780 (vt 0x51C): la reacción 35 la primera vez; después solo refresca su turno
  (reaction +0x2C, `reactions::Stamp`). `SetUpDestroyedReaction` 0x72B7C0 (vt 0x520): quita las 13 que empezó,
  +0xF8 = 0 y crea la 36. En la base `Spell` las dos están vacías (0x55CE10 / 0x55CE20). `SpellOps` tiene ahora
  `updateStruckReaction` / `setUpDestroyedReaction` y `SpellHitSpell` (fn_00720B70) las llama en el hechizo golpeado.
- `IsUnder` 0x72BD20: `dist(p, castPos) < GetRadius() − margen` (x, z). `fn_0072BA00` (para `SPELL_AT_POINT`): el primer
  escudo disponible cuya magnitud cubre el punto; su máscara se prueba como `(bit | máscara) != 0`, así que no filtra.
- Aviso para las demás clases: en un `Spells/*.cpp`, una función del espacio anónimo llamada `CloseDown` y registrada
  como `ops.closeDown = CloseDown` dentro de `openblack::magic::Register...()` **resuelve a `magic::CloseDown`** (el
  despacho), que se llama a sí mismo sin fin: el juego se cuelga al cerrarse el hechizo. Aquí se llama `ShieldCloseDown`.

### El objeto (`MapShield.cpp`)

- Lista `g_game +0x205CA4`, el más nuevo primero; `MapShield` ctor 0x72C070 (`FixedObject(pos, info, 0, 1)`).
- **MagicShield** (fn_0072C250): `SetScale(0.017 × r)`; `Draw`, `DrawShield` y `ProcessShield` vacíos; `SetDying`
  0x72C320 lo borra al momento (3); `IsEffectReceiver` 0; no interactúa con las físicas.
- **PhysicalShield** (fn_0072C9F0 tras los ceros y unos de fn_0072CB70):
  - `creationTurn` = el turno; **`SetScale(1.0)` y la altura `shieldHeight + raiseWithScale × 1.0` con el 1.0 que
    aún tiene `finalScale`**; `startSpin = clamp(Spell +0x98, ±3)` — **R14: Spell +0x98 es el `curl` del
    PSysProcessInfo del hechizo** (+0x64 + 0x34; el curl del guion o de la interfaz); `endSpin = ±0.15` con el signo del
    inicial (0 cuenta como +); `startScale = 0.017 r × 0.01`; `finalScale = 0.017 r` y `SetScale(finalScale)`.
  - fn_0072CD40: la matriz y escala actuales y previas, **dos `ProcessShield` para cebarlo** y el efecto
    SF_PhysicalShieldFX (PARTICLE_TYPE 0x43) en `(x, suelo + altura, z)` con magnitud 1, luego `SetPlayer`,
    `AddTarget(el escudo)` y `SetMagnitude(radio)`. El escudo lo avanza él mismo cada turno con un PSysProcessInfo de
    ceros, fuerza 1 y activo (vt 0x100 en `ProcessShield`).
  - `ProcessShield` 0x72D190 (desde `Spell::ProcessSpells` por fn_0072BF80, antes de los mantenimientos):
    - `t` = (turno − creación) × 0,1 s. Antes de 0,5 s, **A = 1 − t/0,5: el escudo nace a tamaño completo y se encoge
      mientras no se dibuja**, y el giro no avanza. Después, `u = t − 0,5`: A = x + x² − x³ con x = u/1,5 (1 pasado
      1,5 s) y el giro `startSpin + (endSpin − startSpin) × B`, B = y + y² − y³ con y = u/6.
    - Muriendo: `dieTime += dt`; pasado 1,5 × 1,5 = 2,25 s se borra; si no, A × (1 − clamp(dieTime/1,5)).
    - escala = start + (final − start) × A; `bob = fmod(bob + 1,3 dt, 2π)`.
    - Altura (+0x1C) = `shieldHeight + raiseWithScale × finalScale + (sin(bob) + 1) × bobMagnitude × escala × 0,5`:
      **el bamboleo va con la escala nueva** (el informe decía A); con los datos 0, -2 y 3 el escudo está entre -1,36 y
      +0,68 m respecto del suelo a tamaño completo.
    - `SetScale(escala)` solo si cambió respecto del turno anterior y **difiere más de 0,3 (double 0x900C70) de
      `GetScale`**: la escala de las colisiones va a saltos detrás de la dibujada (0,68 → 0,276 al encogerse, luego
      0,597 al crecer, para r = 40).
    - La matriz: identidad × escala girada por el ángulo en Y (filas 0 y 2), en `(x, suelo + altura, z)`.
  - `DrawShield` 0x72CED0 (cada fotograma, por fn_0072BF50): escala y matriz interpoladas entre el turno anterior y
    este por la fracción del turno; alfa (+0x70) = `max(40, min(1, fuerza) × 255)`; **solo se dibuja pasados 0,5 s**, con
    `SetGlobalAlpha(1)` y el tinte blanco (se ve el alfa de su propia textura: en openblack el componente `Alpha` = 1, o
    0 antes de 0,5 s). El alfa del PSys (+0x14 → +0x6C, `Effect::SetGlobalAlpha`) es el del escudo y, muriendo, × el
    recorte de dieTime/1,5 **que el código hace al revés**: 1 hasta 1,5 s y 0 después.
  - `SetDying` 0x72D170: muriendo y sin hechizo; se desvanece solo (encoge en 1,5 s y se borra a los 2,25 s).
  - Físicas: `InteractsWithPhysicsObjects` 1, `GetAlwaysRemainsInPhysicsInternalSystem` 1 (queda en el sistema aunque
    nada se mueva cerca: `PhysicsObjects` lo añade en cada `BeginTurn`), constantes de la fila 10, peso
    `GMapShieldInfo.weight` = 50000 × escala³ (`Object::GetWeight`). La malla de colisión es la submalla física de 554
    (28 vértices), a la escala del objeto.
  - `ReactToPhysicsImpact` 0x72D610: si quien le dio (`GetGameObjectWhoHitMe`, el que golpea) está disponible,
    `PhysicallyDestroysAbodes` y el hechizo tiene fuerza: `SpellEvent 5` en su MapCoords (x, z, su altura sobre el
    suelo) sin objetivo, `PayFor(|v| × masa × chantCostPerImpactMomentum (25) × 0,0001, forzado)` con la velocidad y
    masa del que golpea, `Town::UpdateAggressor` (no portado) y la reacción de golpe o de destrucción.
- `IsPointDefinietlyWithinShieldVolume` (vt 0x870): MagicShield 0x72B850 es una esfera 3D de radio la magnitud del
  hechizo; PhysicalShield 0x72B8E0 un cono de su `Get2DRadius` R: `d² < R²` (x, z) y **la altura del punto sobre el
  suelo (MapCoords +8, R14: relativa)** menor que `H (1 − d/R)`, con H = `GetHeight` 0x638120 (malla +0x28 × escala ×
  2). `fn_0072B990` (`Reaction::ApplyReactionToLivingObjectsAtSquare`): un vivo a menos del 2D radius de un escudo
  ignora una reacción cuyo origen no está dentro de él → `map_shield::IsReactionBlockedByShield(vivo, origen)`, para
  los repartos de reacciones de los aldeanos (el de fuego tiene su `TODO(M6)` en `VillagerFire.cpp`).

### Las partículas (`PSys/Rules/Shield.cpp`)

- **Registro `DefensiveShield`** (lista 0xD4EE48, `PSysShield.cpp`): `UR_AddDefensiveSphere` 0x6A2A60 crea la primera
  vez una `DefensiveSphere` (fn_006D0B20) en `GetCurrentParentPos` (el origen del efecto, en SF_DefenseSphere) con el
  radio del proveedor (magnitud × 1,11062); después solo sigue el radio. Se borra con su colección (el `~Effect`).
- **API para las bolas de fuego (lane M5)**, en `PSys/Rules/Shield.h`, espacio `psys::shields`:
  `DoAnyShieldDeflections(effect, atom, oldGlobal)` es `UpdateRuleGravityWithFloor::DoAnyShieldDeflections` 0x6A1FA0.
  El original lo llama **al final de la actualización de cada átomo** (0x6A1F48) solo con `CheckShieldDeflections`
  (+0x72), con `oldGlobal` = la posición global del átomo **al empezar su actualización** (0x6A1903). Margen =
  1,25 × baseScale × ruleScale; si el átomo cruzó hacia dentro de una esfera: la intersección (`FindIntersect`
  0x57CCD0, el radio + margen), una chispa (`AddImpactTarget`, fn_006D0AF0), y un `SpellEvent 4 {impacto, el último
  movimiento global, 1, target = el hechizo del escudo}` al propio efecto del átomo; con 0 el átomo queda en el
  impacto (`GlobalToLocal`) y su velocidad rebotada (`v −= 2(v·n)n`, 0x57CFD0) y devuelve true; con 1 pasa. En el
  hechizo, `ApplyDefaultSpellEffect` → `SpellHitSpell`: el escudo paga fuerza × `costPerShieldCollide` (FIRE 600/800/
  1000) y la bola un evento; si el escudo sigue con fuerza, reacción de golpe y 0 (rebota). También hay
  `FindShieldContainingPoint`, `FindShieldCrossedInto`, `IsPointInShield` (estricto `< (r + m)²`),
  `HasCrossedIntoShield` y `SpellOf`. `UR_CloudMoverNew` (0x6D46D9) también llama a la función.
- `ApplyDefaultSpellEffect` con `event +0x20` (`checkShields`, 0x720D19): con una esfera que contenga el punto (margen =
  el radio del efecto) se manda a sí mismo un evento 4 con él mismo de objetivo; con 0 se corta el evento, si no, chispa
  en el punto. Portado tal cual (la intención no está clara).
- `SpellTargets` guarda también **puntos** (12 bytes, +0x14): `Effect::AddTargetPoint` / `TakeTargetPoint`
  (fn_00670F00 toma primero un punto y si no un objeto, su posición).
- `UpdateRuleShieldSpark` 0x6A2BF0 (una AtomCreateRule): por cada impacto, mientras haya menos de
  `MaxNumAtomsForCollection`, un átomo en el impacto con su `SoundSpark`, tres ángulos al azar y un número de ondas
  2..5; los de más de `SparkLife` se borran. `ModifySubCollection` 0x6A2F90 dibuja con sus N átomos **un arco desde el
  centro de la esfera hasta el impacto**: el átomo i (de N a 1) en u = i/N a u R a lo largo de la dirección del impacto,
  desviado R × WiggleAmpl × sin(k u π) sin(4t + a3) y sin((k+1) u π) sin(2t + a3); escala 0,04 R (1,5 + 0,4 (1 +
  sin(a3 − 6t))), alfa (1 − t/SparkLife) × 510 entre 0 y 255.
- `UR_InitialSpin` 0x69E490: cuando el efecto tiene jugador, giro = curl (manager +0x58 = PSysProcessInfo +0x34) ×
  ScaleAngularVelocity entre ±MaxAngularVelocity, que se apaga en TimeToFade; gira el átomo en su Y.
- `UR_VapourEndEffect` 0x6A39E0: el parche de superficie se pone en su padre (el trazador de la esfera), orientado la
  primera vez con su Y hacia fuera y después girado lo que se movió el padre (eje último × actual, ángulo acos);
  escala = ScaleFactor, **alfa = edad × 30** (plena a los 8,5 s).
- `SetCollectionAlpha` 0x6A2720: alfa de la colección = clamp(proveedor, 0, 255) (en la cúpula, la fuerza × 255, 40..255).
- `UR_AtomsAtEPTarget` 0x69A960: toma un objetivo objeto la primera vez y hace un átomo por punto extra de su malla (5
  si no tiene; MSH_S_SOLID_SHIELD no tiene, así que los cinco van a la posición del objeto); si el objeto se va, borra
  sus átomos.
- `CheckShieldDeflections` 0x6A2570 (regla de átomo, ningún archivo la usa) portada salvo `MoveToBaseGroup`.
- **`UR_SphereSurfaceTracer` 0x6A32B0 corregido** (PSys.cpp): la propiedad del radio es `ScaleSphereRadius` (no
  `SphereRadiusFP`, que no existe: la cúpula salía de 1 m), los ángulos con `fmod 2π`, el alfa `Alpha × ScaleAlpha`
  y, fuera de jerarquía, + la posición del padre.

### Corrección en el núcleo del PSys: las jerarquías

- En el original (ctor de `AtomCollection` 0x6748B0, `LocalToGlobal` 0x6751D0 / fn_006752D0, fn_00673DB0,
  `CommonInitNewAtom` 0x674C60): una colección está en jerarquía si **cualquier** antepasado es de un grupo marcado en
  `Hierarchies` (no solo el padre); su marco es el producto de las matrices locales de **los átomos antepasados cuyo
  grupo está marcado** (los demás no cuentan), y esa matriz lleva la **escala** del átomo (baseScale × ruleScale, la Y
  × el stretch). Un átomo nuevo nace en 0 si su padre es de grupo marcado y si no en la posición del padre.
- Portado en `Effect::CreateCollection`, `LocalToGlobal` / `GlobalToLocal` (nuevos), `GlobalPosition`,
  `SpawnPosition` y `PostUpdate` (el marco y su escala bajan por los átomos no marcados; la escala dibujada de un átomo
  en jerarquía también se multiplica). La cúpula lo necesita: los parches (grupo 2) y las chispas (grupo 4) están en el
  marco de la raíz, que crece de 0,1 a 1 en 2 s (`UR_ChangeScale`) y encoge al cerrarse. Cambia también, hacia el
  original, SF_Bonfire, SF_Smoke/Steam (raíz 0,8) y los efectos en la mano y sobre el pedestal que escalan su raíz
  (`SetScale`). Ninguna regla con `GlobalToLocal` propio (HandFollow, Fireball) está en una colección afectada.

### Ganchos y capturas

- `OPENBLACK_TEST_SPELL` admite `curl` (el séptimo valor); `OPENBLACK_TEST_SHIELD_SHOT` captura por turnos
  ([openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración)).
- `test_shield`: recorte, coste, curvas, ayudantes de esfera, registro y desvío con un efecto real, marco de jerarquía y,
  con `OPENBLACK_GAME_PATH`, las filas reales (5 / 1000 / 30, 25, 0 / -2 / 3, 20 y 22, peso 50000).
- Capturas en `dev\_audit\magic\`: `m6s_shield_dome.png` (SHIELD r 40 en el almacén de Land1, desde arriba: la cúpula
  translúcida de parches MSH_S_SPELLBALLSURFACE02), `m6s_shield_dome_side.png`, `m6s_dome_t10/t30/t100.png` (1, 3 y
  10 s); `m6s_phys_t03/t07/t10/t13/t25.png` (PHYSICAL_SHIELD r 40: oculto a 0,3 s, 0,7, 1,0 (A 0,41), 1,3 (0,67) y
  2,5 s, completo) y `m6s_physical_full.png`; `m6s_physical_rock_slow.log` (una roca a 12 m/s rebota: dos golpes,
  momentos 8191 y 16115, paga 20,5 y 40,3 cánticos; el evento 5 cae en la roca), `m6s_physical_rock.log` (a 33 m/s
  la detecta, 166,6 cánticos, pero **atraviesa** la cáscara fina: el contacto de `PhysOb` no la para; sin verificar si
  el original la para), `m6s_physical_close2.log` (duración 3 s: se cierra en el turno 31, encoge en 1,5 s y se borra
  en el 54 con el hechizo) y `m6s_magic_close2.log` (el PSys se apaga 2,2 s y el hechizo se borra en el 54).

### Sin portar / UNVERIFIED

- Las reacciones de los aldeanos al escudo (`ReactToMagicShieldPriority` 0x765BB0, `SetupReactToMagicShield`
  0x765C60, las de golpe y destrucción) y de la criatura (`CreatureMustAvoid` 0x72C170, la ruta 0x54AF60): solo están los
  datos de las reacciones; `GetImpressiveValue` 0x72BA80 / 0x72D7F0; `Town::UpdateAggressor`.
- Los escudos no están en la rejilla de objetos del mapa: `ApplyEffectToMapPos` no los alcanza (el físico admitiría
  efectos sin quemar, `IsEffectReceiver` 0x72CC80; el mágico ninguno).
- `CallVirtualFunctionsForCreation` (los enlaces de caminos y `fn_0057E220` (5, 0xD) / (4, 0xD) del objeto 3D).
- El dibujo de los parches de la cúpula es el de `Creators/Mesh.cpp` (M3): sin color del jugador (`UsePlayerColor`,
  mezcla 0,5), sin `DrawCutByPlane` y sin aditivo (que solo vale con `MeshChangeMaterialProps`), así que la cúpula se ve
  tenue. `OrientToSurface` del trazador (lo usan SF_DefenseSphereInHand/OnHolder) y `MoveToBaseGroup` no están.
- Los puntos extra de las mallas no se cargan (`UR_AtomsAtEPTarget` usa la posición del objeto, exacto para 554).
- `Get2DRadius` / `GetHeight` salen de la caja de la malla (la cabecera de 554 la trae a cero; el original lee campos
  del LH3DMesh que se calculan al cargar, sin comprobar).
- La bola de fuego aún no llama a `DoAnyShieldDeflections` (es de la lane M5; su `TODO(M6)` en `Fireball.cpp`).

## Revisión de la ola 2 (lane «review2»: M2, M3, M5, M6a, M7 juntos)

Comprobado contra el ejecutable (`dev\tmp_dis\miracles\impl\review2\`) y con las cadenas completas en el juego.

### Fórmulas releídas en el exe (coinciden)

- Gestos: `MatchForward` 0x57A1A0 (giros alineados desde cualquier punto de partida, absorción de esquinas pequeñas,
  error envuelto con fn_0057A150, las dos constantes de `crt_xc` 0x579DC0/0x579DF0 = 3π/32 × 7/4 y × 2).
- Lanzar: `SpellSeed::ApplyThisToMapCoord` 0x728E20, `Cast` 0x729520, `DoPreCastThings` 0x729460 (la rama «semilla de
  tipo 2 → magnitud 1» mira `GMagicInfo +0x28`, que es −1 en todas las filas: muerta) y `SendApplyToMapCoord` 0x5D3340.
- `Pot::AddResourceToPos` 0x66F270: la espiral de 9 celdas, primero la lista +4 y luego la +0, `IsCloseToEqual` con
  `Get2DRadius × GetRadiusMultiplierForApplyingPotToPos`, envenenado en arg5 y aceleración en arg6.
- Calor: fn_0072F980 (inmune el bombero, radio, la comprobación de altura solo si alguno de los dos está a 3 m o más
  sobre el suelo, `min(10·ΔT, 0,5·calor de la fuente)`, la fuente pierde calor si no arde, el grupo, `SetupOnFire` si el
  aldeano no está en el estado 219).
- `UpdateRuleGravityWithFloor` 0x6A1880 (la gravedad en el aire `clamp(v.y + MaxSpeed, 0, 1) × g × gravedad del átomo
  × dt` y la vuelta al suelo).
- `GWeather::CalcAtmos` 0x8400E0 (caja, radio², caída entre los dos radios, `ftol(f × fundido × 256)`, temperatura con
  suma de byte que da la vuelta y los otros cinco bytes con saturación).
- La batería del lugar de culto (fn_0077B6A0: intensidad `usado/capacidad + max(0,2; 0,5 − batería/máx × 0,5)` hasta
  1, `batería − (usado − producido)` sin tope superior, disponible = batería + capacidad) y el toque de un icono
  (`SpellIcon::InterfaceTap` 0x726430 → `ActualInterfaceTap` 0x77F880).

### Arreglado en la revisión

- `Pot::AddResourceToPos` devuelve `cantidad − lo que quedó` en todos los caminos (0x66F511), también cuando hace una
  pila nueva; antes devolvía la cantidad entera. Solo lo usaba el registro de la mano.
- `MapCoords::IsWater` 0x6035B0 responde **1** fuera del mapa y donde no hay bloque de tierra (0x603617); la copia de
  `ECS/PotResource.cpp` respondía 0, así que la comida o la madera echadas sobre mar abierto sin bloque hacían una pila.
- La semilla de un uso se ata al mejor icono del jugador (`CreateSpellIntoHand` 0x72A730 → fn_007282A0), como arriba.
- La bola de un uso tiene su `SpellSeedGraphic` dentro (0x72A450) y se borra con ella.

### Pregunta abierta: el tamaño de la bola de fuego lanzada con la mano

`Spell::InitWithPos` 0x71FE50 da al PSys la magnitud `SpellCastData[0]` sin comprobar si es 0, y ese valor es el
tamaño del paquete de gesto de la interfaz (`m_Gesture` +0x1B8, fn_0071FA10). **Solo lo escribe el círculo** (la rama de
los gestos de tamaño de `ProcessPowerUpSystem`, 0x5CF559, único llamador de 0x57A5E0, y la copia del círculo pendiente
en 0x5D33BA); la selección por gesto y los demás reconocimientos no lo tocan, y el `GInterface` nace a cero
(`Base::operator new` 0x4366F0 borra la memoria). `SF_FireBallThrow` pone la escala del átomo con
`MagnitudeFloatProvider` (mín 0,01, máx 10, `UpdateParams` 0x69DA90) y `SetScale` 0x6A2700 (+0x78), y de esa escala
salen el radio y la capacidad de calor de la `MagicFireBall`. Así, en el port igual que (según lo leído) en el
original, una bola de fuego lanzada sin haber dibujado nunca un círculo sale con escala 0,01 (unos 4 cm, casi no
calienta: `review2_disp_fireball_t33.png`, `review2_land2_cast.png`), y tras un círculo de escudo o tormenta sale con el
tamaño de ese círculo (hasta 10, 40 m). Con `SPELL_AT_POS` la magnitud es el radio del guion (10 en
`m5_fireball.png`). No cuadra con cómo se recuerda el juego: queda **UNVERIFIED** si hay otro escritor que no se ve
(p. ej. una copia de la estructura entera) antes de cambiar nada.

### Cadenas probadas en el juego (capturas en `dev\_audit\magic\`)

- Land1, dispensador → bola → semilla → lanzar: `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1812,2652,1"`,
  `OPENBLACK_TEST_TAP="1812,2652,200"`, `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"`,
  `OPENBLACK_TEST_THROW_VEL`: la bola da la semilla FIRE lista (3500 cánticos), se arma (estado 8) y al soltar sale el
  hechizo con su `MagicFireBall` (T 6000, fuego 2 en el granero 52, que no llega a prender con una bola de 0,01).
  `review2_disp_fireball.log`.
- Land1, comida junto al almacén: el mismo dispensador con `FOOD`: dentro del radio del almacén (18,5 m) todo entra en
  él (`review2_disp_food.log`); un poco más allá (`review2_disp_food_pour.png`) hace una `MagicFood` de 200 que crece
  18 por grano, con la mano alzada 16 m y el chorro de 4 s.
- Land2, icono → carga → semilla → lanzar: `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE"`, `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="FIRE,200"` y
  `OPENBLACK_TEST_CAST`: `seed 3120 of icon 3082 in the hand with 3500 chants`, armada y lanzada
  (`review2_land2_cast.png`, `review2_land2_seed.log`). Con 3000 cánticos el icono se queda cargando con la batería a 0
  (`review2_land2_charge.log`). Una bola de un uso tocada con cánticos en el lugar sale atada al icono 3081
  (`review2_land2_oneshot_icon.log`).
