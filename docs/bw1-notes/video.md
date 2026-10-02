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
8. `HelpSystem` fn_005C6C40 0x54D9EF, **(inferido)** esconder el HUD.

Si el fichero no abre: fps 1, frames 0 → fin 0 y el siguiente `Process3dEngine` lo borra (0 ≥ 0).
Si un vídeo sustituye a otro, `VideoPreviousPause` y 0xD019A0 se toman **ya en pausa y con pantalla ancha**: al acabar el
segundo el juego sigue en pausa (fiel por lectura, sin verlo en el juego).

### Calendario: fundido de 5 s y la intro a 60 s

**Fiel.** Por defecto el vídeo dura hasta su último frame con 5 s de fundido. `StartAVISequence(1)` 0x68F450, si hay
reproductor, pisa el calendario: **fundido = fps·58** (`fps*7`, `+fps*28`, `*2`, 0x68F4A1..0x68F4AF) y **fin = fps·60**
(`imul 0x3C` 0x68F4C3), +0x250530 = 1, y `SetupScreenFadeBackToNormal(0)` 0x68F4E9 (sólo si hay reproductor, que siempre
lo hay). INTRO.bik: fundido de 1392 a 1440; **los frames 1440..1600 nunca se ven**. fall.bik: fundido de 1080 a 1200.
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

**Fiel por lectura (sin hecho en openblack).** `KickOffFallingSpellVideo` 0x5539A0: `EndFallingSpellVideo` del anterior,
`+0x205A28 = 2`, `new(0x40)` `FallingSpell`, `FallingSpellVideo = obj`, `Init` 0x526060 → `PlayFullScreenMovie` (pausa
y pantalla ancha como la intro). Se dibuja **encima del mundo**, que sí se dibuja, con alpha base 0x50 (31 %);
`Process3dEngine` caso 2 (0x54DD9B..0x54DDE0) llama `FallingSpell::Draw` 0x5267D0 y, si ya no hay vídeo o
`FallingSpell+0x20 == 4`, `EndFallingSpellVideo` 0x553A10: `+0x205A28 = 0`, `Close`, `delete`, `FallingSpellVideo = NULL`
y `fn_0054DA00`. Como ya no hay `FallingSpellVideo`, ese salto es el normal: si el vídeo sigue, 48 frames de fundido
**con base 0xFF** (y el primer fotograma, alpha 1.0, tapa el mundo). Fundido propio: los últimos 5 s (frames 1080..1200).

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

## openblack

Hitos V1 y V2 (borrador patch12, sesión *asistente*; inertes hasta V4: nadie llama todavía a `Play`).

- `src/Video/BikFile.{h,cpp}` (**V1**, fiel al formato): lee y valida el contenedor como `bik_frames.py` (firma `BIK`,
  imagen y fps no nulos, tablas dentro, offsets crecientes con el último = tamaño, tamaños de audio); `Fps()` = la
  división entera de 0x844EC2; `FrameData`/`VideoData`/`AudioData`, keyframes.
- `src/Video/VideoDecoder.h`: `IVideoDecoder` (`Open`, `DecodeNext(i)` → RGBA8; vacío = el frame falló y se queda la
  imagen anterior) y `NullVideoDecoder` (negro opaco). El de verdad es V5.
- `src/Video/VideoPlayer.{h,cpp}` (**V2**): `video::VideoPlayer` con los campos del original y su dirección. `Play` =
  `PlayFullScreenMovie` + `fn_0054AB20` (el reproductor existe aunque no abra); `SetSchedule`/`ScheduleIntro` = 58·fps /
  60·fps; `Process(realMs)` = `Process3dEngine` 0x54DAB5..0x54DD76 + `VideoPoll` + `DecodeNextFrame`; `Skip` =
  fn_0054DA00; `EscapeKey(shift, ctrl)` = ProcessKey 0x63F3B9..0x63F402; `Stop` = `DeleteVideo`; `FinishedVideo`
  privado; `IsPlaying()` atómico (para el audio, desde cualquier hilo); `CoversScreen()` = 0x54DD5E; `GetFrame()` (RGBA8
  como se muestrea, el búfer de 16 bits, color de vértice, `serial`). Funciones puras `FadeAlpha`, `VertexColour`,
  `FullScreenRect` (barras sin recortar), `FramesDue`. `video::Get()` / `video::IsPlaying()` con `GameHooks()`:
  `game_clock::Pause/IsPaused`, `help::Get()->SetWideScreen(on, 0)` (que mueve las barras de `ScreenFade` y avisa al
  audio con dueño 0) y `game_music::ScriptStopMusic`.
