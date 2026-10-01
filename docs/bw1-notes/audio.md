# Audio: el motor, los bancos, la música, las voces y el guion

Esta página explica cómo suena Black & White 1. Cubre el motor del original (GAudio en `runblack.exe` sobre LHaudiodllR
y QMixer), los bancos y sus formatos (.sad, .sas y la música MP2 en segmentos), la música (LHMusic y la parte de música
de GAudio), las voces y los textos, y las funciones CHL de audio. Para cada tema se dice qué hace openblack: la fase A
ya está hecha y las fases B y C quedan pendientes.
El «qué suena y cuándo» de cada objeto, animación o golpe está en las páginas de cada tema
([enlaces](#qué-suena-y-cuándo)). El plan completo está en `C:\Users\diewgarc\dev\tmp_dis\audio\PLAN.md`.

- [Motor de audio](#motor-de-audio)
  - [Original: GAudio, LHaudio y QMixer](#original-gaudio-lhaudio-y-qmixer)
  - [Capas del motor de openblack](#capas-del-motor-de-openblack)
  - [Estado del motor de efectos en openblack](#estado-del-motor-de-efectos-en-openblack)
- [Bancos y formatos](#bancos-y-formatos)
  - [Tabla de bancos de efectos y diálogo](#tabla-de-bancos-de-efectos-y-diálogo)
  - [Formato .sad](#formato-sad)
  - [Carga de las ondas y caché](#carga-de-las-ondas-y-caché)
  - [Canales, prioridades y bucles](#canales-prioridades-y-bucles)
  - [Volúmenes maestros, foco y reinicio](#volúmenes-maestros-foco-y-reinicio)
  - [.sas y tablas de animación](#sas-y-tablas-de-animación)
- [Música](#música)
  - [Bancos de música: segmentos MP2](#bancos-de-música-segmentos-mp2)
  - [LHMusic: el motor de 6 pistas](#lhmusic-el-motor-de-6-pistas)
  - [Tabla MUSIC_TYPE](#tabla-music_type)
  - [GameMusic: ProcessMusic y sus fuentes](#gamemusic-processmusic-y-sus-fuentes)
  - [Intro, tráiler, outro, vídeos y menú](#intro-tráiler-outro-vídeos-y-menú)
  - [La música en openblack](#la-música-en-openblack)
- [Voces y textos](#voces-y-textos)
  - [Tabla de voz de los textos](#tabla-de-voz-de-los-textos)
  - [Narración y tiempo de lectura](#narración-y-tiempo-de-lectura)
  - [Consejeros, Guidance, confirmaciones y voces nocturnas](#consejeros-guidance-confirmaciones-y-voces-nocturnas)
  - [Voces y textos en openblack](#voces-y-textos-en-openblack)
- [Guion: CHL de audio](#guion-chl-de-audio)
  - [Interruptores de GScript](#interruptores-de-gscript)
- [Fase A implementada](#fase-a-implementada)
- [Fases B y C](#fases-b-y-c)
- [Qué suena y cuándo](#qué-suena-y-cuándo)
- [Pendiente](#pendiente)
- [Ganchos de prueba](#ganchos-de-prueba)
- [Fuentes](#fuentes)

## Motor de audio

### Original: GAudio, LHaudio y QMixer

**Fiel** (`engine.md` §1.1-1.2, 1.12; PLAN §0, §8.1).

- **Creación.** `pc_main` 0x641D79 llama a fn_00590FD0, que crea `GGlobal::Global.audio = new GAudio`
  (0x3D4 bytes, ctor 0x426D40). Antes van `PlayLogoScreens` y `Report3D("InitialiseAudio")`, y después
  `PlayPreIntroVideo`. El ctor hace lo siguiente:
  - Crea `LH_AudioSystem` (0x1EC bytes) con los valores de GAudio: 16 canales de muestras (sys+0xCC), 22050 Hz
    (0x5622, sys+0xD8), una caché de ondas de RAM/8 (`dwTotalPhys >> 3`, 0x426DDC) y sin hardware.
  - Con `LHWaveIsInstalled() == 1` registra `fn_00427200` como función de posición 3D, con 800.0f como distancia
    máxima de los anim-effects (0x426E6B; es una distancia, no un número de sonidos).
  - Lee los volúmenes maestros (fn_00428250) y registra los 85 bancos de música de 0x9C9748 y los 11 de efectos de
    0x9CB3F8 (fn_00429CB0 → fn_0042A350 → fn_0042A390).
- **LH_AudioSystem.**
  - Ctor 0x10015290 y `Create` 0x100153F0. `Create` solo se puede llamar una vez («NO, NO, NO!! - one-time audio
    object only»).
  - Lee la clave del registro `...\Audio\Override` (0x100103E0). Sus valores son NoAudio, Wave, Midi, Music, Redbook,
    Atmos, Sfx, 3DSpatialisation, UseHardware, SpeakerConfig, HeapSize, MaxSamp, HWRate y Latency. En la máquina del
    usuario la clave no existe.
- **QMixer** (0x10015820).
  - Abre 22 canales por software: 16 de muestras y 6 de música. Trabaja a 22050 Hz.
  - `QSWaveMixSetPanRate(16, 1, 100)`.
  - La bomba va por `timeSetEvent(20, …, 0x10015800)`, cada **20 ms**.
  - La velocidad del oyente es 0 (0x10015C1A): **no hay Doppler, ni EAX, ni reverberación**.
- **Todos los disparos pasan por un solo camino**: `GAudio::PlaySoundEffect` 0x429E30 (con sus 5 variantes) o
  `SamplePlayAnimEffect` 0x42A4B0. Las únicas excepciones son:
  - `HelpDude::PlaySample` 0x5BB530, que llama a `LHSamplePlay` directo, sin los filtros de GAudio;
  - `LHAtmosProcess`;
  - 8 llamadas a `LHMusicPlay`.
- **Parar y consultar** sí se hace también fuera de GAudio:
  - `LHSampleStop` en FallingSpell (0x526FD6, 0x527181, 0x52719D) y en `HelpDude::StopSentence` 0x5BB875;
  - `LHMusicStop` en FallingSpell 0x5271B0 y en `PlayPreIntroVideo` 0x642907;
  - `LHSampleIsPlaying` y `LHSampleSetVolume` en PSysSound fn_006D1110 (0x6D120A, 0x6D1239);
  - las consultas de HelpDude (0x5BB797, 0x5BB80D, 0x5BCD66).
- **Filtros de `PlaySoundEffect`** (0x429E37..0x429FD9, 0x42A56E):
  - No suena nada si `g_game` o HelpSystem son nulos.
  - Corte 3D si dist²(cámara, pos + desplazamiento) > max². El máximo es el del .sad (+0x26C) o, si vale 0, el de
    opts+0x58 = 9999. Dentro de la ciudadela, la distancia se mide desde `LH3DTech::g_camera` 0xEA1DB8.
  - userParam 1 (pantalla ancha del guion) no suena con las bandas negras puestas. Dentro de la ciudadela solo suena
    userParam 2.
  - Con `SET_GAME_SOUND` solo suenan los bancos 6 y 7 (GAudio+0x3C0/+0x3C4).
  - userParam 4 depende de los estados de interfaz 0x10, 0x16 y 0x17.
  - Con is3D y track, no suena nada si el dueño es un GameThing con `IsAvailable() == 0`. El símbolo
    `GAudio::IsInsideCitadel` 0x429D20 está mal puesto: en realidad es «dueño no disponible».
- **Dentro de la ciudadela** = `g_game+0x205A28 == 1` (0x4282F0; el símbolo `GetWideScreenControl` está mal puesto).
  Lo pone a 1 `GoInsideCitadel` 0x554004 y a 0 `LeaveInsideCitadel` 0x553B1F.
- **Orden por turno**, `ProcessAudioGameTurn` 0x427080. Lo llama `GGame::EndTurn` después del turno 5 (0x54E997):
  1. Solo si `LHWaveIsActive` (0x427086): la música (0x427DF0), los objetivos de ambiente (0x429100), los bancos de
     ambiente (0x428FE0), `UpdateChannels` + el oyente (0x4270D0) y `LHAtmosProcess(1)` si `g_game+0x250188 == 0`
     (vídeo, **(inferido)**).
  2. **Siempre**: fn_00429700, que purga la lista ThingMusicInfo.

### Capas del motor de openblack

**Plan** (PLAN §2.1-2.4, con las correcciones de diseño de §8.6). Hay una capa por cada pieza del original:

```
          juego (ECS, mano, cámara, Magic, Worship, CHLApi, Help, menús, Debug)
                               │  solo #include "Audio/Audio.h"  (fase B)
 4. API pública  audio::        fachada fina con las firmas de GAudio
 3. Servicios    GameSfx · AnimEffects · SoundTags · Atmos (SoundMap + AtmosBanks)
                 GameMusic (+ ThingMusic) · Voices · Advisor · Guidance · Confirmation · Spooky
                 ScriptSound · ScriptAudioState
 2. GAudio       AudioSystem: bancos por tipo (0x9CB3F8, 0x9C9748), filtros, volúmenes maestros,
                 Init/Reset/ProcessTurn/foco, dueños fijos 0x270C..0x270F
 1. LHaudio/QMixer emulado
                 Banks (LHBankRegister, caché de ondas) · SamplePlay (16 canales, de agua)
                 MusicEngine (LHMusic: 6 pistas, hilo de 120 ms) · QMixerLaws (volumen, distancia, polar)
 0. Dispositivo  AudioPlayer (OpenAL) + decodificadores (dr_wav PCM/ADPCM, dr_mp3 capa II)
```

Reglas:
1. **Un solo punto de entrada.** Fuera de `src/Audio` nadie llama a OpenAL ni a `CreateEmitter`/`PlaySound` (fase B).
2. **Canales, no entidades.** Cada canal tiene su fuente AL y no hay `AudioEmitter` en el registro ECS.
3. **Una muestra es (banco, número).** La clave `"<archivo>.sad/<n>"` no distingue mayúsculas.
4. **Todo va por turno**, salvo `HelpDude::UpdateSaySentence` (por fotograma, 0x5BDE48, con un retardo en ms reales
   0x5BB554), el woosh de la cámara (`GetTickCount & 3`) y el hilo de música (120 ms reales).
5. **El audio no incluye componentes del ECS.** Lo que necesita del juego lo pregunta por `audio::GameQueries`
   (`src/Audio/GameQueries.h`), unas `std::function` que registra `Game.cpp`. Una consulta sin dueño devuelve el valor
   de un juego sin ese sistema: sin vídeo, tierra 0, sin cámara, sin pantalla ancha, alineamiento 0, sin pueblos y las
   ramas de música de ciudadela, pelea, cántico y baile en false.
6. **La lógica es pura y se prueba sin AL**, con sinks falsos.
7. **No se abre un segundo dispositivo OpenAL.** La música usa el contexto que ya abre `AudioPlayer`.
8. **Los contadores cíclicos van en un `enum class Counter`** con la dirección en el comentario, no con direcciones
   como claves. La API no tiene argumentos por defecto inventados.

### Estado del motor de efectos en openblack

En la rama común hay **tres motores que se pisan** (fase B: unificarlos):
1. `AudioManager` ECS: un emisor por sonido, sin canales y con una **fuga** de búfer AL en cada disparo
   (AudioManager.cpp:157-160, 249).
2. `sample_play` de agua: fiel a LHSamplePlay, con 16 canales, modos 1/2/3, prioridad, robo de canal y las leyes de
   volumen y distancia de `test_audio_laws`. Está sin fusionar.
3. Reproductores caseros por módulo: `AnimationSounds`, `LanternSounds`, `SpellSounds`, `FireSound`, `PlayAt`…

Fallos conocidos de la rama común (hito B0, PLAN §1.1):
- El tag 0x50 (MPEG dentro de RIFF) no se decodifica, así que HelpSprites y villagers están mudos.
- La muestra vacía hace `return` en lugar de `continue` (Game.cpp), y se pierden InGame 166..210 y spells 32..88.
- `AL_PITCH = 1` en cada fotograma.
- El oyente se actualiza cada fotograma, con los ejes sin intercambiar.
- Se ignoran los bucles finitos y el tramo de bucle.
- En agua, el filtro compara «Villagers.sad» ≠ «villagers.sad», distinguiendo mayúsculas.

## Bancos y formatos

### Tabla de bancos de efectos y diálogo

**Fiel.** Es `AUDIO_SFX_BANK_TYPE`, en la tabla 0x9CB3F8 (11 `const char*`), y el banco queda en GAudio+0x3A8 + 4·tipo.
Se registran todos al arrancar con `LHBankRegister(ruta, 0)`. Las rutas no llevan carpeta de idioma: el idioma lo decide
lo que esté instalado.

| tipo | banco | ruta | prioridad típica (datos) |
|---|---|---|---|
| 0 | — | nulo | |
| 1 | InGame | `audio/sfx/game/ingame.sad` | 0..2000 |
| 2 | Editor | `audio/sfx/game/editor.sad` | 0..300 |
| 3 | Spells | `audio/sfx/game/spells.sad` | 0..6000 |
| 4 | Creature | `audio/sfx/creature/creature.sad` | 200/400 |
| 5 | ScriptSfx | `audio/sfx/script/scriptsfx.sad` | 200..9999 |
| 6 | HelpSprites | `audio/dialogue/HelpSprites.sad` | 9999 |
| 7 | Villagers | `audio/dialogue/Villagers.sad` (en disco `villagers.sad`) | 9999 |
| 8 | VillagersBanter | `audio/dialogue/VillagersBanter.sad` | 2 |
| 9 | SpellDialogue | `audio/dialogue/SpellDialogue.sad` | 2000 |
| 10 | Guidance | `audio/dialogue/Guidance.sad` | 20 |

El juego registra además otros bancos:
- Uno **por especie de criatura**: `audio\sfx\creature\%s.sad`, en `LH3DCreature::LoadBinary` 0x4EBD81 →
  `GAudio::RegisterBank` fn_00428620 (+0x5288).
- Los 13 de ambiente (`Audio\SFX\Atmos`).
- El de un vídeo (`PlayFullScreenMovie` 0x54D920) y `audio\music\intro\trailer.sad` (0x6427CE).
- El de HelpDude (fn_005BB1B0, 0x5BB1E6, **(inferido)**).

openblack: `audio::SfxBank` y `k_SfxBankPaths` en `src/Audio/BankTables.h` (hito A1). La carga de `Game.cpp` todavía
trata cada .sad como un grupo por nombre de archivo (fase B0).

### Formato .sad

**Fiel** (`engine.md` §1.5, `music.md` §3.1). Un .sad es `"LiOnHeAd"` seguido de bloques
`{char nombre[32]; u32 tamaño; datos}`. `PackFile` de openblack ya los lee.

| bloque | contenido |
|---|---|
| `LHFileSegmentBankInfo` (532 B) | u32 a, u32 b, u32 **música** y un título de 520 B. La música es 1 en los 58 bancos de `Music\`, en `Intro\trailer.sad` y en `Dialogue\MissionariesVerse1..3.sad`; 0 en el resto. a/b son 0 salvo en `ocean.sad` (7, 6), **(inferido)**: versión del editor |
| `LHAudioWaveData` | las ondas concatenadas |
| `LHAudioBankSampleTable` | u32 `n | (nAtmos << 16)` + n registros de 0x280 B (nAtmos = n en los 13 de `Atmos\`) |
| `LHAudioAnimArrayTable` | u32 filas, u32 ancho (6), filas × 6 s32: tablas de anim-effects |
| `LHAudioWaveNumTable` | las listas {nº, muestras…} a las que apunta la columna 6 |
| `LHAudioListViewText` | texto del editor de bancos, no se usa |

Registro de muestra (0x280 B; los campos de reproducción completos están en `tmp_dis\sound\notes.txt`):

| off | campo |
|---|---|
| +0x000 | ruta del .wav original (char[256]). En los diálogos es `K:\4frosty\Spanish\…\HELP_TEXT_….wav`: es la base de la tabla de voz |
| +0x104 | id, 1-based: el número de muestra que usa el juego |
| +0x108 | muestra cuya onda se usa (clones: editor 1..6 → 1); la caché compara esto |
| +0x10C / +0x110 | tamaño y desplazamiento en WaveData. Tamaño 0 = muestra vacía (InGame 165, spells 31) |
| +0x118 | u16 grupo de clones (modo 3) o, en música, el grupo de sincronía (solo en el segmento 0) |
| +0x11A | u16 grupo de ambiente |
| +0x124 | WAVEFORMATEX (+0x128 = Hz) |
| +0x138 / +0x13C | inicio y fin del bucle en tramas (−1 = no hay), sacados de los `cue` del .wav |
| +0x140 | descripción (en música, los marcadores `!n=etiqueta`) |
| +0x240 | prioridad |
| +0x244 | banderas de override (0x20 volumen, 0x40 vueltas, 0x80 min, 0x100 max, 0x200 escala, 0x400 modo…) |
| +0x248..+0x27C | vueltas, pan, volumen/userParam (+0x25C; userParam = >> 16), tono y desviación, min/max/escala (+0x268/+0x26C/+0x270), modo (+0x274), +0x278 (si es 0, `LHSamplePlay` da error en 0x100118FD), frecuencia de ambiente (+0x27C) |

Formatos de onda en los datos:

| formato | dónde |
|---|---|
| RIFF PCM de 16 bits, 22050 Hz (11025 en 1 de InGame; 44100 en 210 de VillagersBanter y 1 de Scriptsfx) | casi todos los efectos |
| RIFF MS-ADPCM (0x0002) | high, rain, wind, stream, 3 de InGame, 27 de spells, 2 de Scriptsfx, 23 de VillagersBanter |
| RIFF **MPEG-2 capa II (0x0050)**, 22050 Hz, 64 kbps mono | todo HelpSprites (1923) y villagers (1328); Guidance 129/227, SpellDialogue 9/35, Scriptsfx 21, country 17, InGame 2 |
| tramas MPEG capa II crudas | los 58 bancos de `Music\` y MissionariesVerse1..3 ([abajo](#bancos-de-música-segmentos-mp2)) |

openblack: `AudioBankInfo` (los 3 u32) y `PackFile::IsAudioMusicBank()` en `components/pack` (A1, **fiel**: 0x10002240,
`LHIsMusicBank` 0x10002EE0). `Game.cpp` todavía reconoce la música por la extensión «.mpg» del primer nombre.

### Carga de las ondas y caché

**Fiel** (`engine.md` §1.3-1.4).
- `LHBankRegister(path, inMemory)` 0x10002240. El juego **siempre** pasa `inMemory = 0` (0x426EEE, 0x428F30, 0x42A3B1,
  0x4EBDB9, 0x54D991, 0x5BB1DE, 0x6427C1). Al registrar solo se leen las cabeceras y el fichero queda abierto.
  `LHMusicGetTotalGroups` (sys+0x40) es el máximo del u16 +0x118 de los bancos de música.
- Las muestras **no se transmiten**: se cargan enteras al primer uso (0x10011420 → fn_100032D0) en una **caché FIFO**
  (lista 0x10042F80) con un presupuesto de RAM/8 ([0x100383B8]). La víctima es la onda más antigua que ningún canal
  esté usando.
- QMixer convierte ADPCM y MPEG con ACM. Con opts+0x164 (`keepPcm`), el DLL deja el PCM en SampleInfo+0x80/+0x84
  (fn_10010910, 0x10011CB3/0x10011E25), y HelpDude lo copia para el lip-sync (0x5BB57B..0x5BB5C9).
- openblack: **pendiente** (B0/B1). Hoy se decodifica y se crea un búfer AL en cada disparo. Pregunta abierta al
  usuario: carga perezosa como el original o todo al arrancar (PLAN §6.3).

### Canales, prioridades y bucles

**Fiel** (`engine.md` §1.6-1.7; NOTES de agua):
- **16 canales** compartidos por 2D, 3D, ambiente y diálogo. Modos 1/2/3 y un grupo de clones.
  - Una muestra nueva roba el canal de menor prioridad si esa prioridad es menor que la suya.
  - Por las prioridades de los datos, el diálogo de ayuda no se pierde nunca y el ambiente cede primero.
- **3D**: `QSWaveMixEnableChannel(…, 0x20, f | 0x100)`, `SetDistanceMapping {min, max, escala}`.
  - Ley de distancia: d ≤ min → 1; d > max → 0; si no, `min / ((d − min)·escala + min)` (0x1802CE50).
  - Volumen: `floor(m·v/127)·258/32767` (0x100133C1).
  - `LHSampleUpdate3DChannels` 0x10014310 solo mueve los canales 3D con track. Para el canal si su dueño ya no está
    (0x1001439D) o si pasa de su distancia máxima (0x100143BC).
- **Bucles**: `QSWaveMixPlayEx(…, 0x421, onda, vueltas, &p)` en 0x10012949, con `lStart..lEnd` si los dos son ≠ −1
  (+0x138/+0x13C).
  - Vueltas: 0 = una vez, −1 = siempre, N = N repeticiones del tramo y luego sigue hasta el final. Si son N o N+1 pasadas
    queda **(inferido)**: es la pregunta 2 de PLAN §6.
  - Ejemplos: G_VillageBell InGame 30 (5 vueltas, 0..27400 de 57855), G_PickUpFood 44 (−1, 47743..110078), las palomas
    del editor 342/343 (5), jungle (3..5), swamp (2..6), country bird15 (6).
- **`ReleaseLoop`** (`LHSampleReleaseLoop` 0x10012F20): actúa sobre el **primer** canal que coincide y está en uso, con
  `StopChannel(0x1000)`. Pone a 0 las vueltas restantes y la pasada actual acaba. Si ese primer canal está inactivo,
  devuelve 0 sin seguir buscando.
- **Sin API de fundido**: cada llamador hace el suyo.

### Volúmenes maestros, foco y reinicio

**Fiel** (`engine.md` §1.8-1.10; PLAN §1.1).
- **Volúmenes maestros**:
  - Hay dos, de 0 a 127: `AudioSampleMasterVolume` y `AudioMusicMasterVolume`, en
    `HKCU\Software\Lionhead Studios Ltd\Black & White\BWSetup`.
  - Se leen en fn_00428250 (en el arranque y al cancelar las opciones, 0x5158B8) y se guardan en fn_004282B0
    (`ToBeDeleted` y aceptar las opciones, 0x5158EE).
  - El deslizador de las opciones (`DialogBoxOptions` 0x5145A3/0x5145AE) hace `LHMusicSetMasterVolume(ftol(slider·127))`.
  - Sin la clave del registro, valen 127 (`[0x10056280] = 0x7F`, 0x1000DE08).
- **Alt-Tab** (0x7DE6D0 → 0x642470 → 0x428720):
  - Al perder el foco, `LHGlobalSwitch(0)`: `LHWaveSwitch(0)` (StopAll) y `LHMusicSwitch(0)` (`LHMusicStop(0)`).
  - Al volver, `LHGlobalSwitch(1)` solo reactiva las banderas: **no se reanuda nada**.
- **`GAudio::Reset`** 0x426CA0 (desde `GGame::Init`). En orden:
  1. Pone a cero +0x18C, pos[grupo] (fn_00428190) y +0x28/+0x24/+0x180/+0x190, y +0x1C = −1.
  2. `LHMusicStop(0)`, `LHAtmosProcess(0)` y `StopAll`.
  3. `LHGlobalSwitch(0)`, espera a que no suene nada, `ClearInfoList` y `LHGlobalSwitch(1)`.
  4. `ReleaseAllThingMusicInfo` 0x4291B0.
- **Pausa**: `PauseGame` no toca el audio. `EndTurn` en pausa llama a `AtmosProcess(0)` (0x54AE20, 0x4286C0).
  Quién llama a `LHMusicPause` 0x1000EA10 está sin leer.
- openblack:
  - Volumen de música: `EngineConfig::audioMusicMasterVolume` (127) en el panel de depuración «Music» (A8). Todavía no
    se guarda en disco.
  - La parte de música de `GAudio::Reset` y `GScript::Reset` va en `Game::LoadMap`.
  - El volumen de efectos, Alt-Tab y la pausa: **pendientes** (B1).

### .sas y tablas de animación

`Data\SmallSounds.SAS` es el texto de los eventos de sonido por clip de animación (`LoadAllAnimations` 0x550180). Junto
con las tablas `LHAudioAnimArrayTable` de cada .sad, alimenta `SamplePlayAnimEffect` 0x42A4B0 → 0x10014A20. La clave
tiene 5 columnas y hay 3 acciones (0 tocar, 1 parar, 2 soltar). El detalle de los clips y el banter está en
[animation.md](animation.md#sonidos-de-los-clips). El original no tiene otro formato de audio.

## Música

### Bancos de música: segmentos MP2

**Fiel** (`music.md` §2.3, §3.1; `music_sad_table.md`).
- No hay LHStream ni ficheros sueltos. Cada .sad de música es un banco `LiOnHeAd` normal con la marca de música.
- Su tabla de muestras son **segmentos** `c:\windows\temp\sectNNNN.mpg` de MPEG-1/2 Audio Layer II, a 22050 Hz:
  128 kbps estéreo, 96/64 kbps mono, y 160 kbps en el Outro.
- Cada segmento tiene **21 tramas = 24192 muestras (0x5E80) = 1,0971 s**. Son contiguos y están alineados a trama: se
  comprobaron 13457 segmentos sin un fallo.
- Del **segmento 0** salen:
  - el grupo de sincronía (u16 +0x118);
  - las banderas (+0x244) que `LHMusicPlay` aplica: 0x20 volumen (u16 +0x25C), 0x40 vueltas (+0x248, −1 = siempre),
    0x80/0x100/0x200 min/max/escala del 3D (+0x268/+0x26C/+0x270);
  - los Hz (+0x128).
- **Marcadores**: fn_1000D9E0 lee la descripción (+0x140) de cada segmento, con la forma `"!<muestra>=<etiqueta>!…"`, y
  forma nodos {trozo i+1, muestra, etiqueta}. Recorre los segmentos del último al primero e inserta cada nodo en
  cabeza. Solo los usan `MissionariesVerse1..3.sad` (L1..L12 son los versos, P los golpes).
- Bancos (tabla completa en `music.md` §3.2):

  | bancos | grupo | otros datos |
  |---|---|---|
  | los 24 de `align` | **1** | 430 segmentos (471,1 s, 7 814 232 B), versiones paralelas de la misma melodía |
  | cánticos | 6..13 | 0x3C0, bucle −1, 3D 30/120/2 |
  | citadel | 4 | |
  | CreatureFight | 5 | |
  | el resto | 0 | |

  Volúmenes del .sad: Script01 60, Script02..04 65, Epic04 80, Sleg 65, Khazar 65, Gregorian 40, Circus 60,
  CreatureGuide 70, Gregorian3D 50, Circus3D 60; los demás 127.
  Distancias 3D: PiperTune_M 15/100/4, Pipercave_M 25/80/4, SingingStonesA 100/200/4, MissionariesBackground
  30/100/4, Whistle* 30/80/2, Circus3D 30/120/3, Gregorian3D 60/120/4.

openblack: `MusicBank` (`src/Audio/MusicBank.{h,cpp}`, A2, **fiel**) cubre `Register` (0x10002240), los segmentos, el
grupo (0x10002F30), las distancias (0x10002EF0/0x10002F10), las banderas, los Hz, `GetVolume`, `GetLoops`,
`GetDistanceMapping` (0x1000E30A..0x1000E338), `ParseMarkers` (0x1000D9E0) y `ReadSegment`. El fichero queda abierto y
cada segmento se lee al usarlo. Un marcador sin etiqueta da una etiqueta vacía **(aproximado)**: no se da en los datos.

### LHMusic: el motor de 6 pistas

**Fiel** (`music.md` §2.3; volcados `music_dll_play.txt`, `music_dll_thread.txt`, `music_dll_stop_etc.txt`).
- **6 canales** `LH_MusicInfo` (sys+0x84, 6 × 0x6C). El **maestro** (0x10056284) es el último arrancado: sube hasta su
  objetivo, y todos los demás bajan −3 por vuelta hasta 0 y se liberan.
- **Hilo «music»** 0x1000EB40, lanzado con `_lhbeginthread("music", 0xF)` en 0x1000DF27:
  - Una vuelta recorre los 6 canales y luego hace `Sleep(120)` (0x1000F726), o `Sleep(5000)` si falló una lectura
    (0x1000F71F).
  - Encola hasta **4 trozos** por canal (0x1005628C).
  - Lee cada segmento del disco y lo decodifica con su propio decodificador MPEG (0x1000F740), con el estado continuo
    entre trozos; solo se reinicia cuando `LHMusicPlay` lo pide (0x1000E448).
- **Fundidos**:
  - El maestro sube **+4** por vuelta (0x1000F59F) solo si opts+0x20 lo pide; si no, entra de golpe al volumen la
    primera vez.
  - Los demás canales, o el maestro por encima de su objetivo, bajan **−3** (0x1000F613).
  - Duraciones: 0→127 en 32 vueltas (≈ 3,8 s), 0→80 en 20, 127→0 en 43 (≈ 5,2 s) y 80→0 en 27. Las vueltas son fiel; la
    duración real (120 ms más el tiempo de proceso) es **(inferido)**.
- **`LHMusicPlay`** 0x1000DF60:
  - Sin banco de música, hace `LHMusicStop(1)`.
  - Si el banco ya suena, es un **re-disparo**: solo cambia el objetivo, el fundido, la sincronía y el maestro.
  - Si no, toma un canal libre; si no lo hay, no suena. Una 7.ª pista se rechaza.
- **Sincronía de grupo** (0x1000ED03..0x1000ED7E, 0x1000F385): con `sync`, la pista nueva arranca en el **mismo trozo y
  la misma muestra** (`QSWaveMixGetPlayPosition`) que otra pista del mismo grupo que esté sonando. Así el cambio
  bueno↔malo o de tribu no reinicia la melodía.
- **Callback de fin de trozo** 0x1000DC80: avanza el trozo audible (+0x48) y, en el último, pasa al estado 3.
  Estados: 0 libre, 1 sonando, 2 en pausa, 3 terminado, 4 último trozo encolado.
- **Volumen** QMixer = `floor(floor(cur·sad·258/127)·master/127)` (0x1000F423..0x1000F464). Es decir, la ganancia es
  cur/127 · vol.sad/127 · master/127. En 3D se aplica la ley de distancia de QMixer.
- **Funciones**:

  | función | dirección | qué hace |
  |---|---|---|
  | `LHMusicStop(fade)` | 0x1000E530 | con 1 todos bajan; con 0 se cortan |
  | `LHMusicStop(info, fade)` | 0x1000E620 | para el maestro, para todos |
  | `GetInfo` | 0x1000E750 | |
  | `GetStatus` | 0x1000FC00 | |
  | `GetCurrentChunk` | 0x1000FB40 | |
  | `GetMasterInfo` | 0x1000FB70 | |
  | `GetTotalGroups` | 0x1000FB60 | |
  | `SetMasterVolume` | 0x1000E890 | sin signo: por encima de 127 da 127 |
  | `SetPitch` | 0x1000E970 | entre 50 y 250 |
  | `Set3DPosition` | 0x1000FBA0 | |
  | `Pause` / `Restart` | 0x1000EA10 / 0x1000EA80 | |
  | `Switch` | 0x1000EB00 | |
  | `Close` | 0x1000E7A0 | |

openblack:
- `MusicEngine` (`src/Audio/MusicEngine.{h,cpp}`, A3, **fiel**) es la lógica del DLL sobre una interfaz `IMusicSink` (lo
  que hace QMixer). Así se prueba sin OpenAL (`test_music_engine`).
- `MusicStream` + `MusicSystem` (`src/Audio/MusicStream.{h,cpp}`, A4):
  - Usan el **contexto OpenAL de `AudioPlayer`**, sin abrir otro dispositivo. Tienen una fuente por canal, con su cola de
    búferes y un decodificador dr_mp3 continuo por pista (`MusicSegmentDecoder`).
  - El hilo hace una vuelta y luego espera 120/5000 ms. Bombea cada 20 ms.
  - Hay un cerrojo recursivo, que hace de la sección crítica 0x100562B0.
  - Ganancia = volumen QMixer / 32767, por la ley 3D de QMixer.
  - `AudioManager::PlayMusic` sigue en su sitio: se retira en B1.
- **(aproximado)**:
  - Un segmento que no decodifica da una trama de silencio, para que el callback llegue.
  - Si la cola de OpenAL se vacía, la fuente se para y se reanuda en cuanto hay datos; QMixer toca la siguiente onda en
    cuanto está.
  - Antes de la primera trama no hay WAVEFORMAT.
  - Bajar de volumen dentro de la vuelta: el original no vuelve a mirar el estado dentro del bucle (MusicEngine.cpp:536).
- **(inferido)**:
  - La unidad de `lStart` y de `QSWaveMixGetPlayPosition` son muestras.
  - `FlushChannel` avisa de cada trozo vaciado antes de volver: se deduce de la espera de `LHMusicClose`, 0x1000E854.
  - El reloj de los marcadores va en double (0x1001F874).

### Tabla MUSIC_TYPE

**Fiel**. Es la tabla 0x9C9748: 85 entradas `{ruta, "MUSIC_TYPE_…"}` de 8 bytes, hasta 0x9C99F0, en GAudio+0x2C + 4·tipo.
El ctor las registra todas: 0x426E82..0x426F1F. Si existe `-NOLOADMUSIC` (byte 0xD46AC3), no se registra ninguna. Si
`GetFileAttributes` falla, usa la ruta `"%c:\%s"` en el CD (`g_GameDriveCharacter` 0xC2B9E8).

| tipos | contenido |
|---|---|
| 1..3 | `align/evil|neutral|good.sad` (genéricas) |
| 4..27 | tribu × alineamiento. **22..24 (nórdico) apuntan a las mismas cadenas que 4..6 (celta)**: 0x9CAE4C, 0x9CAE0C, 0x9CADCC |
| 28..43 | cánticos normal/_vox por tribu |
| 44..46 | `citadel.sad`, la misma cadena 0x9CA498 |
| 47..53 | flautista, ermitaño, misioneros. 51..53 son `audio/dialogue/MissionariesVerse1..3.sad`, que son bancos de música |
| 54 | intro |
| 55..84 | guiones. **56 WELCOME_DANCE apunta a `FollowUsWelcome.sad`, que no está instalado**: el banco queda nulo y ningún guion lo usa. 74 CREATURE_FIGHT, 75 CREATURE_BIG_FIGHT, 77 OUTRO |

`Audio\Music\script\MissionariesSad.sad` está en el disco pero no en la tabla. Los valores de bw1-decomp a partir del 84
no existen en W120. openblack: `audio::MusicType` y `k_MusicBanks` en `BankTables.h` (A1). Lista con nombres:
`tmp_dis\audio\music_types.md`.

### GameMusic: ProcessMusic y sus fuentes

**Fiel** (`music.md` §2.2, §2.4-2.6).

**Campos de música de GAudio**:

| campo | contenido |
|---|---|
| +0x18 | pos[grupo] (malloc de `4·TotalGroups`) |
| +0x1C | tipo de alineamiento que suena (−1 = ninguno) |
| +0x20 | turnos de silencio |
| +0x24 | último tipo terminado |
| +0x28 | tipo del guion |
| +0x2C | bancos[85] |
| +0x180 | música del guion arrancada |
| +0x184 / +0x188 | lista ThingMusicInfo |
| +0x18C | pueblo actual |
| +0x190 | alineamiento en la cámara |

**`ProcessMusic`** 0x427DF0 se llama cada turno. Prueba las fuentes por orden de prioridad y la primera que coge la música
gana:
1. Vídeo (`g_game+0x250188`) → «nada» sin parar.
2. Sin LHMusic, o `LandNumber == 6` (0x427E1D) → return.
3. `ProcessCitadelMusic` 0x427B60.
4. Música del guion fn_00427CA0 (entonces +0x1C = −1).
5. Pelea de criaturas fn_00427660.
6. Cántico `ProcessChantMusic` 0x427790.
7. Baile de la criatura 0x427EC0.
8. Música de objetos fn_00429790.
9. Alineamiento y tribu 0x4279C0 (el símbolo «LoginBox::ControlCallback» está mal puesto); entonces +0x180 = 0.
10. Si no hay ninguna: `SavePositions` y `LHMusicStop(1)`, fundido a silencio.

«Nada» (0x427E95..0x427EB3) pone «Music Playing=NONE», +0x180 = 0 y +0x1C = −1, sin parar nada.
`SavePositions` (fn_004281C0) va antes de cada `LHMusicPlay`: guarda pos[grupo−1] = trozo audible + 2 de cada canal en
estado 1 con grupo > 0.

Las fuentes:
- **Guion** fn_00427CA0. Usa vol 127, inicio 1, sin sync ni fundido, 2D, y el volumen y las vueltas del .sad.
  - Callback de fin 0x426B80: si GAudio+0x28 == dato, GAudio+0x28 = 0.
  - Callback de marcadores 0x426BA0 (tabla 0x426C10): `L<n>` → GScript+0x98 = n, +0x9C = 1; `P`/`W` → +0x9C++.
  - `StartScriptMusic` 0x428230 hace +0x28 = tipo; con un tipo, +0x180 = 0, y se relanza aunque sea el mismo.
  - Con +0x28 == 0 y +0x180 activo: `LHMusicStop(1)`.
  - `CitadelHeart::Built` 0x4650D1 lanza EPIC_01 (61) si `LandNumber != 1`.
- **Alineamiento y tribu** 0x4279C0. Exige:
  - que haya cámara;
  - que no esté la pantalla ancha del guion (HelpSystem +0x45E8 && +0x45EC) ni las bandas en movimiento
    (fn_005C6C50);
  - GScript+0x94 ≠ 0;
  - turno > 20.

  El tipo sale de fn_00427460:
  - a = fn_00426C80(`Discrete`(GAudio+0x190)). `Discrete` es 0x414730: `ftol(min((a+1)/2·7, 6))`. La tabla 0x9C99F0
    `{0,0,1,1,1,2,2}` da 0 malo, 1 neutral, 2 bueno, con umbrales ±3/7.
  - El pueblo más cercano a menos de 400 (fn_00602160), con la cámara a < 400 de altura sobre el suelo:
    - a ≤ 300 → tribu del pueblo;
    - con histéresis, el pueblo anterior mientras esté a < 400.
  - Si no hay pueblo, genérica a + 1.
  - Tribu(a, t) = t ≥ 9 ? 5 : {4,4,7,10,13,16,19,22,25}[t] + a (0x427410, tabla 0x9C9A0C).
  - 300 y 400 son `townTriggerDistance` / `townTriggerOffDistance` de info.dat (0xD9A934 / 0xD9A938).

  Se toca con vol **80**, inicio pos[g−1], sync 1, fundido 1, 2D, vueltas = (inicio ≥ n/2) (0x427AF9..0x427B01). El
  callback de fin es 0x426B40 → 0x4279A0. Al acabar una pista, **3500 turnos de silencio** mientras no cambie el tipo.
- **Objetos** (ATTACH_MUSIC) fn_00429790 / fn_00429500.
  - Cada `ThingMusicInfo` ocupa 0x38 B: +0x14 tipo, +0x18 objeto, +0x1C activo, +0x20 terminado, +0x24 arrancado,
    +0x28/+0x2C posición propia.
  - Suena en 3D si la cámara está a menos de `GetPlayDistance` (0x4293E0: el máximo del banco, o 100), con sync y
    fundido = (grupo > 0).
  - Cada turno hace un re-disparo de `LHMusicPlay` + `Set3DPosition`.
  - Un objeto inactivo en rango bloquea igual el alineamiento.
  - Nadie pone +0x20 = 1 en 0x429180..0x429950 (duda abierta).
- **Ciudadela** 0x427B60 (44 + alineamiento discreto del jugador local; `LHSampleStopAll` una vez, bandera 0xC56164;
  vol 127, sync 1, fundido 1).
- **Pelea** 0x427660 / 0x427590 (estado de interfaz 0x10 o una `GArena` a < 100; 75 BIG_FIGHT si `g_game+0x205A0C`, si
  es multijugador o si `LandNumber ≥ 4`; si no, 74).
- **Cántico** 0x427790 (lugar de culto a ≤ 100, ciudadela a ≤ 150; tabla 0x9C9A30 `{28,28,30,…,42}` + (bailarines > 8)
  = la versión _vox; 3D en `GetSpecialPos(8)`).
- **Baile de la criatura** 0x427EC0 (acción 0x17 a < 75, Δaltura < 60; la acción 0x17 es **(inferido)**).

openblack:
- `GameMusic` (`src/Audio/GameMusic.{h,cpp}`, A5/A7/A9, **fiel**): ctor fn_00426D40 (parte de música), `Reset` 0x426CA0,
  `ProcessAudioGameTurn` 0x427080, `ProcessMusic` 0x427DF0, `StartScriptMusic` 0x428230, `ProcessScriptMusic`
  0x427CA0, los callbacks 0x426B40/0x426B80/0x426BA0, `ProcessAlignmentMusic` 0x4279C0 y fn_00427460, `DiscreteAlignment`
  0x414730, `AlignmentIndex` fn_00426C80, `TribeMusicType` fn_00427410, `SavePositions` fn_004281C0, `ResetPositions`
  fn_00428190, la música de objetos fn_00429790/fn_00429500/fn_00429420/fn_00429680/fn_004296C0, `PurgeThingMusic`
  fn_00429700 y las funciones CHL.
- `ThingMusicList` (`src/Audio/ThingMusic.{h,cpp}`): 0x429180, 0x429230, 0x429340, 0x4291B0, fn_00429880,
  fn_004298A0, 0x4298C0, 0x4298F0, y la ida y vuelta de `MapCoords` (0x603340, 6553.6 en 0x8AC400, 10/65536 en 0x8AA3A4).
- Ganchos de `Game.cpp`: `GAudio::ProcessAudioGameTurn` después del turno 5 (0x54E997) y `Reset` en `LoadMap`.
- Las ramas de ciudadela, pelea, cántico y baile son consultas sin dueño (false) hasta C1/C3/C4. El alineamiento de la
  cámara (GAudio+0x190) y los pueblos con tribu tampoco tienen dueño todavía: **hoy suena siempre la genérica neutral
  (tipo 2) a volumen 80** a partir del turno 20 en las tierras ≠ 6.
- **(aproximado)**:
  - Un tipo fuera de 0..84 no tiene banco: el original lee los campos siguientes.
  - El grupo 0 lee `pos[-1]`, fuera del array: aquí da 0, y el DLL arranca en el trozo 1 (pregunta 5 de PLAN §6).
  - Fuera de −1..1, el alineamiento y la tribu leen fuera de sus tablas: se toma neutral, o lo mismo que a partir de 9.
  - Sin cámara, un objeto queda fuera de rango.
  - `LHWaveIsActive` = el motor de música existe y está activo.
  - `GameThing::IsAvailable` = una entidad válida con Transform. Su posición en float sustituye a los MapCoords, sin el
    redondeo 16.16.
- **(inferido)**: el turno de openblack (que vuelve a 0 en cada `LoadMap`) hace de `g_game+0x205A40`, y la cámara de
  render hace de la de GGame.

### Intro, tráiler, outro, vídeos y menú

**Fiel salvo lo marcado** (`music.md` §2.7).
- **Tráiler**: `PlayPreIntroVideo` 0x6426F0 pone `pre_intro.bik` sin sonido Bink y toca `audio\music\intro\trailer.sad`
  (0xBFEB44) con `LHMusicPlay` (vol 127, inicio 1, sin fundido, 2D). Al acabar, `LHMusicStop(0)` (0x642907).
- **Intro**: START_MUSIC 54 en el guion `followus.txt::FollowUs`.
- **Outro**: START_MUSIC 77 en `landcontrol5.txt::VillageWavingSequence` (`Outro.sad`, 160 kbps, bucle −1).
- **Vídeos**: `PlayFullScreenMovie(path, sad)` 0x54D920 registra un banco opcional y fn_0054A9B0 lo arranca en la
  trama 3 (vol 127, inicio 1, fundido 1). También pone GAudio+0x1C = −1.
- **Menú principal**: ninguno de los sitios que llaman a `LHMusicPlay` es del menú, así que el menú no tiene música
  propia **(inferido)**.
- `Data\intro.wav` no aparece en el exe.
- `MusicMoodController` 0x633EF0 no es audio: son paquetes de red.
- openblack: **pendiente** (C5, no hay vídeos).

### La música en openblack

Resumen de la fase A. La cadena es `Game::Initialize` → `audio::music::Start()` (`LH_AudioSystem` init 0x1000DD50,
sobre el contexto del `AudioManager`) → `audio::game_music::Start(GameQueries, townTrigger)`. Por fotograma,
`audio::music::Update()` aplica el maestro de la configuración y el gancho de prueba. En el turno corre
`game_music::ProcessTurn`. Al cerrar, `game_music::Shutdown` y luego `music::Shutdown` (`LHMusicClose` 0x1000E7A0),
antes de soltar el contexto. Panel de depuración: la ventana «Music» (`src/Debug/Music.{h,cpp}`), con el maestro, los 6
canales, un reproductor de cualquier MUSIC_TYPE (con o sin sync y fundido; parar con fundido o en seco), el estado de
GameMusic (+0x28, +0x180, +0x1C, +0x20, +0x24, +0x18C, pos[grupo]), GScript +0x84..+0x9C y la lista de objetos.

## Voces y textos

### Tabla de voz de los textos

**Fiel** (`voices.md` §2.2, §3.2).
- La tabla está en 0x915D40, con copias idénticas en 0x942B38 (SAY), 0x957310 (GConfirmation) y 0x96BA30 (GGuidance).
  Son entradas `{u32 id, u32 banco, u32 muestra}`, 0x1B3E = **6974** textos (`HELP_TEXT_LAST`).
- El índice es la posición del `ADD_TEXT` en `Scripts\InfoScript2.txt`. `HelpTextEnums.h` de bw1-decomp es de otra
  versión y se desfasa a partir del 1009.
- **3477 textos tienen voz**: 1922 en HelpSprites (6), 1328 en villagers (7) y 227 en Guidance (10). Ninguno en
  VillagersBanter ni en SpellDialogue.
- **Se reconstruye desde los datos**: el nombre del texto es el nombre del .wav de su muestra. Se busca primero en
  villagers, luego en HelpSprites y luego en Guidance. Hay un solo duplicado: `HELP_TEXT_LAND_2_WORKSHOP_10` está en
  HelpSprites 801 y en villagers 399, y el exe da villagers 399. Que villagers vaya primero se deduce de ese único caso
  **(inferido)**.
- **Narradores** (InfoScript2 W120): 0 NONE, 1 DEFAULT, 2 GOOD_SPIRIT, 3 EVIL_SPIRIT, 4 WOMAN, 5 MAN, 6 OGRE, 7 KHAZAR,
  8 LETHYS, 9 NEMESIS, 10 BOY, 11 BIG_VOICE, 12 TRAINER. `ADD_TEXT` usa además GUIDE y MONK sin declararlos; su valor es
  **(inferido)**, y openblack les da −1.
- `HelpTextDatabase` 0xD17CA8 guarda {+0 narrador, +4 arg0, +8 texto} (fn_005CAD00). Sobre el texto se aplica la
  conversión fn_007191F0: `~` → 0xF8FE y `\n` → salto de línea.
- **Dueños fijos de canal**:

  | dueño | quién |
  |---|---|
  | 0x270C | consejeros (HelpDude) |
  | 0x270D | SAY con alt |
  | 0x270E | solo lo para STOP_SOUND_EFFECT(isSay); nadie lo reproduce |
  | 0x270F | narración y SAY sin alt |
  | el número de muestra | PLAY_SOUND_EFFECT del guion |

### Narración y tiempo de lectura

**Fiel salvo lo marcado** (`voices.md` §2.3-2.4; PLAN §8.1 n.º 12).
- **`RUN_TEXT`** (`GScript::RunText` 0x6F7D60): un id ≥ 6974 da «Invalid text» y pasa a 0. Con `singleLine` o con la
  bandera +0xB0, `ClearAllText` 0x5C5550. Después va a fn_005C5F90:
  - muestra el texto y empieza el tiempo de lectura (fn_005C6100 → fn_005C61B0);
  - lo guarda en la cola de 6 (+0x584..+0x598) y en el historial de 1024 (fn_005C5EE0);
  - y dice la voz:
    - banco 6 con narrador 2 o 3 → el **consejero** bueno o malo (`HelpDudeControl::Say` 0x5C36D0, con un retardo de
      0..500 ms y dueño 0x270C);
    - otro banco con muestra → `GAudio::PlaySoundEffect` 2D, dueño 0x270F, +0x164 = 1.
- **`IsTextRead`** 0x5C64E0 → 0x5C6340:
  - Con `withInteraction` espera al clic.
  - Si el texto tiene voz (fn_005C62F0: entrada, banco, muestra y el banco registrado):
    - consejero: el texto está leído cuando ninguno habla ni ha callado hace menos de 200 ms (fn_005BB730);
    - narración: mientras suena 0x270F, fin = ahora + 450 ms.
  - Si no tiene voz: dentro de la ciudadela, por ms; si no, por turnos.
- **Tiempo de lectura sin voz** (fn_005C61B0): `s = (readDefaultWordGTTime·n + readDefaultAdjustGTTime) · msPorTurno ·
  0,001 · f(READ_SPEED)`.
  - Las dos constantes son 5 y 8 en info.dat 0x4AEC4, en `HelpSystemInfo` 0xD1617C/0xD16178. Los valores salen por
    patrón **(inferido)**.
  - `msPorTurno` = [0xD01A38] = 100 (0x54F4A5).
  - f(r) = r ≤ 0,5 ? 3 − 4r : (1 − 2(r − 0,5))·0,8 + 0,2 (fn_005C6CB0), con READ_SPEED = 0,5 por defecto.
  - n = palabras (fn_005CBEC0 sobre el partidor fn_005CB590).
- **Clic** (`HelpSystem::ProcessInterface` 0x5C69B0, desde `GInterface` 0x5D11C0): con texto sin leer y (pantalla ancha
  o la tecla [0xE85410]):
  - para a los espíritus (fn_005C6720, sin leer);
  - hace `StopPlayingSoundEffect(0, 0x270F, 7)`: **solo villagers**, con una rampa de 20 ms (DLL 0x10012C50).
- `GAME_CLEAR_DIALOGUE` 0x6FF6F0 y `GAME_CLOSE_DIALOGUE` 0x6FF700 borran el texto, pero **no paran la voz**.
- **No hay ducking** de la música por las voces: la música solo cambia con las opciones. Esto es fiel en el exe; lo que
  pasa dentro del hilo del DLL es **(inferido)**.

### Consejeros, Guidance, confirmaciones y voces nocturnas

**Fiel en el original, pendiente en openblack** (`voices.md` §2.5-2.10).
- **Consejeros**:
  - `HelpDudeControl::Say` 0x5C36D0: v = |+0x3514| − 0,95 (0,95 en 0x915438); retardo = v < 0 ? 0 : min((v + 1)·250,
    500) ms (+0x3514 = posición de flotación del consejero, **(inferido)**).
  - `HelpDude::SaySentence` 0x5BB340 → `UpdateSaySentence` 0x5BB610 (por fotograma) → `PlaySample` 0x5BB530
    (`LHSamplePlay` directo, sin filtros, con el PCM para la boca).
  - Lip-sync: `ApplyLipSync` 0x5BCD00 y `AutoVoiceParams::CalcKey` 0x428850. CalcKey usa la FFT `AudioAnalyse::Analyse`
    0x428C60 + `four1` 0x428D50, que están sin volcar.
  - `IsTalking` 0x5BB760 y `StopSentence` 0x5BB840.
  - Efectos de los espíritus: `HelpDude::PlaySoundFX` 0x5C2800 (banco `GetSoundFXBank` 0x5BC7C0).
- **GGuidance** (`Guidance.sad`, 0x71AB10..0x71D490): reacciones de los aldeanos.
  - 33 tipos con una tabla de intervalos 0x980190 {base, nivel de ayuda, siempre}.
  - `PlayNow` 0x71AF50. El intervalo es base + rand(5·base·(1 − r³)) (0x71AEE0). `PlaySample` 0x71C6F0.
  - Los tipos 9..30 hacen hablar a un consejero (`HelpSpiritSay` 0x71D270).
  - `BeliefSFX` 0x71BF70 (umbrales 0,05/0,4/0,7, max 200).
  - Latido: InGame 45 en bucle, 3D, max 500 (0x71C190 → 0x71C460). El tono 30+70·v suavizado 0,1 es **(inferido)**.
  - Los disparadores 0x71B130..0x71D1C0 están sin volcar (PLAN §8.3 F4).
- **GConfirmation** (START_ANGLE_SOUND 285 / «START_ANGLE_SOUND» 348 = `StartPitchSound`):
  - Init 0x71A560, Start 0x71A610, Process 0x71A650.
  - HelpSprites 1686 (BETTER_14), 1673..1685 y 1704..1717.
  - Nunca dice «no»: es un fallo del original y se copia.
- **GSpookyVoices** 0x72E2A0..0x72E870: de noche (reloj real 20:45-20:59 o 23:00-05:59) susurra el nombre del perfil
  elegido por Soundex entre los 100 `HELP_TEXT_SPOOKY_NAMES_*`. El Soundex está sin volcar.
- La **criatura no habla**: `creature.sad` y los bancos por especie son efectos **(inferido)**.

### Voces y textos en openblack

- **`helptext`** (`src/Common/HelpText.{h,cpp}`, A10):
  - `Entry {arg0, narrator, name, text}`: el nombre lo necesita la tabla de voz; el original no lo guarda.
  - `Parse` (UTF-16 con o sin BOM, nombres de narrador resueltos con el propio archivo), `ConvertScriptText`
    fn_007191F0, `GetEntry`, `Count`.
  - `k_TextCount = 0x1B3E` y `k_NarratorGoodSpirit/EvilSpirit = 2/3`.
  - Un número de narrador se lee como `_wtoi` **(inferido)**: el lector `LHScriptX` 0x7E7960 está sin leer.
- **`audio::VoiceTable`** (`src/Audio/Voices.{h,cpp}`, A10): `Build` con la regla de arriba y `Get`. `voices::BuildTable`
  se llama en `Game::Initialize` con los nombres de onda que `Game.cpp` lee de los bancos 7, 6 y 10. Están también
  `VoiceOwner` (0x270C..0x270F) y `TextVoice::HasVoice` (fn_005C62F0).
- **`help::HelpSystem`** (`src/Help/HelpSystem.{h,cpp}`, A11, **solo la parte de texto**):
  - `RunText` 0x6F7D60, `RunTextWithNumber` 0x6F7C70, `TempText` 0x6F7E40, `TempTextWithNumber` 0x6F7F50,
    `ClearDialogue` 0x6FF6F0, `CloseDialogue` 0x6FF700.
  - `SayText` fn_005C5F90, `AddText` fn_005C6100, `ClearAllText` 0x5C5550, `ClearTextDisplayed` 0x5C54E0, `Reset`
    0x5C5580.
  - `IsTextRead` 0x5C64E0, `ProcessInterface` 0x5C69B0.
  - `CountWords` fn_005CBEC0, `ReadSpeedFactor` fn_005C6CB0, `RouteOf` (las ramas de 0x5C6025..0x5C60DB) y el historial
    fn_005C5EE0/fn_005C5F50.
  - **No se dibuja nada**: `OPENBLACK_TEXT_TRACE` escribe cada texto en el log.
  - Las voces no suenan todavía (B7). `voiceBankLoaded` no está registrado, así que IsTextRead siempre toma la rama del
    tiempo de lectura, como el original cuando los bancos de diálogo no están.
  - El clic es el botón izquierdo al bajar **(inferido)**.
  - El reloj escalado 0xEA1C78..0xEA1C80 son ms reales desde el arranque **(aproximado)**.
  - `+0x460C` (`GInterface::SetActive`) no está portado.

## Guion: CHL de audio

**Fiel** (`script.md` §2.1-2.7).
- La tabla CHL `g_scriptFunctionTable` 0xC0DB98 tiene 464 entradas de 0x90 B. Los **37 opcodes** de audio coinciden con
  los bindings de openblack.
- En W120 no existen ENABLE_BANTER, IS_PLAYING_SOUND, los fundidos de música ni STOP_ALL de audio: no hay que
  inventarlos.
- `ScriptErrorMessage` solo avisa: la función **sigue** con el valor malo.
- Estado en openblack: **hecho** = implementado en la fase A; **stub** = `NotImplemented`.

| op | nombre | original | openblack |
|---|---|---|---|
| 13 | RUN_TEXT | 0x6F7D60 | hecho (texto; voz en B7) |
| 14 | TEMP_TEXT | 0x6F7E40 | hecho (sin voz, como el original) |
| 15 | TEXT_READ | 0x6F8260 → IsTextRead | hecho (sin la rama de voz hasta B7) |
| 43 | PLAY_SOUND_EFFECT | 0x70F7F0 (dueño = n.º de muestra) | stub (B6) |
| 44 | START_MUSIC | 0x70FB20 (error fuera de 0..0x55 y sigue; +0x98 = +0x9C = 0) | hecho |
| 45 | STOP_MUSIC | 0x70FB90 | hecho |
| 46 | ATTACH_MUSIC | 0x70FBF0 (error fuera de 1..84 y lo añade igual) | hecho |
| 47 | DETACH_MUSIC | 0x70FC60 | hecho |
| 30 / 31 | START_CAMERA_CONTROL / END_CAMERA_CONTROL | 0x6ECCA0 / 0x6ECEF0 (al soltar: +0x84 = 1, 0x6ECE74) | hecho el estado; la cámara de guion, pendiente ([abajo](#diálogo-pantalla-ancha-y-cámara-del-guion)) |
| 32 | SET_WIDESCREEN | 0x6F7BF0 → HelpSystem::SetWideScreen 0x5C6AD0 | hecho, con el dueño |
| 120 / 122 | START_DIALOGUE / IS_DIALOGUE_READY | 0x710690 / 0x710830 (sin audio) | hecho |
| 121 | END_DIALOGUE | 0x710780 (+0x84 = 1, +0x9C = 0 solo si la tarea tiene el diálogo) | hecho |
| 149 | MOVE_MUSIC | 0x70FCA0 | hecho |
| 181 | ENABLE_DISABLE_MUSIC | 0x70FD10 | hecho |
| 182 | GET_MUSIC_OBJ_DISTANCE | 0x70FD70 | hecho |
| 183 | GET_MUSIC_ENUM_DISTANCE | 0x70FDE0 (con un tipo inválido empuja dos veces) | hecho, con el fallo |
| 184 | SET_MUSIC_PLAY_POSITION | 0x70FE60 | hecho |
| 190 | RESTART_MUSIC | 0x70FF00 | hecho |
| 191 | MUSIC_PLAYED (objeto) | 0x70FF40 (true si el objeto no vale) | hecho |
| 231 / 232 | TEMP_TEXT_WITH_NUMBER / RUN_TEXT_WITH_NUMBER | 0x6F7F50 / 0x6F7C70 | hecho |
| 246 | SPIRIT_SPEAKS | 0x710C40 (consulta) | sin cambios |
| 285 / 348 | START_ANGLE_SOUND / StartPitchSound | 0x70FFA0 / 0x70FFE0 | stub (C7) |
| 317 | SET_CREATURE_SOUND | 0x710020 | hecho |
| 335 | LAST_MUSIC_LINE | 0x710050 (sin audio: TEXT_READ, 0x7100A6) | hecho |
| 340 | GAME_PLAY_SAY_SOUND_EFFECT | 0x70F9B0 → 0x70F8E0 | stub (B7) |
| 350 | MUSIC_PLAYED (tipo) | 0x70FBA0 (GAudio+0x28 != tipo; sin audio: TEXT_READ, 0x70FBE4) | hecho |
| 357 | SET_GAME_SOUND | 0x7100B0 | stub en esta rama (hecho en agua, B6) |
| 409 | SOUND_EXISTS | 0x710100 → LHWaveIsInstalled | stub (B6) |
| 411 / 412 | GAME_CLEAR_DIALOGUE / GAME_CLOSE_DIALOGUE | 0x6FF6F0 / 0x6FF700 | hecho |
| 424 | STOP_SOUND_EFFECT | 0x70FA50 (isSay: narrador 2 → 0x270C; si no, 0x270E + 0x270D; nunca 0x270F) | stub (B6/B7) |
| 445 | ENABLE_DISABLE_ALIGNMENT_MUSIC | 0x710120 | hecho |
| 447 / 448 | ATTACH / DETACH_SOUND_TAG | 0x710150 / 0x7101D0 → 0x71E840 / 0x71EBE0 | stub (B6) |
| 450 | GAME_SOUND_PLAYING | 0x710230 | stub (B6) |
| 458 | SAY_SOUND_EFFECT_PLAYING | 0x710280 | stub (B7) |

`GetScriptGameThing` 0x70D220 se aproxima en `MusicThing` (CHLApi.cpp): 0 es nulo y una entidad válida es un objeto
vivo. El original busca el id en su tabla 0xD967F8 (1..0x1FF) **(aproximado)**. `CHAR2WCHAR` 0x8300A0 se hace byte a
byte, sin el límite de 0x7FF **(aproximado)**: solo cambia en 0x80..0x9F de CP-1252.

### Interruptores de GScript

**Fiel.** Están en g_game+0x250090 y son atómicos en openblack, porque el hilo de música escribe la línea y los golpes.
Viven en `src/Audio/ScriptAudioState.{h,cpp}` (A6), y `GScript::Reset` 0x6EB2D0 (`ScriptAudioState::Reset`, desde
`LoadMap`) los deja así:

| campo | qué es | tras Reset | quién lo escribe y quién lo lee |
|---|---|---|---|
| +0x84 | sonidos de las criaturas | 1 (0x6EB2F4) | `SET_CREATURE_SOUND`; `END_DIALOGUE` (0x71080A) y la suelta de la cámara de guion (fn_006ECD70 0x6ECE74, desde `END_CAMERA_CONTROL` y al parar la tarea) lo ponen a 1. Lo lee fn_00483290+0x16A: con 0, solo suena la criatura local |
| +0x90 | solo diálogo (SET_GAME_SOUND) | 0 (0x6EB403) | se guarda aquí para el reset; se usa en B6 |
| +0x94 | música de alineamiento | 1 (0x6EB409) | `ProcessAlignmentMusic` no toca nada con 0 (0x427A20) |
| +0x98 | última línea `L<n>` de la música del guion | 0 (0x6EB306) | 0x426BD1; START_MUSIC la pone a 0 |
| +0x9C | golpes | 0 (0x6EB30C) | 1 en `L`, +1 en `P`/`W`. Lo lee fn_005CB590+0x62B, **(inferido)**: el texto de una canción lo sigue |

`HelpSystem::Reset` (0x6EB340) también va en `LoadMap`.

### Diálogo, pantalla ancha y cámara del guion

**Fiel** en el estado (`src/Help/ScriptControl.{h,cpp}` y `HelpSystem`; `script.md` §2.8). El original guarda qué
**tarea** del guion tiene cada cosa, con `ScriptDLL::TaskNumber` 0x6F69F0 (ScriptLibraryR.dll 0x10008320: la tarea
en curso, +0x14; 0 fuera de una tarea). openblack lo da con `LHVM::GetCurrentTaskNumber` (la tarea `_currentTask`
que corre la función nativa), `GetCurrentTaskScriptType` (0x6F6A90) y `GetTaskScriptType` (0x6F6C50 → 0x100051F0:
+0x158, 1 si la tarea no existe).

| campo | qué es | quién lo escribe |
|---|---|---|
| HelpSystem+0x45CC | la tarea que tiene el diálogo | `DialogueControlRequest` 0x5C6790 (si nadie lo controla), `ClearDialogueControl` 0x5C67E0 |
| HelpSystem+0x45E8 / +0x45EC | pantalla ancha y la tarea que la puso (0 = el juego) | `SetWideScreen` 0x5C6AD0 (solo si +0x45E8 cambia; al apagar, +0x45EC = 0) |
| GScript+0xA8 | la tarea que tiene la cámara | `START_CAMERA_CONTROL` 0x6ECCDF / 0x6ECD5B; 0 al soltar (0x6ECE59) |
| GScript+0x78 / +0x80 | se dibujan las correas / los resaltados | 0 al coger la cámara fuera de la ciudadela; 1 al soltarla y en Reset |

- `IsDialogueControlled` 0x5C6740 = +0x45CC != 0, o +0x45E8 y +0x45EC. `IS_DIALOGUE_READY` empuja su negación.
- `START_DIALOGUE`: sin dueño, los dos consejeros a casa (`SpiritHome(1/2, 0)`) y la petición; empuja **true aunque
  la petición falle** por la pantalla ancha de otra tarea (0x710722..0x71072E). La misma tarea: aviso y true. Otra:
  false, salvo si el dueño es Help (tipo 2) y la que pide es Script (1): `StopHelpScripts` (máscara 0x4A) y se
  vuelve a mirar el dueño.
- `END_DIALOGUE`: solo la tarea dueña. `SpiritHome(1/2, es Help)`, fn_005C6800 (suelta el diálogo, quita la pantalla
  ancha, consejeros a casa, borra el texto) y +0x84 = 1, +0x9C = 0.
- `SET_WIDESCREEN`: solo la dueña o cualquiera si no hay dueña; si la dueña la vuelve a encender (valor != 0)
  avisa y sigue (0x6F7C23..0x6F7C32).
- `START_CAMERA_CONTROL`: en la ciudadela (g_game+0x205A28 == 1) solo tareas TempleHelp/TempleSpecial (0x18), sin
  modo de cámara. Fuera, si se crea el modo `CameraModeScript` (fn_00461140; no si `CantExitCurrentMode`).
- `END_CAMERA_CONTROL`: si la tarea la tiene, fn_006ECD70: modo `CameraModeNew3`, FOV 70° (0x8C762C) en 0.5 s,
  +0xA8 = 0, +0x84 = +0x80 = +0x78 = 1, fn_0042A5F0(1), SuperVillagers fuera, +0x7C = 0.
- Al **parar una tarea** (callback 0x6EC6D0, el `stopTaskCallback` de LHVM): fn_005C6800(tarea) y fn_006ECF20(tarea)
  devuelven el diálogo y la cámara. ScriptLibraryR.dll lo llama antes de sacar la tarea de la lista (0x100065CD,
  la baja desde 0x10006612): fn_005C6800 aún ve su tipo, igual que en LHVM.

En openblack falta lo visual: no hay modos de cámara (se toma siempre **(inferido)**, y la cámara no cambia), ni FOV
de salida, ni consejeros (`SpiritHome`, fn_005C6720 son ganchos vacíos), ni `DialogBoxBase::HideAll` /
`GInterface::SetActive`. La ciudadela es el interior del templo de openblack **(inferido)**. Las barras sí:
el gancho de `SetWideScreen` mueve `ScreenFade` (en 16:9 no se ven, como en el original). La música de alineamiento
ya lee la pantalla ancha del guion (0x4279E9).

En juego (Tierra 1, `FollowUs`): START_CAMERA_CONTROL → START_DIALOGUE → START_GAME_SPEED → SET_WIDESCREEN →
**START_MUSIC 54** (suena intro.sad). Ahora se para en `FollowUs_loop_4`: espera a que el padre llegue a
`FatherPosKiss` (GET_DISTANCE == 0), y `MOVE_GAME_THING` (033) es stub. El primer RUN_TEXT viene después.

## Fase A implementada

La fase A (PLAN §4) se hizo en el worktree `openblack-audio` (rama `local/audio`). Solo **añade** código: no toca los
archivos de `src/Audio` que reescribe agua, ni `Debug/Audio.cpp`, ni retira `AudioManager::PlayMusic`.

| hito | estado | archivos | test |
|---|---|---|---|
| A1 Tablas | hecho | `src/Audio/BankTables.h`, `components/pack` (`AudioBankInfo`) | `test_audio_tables` |
| A2 MusicBank | hecho | `src/Audio/MusicBank.*` | `test_music_bank` (contra `music_sad_table.md`) |
| A3 MusicEngine | hecho | `src/Audio/MusicEngine.*` | `test_music_engine` (0→127 en 32 vueltas, 127→0 en 43, 80→0 en 27, sync, 7.ª pista, maestro) |
| A4 Streaming MP2 | hecho, sobre el contexto de `AudioPlayer` | `src/Audio/MusicStream.*`, `src/Debug/Music.*`, `Debug/Gui.cpp` (dos líneas) | `test_music_stream` |
| A5 Música del guion | hecho | `src/Audio/GameMusic.*`, `CHLApi.cpp` | `test_game_music` |
| A6 Estado del guion | hecho | `src/Audio/ScriptAudioState.*`, `CHLApi.cpp` | `test_game_music` |
| A7 ThingMusic | hecho | `src/Audio/ThingMusic.*`, `CHLApi.cpp` | `test_game_music` |
| A8 Volumen de música | parcial: `EngineConfig::audioMusicMasterVolume` y el deslizador de depuración; no se guarda, y no está en el menú | `src/EngineConfig.h`, `src/Debug/Music.cpp` | traza `gain=` |
| A9 Alineamiento y tribu | lógica hecha; sin dueño para el alineamiento en la cámara ni para la tribu de los pueblos | `src/Audio/GameMusic.*`, `src/Audio/GameQueries.h` | `test_game_music` (con consultas falsas) |
| A10 Tabla de voz | hecho | `src/Common/HelpText.*`, `src/Audio/Voices.*` | `test_voice_table` (6974 / 3477 / 1922 / 1328 / 227, WORKSHOP_10 → villagers 399) |
| A11 HelpSystem (texto) | hecho, sin dibujo | `src/Help/HelpSystem.*`, `CHLApi.cpp`, `Game.cpp` (clic) | `test_help_system` |

Los tests que leen datos usan `OPENBLACK_TEST_BW_ROOT` y hacen `GTEST_SKIP` si no hay instalación.

## Fases B y C

**Pendiente** (PLAN §4-5). La fase B empieza cuando Milagros y agua estén fusionados en `local/hand-hbn`:

| hito | contenido |
|---|---|
| B0 | Arreglos del reproductor: `continue` en la muestra vacía, RIFF 0x50 → dr_mp3, un búfer AL por onda (fuera la fuga), bucles finitos y tramo, «villagers.sad» sin distinguir mayúsculas, AL_PITCH |
| B1 | Núcleo `audio::` (`Audio.h`, `AudioSystem`, `GameSfx`, `QMixerLaws`, SamplePlay con `SfxBank` y `Owner`), maestro de efectos persistente, `ClearMap`/`Paused`/`OnFocus`; `MusicStream` se funde con `AudioPlayer` y se retira `AudioManager::PlayMusic` |
| B2 | AnimEffects único (AnimEffectBank de Milagros + AnimationSounds) |
| B3 | SoundTags completo; `LanternSounds` encima |
| B4 | Llamadores del mundo: mano, árboles, rocas, cámara (woosh con d > 150 también en los marcadores), colisiones, barco, volcán, vapor |
| B5 | Milagros: SpellSounds, FireSound, gesto 3D, PlayTapSound, SpellDialogue por canal; PSys `AddSoundToAtom` 0x69DCA0… (F3) |
| B6 | CHL de efectos: PLAY/STOP_SOUND_EFFECT, GAME_SOUND_PLAYING, ATTACH/DETACH_SOUND_TAG, SOUND_EXISTS, SET_GAME_SOUND; ambiente (`GSoundMap` 0x71D6F0, LHAtmos 0x428FE0 / 0x100018B0, de agua) |
| B7 | Voces en canal: RunTextVoice, SAY, STOP, TEXT_READ con voz (+450/+200 ms), corte por clic, `Advisor` y CalcKey |
| B8 | Interfaz y mano: MenuButton 159, Logo 160, ClickOnSpell 42, conquista 205, gritos 180/187/194+rand7, llamar a la puerta 110+c%9, influencia 52/129 |
| B9 | GGuidance, BeliefSFX, latido |
| B10 | GSpookyVoices (antes hay que volcar el Soundex 0x72E4E0..0x72E870) |
| C1 | Criatura: cola de eventos, clave de 5 columnas, bancos por especie, filtro de jugador local / SET_CREATURE_SOUND; baile y pelea en GameMusic |
| C2 | Clima y alineamiento en el ambiente |
| C3 | Aldeanos, edificios y cánticos |
| C4 | Ciudadela interior y `ProcessCitadelMusic` |
| C5 | Vídeos (tráiler, `PlayFullScreenMovie`) |
| C6 | Guardar y cargar: `GAudio::Save` 0x428310 / `Load` 0x428480, `ThingMusicInfo::Save` 0x429950 / `Load` 0x429AE0, `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0 |
| C7 | GConfirmation (necesita `CameraModeNew3` 0x454900/30) |

Mientras tanto, se prohíbe añadir llamadores nuevos a los módulos viejos (`AnimationSounds`, `LanternSounds`, `AudioManager`).

## Qué suena y cuándo

Cada página de tema dice qué suena y cuándo. Aquí solo está el motor:
- Coger y soltar, montones, vasijas: [objects-and-resources.md](objects-and-resources.md#sonidos-informe-tmp_dissoundnotestxt).
- Sonidos de los clips de animación y el banter: [animation.md](animation.md#sonidos-de-los-clips).
- Golpes, choques y lanzamientos: [physics.md](physics.md#sonidos-polvo-y-aspecto-de-los-golpes).
- Farolas (SoundTag, de noche): [day-night-weather.md](day-night-weather.md#luces-de-noche-informe-night_visualstxt).
- Árboles (hojas, caída): [objects-and-resources.md](objects-and-resources.md).
- Partículas de los milagros (SOUND_ACTION, PSysSound, spells.sad): [particles.md](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp).
- Agua (la mano en el agua, golpes, ahogarse, barco, cascada y arca, ambiente del mar, la costa y los lagos):
  [water.md](water.md#audio-del-agua). Hechizos: [particles.md](particles.md) y las páginas de Milagros.
- Inventario completo de los efectos del original (cada llamada, banco y muestra): `tmp_dis\audio\sfx_inventory.md` y
  `sfx_inventory_tables.md`. Interfaz y criatura: `ui_creature.md`.

## Pendiente

- **Fases B y C** completas ([arriba](#fases-b-y-c)). La fase A no se oye en el juego, salvo la música.
- **A8**: guardar `AudioMusicMasterVolume` y `AudioSampleMasterVolume`, y dónde va el deslizador. Pregunta 4 de PLAN §6.
- **A9 en juego**: falta quién da el alineamiento en la cámara (GAudio+0x190, fn_005E2240 desde fn_0064AC30) y la tribu
  de los pueblos (Town +0x5B8). Hoy suena la genérica neutral.
- **Música**:
  - quién pone ThingMusicInfo+0x20;
  - `LandNumber` 6 y `g_game+0x205A0C`;
  - si `GetDistanceInMetres` es 2D o 3D (0x74CCB0);
  - quién llama a `LHMusicPause`;
  - el grupo 0 (`pos[-1]`).
- **Bucles**: N o N+1 pasadas (pregunta 2 de PLAN §6).
- **Texto**:
  - los valores 8/5 de info.dat;
  - GUIDE y MONK;
  - `GRand::LocalRand(14)`: 0..13 o 0..14 (0x6DE570);
  - qué hace fn_005C6720 (HelpDudeControl fn_005C3780, se lee HelpDude::IsTalking 0x5BB760);
  - la cámara de guion (CameraModeScript 0x461180, CameraModeNew3, FOV de salida) y los consejeros de
    `SpiritHome`;
  - la tecla [0xE85410];
  - el dibujo del texto (HelpText fn_005CCED0).
- **Sin volcar**: la FFT del lip-sync (0x428C60, 0x428D50), los disparadores de Guidance (PLAN §8.3 F4), el Soundex,
  `GSoundMap::Reset` 0x71D6D0 y los modificadores de sonido de PSys (F3).
- **Lista de (aproximado) e (inferido) del código de la fase A**, todos con su comentario en el código:
  - `MusicThing` (tabla de objetos del guion) y `CHAR2WCHAR`;
  - la cámara de guion siempre se toma (sin modos de cámara) y la ciudadela como el interior del templo;
  - `GameThing::IsAvailable` y los MapCoords en float;
  - `LHWaveIsActive`;
  - el turno de openblack como `g_game+0x205A40`;
  - la cámara de render como la de GGame;
  - los tipos y alineamientos fuera de rango;
  - el grupo 0;
  - el silencio ante un fallo de decodificación;
  - la cola vacía en OpenAL;
  - la unidad de `lStart` y del play position;
  - el aviso dentro de `FlushChannel`;
  - el reloj de los marcadores en double;
  - el marcador sin etiqueta;
  - el reloj escalado en ms reales;
  - el clic con el botón izquierdo al bajar;
  - el número del narrador como `_wtoi`.
- **Fallos del original que se copian** (confirmar con el usuario, pregunta 6 de PLAN §6):
  - el niño sacrificado pide ScriptSfx 179, que no existe;
  - GConfirmation nunca dice «no»;
  - falta MONSTER_09;
  - STOP_SOUND_EFFECT(isSay) no para 0x270F;
  - WELCOME_DANCE no tiene fichero;
  - GET_MUSIC_ENUM_DISTANCE empuja dos veces.
- **Comprobaciones en el original** que solo puede hacer el usuario (pregunta 7 de PLAN §6):
  - si hay música en el menú;
  - si `_vox` es el cántico completo más las voces;
  - si vuelve la música tras un Alt-Tab.

## Ganchos de prueba

| Gancho | Qué hace |
|---|---|
| `OPENBLACK_MUSIC_TRACE=1` | Una línea `music:` por vuelta del hilo con cada canal ocupado: banco, estado, cur/target/sad, trozo, vueltas, cola, volumen QMixer, ganancia y muestras decodificadas. También escribe `game music: Music Playing=…` cada vez que cambia |
| `OPENBLACK_TEST_MUSIC="<tipo>[,<tipo>@<s>][,stop@<s>][,cut@<s>]"` | La primera pista se toca como el tráiler (vol 127, sin sync ni fundido, 2D). Cada una de las siguientes, a los `s` segundos, con sync y fundido, como `ProcessCitadelMusic` (para oír un cambio sincronizado). `stop` = `LHMusicStop(1)`, `cut` = `LHMusicStop(0)`. Es un gancho, no un comportamiento del original |
| `OPENBLACK_TEST_MUSIC_VOLUME=<0..127>` | El maestro de música al arrancar |
| `OPENBLACK_TEST_SCRIPT_MUSIC="<tipo>[@<turno>]"` | Un START_MUSIC del guion en ese turno (30 por defecto) |
| `OPENBLACK_TEXT_TRACE=1` | Cada texto de RUN_TEXT/TEMP_TEXT en el log (`|` por cada salto de línea) |
| `OPENBLACK_TEST_BW_ROOT=<instalación>` | Para los tests con datos: `test_audio_tables`, `test_music_bank`, `test_music_stream`, `test_game_music`, `test_voice_table`, `test_help_system` |
| Ventana de depuración «Music» | Maestro, 6 canales, reproductor de MUSIC_TYPE, estado de GameMusic y de GScript, lista de objetos con música |

Ejemplo: `OPENBLACK_TEST_MUSIC="3,1@20" OPENBLACK_MUSIC_TRACE=1` arranca good.sad y a los 20 s cambia a evil.sad
sincronizado (mismo trozo).

## Fuentes

- Informes en `C:\Users\diewgarc\dev\tmp_dis\audio\`:
  - `PLAN.md` (síntesis, arquitectura §2, hitos §4, revisión crítica §8);
  - `engine.md` (motor, .sad, caché, canales, volúmenes);
  - `music.md`, `music_sad_table.md`, `music_types.md` (música);
  - `voices.md` (voces, tabla 0x915D40, HelpSystem, consejeros, Guidance);
  - `script.md` (los 37 CHL);
  - `openblack_audit.md` (auditoría de los tres árboles);
  - `sfx_inventory.md`, `sfx_inventory_tables.md` (cada efecto);
  - `ui_creature.md` (interfaz y criatura).
- Volcados: `music_dll_play.txt`, `music_dll_thread.txt`, `music_dll_stop_etc.txt`, `music_dis_process.txt`,
  `music_dis_init.txt`, `script_dis_thingmusic.txt`, `script_dis_helpsys.txt`, `script_dis_text.txt`,
  `engine_bankreg.txt`, `engine_sadblocks_out.txt`, `engine_loops_out.txt`, `voices_texttable.txt`, `voices_namerule.py`.
- Otros: `tmp_dis\sound\notes.txt` (registro de muestra), `tmp_dis\agua\audio.md` y `tmp_dis\agua\re\NOTES.md`
  (SamplePlay, ambiente, leyes de QMixer), `anim\sounds_props.md` (.sas y tablas de animación).
- Código: `src/Audio/{BankTables.h, GameQueries.h, MusicBank, MusicEngine, MusicStream, GameMusic, ThingMusic,
  ScriptAudioState, Voices}`, `src/Help/HelpSystem`, `src/Common/HelpText`, `src/Debug/Music`, `components/pack`
  (`AudioBankInfo`), `src/CHLApi.cpp`, `src/Game.cpp`; tests `test/test_{audio_tables, music_bank, music_engine,
  music_stream, game_music, voice_table, help_system}.cpp`.
