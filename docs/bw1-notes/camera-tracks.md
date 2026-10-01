# Pistas de cámara (`Data\camera.edt`) y `WALK_PATH`

Código: `src/3D/CameraTracks.{h,cpp}` (lector y evaluación), `src/ECS/MobileWalkPaths.{h,cpp}` +
`ECS/Components/MobileWalkPath.h` (el `DataPath` de un MobileObject), `src/CHLApi.cpp` (`WALK_PATH`,
`GET_WALK_PATH_PERCENTAGE`, `CONVERT_CAMERA_POSITION/FOCUS`). Guiones de RE: `dev\tmp_dis\agua\re\edt_dump.py`
(vuelca las pistas), `emu_walkpath.py` (Unicorn: la cadena original sobre una pista), `cmp_trace.py` (compara con la
traza de openblack).

## El fichero

`GCameraEditor::CreateSegFile` 0x445530 abre `.\Data\camera.edt` una vez (el `LHFile` global 0xC59CF4) y
`LHFile::GetSegment` 0x7BDDD0 lee segmentos por nombre. Formato: `"LiOnHeAd"` y luego segmentos de nombre de 32 bytes
(con ceros), u32 tamaño y los datos. En el disco original (150536 bytes): `EDITOR` (58084 bytes, nombres del editor;
el juego no lo lee), `Cam0`..`Cam555` (32 bytes) y `Track0`..`Track52`. Creature Isle trae el suyo.

- **`Cam%d`** (`GCameraEditor::LoadCameraFromHD` 0x446FE0 copia los 32 bytes tal cual): posición (3 floats), foco
  (3 floats), y dos floats que valen 0 y −1 en las 556. `CONVERT_CAMERA_POSITION` 0x6ED200 mete los floats 0..2,
  `CONVERT_CAMERA_FOCUS` 0x6ED270 los 3..5. Si el segmento no existe el original deja el búfer de la pila sin
  inicializar; openblack devuelve (0, 0, 0).
- **`Track%d`** (`ScriptedCamera::Create` 0x447060 → `ScriptedCamera` de 12 bytes): un u32 (0 en todas; va a +0) y
  dos `LH3DWay`, cada uno con su u16 de tamaño delante; se copian a memoria nueva, `LH3DWay::AdjustPtr` 0x844570
  rehace los punteros y cada uno recibe un `LH3DWay::Running` (0x843ED0): +4 el de la **posición** de la cámara,
  +8 el del **foco**. Si falta: `"Cannot load track No %d"` y devuelve 0.

### `LH3DWay` (0x24 + 44·n bytes)

| Offset | Contenido |
|---|---|
| +0x00 | u16 tamaño; +0x02 u16 = 0x63 en todas |
| +0x04 | n puntos |
| +0x08 | float 0,26 en todas (no lo lee nada de lo de abajo) |
| +0x0C | lo recalcula `AdjustPtr`: Σ, en los tramos con tiempos distintos, dt·(v0 − a·t0 + a/2) con t0 = tiempo[i]·0,001, dt = tiempo[i+1]·0,001 − t0, a = (v[i+1] − v0)/dt; 0,1 si sale 0. Lo usa el `WALK_PATH` de los Living (abajo), no el de los MobileObject |
| +0x10 | duración en ms = ftol(último tiempo) (12212 en Track20, 12884 en Track21) |
| +0x14..+0x20 | punteros (del editor; `AdjustPtr` los rehace desde +0x24) |
| +0x24 | n puntos (vec3), luego n pares de asas (vec3 ×2: las dos asas interiores del tramo i→i+1), n tiempos en ms, n velocidades (unidades/s) |

Cada tramo i es una Bezier cúbica (P[i], asa[i].a, asa[i].b, P[i+1]).

### `LH3DWay::Running` (0x20C bytes)

+0 tramo actual, +0x04..+0x200 tabla de 128 floats, +0x204 último parámetro t, +0x208 el way.

- `fn_00843F00` (tabla del tramo s): longitud total de la Bezier en 127 pasos (t = i·(1/127), i = 0..127, desde P[s]);
  luego tabla[i] = 127·longitud acumulada(i)/total para i = 1..126, tabla[0] = 0, tabla[127] = **1,0** (+0x200).
