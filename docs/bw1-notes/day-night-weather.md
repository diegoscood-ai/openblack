# Día, noche y clima del original

Informes y scripts: `C:\Users\diewgarc\dev\tmp_dis\daynight\` (`emu_cycle.py`, `emu_update.py` emulan con Unicorn las
funciones del reloj de `runblack.exe`; `night_visuals.txt` es el informe de las luces de noche).

El tiempo del juego (climas, tormentas, lluvia) está al final, en «Tiempo y clima».

- [Reloj](#reloj-hecho-src3ddaynightclock)
- [Tipo de cielo](#tipo-de-cielo-src3dskytype)
- [Luces de noche](#luces-de-noche-informe-night_visualstxt)
- [Clima](#clima)
- [Tiempo y clima](#tiempo-y-clima-m6a-srcecsweather)

## Reloj (hecho: `src/3D/DayNightClock.*`)

Hay **dos relojes** en horas 0..24:

- **Hora visual** (`GLandAlignement::VisualTime` 0xBF3380). Avanza en línea recta, un día cada `duración` segundos de
  tiempo de juego (1700 s por defecto, unos 28 min). La mueve `GLandAlignement::UpdateTime` 0x5E1FE0 una vez por
  turno, llamada desde `GGame::ProcessTurn` 0x54E6AE con `(escala · 0,1, 0,1)`. Con el juego en pausa no avanza y con
  el juego rápido va más deprisa (va por turnos).
- **Hora de guion**: la hora visual pasada por una función lineal a trozos (`fn_00869FD0`; `fn_0086A160` visual →
  guion, `fn_0086A110` guion → visual). Lleva los umbrales del ciclo a las horas fijas 3,5 / 7,5 / 8 / 8,5
  (0xC395B0..BC), reflejadas en las 12. La usan `GET_GAME_TIME` / `SET_GAME_TIME`, el sol (`fn_0086C020`), la luna
  (`LH3DAtmos::UpdateGame`) y las luces de las farolas (`fn_0086C220`). openblack le pasa esta hora al sol y a la luna
  (`Sky::SetTime` / `GetTime`). **No** es la hora del tipo de cielo: ese va sobre la hora visual (ver
  [Tipo de cielo](#tipo-de-cielo-src3dskytype)).

**Ciclo** (`GGameInfo::SetVisualTimeCycle` 0x557620, `(duración, noche, cambio)`; fracciones del día entero):

- Velocidad: `n = ftol(duración · 0,41666666f)` ([0x8DF8F0] = 0x3ED55555) y `10/n` horas por segundo, igual de día
  y de noche (0xBF338C y 0xBF3390 valen lo mismo). El producto se queda en el registro x87 antes de `__ftol`: con la
  FPU a 24 bits (lo que pone D3D, **(inferido)**) es el producto en float que hace openblack; con 53 bits las
  duraciones múltiplo de 2,4 (1200, 2400) darían un `n` menos.
- Umbrales (`LH3DSky::SetDayNightTimes` 0x869FA0 → 0xFA26A0..94): N = 12·noche, E = 12·cambio + N (guardado en
  float), c = min((E − N)·0,25, N), y `SetDayNightTimes(N − c, c + N, E − c, E + c)` (0x55768F..0x5576E4; **fiel**,
  openblack lo hace en este orden desde `DayNightClock::SetCycle`, que además llama a `sky_type::SetThresholds`).
  Por defecto (1700; 0,083; 0,07) son **0,786 / 1,206 / 1,626 / 2,046 h**
  desde medianoche. Es decir, hay noche cerrada solo entre las 23,2 y las 0,8 h visuales y es de día entre las 2 y las
  22 h: la noche dura unos 2 min reales y cada transición otros 30 s.
- `LH3DSky::Time2SkyType` 0x86A1B0 sobre la hora visual: 2 de noche, 1 al ocaso, 0 de día. `IsVisualNight` es > 1,2
  (el double). Todo el detalle, en [Tipo de cielo](#tipo-de-cielo-src3dskytype).
- `fn_0086A3B0` pone (4,5; 7; 7,5; 8,25) al abrir el cielo, pero `GLandAlignement::Open` lo pisa en seguida con el
  ciclo por defecto.
- En la campaña los umbrales son **siempre** los de por defecto: `challenge.chl` no llama nunca a 407 ni a 408, Land1,
  Land4, Land5 y LandT no tienen `SET_NIGHTTIME` y Land2/Land3 repiten (1700; 0,083; 0,07). Los playgrounds sí los
  cambian: SandBox Creature Training y ThreeGods (1000; 0,23; 0,17) → 2,25 / 3,27 / 4,29 / 5,31; Demon God
  (1143; 1; 0) → 12 / 12 / 12 / 12, tipo 2 siempre salvo a las 12 en punto (0); Ultimate Sandbox (3400; 0,4; 0,01) →
  4,77 / 4,83 / 4,89 / 4,95.

**Avance por turno** (`UpdateTime(a, b)`, a = escala · 0,1 y b = 0,1):

1. Si no hay un `MOVE_GAME_TIME` en curso: paso 2,5 h/s y destino = visual + velocidad · a, donde velocidad =
   lerp(día, noche, tipo de cielo / 2).
2. La hora visual se acerca al destino por el camino corto (±12 h), a lo sumo paso · b por turno, y da la vuelta en 24.
3. `SetTime` 0x5E22E0 normaliza el destino y lo pone a 0 fuera de ±1000. Con segundos ≠ 0, paso = |diferencia| /
   segundos. `MOVE_GAME_TIME` termina en el turno siguiente a llegar.

Comprobado: 20 000 turnos más un `MOVE_GAME_TIME(3, 30)` dan en el port los mismos valores que la emulación del
original, hasta el sexto decimal.

**Arranque y guiones**

- `GLandAlignement::Open`: escala 1, ciclo por defecto y hora 12. openblack llama a `DayNightClock::Reset` al empezar
  `Game::LoadMap`, antes del guion de la isla.
- Guion de isla: `SET_NIGHTTIME(1700, 0.083, 0.07)` en Land2/Land3 llega a `SetVisualTimeCycleFromMapEditor` 0x557BB0,
  que limita noche ≤ 1 y cambio ≤ 1 − noche.
- CHL: `SET_GAME_TIME` 112, `GET_GAME_TIME` 113, `GAME_TIME_ON_OFF` 288 (escala 1/0), `MOVE_GAME_TIME` 289 (hora,
  segundos), `SET_GAME_TIME_PROPERTIES` 407, `RESET_GAME_TIME_PROPERTIES` 408.
- Land1 empieza con `FollowUs`: `SET_GAME_TIME(7.3)` y `GAME_TIME_ON_OFF(0)`, un amanecer con el reloj parado; más
  tarde hace `MOVE_GAME_TIME(12, 460)`. `LandControl1` pone 15,4 h y enciende el reloj. Otras secuencias fijan su
  hora, por ejemplo `VillageWavingSequence` 12/16 h y la de los meteoritos de Land4 16,5 h.
- En openblack la intro no avanza (faltan funciones CHL), así que Land1 se queda de momento a las 7,3 h con el reloj
  parado. Para ver el ciclo hay que usar el menú World o los ganchos de abajo.
- **Arreglo del VM**: las funciones CHL sin implementar dejaban sus argumentos en la pila, y los de las siguientes se
  desplazaban. Por ejemplo, `SET_PROPERTY` dejaba un 1,0 que luego leía `SET_GAME_TIME`. Ahora `LHVM::Opcode05Sys`
  quita `stackIn` valores cuando la función no ha sacado ninguno, por debajo de lo que haya metido.

**Ganchos de prueba**

- `OPENBLACK_TIME_OF_DAY=<hora de guion>` fija la hora en cada turno (`ForceScriptTime`, con el salto del cielo).
- `OPENBLACK_TEST_MOVE_TIME="hora,segundos"` hace un `MOVE_GAME_TIME` al cargar; el guion puede pisarlo.
- `OPENBLACK_CLOCK_TRACE=1` escribe el reloj en el log cada 50 turnos: hora visual, de guion, tipo de cielo calculado
  ahora, el del fotograma (`sky_type::Frame()`) y el de la cúpula (`Dome().Built()`).
- El menú World muestra la hora de guion (deslizador), la visual, el tipo de cielo del fotograma y el de la cúpula.

## Tipo de cielo (`src/3D/SkyType.*`)

Una sola API, `openblack::sky_type::` (`src/3D/SkyType.h`), con el convenio del original: **2 noche, 1 ocaso, 0 día**,
continuo. Informes: `dev\tmp_dis\unify2\shader_sky_type_original.md` (su «Verificación adversaria» manda) y
`shader_sky_type_openblack.md`; plan: `SHADERS_PLAN.md` §6. Todo **fiel** salvo lo marcado.

| Función | Original | Qué hace |
|---|---|---|
| `At(hora)` | `Time2SkyType` 0x86A1B0 | Pliega en 12 solo si hora > 12 (`test ah,0x41`); luego `<` estricto contra A..D: 2, 2 − (h − A)/(B − A), 1, 1 − (h − C)/(D − C), 0. No hay división por cero: con A = B la rampa es inalcanzable. `DayNightClock::Time2SkyType` reenvía aquí con sus umbrales |
| `SetThresholds(A,B,C,D)` | `SetDayNightTimes` 0x869FA0 | A → 0xFA26A0, B → 0xFA269C, C → 0xFA2698, D → 0xFA2694; lo llama `DayNightClock::SetCycle` (0x557620) |
| `SampleFrame(visual)`, `Frame()`, `FrameHour()` | `fn_0086A2C0`, [0xFA26BC], [0xFA26C4] | Normaliza a [0, 24) con los bucles de 0x86A2C4..0x86A308 y guarda hora y tipo. `DrawSky` 0x5E2226 lo llama una vez por fotograma con la hora visual [0xBF3380]: en openblack, al principio de `Renderer::DrawScene` (una vez por fotograma, no en cada pasada) |
| `Jump(visual)` | `fn_0086A270` | `SampleFrame` y la cúpula entera de golpe. Lo llama `fn_005E22A0` 0x5E22CB (`ForceVisualTime` 0x5575D0): en openblack, `DayNightClock::ForceScriptTime` (Reset/LoadMap, `SET_GAME_TIME`, el deslizador, `OPENBLACK_TIME_OF_DAY`, `config.timeOfDay`). `MOVE_GAME_TIME` no salta |
| `IsVisualNight(T)` | 0x5575E0 | T > el double 1,2 ([0x8D8758] = 33 33 33 33 33 33 F3 3F): T = 1,2f ya es noche. La usa `DayNightClock::IsVisualNight`; `ChildAtCreche` 0x757CFF tiene la misma comparación |
| `EveningRamp(visual, w, o)` | `fn_00557AE0` | u = 24 − visual, s = D + o; u < s → 1; !((D + w) + o > u) → 0; si no, 1 − (u − s)/w. Para los deseos Relaxation 0x7488C0 / Sleep 0x748960 (sin llamador aún, **pendiente** de mapa V3) |
| `LightColumn(T)` | 0x86985E..0x8698AD | (2 − T)·6,0f·2,5f = (2 − T)·15, columna de la tabla de luz |
| `HazeFactor(T)` | 0x869D5F..0x869D7A | v = T > 1 ? 2 − T : T; v² |
| `DomeWeightOf`, `BlendTexel555`, `DomeBlend` | `fn_0086B7F0`, `fn_0086B9A0`, `fn_0086A330` | La cúpula, abajo |

Quién llama a qué:

- Calculan el tipo en el momento con la hora visual, como el original: la luz de los modelos
  (`model_light::UpdateFrameLight(…, sky_type::At(visual))`, como `fn_005E5830` 0x5E58D1..0x5E58DF; da lo mismo que
  `Frame()` porque `DrawSky` muestrea la misma hora justo después), `DayNightClock::ProcessTurn` (0x5E202B), las
  luciérnagas (`GetSkyType`, 0x52B7CA / 0x52B820) y `IsVisualNight` (luces de noche).
- Leen el muestreo del fotograma: la cúpula y, por el reenviador obsoleto `SkyInterface::GetCurrentSkyType`
  (= 2 − `Frame()`), `LandLightTable::Build` (la conecta «sistemas»). El sonido (`GSoundMap`, 0x71DDF1) debe leer
  `Frame()`, el T del fotograma anterior (ProcessTurn 0x54D830 y EndTurn 0x54D837 van seguidos, sin `DrawSky`):
  `audio::ProcessTurn` lo lee así desde B11c de «audio» (hecho).
- Quitado: `Sky::GetCurrentSkyType` con hora de guion y umbrales inventados 3,5 / 7,5 / 8 / 8,5 con `<=` (queda solo
  el reenviador), `Sky::SetDayNightTimes`, `u_skyAlphaThreshold.x` (fs_object no lo lee), `u_sky` y su rampa de
  reserva de `fs_water` (sin `palette.raw` el mar va ahora sin luz, blanco, **(inferido)**: el original siempre tiene
  la tabla). `u_skyAndBump.x` lleva 2 − `Frame()` hasta que «sistemas» lo quite de fs_terrain.
- Precisión: se supone la FPU a 24 bits (lo que pone D3D, **(inferido)**), la misma hipótesis que `SetCycle`. Con
  ella las rampas de `Time2SkyType` ya salen redondeadas a float, los bucles de `SampleFrame` van en float (h = −1e-7
  da 24 → 0; con 53/64 bits se quedaría en 23,9999999 y se guardaría 24,0f) y la resta de la histéresis se redondea a
  float antes de compararla con el double 0,03f.
- NaN: `Time2SkyType` compara con `fcomp` / `test ah,1` / `je` (0x86A1DC..0x86A246), así que «no ordenado» cuenta
  como «<»: una hora NaN da 2 (noche). openblack lo copia con comparaciones `!(t >= x)`. En `SampleFrame` el original
  se queda en bucle infinito con NaN; openblack sigue (diferencia de openblack).

**La cúpula** (`DomeBlend`, `Sky::UpdateDome`, `fs_sky.sc`):

- `fn_0086A3B0` crea 3 texturas dinámicas de 256×256 (flags 0x104, formato 4; [0xFA2738 + 4a]), una por alineación,
  y las construye enteras. Fuentes: `Data\WeatherSystem\sky_<good|ntrl|evil>_<day|dusk|night>.555`, índice
  3·hora_del_día + alineación ([0xFA26E8]).
- Cada fotograma, `fn_0086A330` (tras `SampleFrame`, 0x5E222B): si la cúpula está entera y |`Frame()` − construido|
  > 0,03 (el double [0x99A168] = (double)0,03f, estricto), guarda el nuevo T ([0xFA26C0]) y vuelve a la fila 0
  ([0xFA26B8]); mientras falten filas mezcla 32 más con el T guardado (el primer bloque en el mismo fotograma). Son
  [0xEDD470] ? 128 : 256 filas. [0xEDD470] depende del nivel de detalle: `fn_00823AD0` lo copia de la tabla
  [0x9A38E0 + 4·nivel] = 1, 1, 0, 0, 0, 0, 0 (0x823C5B..0x823C69; `DetailLevel::skyNoBlend`), y `fn_0082A8E0` lo
  vuelve a escribir en sus dos salidas (0x82AB1B / 0x82AB30). En los niveles 2..6 son 256 filas en 8 fotogramas, con
  mezcla. En los niveles 0 y 1 `fn_00869670` es falso: sin mezcla, 128 filas y tinte por T (abajo, sin portar).
  openblack mezcla 256 filas en todos los niveles.
- `fn_0086B7F0`: T ≤ 1 → w = ftol(T·255) entre día (255 − w) y ocaso (w); si no, w = ftol((T − 1)·255) entre ocaso
  y noche. `fn_0086B9A0` (camino 555): por canal de 5 bits `(c_inf·(255 − w)) >> 8 + (c_sup·w) >> 8`; la suma de
  pesos es 255/256 (un canal de 31 sale 30) y el bit 15 sale a 0. Como cada término se trunca por separado, la cúpula
  sale algo más oscura y con más bandas que la mezcla lineal de antes: unos −4/−5 por canal (de 255) a mediodía y
  hasta un 40 % menos en los canales oscuros de noche (las texturas de noche tienen valores 1..4 de 31). Es lo que
  hace el original (tablas 0xFA2554 / 0xFA2514, p = 255 − ftol(T·255) en 0x86B8E3): **no «arreglarlo»**.
- La textura (formato 4) solo recibe la marca +0x138 cuando el bloque llega a la última fila (0x86B95B..0x86B977); el
  unlock `fn_00838EB0` solo sube él mismo los formatos 1, 2 y 0x20. La marca +0x138 significa «sucia, subir en
  el próximo enlace»: el camino de `SetTexture` de la textura LH3D (0x837EC6..0x837F74) la mira, bloquea la superficie
  D3D (vtable +0x64, flags 0x821), convierte la copia de sistema con [+0x134] (0x837F19), desbloquea, llama a
  `IDirect3DDevice7::SetTexture` (+0x8C) y borra la marca (0x837F74); es decir, en el `DrawSky` del mismo fotograma.
  openblack sube las 3 capas a la GPU solo al terminar la última fila, así que la cúpula cambia de golpe tras 8
  fotogramas, como el original.
- openblack lo hace en CPU, en `Sky::BlendDome`, sobre una copia de las 9 texturas; `fs_sky.sc` ya no mezcla horas,
  solo alineaciones (capa 0 mala, 1 neutral, 2 buena; `u_typeAlignment.x` no se usa). Cómo mezcla el original la
  alineación está **sin leer** (openblack mantiene su mezcla lineal de las dos más cercanas, **(inferido)**).
- La cúpula inicial se construye con el T actual (el original usa la hora de `fn_0086A3B0` con sus umbrales
  4,5 / 7 / 7,5 / 8,25); el último salto de hora de `Open` (0x5E1D9C; el primero es `fn_005576F0` 0x557702) y el
  `ForceScriptTime(12)` de `Reset` en openblack la rehacen entera en seguida (gana el último salto), **(inferido)** sin
  efecto visible. `fn_005E22A0` llama además a `fn_005E1DE0` (0x5E22D3, lee [0xBF3378]) tras el salto: no es tipo de
  cielo y no está en `ForceScriptTime`.
- Sin portar: el modo sin mezcla (`fn_00869670` falso: copia de las texturas de día y tinte del color del cielo por
  T, 0x86B1C1..0x86B2A4), el camino 565 ([0xEDD46C]) y la tabla de luz tras `Jump` (openblack la rehace cada
  fotograma).
- Orden en `GGame::Load`: el salto (0x554B6F) va antes de `SetVisualTimeCycle` (0x554C9E), con los umbrales anteriores;
  openblack no carga partidas guardadas.

Pruebas: `test_sky_type` (umbrales por defecto, `At` con Demon God y con NaN, normalización (también
−1e-7 → 0 a 24 bits), 1,2 double, rampa de tarde, columna y neblina, pesos, mezcla 555, histéresis y filas (también la
resta redondeada a float), salto desde `ForceScriptTime`).

## Luces de noche (informe `night_visuals.txt`)

- **Ventanas** (`Abode::Draw` 0x515F70): las submallas con la marca `isWindow` del L3D se ven sin luz, con un gris
  plano 224..252 que parpadea un poco. Ocurre solo si hay alguien en casa (Abode +0xB6) y es de noche visual. Cada
  casa tiene su desfase de hora: la parte fraccionaria de |x + z|·0,1 + y. openblack: `night_lights::WindowGrey`
  (`src/3D/NightLights.*`). Como openblack todavía no manda a los aldeanos a casa, cuenta "alguien en casa" como
  que la casa tiene habitantes **(aproximado)**.
- **Casa de los aldeanos del guion**: al crear un aldeano con CREATE_VILLAGER, openblack (b8657f1d) le da la casa
  más cercana a la posición de abode del guion (a 1 unidad o menos) o, si no hay, cualquiera con sitio, y lo añade
  a sus habitantes. Regla propia, sin contrastar con el original **(aproximado)**.
- **Luz de la mano y de las farolas** (hecho; `night_lights::Update`, llamado desde `Renderer::UpdateClouds`):
  - Mecanismo (`fn_008229B0`): una imagen de 8 bits, remapeada al cargar a min(47, v·48/255 + 0,5), se estampa en
    la luminosidad de las celdas con una bilineal que el original desplaza un texel. v = r·trunc(I·255)/255. Si
    v ≤ lum·((G255·48) >> 8) >> 8 la celda no cambia (G255 es el verde de la tabla[255]). Si no, v sustituye a la
    tierra normal (≥ 48) o se queda el máximo con otra luz. El rango 0..47 es la rampa cálida de la tabla de luz.
    openblack la escribe en el mismo cap R8 que las sombras de nubes (el shader toma el mínimo).
  - Solo cuando la media de la base de la tabla está por debajo de 120.
  - Mano: `light_hand.raw` (12×12) desde la posición del modelo de la mano − 55, con fuerza clamp((120 − media)/15).
    Sin luz si la mano está oculta.
  - Farolas: `CREATE_STREET_LANTERN` (tipo 7 = MSH_O_TOWNLIGHT, llamas a +5) y hogueras (MSH_B_CAMPFIRE, +1).
    Estampa `village_diffuse.raw` (14×14) desde pos − 50, con ±0,5 de temblor cada 30 ms. Fuerza I/255 de
    `fn_0086C220` por hora de guion: se enciende de 16,5 a 17,5 y se apaga de 6 a 7.
  - Sprites aditivos: dos llamas `S_Fire` (fotogramas 0..31 al revés en 700 ms con un reloj global entero, que solo
    corre con alfa ≠ 0; la llama i empieza en la tabla global 0xC383BC, que cada luz nueva reescribe con
    ftol(Random(0, 31)) (fn_00823240 0x8233E4..0x8233F8), así que todas van en fase con la última creada;
    fn_00823570, `frame_anim::LanternCell`; semitamaño 1 ± 0,1) y un halo
    `smoke` celda 56 ((flags & ~7) | 0x38, 0x8233BC..0x8233EB; semitamaño 3 ± 0,1). Color 0xF38421 con alfa trunc(I/2).
  - Aproximado: el shader de sprites solo usa la máscara alfa, sin el RGB de la textura.
  - El quad aditivo de la mano sobre el agua (±60, `atmos.raw`) está hecho (W9, `src/3D/HandWaterGlow.*`, ver
    [rendering.md](rendering.md#cielo-sol-luna-y-nubes-original)). Las dos luces de la puerta nórdica
    (MSH_O_TOWNLIGHT en (±15, 30, 0) de la puerta) están hechas (5877f018, ver map-loading.md).
  - Informe: `night_visuals.txt`, secciones 3 y 5.
- **Sonido de las farolas** (hecho; `src/Audio/LanternSounds.*`, desde B3 del audio un `audio::tags` por farola,
  [audio.md](audio.md#b3-soundtag-completo); informe `tmp_dis\mapa\flecos_lantern-sound.md`, volcados
  `d_soundtag.txt`, `d_gaudio_sfx.txt`, `d_5e5830.txt`, `d_streetlantern.txt`, script `sadhdr.py`):
  - **Creación**: `GStreetLantern::CallVirtualFunctionsForCreation` 0x734810 crea un `SoundTag` (`fn_0071E8C0`,
    +0x60) con desplazamiento (0, `Object::GetHeight` 0x638120, 0), muestra 0x93, 3D, modo 2, bucles −1, banco 1
    (`Audio\SFX\Game\InGame.sad`) y luego `SetActive([0xDA0A10])`. **Las dos clases de farolillo** lo tienen (no mira
    +0x58); no lo tiene el objeto con la marca UNAVAILABLE (+0xA & 1).
  - **Muestra**: 0x93 = LH_SAMPLE_G_LANTERN_01 (147) = `G_Lantern_01.wav`, 22050 Hz, ~4,1 s, prioridad 200, marcas de
    sustitución 0x7C0: bucles −1, minDist 3, maxDist 5, escala 4, modo 2. Sin marca de volumen ni de tono, así que
    volumen 127 (ganancia 1) y tono 100 % con la desviación del ±15 % de siempre. Modo 2 = un canal por farola: si ya
    suena, no hace nada.
  - **Interruptor día/noche**: `fn_007349E0(on)` guarda [0xDA0A10] y hace `SoundTag::SetActive` en cada farola. Único
    llamador `fn_005E5830` (`GLandscape::Draw`, cada fotograma): 1 cuando la media de la base de la tabla de luz es
    **< 120** (la misma prueba que la mano y las luces del pueblo, sin prueba de hora), 0 si no. `SetActive(0)` llama a
    `StopPlayingSoundEffect`: corte seco.
  - **Por turno**: `SoundTag::ProcessSoundTags` 0x71E5F0 desde `GGame::EndTurn`; cada tag activo llama a
    `GAudio::PlaySoundEffect` 0x42A100 con (x, altitud + y, z) más el desplazamiento. 0x429E30 **no arranca** la muestra
    si la cámara está a más de maxDist (5 unidades) de ese punto; una vez arrancada el bucle sigue donde vaya la cámara.
    Si la farola deja de estar disponible, `CreateSoundTagForDeadObject` llama a `LHSampleReleaseLoop`: acaba la pasada
    en curso y el tag se borra.
  - openblack: `audio::lantern_sounds::SetOn` desde `night_lights::Update` (la misma media < 120),
    `ProcessTurn` en el bloque de turno de `Game.cpp` y `Clear` junto a `night_lights::Clear` al cargar mapa (antes del
    `Registry::Reset`, que borraría las entidades del emisor sin liberar la fuente de OpenAL y el bucle seguiría
    sonando). El emisor se crea con `PlayType::Repeat` y se suelta poniéndole `PlayType::Once` (= `LHSampleReleaseLoop`:
    `AudioManager::Update` le quita `AL_LOOPING` y lo destruye al acabar la pasada).
  - **No portado** (sin verificar): la curva de atenuación de QMixer para {min 3, max 5, escala 4}
    (`QSWaveMixSetDistanceMapping`, está en la DLL externa). openblack deja el modelo por defecto de OpenAL como en
    todos los sonidos 3D; un cambio general de audio podría poner `AL_REFERENCE_DISTANCE` / `AL_MAX_DISTANCE` por
    fuente desde el `.sad`, pero solo tras descifrar QMixer.
  - Por los 5 unidades de los datos, en juego normal solo se oye con la cámara casi encima de la farola.
  - Comprobado en juego (`OPENBLACK_LANTERN_SOUND_TRACE=1`): con `OPENBLACK_TIME_OF_DAY=22` y la cámara en la farola de
    pueblo de Land1 `2493,2534` arranca a 1,7 unidades y no se repite (modo 2); en el farolillo de campo `1365,2571`
    arranca igual (las dos clases); a mediodía nunca arranca; al cambiar de mapa (`OPENBLACK_TEST_MAP_CYCLE`) el corte de
    `Clear` no deja el bucle suelto. El paso noche → día **no se pudo provocar en juego**: el guion CHL de Land1 fija la
    hora de guion en 7,3 cada turno (por encima del umbral de oscuridad, que está entre 7,2 y 7,3), así que
    `OPENBLACK_TEST_MOVE_TIME` no la mueve y `OPENBLACK_TIME_OF_DAY` la clava sin transición; el corte seco se verificó
    por el mismo camino de código (`Clear`).
- **Luciérnagas** (hecho; `src/ECS/FireFlies.*`):
  - Cuándo: con hora visual > 12 y tipo de cielo > 1 aparecen hasta 50, la mitad en árboles y la mitad en rocas
    al azar. Cada turno una vuela a la casa o farola más cercana a menos de 300 m, a +(altura + 2). Por la mañana
    cada turno una vuelve a un árbol o una roca.
  - Vuelo: dura la distancia / (3·velocidad) con suavizado smoothstep.
  - Dibujo: órbita de radio 8 más un temblor de radio 1. Sprite 37 de `S_SpriteSheet3`, semitamaño 0,3, alfa 190,
    que se desvanece entre 100 y 300 m.
  - Aproximado: la búsqueda en espiral con un 50 % por celda se reduce a "la más cercana que pase una moneda".
- **Sonido**: `GSoundMap` mezcla el ambiente de día y el de noche según max(0, tipo de cielo − 1) (hecho, ver
  [objects-and-resources.md](objects-and-resources.md#sonidos-informe-tmp_dissoundnotestxt), "Ambiente"); los
  sonidos de las casas se eligen por el tipo de cielo (pendiente).
- Solo jugabilidad, fuera por ahora: los leones y los lobos van a su guarida (22 / 23 h), los niños salen de la
  guardería, el deseo de comida y los deseos de la criatura.

## Clima

El tiempo del juego (climas, tormentas, lluvia y su dibujo) está portado y descrito abajo, en
[Tiempo y clima](#tiempo-y-clima-m6a-srcecsweather). Aquí quedan las notas del cielo de la primera lectura.

- Nubes del cielo (`CloudInSky`): hechas, ver [rendering.md](rendering.md#cielo-sol-luna-y-nubes-original) (colocación, color por hora y alineación,
  alineación del cielo suavizada 0,001/ms). Lo que depende del tiempo va por `Clouds::WeatherOvercastAtCamera()`
  (byte 3 de `weather::atmos::GetWeatherSmooth` en la cámara × 0,01; 0 con el cielo despejado).

Agua y clima: **no hay suelo mojado** en el original; el mar solo cambia con el tiempo por el tope de nublado de la
tabla de luz y el destello del relámpago (`LandLightTable::Build`). El nublado tiene una sola fuente,
`Clouds::WeatherOvercastAtCamera()` (lee `weather::atmos::GetWeatherSmooth` en la cámara); `3D/SkyWeather` solo da el destello (`weather::LightningFlashAtCamera` de
`ECS/Weather/LightningFlash` en la posición de la cámara).
- **Nubes de tormenta** (sin hacer; informe `tmp_dis\daynight\gweather_drawclouds.txt`,
  `tmp_dis\mapa\clouds_placement.md`): `GClimate::CreateStorm` 0x772E00 → `GWeather` 0x83F590 → `DrawClouds`
  0x83FC90, un grupo de hasta 16 bolas (8 por defecto, `CHANGE_CLOUD_PROPERTIES` cambia número, negrura y altura) en
  lx, lz ∈ −1..1, ly ∈ −10..10 + altura; cada 400 fotogramas un objetivo nuevo (ly 0..20 + altura), 1/400 del camino
  por fotograma; mundo X = R·lx/2 + cx, Z = R·lz/2 + cz, Y = suelo + ly con R = radio(t) + radio2; tamaño R·2·Random(0,01,
  0,015); color [0xFA26A4]·(1 − 0,5·negrura), alfa·intensidad·0,75, con neblina; no se dibujan con alfa ≤ 5. Son los
  únicos grupos de nubes del original. Ya portadas: [El destello y las nubes de las tormentas registradas](miracles.md#el-destello-y-las-nubes-de-las-tormentas-registradas-ecsweatherlightningflash-stormclouds).

Pendiente (siguiente tema): `GClimate` (temperatura, lluvia,
nieve y tormentas), `LH3DAtmos::Render3D` (lluvia/nieve por casillas de 80×80), relámpagos, tiempo nublado en la tabla
de luz y la neblina. Hecho después en gran parte: ver [Tiempo y clima](#tiempo-y-clima-m6a-srcecsweather) y su «Sin portar / UNVERIFIED».

## Tiempo y clima (M6a, `src/ECS/Weather`)

Dueño: Milagros (WIKI_PLAN F2; la sección venía de magic.md).

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
    aquí son dos *callbacks* (`SetForkCallback` / `SetSheetCallback`) que otra lane rellenará. El destello
    (`fn_00837290`, `Storm::flash`) está en [El destello y las nubes de las tormentas registradas](miracles.md#el-destello-y-las-nubes-de-las-tormentas-registradas-ecsweatherlightningflash-stormclouds) (`ECS/Weather/LightningFlash`).
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
- El destello de los relámpagos (`fn_00837290`) y [0xFA2768] ya están (ver [El destello y las nubes de las tormentas registradas](miracles.md#el-destello-y-las-nubes-de-las-tormentas-registradas-ecsweatherlightningflash-stormclouds)), menos su sello de luz en el
  terreno; los *callbacks* de rayo y trueno están puestos pero vacíos (los rellenará la lane del rayo).
- Las gotas en el suelo (`g_water_drop_cb`).
- La influencia virtual del tiempo, `LHInetWeather` y el tiempo ambiente de la partida guardada.
- `GClimate+0x84` ("relámpago aunque haga menos de 30 grados"): **no se ha encontrado quién lo escribe**, se asume 0.
- El campo de distancia por bloque (+0x9BC) que usa `Render3D` para descartar bloques: aquí se usa la distancia 2D a la
  cámara con el mismo umbral (400 + 160), y el corte real sigue siendo el de 400 m de `fn_00834370`.

### Ganchos

`OPENBLACK_TEST_WEATHER="x,z,radio[,lluvia[,fundido[,temperatura]]]"`, `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` y
`OPENBLACK_WEATHER_TRACE=1`; ver
[openblack-internals.md](openblack-internals.md#variables-de-entorno-de-depuración).
