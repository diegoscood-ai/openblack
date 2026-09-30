# Día, noche y clima del original

Informes y scripts: `C:\Users\diewgarc\dev\tmp_dis\daynight\` (`emu_cycle.py`, `emu_update.py` emulan con Unicorn las
funciones del reloj de `runblack.exe`; `night_visuals.txt` es el informe de las luces de noche).

## Reloj (hecho: `src/3D/DayNightClock.*`)

Hay **dos relojes** en horas 0..24:

- **Hora visual** (`GLandAlignement::VisualTime` 0xBF3380). Avanza en línea recta, un día cada `duración` segundos de
  tiempo de juego (1700 s por defecto, unos 28 min). La mueve `GLandAlignement::UpdateTime` 0x5E1FE0 una vez por
  turno, llamada desde `GGame::ProcessTurn` 0x54E6AE con `(escala · 0,1, 0,1)`. Con el juego en pausa no avanza y con
  el juego rápido va más deprisa (va por turnos).
- **Hora de guion**: la hora visual pasada por una función lineal a trozos (`fn_00869FD0`; `fn_0086A160` visual →
  guion, `fn_0086A110` guion → visual). Lleva los umbrales del ciclo a las horas fijas 3,5 / 7,5 / 8 / 8,5
  (0xC395B0..BC), reflejadas en las 12. La usan `GET_GAME_TIME` / `SET_GAME_TIME`, el sol (`fn_0086C020`), la luna
  (`LH3DAtmos::UpdateGame`) y las luces de las farolas (`fn_0086C220`). openblack le pasa esta hora al cielo
  (`Sky::SetTime`); es equivalente a `Time2SkyType(hora visual)` con los umbrales del ciclo.

**Ciclo** (`GGameInfo::SetVisualTimeCycle` 0x557620, `(duración, noche, cambio)`; fracciones del día entero):

- Velocidad: `n = trunc(duración · 0,416667)` y `10/n` horas por segundo, igual de día y de noche (0xBF338C y 0xBF3390
  valen lo mismo).
- Umbrales (`LH3DSky::SetDayNightTimes` 0xFA26A0..94): con A = 12·noche, B = 12·cambio y c = min(B/4, A) quedan
  (A − c, A + c, A + B − c, A + B + c). Por defecto (1700; 0,083; 0,07) son **0,786 / 1,206 / 1,626 / 2,046 h**
  desde medianoche. Es decir, hay noche cerrada solo entre las 23,2 y las 0,8 h visuales y es de día entre las 2 y las
  22 h: la noche dura unos 2 min reales y cada transición otros 30 s.
- `LH3DSky::Time2SkyType` 0x86A1B0 sobre la hora visual: 2 de noche, 1 al ocaso, 0 de día. `IsVisualNight` es > 1,2.
- `fn_0086A3B0` pone (4,5; 7; 7,5; 8,25) al abrir el cielo, pero `GLandAlignement::Open` lo pisa en seguida con el
  ciclo por defecto.

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

- `OPENBLACK_TIME_OF_DAY=<hora de guion>` fija la hora en cada fotograma.
- `OPENBLACK_TEST_MOVE_TIME="hora,segundos"` hace un `MOVE_GAME_TIME` al cargar; el guion puede pisarlo.
- `OPENBLACK_CLOCK_TRACE=1` escribe el reloj en el log cada 50 turnos.
- El menú World muestra la hora de guion (deslizador) y la visual.

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
  - Sprites aditivos: dos llamas `S_Fire` (fotogramas 0..31 al revés en 700 ms, semitamaño 1 ± 0,1) y un halo
    `smoke` fotograma 56 (semitamaño 3 ± 0,1). Color 0xF38421 con alfa trunc(I/2).
  - Aproximado: el shader de sprites solo usa la máscara alfa, sin el RGB de la textura.
  - Falta el quad aditivo de la mano sobre el agua (±60, `atmos.raw`) y las dos luces de la puerta nórdica
    (MSH_O_TOWNLIGHT en (±15, 30, 0) de la puerta).
  - Informe: `night_visuals.txt`, secciones 3 y 5.
- **Sonido de las farolas** (hecho; `src/Audio/LanternSounds.*`; informe `tmp_dis\mapa\flecos_lantern-sound.md`, volcados
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
- **Sonido**: `GSoundMap` mezcla el ambiente de día y el de noche según max(0, tipo de cielo − 1); los sonidos de las
  casas se eligen por el tipo de cielo. Pendiente.
- Solo jugabilidad, fuera por ahora: los leones y los lobos van a su guarida (22 / 23 h), los niños salen de la
  guardería, el deseo de comida y los deseos de la criatura.

## Clima

- Nubes del cielo (`CloudInSky`): hechas, ver [rendering.md](rendering.md) (colocación, color por hora y alineación,
  alineación del cielo suavizada 0,001/ms). Lo que depende del tiempo va por `Clouds::WeatherOvercastAtCamera()` (hoy 0).
- **Nubes de tormenta** (sin hacer; informe `tmp_dis\daynight\gweather_drawclouds.txt`,
  `tmp_dis\mapa\clouds_placement.md`): `GClimate::CreateStorm` 0x772E00 → `GWeather` 0x83F590 → `DrawClouds`
  0x83FC90, un grupo de hasta 16 bolas (8 por defecto, `CHANGE_CLOUD_PROPERTIES` cambia número, negrura y altura) en
  lx, lz ∈ −1..1, ly ∈ −10..10 + altura; cada 400 fotogramas un objetivo nuevo (ly 0..20 + altura), 1/400 del camino
  por fotograma; mundo X = R·lx/2 + cx, Z = R·lz/2 + cz, Y = suelo + ly con R = radio(t) + radio2; tamaño R·2·Random(0,01,
  0,015); color [0xFA26A4]·(1 − 0,5·negrura), alfa·intensidad·0,75, con neblina; no se dibujan con alfa ≤ 5. Son los
  únicos grupos de nubes del original.

Pendiente (siguiente tema): `GClimate` (temperatura, lluvia,
nieve y tormentas), `LH3DAtmos::Render3D` (lluvia/nieve por casillas de 80×80), relámpagos, tiempo nublado en la tabla
de luz y la neblina.