- `src/Game.cpp`: `video::Get().Process(game_clock::FrameRealMs())` tras `UpdateRealClock()` (después de los turnos,
  como `Process3dEngine` tras el bucle de turnos); en `ProcessEvents`, ESC con vídeo → `EscapeKey` (sin vídeo sale de
  openblack como siempre: el ESC de openblack no es el del original).
- `src/Graphics/Renderer.cpp` (**V3**, sesión *sistemas*): `Renderer::DrawVideoOverlay` en `RenderPass::ScreenOverlay`,
  tras el mensaje de la mano y antes de `DrawScreenOverlay` (fundido y barras). Una textura RGBA8 `clamp` del tamaño del
  vídeo, rehecha si cambia el tamaño y subida con `updateTexture2D` sólo cuando cambia `serial` (`UploadToTextures`
  0x84514E). Los quads de fn_00845740 tile a tile sobre `video::FullScreenRect` (0x54DBEB..0x54DC6D) con los **mismos
  texels** que cada tile de 256x256 (medio texel hacia dentro, sin repetición: el filtro no llega al tile vecino), el
  color `Frame::colour` (0x54DC11..0x54DC4D) como color de vértice, y el estado `render_modes::State` del material modo 6
  de dos caras con ZFUNC ALWAYS y sin Z. Programa `WorldQuad` (vs_blob + fs_world_quad: color = textura × difuso,
  alfa = `s_alpha.r` × difuso) con una textura R8 1x1 blanca como `s_alpha`: **no hay shader nuevo**.
  `Renderer::DrawScene`: con `video::Get().CoversScreen()` (0x54DD5E..0x54DD7D → 0x54E2A4) no se dibuja nada del mundo
  (ni sombras, ni reflejo, ni cielo, ni el mensaje de la mano); sólo el vídeo y `DrawScreenOverlay` (el fundido del guion
  y las barras, 0x54E2D7..0x54E2ED). Comprobado en el juego: con `fall` el mundo sale × 0,686 = 1 − 0x50/255 en todos
  los píxeles medidos; con `intro` la pantalla es el negro del descodificador nulo (sin el azul del borrado).

Diferencias:

- **(aproximado)** Ritmo: no hay hilo de 16 ms ni `BinkWait`; `Process` descodifica todos los frames que tocan por el
  reloj de pared desde el primer `Process` (frame i a i·den·1000/num ms). Mismos frames a las mismas horas; tras un
  parón largo openblack descodifica de golpe lo atrasado (sólo se ve el último) donde el original iría de uno en uno
  cada 16 ms.
- **(aproximado)** La espera de hasta 0,5 s (0x54DB85..0x54DBD1) no bloquea: se queda la última imagen.
- **(aproximado)** El paso a 16 bits parte del RGBA8 del descodificador (`v >> 3`), no del YUV→555 de la DLL: lo
  comprobará V5 contra los frames de oro (el 555 de la DLL es exactamente `rgb32 >> 3`).
- **(inferido)** Un salto por pulsación: las repeticiones de tecla de SDL se ignoran.
- No portado: el banco de sonido (los dos llamadores pasan NULL), `ClearTipVideo`, la ruta del CD, la cadena de
  estadísticas, el mosaico de 256x256 (una sola textura con los mismos texels por tile), `GAudio+0x1C = −1` (audio no
  tiene cómo; `ProcessMusic` lo repite en el fundido) y fn_005C6C40 (inferido: esconder el HUD).
- **(inferido)** V3: el alfa de las texturas de 16 bits es 1 (`CreateTexture` flags 0x104; un A1R5G5B5 con el bit 15 a 0
  de Bink no se vería); el filtro bilineal del driver; el vídeo debajo del fundido y las barras (`thedraw` es una
  retrollamada de render: no se ha leído cuándo la llama LH3D frente a FinishFrame); los píxeles con el centro de bgfx,
  sin el medio píxel de D3D7 (como los demás rectángulos de `ScreenOverlay`).
- **(aproximado)** V3: mientras el vídeo tapa la pantalla openblack borra a 0x274659 como siempre (el original no borra);
  sólo se ve fuera del rectángulo del vídeo, en pantallas que no son 16:9 y antes de que lleguen las barras.

## Plan V3..V8

