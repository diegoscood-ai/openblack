# Magia: el núcleo de los milagros

Núcleo del sistema de magia del original y de su port: las tablas de info.dat, el ciclo de vida de los hechizos, los
cánticos, los eventos y efectos, las reglas de lanzamiento, las semillas y los milagros de un uso, lanzar desde la mano
y los gestos, el culto y el poder de oración, la influencia, la alineación, las reacciones, la vida, el modelo del
fuego, el orden en el turno y los ganchos. Cada milagro tiene su sección en [miracles.md](miracles.md); el motor de
partículas (PSys) está en [particles.md](particles.md), y el tiempo y el clima en
[day-night-weather.md](day-night-weather.md#tiempo-y-clima-m6a-srcecsweather).

Plan e informes: `dev\tmp_dis\miracles\` (`PLAN.md` y los informes que cita: `core.md`, `casting.md`, `sources.md`,
`destructive.md`, `resources.md`, `protect_creature.md`, `visuals_sound.md`). Direcciones W120. Esta página recoge lo
verificado en `runblack.exe` al portarlo, por hitos.

- [Tablas de info.dat](#tablas-de-infodat-m0-srcmagicmagictables)
- [Núcleo de los hechizos](#núcleo-de-los-hechizos-m1-srcmagiccore-srcmagicspells-srcecseffects)
- [Lanzar desde la mano, gestos y efectos de la mano](#lanzar-desde-la-mano-gestos-y-efectos-de-la-mano-m2-srcmagicgestures-srcmagichand-handspellseedcpp)
- [Culto: de dónde salen los milagros](#culto-de-dónde-salen-los-milagros-m7-srcworship-ecssystemsimplementationsvillagerworship)
- [Influencia](#influencia-m1i-srcecsinfluence)
- [Alineación del jugador](#alineación-del-jugador-galignment-gplayer-0x60-srcecseffectsalignment-componentsplayeralignment)
- [Reacciones](#reacciones-ecseffectsreactions)
- [Vida de los objetos](#vida-de-los-objetos-m0-srcecslife)
- [Fuego](#fuego-m5-srcecsfire)
- [Tiempo y clima](#tiempo-y-clima)
- [Milagros uno a uno](#milagros-uno-a-uno)
- [Revisión de la ola 2](#revisión-de-la-ola-2-lane-review2-m2-m3-m5-m6a-m7-juntos)
- [Suposiciones auditadas](#suposiciones-auditadas-2026-10-01)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

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
- `GAlignment::Update` 0x414410, el cambio de alineación que dejan los efectos: en
  [Los efectos de los hechizos](magic.md#los-efectos-de-los-hechizos-galignmentupdate-0x414410).
- Las reacciones (`CreateReaction` 0x6E3D70, `SpreadReaction` 0x6E3E10): en [Reacciones](magic.md#reacciones-ecseffectsreactions).

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
  - **La bola es aditiva (fiel, corregido el 2026-10-01 con la captura del original).** La malla tiene una submalla
    física (`Smooth`, no se dibuja) y la visible, el casquete, una primitiva `AlphaTextured` (tipo 4, byte +5 = 5: dos
    caras y repetición) cuya piel 0xF49809BD ARGB4444 es una bola turquesa oscura (51, 119, 136) con un brillo blanco
    arriba a la izquierda, casi opaca (alfa 13-15 de 15, o 0 fuera).
    - Pero el archivo no manda: `CallVirtualFunctionsForCreation` 0x72A450 la carga con
      `GJUtils::GetSharedMesh` 0x57DFB0 y `MaterialProperties` {1, 1, 0, 1, 1} (bytes en 0x72A474..0x72A485). El byte
      +3 = 1 hace que `PGetSharedMesh` (0x57DF18) llame a fn_0057E1D0, que pasa `GJUtils::SetMaterialProperties`
      0x57E120 a todas las primitivas al cargar la malla:
      - tipo 4 → 6; si +4 = 0 → 3; si +0 (aditivo) = 1 → 13; si +1 (escribe Z) = 1: 6→5, 13→12, 8→3, 16→9; si no:
        5→6, 12→13, 2 o 3→8, 9→16;
      - +2 (dos caras) pone o quita el bit 0 del byte +5.
      - Para la bola: **modo 12** (`fn_0082EB50`: `SRCALPHA / ONE`, color y alfa = textura × difuso, escribe Z) y **una
        sola cara** (byte +5 = 4).
    - `Draw` 0x518E90 tiñe el objeto con `0x96FFFFFF` (byte de [0xBE8E8C]; fn_0080BF10 multiplica el difuso: alfa
      0xFF × 0x96 >> 8 = 0x95) y llama a `SetGlobalAlpha(1)` (LH3DObject vt 0x48, bit 0x80 de las banderas), que pasa a
      la tabla de modos alternativa 0xC387C8. Esa tabla **deja igual** los modos aditivos 10-13 (leída del ejecutable).
    - Resultado: la bola **suma** a lo que tiene detrás su textura × luz × (0,58 × alfa de la textura). Sobre la
      arena de día sale casi blanca y nacarada: la textura turquesa se vuelve celeste y el brillo, blanco saturado.
      El fondo se ve a través con tonos verdes y rosas.
    - Antes openblack la mezclaba como modo 5 (`SRCALPHA / INVSRCALPHA`, dos caras). Eso tapaba la mitad del fondo con
      el turquesa oscuro: una bola verdosa y oscura. La investigación anterior (luz N·L, ambiente 90/256, alfa 0x95)
      era correcta, pero se le escapó este cambio de material al cargar.
    - openblack:
      - `graphics::MaterialProperties` y `L3DSubMesh::SetMaterialProperties` (el cambio de tipo de 0x57E120, con el
        tipo guardado en `Primitive::materialType`) y `L3DMesh::SetMaterialProperties` (fn_0057E1D0), en
        `src/3D/L3DSubMesh.*` y `L3DMesh.h`;
      - `Game.cpp` lo aplica a `O_Bibble_up` al cargarla;
      - `Renderer::DrawSubMesh`: un objeto con `components::Alpha` (la tabla 0xC387C8) conserva la mezcla aditiva de sus
        primitivas aditivas, y sin escribir Z las de los modos 11 y 13.
    - Capturas: `dev\_audit\magic\orbref_a.png` (antes), `orbref_b.png` y `orbref_c.png` (después), y la comparación
      `dev\_audit\magic\ref\orb_compare.png` con la captura del original del usuario (`ref\dispenser_original.png`).
    - Diferencias que quedan con esa captura, **pendientes**:
      - en el original la bola flota más alta sobre el dispensador y se ve más grande;
      - en openblack el efecto de la semilla de FUEGO se ve como un núcleo amarillo dentro de la bola, y en el
        original no se ve (en su centro hay una mancha celeste);
      - la arena del original es más clara, y como la bola es aditiva el fondo cambia mucho su aspecto.
    - Para ordenarla en el Z-sorter, `Draw` adelanta su posición hacia la cámara su radio (vt 0x60) y luego la
      restaura. Así la bola se pinta después de la semilla de dentro. `DrawSpellGraphic` recibe como alfa el byte alto
      del difuso (0x95). openblack: `components::Alpha` = 149/255 en `OneOffSpellSeedArchetype` (pasada `MainBlended`).
      El adelanto por el radio sí está (lane «seed»): `one_off::UpdateFrames` guarda `OneOffSpellSeed::sortPoint` = centro de
      la caja + normalize(cámara − centro) × radio (`Get2DRadius`: media extensión mayor en x/z × escala, 2,3 m) y
      `RenderingSystem` / `Renderer` ordenan la bola por ese punto (`RenderContext::sortPoints`).
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
- **Con la mano de verdad** (fiel, lane «grab», `HandSystem.cpp` / `HandPlacement.cpp`):
  - El objeto bajo el cursor (`SendObjectDrawCollision` 0x5D56C0, triángulo exacto) llega a `ActionPressed`
    fn_005D1330 → `StartGrab` 0x5D1740 si `ValidForPlaceInHand` (vt 0x6FC) o `InterfaceValidToTap` (vt 0x740). La bola
    tiene las dos: es un `MobileObject` (`Mobile::ValidForPlaceInHand` 0x425B00 = 1) y `InterfaceValidToTap` 0x72A630 = 1.
  - Pulsar sobre ella empieza el agarre (estado 13). Si se suelta antes de 225 ms (`State_Grab` 0x5D5250, 0xE1) es un
    **toque**: `Tap` 0x5D3930 → 0x5D38A0 → paquete 0x20 → 0x5DA650 → `InterfaceTap`, y la semilla cargada pasa a la mano.
  - Si se mantiene pulsado, **se coge la bola misma**: `GenericPickup` 0x5D2800 (paquete 0x13) → `PlaceObjectInMagicHand`
    → `InterfaceSetInMagicHand` 0x72A530 (solo marca la magia como habilitada). Se lleva como un `MobileObject`
    (`GetHoldType` 0x607120 = 6, `Object::GetHoldRadius` 0x638C00) y se suelta o se lanza con física: constantes 9
    (`GetPhysicsConstantsType` 0x72A920) y la info `GMobileObjectInfo` 25 (0xD39F3C; **(inferido)** que sea la 25, por
    el paso 0x114 desde la de WHALE). El dispensador ya no la ve en su sitio y hace otra al recargar.
  - El toque y el agarre piden la mano dentro de la influencia del jugador (`InterfaceMustBeInInfluenceForInteraction`
    0x4028A0 = 1; `m_InInfluence` de fn_005D1120, tipo 1). Fuera de ella no pasa nada.
  - Volumen de selección: la malla tal como se dibuja, girada hacia la cámara (fn_00518720), así que vale la cúpula
    que se ve desde cualquier lado. **(inferido)**: si el rayo da en la semilla de dentro (`SpellSeedGraphic`, que no es
    un `Object`), cuenta como si diera en su bola o icono.
  - Los iconos de los lugares de culto y de los centros de pueblo (`Object::ValidForPlaceInHand` 0x402870 = 0) se tocan
    al pulsar (`StartGrab` → `Tap` al momento), con la misma regla de influencia.
  - **Pendiente**: el texto de ayuda al pasar por encima (fn_005D6D70: en una bola, `GetOverwritePickUpToolTip` 0x72AC50
    = el texto de su magia +0x110; el de tocar es 0xEF7). Tampoco están `GInterface::StartImmersion(0xE)` ni el registro
    `GameThingClicked` de fn_005D36D0.
  - Gancho: `OPENBLACK_MOUSE_AT=0.5,0.5 OPENBLACK_CAMERA_LOCK=1948,40,2550,1939.2,33,2537.7` con
    `--mod test.miracle-dispensers` (la bola de FUEGO en el centro). Toque: `OPENBLACK_TEST_CAST="press@5,release@5.1"`.
    Coger la bola: `"press@5,release@5.6"`. Dejarla: añadir `",press@7,release@7.2"`. Capturas `grab_tap.png`,
    `grab_hold.png` y `grab_drop.png` en `dev\_audit\magic`.
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

Cómo el hechizo es dueño de su efecto y lo avanza (`StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, el
evento 3 de `LandscapeCollide`): en [PSys enlazado al hechizo](particles.md#psys-enlazado-al-hechizo-psysspelllinkh).

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
  - **Matriz** (`Band::Draw` 0x68D8BB..0x68D9EA): la local es 10·I con la traslación (0, 0, +0x18 + índice·+0x1C)
    (0x68D900..0x68D909), es decir, a lo largo del **eje Z propio del hueso raíz** (el antebrazo). Solo cuando ha
    llegado (f ≥ 1, 0x68D90D) cada fila gira su (x, y) por el ángulo +0x20 alrededor de esa Z (0x68D922..0x68D9DB:
    (x, y) → (c x + s y, c y − s x), c guardado como float en 0x68D929, s en la pila; `lh_matrix::TurnRows(2)`).
    Después fn_007FAFF0 0x68D9EA = local × hueso (filas; en glm hueso · local). El hueso son los 0x30 primeros bytes
    de la matriz apuntada por CHand +0x47F0 (copiados en 0x68D0F2..0x68D100; `PrepareForDrawing` 0x46CAE5 copia la
    misma en la matriz del objeto de la mano). Resultado: una pulsera que rodea la muñeca y gira sobre el eje del
    antebrazo. openblack lo tenía a lo largo de la Y y girando sobre la Y (el anillo colgaba bajo la mano y daba
    vueltas de canto); corregido (`DrawBand`, capturas `_audit/magic/wristring_{before,after}_1500{0,1}.png`).
  - Permanentes: `SetPULevel(pu + 1, 1)` desde `SpellSeed::SetPowerUp` 0x729BFC..0x729BFE (pu = POWER_UP_TYPE: −1 sin
    power-up, 0 = PU1, 1 = PU2), así que 0 / 1 / 2 anillos (máximo 5); empiezan a los 2,4 s; alfa 20→130 en 0,85 s,
    con lerp de matrices. Vuelan desde delante de la cámara hasta el hueso raíz de la mano (la muñeca).
  - Color (`Band::Draw` 0x68D849..0x68D8B1, en cada dibujo): +0x4C = `GetPlayerColour` 0x64D800 del jugador local
    (g_game +0x205A59) con el alfa de la banda; +0x50 = lerp por canal de los colores +0x34 / +0x38 del ctor
    (fn_0068CA30, args 8 y 9), 0 en todos los llamantes. Un solo dibujo por banda (0x68DD46 vt+0x104). Recuerdo del
    usuario: un anillo rojo translúcido llega a la muñeca al coger un milagro (el exe lo confirma: rojo del jugador 1).
    openblack: `components::ObjectColour`.
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
  el `ParticleChainCreator` de M5 (`Graphics/RendererChain.cpp`). Color: fn_00671110 la crea con
  `PSysInterface::Create` y le hace `SetPlayer` (vt 0x20) del jugador local (g_game +0x205A59, 0x671172..0x671197);
  `ParticleChainCreator0` de `SF_GestureChain` tiene `UsePlayerColor 1` (blanco 255 × el color del jugador, alfa 10),
  así que la estela sale en el color del jugador (rojo para el 1). Lo mismo hace fn_00671260 con PT 35 (0x6712CD..
  0x6712EA); la selección (fn_006711D0) no recibe jugador.
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
  - La red de ruido: 256 × `1 − GameFloatRand(2)` (fn_00590DF0) desde la semilla 0 (inferido: antes de
    `GGame::Init`), los mismos valores en cada partida (`PSys/Noise.cpp`, game_random).
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
- **Lo que lee el audio** (`GGuidance::CheckWorshipSiteDesiresSFX` 0x71B270). Recorre `GPlayer+0xA48` →
  `Citadel+0x34..+0x48` en orden de hueco (`citadel::WorshipSitesOf`). Se salta los lugares sin bailarines: fn_0077B960
  salta a 0x77CFB0, que da `Dance+0x90` o 0 sin baile (`site::DancerCount`). De los demás se queda con el más cercano
  a la cámara, a menos de 200 m (0x980130). Luego pide su `CalculateDesireForFood` (vt+0x420 de `??_7WorshipSite`
  0x8F2840 = 0x77C310; `site::CalculateDesireForFood`), que vale `1 − min((comida + 0,0001) / (necesaria + 0,0001), 1)`.
  - La comida es la de la olla del lugar (+0xB4, `Pot::JustGetResource` 0x66D390).
  - La necesaria sale de `Dance::CalculateFoodNeededByDancers` 0x50BF20: la suma, por bailarín, de
    `(1 − comida en la barriga +0xE8) × foodReqiredForDinner` (+0x2D8).
  - Lee también `Citadel+0x70`, la fracción del sonido de tensión del culto, limitada a 1 en 0x71B31C
    (`citadel::StrainSoundFractionAtMostOne`). Solo la escribe `SetWorshipStrainSoundFrac` 0x463850 (desde
    `ProcessSpellIcons` 0x46396C) y se guarda y carga con la partida (0x463D6A / 0x463FB9).
  - **(aproximado)** openblack suma los bailarines en el orden en que se unieron, no grupo a grupo; solo cambia el
    redondeo.
  - **(inferido)** El valor inicial de +0x70 es 0: no se ha leído el constructor de Citadel.

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
  el nivel de mejora más alto que el jugador tiene habilitado y pone +0x58 = 0,5. **+0x58 no es un alfa**: solo lo lee
  `DrawSpellGraphic` 0x51A712 como tamaño de la banda (0,2 × +0x58 × escala). La semilla del icono se pinta opaca
  (el icono pasa alfa 0xFF). Antes openblack la pintaba a medias: corregido.

### SpellSeedGraphic: la semilla que flota en la bola y en los iconos (`Worship/SpellSeedGraphic.cpp`, fiel salvo lo marcado)

Objeto de `SpellIcon.cpp` (no es un `Object`; lista 0xD9D3D0). Campos: +0x14 MapCoords de la malla, +0x2C la malla
(Game3DObject), +0x30 la banda, +0x34/+0x38 fases de las fiolas, +0x3C ángulo y, +0x40/+0x44 ángulos de la banda,
+0x48 semilla, +0x50 PSys de soporte, +0x54 escala, +0x58 tamaño de la banda, +0x5C auto-update, +0x60 PU, +0x64 el
punto dado. Fila de semilla = 0xD9D678 + tipo × 0x190 (offsets de memoria = fichero + 0x10).

- `Create` 0x726F60 → fn_00727190: la malla `GSpellSeedInfo.mesh` (+0x130 del fichero) y `ReplaceMeshGivenSeedType`
  0x728450 (tabla 0x72854C por semilla − 3): FLYING_FLOCK pone la malla 1 (AnimalBat1) si la alineación del jugador
  (GPlayer+0x60 → +8) < `alignmentSwitch` (fn_00723140), si no la 11 (AnimalSpellDove), y fn_00727440 lo rehace cada
  30 turnos (`g_game +0x205A40 % 0x1E` en fn_00727350; openblack usa `Game::GetTurn`, **(inferido)** que ese campo
  sea el contador de turnos); FOOD y las fiolas de criatura llevan el envmap 0 (`envmap.raw`) y BEAM_EXPLOSION propiedades
  {1,0,1,1,0}: **no portado** (openblack no tiene envmap por objeto). El PSys de soporte (+0x164 del fichero) se crea
  en el punto + `unknown0x154` × escala con magnitud = escala; la banda (`CreatePUBand` 0x727080) si pu ≠ −1.
- fn_007270E0: +0x64 = punto, malla en punto + `unknown0x150` × escala (−1,5 casi siempre: las mallas I_* tienen el
  origen abajo y ~3 m de alto, así quedan centradas), efecto en punto + `unknown0x154` × escala.
- La bola (`OneOffSpellSeed::Draw` 0x518E90), cada fotograma que se ve: `GetSpellGraphicPos` 0x72A840 = la matriz
  dibujada aplicada al punto de malla `ResolveLoad()+0x18` (el centro de la caja, **(inferido)** por ser el punto en
  que gira fn_00518720) y escala = escala del objeto 3D × 0,6 ([0x8C7BDC]); `DrawUpdateAtPos` 0x727630 (+0x54 =
  escala, fn_007270E0, fn_007274D0: PSys a su punto, magnitud = escala, `Process_` con la info a cero, poder 1,
  activo) y `DrawSpellGraphic(bola, 0, 1, 0x95)`.
- Los iconos (`SpellIcon::Draw` 0x5198D2, `TownCentre::Draw` 0x5164D4 → `DrawSpellSeedGraphic` 0x726D30):
  `UpdateOnly(ms)` y `DrawSpellGraphic(icono, 0, 1, 0xFF)` (los dos tiñen el icono con 0xFFFFFFFF). La semilla queda
  donde la creó `Create3DSpellObject` (punto especial 0 + 1, escala 1).
- `DrawSpellGraphic` 0x519AD0 (leído entero en la parte de semillas del jugador):
  - solo si `useMesh` (+0x168 del fichero, fn_00727690) vale 1. **STORM, FIRE, LIGHTNING_BOLT, WATER y TELEPORT tienen
    0**: en la bola y en el icono solo se ve su efecto de soporte (LIGHTNING_STORM / FIREBALL / LIGHTNING_BOLT / WATER /
    TELEPORT_ON_HOLDER). openblack pintaba sus mallas (I_Lightning2, I_Blast, I_Lightning, el cuerno para el agua y
    el escudo para el teletransporte): eran los «iconos equivocados».
  - tamaño = `GSpellSeedInfo.scale` (+0x134) × +0x54; ángulo +0x3C += 2 rad/s × dt ([0x8D8700]), fmod 2π (double
    [0x8D45D8]); `SetPosition` 0x423140: filas X = (cos, 0, sin), Z = (−sin, 0, cos). **Sin bote ni pulso** para las
    semillas del jugador: `AsMagicCreatureSpellInfo` (vt 0x38) de su magia base es NULL y salta a 0x51A0B3. El bote
    (+0x38 a 0,35/0,5 por s, `0,5(1 + sin 2π f)`), los cuadros UV 8×4 a −15 por s (+0x34) y los aplastamientos
    0,7/0,8/1,5 del switch 0x519D76 (por GMagicCreatureSpellInfo+0x58) son de las fiolas 12..27. Portados solo los
    cuadros UV (0x519B79..0x519C1B, `frame_anim::SpellIconFrame`, ver
    [rendering-objects.md](rendering-objects.md#texturas-animadas-por-fotogramas)); el bote y los aplastamientos no.
  - alfa difuso = el del dueño (0x51A0B3..0x51A0E1) y `SetGlobalAlpha(alfa ≠ 0xFF)` (0x51A0EB), pero con arg 2 = 0
    `GetAltitudeAndSetColorSpecular` (0x51A187) reescribe todo +0x4C con tabla[luminosidad] (0x803409..0x803413) o
    tabla[255] (0x803365 / 0x8033DA), de alfa 0xFF (todo `palette.raw` tiene alfa 0xFF): en la bola la semilla va por
    la tabla 0xC387C8 con alfa 0xFF ([0xC37D8C], 0x80DEF8), **opaca** (no 0x95). openblack: `components::Alpha` = 1.
  - con arg 2 = 0 (todas las llamadas del mundo) `GetAltitudeAndSetColorSpecular` 0x803340 (0x51A187, en +0x14) pone
    la luz de la casilla en la malla, sin neblina después: el modo `land_light::ObjectMode::Cell` de `SpellIcon::Draw`
    (`SpellSeedGraphic::landCellLight`, `LandLightOf` de `RenderingSystem.cpp`). Las fiolas de criatura van por
    fn_00801C90 + fn_007FEB30 (0x519D90 / 0x519D9E), la luz de los modelos.
  - el PSys recibe el alfa: `GJPSysInterface::SetAlpha` 0x55ED50 (vt 0x12C) escribe el byte +0x6C del gestor;
    fn_00679860 0x679875 lo copia en [0xC0215C] y fn_00679920 0x679BC2..0x679BDF hace alfa del átomo × él >> 8 si no
    es 0xFF. En la bola (0x95) el efecto aditivo de la semilla suma 149/256 de su luz: sin eso (antes) el centro de la
    burbuja salía blanco quemado y tapaba el icono (`orbcolour_compare.png`). Luego se pinta tal como se dio el último
    paso.
  - la banda si pu ≠ −1: +0x44 += 10,3 × dt ([0xBE8E94]), +0x40 += dt; pu + 1 dibujos en +0x64 con tamaño
    0,2 × +0x58 × +0x54, filas: identidad con la fila 1 y la 2 cambiadas (la vieja 1 negada), giro (x, z) por base
    + +0x44, (x, y) por 0,3, (x, z) por k, (x, y) por 0,2; base, k = 0, −1 la primera y 0,5, 1 las demás. Después
    fn_0051A830 la gira hacia la cámara ([0xBE8E8E] = 1; `billboard::BandToEye`, ver
    [rendering-objects.md](rendering-objects.md#objetos-que-miran-a-la-cámara-billboards)).
  - **Color de la banda** (`SetColour` 0x7F9770 en 0x51A3BE: edx → +0x4C, el argumento → +0x50): +0x4C =
    `GetPlayerColour` 0x64D800 (tabla 0xBFF0B8 por `GetRemapedPlayer`) del dueño (vt 0x1C), o del jugador local
    (g_game +0x205A59) si el dueño es el neutral (g_game +0x205A5B) (0x51A322..0x51A36D); su rgb con alfa
    (+0x70 × alfa del llamante) >> 8 (0x51A397..0x51A3B9); +0x70 = 0x3C (fn_00726F10 0x726F4E, único escritor), así que
    en un icono (alfa 0xFF) el alfa es 59 y en la bola (0x95) 34. +0x50 (especular) = 0x141414 (byte [0xBE8EA0] = 20).
    Rojo para el jugador 1. openblack: `components::ObjectColour` (nuevo) + `Alpha`; **(aproximado)**: el especular no
    se pinta (la ruta de color de vs_object no lo tiene) y la luz del modelo (90 + 166 N·L) es la de los átomos de
    malla del PSys. **(inferido)**: el jugador local es PLAYER_ONE.
  - **Cada nivel se dibuja dos veces** con la misma matriz y color: 0x51A780 vt+0x104 y luego 0x51A7A3 vt+0x104 o, en
    el último nivel con arg 1 = 0 (todas las llamadas: iconos y bolas), 0x51A796 vt+0x100. El objeto es un
    `LH3DStaticObject` (LH3DObject::Create(0) 0x80B4F8, vtable 0x9A2974). vt+0x104 = fn_00815980: prueba de pantalla
    (CheckRegionOnScreen 0x868C80) y de distancia, luego dibuja ya (vt+0x108 = fn_0080DB30). vt+0x100 = fn_00815A70:
    la misma prueba, LOD por distancia (vt+0x1D0), apunta g_last_distance / g_last_selected_box y, si el objeto tiene
    el bit 0x10 de +4 (vt+0x44 = fn_007F97C0), lo mete en el Z-sorter (`NewZObject` 0x83F310 con fn_007FA980 → vt+0x108,
    clave = distancia² a la cámara, 0x815F0F..0x815F53); si no, dibuja ya. Ese bit lo pone `SetMesh` (vt+0xF4 =
    fn_007F9E10 → vt+0x40 = fn_007F97A0) cuando la malla tiene el bit 0x200 en sus flags (fn_007F9D40), y
    `Power_Up_Band.L3d` lo tiene (flags 0xA2200). Así que: todos los dibujos son inmediatos salvo el segundo del último
    nivel, que va ordenado con los transparentes (con el estado del objeto al vaciarse el sorter, que es el del último
    nivel: nada lo cambia después). Mismo material y mismo modo de cara en las dos pasadas (las dos acaban en
    fn_0080DB30): no hay pasada de caras traseras ni media banda. Aditivo, así que cada banda suma su luz dos veces.
    openblack: dos entidades por nivel (`k_DrawsPerBand`, `extraBands` = 2 (pu + 1) − 1). **(aproximado)**: el orden
    respecto a la burbuja (inmediatos antes, el del Z-sorter entre los transparentes) no se reproduce: las 2 (pu + 1)
    van en la pasada de translúcidos de openblack.
- openblack: `seed_graphic::DrawUpdateAtPos` / `UpdateOnly` / `DrawSpellGraphic` / `UpdateIconGraphics`;
  `one_off::UpdateFrames` (bola) y `worship::Update` (iconos) los llaman cada fotograma. **(inferido)**: también
  cuando no están en pantalla.
- Capturas (`dev\_audit\magic\`, `--mod test.miracle-dispensers` con `level=all`): `seed_<semilla>_a/_b.png` (dos
  cuadros, 10 fotogramas de diferencia) y `seed_grid1.png` / `seed_grid2.png` (recortes aclarados), y
  `seed_land2_icons_650/660.png` (iconos del lugar de culto de Land 2).
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
  (`TotemStatue::SetWorshipPercentage` 0x738270, que lo sube 8 m con un *Zoomer* de |Δ|·5200 ms, que va en ms:
  [engine-math.md](engine-math.md#zoomer-lh3dlib)) y se manda a los aldeanos
  que falten.
- `Town::GetWorshipersNeeded` 0x73C860: `objetivo = pct > 0 ? max(1; int(población × pct + 0,5)) : 0`;
  `resultado = objetivo − (adorando + en camino) + los que piden volver a casa`.
- `Town::AdjustWorshipersWorshipping` 0x73C0F0: dos pasadas (la segunda acepta también los marcados 0x200); para
  mandar, los aldeanos disponibles **más cerca** del centro del baile primero
  (`fn_0073C590` = `GetDistanceModifier(distancia; distancia del centro a la ciudad + 100) × vida³`); para retirar, los
  que están o van al lugar, los **más lejanos** primero (estado 163).
  - `GetDistanceModifier` 0x74F290 es `SigmoidThreshold(0,5; 1 − min(d; max)/max)`, con el umbral en el **primer**
    argumento (`push 0x3F000000` en 0x74F2B7): **baja** con la distancia, de 0,99996 en d = 0 a 3,6e-5 en d ≥ max (ver
    [engine-math.md](engine-math.md#distancias-de-gutils)). openblack los pasaba al revés y mandaba primero a los más
    lejanos; corregido en la sesión «sistemas2».
  - Es **vida³**, no vida²: tras `GetLife` (0x73C630) el bucle 0x73C63A..0x73C644 (`mov eax, 2`, y dos vueltas de
    `dec eax; fmul vida; jne`) multiplica la vida dos veces más, y el modificador entra al final (0x73C646).
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

## Alineación del jugador (`GAlignment`, GPlayer +0x60; `src/ECS/Effects/Alignment.*`, `components::PlayerAlignment`)

- Valor de −1 (malvado) a +1 (bueno) en +0x08 y un cambio pendiente en +0x0C. Partida nueva: 0 (`GGame::Init`
  0x54FEA0 toma el del perfil, 0 sin él). Vive con el jugador, no con la tierra (no se borra al cargar
  mapa): en openblack, un `components::PlayerAlignment` por `PlayerNames` fuera del registro de la tierra
  (`Magic/Core/Players`, `AlignmentOf`), el mismo que usan los milagros con `GAlignment::Update` 0x414410.
- **Actos** (`GAlignment::Update` 0x4145A0 para árboles): ±`GPlayerInfo::treePullPutAlignmentChange` (0,005), pesado por
  la alineación actual (fn_00414660): hacia donde ya se inclina cuenta `v·(1 − |a|/2)`, en contra `v·(1 + |a|/2)`; se suma
  al pendiente. Arrancar con la mano (`Tree::InterfaceSetInMagicHand`) es malo; replantar (`Tree::EndPhysics`) y el árbol
  que planta el agua (`Tree::ApplyWaterSpell`) son buenos.
- **Cada turno** (`GPlayer::Process` → `ProcessForPlayer` 0x4141A0 → `Process` 0x414140; en openblack la ranura 3 de
  `Magic/MagicLoop.cpp`, `GPlayer::ProcessPlayers`): el pendiente, limitado a −1..1,
  por `maxAlignmentChangePerGameTurn` (0,0019444 = 0,7 por hora de juego) se suma (`CrudeUpdate`, limitado a −1..1) y el
  pendiente vuelve a 0. O sea, el pendiente es una **fracción del ritmo máximo** de ese turno: un árbol arrancado mueve la
  alineación unas 10⁻⁵ (−0,005 × 0,0019444). Es lo que dice el código; otros actos (efectos, milagros, muertes) aportan
  mucho más.
- Guion: `GET_ALIGNMENT(jugador)` devuelve el valor; `SET_ALIGNMENT(jugador, v)` **suma** v (`CrudeUpdate`, pese al
  nombre) y fuera de −1..1 da el error «Alignment out of range» sin hacer nada (`GScript::SetAlignment` 0x6F99C0).
- Sin portar: el historial (`CAlignmentHistory::Add` 0x415260, que leen los consejeros y la vista bueno/malo) y
  `GGuidance::HelpSpritesAlignmentProcess`. La alineación del **terreno** (`MapCoords::GetAlignment`, la del crecimiento y
  los campos) es otra cosa, de la influencia de cada celda, y sigue sin portar. Traza: `OPENBLACK_ALIGNMENT_TRACE=1`.

### Los efectos de los hechizos (`GAlignment::Update` 0x414410)

- `GAlignment::Update` 0x414410 (R7 resuelta): nada si la vida no cambió. `K = |Δvida| +
  GPlayerInfo.applyEffectAlignmentChangeAddition`: el jugador 0 guarda en +0x64 el puntero a `GPlayerInfo` 0xD47988,
  y +0x1C en memoria es el archivo +0x0C. Para aplastar, golpear, curar y empujar, `pendiente += f(v ×
  GAlignmentInfo[i][col] × K)`; para quemar, lo mismo con `ConvertTemperatureToDamage`. **fn_00414660 compara el
  signo del cambio con el de A** (0 cuenta como positivo): mismo signo `v(1 − |A|/2)`, signo contrario `v(1 + |A|/2)`
  (leído de nuevo en 0x414660..0x4146AD: `je 0x414696` si A ≥ 0; en cada rama `jne` si v < 0; corregido el
  2026-09-30, la primera lectura de M1 decía que solo miraba el signo de v).

### La alineación del cielo (`alignment::GetInterfaceAlignment`)

`fn_0064AC30`, una vez por turno al final de `GPlayer::ProcessPlayers` (0x64A697; aquí en el hueco 3 del turno, después
de `alignment::ProcessPlayers`): el jugador con más influencia (`Influence::CalculateMostInfluentialPlayer` 0x5CD630: el
primero, en el orden de los jugadores, cuya influencia supera la de los anteriores y 0; si ninguno, el neutral) en la
posición de la interfaz, **GInterfaceStatus +0xB0 = la posición de la cámara** (lo dice `UpdateSpellInfo` 0x5DC948,
que calcula el frente de la cámara como +0xBC − +0xB0), y `x = clamp((alineación + 1)/2, 0, 1)` es lo que recibe
`fn_005E2240`. Empieza en 0,5; `DoCitadelMultiplayer` la fija a 0,5 (no hay multijugador).
`Clouds::InfluentialPlayerAlignment` (de mapa) devuelve `2x − 1`, salvo el gancho `OPENBLACK_TEST_SKY_ALIGNMENT` o el
deslizador de depuración movido de 0.

## Reacciones (`ECS/Effects/Reactions`)

- Reacciones (`ECS/Effects/Reactions`, el único módulo, unido al de los animales): `CreateReaction` 0x6E3D70 crea
  el objeto de 0x44 bytes (radio del ctor 0x6E39D0: 1 si la reacción crece, si no `maxReactionDistance`) y lo reparte
  una vez (`SpreadReaction` 0x6E3E10, la espiral de [animals.md](animals.md#reacciones)): cada vivo de la celda, en el
  orden de la celda, va al manejador de su clase (`SetLivingReactionHandler`: animales en `ECS/AnimalFlee.cpp`,
  aldeanos en `VillagerReactions.cpp`, que despacha fuego y teletransporte). Lo común a los vivos también está ahí:
  los registros (+0x98, `components::ReactionRecords`), la puntuación fn_006E4620 y la regla de cambio. Antes el
  fuego repartía con `maxReactionDistance` siempre (inf) y ordenaba los aldeanos de la celda por entidad; ahora usa el
  radio del ctor (en info.dat REACT_TO_FIRE no crece: 35 m, el mismo) y el orden de la celda, como los animales
  (aproximado: el de la rejilla de openblack, que se rehace una vez por turno y antes de un reparto fuera del turno, no
  las listas del original). El reloj es uno, el turno de juego fijado al principio del turno (`BeginTurn`, que además
  quita las reacciones cuyo iniciador ya no existe); los registros de los animales también lo usan.

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

## Fuego (M5, `src/ECS/Fire`)

Informes: `destructive.md` §2-4 y §7, `visuals_sound.md` §4.1-4.2, `psys/part_render.md` §8 y §10. Todo lo de abajo está
leído en el exe (W120) salvo lo marcado UNVERIFIED o «(inf)».

La bola de fuego y el rayo, que son los que encienden la mayoría de los fuegos, están en
[Bola de fuego y rayo](miracles.md#bola-de-fuego-y-rayo-m5-magicobjectsmagicfireball-psysrulesfireballlightning).

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
  `int(fmod(25·edad, 32))`) en rachas de 30 turnos; tinte del árbol ardiendo (fn_0074B3A0: gris 50, o
  `max(50, 255 − (1 − vida)·2550)` con vida > 0,9, **con tope sin signo en el brillo de los árboles del fotograma
  [0xC22FA0]** (0x74B47B, `ecs::TreeBrightness`, el mismo que multiplica a un árbol sin fuego en 0x74B077; antes el port
  ponía 255, y de noche el árbol quemado salía más claro), gris del carbonizado (fn_00730570: `k = ftol(c·255) & 0xFF`
  y cada canal `((unsigned)(−175k) >> 8) − 1` (0x730585..0x7305D7) = **`255 − ceil(175k/256)`**: 255 con k = 0, **80**
  con k = 255; antes el port truncaba y daba 81; `test_fire` lo compara con el código entero del exe para los 256 k) y
  brillo `GetFireEffectCharingColor` 0x730480. El mapa de luz `S_LMFireBall` del objeto ardiendo (bit 4 de +0xB5,
  sesión sistemas U5) **solo existe bajo los MultiMapFixed**: la marca `Object +0x24 & 2` (0x7312D5) la pone solo el ctor
  de `MultiMapFixed` (0x52E207: casas, BigForest, Feature...), no un árbol suelto.
  - **Pendiente (render)**: mientras dibuja el árbol ardiendo, 0x74B4D6..0x74B51E ponen `OverrideMaterial` [0xECA658] = 1
    y `OverrideRenderMode` [0xECA65C] = `ftol(min(254, 230 + calor·25/255))` (calor 255 si T > 1,5·Tc, si no
    `ftol((T − Tc)·255/(0,5·Tc))`; tope 254 [0x99A17C]), y lo quitan tras `AddForDrawing` (0x74B5D8). No es un material de
    brillo: las funciones de modo 0x82E080.. lo leen como **ALPHAREF** de las primitivas con prueba de alfa
    (`render_modes::AlphaRef` `forced`), así que el follaje del árbol ardiendo se recorta (solo pasan los texeles casi
    opacos). Sin portar: los árboles van instanciados y `fs_object` toma el ALPHAREF por dibujo
    (`u_skyAlphaThreshold.y`), así que hace falta un dibujo propio para cada árbol ardiendo (`FireGraphic.cpp`, TODO).
- **Aldeanos** (`VillagerFire.cpp`, `VillagerFireman.cpp` 0x75A3D0-0x75B460 y `ReactToFire` 0x765870): estados 215
  `REACT_TO_FIRE`, 216 `PUT_OUT_FIRE_BY_BEATING`, 219 `ON_FIRE` y 220 `MOVE_AROUND_FIRE`. Los de agua (217, 218) **en
  W120 se rinden en el acto** (`DECIDE_WHAT_TO_DO`), así que nadie acarrea agua. Un aldeano que apaga no recibe calor
  (fn_0072F980). `SetupOnFire` 0x75B170 guarda el estado y el destino anteriores y pasa a `ON_FIRE` con el fuego que lo
  calienta. **R10** (la decisión de 0x765870) queda leída en `ReactToFire`: el aldeano busca el fuego del grupo más
  cercano a él que esté por encima de la temperatura de reacción (`fn_00730070`).
  - Sitio del bombero (`GetFireFightingPos` 0x75AA90): en la recta del fuego al aldeano, a `max(radio seguro, radio
    del objeto)` (0x75AAF2..0x75AB16; antes el port tomaba el mínimo, y el aldeano iba y venía 216 ⇄ 220 cada turno) +
    el radio del aldeano (0x75AB23) + `GameFloatRand(1)`. La llegada de `MOVE_AROUND_FIRE` y de `GO_TOWARDS_TELEPORT`
    es `MobileWallHug::AreWeThere` 0x60AD60: `d² < (paso +0x5A + extra)²` estricto, el paso de `RebuildMoveByStep`
    0x609D10 = `WallHug::speed` (antes, 1 m). Comprobado: Land1, `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`,
    `OPENBLACK_VILLAGER_TRACE=1` (`dev\_audit\magic\fix_firemen.log`): 13 cambios 216 → 220 y 25 220 → 216 en toda la
    vida del fuego (antes 3213 en 650 turnos), cada aldeano decenas de turnos en cada estado.
  - `Villager::ReactionValidate` 0x756A00 (`villager_reactions::ReactionValidate`): la columna «validate» (+0x80) de la
    tabla de estados 0xD09198 en las filas de reacción (201, 202, 251, 215-218, 220, 6-30, 140-146), que
    `Villager::ProcessState` 0x74FF91/0x74FFD9 corre cada turno para el estado de arriba (+0x8C) y el guardado (+0x8D)
    antes del estado: `PopFromPrevious` 0x751E50 si el objeto de la reacción (+0xBC) no existe o no está disponible
    (`GameThing::IsAvailable` 0x401810, vt 0x2C), o si la fila de `ReactionInfo` (0xD4F6B0, `Reaction::GetInfo`
    0x6E4709) pide `whetherReactionFinishesIfInitiatorInHand` (+0x28) y el objeto está en la mano (+0x24 & 4).
    `ReactToFire` 0x765870 y `GoToTeleportReaction` 0x7662F0 no comprueban nada más (el primero solo devuelve 0 si el
    objeto no es un `Object` o no tiene fuego, sin cambiar de estado). Conectada (2026-10-01, fusión de V2):
    `LivingActionSystem::VillagerCallValidate` la llama en toda fila sin validate propio cuyo validate original es
    0x756A00 (`VillagerOriginalFns.h`); las salidas propias (inferido) de `ReactToFire` y `GoToTeleportReaction` ya no
    están (detalle en [villagers.md](villagers.md)).
  - **Qué saca al aldeano de 215 cuando el objeto deja de arder** (2026-10-02): no es el estado. El fuego quita su
    `REACT_TO_FIRE` al bajar de la temperatura de reacción (fn_0072EFB0 0x72F781), al borrarse (`FireEffect::ToBeDeleted`
    0x72EC4C) o al moverse, con `RemoveAllReactionsOfTypeInitiatedByObject` 0x6E4780, que llama a `Reaction::ShutDown`
    0x6E4720 de cada una: +0x34 = 1 y, mientras quede algún seguidor (+0x1C), `StopReactingAndSetState` (vt +0x99C,
    0x5F11C0: `ResetStateAfterReacting` 0x751E10 = `PopFromPrevious` y `DECIDE_WHAT_TO_DO` si el estado final es de
    reacción; luego `StopReacting`) del primero de la lista +0x18 (0x6E4731..0x6E4743). Así el aldeano vuelve a lo que
    hacía en el mismo turno, esté en 215, huyendo hacia 215 o apagando. Portado: `villager_fire::ShutDownReaction`,
    llamado por `RemoveReactions` de `FireEffect.cpp` antes de quitar la reacción (el orden de los seguidores es
    (inferido): por entidad). Para una `REACT_TO_FIRE` quitada por otra vía (`Pot::RemoveReaction` 0x66D6A0 quita todas
    las de un objeto; las reacciones de openblack no guardan la lista de seguidores), `ReactToFire` hace el mismo
    `StopReactingAndSetState` al ver que su reacción ya no está (aproximado: un turno más tarde). Sigue sin portar
    `Living::ProcessReaction` 0x5F1270 (cada turno: reacción no disponible → `StopReacting`; objeto +0xBC nulo o no
    disponible, o pasados los turnos de la tabla 0xC09CF0 de su tipo → `StopReactingAndSetState`), `TODO` en
    `VillagerCore.cpp` (sesión mapas).

### Natives CHL (`Magic/Script/CHLFire.cpp`)

170 `IS_ON_FIRE` 0x6FB4C0, 171 `IS_FIRE_NEAR` 0x6F7910 (`FindNearForScript` con el predicado 0x6F7100; una bola de fuego
no está en las celdas, así que no la encuentra), 174 `SET_TEMPERATURE` 0x6FB840, 175 `SET_ON_FIRE` 0x6FB780, 321
`SET_HURT_BY_FIRE` 0x6FDF40 y 426 `SET_SET_ON_FIRE` 0x6FDEE0 (los dos últimos, bits 2 y 3 de Object +0x0A).

### Captura

- Capturas en `dev\_audit\magic\`:
  - `m5_tree_fire.png` (`OPENBLACK_TEST_FIRE="1818.6,2628.4,500,tree,110"`): el árbol ardiendo, carbonizado y con
    llamas, y aldeanos alrededor; el registro muestra la propagación al aldeano 30, que huye en estado 219 y muere, y de
    ahí al objeto 52. `m5_gfx.log` tiene la traza del gráfico (2 llamas vivas de las 2 que permite el árbol, escala 0,25).

## Tiempo y clima

Está en [day-night-weather.md](day-night-weather.md#tiempo-y-clima-m6a-srcecsweather) (LH3DAtmos, GClimate, tormentas, lluvia y consultas `Weather.h`).

## Milagros uno a uno

Cada milagro tiene su sección en [miracles.md](miracles.md):

- [Comida y madera](miracles.md#comida-y-madera-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)
- [Agua](miracles.md#agua-m4a-magicspellsspellwater-psyscreatorsmist)
- [Curar](miracles.md#curar-m4-m4h-magicspellsspellhealcpp-psysruleshealcpp)
- [Bosque](miracles.md#bosque-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees)
- [Bandadas](miracles.md#bandadas-m4c-magicspellsspellflock-psysrulesflockcpp)
- [Bola de fuego y rayo](miracles.md#bola-de-fuego-y-rayo-m5-magicobjectsmagicfireball-psysrulesfireballlightning)
- [Escudos](miracles.md#escudos-m6-shield-magicspellsspellshield-magicobjectsmapshield-psysrulesshield)
- [Teletransporte](miracles.md#teletransporte-m6t-srcmagicobjectsmagicteleport-srcecssystemsimplementationsvillagerteleport)
- [Tormenta, tormenta eléctrica y tornado](miracles.md#tormenta-tormenta-eléctrica-y-tornado-m6-storm-magicspellsspellstormandtornado-psysrulesstorm-ecsweatherlightningflashstormclouds)
- [Explosión de rayo y clases de PSys que faltaban](miracles.md#explosión-de-rayo-y-clases-de-psys-que-faltaban-m6b-psysrulesexplosionkeypointsorientforestcpp)
- [Milagros de la criatura](miracles.md#milagros-de-la-criatura-m8-pendiente)

El motor de partículas (tipos de partícula, registro de clases, creadores, sonido e índice de reglas) está en
[particles.md](particles.md).

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

### El tamaño de la bola de fuego lanzada con la mano (inferido, recuerdo del usuario)

`Spell::InitWithPos` 0x71FE50 da al PSys la magnitud `SpellCastData[0]` sin comprobar si es 0 (`PSysInterface::Create`
0x68E910 → `GJPSysInterface::Create` 0x68F3DA la guarda en el manager +0xA0, que lee `MagnitudeFloatProvider`
0x69DA90). En `SpellSeed::Cast` 0x729520 ese valor sale del paquete de gesto (+0x14, fn_0071FA10), que es
`GInterface` +0x1B8 copiado entero en el paquete 0x12 (`SendApplyToMapCoord` 0x5D362D → fn_00550E90 → formato 15 de
`SendPacketCompressed`, un bloque de 0x18 bytes, sin cuantizar) y que **solo escribe el círculo** (0x5CF57A y 0x5D33BA;
el `GInterface` nace a cero). Pero justo después, `SpellSeed::DoPreCastThings` 0x729460 hace
`if (magicInfo.spellSeedType == FIRE) castData.magnitude = 1.0` (0x729502..0x72950B): los programadores fijaron la
bola de fuego a magnitud 1 fuera cual fuera el gesto. Leído al pie de la letra esa rama está muerta: info.dat deja
`GMagicInfo` +0x28 (`spellSeedType`) a −1 en todas las filas y nada lo escribe en el juego, así que la bola saldría con
el tamaño del último círculo, o 0 → 0,01 (4 cm, casi no calienta) si nunca se dibujó uno. El usuario recuerda (2026-10-01)
que una bola lanzada desde la mano salía **siempre grande**, con cualquier gesto: manda su recuerdo, y el port aplica la
rama con el tipo de la propia semilla (`GSpellSeedInfo`, semilla +0x6C) cuando la fila deja el campo a −1
(**inferido**, `SpellSeed.cpp` `DoPreCastThings`). Resultado: escala de átomo 1 × 4,0168 del sprite raíz, la bola se ve
en vuelo y prende la casa y el árbol donde cae (`fix_fireball_flight.png`, `fix_fireball_hut.png`). Con `SPELL_AT_POS` la
magnitud sigue siendo el radio del guion (10 en `m5_fireball.png`), porque no pasa por la semilla.

### Cadenas probadas en el juego (capturas en `dev\_audit\magic\`)

- Land1, dispensador → bola → semilla → lanzar: `OPENBLACK_TEST_DISPENSER="NORSE_ABODE_SPELL_DISPENSER,1812,2652,1"`,
  `OPENBLACK_TEST_TAP="1812,2652,200"`, `OPENBLACK_TEST_CAST="press@30,release@31,shot@33"`,
  `OPENBLACK_TEST_THROW_VEL`: la bola da la semilla FIRE lista (3500 cánticos), se arma (estado 8) y al soltar sale el
  hechizo con su `MagicFireBall` (T 6000). Con la bola de 0,01 de antes el granero no llegaba a prender
  (`review2_disp_fireball.log`); con la magnitud 1 de la semilla FIRE arden la casa, un árbol y los aldeanos de al lado
  (`fix_fireball_hut.png`, con `OPENBLACK_CAMERA_FLY=1800,75,2600,1826,30,2641`, `OPENBLACK_MOUSE_AT=0.5,0.55` y
  `OPENBLACK_TEST_THROW_VEL=0,2,6`).
- Land1, comida junto al almacén: el mismo dispensador con `FOOD`: dentro del radio del almacén (18,5 m) todo entra en
  él (`review2_disp_food.log`); un poco más allá (`review2_disp_food_pour.png`) hace una `MagicFood` de 200 que crece
  18 por grano, con la mano alzada 16 m y el chorro de 4 s.
- Land2, icono → carga → semilla → lanzar: `OPENBLACK_TEST_WORSHIP_SITE="NORSE,FIRE,HEAL,FOOD,WOOD"`,
  `OPENBLACK_TEST_TOWN_SPELL="0,FIRE"`, `OPENBLACK_TEST_MANA=20000`, `OPENBLACK_TEST_TAP_ICON="FIRE,200"` y
  `OPENBLACK_TEST_CAST`: `seed 3120 of icon 3082 in the hand with 3500 chants`, armada y lanzada
  (`review2_land2_cast.png`, `review2_land2_seed.log`). Con 3000 cánticos el icono se queda cargando con la batería a 0
  (`review2_land2_charge.log`). Una bola de un uso tocada con cánticos en el lugar sale atada al icono 3081
  (`review2_land2_oneshot_icon.log`).

## Suposiciones auditadas (2026-10-01)

Auditoría de TEAM_GUIDELINES §1.7 sobre todo lo que añade local/magic: 245 hallazgos, 41 corregidos para igualar el
original, 52 con la fuente añadida, 139 marcados en el código y 13 sin cambio (ya fieles o de otra sesión). Tabla por
fichero: `dev\_audit\magic\assumptions_audit.md`. Lo que queda marcado, por tema:

- **Corregido para igualar el original:**
  - Hechizos: un hechizo sin PSys se lanza igual y acaba al turno siguiente (0x71FE50, paso 8).
  - Semillas y lanzadores: `ProcessSpellSeed` devuelve siempre 1 (0x721370); un creador sin objeto no es funcional
    (0x405240); la selección de milagro pone a 0 el gesto de potenciación (0x5CF010).
  - Mano: el fotograma del brillo se redondea (`fistp` 0x68D323, no `__ftol`) y la vuelta es «> 64» (0x68D0C0).
  - Teletransporte: los destellos SPOT_VISUAL 14 duran lo que su entrada.
  - Jugador del guion: el byte g_game+0x205A5B es el hueco del **jugador neutral** (7; GGame::SetupPlayers 0x550458,
    GPlayer::IsNeutral 0x64AC00). Por eso el jugador 0 del guion y una pila mágica sin dueño son neutrales.
  - Bola de fuego: la bola rebota en los escudos (DoAnyShieldDeflections 0x6A1FA0 desde GravityWithFloor 0x6A1F48);
    el lanzamiento no humano se vuelve a resolver si v² > **0,01** (el double [0x8C7620] de `fcomp qword` en 0x69EC60;
    leído como float parecía 89129, corregido el 2026-10-02) y la subida pasa de 30° (el double [0x9375F0] =
    0,52370351552963257, `fptan` 0x69EC77).
  - Rayo: los modos van por orden (mano, gestor, padre; 0x690F88) y el del padre usa un círculo, sin cono; las
    horquillas solo se actualizan con el efecto activo.
  - UR_WillowWisp: la edad de cada átomo es fracción·dt (0x6A70CC).
  - Fuego:
    - La reacción de fuego no sale ni en la mano ni en vuelo (+0x24 & 0x44, 0x72F729).
    - Una Feature quemada se borra (0x6378E0); un campo quemado pone T = 0 y borra su fuego (0x52A010).
    - StartOnFire incluye campo, estático móvil, estático animado y fragmento (0x52EC60).
    - El bit 0 del gráfico de fuego es IsMorphWithLand.
    - La lluvia se corta solo por debajo de 0 (fn_008341B0).
  - Aldeanos:
    - Las funciones de salida reciben el estado siguiente (ExitPutOutFire 0x752530, ExitReaction 0x7527A0,
      ExitMoveToWorshipSite, ExitAtWorshipSite 0x76C1F0).
    - El escudo bloquea la reacción de fuego (fn_0072B990).
    - Un aldeano que va a adorar no lucha contra el fuego (0x765A6A).
    - Ya no se cuenta dos veces a quien adora: el lugar de culto lleva la lista de sus aldeanos (+0xD4, fn_0077D040).
    - `SetupMoveToWithHug` (0x5F2890) pone TOP y luego FINAL (0x752440) en una sola función compartida
      (`VillagerMove.cpp`).
  - Culto: la semilla entra en la mano solo si `InterfaceSetInMagicHand` devuelve 1 (0x5DA77C); la ciudadela influye
    con factor 1 (0x463240).
- **(aproximado):**
  - Reacciones: la rejilla de celdas de openblack y su orden; `InBounds` usa la extensión de la tierra, no
    MapCoords::InBounds 0x6042C0.
  - EffectValues: `ReduceLife` de Object para todas las clases.
  - Rayo: la segunda horquilla en lugar del árbol fn_00691F30.
  - Mapas de luz: alfa = máximo RGB, sin nivel ×190.
  - Gráfico de fuego: el ruido del carbonizado es de dos senos (no VLNoise 0x590C30); se actualizan todos los fuegos.
  - Números aleatorios: las tormentas, la lluvia, el fuego y los milagros ya van por `game_random` (GRand, el PSys y la
    CRT del original); las luciérnagas y otros sistemas siguen con los de openblack (fase B).
  - Aldeanos: MOVE_AROUND_FIRE va recto (GetViaPoint 0x75A440 sin portar); la decisión de luchar contra el fuego
    (0x765870: fórmula leída, sin término aleatorio) toma fn_00730290 / fn_007302E0 sin trazar; FLYING / LANDED no se
    ejecutan al aterrizar tras un teletransporte.
  - Gestos: los fotogramas con ratón hacen de mensajes de ratón.
  - Culto: la cuenta de los que vuelven a casa (vt 0x8C8 sin identificar).
- **(inferido):**
  - Reacciones: GetReactionPower = 1 para todos (Spell 0x55CF10 y Tree 0x55D8D0 sin portar).
  - EffectValues: un golpe por objeto en ApplyEffectToMapPos 0x525100.
  - Semillas: la mano derecha para la semilla de un uso; el jugador local para comprobar la influencia de lo que
    se lleva en la mano.
  - Valores por defecto de las reglas de PSys cuyo ctor no se leyó (Gravity 10, giros, rastro, malla, gesto 5 s,
    cadena de 0,5·escala).
  - Escudo: el castData por defecto (40).
  - Culto: el anillo de baile de 6 m y el punto de icono de reserva.
  - Muchos valores de defensa de openblack: índices fuera de rango, topes, 0,0001.
- **Pendiente (TODO con dirección en el código):**
  - Rayo: la rama de un solo objetivo (0x691CF5), el corte por escudo fn_006D0BC0 y el sonido por estado.
  - Reacciones: la rama sigilosa de 0x6E3E10.
  - Criatura: contador de muertes y alineamiento (+0x11C0, +0x168).
  - Semillas: el objetivo MagicFireBall (0x728A20).
  - Culto: el camino por sendero (58).
  - Fuego: la ruta alrededor del fuego.
  - Clase de hechizo sin portar (criatura, M8), que corre como un Spell simple (tormenta, agua y bandadas ya tienen
    la suya, oleada 4).

### Oleada 4 (agua, bandadas, tormenta, explosión de rayo; lane audit4)

25 hallazgos (tabla en `dev\_audit\magic\assumptions_audit.md`, sección «Wave 4»): 4 corregidos, 1 comentario, 7
marcados, 11 comprobados con el desensamblado y 2 sin cambio.

- **Corregido para igualar el original:**
  - Bandadas: al final de `SpellFlock::Process` la posición del jefe va a la **bandada** (+0x14, el centro del
    dominio), no al hechizo (0x7234F2..0x723519).
  - Tornado: la espiral de la recogida es GUtils::Spiral 0x74D7E0 empezada con dirección 1 (0x6D22E9); el port
    recorría la simétrica.
  - Tormenta: la reacción de apagar fuegos se olvida cuando no está disponible (vt 0x2C, 0x72DBA2), no solo cuando
    desaparece.
  - Bucle: fn_0064AC30 (alineamiento del cielo) va tras los viajeros de los teletransportes, al final de
    GPlayer::ProcessPlayers (0x64A697).
- **(aproximado):** la subcolección añadida a mitad de paso se actualiza ese paso (PSys.cpp); `MoveToBaseGroup` sin
  colección raíz borra el átomo; las vasijas por celda del tornado salen del registro; el color base de las nieblas
  es el del fotograma anterior; la salida de `UR_ForestPath` sin claves es 0.
- **(inferido):** el +0x80 de `EventConditionAtomNearVillagers` en metros; el destino del jefe lobo antes de su
  primer turno.
- **Sin cambio:** `FixedObjectsInMapCell` recorre todo el registro por celda (lento con muchos objetos fijos);
  `SetDeathCallback` de animales tiene una sola ranura (solo la usan las bandadas).

## Pendiente

Lo que falta está en cada tema, al final de su sección:

- Culto: de dónde salen los milagros: [Diferencias con el original y lo que falta](#diferencias-con-el-original-y-lo-que-falta)
- Influencia: la influencia virtual, los aliados, la regla de multijugador, el dibujo y `CalculateMostInfluentialPlayer` ([Influencia](magic.md#influencia-m1i-srcecsinfluence)).
- Lanzar desde la mano: la ayuda, la inmersión, los iconos de gesto del HUD, el brillo de la mano y alimentar una bola de fuego en vuelo ([Lanzar desde la mano, gestos y efectos de la mano](magic.md#lanzar-desde-la-mano-gestos-y-efectos-de-la-mano-m2-srcmagicgestures-srcmagichand-handspellseedcpp)).
- Alineación: el historial (`CAlignmentHistory::Add` 0x415260) y la alineación del terreno ([Alineación del jugador](magic.md#alineación-del-jugador-galignment-gplayer-0x60-srcecseffectsalignment-componentsplayeralignment)).
- Vida: la cuenta de aldeanos heridos del pueblo (Town+0x714) y la marca 0x40 de `Object::SetLife` 0x63A140 ([Vida de los objetos](magic.md#vida-de-los-objetos-m0-srcecslife)).
- Bola de los dispensadores: el usuario da por buenos el tamaño de las semillas y la altura de la burbuja (2026-10-01). La captura de referencia del original (`dev\_audit\magic\ref\dispenser_original.png`) es un orbe de AGUA, no de fuego: su mancha celeste es el efecto de la semilla de agua. Queda (aproximado) que la luz del terreno y la neblina se toman en `posición + facingOffset` y no en el punto adelantado hacia la cámara (Draw 0x518FCD..0x518FF2) ([Semillas y milagros de un uso](#semillas-y-milagros-de-un-uso-spellseed-oneoffspellseed)).
- Dispensador roto por una roca lanzada: openblack lo parte en trozos como una casa; el original lo dibuja con `MultiMapFixed::Draw` (`SpellDispenser::Draw` 0x722940 -> 0x518090). Falta leer `Abode::ReactToPhysicsImpact` 0x406240 y qué le pasa a su orbe.
- Semillas COMIDA y BEAM_EXPLOSION: también se cargan con propiedades de material (`{1,0,1,1,0}`); aplicar `L3DMesh::SetMaterialProperties` como a la burbuja.
- Vórtice entre tierras (`MagicVortex`, CREATE VORTEX): sin portar; al soltar, fn_005FE3B0 marca `thing+0x25 |= 0x40` en 0x5FE5DD (`script_held::SetCannotBeEaten`).
- Lluvia en el crecimiento de los árboles (`GrowTree`, fórmula en [trees.md](trees.md)): el clima es de Milagros.
- Árbol ardiendo: el ALPHAREF forzado 230..254 (`OverrideRenderMode`, 0x74B4D6..0x74B51E) necesita un dibujo propio
  por árbol ([Fuego](magic.md#fuego-m5-srcecsfire)). `Living::ProcessReaction` 0x5F1270 de los aldeanos (mapas).
- `OPENBLACK_TIME_OF_DAY` no se aplica ya en Land 1 (el guion controla el reloj).
- Relevo para una sesión nueva de milagros: `Desktop\B&W\Prompts y detalles.md`, sección MILAGROS.
- Fuego: el mapa de luz `S_LMFireBall` del objeto ardiendo, solo bajo los MultiMapFixed (sistemas U5,
  [Fuego](magic.md#fuego-m5-srcecsfire)).
- Lo marcado en el código por la auditoría y el tamaño (inferido) de la bola de fuego: [Suposiciones auditadas](magic.md#suposiciones-auditadas-2026-10-01), [El tamaño de la bola de fuego lanzada con la mano](magic.md#el-tamaño-de-la-bola-de-fuego-lanzada-con-la-mano-inferido-recuerdo-del-usuario).

Lo pendiente de cada milagro está en [Pendiente](miracles.md#pendiente).

## Ganchos de prueba

Todos los `OPENBLACK_*` están en [openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración).
Por tema:

- Núcleo de los hechizos: [Ganchos y trazas](#ganchos-y-trazas)
- Lanzar desde la mano, gestos y efectos de la mano: [Ganchos, pruebas y capturas](#ganchos-pruebas-y-capturas)
- Culto: de dónde salen los milagros: [Capturas](#capturas)
- Fuego: [Captura](#captura)

## Fuentes

- `dev\tmp_dis\miracles\`: `PLAN.md`, `core.md`, `casting.md`, `sources.md`, `influence.md`, `destructive.md`,
  `resources.md`, `protect_creature.md`, `visuals_sound.md`, y `impl\review2\` (revisión de la ola 2).
- `dev\_audit\magic\`: capturas y registros citados, y `assumptions_audit.md` (la auditoría de suposiciones).