- `fn_00844280(muestra, &out)`: tramo = el primero i con tiempo[i+1] ≥ muestra (uno nuevo rehace la tabla);
  muestra ≥ duración → último punto (no toca tramo ni t); muestra ≤ 0 → primer punto y t = 0. Si no:
  L = |P[s+1] − P[s]| (la cuerda, `fn_008433E0`), v0 = velocidad[s], dt = (tiempo[s+1] − tiempo[s])·0,001,
  τ = (muestra − tiempo[s])·0,001, a = 2(L − dt·v0)/dt², **u = τ(v0 + aτ/2)/L** (aceleración constante que recorre
  la cuerda justo en tiempo[s+1]). Después k = ftol(127u), lo = tabla[k], hi = tabla[k+1]; si u == lo, t = lo; si
  no, los bucles que buscan la entrada mueven un puntero pero **no** cambian lo ni hi, y t = w·lo + (1 − w)·hi con
  w = (hi − u)/(hi − lo), que es otra vez u (con el redondeo de la escala de la tabla, hasta ~4·10⁻⁴ en t con la FPU
  a 24 bits). Así que la tabla no reparametriza nada. Fuera de la tabla (u < 0 o u > 1, que solo pasa si v0 > 2L/dt;
  en las pistas de los tiburones todas las velocidades son 10) el original lee otros campos del objeto: openblack
  usa t = u. out = la Bezier del tramo en t; t queda en +0x204.
- `fn_008439C0(tramo, t, &out)` es la Bezier sola (la que usa el `WALK_PATH`); suma la x en otro orden que y y z
  (((B2·3ut² + B1·3u²t) + B3·t³) + B0·u³), openblack lo copia.

## `WALK_PATH` (177) de un MobileObject

`GScript::WalkPath` 0x6FBB50 saca (objeto, adelante, pista, desde, hasta). Si el objeto es Living va a 0x5EE100 (abajo);
si es MobileObject (`__RTDynamicCast`) a `fn_006076C0(pista, desde, hasta, adelante)`; si no, "Thing is invalid for move
path". `fn_006076C0`: mete el objeto una vez en la lista g_game+0x205CD4 (= `GlobalGameLists` +0x130) y le pone un
`DataPath` nuevo (0x30) en +0x64: +0x14 = `ScriptedCamera::Create(pista)`, +0x1C = hasta, +0x20 = adelante,
+0x28 = **100** (paso), +0x2C = 1, **+0x24 = desde × duración**.

Cada turno, en `GGame::ProcessTurn`: `Whale::ProcessAll` 0x54E5C7 (inicio de turno = Pos), luego
`GlobalGameLists::Process` 0x5913ED llama a `MobileObject::MoveAlongPath` 0x607790 (vt+0x52C) de cada objeto de la
lista, y después `GScript::Process` 0x54E693 (los guiones: el `WALK_PATH` se ve a partir del turno siguiente):

1. muestra = ftol(+0x24) si adelante, ftol(duración − +0x24) si no; limitada a 0..duración.
2. `fn_00844280` en el Running de la **posición** (su punto se tira) y `fn_008439C0` en el way del **foco** con el
   tramo y t que dejó: **el objeto sigue la curva del foco, con el tiempo de la curva de la posición**.
3. `fn_00607990`: si +0x24 / duración < hasta: +0x24 += 100 (tope en la duración) y
   `SetPos(MapCoords(ftol(x·6553,6), ftol(z·6553,6)), y relativa 0)` + `Game3DObject::SetPosition(coords, 0, 0, 0, 1)`
   (altura = GetAltitude + 0; también pone rotación 0 y escala 1 en el objeto 3D, que el `Draw` del tiburón vuelve a
   poner cada fotograma). Si no, sale de la lista **sin moverse** ese turno (el DataPath se queda).

