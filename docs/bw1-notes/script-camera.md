# Cámara del guion (GCamera y CameraModeScript)

Cómo mueve el guion la cámara en runblack.exe v1.42 (W120) y qué tiene openblack: los zoomers de GCamera, el modo de
cámara del guion, la regla de llegada, los opcodes CHL de cámara, el FOV y el cambio de manos al soltar el control.
Las pistas de `camera.edt` están en [camera-tracks.md](camera-tracks.md); los candados START/END_CAMERA_CONTROL, en
[audio.md](audio.md) (`Help/ScriptControl`).

- [GCamera y sus zoomers](#gcamera-y-sus-zoomers)
- [El modo del guion](#el-modo-del-guion)
- [Regla de llegada](#regla-de-llegada)
- [Un fotograma de GCamera::Update](#un-fotograma-de-gcameraupdate)
- [Seguimientos](#seguimientos)
- [Cámara doble](#cámara-doble)
- [Temblor](#temblor)
- [Zonas y giro fijo](#zonas-y-giro-fijo)
- [Opcodes](#opcodes)
- [Soltar la cámara](#soltar-la-cámara)
- [openblack](#openblack)
- [Pendiente](#pendiente) · [Ganchos de prueba](#ganchos-de-prueba) · [Fuentes](#fuentes)

## GCamera y sus zoomers

**Fiel.** GCamera (0x1D8 bytes) guarda la cámara en Zoomers de LH3DLib (`SetDestinationWithSpeedAndTime` 0x407D60,
`Update` 0x442720; [engine-math.md](engine-math.md#zoomer-lh3dlib), `src/Common/Zoomer.h`): el foco en +0x88/+0xB8/+0xE8,
la posición en +0x118/+0x148/+0x178 y el FOV (radianes) en +0x1A8. Un MOVE parte del valor y la velocidad actuales y
llega con velocidad 0 en T; T < 0,001 coloca al momento; al acabar T el valor se clava en el destino. La pila de modos
está en +0x28 (12) con el índice en +0x58; el jugador usa `CameraModeNew3` (vtable 0x8C7BFC), que siempre deja salir.

## El modo del guion

**Fiel.** START_CAMERA_CONTROL (0x6ECCA0), fuera de la ciudadela, llama a `fn_00461140`: si `CantExitCurrentMode`
0x441B70 no hay modo y el control falla; si no, `CameraModeScript` (ctor 0x461180, vtable 0x8C7D5C, hereda de
`CameraModeFollow`). Mientras vive, `CanExit` 0x461B70 = 0: **ninguna otra tarea coge la cámara**. No toca los zoomers.
`SetCameraPosition/Focus` 0x461370/0x4612B0 los fijan y `MoveCameraPosition/Focus` 0x4616F0/0x461430 los mueven (con
velocidad final 0); los cuatro sueltan antes la pista y el seguimiento. `RunPath` fn_00461A80 carga `Track%d` y
`UpdatePath` 0x461AB0 avanza con los ms de juego del fotograma: posición = `CameraWayRunner::Get`, foco = Bezier del way
de foco en el tramo y la t del runner de **posición**, y `SetPositionAndFocus` 0x4438C0 los fija.

## Regla de llegada

**Fiel.** `GCamera::Arrived` 0x443050: sin modo, 1; si no, vt+0x34 del modo. `CameraModeScript::Arrived` 0x461B40: con
pista, duración ≤ ms recorridos; sin pista, `CameraMode::Arrived` 0x441700: |posición − destino|² < 0,001 **y**
|foco − destino|² < 0,001 ([0x8AA3B0]). `CameraModeNew3` usa también 0x441700 (su vtable +0x34). No mira el FOV.

## Un fotograma de GCamera::Update

**Fiel** (0x441F80, desde `ProcessGraphicsEngine` 0x54D879, por fotograma):
1. dt = `GetCameraTimeInc` 0x555820 · 0,001 = ms de pared (`g_delta_time`), tope 0,1 s [0x8AB22C].
2. `Update` del modo (vt+0x08; Script: `UpdatePath` y el seguimiento `CameraModeFollow::Update` 0x44C160).
3. Los 6 zoomers con dt; NaN → el último bueno (0x4420D9).
4. Posición y foco casi iguales (< 0,001) → posición x − 1, y + 1, solo en lo dibujado (0x4421D5).
5. Disco del mundo: si el **destino** de la posición está a más de 3500 de (2560, 0, 2560), destino nuevo
   d / (|d| · 0,000285796) + centro en 3 s (0x44222C).
6. Ciudadela: no se mueve lo dibujado (0x442337). Si no, lo dibujado queda 1 m sobre el suelo, subiendo también el
   foco (0x44242B).
7. FOV: su zoomer con `g_game_time_inc` · 0,001 (**tiempo de juego**, no de pared) y `LH3DTech::ChangeFov` 0x8195B0.

## Seguimientos

**Fiel** salvo lo marcado (`CameraModeFollow`, del que hereda el modo del guion; lectura completa en
`dev\tmp_dis\camara\step2.md`).
- Campos: +0x08 cosa seguida con la posición, +0x4C (Script) cosa seguida con el foco (`GetFocusThing` 0x4611F0 = +0x4C
  o, si es nula, +0x08), +0x0C rumbo, +0x10 cabeceo, +0x14 distancia, +0x18 factor de tiempo (0.2 en Script), +0x1C
  «detrás» (1 en Script).
- `Set(cosa)` 0x44BA00: rumbo y cabeceo de los **destinos** de los zoomers (`GetHeadingAndPitchFromPoints` 0x4428D0),
  distancia = alto · 8 (`GetThingViewingDistance` 0x441F20); con «detrás», rumbo 0. `fn_0044BA90(cosa, d)`: igual con
  distancia d y sin poner el rumbo a 0. `fn_0044BB30` coloca ya (Zoomer::SetPosition) y pone GCamera+0x68 = 2.
- `CameraModeFollow::Update` 0x44C160, por fotograma: distancia recortada a 2..1500 y guardada; T = (+0x68 > 2 ? 1 :
  2 − +0x68 / 2) · factor (0x44C1A5: 0.4 s tras el cambio de modo, 0.2 s pasados 2 s; factor 0 → colocar); foco hacia el
  punto de la cosa (MapCoords: x, z / 6553.6, y = suelo + altitud +0x1C; en `Update` la traslación del Game3DObject si
  la tiene; más media altura; rebaño: `Flock::GetFlockPos` 0x530570 con la media altura del líder); posición =
  `SetPointFromPointDistanceHeadingAndPitch` 0x442810 desde ese punto con la distancia, cabeceo ≥ 0.241661 (guardado) y
  el rumbo, que con «detrás» sobre un MobileWallHug es rumbo − (ángulo del objeto − π/2) (0x44C785).
- `Validate` (0x461270 + 0x44BB10, **una vez por turno** desde `GGame::ProcessTurn` 0x54E74E): suelta la cosa que ya no
  está.
- Set/Move de un punto sueltan el seguimiento de su lado (0x461370 `Set(0)`, 0x4612B0 `SetCameraFocus(0)`);
  `RunPath` suelta solo +0x4C.
- Cara de un objeto (`fn_006ED710`, 106/107): foco = punto del MapCoords + media altura; posición a distancia d con el
  rumbo `GetFacingDirection` (vt+0x4EC; normalizado a ≤ 2π) y cabeceo 0.1.
- FollowUs: `SET_FOCUS_AND_POSITION_FOLLOW(Son, 3)` y `CAMERA_PROPERTIES(3, 0, 22.5, true)`: la cámara va pegada al niño,
  22,5° respecto a hacia dónde mira.

## Cámara doble

**Fiel** salvo lo marcado (`CameraModeTwoObjects`, 0x30 bytes, vtable 0x8C7DD0, «Dual Cam»; lectura completa en
`dev\tmp_dis\camara\step3.md`). Se apila encima del modo del guion (ctor 0x461BB0; con un punto fn_00461CB0; uno igual
al actual se borra a sí mismo).
- `Update` 0x461DE0, por fotograma: T = (+0x68 > 1.5 ? 1 : 2 − +0x68 / 1.5), **sin factor**; A y B = MapCoords de las
  dos cosas (o el punto); foco = punto medio subido por el alto medio · 0.5; distancia = ((separación en x/z + los dos
  `Get2DRadius`, 30 si no es un Object) · factor (1, o 1.2 con punto) + alto mayor · 1.4; rumbo = π/4 − la dirección de
  B − A; cabeceo π/8; los zoomers hacia esos destinos en T.
- 093 START (0x6ED2E0), 094 UPDATE (0x6ED370, `SetObjects` 0x461C90), 095 RELEASE (0x6ED410: `Delete` y `PopViewMode`,
  GCamera+0x68 = 0), 105 CON PUNTO (0x6ED460, sin comprobaciones). El fin del control (fn_006ECD70) quita una doble
  antes de borrar el modo del guion. Con la doble encima, los opcodes del guion encuentran «el modo equivocado».
- Por turno, `CheckStackedModesForValidity` 0x441D40 quita la doble cuyas cosas ya no están (`IsStillValid` 0x461D90).
- openblack: `script_camera::State::duals`, capa encima del modo del guion (sin pila de modos: **(aproximado)**); la
  doble toma los zoomers del jugador como `BeginFrom` y, si se va sin guion debajo, se los devuelve.

## Temblor

**Fiel.** SHAKE_CAMERA 201 (0x6EE0F0) → `PSysGlobal::StartCameraShake` 0x68F400 → `LH3DCameraChecker::Create` 0x821050
(radio, punto, amplitud, ms). Lo aplica fn_008210C0 desde `LH3DTech::UpdateCamera` 0x819920, **solo a lo dibujado**
(nunca a los zoomers): el temblor más cercano a la cámara, si está dentro de su radio (sin caída con la distancia);
amplitud = restante / total · amplitud; seis tiradas `Random` 0x81D180 (pos.z, pos.y, pos.x, foco.z, foco.y, foco.x) o
dos con «solo y». Cada fotograma dibujado (fn_00821270) resta `g_delta_time` y se libera al llegar a 0.
openblack: `src/Camera/CameraShake.{h,cpp}` (`camera_shake::`, con `graphics::lh3d::Random`) y
`script_camera::ApplyShake`, cada fotograma desde `Game.cpp` con la cámara que se dibuje (la del guion o la del
jugador), como desplazamiento solo de dibujo de `Camera::SetDrawOffset` (de sistemas): los zoomers no tiemblan.

## Zonas y giro fijo

- SET_CAMERA_ZONE 142 (0x6ED890): `ResetExclusionFile(1)` 0x455320 y `LoadExclusionFile` 0x455370 de
  `.\Data\Zones\%s` (segmento «cameraexc»: flags, dos límites de 500, n puntos del campo de fuerza, exclusiones),
  campo de fuerza encendido. **Fiel** el cargador y `InsideInclusion` 0x455E20 (`src/Camera/PlayerCameraScript.{h,cpp}`,
  `player_camera::`); los nueve `.exc` de `Data\Zones` se leen bien. **Pendiente:** lo que hace con ella la cámara del
  jugador (`CameraModeNew3::Update` 0x45F982: recolocar, temblor, pulso, dibujo del campo, influencia 0x5CD32F).
  GET_INCLUSION_DISTANCE 150 (0x6ED990) da por eso siempre FLT_MAX **(aproximado)**.
- SET_FIXED_CAM_ROTATION 209 (0x6EE1A0): solo con el modo del jugador; `ForceRotateAboutPoint` 0x457330 guarda el punto
  (`player_camera::Get().fixedRotation`). **Pendiente:** que `DefaultWorldCameraModel` gire alrededor de él (0x45AB00,
  0x460135). Ningún mapa lo usa.

## Opcodes

**Fiel** salvo lo marcado. Los que mueven comprueban el modo: sin modo «Script camera has been removed!»; otro modo
«We are in the wrong camera mode!» y no hacen nada.

| CHL | Original | Qué hace |
|---|---|---|
| 001 / 002 SET_CAMERA_POSITION / FOCUS | 0x6EC8F0 / 0x6EC9A0 | fija (sin modo de guion no hace nada) |
| 003 / 004 MOVE_CAMERA_POSITION / FOCUS | 0x6ECAA0 / 0x6ECBA0 | mueve en t s de pared |
| 035 HAS_CAMERA_ARRIVED | 0x6ED170 | regla de llegada |
| 119 RUN_CAMERA_PATH | 0x6ED7F0 | pista de camera.edt |
| 279 SET_CAMERA_LENS | 0x6EE2E0 | **`SetCameraFov(70°, x)`: el argumento es el tiempo** (copiado) |
| 280 MOVE_CAMERA_LENS | 0x6EE280 | FOV = lente · 0,0174533 en t s de juego |
| 283 / 284 STORE / RESTORE_CAMERA_DETAILS | 0x6EE330 / 0x6EE390 | guarda lo dibujado / `SetPositionAndFocus` |
| 286 / 287 SET / MOVE_CAMERA_POS_FOC_LENS | 0x6EE3C0 / 0x6EE4B0 | posición, foco y FOV, **la lente sin pasar a radianes** (copiado; ningún mapa los usa) |
| 314 / 315 GET_STORED_CAMERA_POSITION / FOCUS | 0x6EE630 / 0x6EE6A0 | lo guardado |
| 377 GET_FACING_CAMERA_POSITION | 0x6EE710 | posición + d · vector delante (inferido: unitario hacia el foco) |
| 049 / 276 FOCUS_FOLLOW / SET_FOCUS_FOLLOW | 0x6EDF30 / 0x6EDB40 | el foco sigue a la cosa (0x4619B0) |
| 050 POSITION_FOLLOW | 0x6EDE70 | la posición sigue a la cosa (`Set` 0x44BA00) |
| 277 SET_POSITION_FOLLOW | 0x6EDA80 | `Set` + colocar ya (fn_0044BB30) |
| 178 / 278 (SET_)FOCUS_AND_POSITION_FOLLOW | 0x6EDDA0 / 0x6ED9B0 | `fn_0044BA90(cosa, d)` (278 además coloca ya) |
| 180 CAMERA_PROPERTIES | 0x6EDFF0 | distancia, factor de tiempo, rumbo (° · 0.0174533), «detrás» |
| 106 / 107 SET / MOVE_CAMERA_TO_FACE_OBJECT | 0x6ED500 / 0x6ED600 | cara de un objeto, fijar / mover en t |
| 203 SET_AVI_SEQUENCE | 0x6FC050 | (aproximado) sin vídeo: solo quita el fundido a negro (`SetupScreenFadeBackToNormal(0)` 0x6EBB00), como si el vídeo acabara al instante |

| 093 / 094 / 095 START / UPDATE / RELEASE_DUAL_CAMERA | 0x6ED2E0 / 0x6ED370 / 0x6ED410 | cámara doble |
| 105 CREATE_DUAL_CAMERA_WITH_POINT | 0x6ED460 | cámara doble con un punto |
| 201 SHAKE_CAMERA | 0x6EE0F0 | temblor (solo lo dibujado) |
| 142 SET_CAMERA_ZONE / 150 GET_INCLUSION_DISTANCE | 0x6ED890 / 0x6ED990 | zona de la cámara del jugador (cargada; su efecto, pendiente) |
| 209 SET_FIXED_CAM_ROTATION | 0x6EE1A0 | punto de giro fijo del jugador (guardado; su efecto, pendiente) |

Sin portar: el seguimiento del jugador PC 372/373 (0x6EDC00 / 0x6EDCD0).

## Soltar la cámara

**Fiel.** `fn_006ECD70` (END_CAMERA_CONTROL 0x6ECEF0 y la parada de la tarea fn_006ECF20): quita la cámara doble; si el
modo es el del guion lo borra y crea un `CameraModeNew3`, que arranca desde los zoomers actuales (sin salto); el FOV
vuelve **siempre** a 70° en 0,5 s; luego el estado del guion (`Help/ScriptControl`).

## openblack

- `src/Camera/ScriptCamera.{h,cpp}` (`script_camera::`): los zoomers de posición, foco y FOV, el modo del guion
  (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame` (pasos 1-5 y 7) y
  `DrawnCamera` (pasos 3-6). `UpdateCamera` lo hace cada fotograma desde `Game.cpp` y, mientras el modo del guion
  conduce, el modelo del jugador (`DefaultWorldCameraModel`) ni mueve la cámara ni lee teclas (Script no tiene teclas,
  0x44C3BD). Las posiciones y focos van en `Zoomer3d` (`Common/Zoomer.h`, el mismo de la cámara del jugador).
- **Cambio de manos de los zoomers (fiel):** GCamera tiene unos solos zoomers para todos los modos. openblack tiene los
  del jugador (`Camera::GetOriginZoomer/GetFocusZoomer`) y los del guion: `Begin` copia los del jugador tal cual (valor,
  velocidad, destino y tiempo: el modo del guion sigue hacia donde iba el del jugador, 0x461180 no los toca) y `End`
  devuelve los del guion a la cámara (`HandBack`), de donde arranca el jugador como `CameraModeNew3::Initialise`
  0x456640. Con «free start» o los ganchos `OPENBLACK_CAMERA_LOCK/FLY` el jugador nunca soltó la cámara y no se copia.
- `CHLApi.cpp`: los opcodes de la tabla; `StartCameraControl` pasa `cameraTaken = script_camera::Begin(...)`;
  END_CAMERA_CONTROL y la parada de la tarea (Game.cpp) llaman a `script_camera::End`. Al cargar mapa, `Reset`.
- **(aproximado)** Mientras conduce el guion, la cámara del jugador lleva lo dibujado (con el empujón y el metro sobre
  el suelo), no los zoomers del guion. Sin modo de guion, 035 compara los zoomers del jugador con su destino (la misma
  regla 0x441700). `SetPositionAndFocus` no tiene la salida temprana de 0x4438C0. Los ms de juego del fotograma son los del
  reloj de fotograma anterior (el original actualiza la cámara tras los turnos).
- **(inferido)** 284/286 sin modo de guion fijan también la cámara del jugador.
- El FOV va a `config.cameraXFov` (grados) solo cuando su zoomer cambia: un FOV propio del jugador dura hasta que un
  guion toca la lente.
- Mod `game.skip-intro`, «free start» (no original): la tarea de la apertura sí crea el modo del guion (ninguna otra
  coge la cámara mientras la tiene), pero `Drives()` es falso y no mueve nada; sus opcodes de cámara no hacen nada
  ([mod-library.md](mod-library.md#gameskip-intro)).

## Pendiente

- 372/373 (seguir la mano del jugador PC, `GComputerPlayer::GetHandPos` 0x657FE0).
- SET_AVI_SEQUENCE 203 con vídeo: `PlayFullScreenMovie("data\intro.bik")` 0x54D920 pausa el juego y lo devuelve a los
  58 s; sin Bink en openblack no hay película ni pausa (aproximado). La secuencia 2 (vídeo de la caída del hechizo).
- (aproximado) El ángulo de la criatura (LH3DCreature +0x84) no existe: sin ajuste de rumbo y `GetFacingDirection` 0.
  El GameAngle de un aldeano sale de `WallHug::yAngle` redondeado a 2048 por vuelta.
- Cámara del jugador: temblor, zona (recolocar, campo de fuerza) y giro fijo (planes en step3.md §B-D).
- (aproximado) Sin pila de modos: la doble va siempre encima del guion; una doble sobre el modo del jugador se
  aproxima.
- `CameraModeNew3::Reinitialise` 0x4589B0 (cómo recoge el jugador la cámara), sin leer.

## Ganchos de prueba

- `test_script_camera`: un solo modo, llegada, tope de 0,1 s, colocar con T < 0,001, disco, empujón y suelo, FOV con
  tiempo de juego y la vuelta a 70° en 0,5 s; `ScriptCameraFollow.*`: regla de T, distancia y cabeceo, punto desde
  distancia/rumbo/cabeceo, rumbo y cabeceo entre puntos, «detrás», colocar ya, cara de un objeto, cosas que desaparecen.
- `test_script_camera_dual`: la doble (ritmo, foco, distancia, rumbo, con punto, validez, fin del control), el temblor
  (radio, decaimiento, tiradas), el cargador de zonas e `InsideInclusion`.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` ganan a la cámara del guion (`Drives()`, no original).
- En el juego: Land 1 sin el mod `game.skip-intro` (`--mod game.skip-intro=off`) corre FollowUs y CreaturesInGlade
  con la cámara del guion.

## Fuentes

- `dev\tmp_dis\camara\original.md` (paso 1) y `dev\tmp_dis\camara\step2.md` (seguimientos, cara de un objeto,
  SET_AVI_SEQUENCE), `dev\tmp_dis\camara\step3.md` (cámara doble, temblor, zonas, giro fijo), con sus auditorías
  `audit_step1.md` / `audit_step2.md` / `audit_step3.md` en la misma carpeta.
