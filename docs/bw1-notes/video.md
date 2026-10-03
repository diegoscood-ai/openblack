# Vídeos Bink (.bik)

Cómo reproduce runblack.exe v1.42 (W120) sus cinco vídeos Bink y qué tiene openblack: los ficheros y cuándo sale cada
uno, la clase `LHVideoPlayer` y su copia a 16 bits, el vídeo a pantalla completa de `GGame` (ritmo, pausa, pantalla
ancha, fundido, ESC, el mundo 3D sin dibujar), la caída del hechizo, el arranque y la pantalla de carga, el audio de
cada vídeo, y el reproductor de openblack (`src/Video/`, hitos V1-V3) con el plan V4..V8.

- [Los cinco vídeos](#los-cinco-vídeos)
- [binkw32.dll y el contenedor](#binkw32dll-y-el-contenedor)
- [LHVideoPlayer](#lhvideoplayer)
- [La copia a 16 bits (555 / 565)](#la-copia-a-16-bits-555--565)
- [El vídeo a pantalla completa](#el-vídeo-a-pantalla-completa)
  - [Empezar: PlayFullScreenMovie](#empezar-playfullscreenmovie)
  - [Calendario: fundido de 5 s y la intro a 60 s](#calendario-fundido-de-5-s-y-la-intro-a-60-s)
  - [Ritmo](#ritmo)
  - [Cada fotograma: Process3dEngine](#cada-fotograma-process3dengine)
  - [Fin: DeleteVideo y FinishedVideo](#fin-deletevideo-y-finishedvideo)
  - [Pausa y pantalla ancha](#pausa-y-pantalla-ancha)
- [Saltar con ESC](#saltar-con-esc)
- [La caída del hechizo (fall.bik)](#la-caída-del-hechizo-fallbik)
- [Arranque y pantalla de carga](#arranque-y-pantalla-de-carga)
- [El audio de cada vídeo](#el-audio-de-cada-vídeo)
- [openblack](#openblack)
- [Plan V3..V8](#plan-v3v8)
- [Pendiente](#pendiente) · [Ganchos de prueba](#ganchos-de-prueba) · [Fuentes](#fuentes)

## Los cinco vídeos

**Fiel.** Todos son Bink 1 revisión `i` (`BIKi`) y **ninguno tiene pista de audio**. No hay `.smk` ni otros `.bik` en
el install (tampoco en `CreatureIsle\`).

| fichero | px | fps | frames | quién lo pone y cuándo |
|---|---|---|---|---|
| `Data\logo.bik` | 768x512 | 15 | 2 | `PlayLogoScreens` 0x642950 desde `pc_main` 0x641D5D, siempre al arrancar: dos **imágenes fijas** (`BinkGoto` 0x642A2C), antes de que exista GAudio |
| `Data\pre_intro.bik` | 768x512 | 25 | 2515 (100,6 s) | `PlayPreIntroVideo` 0x6426F0 desde `pc_main` 0x641D94, sólo sin perfiles (o con `[0xD46ABD]`, (inferido) una opción de línea de comandos): bucle modal propio |
| `Data\INTRO.bik` | 640x360 | 24 | 1601 (66,7 s) | opcode 203 `SET_AVI_SEQUENCE(1)` → `StartAVISequence(1)` 0x68F450 → `PlayFullScreenMovie("data\intro.bik", NULL)` 0x68F489; en Land 1 lo pide `FollowUs` (`rt_chl_code.txt` 51023) |
| `Data\Spells\fall\fall.bik` | 640x360 | 24 | 1200 (50,0 s) | `SET_AVI_SEQUENCE(2)` → `KickOffFallingSpellVideo` 0x5539A0 → `FallingSpell::Init` 0x526060 → `PlayFullScreenMovie` 0x5261FC |
| `Data\tips.bik` | 768x512 | 15 | 35 | `MakeTipVideo` 0x5F3CE0, pantalla de carga: **35 imágenes** de consejo, una por frame (`BinkGoto(tip + 1)` 0x5F3D5F) |

INTRO.bik, pre_intro.bik y fall.bik tienen **un solo keyframe** (el 0): sólo se descodifican desde el principio, por
eso el original nunca los busca; logo y tips son todo keyframes. Literales: 0xBFEBAC/0xBFEBBC (logo), 0xBFEB80/0xBFEB98
(pre_intro), 0xC0426C (intro), 0xBE9C38 (fall), 0xBF3BB4 (tips); `%c:\%s.%s` 0xBEC348 es la ruta del CD.

## binkw32.dll y el contenedor

**Fiel.** `binkw32.dll` del install: RAD **Bink 1.0w**, 2000-12-03, i386 de **32 bits** (openblack es x64: no se puede
cargar; sólo sirve como oráculo fuera del repo). Importaciones usadas (IAT 0x8A9934..0x8A9964): `BinkOpen` 0x844E8C,
`BinkGetSummary` 0x844EB6, `BinkDoFrame` 0x8450FE, `BinkCopyToBuffer` 0x845146, `BinkNextFrame`, `BinkWait`,
`BinkService` (0x54DAD7, fn_0054AB20, `FallingSpell::Draw` 0x526DE8), `BinkGoto` (tips, logo), `BinkGetRealtime`
0x54DC8D (sólo la cadena `GGame::VideoStatistics` 0xCD3618), `BinkSetSoundOnOff` (pre-intro, dos veces con 0) y
`BinkClose`. `BinkSetSoundSystem`/`BinkOpenDirectSound` sólo están en fn_00844C60, que **nadie llama**; `BinkSetVolume`,
`BinkPause` y `BinkBuffer*` no se importan.

`BinkOpen(nombre, 0x08080000)` 0x844E86: `0x00080000` = `BINKNOSKIP` (no salta frames si va tarde); `0x08000000` =
`BINKNOTHREADEDIO` **(inferido)**, sin efecto observable.

El contenedor (formato público de Bink 1, FFmpeg `libavformat/bink.c`; el exe no lo lee): cabecera de 44 bytes (`BIK` +
revisión, tamaño − 8, frames, frame mayor, frames, ancho, alto, fps como fracción, flags de vídeo, pistas de audio), 12
bytes por pista, `frames + 1` offsets (bit 0 = keyframe, el último = tamaño del fichero) y por frame un paquete (por
pista un tamaño de 32 bits y sus bytes, luego el vídeo). Comprobado en los cinco ficheros (`bik_frames.py`).

## LHVideoPlayer

**Fiel.** Clase de 0x68 bytes (0x844C60..0x845B30), `new(0x68)` en los cuatro sitios que la usan. Campos: +0x00 ancho,
+0x04 alto, **+0x08 fps entero** (`FileFrameRate / FileFrameRateDiv`, `div` sin signo 0x844EC2; 1 si `BinkOpen` falla,
0x844EA1), +0x0C frames, +0x10 frame actual (0 tras `Open`; −1 en tips/logo/pre-intro), +0x14 abierto, +0x18 16 bits
(vale 1 en los cuatro usos), **+0x1C el HBINK**, +0x20 «hay que hacer `BinkNextFrame`», +0x24..+0x3C el mosaico de
texturas, **+0x48 el framebuffer** `ancho*alto*2` bytes a cero, +0x4C..+0x64 lo que guarda `DrawToScreen`.

- `Open` fn_00844E70: `Close`, `BinkOpen`, `BinkGetSummary`, el mosaico de **texturas de 256x256** (640x360 y 768x512 →
  3x2 = 6; `CreateTexture` flags 0x104 en 16 bits) y `[0xEF7514] = this`. Con el 3er argumento (sólo `MakeTipVideo`) y
  la imagen pequeña, una sola textura.
- `DecodeNextFrame` fn_008450B0: sin HBINK nada; `frame > frames` → sólo `++frame` (0x8450C4 `jg`); si no,
  `BinkNextFrame` si +0x20, `BinkDoFrame` y, si da 0, `BinkCopyToBuffer` + `UploadToTextures` fn_00845420 y +0x20 = 1;
  siempre `++frame` 0x845164.
- `DrawToScreen` 0x8456C0 sólo guarda los parámetros; el dibujo lo hace el callback `thedraw` 0x844E30 → fn_00845740,
  un quad por tile con el color de vértice dado.
- Los materiales del mosaico: `CreateMaterial(modo 6, textura)` 0x844FC6 (SRCALPHA / INVSRCALPHA, color y alfa
  MODULATE, sin escritura de Z), `+5 &= ~4` 0x844FD7 (sin repetición: `SetD3DTillingOff` 0x8459B1) y `+5 |= 1` 0x844FE4
  (dos caras: CULLMODE 1 = NONE, 0x8459E4).
- fn_00845740 (leído en `tmp_dis\psys\lh3d.asm`): `w`/`h` 0 → la pantalla (`[0xE85058]`/`[0xE8505A]`, 0x845798..0x8457B5);
  escala `sx = w / ancho`, `sy = h / alto` (0x8457B9..0x8457D3); por tile `x0 = x + tx·256·sx`, `x1 = x0 + n·sx` (igual
  en y), con `n` = 256 salvo la última columna/fila, `ancho & 0xFF` / `alto & 0xFF` (0x84583A..0x845870); u, v de 1/512
  a `n/256 − 1/512` (0x845891..0x84596C: medio texel hacia dentro); el color en los cuatro vértices (0x8458C1..0x8458DB);
  ZFUNC (0x845A16) = **ALWAYS** si el 2º `bool` es 0 (si no LESSEQUAL), ZWRITEENABLE (0x845A49) = el 1er `bool`;
  `DrawAndClip2D` FVF 0x1C4 0x845A7D; al final ZFUNC vuelve a 4 (0x845ADC). `Process3dEngine` pasa los dos `bool` a 0
  (0x54DC56 / 0x54DC58).
- `EnterVideoSection`/`LeaveVideoSection` 0x844C80/0x844CA0: la sección crítica 0xEF74F8 entre el hilo del temporizador
  y el del juego.

## La copia a 16 bits (555 / 565)

**Fiel.** `BinkCopyToBuffer(bink, +0x48, ancho*2, alto, 0, 0, flags)` 0x845146 con `flags = [0xEDD46C] ? 10 : 9`
(0x845119..0x845126): 9 = `BINKSURFACE555`, 10 = `BINKSURFACE565` (7 = 4444 nunca se usa). `[0xEDD46C]` lo pone a 1
`fn_0085D7F0` 0x85D930 sólo si el formato de 16 bits elegido tiene la máscara verde 0x7E0, es decir si la tarjeta no
ofrece X1R5G5B5 → **555 es el camino normal**.

- Oráculo (`dev\_scratch\asistente\video\golden`, la DLL del usuario con la misma secuencia de llamadas): el 555 es
  exactamente `rgb32 >> 3` y el 565 exactamente `R >> 3, G >> 2, B >> 3` del `BINKSURFACE32` del mismo frame, en todos
  los píxeles (frame 0 de los cinco vídeos para el 565; auditoría de patch12).
- Las texturas vuelven a 8 bits al muestrearlas: **(inferido)** por replicación de bits (`(n << 3) | (n >> 2)`), lo
  hace el driver de D3D7, no el exe, y el oráculo no lo mide (sus PNG lo suponen).
- El mismo interruptor `[0xEDD46C]` decide el corte de las `.raw` sin alfa en fn_00837400 (`cmp` 0x8376B9): la rama 555
  0x837765..0x83779F hace `((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3)`; su rama 565 propia 0x8376E3..0x83771E
  hace `(G & 0xF8) << 3`: **el bit bajo del verde siempre es 0** (no es el 565 de Bink).

openblack: `src/Graphics/Rgb16.h` (`graphics::rgb16`): `Pack555` (rama 555 y BINKSURFACE555), `Pack565` (BINKSURFACE565,
`G >> 2`), `PackRaw565` (la rama 565 de las .raw), `Expand5/6` (replicación, inferido), `Quantize/Expand` sobre spans.
`test_rgb16` emula las dos ramas de fn_00837400 instrucción a instrucción.

## El vídeo a pantalla completa

El de INTRO.bik y fall.bik. Campos de `g_game` (0xD0195C): **+0x250188** el `LHVideoPlayer*` (≠ NULL = hay vídeo; es lo
que lee el audio), **+0x25018C** frame en que empieza el fundido, **+0x250190** frame final, **+0x250194** alpha (float),
+0x250530 «es la intro». Globales: `VideoFramesReady` 0xD01988, `VideoFinished` 0xD0198C, 0xD01990 «música del vídeo
arrancada», 0xD01994 su banco, 0xD01998 el temporizador, `VideoPreviousPause` 0xD0199C, 0xD019A0 la pantalla ancha de
antes, el byte 0xD01984 «no se puede saltar», `FallingSpellVideo` 0xCD3B10, `VideoLetterboxScale` 0xBEC16C = 1.0.

### Empezar: PlayFullScreenMovie

**Fiel.** 0x54D920 (`ecx` = g_game, `ret 8`; el 2º argumento es un **nombre de banco de audio**, no el `const Rect&` de
symbols.txt, y los dos llamadores pasan NULL):

1. `ClearTipVideo` 0x54D923; +0x250530 = 0; `DeleteVideo` del anterior (0x54D939); alpha = 1.0 (0x54D946).
2. `[0xCD3B20]+0x1C = −1` 0x54D963: el GAudio (el mismo `ecx` de `LHBankRegister` y `StartScriptMusic`), +0x1C =
   `GameMusic::_alignmentType`, «la música que suena» olvidada.
3. `VideoPreviousPause = (flags >> 2) & 1` 0x54D95C y `PauseGame(1)` 0x54D96A.
4. El banco: libera el anterior (fn_00428640) y registra el nuevo (fn_00428620 → `LHBankRegister`, nulo con NULL).
5. `fn_0054AB20(file, 5)` 0x54D9A5: con `[0xD46AB8] ≠ 0` prueba antes `%c:\nombre.ext` en el CD; `VideoFinished = 0`,
   0xD01990 = 0, `VideoFramesReady = 0`; `new(0x68)` → +0x250188 **aunque el fichero no abra**; `Open(path, 1, 0)`;
   **fin = frames** (0x54AC4E) y **fundido = frames − fps·5** (0x54AC70); un *pre-roll* de hasta 10 `BinkWait`/
   `BinkService`/`Sleep(0)` que no descodifica nada.
6. `fn_0054AB00` 0x54D9AD: `timeSetEvent(16 ms, resolución 5, 0x54AAE0, periódico)`.
7. `0xD019A0 = HelpSystem+0x45E8` 0x54D9BE y, si no había pantalla ancha, `HelpSystem::SetWideScreen(1, 0)` 0x54D9E4.
8. `HelpSystem` fn_005C6C40 0x54D9EF, **siempre** (también si la pantalla ancha ya estaba puesta): +0x45F0 = −FLT_MAX
   (0x5C6C40), así que `GetWideScreenPercentage` 0x5C6B60 = |t·0,001/wideScreenTime| limitado a [0, 1] da 1 en el acto:
   las barras salen enteras en el primer frame y siguen así (en pausa se suma 0, fn_005C6BB0). Al acabar,
   `SetWideScreen(0)` deja +0x45F0 = (1 − 1)·2000 = 0 y las barras se van en 2 s de reloj de juego.

Si el fichero no abre: fps 1, frames 0 → fin 0 y el siguiente `Process3dEngine` lo borra (0 ≥ 0).
Si un vídeo sustituye a otro, `VideoPreviousPause` y 0xD019A0 se toman **ya en pausa y con pantalla ancha**: al acabar el
segundo el juego sigue en pausa (fiel por lectura, sin verlo en el juego).

### Calendario: fundido de 5 s y la intro a 60 s

**Fiel.** Por defecto el vídeo dura hasta su último frame con 5 s de fundido. `StartAVISequence(1)` 0x68F450, si hay
reproductor, pisa el calendario: **fundido = fps·58** (`fps*7`, `+fps*28`, `*2`, 0x68F4A1..0x68F4AF) y **fin = fps·60**
(`imul 0x3C` 0x68F4C3), +0x250530 = 1, y `SetupScreenFadeBackToNormal(0)` 0x68F4E9 (sólo si hay reproductor, que siempre
lo hay). INTRO.bik: fundido de 1392 a 1440; **los frames 1440..1600 nunca se ven**. fall.bik: **sin fundido propio**
(`FallingSpell::Init` pone fundido = fin, 0x5262C4..0x5262CF); se funde sólo al acabar el hechizo (48 frames).
`SET_AVI_SEQUENCE(2)`: `KickOffFallingSpellVideo` y `SetupScreenFadeBackToNormal(0)`.

### Ritmo

**Fiel.** El temporizador de 16 ms (callback 0x54AAE0, comprueba el id 0xD01998) llama `VideoPoll(1, 1)` 0x54AA40: si
`frame > fin` (0x54AA6A `jle`) `DeleteVideo`; si no, `BinkNextFrame` pendiente, y si `BinkWait` dice que toca,
**un** `DecodeNextFrame`, `fn_0054A9B0` (la música del banco a partir del frame 3; nunca con banco NULL) y
`++VideoFramesReady`. Es decir, Bink marca la hora y el temporizador descodifica como mucho un frame cada 16 ms, en
otro hilo, con `BINKNOSKIP`.

### Cada fotograma: Process3dEngine

**Fiel.** `GGame::Process3dEngine` 0x54DA80, tras `LH3DRender::StartFrame` 0x54DAB5 y dentro de la sección del vídeo:

1. `BinkService` 0x54DAD7; si `VideoFinished`, `FinishedVideo` 0x54DAE7. Sin vídeo, directo al punto 6.
2. **alpha = 1.0 cada fotograma** (0x54DB05), antes de mirar nada.
3. `frame ≥ fin` (0x54DB10 `jl`) → alpha = 0, `DeleteVideo`, al punto 6.
4. `frame > fundido` (0x54DB2D `jle`) → si la pausa no es `VideoPreviousPause`, `PauseGame(VideoPreviousPause)`
   0x54DB42 (el juego vuelve a correr durante el fundido) y
   **alpha = 1 − (f − ini) / (|fin − ini| + 1)** (0x54DB4A..0x54DB7F: enteros, `fild`/`fidiv`/`fsubr 1.0`, FPU a 24
   bits).
5. Si alpha es exactamente 1.0 (comparación de bits), no es el hechizo y no hay frame nuevo: hasta **1000 ×
   (`VideoPoll(0, 0)` + `Good_sleep_us(500)`)** = 0,5 s de espera (0x54DB9E..0x54DBD1). Luego, si sigue habiendo vídeo:
   - barras: `ftol((H − W·0.5625) · VideoLetterboxScale) / 2` (0x54DBEB..0x54DC0F, `cdq; sub; sar`, con signo,
     **sin recortar a 0**: en pantallas más anchas que 16:9 salen negativas y la imagen se sale por arriba y abajo;
     `ScreenFade::LetterboxHeight` fn_0081E8B0 sí recorta, no es la misma);
   - color: `0x00FFFFFF | ftol(base · alpha) << 24`, base **0xFF**, o **0x50** con `FallingSpellVideo`
     (0x54DC11..0x54DC4D, `__ftol` trunca);
   - `DrawToScreen(color, 0, barras, W + 1, H − 2·barras + 1)` 0x54DC6D (W, H = `[0xE839E4]`/`[0xE839E8]`),
     `VideoFramesReady = 0`, la cadena de estadísticas.
6. **El mundo 3D no se dibuja** si hay vídeo, alpha == 1.0 y no es el hechizo (0x54DD5E..0x54DD7D → 0x54E2A4). Lo que
   viene después sí se hace: `GScript::ProcessFade` / `Temple::UpdateFade` y `HelpSystem::Draw3D` (barras de cine y
   textos) 0x54E2D7..0x54E2ED. Durante el fundido (alpha < 1) el mundo se dibuja y el vídeo se mezcla encima.

### Fin: DeleteVideo y FinishedVideo

**Fiel.** `DeleteVideo(bool)` 0x54A940 (el `bool` no se usa): destruye el reproductor, +0x250188 = 0,
**`++VideoFinished`**, 0xD01984 = 0, `timeKillEvent`, 0xD01998 = 0, 0xD01990 = 0. Lo llaman `Process3dEngine` (fin),
`VideoPoll` (pasado el fin), el salto, `PlayFullScreenMovie` y `fn_0054AB20`.
`FinishedVideo` 0x54D8D0, el fotograma siguiente: `PauseGame(VideoPreviousPause)`; si `HelpSystem+0x45E8 ≠ 0xD019A0`,
`SetWideScreen(actual == 0, 0)` 0x54D902; 0xD01984 = 0; `VideoFinished = 0`. Como `fn_0054AB20` pone `VideoFinished = 0`
después de su `DeleteVideo`, un vídeo sustituido no llama `FinishedVideo`.

### Pausa y pantalla ancha

**Fiel.** `PauseGame` 0x54AE20: nada si ya está así o en multijugador; cambia el bit 2 de `flags` y para o arranca el
reloj del juego (`GameClock` +0x205D68). Durante el vídeo opaco el juego está **en pausa** (no hay turnos; en `EndTurn`
sólo `AtmosProcess(0)`); en el fundido vuelve la pausa de antes. La pantalla ancha va con dueño 0 (`+0x45EC = 0`).

## Saltar con ESC

**Fiel.** `GGame::ProcessKey` 0x63EF20, tecla `LH_KEY 1` (ESC; índice 0 de la tabla 0x63F6B8 → 0x63F6A0 → 0x63F3B9):

- Con `[0x9A161C]` (0x10) o `[0x9A161E]` (0x20) en los modificadores → nada (0x63F3C6 / 0x63F3D6). **(inferido)** Shift
  y Ctrl: la misma función los traduce a DIK 0x2A / 0x1D en 0x63F2C6..0x63F2EC (y `[0x9A1620]` 0x40 → 0x38 = Alt, que
  el salto no mira). Son constantes (nadie las escribe).
- Con vídeo y sin el byte 0xD01984 → `fn_0054DA00` 0x63F3F5 y `GAudio::StartScriptMusic(0)` 0x63F402 (la música del
  guion se suelta). Sin vídeo, el ESC normal (0x63F4D7).
- Antes de la tabla, ProcessKey tiene otros caminos de ESC que salen sin llegar aquí (0x63EF7A..0x63F2A4): la caja de
  `+0x205A10`, en la tierra 6 el fundido de vuelta y el tutorial, una `SetupBox` o diálogo activos, dentro de la
  ciudadela. **Pendiente** de leer del todo; ninguno existe en openblack.

`fn_0054DA00`: con `FallingSpellVideo` → `EndFallingSpellVideo` 0x553A10 y nada más. Si no, con vídeo: si ya está en el
fundido (`frame > ini`) → `DeleteVideo` ya; si no, **ini = frame** y **fin = min(frame + 48, frames)** (0x54DA47
`add 0x30`, `jle` con signo; 2 s a 24 fps) y `PauseGame(VideoPreviousPause)` 0x54DA63. El byte 0xD01984 lo pone
`pc_main` 0x641E49 tras la caja del primer perfil y lo borran `DeleteVideo`/`FinishedVideo`: **el primer vídeo de un
perfil nuevo no se puede saltar** (inferido: lo que se ve en el juego).

## La caída del hechizo (fall.bik)

**Fiel (V6, `src/Video/FallingSpellVideo.{h,cpp}`).** Leído entero: `KickOffFallingSpellVideo` 0x5539A0,
`EndFallingSpellVideo` 0x553A10, `FallingSpell::Init` 0x526060, `Close` 0x5264A0, la actualización sin símbolo 0x526E00
(la llama `fn_00553A60` 0x553A6A), `Draw` 0x5267D0, la retrollamada 0x526480 → 0x526530 y `Temple::UpdateFade` 0x794280.

- **Arranque.** `SET_AVI_SEQUENCE(on, 2)` → `StartAVISequence` 0x68F45F → `KickOffFallingSpellVideo` y siempre
  `SetupScreenFadeBackToNormal(0)` 0x68F471. KickOff **no hace nada** si el jugador local no tiene criatura
  (0x5539A5..0x5539C0: `g_game + 0xA64 + 0xA60·[+0x205A59]` = `players[PlayerIndex].creature`, `GPlayer` de 0xA60 bytes
  desde +0x18 con `creature` en +0xA4C). Si la tiene: `EndFallingSpellVideo` del anterior (0x5539C4), `+0x205A28 = 2`,
  `new(0x40)` (Game.cpp línea 0x1ADD), ctor fn_00527240 (+0 = +4 = 0), `FallingSpellVideo` = el objeto, `Init`.
  `SET_AVI_SEQUENCE(off, 2)` → `StopAVISequence` 0x68F4F7 → `EndFallingSpellVideo` (sin objeto, nada).
- **Init.** Carga la ruta de cámara `data\spells\fall\fall.cm2`, hace una `CreatureFalling` (0x57B8 bytes, vtable
  0x8D8BD8) de la criatura del jugador con sus brillos de mano, `PlayFullScreenMovie("data\spells\fall\fall.bik",
  NULL)` 0x5261FC (**pausa y pantalla ancha como la intro**: el juego sí se para) y, con reproductor: fps ≤ 0 → 0x18,
  la cámara en el punto de la ruta del ms 0 (×0,8), y **+0x25018C = +0x250190** (0x5262C4..0x5262CF): **el vídeo no tiene
  fundido propio** (los 5 s de `fn_0054AB20` se anulan). Luego +0 = 1, +0x10 la posición de cámara guardada, un
  `LH3DSprite` y 16 chispas (+0x34/+0x38), `+0x1C = +0x20 = +0x24 = 0`, un `LightBurst` (+0x3C) que se inicializa dos veces, +0x28 = 0,
  +0x30 = 1.0, +0x2C = 0 y la retrollamada de fin de frame 0x526480 (los destellos de luz, desde el estado 2).
- **Cada frame (modo 2).** `Process3dEngine` 0x54DD83: con `+0x205A28 == 2` (caso 2, 0x54DD9B..0x54DE02) **no se dibuja
  la tierra** (el caso 0 es 0x54DE57): `LH3DAtmos::Update3D`, `g_mode_cleaning = 0`, la actualización 0x526E00, y si no
  hay vídeo o **`+0x20 == 4`** → `EndFallingSpellVideo` 0x54DDD6; si no `FallingSpell::Draw` (el vídeo **primero**, con
  `LHVideoPlayer::thedraw(0)` 0x52689F y alpha base 0x50, luego la criatura que cae `DrawNow` 0x526A42 con su tinte por
  tiempo, y las chispas) y las partículas líquidas. Otros lectores de +0x205A28: `GCamera::Update` 0x44233C..,
  fn_00516CB0, fn_00517080, `AddPlayerSparkles` 0x55264D, fn_005739F0, `Process3dEngine` 0x54E3D2 / 0x54E4BC
  (`Render2D` del clima se salta en modo 2) **(no portados)**.
- **La actualización 0x526E00.** Sin vídeo: `++(+0x20)` y nada más. Con vídeo, `t = frame·1000/fps` (0x526E61, enteros):
  la criatura avanza `t − (+0xC)` (+0xC = 100 tras Init), la cámara por la ruta y `ChangeFov(π/4)`. Sonidos por
  `+0x24` (cada `if` tras el anterior: una sola llamada puede pasar varios): **> 17 450 ms** 151 ScreenRumble (ScriptSfx);
  **> 19 450** 56 S_LasersbeamExplode_02 (Spells) y para 172 (InGame, dueño 1); **> 31 650** 166 G_Creed_01.
  Estado `+0x20`: **0 → 1 a > 13 450 ms** (+0x1C = 1; 168 G_CitadelExplode_01 y 172 G_Volcano_02 dueño 1);
  **1 → 2 a > 37 750** (30 S_HealChakra; 166 dueño 2 con tono 0x85); **2 → 3 a > 43 900**: fundido del templo a blanco
  (`[0xE06024] = 0xFFFFFF`, objetivo `[0xE06020] = 1.0`, actual `[0xC2A150] = 0`, `[0xE06028] = 0`), para 166 (dueños 0 y
  2), **`LHMusicStop(1)` 0x5271B0** (toda la música se funde) y 168; **3 → 4** cuando `[0xE06028] ≠ 0` (el fundido llegó
  al blanco): objetivo 0, actual 1.0, `[0xE06028] = 0` (vuelve del blanco). Bancos: +0x3AC InGame, +0x3B4 Spells,
  +0x3BC ScriptSfx (`sfx_inventory.md` 0x526F6E..0x5271E1).
- **`Temple::UpdateFade`** (0x54E2DE, cada frame salvo que objetivo == actual y el modo no sea 1; modo 3 ninguno):
  avanza `g_delta_time · 0,001` por frame (1,0 por segundo, reloj real, también en pausa). Subiendo, al pasarse:
  `++[0xE06028]`, actual = objetivo y **objetivo = 0** (vuelve a bajar solo); bajando, al pasarse: actual = objetivo,
  `++[0xE06028]` y con objetivo ≤ 0 el color a 0. Escribe `(alpha << 24) + rgb` (alpha 0xFF por encima de 1, si no
  `ftol(actual·255)`) en `[0xFA51D8]` con fn_0053CE60: **el mismo color de fundido de pantalla del guion**. La FPU va
  a 24 bits (fn_007DEE00, llamada tras `FinishFrame` 0x54E426 / 0x54E4D1 y en `EndTurn`): cada paso redondea a `float`
  y se comparan `float`s; un paso que cae **justo** en el objetivo no cuenta (objetivo == actual: `Process3dEngine` deja
  de llamarlo y el fundido se queda) **(inferido: que nada suba la precisión entre medias)**. Al arrancar, `DoLogo`
  (0x5FA0E1..0x5FA0FF, primera vuelta de `GGame::Loop`) pone actual = objetivo = 0 y `[0xE06028] = 0`: en reposo.
- **El final.** `EndFallingSpellVideo`: `+0x205A28 = 0`, `Close` (quita la retrollamada, borra la criatura y la ruta,
  devuelve la **luz de los modelos** [0xEA9E90] (no la cámara; fn_0081E1F0 0x5264F4), libera chispas y destellos), `delete`, `FallingSpellVideo = NULL` y `fn_0054DA00`. Sin
  `FallingSpellVideo` ese salto es el **normal**: fundido de 48 frames desde el actual **con base 0xFF** (el primer
  fotograma, alpha 1.0, tapa el mundo) y la pausa devuelta. Por tiempo: a 43,9 s el blanco sube en 1 s, a ~44,9 s acaba
  el hechizo, el vídeo se funde en 2 s y el blanco baja en 1 s. ESC (`fn_0054DA00` 0x54DA0C) hace lo mismo antes.

## Arranque y pantalla de carga

**Fiel por lectura; pendiente en openblack (no hay front end ni pantalla de carga).**

- `PlayLogoScreens` 0x642950: `Open(logo.bik, 1, 0)`, dos pasadas con `BinkGoto`, `DecodeNextFrame`,
  `UploadToTextures` y fundido con un `Zoomer`; sin audio.
- `PlayPreIntroVideo` 0x6426F0: bucle propio (no usa `GGame`): `frame = −1`, `BinkSetSoundOnOff(0)` 0x6427AB,
  `trailer.sad` por `LHMusicPlay` 0x642806, cursor escondido; por vuelta espera a `BinkWait` (hasta 50000 × 0,5 ms),
  descodifica, `DrawToScreen(0xFFFFFFFF, 0, 0, W, H)` (sin barras), `Flip`; sigue mientras `frame < frames − 1` y
  `[0xE85474] ≠ 1` (inferido: una tecla). Al acabar `BinkSetSoundOnOff(0)` otra vez, `LHMusicStop(0)` 0x642907.
- `MakeTipVideo` 0x5F3CE0: `Open(".\data\tips.bik", 1, 1)` (una textura), `BinkGoto(consejo + 1)`, una imagen.
  `ClearTipVideo` 0x5F3D90 lo borra (también desde `PlayFullScreenMovie`).
- `fn_0054AD00` (reproducción bloqueante con `VideoTimerSection`) no tiene llamadores.

## El audio de cada vídeo

**Fiel** (sesión *audio*, `video_audio.md`). Ningún `.bik` suena: todo el sonido sale de LHaudio.

| vídeo | audio |
|---|---|
| logo | ninguno (GAudio aún no existe) |
| pre_intro | `audio\music\intro\trailer.sad` por LHMusic (vol 127, sin fundido), cortado en seco con `LHMusicStop(0)` |
| tips | ninguno |
| intro | el juego en pausa → ambiente apagado (`AtmosProcess(0)`); la música que sonaba sigue (en Land 1, `intro.sad` de `START_MUSIC(54)`, si no ha acabado); en el fundido `ProcessMusic` sale por «nada» sin parar la pista y no hay `LHAtmosProcess(1)` (0x427DF8 / 0x4270B1 leen +0x250188) |
| fall | igual, más los efectos de `FallingSpell::Draw` a tiempo del vídeo (`t = frame·1000/fps`) y `LHMusicStop(1)` a 43,9 s |
| ESC | `StartScriptMusic(0)`: la música del guion se suelta |

## El descodificador (V5): FFmpeg + los colores de binkw32

**Fiel (comprobado bit a bit).** La imagen sale idéntica a la de `binkw32.dll` 1.0w del juego en los 55 frames de oro
de los cinco vídeos (0,1,2,10,100 y el último de INTRO/pre_intro/fall; todos los de logo y tips): 19 673 088 téxeles
555 iguales, el 565 del frame 0 de los cinco también, y el RGBA8 igual al `BINKSURFACE32` del oráculo; también por
`BinkGoto` (tips/logo en orden revuelto). Prueba: `dev\_scratch\asistente\video\ffmpeg\check\` y su `README.md`.

- **Descodificador:** el `bink` de libavcodec (FFmpeg 7.1.2) alimentado con los paquetes de `BikFile`: `codec_tag` =
  `BIK` + revisión, tamaño de la imagen y los 4 bytes de flags de vídeo de la cabecera como *extradata* (lo mismo que
  daría `libavformat/bink.c`; no se usa libavformat). Da YUV 4:2:0 y sus planos coinciden con los de RAD en todos los
  frames de oro.
- **Los colores de BinkCopyToBuffer** (dentro de la DLL, no del exe; reconstruidos de los frames de oro):
  croma **sin interpolar** (un U/V por bloque 2x2) y cuatro tablas 16.16 truncadas hacia cero por separado, sumadas a
  una luma con *floor* y luego recortadas a 0..255:
  `y' = max(0, 76309·(Y−16) >> 16)` (Y < 16 → 0; Y > 235 no se recorta);
  `R = y' + trunc(104597·(V−128)/65536)`; `G = y' + trunc(−25675·(U−128)/65536) + trunc(−53279·(V−128)/65536)`;
  `B = y' + trunc(132202·(U−128)/65536)`. Son las constantes BT.601 de rango limitado de siempre **salvo la de B**:
  la clásica 132201 falla en U = 70 (RAD da −117). R, G y B quedan determinados por (Y, U, V) en los 449 685 casos
  distintos de los frames de oro y el modelo acierta todos.
- **(aproximado)** Los datos fijan cada constante sólo a un intervalo (Y 76305..76309, Rv 104579..104605, Bu
  132202..132221, Gu 25674..25683, Gv 53248..53302); dentro de ellos las tablas sólo cambian en cromas extremos. En los
  cinco vídeos enteros U va de 16 a 212 y V de 40 a 219: el único caso dudoso que aparece es **V = 219** (10 muestras
  de croma en todo pre_intro.bik), donde G podría ser 1 menos (`k_GreenFromV` en `src/Video/BinkYuv.h`).
- **FFmpeg recortado:** `vcpkg-overlay-ports/ffmpeg` (el port 7.1.2#3 de vcpkg con
  `--disable-everything --disable-network --enable-decoder=bink --enable-demuxer=bink --enable-protocol=file`, sin
  aceleración por hardware ni Media Foundation) y en `vcpkg.json` sólo la *feature* `avcodec`. En Windows x64:
  `avcodec-61.dll` + `avutil-59.dll` (~1,2 MB), copiadas junto al exe por el paso *applocal* de vcpkg (como `lua.dll`).
  Primera compilación ~25 min (casi todo el `configure` en msys).
- **Licencia:** sin `gpl`/`version3`/`nonfree` FFmpeg es **LGPL-2.1-or-later** (`libavcodec/bink.c` incluido),
  compatible con la GPL-3 de openblack; el overlay aborta si se pide alguna de esas *features*.

## openblack

Hitos V1 y V2 (borrador patch12, sesión *asistente*; inertes hasta V4: nadie llama todavía a `Play`).

- `src/Video/BikFile.{h,cpp}` (**V1**, fiel al formato): lee y valida el contenedor como `bik_frames.py` (firma `BIK`,
  imagen y fps no nulos, tablas dentro, offsets crecientes con el último = tamaño, tamaños de audio); `Fps()` = la
  división entera de 0x844EC2; `FrameData`/`VideoData`/`AudioData`, keyframes.
- `src/Video/VideoDecoder.h`: `IVideoDecoder` (`Open`, `DecodeNext(i)` → RGBA8; vacío = el frame falló y se queda la
  imagen anterior) y `NullVideoDecoder` (negro opaco: para los tests, y de reserva si el descodificador rechaza una
  película válida, que entonces se ve en negro con su pausa, fundido y salto).
- `src/Video/FfmpegDecoder.{h,cpp}` (**V5**, fiel: ver arriba) y `src/Video/BinkYuv.h` (las tablas de color de
  binkw32): el descodificador por defecto de `GameHooks()`. `DecodeNext(i)` en orden descodifica un paquete; otro `i`
  (`BinkGoto`) vuelve al último *key frame* <= i y descodifica desde ahí.
- `src/Video/VideoPlayer.{h,cpp}` (**V2**): `video::VideoPlayer` con los campos del original y su dirección. `Play` =
  `PlayFullScreenMovie` + `fn_0054AB20` (el reproductor existe aunque no abra); `SetSchedule`/`ScheduleIntro` = 58·fps /
  60·fps; `Process(realMs)` = `Process3dEngine` 0x54DAB5..0x54DD76 + `VideoPoll` + `DecodeNextFrame`; `Skip` =
  fn_0054DA00; `EscapeKey(shift, ctrl)` = ProcessKey 0x63F3B9..0x63F402; `Stop` = `DeleteVideo`; `FinishedVideo`
  privado; `IsPlaying()` atómico (para el audio, desde cualquier hilo); `CoversScreen()` = 0x54DD5E; `GetFrame()` (RGBA8
  como se muestrea, el búfer de 16 bits, color de vértice, `serial`). Funciones puras `FadeAlpha`, `VertexColour`,
  `FullScreenRect` (barras sin recortar), `FramesDue`. `video::Get()` / `video::IsPlaying()` con `GameHooks()`:
  `game_clock::Pause/IsPaused`, `help::Get()->SetWideScreen(on, 0)` (que mueve las barras de `ScreenFade` y avisa al
  audio con dueño 0) y `game_music::ScriptStopMusic`.
- `src/Video/FallingSpellVideo.{h,cpp}` (**V6**): `video::FallingSpellVideo` (el `FallingSpell` en lo que toca al vídeo,
  y +0x205A28): `KickOff` (0x5539A0, con el gancho `hasCreature`: **(inferido)** una entidad `Creature` de `PLAYER_ONE`),
  `Start` (lo de después de la prueba; lo usa `OPENBLACK_TEST_VIDEO=fall`), `End` (0x553A10 → `VideoPlayer::Skip`),
  `ProcessFrame(realMs)` (caso 2 + `Temple::UpdateFade`), `Update` (0x526E00), `HidesWorld()` (modo 2), `State()`,
  `SoundState()`; `TempleFade` puro (0x794280, en `float` como la FPU a 24 bits). Ganchos (`GameHooks()`):
  `sound(FallingSpellSound)` → `audio::PlaySoundEffect(PlayOptions)` 2D (dueño `Owner::None()` / `Key(1)` / `Key(2)`,
  tono 133 sólo en 0x527119) o `audio::StopSoundEffect(muestra, dueño, banco)`; `musicStop(1)` → `audio::MusicStop(1)`;
  `setScreenFadeColour` → `ScreenFade::SetColour` (nuevo, fn_0053CE60). `VideoPlayer::GameHooks().endFallingSpellVideo` → `End()`. Opcode 203
  (`CHLApi.cpp`): secuencia 2 con `on` → `KickOff()` + `FadeBackToNormal(0)`, sin `on` → `End()`, tras el `FreeStart()`
  como V4. `Game.cpp`: `GetFallingSpell().ProcessFrame(FrameRealMs())` justo tras `video::Get().Process`.
  `Renderer::DrawScene`: con `HidesWorld()` el mismo camino que `CoversScreen()` (sólo el vídeo y las capas finales).
- `src/Game.cpp`: `video::Get().Process(game_clock::FrameRealMs())` tras `UpdateRealClock()` (después de los turnos,
  como `Process3dEngine` tras el bucle de turnos); en `ProcessEvents`, ESC con vídeo → `EscapeKey` (sin vídeo sale de
  openblack como siempre: el ESC de openblack no es el del original).
- `src/Graphics/Renderer.cpp` (**V3**, sesión *sistemas*): `Renderer::DrawVideoOverlay` en `RenderPass::ScreenOverlay`,
  tras el mensaje de la mano, en el orden de `LH3DRender::FinishFrame` 0x82F460 (`Renderer::DrawFinishFrameOverlays`):
  primero las barras si pct ≠ 0 (0x82F652..0x82F6DD, fn_0081E590 dos veces, alto (int)((h − w·0,5625)·pct)/2 de
  fn_0081E8B0), luego las retrollamadas con el bit 0x80000000 (0x82F6E5..0x82F718), entre ellas `thedraw` 0x844E30
  (registrada con 1 en 0x54B62D; `RegisterFinishFrameCallback` 0x82F2C0 pone el bit), y al final el fundido del guion
  fn_0086FEE0 (0x82F753), que vuelve a pintar las barras encima de su color. El vídeo tapa las barras justo en su borde:
  con pct = 1 el rectángulo de `FullScreenRect` encaja exacto entre ellas. Una textura RGBA8 `clamp` del tamaño del
  vídeo, rehecha si cambia el tamaño y subida con `updateTexture2D` sólo cuando cambia `serial` (`UploadToTextures`
  0x84514E). Los quads de fn_00845740 tile a tile sobre `video::FullScreenRect` (0x54DBEB..0x54DC6D) con los **mismos
  texels** que cada tile de 256x256 (medio texel hacia dentro, sin repetición: el filtro no llega al tile vecino), el
  color `Frame::colour` (0x54DC11..0x54DC4D) como color de vértice, y el estado `render_modes::State` del material modo 6
  de dos caras con ZFUNC ALWAYS y sin Z. Programa `WorldQuad` (vs_blob + fs_world_quad: color = textura × difuso,
  alfa = `s_alpha.r` × difuso) con una textura R8 1x1 blanca como `s_alpha`: **no hay shader nuevo**.
  `Renderer::DrawScene`: con `video::Get().CoversScreen()` (0x54DD5E..0x54DD7D → 0x54E2A4) no se dibuja nada del mundo
  (ni sombras, ni reflejo, ni cielo, ni el mensaje de la mano); sólo el vídeo y `DrawScreenOverlay` (el fundido del guion
  y las barras, 0x54E2D7..0x54E2ED), en el mismo orden barras, vídeo, fundido. Comprobado en el juego: con `fall` el mundo sale × 0,686 = 1 − 0x50/255 en todos
  los píxeles medidos; con `intro` la pantalla es el negro del descodificador nulo (sin el azul del borrado).

Diferencias:

- **(aproximado)** Ritmo: no hay hilo de 16 ms ni `BinkWait`; `Process` descodifica todos los frames que tocan por el
  reloj de pared desde el primer `Process` (frame i a i·den·1000/num ms). Mismos frames a las mismas horas; tras un
  parón largo openblack descodifica de golpe lo atrasado (sólo se ve el último) donde el original iría de uno en uno
  cada 16 ms.
- **(aproximado)** La espera de hasta 0,5 s (0x54DB85..0x54DBD1) no bloquea: se queda la última imagen.
- Fiel desde V5: el paso a 16 bits parte del RGBA8 del descodificador y da el mismo 555/565 que el YUV→555 de la DLL
  (comprobado contra los frames de oro).
- **(inferido)** Un salto por pulsación: las repeticiones de tecla de SDL se ignoran.
- V6: las 16 chispas (bocanadas de smoke.raw), el `LightBurst` y la ruta `fall.cm2` (calculada, aún sin aplicar a la
  cámara) los porta milagros2 (miracles.md, «La caída del hechizo»). Sin portar: la `CreatureFalling`, la cámara con
  `ChangeFov(π/4)`, el tinte de la criatura y los `SetScalePowerTime` de `Draw`, `LH3DAtmos::Update3D`/`Render2D`, los otros
  lectores de +0x205A28 y la reescritura de +0x1C por `Draw`. Lo que se ve en openblack en modo 2: el vídeo al 31 % sobre
  **(inferido)** el color de borrado de openblack (no se ha leído qué queda debajo en el original: no hay borrado
  identificado en `StartFrame`).
- **(aproximado)** V6: el fps ≤ 0 → 0x18 de `Init`/0x526E4E no se escribe en el reproductor (se usa al calcular `t`;
  `BikFile` no abre fps 0).
- **(aproximado)** V6: en modo 2 el original dibuja el vídeo dentro de `Process3dEngine` (`thedraw` 0x52689F) y las
  bandas y el fundido de `FinishFrame` van encima; openblack dibuja ahora el orden del original en modo 2: vídeo, bocanadas, destellos, bandas, fundido. Misma imagen con las bandas
  al 100 % (el *letterbox* del vídeo es su altura). El frame en que corre `EndFallingSpellVideo` el original aún dibuja
  el vídeo con base 0x50 (el color lo guardó `DrawToScreen` 0x54DC6D); openblack, con 0xFF (bajo el blanco casi opaco).
- No portado: el banco de sonido (los dos llamadores pasan NULL), `ClearTipVideo`, la ruta del CD, la cadena de
  estadísticas, el mosaico de 256x256 (una sola textura con los mismos texels por tile), y `GAudio+0x1C = −1` (audio no
  tiene cómo; `ProcessMusic` lo repite en el fundido).
- fn_005C6C40 es `ScreenFade::SnapWideScreen`, por el gancho `snapWideScreen` de `VideoPlayer::Hooks`, que `Play` llama
  siempre, como el original (0x54D9EF, tras el salto de 0x54D9D2): también si el guion ya tenía la pantalla ancha.
- **(inferido)** V3: el alfa de las texturas de 16 bits es 1 (`CreateTexture` flags 0x104; un A1R5G5B5 con el bit 15 a 0
  de Bink no se vería); el filtro bilineal del driver; los píxeles con el centro de bgfx,
  sin el medio píxel de D3D7 (como los demás rectángulos de `ScreenOverlay`).
- **(aproximado)** V3: mientras el vídeo tapa la pantalla openblack borra a 0x274659 como siempre (el original no borra);
  sólo se ve fuera del rectángulo del vídeo, en pantallas que no son 16:9 y antes de que lleguen las barras.

## Plan V3..V8

| hito | qué | dueño / bloqueo |
|---|---|---|
| V3 | **Hecho** (sesión *sistemas*): `Renderer::DrawVideoOverlay`, el mundo sin dibujar con `CoversScreen()`, `OPENBLACK_TEST_VIDEO`; sin shader nuevo (`WorldQuad`) | — |
| V4 (hecho, sesión asistente) | Opcode 203 `SetAviSequence` (`CHLApi.cpp`): secuencia 1 → `video::Get().Play(data\intro.bik)` + `ScheduleIntro()` antes del `FadeBackToNormal(0)`; el `FreeStart()` del mod `game.skip-intro` se queda (sin vídeo) | `CHLApi.cpp` compartido |
| V5 | **Hecho**: FFmpeg recortado (`--enable-decoder=bink`, sólo LGPL) detrás de `IVideoDecoder`, bit a bit igual a binkw32 en los frames de oro | — |
| V6 | **Hecho** (sesión *asistente*): `fall.bik` con `FallingSpellVideo` (modo 2, sin fundido propio, estados y sonidos por el tiempo del vídeo, el blanco del templo, fin a estado 4), opcode 203 secuencia 2, ESC; sin la criatura que cae ni la cámara; sonidos y `LHMusicStop(1)` por `Audio.h` | la criatura, la cámara y los destellos: *milagros* |
| V7 | `tips.bik` en la pantalla de carga | bloqueado: no hay pantalla de carga |
| V8 | `logo.bik` y `pre_intro.bik` al arrancar, con `trailer.sad` | bloqueado: no hay front end ni perfiles; audio de *audio* |


**V4 (hecho):** `SET_AVI_SEQUENCE(on, 1)` llama a `video::Get().Play(FindPath("Data/intro.bik"))` y `ScheduleIntro()` (58/60 s) y
quita el fundido (0x68F477..0x68F4E9); una carga de mapa para la película que suene (`GGame::ClearVariables` 0x54BF28). Con
«free start» del mod `game.skip-intro` no hay película. **Pendiente de probar en el juego:** en Land 1 sin el mod, FollowUs
no llega todavía al 203 (en 4 min de juego se queda antes, con opcodes sin portar como DANCE_CREATE); la película se
ve ya con `OPENBLACK_TEST_VIDEO=intro`.

## Pendiente

- V4..V8 (tabla de arriba); `parity.md` («Vídeo Bink») está «en curso» desde V3.
- `OPENBLACK_VIDEO_TRACE` no existe todavía.
- *audio*: conectar `GameQueries::videoPlaying` a `video::IsPlaying()` (`MakeMusicQueries` en `Game.cpp` y las queries
  de `AudioSystem`); un modo de olvidar `GameMusic::_alignmentType` para 0x54D963.
- ProcessKey 0x63EF7A..0x63F2A4: los otros caminos de ESC, sin leer del todo.
- Comprobar en el juego que el primer vídeo de un perfil nuevo no se salta, y con `OPENBLACK_TEST_VIDEO=fall` el
  blanco a 43,9 s, el fin a ~44,9 s y el fundido opaco de 48 frames.
- Oír en el juego los 11 sonidos y la parada de música a 43,9 s (`OPENBLACK_TEST_VIDEO=fall`).
- *milagros*: la `CreatureFalling`, la cámara de `fall.cm2`, las chispas y los destellos de `FallingSpell::Draw`.

## Ganchos de prueba

- `test_bik_file` (6): ficheros sintéticos (formato, audio, 13 rechazos) y los cinco `.bik` reales con
  `OPENBLACK_TEST_GAME_PATH` (u `OPENBLACK_TEST_BW_ROOT`); sin carpeta se saltan 2.
- `test_video_player` (19): ganchos y reloj falsos, `.bik` sintéticos: pausa y pantalla ancha guardadas y devueltas,
  fichero que no abre, ritmo, fundido y fin, calendario de la intro (1392/1440), salto (48 frames, tope, segundo
  salto), ESC con Shift/Ctrl/byte 0xD01984, hechizo, película sustituida, 555 y 565, decodificador nulo, frames
  fallidos.
- `test_falling_spell_video` (15): `FallingSpellFilmMs`, `TempleFade` (sube, se da la vuelta sola, baja, color a 0,
  justo en el objetivo, por encima de 1, cuándo corre), sin criatura nada, KickOff (modo 2, pausa, 0x50, fundido = fin), sin fundido
  propio pasado el frame 1080, estados y los 11 sonidos con sus direcciones, el blanco y el fin a estado 4 (26
  frames de 40 ms, el salto normal de 48), ESC, StopAVISequence, vídeo corto, vídeo que no abre, KickOff dos veces.
- `test_rgb16` (5): las ramas 555 y 565 de fn_00837400 emuladas, expansión, cortes, spans.
- `test_ffmpeg_decoder` (7): las tablas de color y el croma sin interpolar; con la carpeta del juego, el CRC-32 del
  555 de logo.bik (frames 0 y 1, y vuelta al 0), tips.bik por salto (34, 20, 34) e INTRO.bik frame 10 en orden,
  contra los `.bin` de oro de la DLL (sólo los CRC están en el test).
- Juego: `OPENBLACK_TEST_VIDEO=<intro|fall|ruta>` (Game.cpp, al cargar el mapa): `intro` = `Data\intro.bik` +
  `ScheduleIntro()` (60 s), `fall` = `video::GetFallingSpell().Start()` (KickOff sin la prueba de la criatura:
  modo 2, alfa 0x50 sin mundo, estados, blanco y fin; V6), si no la ruta dada. Fotos de V3: `dev\tmp_dis\unify\shots\video\intro_mid.png`
  (frame 1800, el vídeo tapa la pantalla) y `fall_mid.png` (frame 1500, el mundo × 0,686).

## Fuentes

- `dev\tmp_dis\video\original.md` (lectura del original) y `dev\tmp_dis\video\plan.md` (decisión y plan V0..V8).
- `dev\tmp_dis\audio\video_audio.md` (sesión *audio*: el audio de cada vídeo).
- `dev\_scratch\asistente\video\golden\README.md` (frames de oro de la DLL; 555 = flag 9 en 0x845146) y
  `oracle\` (el exe de 32 bits que los saca).
- `dev\_scratch\asistente\video\patch12\README.md` y `audit.md` (V1-V2 y su auditoría).
- `dev\_scratch\asistente\video\ffmpeg\README.md` (V5: el overlay, tamaños, la reconstrucción de los colores y
  la comparación con los frames de oro) y `patch5\README.md` (cómo se aplica).
