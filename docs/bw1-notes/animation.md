# Animación esquelética (aldeanos y animales)

Informes completos con direcciones: `C:\Users\diewgarc\dev\tmp_dis\anim\anm_format.md` (formato y reproducción) y
`villager_anims.md` (qué clip toca en cada estado; tabla de los 255 estados, animales, bailes). Scripts de lectura en
la misma carpeta (`anm.py`, `pack.py`, `l3d.py`, `gen_state_fns.py`).

## Clips (`Data\AllAnims.anm`)

- Índice del clip = enum `ANM_*` de `Data\AllMeshes.h` del juego (441 clips; el `AllMeshes.h` de bw1-decomp es el de
  Creature Isle y tiene otros números). En openblack: `ecs::ClipId(índice)` (el gestor guarda ids con hash).
- Cabecera (0x54): 0x20 **duración en ms** de tiempo de juego; 0x28 **zancada** (metros de un ciclo; walk man 1,16);
  0x2C..0x34 vector de desplazamiento; 0x38 fotogramas; 0x40 tamaño del clip en bytes (openblack lo llamaba
  `animationDuration`); 0x50 flags: byte bajo = huesos, **0x100 bucle**, 0x200 (en ejecución) sin sincronía por
  distancia (zancada < 0,05). `LoadAllAnimations` (0x550180) quita el bucle a 323 `P_OUT_OF_PRAY`.
- Fotograma: punteros y cuenta + huesos × 12 floats. **No hay tiempos por fotograma** (lo que openblack leía como
  tiempo es un puntero): las claves están equiespaciadas. Bucle: periodo = duración y la última clave vuelve a la
  primera; sin bucle: periodo = duración·n/(n−1) (enteros) y se queda en la última.
- Matrices: filas (ejes x, y, z, traslación), vectores fila; cada hueso **relativo a su padre** (misma convención que
  los huesos de la malla). Sin desplazamiento de la raíz (el avance solo está en la cabecera).

## Reproducción (`LH3DAnim::GetPose` 0x839980)

- i = n·t/periodo; interpolación **lineal de los 12 floats** entre la clave i y la siguiente, sin cuaterniones ni
  re-ortonormalizar; W = L · W(padre) (la raíz bajo la matriz del objeto). openblack: `L3DAnim::SampleLocal`,
  `graphics::ComputePose` (`3D/SkeletalPose`).
- Tiempo (fn_005167D0): + ms de tiempo de juego del fotograma (0 en pausa); con bucle módulo duración, sin bucle se
  queda al final. Estados que se mueven (info `field0x14`): avanza por distancia, metros / escala / zancada ×
  duración (fn_0051AF00).
- Cambio de clip (`Living::SetAnim` 0x5ECBA0): **corte instantáneo** (no hay mezcla aunque el motor la tenga), el
  mismo clip no reinicia. Estado por objeto: clip y tiempo en ms.

## Qué clip (`Villager::GetAnimId` 0x750110)

- Estado 0 o ≥ 255: `P_STAND` 385. Si el estado tiene función de animación (tabla fija 0xD09198, ranura 0x60) manda
  ella; si no, el clip del estado en info.dat (`villagerStateTable.field0x0`; −4 = no se dibuja).
- Andar (`MoveToPosAnimation` 0x423400): vida ≤ 0,15 gatea, ≤ 0,30 cojea; por velocidad (umbral info.dat
  `speedThreshold` 1 hombres 3/4 m/s, 0 mujeres 2/3): WALK / RUN / SPRINT `_MAN`/`_WOMAN`; llevando herramienta o
  carga `CARRY_AXE` / `CARRY_OBJECT_RUN`.
- Cambio de estado (`SetTopState` 0x5F28E0): clip de salida del estado anterior (salvo `field0xf0`), si no el del
  estado nuevo y su clip de entrada. Mientras suena uno de esos la lógica del estado espera (turnos × 100 ms ≥
  duración). Rareza del original: tras un clip de salida el de entrada no se ve, se repite el del estado.
- openblack: `ECS/VillagerAnimations` (tabla generada `VillagerAnimationTable.h`), llamado desde
  `LivingActionSystem::VillagerSetState` y `Update`. Las funciones que necesitan lo que aún no existe (tipo de
  aterrizaje, agua, bailes, peleas, fútbol, criaturas) toman la rama del original para su ausencia.

## Velocidad de marcha

`dev\tmp_dis\anim\speed_units.md`: 1 unidad del mundo = 1 m (MapCoords 6553,6 por metro). El u16 de velocidad
(+0x5A) es lo que avanza **por turno** en MapCoords (`GetSpeedInMetres` 0x60C070 = u16 / 6553,6), y las tablas de
info.dat (speedGroup) están en esas unidades: 1475 = 0,225 m/turno = 2,25 m/s. openblack movía `WallHug::speed` =
2,25 por turno (10 veces demasiado rápido): ahora es m/s × 0,1. Un hombre normal da 2,25 / 1,16 ≈ 1,94 ciclos de
paso por segundo, con los pies sincronizados (el clip avanza con la misma distancia). `ECS/VillagerSpeed` (investigación `dev\tmp_dis\anim\speed_exact.md`): en cada cambio de estado
(`SetStateSpeed` 0x753760) la velocidad sale de la entrada del speedGroup que pide el estado final (índice en
`villagerStateTable.field0x24`), × 0,85 si tiene pueblo (base de las necesidades del pueblo; openblack aún no tiene sus
deseos) × las cargas de leña y comida (sin carga: 1); herido: 0,4-0,6 × speed4 o 0,5-0,75 × speedDefault. Luego
`SetSpeed` (0x750ED0) la multiplica por f = 1 + ((índice de creación × 47) % 31 − 16) × 0,01 (openblack usa el índice de
la entidad), menos: niño min((13 − edad) × 0,02, 0,4); viejo (> 60) min((edad − 60) × 0,02, 0,4); adulto 0,1 × vida
(y el hambre al cubo, que aún no existe) y 0,2 las mujeres; se trunca a u16. Un hombre normal va a ~1,7-1,9 m/s y una
mujer a ~1,4-1,6. Sin hacer: la creencia del pueblo en el jugador, el bonus de maravilla del jugador y la escala del
mapa (`SET_GLOBAL_LAND_BALANCE`).