| hito | qué | dueño / bloqueo |
|---|---|---|
| V3 | **Hecho** (sesión *sistemas*): `Renderer::DrawVideoOverlay`, el mundo sin dibujar con `CoversScreen()`, `OPENBLACK_TEST_VIDEO`; sin shader nuevo (`WorldQuad`) | — |
| V4 | Opcode 203 `SetAviSequence` (`CHLApi.cpp`): secuencia 1 → `video::Get().Play(data\intro.bik)` + `ScheduleIntro()` antes del `FadeBackToNormal(0)`; el `FreeStart()` del mod `game.skip-intro` se queda (sin vídeo) | `CHLApi.cpp` compartido |
| V5 | El descodificador: FFmpeg recortado (`--enable-decoder=bink`, sólo LGPL) detrás de `IVideoDecoder`, comprobado contra los frames de oro | OK del usuario a la dependencia |
| V6 | `fall.bik`: `KickOff/EndFallingSpellVideo`, alpha 0x50, el mundo debajo, fin con `FallingSpell+0x20 == 4`, `SetFallingSpellVideo` y el gancho `endFallingSpellVideo` | con *milagros* (no hay `FallingSpell`) |
| V7 | `tips.bik` en la pantalla de carga | bloqueado: no hay pantalla de carga |
| V8 | `logo.bik` y `pre_intro.bik` al arrancar, con `trailer.sad` | bloqueado: no hay front end ni perfiles; audio de *audio* |

## Pendiente

- V4..V8 (tabla de arriba); `parity.md` («Vídeo Bink») está «en curso» desde V3.
- Las barras de `SetWideScreen(1, 0)` no se ven durante el vídeo: `ScreenFade::UpdateWideScreen` avanza con el tiempo de
  juego (fn_005C6BB0) y el juego está en pausa; comprobar en el original si se deslizan con el juego en pausa.
- `OPENBLACK_VIDEO_TRACE` no existe todavía.
- *audio*: conectar `GameQueries::videoPlaying` a `video::IsPlaying()` (`MakeMusicQueries` en `Game.cpp` y las queries
  de `AudioSystem`); un modo de olvidar `GameMusic::_alignmentType` para 0x54D963.
- ProcessKey 0x63EF7A..0x63F2A4: los otros caminos de ESC, sin leer del todo.
- El orden entre el dibujo del vídeo (`thedraw` 0x844E30) y `HelpSystem::Draw3D`: quién queda encima.
- Comprobar en el juego que el primer vídeo de un perfil nuevo no se salta y el fundido de fall.bik tras
  `EndFallingSpellVideo`.

## Ganchos de prueba

- `test_bik_file` (6): ficheros sintéticos (formato, audio, 13 rechazos) y los cinco `.bik` reales con
  `OPENBLACK_TEST_GAME_PATH` (u `OPENBLACK_TEST_BW_ROOT`); sin carpeta se saltan 2.
- `test_video_player` (19): ganchos y reloj falsos, `.bik` sintéticos: pausa y pantalla ancha guardadas y devueltas,
  fichero que no abre, ritmo, fundido y fin, calendario de la intro (1392/1440), salto (48 frames, tope, segundo
  salto), ESC con Shift/Ctrl/byte 0xD01984, hechizo, película sustituida, 555 y 565, decodificador nulo, frames
  fallidos.
- `test_rgb16` (5): las ramas 555 y 565 de fn_00837400 emuladas, expansión, cortes, spans.
- Juego: `OPENBLACK_TEST_VIDEO=<intro|fall|ruta>` (Game.cpp, al cargar el mapa): `intro` = `Data\intro.bik` +
  `ScheduleIntro()` (60 s), `fall` = `Data\Spells\fall\fall.bik` con `SetFallingSpellVideo(true)` (alfa 0x50 sobre el
  mundo; el objeto `FallingSpell` es V6), si no la ruta dada. Fotos de V3: `dev\_audit\sistemas\video\intro_mid.png`
  (frame 1800, el vídeo tapa la pantalla) y `fall_mid.png` (frame 1500, el mundo × 0,686).

## Fuentes

- `dev\tmp_dis\video\original.md` (lectura del original) y `dev\tmp_dis\video\plan.md` (decisión y plan V0..V8).
- `dev\tmp_dis\audio\video_audio.md` (sesión *audio*: el audio de cada vídeo).
- `dev\_scratch\asistente\video\golden\README.md` (frames de oro de la DLL; 555 = flag 9 en 0x845146) y
  `oracle\` (el exe de 32 bits que los saca).
- `dev\_scratch\asistente\video\patch12\README.md` y `audit.md` (V1-V2 y su auditoría).