Unidades: la muestra es el tiempo de la pista en ms y el paso de 100 es un turno de 100 ms, así que la pista se
recorre a tiempo real (Track20: 123 turnos; Track21: 129). Hacia atrás recorre las muestras desde el final; `desde` y
`hasta` siempre se comparan con +0x24 / duración, así que `WALK_PATH(o, 0, 21, 0,25, 0,75)` empieza en la muestra
9663 y para cuando +0x24 llega a 9721 (0,75 de la duración). No hay bucle: al acabar, el objeto se queda donde está.
Un segundo `WALK_PATH` sobre el mismo objeto no lo repite en la lista y sustituye el DataPath (el viejo se pierde).
`ToBeDeleted` 0x606F4A lo saca de la lista (en openblack el componente muere con la entidad).

`GET_WALK_PATH_PERCENTAGE` (179, 0x6FBC50): **1,0** para todo lo que no sea Living; para un Living
`Living::GetWalkPathPercentage` 0x5EE520 = +0xAC→+0x24 / duración. Ningún guion original lo llama.

### Living (no portado)

0x5EE100 (símbolo mal puesto `Animal::DebugText`): borra el DataPath viejo de +0xAC, crea uno con la pista, +0x18 =
pista, +0x1C = hasta, +0x20 = adelante, **+0x28 = duración / (longitud del foco (+0x0C) / (velocidad(+0x5A)/655·0,1))**,
+0x2C = velocidad, +0x24 = desde × duración, y le pone una senda (`AddFootpath`, vt+0x8FC). Lo mueve
`Living::MoveAlongPath` 0x5EE230. Lo usan `FollowUs` (Father, pista 22) y `BlindWomanJourney` (pista 7).

## Los tiburones de Land 1 (`FollowUs`, challenge.chl)

`Shark1Pos = CREATE(Marker, CONVERT_CAMERA_FOCUS(221))`, `Shark2Pos = ...(230)`; `Shark1 = CREATE(Whale, 5000,
GET_POSITION(Shark1Pos))`, `Shark2` igual; `WALK_PATH(Shark1, 1, 21, 0, 1)`, `WALK_PATH(Shark2, 1, 20, 0, 1)`.
Cam221/Cam230 son las cámaras de inicio de esas pistas: su foco es el primer punto del foco de Track21/Track20
((1312,57, 0,74, 2010,79) y (1318,92, 0,74, 2000,23)). Los focos van a y ≈ 0,4..0,9 (se ignora: y relativa 0), a
10 unidades/s, hacia (1427, 2056) y (1430, 2042).

**Verificado**: la traza de openblack (`OPENBLACK_WALK_PATH_TRACE=1`) da los mismos tramos, t y puntos que la
emulación Unicorn de 0x844570/0x843ED0/0x844280/0x8439C0 con la FPU a 24 bits en las
71 muestras comparadas de cada pista, y el recorrido hacia atrás 0,25→0,75 de Track21 acaba en la misma muestra.
Con la FPU a 64 bits t cambia hasta 4·10⁻⁴ (unos milímetros). El original corre a 24 bits, y lo pone él mismo:
`fn_007DEE00` hace `fninit` y borra los bits 8–9 (PC = 00, precisión simple) de la palabra de control (`and 0xFCFF`,
0x7DEE0D); se llama al arrancar (`pc_main` 0x641C6F vía `fn_007DEDD0`), en cada `GGame::EndTurn` (0x54E964, 0x54E974,
0x54E984), en `Process3dEngine` (0x54E426, 0x54E4D1), al abrir el paisaje y al cargar. El `__setdefaultprecision` de
la CRT (53 bits) solo corre antes de todo eso.

Gancho: `OPENBLACK_TEST_SHARK=1` crea los dos tiburones de `FollowUs` y sus `WALK_PATH`;
`OPENBLACK_TEST_SHARK="pista,cámara[,adelante[,desde[,hasta]]]"` uno solo en el foco de esa cámara. Captura
`_audit/agua/paths_sharks1.png`.

`RUN_CAMERA_PATH` (119, 0x6ED7F0) usa la misma pista desde la cámara de guion (`CameraModeScript`, `fn_00461A80`):
el lector está (`LoadCameraTrack`, `CameraWayRunner`), la cámara no.