## Tamaño y malla por edad

`Villager::SetAge` (0x7528C0): niño si la edad es menor que `grownUpAge` (13; openblack usaba 18), con las mallas
`childMeshHigh..Low`; un adulto tiene al menos 18 años. Escala (`InitialiseScale` + `SetScaleForAge`): adulto 0,9 y luego
1,05 − rand(0,1) (en (0,95, 1,05]); niño `ageToScale[edad − 1]` + rand(0,75 × la distancia a `ageToScale[edad + 1]`).
Sin hacer: el crecimiento cada 375 turnos (openblack no envejece a los aldeanos).

## Aldeanos ocultos

`Villager::Draw` no dibuja al aldeano si el clip de info.dat de su estado actual es −4 (`ANM_DONT_DRAW`, p. ej.
AT_HOME) ni con la marca de estar en casa (+0xE0 & 4, `ArriveHome` / `LeaveHome`): openblack le quita la malla mientras
tanto (`SkeletalAnimation::hiddenMesh`), así tampoco se puede coger.

## Sonidos de los clips

`Audio/AnimationSounds` (investigación `dev\tmp_dis\anim\sounds_props.md`): `Data\SmallSounds.SAS` da a 115 clips un
grupo de sonido (1 personas, 18 vaca, 36 cerdo, 39 oveja, 40 caballo...) y eventos `ms soundId acción`. Al cruzar un
evento, la clave {voz (1 hombre, 2 mujer, 3 niño), 2, grupo, superficie, soundId} elige una fila de la
`LHAudioAnimArrayTable` de editor.sad (la de más columnas exactas; empate: la última) y una muestra al azar de su lista
de `LHAudioWaveNumTable`. Solo suena a menos del `maxDist` de la muestra desde la cámara (pasos: 20 m; los NULL.wav de
relleno tienen 0 y nunca suenan). Superficie: 7 bajo el agua; si no, el `surfaceSound` de info.dat del segundo
material de la celda a su altitud (1 hierba, 2 grava, 3 duro, 4 barro, 5 nieve, 8 hojarasca). Los gritos de THROWN
solo en los primeros 15 turnos (10 en el vórtice). Falta: los de VillagersBanter.sad (0x92-0x94), parar la sierra
(acción 1) y que el sonido siga al objeto.

## Objetos en la mano

`ECS/CarriedProps`: el `CARRIED_OBJECT` del aldeano (+0xF1, `SkeletalAnimation::carriedObject`, puesto por
`SetStateCarriedObject` y por `BuildingAnimation`: martillo, sierra o mazo) se dibuja con su malla (tabla de
`CarriedObject::Init` 0x462530: hacha 342, caña 355, cayado 354, sierra 383, bolsa 343, pelota 344, martillo 367,
mazo 378, guadaña 384, pala 390, leña 406, ramas 347-349) pegada al hueso 15 de la pose (el agarre al final del brazo
−X), con los ejes −X, −Z, −Y del hueso y sin desplazamiento (`SetLinkedPosition` 0x815FC0). Es una entidad propia
(Transform + Mesh + `CarriedProp`) que se mueve cada fotograma. No se dibuja en la mano ni en los estados ocultos.
Gancho `OPENBLACK_TEST_CARRY=<tipo>`. Hoy casi ningún estado de openblack pone objeto (faltan los oficios).

## Render

Cada aldeano con pose (`components::SkeletalAnimation`) se dibuja por separado con sus huesos (`ecs::PosesByInstance`
en el bucle de instancias de `Renderer.cpp`); el resto sigue instanciado. Gancho `OPENBLACK_TEST_ANIM="clip[,ms]"`:
ese clip en todos los aldeanos (bloqueado; con ms, quieto en ese instante; con `OPENBLACK_START_PAUSED=1` no se
mueven). Traza: `OPENBLACK_ANIM_TRACE=1`.

Animales: aún no tienen estados en openblack y están quietos, así que tocan el `StandAnimation` de su especie
(`ECS/AnimalAnimations`: vaca 42, oveja 142, cerdo 126, caballo 57, león 106, tigre 164, leopardo 80, lobo 184,
tortuga 171; cabra y cebra devuelven −1 y se quedan en reposo).

Pendiente: el resto de funciones de los animales (tabla en `animal_table.txt`), sonidos de los clips
(`Data\SmallSounds.SAS`), objetos en la mano (hacha, bolsa...), sombras dinámicas y reflejo con la pose.
