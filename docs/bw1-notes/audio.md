# Audio: el motor, los bancos, la música, las voces y el guion

Esta página explica cómo suena Black & White 1. Cubre el motor del original (GAudio en `runblack.exe` sobre LHaudiodllR
y QMixer), los bancos y sus formatos (.sad, .sas y la música MP2 en segmentos), la música (LHMusic y la parte de música
de GAudio), las voces y los textos, y las funciones CHL de audio. Para cada tema se dice qué hace openblack: la fase A
y los hitos B0..B10 de la fase B están hechos; la fase C queda pendiente.
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
- [Fase B: B0 y B1 implementados](#fase-b-b0-y-b1-implementados)
- [Fase B: B2 y B3 implementados](#fase-b-b2-y-b3-implementados)
- [Fase B: B4 y B6 implementados](#fase-b-b4-y-b6-implementados)
- [Fase B: B7 implementado (voces en canal)](#fase-b-b7-implementado-voces-en-canal)
- [Fase B: B8 implementado (interfaz y mano)](#fase-b-b8-implementado-interfaz-y-mano)
- [Fase B: B9 y B10 implementados (Guidance y voces nocturnas)](#fase-b-b9-y-b10-implementados-guidance-y-voces-nocturnas)
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
                 Banks (LHBankRegister: bancos, tablas, ondas perezosas, música) · WaveBuffers (búfer por onda)
                 SamplePlay (16 canales, de agua) · MusicEngine (LHMusic: 6 pistas, hilo de 120 ms)
                 QMixerLaws (volumen, distancia, polar)
 0. Dispositivo  Device (el único que llama a OpenAL: un dispositivo, un contexto, alGetError en cada llamada)
                 AlSampleOutput (16 canales) · MusicStream (6 pistas) + decodificadores (dr_wav, dr_mp3 capa II)
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
7. **Un solo motor (B11a).** Un solo dispositivo OpenAL (`src/Audio/Device.{h,cpp}`, `audio::device`): los canales
   (`AlSampleOutput`), los búferes (`WaveBuffers`) y la música (`MusicStream`) le piden fuentes y búferes; nadie más
   incluye `AL/al.h`. Una sola carga de bancos (`src/Audio/Banks.{h,cpp}`, `audio::banks`), llamada desde `audio::Init`.
   No hay `AudioManager`, `AudioPlayer`, `AlCheck`, `SoundGroup` ni `Locator::audio`.
8. **Los contadores cíclicos van en un `enum class Counter`** con la dirección en el comentario, no con direcciones
   como claves. La API no tiene argumentos por defecto inventados.

### Estado del motor de efectos en openblack

**Fases B0..B8 hechas** ([B0-B1](#fase-b-b0-y-b1-implementados), [B2-B3](#fase-b-b2-y-b3-implementados),
[B4-B6](#fase-b-b4-y-b6-implementados), [B5](#fase-b-b5-implementado-los-milagros-en-canal),
[B7](#fase-b-b7-implementado-voces-en-canal), [B8](#fase-b-b8-implementado-interfaz-y-mano)). Hay un solo motor de canales: los 16 canales de
`audio::sample_play` (LHSamplePlay), cada uno con su fuente OpenAL propia y **fuera del registro ECS**, detrás de los
filtros de GAudio (`AudioSystem`) y de la API pública `src/Audio/Audio.h`. Desde B4 todo el mundo (mano, árboles, rocas,
cámara, física, edificios, barco, montones) y los CHL de efectos van por ahí, y desde B5 también los milagros
(`SpellSounds`, `FireSound`, `HandSpellSeed`, `Gesture`, `HandMagicFX`, `SpellSeed`, `WorshipSpellIcon`, `MagicTeleport`,
`Fireball`, `OneOffSpellSeed`, `FireGraphic`) y el panel de depuración. **Ya no hay emisores** (B5) ni segundo motor
(B11a, [abajo](#fase-b-b11a-un-solo-motor)): `AudioManager`, `AudioPlayer` y `Locator::audio` se retiraron; el
dispositivo es `audio::device` y los bancos son `audio::banks`.

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
- openblack (B0, `src/Audio/WaveBuffers.*`): los .sad se leen enteros al arrancar (como antes), salvo los de diálogo
  (tipos 6..10, `Audio\Dialogue`, desde B7): de esos solo se leen las cabeceras (`PackFile::ReadAudioHeaders`) y cada
  onda se lee del fichero al decodificarla (`Sound::waveFile`, `banks::ReadWave`), como `LHBankRegister(path, 0)`.
  Que el resto de bancos se lea entero es **(aproximado)**: el original los registra todos así (0x426EEE). Cada muestra se
  **decodifica una sola vez, al primer uso**, a un búfer AL que se guarda (`Sound::bufferId`) hasta cerrar el audio.
  **(aproximado)**: un búfer por registro de muestra, no por onda +0x108 (los clones se decodifican cada uno), y sin
  presupuesto ni expulsión FIFO (RAM/8 solo importa con menos de 1 GB). RIFF con wFormatTag 0x50 (o 0x55) → el bloque
  `data` a dr_mp3 (capa II), como ACM; 1 y 2 → dr_wav; lo que no es RIFF → MPEG crudo. El tramo +0x138/+0x13C va al
  búfer como `AL_LOOP_POINTS_SOFT`.

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
- **`LHSampleStop`** (0x10012C50 por muestra o dueño, 0x10012DF0 por canal, y el corte de
  `LHSampleUpdate3DChannels` 0x1001439D/0x100143BC): `QSWaveMixSetPanRate(20 ms)`, `SetVolume(0)`, `Sleep(20)`,
  `SetPanRate(100)` y `Flush`: un fundido de 20 ms y el juego espera esos 20 ms. Desde B7 openblack lo hace igual
  (`SampleOutput::StopRamped`: cuatro pasos de ganancia de 5 ms, **(aproximado)**: OpenAL no tiene pan rate).
  `LHSampleStopAll` 0x10012BF0 y el reinicio de un canal cortan en seco.
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
  - No es el foco, es **minimizar**: `GameWindowProc` pone el estado 0x8002 con wParam 1 (0x7DBFF6..0x7DC009,
    **inferido** WM_SIZE SIZE_MINIMIZED) y sub_7DE8D0 (0x7DE8DC) llama a `AltTabDeactivate` 0x7DE6D0. Al restaurar
    (0xF120 pone la bandera [0xE8C0FB], 0x7DC23A..0x7DC245, **inferido** WM_SYSCOMMAND SC_RESTORE),
    `ProcessWindowMessages` 0x7DB9DB llama a `AltTabReactivate` 0x7DE6F0. Mientras está minimizado,
    `ProcessWindowMessages` no sale del bucle de mensajes (`AltTabbedAway`, 0x7DB9E0): el juego entero se para.
  - Al minimizar, `LHGlobalSwitch(0)`: `LHWaveSwitch(0)` (StopAll) y `LHMusicSwitch(0)` (`LHMusicStop(0)`).
  - Al volver, `LHGlobalSwitch(1)` solo reactiva las banderas: **no se reanuda nada**.
- **`GAudio::Reset`** 0x426CA0 (desde `GGame::Init`). En orden:
  1. Pone a cero +0x18C, pos[grupo] (fn_00428190) y +0x28/+0x24/+0x180/+0x190, y +0x1C = −1.
  2. `LHMusicStop(0)`, `LHAtmosProcess(0)` y `StopAll`.
  3. `LHGlobalSwitch(0)`, espera a que no suene nada, `ClearInfoList` y `LHGlobalSwitch(1)`.
  4. `ReleaseAllThingMusicInfo` 0x4291B0.
- **Pausa**: `PauseGame` 0x54AE20 no toca el audio; cambia el bit 4 de g_game+0x14 (0x54AE2F). `EndTurn` con ese bit
  hace GSoundMap::Update (0x54E96F) y ProcessSoundTags (0x54E989) como siempre y luego `AtmosProcess(0)` (0x54E993,
  0x4286C0) en vez de ProcessAudioGameTurn.
  Quién llama a `LHMusicPause` 0x1000EA10 está sin leer.
- openblack:
  - Volumen de música: `EngineConfig::audioMusicMasterVolume` (127) en el panel de depuración «Music» (A8). Todavía no
    se guarda en disco.
  - La parte de música de `GAudio::Reset` y `GScript::Reset` va en `Game::LoadMap`.
  - Volumen de efectos (B1): `EngineConfig::audioSampleMasterVolume` (127), aplicado en vivo con
    `LHSampleSetMasterVolume` (reaplica a los canales en uso) y deslizador `ftol(slider·127)` en la pestaña «Channels»
    del panel de audio. No se guarda.
  - Alt-Tab (B1): `SDL_WINDOWEVENT_MINIMIZED/RESTORED` → `audio::OnFocus` → `LHWaveSwitch` + `LHMusicSwitch`. Perder
    el foco no apaga nada, como en el original. Los canales de ambiente no los para `LHSampleStopAll` (0x10012C13);
    en el original el juego entero se para minimizado, en openblack sigue corriendo y el ambiente sigue
    **(aproximado)**.
  - Pausa (B1): `audio::Paused` = `AtmosProcess(0)`. La pausa de openblack no tiene reloj de turnos (se llama cada
    fotograma), así que GSoundMap::Update y ProcessSoundTags no corren en pausa **(pendiente)**.

### .sas y tablas de animación

`Data\SmallSounds.SAS` es el texto de los eventos de sonido por clip de animación (`LoadAllAnimations` 0x550180). Junto
con las tablas `LHAudioAnimArrayTable` de cada .sad, alimenta `SamplePlayAnimEffect` 0x42A4B0 → 0x10014A20. La clave
tiene 5 columnas y hay 3 acciones (0 tocar, 1 parar, 2 soltar). El detalle de los clips y el banter está en
[animation.md](animation.md#sonidos-de-los-clips). El original no tiene otro formato de audio. En openblack las tablas
de cada banco se leen una vez al registrarlo ([B2](#b2-los-anim-effects-en-el-núcleo)).

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
  - Usan el **dispositivo de audio** (`audio::device`, desde B11a; antes el contexto de `AudioPlayer`), sin abrir otro. Tienen una fuente por canal, con su cola de
    búferes y un decodificador dr_mp3 continuo por pista (`MusicSegmentDecoder`).
  - El hilo hace una vuelta y luego espera 120/5000 ms. Bombea cada 20 ms.
  - Hay un cerrojo recursivo, que hace de la sección crítica 0x100562B0.
  - Ganancia = volumen QMixer / 32767, por la ley 3D de QMixer.
  - Los bancos de MUSIC_TYPE los registra `banks::MusicBankOf` (B11a) al primer uso.
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
sobre el dispositivo `audio::device`) → `audio::game_music::Start(GameQueries, townTrigger)`. Por fotograma,
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
- **GGuidance** (`Guidance.sad`, 0x71AB10..0x71D490): reacciones de los aldeanos y comentarios de los consejeros.
  Todo leído (volcado `tmp_dis\audio\voices_guidance_71ab10.txt`); el detalle está en
  [B9](#fase-b-b9-y-b10-implementados-guidance-y-voces-nocturnas).
  - 33 tipos con una tabla de intervalos 0x980190 {base, nivel de ayuda, siempre}.
  - `PlayNow` 0x71AF50: un tipo no «siempre» calla en el **Land 1 de una partida de un jugador que no es el
    playground** (g_game+0x205A08 == 1, `IsMultiplayerGame` 0x552F80, +0x205A0C); luego el nivel de ayuda
    (HelpSystem+0x45F8 ? +0x45F4 : 0) y turnos desde la última vez > intervalo = base + rand(5·base·(1 − r³))
    (0x71AEE0, sorteado en cada llamada). `PlaySample` 0x71C6F0.
  - Los tipos 9..30 (y 31, 32) hacen hablar a un consejero: `HelpSpiritSay` 0x71D270 arranca el guion de ayuda
    `MultiHelpJustTalkWithText(texto, texto)` (HelpSystem.txt), que espera el diálogo, hace RUN_TEXT y espera a que se lea.
  - `BeliefSFX` 0x437F40 → 0x71BF70 (umbrales 0,05/0,4/0,7, max 200).
  - Latido: InGame 45 en bucle, 3D en la **ciudadela** si su corazón (+0x30) vive, max 500, tono = 30 + 70·v
    suavizado 0,1 (0x71C460, ya **fiel**: 0x8BF51C, 0x92B2C8, 0x8AB22C).
- **GConfirmation** (START_ANGLE_SOUND 285 / «START_ANGLE_SOUND» 348 = `StartPitchSound`):
  - Init 0x71A560, Start 0x71A610, Process 0x71A650.
  - HelpSprites 1686 (BETTER_14), 1673..1685 y 1704..1717.
  - Nunca dice «no»: es un fallo del original y se copia.
- **GSpookyVoices** 0x72E2A0..0x72E870: de noche (reloj real 20:45-20:59 o 23:00-05:59) susurra el nombre del perfil
  elegido por Soundex entre los 100 `HELP_TEXT_SPOOKY_NAMES_*` (volcado `tmp_dis\audio\spooky_72e130.txt`; detalle en
  [B10](#fase-b-b9-y-b10-implementados-guidance-y-voces-nocturnas)).
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
  - Las voces van por los ganchos `sayVoice`, `stopVoicesOnClick` y `spiritStop` y las consultas `voiceBankLoaded`,
    `advisorsTalking` e `isPlaying`, que `Game.cpp` conecta con `audio::voices` y `audio::advisor` (B7,
    [abajo](#fase-b-b7-implementado-voces-en-canal)).
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
| 13 | RUN_TEXT | 0x6F7D60 | hecho (texto A11, voz B7) |
| 14 | TEMP_TEXT | 0x6F7E40 | hecho (sin voz, como el original) |
| 15 | TEXT_READ | 0x6F8260 → IsTextRead | hecho (con la rama de voz desde B7) |
| 43 | PLAY_SOUND_EFFECT | 0x70F7F0 (dueño = n.º de muestra) | hecho (B6) |
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
| 246 | SPIRIT_SPEAKS | 0x710C40 → ConvertScriptSpiritToHelpSpirit 0x710350, GetSpiritWhoTalks 0x5C6E20 (consulta) | hecho (B7) |
| 285 / 348 | START_ANGLE_SOUND / StartPitchSound | 0x70FFA0 / 0x70FFE0 | stub (C7) |
| 317 | SET_CREATURE_SOUND | 0x710020 | hecho |
| 335 | LAST_MUSIC_LINE | 0x710050 (sin audio: TEXT_READ, 0x7100A6) | hecho |
| 340 | GAME_PLAY_SAY_SOUND_EFFECT | 0x70F9B0 → 0x70F8E0 | hecho (B7) |
| 350 | MUSIC_PLAYED (tipo) | 0x70FBA0 (GAudio+0x28 != tipo; sin audio: TEXT_READ, 0x70FBE4) | hecho |
| 357 | SET_GAME_SOUND | 0x7100B0 | hecho (B6, `audio::SetGameSound`) |
| 409 | SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | hecho (B6) |
| 411 / 412 | GAME_CLEAR_DIALOGUE / GAME_CLOSE_DIALOGUE | 0x6FF6F0 / 0x6FF700 | hecho |
| 424 | STOP_SOUND_EFFECT | 0x70FA50 (isSay: narrador 2 → 0x270C; si no, 0x270E + 0x270D; nunca 0x270F) | hecho (B6; con isSay para las voces de B7) |
| 445 | ENABLE_DISABLE_ALIGNMENT_MUSIC | 0x710120 | hecho |
| 447 / 448 | ATTACH / DETACH_SOUND_TAG | 0x710150 / 0x7101D0 → 0x71E840 / 0x71EBE0 | hecho (B6) |
| 450 | GAME_SOUND_PLAYING | 0x710230 → fn_0042A280 | hecho (B6) |
| 458 | SAY_SOUND_EFFECT_PLAYING | 0x710280 | hecho (B7) |

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
**START_MUSIC 54** (suena intro.sad). Con `MOVE_GAME_THING` (033) de mapas (b17111c6) pasa `FollowUs_loop_4`, la
familia anda y suenan las piedras cantoras (PLAY_SOUND_EFFECT 49/50/54); los bloqueos siguientes son de cámara, sin
dueño: HAS_CAMERA_ARRIVED (035, GCamera::Arrived 0x443050), MOVE_CAMERA_POSITION/FOCUS (003/004) y SET_AVI_SEQUENCE
(203, pantalla en negro tras el SET_FADE). Desde B7 la intro habla: ver [B7 en juego](#b7-en-juego).

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

## Fase B: B0 y B1 implementados

Sesión audio, rama `local/audio` sobre `local/hand-hbn` c945eccd (con el motor de agua ya fusionado).

### B0: arreglos del reproductor

| arreglo | estado | dónde |
|---|---|---|
| Muestra vacía → `continue` | ya lo había hecho agua | `Game.cpp` |
| RIFF 0x50 (MPEG capa II) → dr_mp3 | **hecho**: HelpSprites 1 = 5,2 s; las 209 de InGame decodifican | `WaveBuffers.cpp` (`Decode`) |
| Un búfer AL por muestra, al primer uso (fuera la fuga) | **hecho**, también para los reproductores viejos | `WaveBuffers.cpp` (`Get`), `AudioManager::CreateEmitter`/`CreateBuffer` |
| Bucles finitos N | **hecho**: la fuente hace bucle y cada vuelta del desplazamiento es una pasada; tras N vueltas deja de hacer bucle, así que suenan N+1 pasadas del tramo **(inferido**, pregunta 2 de PLAN §6) | `SampleOutput.h` (`LoopCounter`), `AlSampleOutput::Update` |
| Tramo lStart..lEnd | **hecho** con `AL_SOFT_loop_points` (G_VillageBell 0..27400 de 57855) | `WaveBuffers.cpp`, `Sound::loopStart/loopEnd` (+0x138/+0x13C, `Loaders.cpp`) |
| «Villagers.sad» sin distinguir mayúsculas | **hecho**: el filtro compara bancos (`BankId`), no nombres; `RegisterBank` reconoce los 11 tipos de 0x9CB3F8 por ruta sin mayúsculas | `AudioSystem.cpp` |
| AL_PITCH pisado / oyente por fotograma | ya lo había hecho agua | `AudioPlayer.cpp`, `SamplePlay` |

### B1: el núcleo

Capas (PLAN §2.1):
- **0. Dispositivo**: `AudioPlayer` (contexto OpenAL), `AlSampleOutput` (una fuente por canal, fuera del registro; se
  borran en `ClearMap`), `WaveBuffers` (búferes y decodificadores).
- **1. LHaudio/QMixer**: `QMixerLaws` (`qmixer::Gain` 0x100133C1, `DistanceGain` 0x1802CE50, `PolarRelative`
  0x10012269, `FrequencyRatio` 0x10012820, `StartPitch` 0x1001278B) y `SamplePlay` (16 canales: `Start` 0x100113B0,
  `Stop` 0x10012C50 / 0x10012DF0 con la regla del primer canal, `StopAll` 0x10012BF0 sin los de ambiente, `IsPlaying`
  0x10013ED0 / 0x10013FB0, `ReleaseLoop` 0x10012F20 en el primero, `SetPitch` 0x10013520, `SetVolume` 0x10013400,
  `SetMasterVolume` 0x100150E0, `UpdateChannels` 0x10014310 + oyente, `Switch` 0x10015D40, `ClearInfoList`
  0x100142C0).
- **2. GAudio** (`AudioSystem`): bancos (`RegisterBank`, `Bank(SfxBank)` = GAudio+0x3A8+4·tipo, `FindBank`,
  `SampleId`, `CreatureBank`), filtros de `PlaySoundEffect` 0x429E30 (corte 3D con +0x58, userParam 1/2/4,
  SET_GAME_SOUND sobre GScript+0x90 de `ScriptAudioState`, `OwnerUnavailable` 0x429D20), ciclo de vida. (La rama de
  muestra `PlayAnimEffectSample` de `CollisionSounds` se quitó en B4: la física pasa ya su clave.)
- **3. GameSfx**: las 5 variantes 0x429D60 / 0x429DA0 / 0x42A000 / 0x42A040 / 0x42A100, Stop 0x42A210, ReleaseLoop
  0x42A330 / 0x42A310, IsPlaying 0x42A280 / 0x42A2B0 / 0x42A2D0, SetPitch 0x428740 y los contadores cíclicos
  (`enum class Counter`).
- **4. API pública** `src/Audio/Audio.h`.

API pública (sin argumentos por defecto; cada llamador pasa lo que pasa el original):

| función | original |
|---|---|
| `PlaySoundEffect(PlayOptions)` | 0x429E30 (opciones propias: Guidance, PSysSound, SoundTag) |
| `PlaySoundEffect(owner, sample, mode, loops, flag10, is3D, SfxBank / BankId)` | 0x429D60 / 0x429DA0 (3D sin dueño o con dueño no disponible: nada) |
| `PlaySoundEffectAt(owner, pos, sample, mode, loops, flag10, is3D, SfxBank / BankId)` | 0x42A000 / 0x42A040 (track = is3D) |
| `PlaySoundEffectAt(owner, pos, offset, sample, track, mode, loops, flag10, is3D, BankId)` | 0x42A100 (SoundTag fn_0071E680) |
| `StopSoundEffect(sample, owner, bank)` (sample 0 = todas las del dueño) | 0x42A210 → LHSampleStop |
| `StopAllSoundEffects()` | fn_004287D0 |
| `ReleaseLoop(owner, sample, bank)` | 0x42A330 / 0x42A310 |
| `IsPlaying(owner, sample, bank)`, `IsPlaying(owner, SfxBank)`, `IsPlaying(Channel)` | 0x42A280 / 0x42A2D0, 0x42A2B0, 0x10014070 |
| `SetPitch(bank, owner, sample, percent)`, `SetVolume(Channel, v)` | 0x428740, 0x10013400 |
| `NextCounter(Counter)` | contadores 0xC4CC7C, 0xC5E3E4/E8, 0xC6421C, 0xC64220, 0xD18228, 0xD4437C, 0xD559AC, 0xD95AF8/FC |
| `MaxDistance(Sample)`, `CreatureBank(especie)` | 0x42A430, 0x4EBD81 |
| `RegisterObject(id, fn)` / `UnregisterObject` | Get3DSoundPos (vt +0x10) de un dueño que no es GameThing |
| `Init(GameQueries)`, `Shutdown()` | ctor 0x426D40 (maestro fn_00428250), ToBeDeleted 0x426FE0 |
| `ProcessTurn(cielo, turno)` | GGame::EndTurn 0x54E960: GSoundMap::Update, ProcessSoundTags, y tras el turno 5 ProcessAudioGameTurn 0x427080 (con la puerta LHWaveIsActive) o AtmosProcess(0) |
| `Paused()`, `UpdateFrame()` | EndTurn en pausa (0x54E9B4); el maestro en vivo y los bucles finitos |
| `ClearMap()` | GAudio::Reset 0x426CA0 (+ las SoundTags y farolas del mapa) |
| `OnFocus(bool)` | minimizar / restaurar: 0x7DE6D0 / 0x7DE6F0 → 0x642470 → fn_00428720 → LHGlobalSwitch 0x10015790 |
| `SetSampleMasterVolume(v)`, `SampleMasterVolume()` | 0x100150E0 / 0x10015170 |

Declaradas para hitos posteriores (no definidas): `PlayAnimEffect(owner, key, bank, at, track)` y
`AnimEffectAction` (B2, la rama con clave de 0x42A4B0), `SoundExists()` (B6, SOUND_EXISTS 0x710100),
`tags::Create` (3 formas, 0x71E840 / 0x71E8C0 / 0x71EB60), `SetActive`, `Remove`, `Delete`, `RandomSample` (B3),
`voices::RunTextVoice`, `Say`, `IsSaying`, `StopSay`, `CutByClick` y `advisor::Say`, `IsTalking`, `Stop`, `LipSyncKey`
(B7). La música (A3..A9) tiene sus cabeceras: `MusicEngine.h`, `MusicStream.h`, `GameMusic.h`, `ThingMusic.h`. Los nombres de
agua (`sample_play::Play`, `PlaySoundEffect`, `PlayAnimEffect`, `SetVolume(entt::entity)`, `ProcessTurn`…) siguen como
alias con el número de canal como entidad, para los llamadores de fuera de `src/Audio` hasta B4.

Leído en el desensamblado para B1 (no estaba en los informes):
- 0x42A100 tiene **10** argumentos: el 7.º son las vueltas (+0x4C, 0x42A17A) y el 8.º el +0x10 (0x42A121);
  `engine.md` §1.6 los tenía como uno.
- `GAudio::PlaySoundEffect` devuelve siempre 0 (0x429FE8): ningún llamador recibe el canal.
- `LHSampleStop(bank, dueño, muestra)` con muestra ≠ 0 solo para el **primer** canal que coincide; con muestra 0, todos
  los del dueño; con el audio apagado no para nada salvo canales de ambiente (0x10012C50..0x10012DD1). Antes de parar,
  QMixer hace una rampa de volumen de 20 ms (SetPanRate 20, volumen 0, Sleep(20)) **(aproximado en openblack: para en
  seco)**.
- `LHSampleStopAll` no toca los canales de ambiente (+0x00 ≠ 0, 0x10012C13). `SET_GAME_SOUND false` ya no los para
  (antes sí).
- `LHSampleIsPlaying` y `LHSampleReleaseLoop` miran solo el primer canal que coincide y no hacen nada con el audio
  apagado (0x10013F69, 0x10012F2A). `LHSampleSetPitch` ignora el tono 0.
- `LHSampleClearInfoList` no hace nada con el audio apagado (0x100142CC), así que en `GAudio::Reset`, que lo llama entre
  `LHGlobalSwitch(0)` y `(1)`, no borra nada.
- `LHSampleUpdate3DChannels` para el canal si la distancia es **≥** la máxima (0x100143AB) y, si el dueño no está,
  además pone el dueño del canal a 0 (0x100143A2).
- En `SamplePlayAnimEffect` el filtro del dueño no disponible depende solo de track, sin is3D (0x42A5A1).
- El sitio de `ProcessAudioGameTurn` en `GGame::EndTurn` (0x54E960..0x54E9B4): GSoundMap::Update, Dump,
  ProcessSoundTags y, si no hay pausa (`g_game+0x14 & 4`) y el turno > 5, ProcessAudioGameTurn; si no, AtmosProcess(0).

Cambios de comportamiento audibles (todos por el original):
- `SET_GAME_SOUND` ya no corta el ambiente (0x10012C13) y su bandera vuelve a 0 en cada mapa (GScript::Reset 0x6EB403;
  antes era una bandera aparte que no se reiniciaba).
- `Stop`/`ReleaseLoop`/`SetPitch` actúan en el primer canal que coincide (antes en todos).
- Con el filtro de banco por `BankId`, los aldeanos ya no se silencian tras `SET_GAME_SOUND false`.
- Minimizar la ventana para los efectos y la música (perder el foco no).
- Los bucles finitos acaban (campana ×5, ranas, pájaros, palomas) y los bucles con tramo repiten solo el tramo en los
  16 canales. (Los reproductores viejos de `AudioManager::CreateEmitter` ponían el búfer en cola y OpenAL Soft solo
  usa los puntos de bucle en fuentes estáticas; desde B5 no queda ninguno.)

Auditoría de B0-B1 (§1.7 de TEAM_GUIDELINES, sesión audio):
- Comprobado en el desensamblado: 0x429D20, 0x429D60/0x429DA0, 0x42A040, 0x42A100 (orden de los 10 argumentos),
  0x429E30 (orden de los filtros y el `<=` del corte 3D), 0x42A4B0, 0x426E6B (800), 0x54E960, 0x427080, 0x426CA0,
  0x427200, 0x4068F4, 0x63AA39, 0x5D2109 (contadores), 0x7DBFF6 / 0x7DE8DC / 0x7DE6D0 / 0x642470 (minimizar) y en
  LHaudiodllR 0x10011020 (asignación), 0x10012BF0, 0x10012C50, 0x10012F20, 0x10013400, 0x10013520, 0x10013AC0,
  0x10013ED0 / 0x10013FB0, 0x10014070, 0x100142C0, 0x10014310, 0x100146F0, 0x100150E0, 0x10015D40, 0x10001EBF.
- Corregido: `LHSampleSetVolume` y `LHSampleSetPitch` no hacen nada con el audio apagado salvo en canales de ambiente
  (0x10013412, 0x10013572), y `SetVolume` actúa sobre el primer canal de su (banco, dueño, muestra) (0x10013489).
  `LHAtmosProcess(0)` para cada canal guardado sin preguntar si suena (0x10001ED3); `atmos_banks::StopChannel` igual.
- Corregido (fuga): `audio::ClearMap` no paraba los emisores viejos (`AudioManager::CreateEmitter`: árboles,
  AnimationSounds, fuego, hechizos…); el reinicio del registro del mapa nuevo los borraba con su fuente AL sonando (uno
  en bucle, para siempre) y al salir quedaban «7 Sources not deleted» y «Deleting in-use buffer». En el original son
  canales y `LHSampleStopAll` 0x426CE6 los para: ahora `ClearMap` llama a `AudioManagerInterface::DestroyAllEmitters`
  (todos menos el de la música vieja). Traza: `Sample play: N channel sources released` y `AudioManager: N emitters
  destroyed` con `OPENBLACK_AUDIO_TRACE`.
- Marcado **(aproximado)**: `LHSampleIsPlaying(info)` da 0 con el audio apagado (0x1001407A); openblack mira el
  arranque del asa también apagado, porque su juego sigue corriendo minimizado y PSysSound (0x6D120A) relanzaría cada
  turno. `LHSampleSet3DPosition` también busca el primer canal de la terna y respeta los ejes fijos del canal (+0x14
  bits 4/8/0x10, 0x10013BCC): openblack mueve el canal del asa y no tiene ejes fijos.
- La distancia de `SamplePlayAnimEffect` la calcula el llamador (`|LH3DTech::g_camera − pos|`, physics/collision_sounds.md).
- La música y el ambiente se procesan en el orden de `ProcessAudioGameTurn` (antes la música iba al principio del turno).
- `audio::GetSurfaceType` llama a `ecs::sea_cells::GetSurfaceType` (una sola fuente).

## Fase B: B2 y B3 implementados

Sesión audio, rama `local/audio` (sobre B0-B1 2767ffd3 / ca200e26).

### B2: los anim-effects en el núcleo

| pieza | original | dónde |
|---|---|---|
| Tablas de cada banco, leídas **una vez** al registrarlo | `LHBankRegister` 0x10002778..0x100029AB (`LHFileSegmentAnimArray` → banco +0x124 / +0x12C ancho / +0x130 filas; `LHAudioWaveNumTable`) | `AnimEffects.cpp` `anim_effects::RegisterTables` desde el bucle de bancos de `Game.cpp` |
| `AnimEffectTable` (la `AnimEffectBank` de Milagros, sin cambios: mismos miembros, `Load`, `FindList`, `SoundId`, `FindSample`) | `LHFindAttribRow` 0x10014420 (fila con más columnas exactas; empate, la última, fn_10014610) | `AnimEffects.h`; `AnimEffectBank.h` queda como alias |
| `anim_effects::Number` | `LHSampleGetAnimEffectNumber` 0x10014670: lista de 1 → esa muestra; si no, `list[LH_AudioSystem::Rand(n)]` (0x10015710) | `AnimEffects.cpp`, `sample_play::Random` |
| `anim_effects::Play` | `LHSamplePlayAnimEffect(dueño, dist, n, track, banco, min, max)` 0x10014A20: nada con el audio apagado; la distancia **del llamador** ≤ 800 (+0x44, `LHSampleRegister3DObjectFunction` 0x426E6B) y ≤ el maxDist de la muestra (.sad +0x26C, 0x10014ABD); is3D 1, +0x10 0, min/max con sus bits 0x80/0x100 solo si > 0; el punto lo da la función 3D del juego para el dueño (fn_00427200: sin dueño, la cámara; dueño no disponible o el de ambiente, nada) | `AnimEffects.cpp`, `audio::Get3DSoundPos` |
| `anim_effects::PlayKey` | 0x100146F0: mismas puertas (800 también para parar); acción 1 = `LHSampleStop` del primer canal de (banco, dueño, muestra) de cada muestra de la lista (0x1001491C); otra = `LHSampleReleaseLoop` (0x10014990) | `AnimEffects.cpp` |
| `audio::SamplePlayAnimEffect(dueño, dist, clave, acción, banco, track, min, max)` | GAudio 0x42A4B0: acción ≠ 0 va directa a 0x100146F0 sin filtros ni min/max (0x42A4BC); acción 0: número (0x42A4F9; 0 = nada), filtros del userParam (1 con la pantalla ancha del guion, solo 2 en la ciudadela, bancos tras SET_GAME_SOUND, 4 en los estados 0x10/0x16/0x17), dueño no disponible si track (0x42A5A1), y 0x10014A20 | `Audio.h`, `AudioSystem.cpp` |
| `AnimationSounds::Fire` (firma intacta) | fn_00516510: los eventos con from ≤ t < to; sin `Get3DSoundPos` nada; la distancia y la superficie son **las del aldeano** (0x51655F, `GSoundMap::GetSurfaceType` 0x51662B → `ecs::sea_cells::GetSurfaceType`); grupo 1: muerto → nada más (0x5165BC), voz 3 niño / 1 + (+0x1F8 ≠ 0); 0x92 → banter con la casa como dueño (`GetAbode` 0x51675D; sin casa, dueño 0 = la cámara); 0x93/0x94 → banter en el aldeano; paso (4) de un aldeano con la pantalla ancha del guion → **se descarta el resto de la lista** (0x5166A4 salta al final); THROWN 399/401 solo con < 15 / < 10 turnos; siempre track 1, min/max 0 (0x5167A8) | `AnimationSounds.cpp` |
| `AnimationSounds::PlayFromTable` (firma intacta, árboles) | Tree::Draw 0x74B009 (doblar, track 0) / 0x74B25C (susurro, track 1), banco editor (GAudio+0x3B0) | `AnimationSounds.cpp` |

`AnimEffectBank` (de Milagros) sigue como alias de `AnimEffectTable` para sus tests; su `Load(ruta)` copia las tablas
que el núcleo leyó al registrar spells.sad (`test_anim_effects` `MiraclesBankCopiesTheCoresTables`: mismas filas, listas y
muestras). Desde B5 `SpellSounds` ya no la usa: toca por `audio::SamplePlayAnimEffect`.

Cambios audibles (todos por el original):
- Los sonidos de los clips y de los árboles van por los 16 canales (prioridad, modo, robo) y siguen a su dueño una vez por
  turno (`LHSampleUpdate3DChannels`), no cada fotograma.
- Los filtros de GAudio: en la intro de Land 1 (la pantalla ancha del guion dura hasta un clic) no suenan las muestras
  de userParam 1: los susurros de los árboles 307..319 y **todo** VillagersBanter.sad; los creaks 320/321 sí. Los pasos
  de los aldeanos tampoco.
- El banter 0x92 se oye en la casa pero se corta con la distancia del aldeano: con la cámara a 4 m de él ahora suena
  (antes medía desde la casa y casi nunca). Sin casa suena en la cámara (antes nada).
- Parar (la sierra) para el primer canal de la terna (antes todos los emisores del dueño).

Traza `OPENBLACK_ANIM_TRACE` antes/después (Land 1; cámara a 4 m del aldeano 0 en el turno 100; clips forzados
437 bostezo, 354 sierra, 369 sentado; milagro WATER; el «después» con `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` para quitar
el filtro de la intro): mismas líneas y mismos casos — filas de árboles con las 15 muestras 307..321, pasos 259..268 en
superficies 2/3, sierra 229/230 en 2/3, `banter N too far (D > 8)` y las 4 líneas del PSys del agua iguales (spells.sad/51).
Diferencia: el bostezo da 4 banter que suenan (`VillagersBanter.sad/N`) y antes 0, por la distancia del aldeano. Sin el
gancho (fiel) solo suenan los creaks 320/321. Los rechazos del núcleo (800, maxDist) van a `OPENBLACK_AUDIO_TRACE`.

### B3: SoundTag completo

`src/Audio/SoundTags.{h,cpp}` (SoundTag.cpp 0x71E300..0x71ED90, volcado `tmp_dis\mapa\d_soundtag.txt`), API `audio::tags`:

| función | original |
|---|---|
| `Create(cosa, muestra, track, modo, vueltas, flag10, is3D, SfxBank, retardo)` | 0x71E840 (desplazamiento 0) |
| `Create(cosa, desplazamiento, …)` | fn_0071E8C0 → ctor 0x71E300: el punto es el MapCoords de la cosa (x, GetAltitude + y, z) |
| `Create(punto, …)` | fn_0071EA40: suena **ya** por 0x429E30 (dueño el tag, track 0, modo +0x50, sin +0x10) salvo 3D con retardo |
| `CreateAtMapCoords(x, z, altura, …)` | 0x71EB60 (`GameQueries::landAltitude` = LH3DIsland::GetAltitude 0x803090) |
| `SetActive(tag, b)` | fn_0071E640: un tag activo que se apaga corta su muestra (0x42A210) |
| `Remove(cosa, muestra, SfxBank)`, `Remove(…, stop)` | 0x71EBE0 / 0x71EC30: cada tag de fn_0071ED60 (cosa, muestra, tipo) → ToBeDeleted; con stop, 0x42A210 antes |
| `Delete(tag)` | ToBeDeleted 0x71ECB0 → CreateSoundTagForDeadObject 0x71ECD0: olvida la cosa; si la muestra suena con vueltas (fn_0042A460 +0x40 y fn_0042A2D0) suelta el bucle (0x42A310) y el tag vive hasta que acabe; si no, se borra (lo que suene sigue) |
| `RandomSample(primero, n)` | 0x71ED40 |
| (interno) `ProcessSoundTags` | 0x71E5F0 desde `GGame::EndTurn` 0x54E989, del más nuevo al más viejo; fn_0071E680: con cosa, no funcional / sin punto → ToBeDeleted, inactivo → nada, si no 0x42A100 (punto, +0x1C, muestra, +0x30, modo, vueltas, +0x40, is3D, banco) cada turno; sin cosa: el retardo (`CheckDelay` 0x71E760: suena cuando 347 ([0x980530]) · turnos · [0xD01A38] ms ≥ la distancia, si está dentro del maxDist y activo; el retardo se acaba igual) o se borra cuando su muestra para |
| (interno) `Get3DSoundPos` | 0x71EC90: la de su cosa; sin cosa responde 1 sin escribir, así que el canal conserva su punto (`SamplePlay::UpdateChannels`) |

`SoundTag::Set` 0x71E4F0: activo 1, track solo con cosa (0x71E55D), retardo solo si is3D (0x71E56B), +0x34 = 0 en todas
las formas portadas (banco = GAudio+0x3A8 + 4·tipo, GetBank 0x71E610). Hay una cuarta forma **sin portar**: fn_0071E920
(ctor fn_0071E460, +0x34 = 1 → banco de ambiente GAudio+0x194 + 4·tipo), un tag de punto que suena ya por 0x429E30 salvo
3D con retardo; su único llamador es el trueno de `GWeather::Update` 0x83FC62 por el callback [0xEEA388] = 0x429CE0
(muestra 2 + GetTickCount() % 11, modo 2, 3D, tipo 12, retardo 1). Queda pendiente con el trueno. `CreateAtMapCoords`
recibe x/z en unidades del mundo: el MapCoords del original los guarda enteros y los escala por 1/6553,6 ([0x8AA3A4],
0x71EB8A/0x71EBA6). Los nombres de agua (`sound_tags::Create(TagDesc)`, la cascada de
`DesignedScenery`) siguen encima: un `TagDesc` sin cosa es el ScriptMarker de la cascada (un GameThingWithPos que no se
va), que se repite como un tag de cosa.

`LanternSounds` es ahora una farola con su tag (API intacta: `SetOn`, `ProcessTurn`, `Clear`):
`CallVirtualFunctionsForCreation` 0x734810 → fn_0071E8C0(farola, (0, Object::GetHeight, 0), 0x93, 0, 2, −1, 0, 1, InGame, 0)
y `SetActive([0xDA0A10])` 0x734965; `SetOn` = fn_007349E0 (SetActive de cada farola al cambiar). Una farola que se va
deja su tag como el de un objeto muerto (suelta el bucle). Suena igual que antes: en la punta, al acercarse a menos de
5 (0x429E30), en bucle, y no se corta al alejarse (canal sin track); ahora cuenta en los 16 canales y respeta los filtros
de GAudio.

Tests: `test_sound_tags` (10: modo 2 solo re-dispara si no suena, modo 3 reinicia, fuera de rango e inactivo, la cosa que
se va suelta el bucle y el tag espera a que acabe, una sin vueltas se va ya, punto una vez, retardo a 347/s, Remove con y
sin stop y por tipo de banco, farola en su punta) y `test_anim_effects` (7). En juego (`OPENBLACK_TIME_OF_DAY=22`,
`OPENBLACK_AUDIO_TEST_LANTERN=100`): 12 farolas con tag, `G_Lantern_01.wav mode 2 owner sound tag`, en bucle, con la
cámara a 3,2 de la punta.

## Fase B: B4 y B6 implementados

Sesión audio, rama `local/audio` (sobre B2-B3 8af89ed7 y la fusión de `local/hand-hbn` 056ad80f).

### B4: los llamadores del mundo en los canales

Cada sitio va por `audio::` con lo que pasa su llamada original (`sfx_inventory_tables.md`, comprobado en el
desensamblado). Ya nadie fuera de `src/Audio` llama a `AudioManager::PlaySound`/`CreateEmitter`/`PlayAt` salvo los
archivos de Milagros de B5 (lista [arriba](#estado-del-motor-de-efectos-en-openblack)). `Game.cpp` (bancos y
`AudioManager::Update`) y `Locator` siguen usando `AudioManager` para el ciclo de vida, no para sonar.

| sitio openblack | original | qué hace ahora |
|---|---|---|
| `HandHolding` `PickUp` | `GInterface::GenericPickup` 0x5D2800 (0x5D2881..0x5D295D) | `tags::Create(punto del objeto, 10 G_PickUpObject, 0, 3, 0, 0, 3D, InGame, 0)` (0x71EB60) salvo un árbol arraigado; un aldeano vivo (`Object::IsAlive` 0x402610) grita con otro tag igual: niño 180, mujer 194, hombre 187 + `GetRandomSample(7)`. Un `DeadTree` también suena (no tiene `IsTree`: `GameThingWithPos::IsTree` 0x402320 = 0) |
| `HandHolding` (arrancar) | `Tree::InterfaceSetInMagicHand` 0x74B730 | `tags::Create(punto del árbol, 32 + GetRandomSample(3), 0, 3, 0, 0, 3D, InGame, 0)` si no tiene +0x24 & 0x40 |
| `HandPhysics` (soltar árbol) | `Tree::DropSfx` 0x74BC60 | opciones: InGame, dueño el árbol, 3D, track 0, 83 + GetTickCount() % 3, en su punto |
| `HandResources` (madera al almacén) | `Object::DoDeleteObjectAndTakeResource` 0x63AA93 | opciones: InGame, dueño el objeto, 3D, track 0, 155 + `Counter::TreeMulch` ((c + 1) & 3) |
| `HandFish` (agarrar tierra) | fn_005D1AB0 0x5D1FE4 | `tags::Create(punto, 4 + GetRandomSample(6), 0, 3, 0, 0, 2D, InGame, 0)`: el tag es el dueño (canal nuevo en cada agarre) |
| `HandFish` (mano en el agua) | fn_005D1AB0 0x5D2167 | opciones: 99 + `Counter::HandInWater`, 3D, track 0, en (x, 0,2, z) |
| `HandEffects` (multirrecogida) | fn_0068F930 0x68F9E8 / 0x68FA25, StopMultiPickup 0x68FABA / 0x68FAD4 | `PlaySoundEffect` 44/98 3D track 0 en la mano cada turno, `SetPitch(InGame, 0, n, 60 + 180 t)`, `StopSoundEffect(44/98, 0, InGame)` |
| `hand_detail::PlaySample(id)` | `FailApply` fn_005D18F0 0x5D1941 | `PlaySoundEffect(NULL, n, 3, 0, 0, 0, banco)` 2D (lo usa `HandSpellSeed`) |
| `hand_detail::PlaySample3D` | forma de opciones 3D track 0 | sin llamadores; se conserva la firma |
| `Trees.cpp` `PlayAt` (crecer) | `Tree::ApplyWaterSpell` 0x74C500 | opciones 3D, track 0, en el árbol, por los canales; **(aproximado)**: la firma no lleva el árbol y el canal no tiene dueño (el original +0x20 = el árbol, 0x74C4D4) |
| `Rocks::Tap` | `Rock::InterfaceTap` 0x6E751D | opciones: InGame, 130 + `Counter::RockTap`, dueño la roca, 3D, track 0, en el punto de la mano (estado +0xC8) |
| `DefaultWorldCameraModel` | `CameraModeNew3::FlyToPosFoc` 0x45899B, `Update` 0x45E305 | `PlaySoundEffect(0, 46 + (GetTickCount() & 3), 3, 0, 0, 2D, InGame)`. `SetFlight` (marcadores, vuelos del guion y de los ganchos) solo si la cámara está a más de [0x9CE640]·1,5 = 150 del destino (0x458967); el doble clic (`Update`) suena con su propia marca, sin esa prueba |
| `PhysicsObjects` (G_RockPast) | `PhysicsObject::GameTurnUpdate` 0x645C12 | `PlaySoundEffect(0, 69 + GetTickCount() % 5, 2, 0, 0, 2D, InGame)` |
| `CollisionSounds::AttemptToAddSoundEvent` | 0x646919 | `SamplePlayAnimEffect(objeto, |g_camera − punto|, {nivel, 0, A, B, 75}, 0, editor.sad, track = A ≠ 0x16, 0, 0)`: la fila la elige la tabla del banco (`LHSampleGetAnimEffectNumber`), ya no la tabla resuelta de 99 filas; el canal es del objeto y suena en su punto |
| `Buildings.cpp` | `Abode::ReactToPhysicsImpact` 0x406610, `ApplyEffectsDueToPhysicalDestruction` 0x40671D | `CollisionSounds::PlayAnimEffect` con dueño el edificio: golpe {2/3, 0, 0x16, 0x10, 75} (p > 1000 / > 300), derrumbe {1, 0, 0x16, 9, 75}, track 0 |
| `PetitNavire` | `PostDraw` 0x5E0465..0x5E04B9 | opciones: ScriptSfx 62/61/60, 2D, modo 2 |
| `PotResource::PlayPileSound` | fn_0066D1A0 0x66D26A | opciones: InGame, dueño el montón, 3D, track 0, muestra por GetTickCount (firma con el montón) |
| `DesignedScenery` (cascada) | 0x5E3921 / 0x5E3BD5 | sin cambios: `sound_tags::Create(TagDesc)` de agua ya es un `tags::Create` del núcleo (el marcador del original se emula con un tag de punto que se repite como uno de cosa) |

`audio::TickCount()` es el `GetTickCount` del original (ms del reloj del proceso) para los que eligen muestra con él.

Cambios audibles (todos por el original): coger, arrancar, plantar, triturar y golpear rocas suenan **en 3D** donde
pasa (antes 2D y, salvo agua y vasijas, fuera de los canales); al coger un aldeano grita; un árbol muerto suena al
cogerlo; triturar usa el contador y no el azar; el woosh de los marcadores ya no suena en saltos cortos (≤ 150); los
choques eligen su muestra con la tabla del banco y siguen al objeto; todo cuenta en los 16 canales y respeta los
filtros de GAudio (p. ej. G_RockPast, TreeFall, KnockRoof, HandThroughInfluence son userParam 1: callan con la pantalla
ancha del guion).

### B6: CHL de efectos

`src/Audio/ScriptSound.{h,cpp}` (`audio::script_sound`), llamado desde `CHLApi.cpp` con los POP del original:

| CHL | original | openblack |
|---|---|---|
| PLAY_SOUND_EFFECT(sample, bank, pos, withPos) | 0x70F7F0: opciones por defecto, banco GAudio+0x3A8 + 4·bank, muestra, **dueño = la muestra** (`Owner::Key`), is3D = withPos, track 0, punto, +0x164 = 1 | `PlaySoundEffect` |
| STOP_SOUND_EFFECT(isSay, id, bank) | 0x70FA50: sin isSay, `StopPlayingSoundEffect(id, id, bank)`; con isSay, la voz del texto (tabla 0x942B38) en 0x270C (narrador 2) o en 0x270E y 0x270D; 0x270F nunca | `StopSoundEffect` |
| GAME_SOUND_PLAYING(sample, bank) | 0x710230 → fn_0042A280(sample, sample, bank) | `GameSoundPlaying` |
| ATTACH_SOUND_TAG(threeD, sample, bank, obj) | 0x710150 → `SoundTag::Create(cosa, sample, track = threeD ≠ 0, 2, 0, 0, threeD, bank, 0)` 0x71E840 | `AttachSoundTag` |
| DETACH_SOUND_TAG(sample, bank, obj) | 0x7101D0 → `SoundTag::Remove(cosa, sample, bank)` 0x71EBE0 | `DetachSoundTag` |
| SOUND_EXISTS | 0x710100 → GAudio::IsInstalled 0x426D30 → LHWaveIsInstalled | `audio::SoundExists()`: el audio está en un dispositivo OpenAL (no en `AudioManagerNoOp`) **(aproximado)** |
| SET_GAME_SOUND(on) | 0x7100B0 | `audio::SetGameSound` (antes el nombre de agua) |

El objeto de ATTACH/DETACH lo da `MusicThing` (GetScriptGameThing 0x70D220, aproximado igual que la música). Un banco
fuera de 1..10 no suena ni para **(aproximado**: el original indexa GAudio+0x3A8 sin comprobar).

Test `test_script_sound` (10): PLAY(93, 5, p, 1) → canal 3D con dueño `Key(93)` sin track; GAME_SOUND_PLAYING true y
false tras STOP(0, 93, 5); 2D sin corte por distancia; 3D más allá de su máximo no arranca; banco 11 → nada;
SET_GAME_SOUND(false) deja solo HelpSprites/Villagers; ATTACH(1, 126, 5, cosa) → tag modo 2 dueño del canal, 3D con
track, no re-dispara mientras suena, DETACH suelta el bucle y el tag se va al acabar; ATTACH 2D sin track; sin cosa,
nada; SOUND_EXISTS falso sin dispositivo.

En juego (Land 1, 2026-10-01, `OPENBLACK_SFX_TRACE=1`, logs `_audit\audio\b4_run1.log` / `b4_run2.log`): el guion
`FollowUs` toca las piedras cantoras (`PLAY_SOUND_EFFECT(49/50/54, 5, punto, 1)`: Stone1/2/6 3D, dueño la muestra);
lejos se cortan por su máximo (100) y con la cámara al lado (`OPENBLACK_CAMERA_LOCK`) suenan en un canal. También
salen el woosh del vuelo de `OPENBLACK_CAMERA_FLY` (2D), los golpes de roca (3D con la roca de dueño), el agarre de
agua y la recogida (tags), los choques del árbol lanzado ({1/2, 0, 20, 9/12/16, 75} → G_Crash_Tree_*) y el triturado.

## Fase B: B7 implementado (voces en canal)

Sesión audio, rama `local/audio` (sobre `local/hand-hbn` fe72a3e9). Fuentes: `voices.md` §2.3-2.6, `script.md` §2.3 y el
desensamblado de 0x5BB060..0x5BB8A7, 0x5BCD00, 0x5C36D0..0x5C3842, 0x5C52C0/0x5C5290, 0x5C6020..0x5C60DB,
0x5C6A7E..0x5C6AB4, 0x5C6E20, 0x70F8E0, 0x70F9B0, 0x710280, 0x710350, 0x710C40, 0x428850..0x428EE1 (CalcKey,
fn_00428A80, Analyse, four1), el init de HelpDude 0x5C1EA1..0x5C1F24 y, en el DLL, 0x10012BF0..0x10012F13 (LHSampleStop),
0x10014C00 (LHSampleGetPlayPosition) y 0x10015180 (LHSampleGetPercentageDone).

### API (`src/Audio/Voices.h`, `src/Audio/Advisor.h`)

| función | original | qué hace |
|---|---|---|
| `voices::RunTextVoice(narrador, voz)` | fn_005C5F90 0x5C6025..0x5C60DB | HelpSprites con narrador 2/3: `advisor::Stop(1)`, `Stop(0)` y `advisor::Say(0/1, muestra, 0)`; cualquier otra voz con banco y muestra: opciones por defecto, dueño 0x270F, +0x164 = 1, 2D, por `GAudio::PlaySoundEffect` (con sus filtros) |
| `voices::Say(texto, conPos, alt, punto)` | SaySoundEffect 0x70F8E0 | tabla 0x942B38; sin muestra, nada; dueño alt ? 0x270D : 0x270F; 3D si conPos (el punto solo entonces); track 0; +0x164 = 1. Sin texto ni consejero, aunque sea HelpSprites |
| `voices::IsSaying(alt, texto)` | 0x710280 | fn_0042A280(alt ? 0x270D : 0x270F, muestra, banco) |
| `voices::CutByClick()` | 0x5C6AA4..0x5C6AAD | `StopPlayingSoundEffect(0, 0x270F, VILLAGERS)`: toda la narración de villagers, con la rampa de 20 ms |
| `voices::BankRegistered(banco)` | fn_005C62F0 0x5C631E | GAudio+0x3A8 + 4·banco ≠ 0 |
| `advisor::Init(banco)` | fn_005C3660 → fn_005BB060 | el banco HelpSprites y su número de muestras en los dos consejeros |
| `advisor::Say(dude, muestra, soloSiCalla)` | HelpDudeControl::Say 0x5C36D0 | retardo = v < 0 ? 0 : min((v + 1)·250, 500) ms con v = \|+0x3514\| − 0,95 (0x915438); SaySentence; +0x74 = 1 |
| `advisor::SaySentence(dude, muestra, soloSiCalla, retardo)` | HelpDude::SaySentence 0x5BB340 | g_speaker = dude; con frase sonando, nada si soloSiCalla y habla, si no StopSentence; muestra en 1..n; arranque a GetTickCount + retardo |
| `advisor::Update(dt)` | bucle de HelpDudeControl 0x5C3B05 → Update1 0x5BDDA0 | por fotograma: UpdateSaySentence 0x5BB610 (PlaySample 0x5BB530: `LHSamplePlay` directo, dueño 0x270C, sin filtros de GAudio; copia el PCM) y ApplyLipSync 0x5BCD00 |
| `advisor::IsTalking`, `TalkingOrJustStopped`, `PercentageDone`, `StopSentence`, `Stop` | 0x5BB760, 0x5BB730 (+200 ms), 0x5BB7C0, 0x5BB840, fn_005C3750 | como el original, con g_speaker 0xD15AA0 y g_sentence 0xD15A9C globales |
| `advisor::Interrupt(dude, arg)` | fn_005C3780 (desde fn_005C6720 → fn_005C4C20) | elige un HELP_TEXT_INTERRUPTION_* (0xE3F/0xE4C + rand 5, o 0xE44/0xE51 + rand 8 con arg 0 y LocalRand(2) = 0), Stop(dude) y, fuera de la ciudadela, si PercentageDone < 0,9 (0x915440), lo dice |
| `advisor::AnyTalking()` | 0x5C6372..0x5C63A0 | +0x74 && fn_005BB730(0) \|\| +0x78 && fn_005BB730(1): la rama de consejero de IsTextRead |
| `advisor::LipSyncKey`, `Amplitude`, `CalcKey`, `Analyse`, `Four1`, `BandLevel` | +0x2F60, fn_005BB420, 0x428850, 0x428C60, 0x428D50, fn_00428A80 | la boca, sin dibujarla (abajo) |
| `help::SpiritWhoTalks`, `help::ConvertScriptSpiritToHelpSpirit` | 0x5C6E20, 0x710350 | SPIRIT_SPEAKS |
| `sample_play::PlayPosition`, `PercentageDone`; `SampleOutput::StopRamped`, `PlayPositionMs` | 0x10014C00, 0x10015180, 0x10012C50 | posición en ms, fracción hecha, la rampa de LHSampleStop |
| `audio::SetBankSampleCount` / `BankSampleCount` | LHBankGetNumberOfSamples | el tope de SaySentence |

### Cómo va

- **RUN_TEXT** (HelpSystem::SayText): el gancho `sayVoice` llama a `RunTextVoice` con el narrador del texto.
  **TEXT_READ** con voz: consejero → leído cuando ninguno habla ni ha callado hace menos de 200 ms; narración → +450 ms
  tras la última vez que sonó (la lógica ya estaba en A11; ahora las consultas están conectadas).
- **Clic** (`ProcessInterface`, con la pantalla ancha del guion o la tecla): `spiritStop(1, 1)`, `spiritStop(2, 1)`
  (`advisor::Interrupt`) y luego `CutByClick`. Solo se corta villagers; la narración genérica de HelpSprites y Guidance
  sigue. Como en W120, la frase de interrupción **nunca se dice**: fn_005C3780 lee PercentageDone después de Stop, que
  ya no deja hablante (da 1).
- **Consejeros**: un solo hablante (g_speaker). Un SaySentence sin soloSiCalla con una frase sonando la para y deja la
  nueva **sin hablante** (g_speaker se pone en 0x5BB343 antes del StopSentence de 0x5BB377, que lo borra en 0x5BB89F):
  es del original, y el camino de RUN_TEXT no pasa por ahí (para a los dos antes).
- **Lip-sync**: ApplyLipSync toma el tiempo de `LHSampleGetPlayPosition` (ms) y llama a CalcKey(dt, t) sobre el PCM
  (ventana 0x200): Analyse (ventana triangular de paso 1/(n·32768), four1 de Numerical Recipes, módulo
  √((re²+im²)/n)) y las tres bandas de AutoVoiceParams (200-400 Hz ×0,85, 400-700 ×1,1, 700-10000 ×10; umbral 0,05,
  nivel 2,5, ritmo 40·dt·0,18 hacia abajo y el doble hacia arriba, normalizadas si suman más de 1). Si la frase ya no
  suena y t > 0,5 s, StopSentence. No se dibujan la pose (fn_005BF810), la boca (fn_005BCBC0), los AudioTag de la
  onda (BuildAudioTags 0x42AE70) ni los gestos: no hay modelo de consejero.
- **Bancos de diálogo perezosos**: HelpSprites, villagers, VillagersBanter, SpellDialogue y Guidance se registran solo
  con sus cabeceras (`PackFile::ReadAudioHeaders`) y cada onda se lee del .sad al decodificarla, como
  `LHBankRegister(path, 0)` (unos 107 MB que ya no se leen al arrancar).
- **LHSampleStop con rampa**: todas las paradas por muestra, por dueño o por canal (y los cortes de
  `LHSampleUpdate3DChannels`) bajan a 0 en 20 ms y esperan esos 20 ms, como el DLL. `StopAll` y los reinicios, en seco.
- **CHL**: GAME_PLAY_SAY_SOUND_EFFECT (340), SAY_SOUND_EFFECT_PLAYING (458) y SPIRIT_SPEAKS (246) hechos.
  STOP_SOUND_EFFECT con isSay ya estaba (B6, `script_sound::StopSoundEffect`).

### B7 en juego

Land 1, 2026-10-01 (`OPENBLACK_TEXT_TRACE`, `OPENBLACK_AUDIO_TRACE`, `OPENBLACK_SFX_TRACE`; logs
`_audit\audio\b7_run*.log`). La intro `FollowUs` habla y avanza: las doce voces de la familia
(`HELP_TEXT_DEFINITELY_NEWEST_INTRO_01..12`, villagers 57..68, 2D, dueño 0x270F o 0x270D) con sus esperas
SAY_SOUND_EFFECT_PLAYING; luego los RUN_TEXT con voz «¡Has salvado a nuestro hijo!», «¡Gracias! ¡Gracias por tu
misericordia!» (mujer, villagers 119/120), «¡Te alabamos!» (con clic) y los consejeros «Saludos.», «Somos tu
conciencia.», «Bueno.», «Y malo.», «Yin y Yang.», «Blanco y Negro.», «Como parte de ti, te guiaremos por este mundo.»
(HelpSprites, dueño 0x270C); después «Nuestra gente te rendirá culto.», «Ten la bondad de acompañarnos a nuestro
Pueblo.» y «Te enseñaré cómo seguirlos.». Los textos con clic se pasan con `OPENBLACK_TEST_TEXT_CLICK=1` (el clic, con
la pantalla ancha, corta la voz de villagers, como el original). Se para en **HAS_CAMERA_ARRIVED (035)**: tras
GAME_CLEAR_DIALOGUE, FollowUs hace `RUN Drag`, y Drag mueve la cámara (MOVE_CAMERA_POSITION/FOCUS 003/004, sin
implementar) y espera `wait until HAS_CAMERA_ARRIVED` (un stub que da false). Es la cámara, sin dueño.

### (Aproximado), (inferido) y pendiente de B7

- **(aproximado)** el retardo del consejero es siempre 0: +0x3514 es del vuelo del consejero (no portado) y se queda en
  el 0 del init (0x5C1A61).
- **(aproximado)** la rampa de 20 ms de QMixer en cuatro pasos de 5 ms de ganancia; el juego espera los 20 ms.
- **(aproximado)** el PCM del consejero se decodifica otra vez (el DLL lo convierte con ACM al arrancar con +0x164);
  four1/Analyse/CalcKey con los registros x87 en double.
- **(aproximado)** solo los bancos de Dialogue son perezosos; el resto se lee entero (el original los registra todos
  así).
- **(inferido)** el espíritu 1 (HelpSystem+0xC) es el consejero 0 (fn_005C5250: +0x54 ≠ 1); en SPIRIT_SPEAKS, el
  jugador local es PLAYER_ONE; LocalRand con el generador de openblack.
- **Pendiente**: la parte visual de los consejeros (modelos MarkGood/MarkEvil.Hd, vuelo, `SpiritHome`, boca, gestos
  por AudioTag, `HelpDude::PlaySoundFX` 0x5C2800); el guion de ayuda «MultiHelpJustTalkWithText» (Guidance, B9);
  GConfirmation (C7); GSpookyVoices (B10).

## Fase B: B5 implementado (los milagros en canal)

Sesión audio, rama `local/audio` (sobre B7 16f1d791). Archivos de Milagros tocados con su lógica intacta (el diff para su
revisión está en `dev\_scratch\audio\b5_milagros.diff`). Desensamblado: 0x6745D0 (`AtomCore::StartSound`), 0x6D0F70..0x6D13A0
(`PSysSound`: ctor, destructor, `Get3DSoundPos` 0x6D1000, el bucle fn_006D11A0), 0x72EBE0 (`FireEffect::ToBeDeleted`),
0x72EDC0/0x72EDE0/0x72EE70 (fuego), 0x730760 (`ProcessList`), 0x5D2730/0x5D27B0 (bucle del gesto), 0x5CEC50
(`GInterface::Get3DSoundPos`), 0x6882F0/0x688560..0x68866A (gesto reconocido), 0x683184..0x68327F (bola de fuego que
pasa), 0x68CE90 / 0x68DE69 (`PHandFX`), 0x72A640 (`OneOffSpellSeed::InterfaceTap`), 0x5EC340 (`Living::MoveByTeleport`),
0x726490 (`PlayTapSound`), 0x77F4E0 (`PlayFullyChargedSoundFX`), 0x729C40, 0x7314E0/0x731AB0 (vapor) y, en el DLL,
0x10014010 (`LHSampleIsPlaying(banco, dueño, &info)`).

### API nueva (`Audio.h`)

| función | original | uso |
|---|---|---|
| `NewObjectId()` | (openblack) el original compara punteros de dueño | un número para `Owner::Object` de cada PSysSound, FireEffect, PHandFX, datos del gesto, FireGraphic |
| `PlayingChannel(dueño, banco)` | `LHSampleIsPlaying(banco, dueño, LH_SampleInfo**)` 0x10014010: el primer canal del banco y dueño (cualquier muestra), si está en uso; nada con el audio apagado | PSysSound 0x6D120A |
| `Volume(canal)` | `LH_SampleInfo` +0x38 | el fundido de PSysSound 0x6D1223 |

### Cada sitio

| sitio | original | ahora |
|---|---|---|
| `spell_sounds::StartSound` | 0x6745D0: posición global (+ suelo con SnapToGround), superficie con USESURFACE (`ecs::sea_cells::GetSurfaceType`), Delayed → retardo distancia/347 (0x6747AE); si no, `SamplePlayAnimEffect(this, dist, {tamaño, alineamiento, 1, superficie, acción}, 0, spells, track 1, 0, 0)` | `Owner::Object` registrado con `RegisterObject` (su `Get3DSoundPos` 0x6D1000: el átomo con suelo si SnapToGround; sin átomo, el último punto) |
| `spell_sounds::ProcessTurn` | fn_006D11A0: sin átomo, `LHSampleIsPlaying` directo (ninguno → se borra), fundido `max(vol − FadeStep, 0)` con `LHSampleSetVolume` en **ese** canal, y una vez `SamplePlayAnimEffect(this, 0, clave, 1 + SoftRelease, …)` (1 parar, 2 soltar el bucle); con átomo, Looping o Delayed vencido → otra vez dentro de 1200 de la cámara | `PlayingChannel`, `Volume`/`SetVolume`, `SamplePlayAnimEffect` con `AnimAction::Stop`/`Release` |
| `FireSound` | fn_0072EDE0 por turno y ranura: opciones InGame, muestra 2, dueño el fuego, 3D, track 1, en `Get3DSoundPos` 0x72EE70; el .sad le da bucle −1 y modo 2 (FLAGS 0x7E0); fn_0072EDC0: si +0x38 & 0x20, `StopPlayingSoundEffect(fuego, 2, InGame)` | `PlaySoundEffect(PlayOptions)` por turno; `StopSoundEffect(2, dueño, InGame)`; `ToBeDeleted` vacía la primera ranura del fuego (y, en openblack, la otra si la tenía) |
| `HandSystem::BeginApplyOnRelease` / `EndApplyOnRelease` | `SoundTag::Create(GInterface, 3, track 0, modo 2, −1, 0, 3D, IN_GAME, 0)` 0x5D275E; `SoundTag::Remove(this, 3, IN_GAME)` 0x5D27C8 | `tags::Create`/`tags::Remove` con la entidad de la mano izquierda (**aproximado**: su `Transform` por el punto más nuevo del búfer del ratón de `GInterface::Get3DSoundPos` 0x5CEC50); al soltar, el bucle acaba su pasada (`ToBeDeleted` 0x71ECB0) |
| `Gesture` (reconocido) | fn_006882F0 (el estado del registro es el de `MyInterface`): 2D `PlaySoundEffect(0, 0x24, 3, 0, 0, 0, IN_GAME)`; otro interfaz: opciones 3D, track 0, dueño los datos del átomo, en el +0x3C del registro | igual (`RecognisedGesture::fromInterface` / `handPosition`) |
| `Fireball::FlyBySound` | 0x683184: a < 40 de la cámara ahora y > 40 antes (estricto, 0x68322B), velocidad² > 400; `PlaySoundEffect(0, 0x40 + GetTickCount() % 5, 2, 0, 0, 0, IN_GAME)` | igual con `TickCount()` (antes un contador propio y `>=` en el paso anterior) |
| `hand_fx::DoRemoveFromHandVisual` | opciones InGame, 0x77, dueño el PHandFX, 2D (0x68CEE1) | `PlayOptions`, `Owner::Object` fijo |
| `hand_fx::AddSpellToHandVisuals` | `PlaySoundEffect(0, 0x23, 3, 0, 0, 0, IN_GAME)` 0x68DE7D | igual |
| `seed::SetPowerUp` (fn_00729C40) | PU 0/1/2 → `PlaySoundEffect(0, 10/11/12, 2, 0, 0, 0, SpellDialogue)` | igual; con la pantalla ancha del guion lo quita el filtro del userParam 1, como el original |
| `PlayFullyChargedSoundFX` | 0x77F4E0: tabla 0x77F5F8 por tipo de semilla (> 0x1D → 8), `PlaySoundEffect(0, voz, 2, 0, 0, 0, SpellDialogue)` | igual |
| `PlayTapSound` | fn_00726490: opciones InGame, 0x2A, sin dueño, 2D, tono +0x48 de la tabla {100, 115, 130, 145, 155, 175, 190} con el índice en 0..5 | igual; la máscara +0x1C queda a 0 (el .sad de la 42, FLAGS 0x402, no tiene el bit de tono 0x1, así que el tono de las opciones vale sin máscara; el «callerMask 0x1» del PLAN no hace falta) |
| `one_off::InterfaceTap` | opciones InGame, 0x6D, dueño el orbe, 3D, track 0, en el +0xC8 del estado del interfaz (0x72A6F4) | igual (**aproximado**: el punto de interacción de la mano izquierda por el +0xC8) |
| `teleport::MoveByTeleport` | `SoundTag::Create(MapCoords&, 0x27/0x26, track 0, modo 2, 0, 0, 3D, IN_GAME, 0)` 0x5EC358/0x5EC372: salida en las MapCoords del ser, llegada en las del argumento | `tags::CreateAtMapCoords` (antes un emisor suelto con la llegada a altura 0) |
| `FireGraphic` (vapor) | fn_00731AB0 → fn_007314E0: opciones InGame, 0x35, dueño el FireGraphic, 3D, track 0, en su +0x98 | igual (**inferido**: +0x98 = la posición del objeto que arde; su escritor no se ha leído) |
| panel de depuración «Audio Player» | — | «Sound» toca la muestra en 2D por `PlaySoundEffect`; «Music» arranca un MUSIC_TYPE como START_MUSIC (`ScriptStartMusic`) y lo para como STOP_MUSIC; la pestaña «Emitters» y sus volúmenes flotantes desaparecen (el maestro de efectos está en «Channels») |

### Cambios audibles (por el original)

- Todo lo de los milagros ocupa uno de los 16 canales, con prioridad, robo y la ley de QMixer; pasa los filtros de GAudio
  (pantalla ancha del guion, SET_GAME_SOUND, ciudadela, estados de interfaz).
- El fundido de los PSysSound baja el volumen 0..127 del canal (antes una fracción del emisor).
- El crepitar del fuego sigue al objeto (track 1); al quitarse el bucle del gesto acaba su pasada en vez de cortarse.
- El gesto reconocido de otro interfaz, el vapor y el teletransporte suenan en 3D en su punto.
- La bola de fuego que pasa elige su muestra con `GetTickCount() % 5`.

### En juego

Land 1 (logs en `dev\_audit\audio\`): `b5_bolt_fire.log` / `b5_bolt_end.log` (`OPENBLACK_TEST_SPELL=LIGHTNING_BOLT…`,
`OPENBLACK_TEST_FIRE`: S_HandLightning_Final en un canal con `owner object 1`; G_Fire_01 en su canal cada turno sin
reiniciarse, modo 2), `b5_fireball.log` (bola de fuego: S_Fireball_S_01 y S_FireballHitSolid_L_01; al morir el átomo,
«stop … atom gone» y «deleted»; tres fuegos que se turnan las dos ranuras), `b5_gesture.log` (semilla FIRE armada:
G_HandGesture_02 3D en el canal de la etiqueta mientras se pulsa; al soltar se acaba), `b5_seed.log` (G_SpellPowerUpBand
2D; la voz PU 10 la filtra la pantalla ancha de la intro, como el original), `b5_tap.log` (icono de FIRE tocado:
G_ClickOnSpell_01 con tono 100 y la voz HELP_TEXT_ANNOUNCER_VOICE_FIREBALL_01 al cargarse).

### Auditoría de suposiciones de B5 (TEAM_GUIDELINES §1.7)

Repasado el commit 69715e9f contra el desensamblado: 0x726490 (la tabla de tonos {0x64, 0x73, 0x82, 0x91, 0x9B, 0xAF,
0xBE} y el recorte 0..5 de 0x7264B7..0x726513, el tono en +0x48 tras el `push`), 0x6882F0 y 0x6885BB..0x68865B (2D con
modo 3 / opciones 3D con dueño los datos del átomo y track 0), 0x68CE90..0x68CEE1 y 0x68DE69..0x68DE7D, 0x72A660..0x72A6F4
(dueño el orbe, 3D, track 0, el punto +0xC8, y el `ToBeDeleted` justo después), 0x5EC340..0x5EC372 (los nueve argumentos
de `SoundTag::Create`), 0x72EDC0/0x72EDE0 (el bit 0x20 antes del disparo, track 1) y 0x72EC79..0x72ECE4 (solo la primera
ranura, y el máximo recalculado), 0x5D274D..0x5D275E y 0x5D27C3..0x5D27C8, 0x6D1000 y 0x6D11A0..0x6D137A (la clave
{+0x24, +0x28, 1, +0x20, +0x1C} en los dos sitios, la acción `1 + SoftRelease`, el fundido `max(+0x38 − FadeStep, 0)`,
el `< 1200²` estricto), 0x674740..0x6747B4 (el retardo distancia/347), 0x683184..0x68327F, 0x7314E0/0x731B0F,
0x77F4E0..0x77F5EE (`> 0x1D` → 8) y, en el DLL, 0x10014010 (el primer canal del banco y dueño, +0x8C == 1, nada con
+0x14 a 0). Todos coinciden. Los `.sad` confirman lo que el commit afirma: la 42 tiene FLAGS 0x402 (sin el bit de tono
0x1, así que la máscara +0x1C a 0 es lo correcto) y las voces 10/11/12 de SpellDialogue tienen userParam 1 (las quita la
pantalla ancha, como dice el commit). No hay dependencias de componentes ECS en `src/Audio` nuevas, ningún
`CreateEmitter`/`PlayEmitter`/`AudioEmitter` fuera, y cada `RegisterObject` se deshace: PSysSound en `ProcessTurn`/`Clear`
(el `shared_ptr` sigue vivo en `g_Sounds` hasta el `UnregisterObject`), FireEffect en `sound::Free` (que `ToBeDeleted`
llama siempre, antes de que `g_Pool` lo libere) y en `sound::Clear` (antes de `g_Pool.clear()`); el mapa nuevo pasa
`psys::manager::Clear` y `magic::OnLoadMap` → `ecs::fire::Clear` antes de `audio::ClearMap`.

Arreglado en la auditoría:

- B5 no dejó ningún test. `test_sample_play` tiene tres nuevos (15 en total): `OwnerChannelAndTheFadeOfAPSysSound`
  (el primer canal del banco y dueño, otro banco o dueño → ninguno, el fundido 127 → 97 → 0 con su ganancia, y al
  acabar la onda ya no hay canal: el PSysSound se borra), `OwnerChannelNeedsActive` (0x10014018) y
  `TapSoundKeepsTheOptionsPitch`, que con la 42 real comprueba el **tono 175 de la colocación 5** de la verificación del
  PLAN (lo que no se pudo ver en juego) y que con el bit 0x1 del `.sad` ganaría el tono del banco.
- `PSysSoundPosition` pide `atom->drawn`, que 0x6D1000 no mira: el comentario lo marca ahora como de openblack (el +0xF4
  solo se escribe al dibujar el átomo).
- `Audio.h`: un `Owner::Object` que solo toca canales sin track no necesita `RegisterObject` (PHandFX, vapor, gesto).
- `ECS/Trees.cpp` (sin dueño): el crecer del árbol regado sorteaba la muestra con `Locator::rng` aunque su comentario
  citaba `GetTickCount() % 9`; ahora usa `audio::TickCount()` como 0x74C4B3..0x74C4C0.

En juego (Land 1, `audit_fire.log`): con `OPENBLACK_TEST_FIRE` y la cámara al lado, G_Fire_01 se queda en **un** canal
(índice 2) 72 turnos seguidos sin reiniciarse (modo 2) con `owner object 1`; el único cambio de asa fue al principio, con
la cámara de la intro rozando el máximo de 80 de la muestra. Eso cierra el «el fuego ocupa un canal» del PLAN.

### (Aproximado), (inferido) y pendiente de B5

- (Aproximado) el bucle del gesto usa la entidad de la mano izquierda en vez de `GInterface`; el orbe de un uso suena en
  el punto de interacción de la mano en vez de `GInterfaceStatus` +0xC8.
- (Inferido) `FireGraphic` +0x98 = posición del objeto.
- (Aproximado, de Milagros, sin cambiar) `FireSound::Consider` no para el sonido de la ranura cuando es el mismo fuego
  (fn_0072EDC0 en 0x72F82C); el alineamiento del dueño en la clave de PSysSound (0x674678) no se rellena.
- Pendiente: `FallingSpell` (LHSampleStop/LHMusicStop directos 0x526FD6..0x5271B0), el poder tribal de
  `DoPostCastThings` 0x72930A (27 + tribu, banco 9; nunca en el juego normal), `PSysSound::Save`/`Load` (C6).

## Fase B: B8 implementado (interfaz y mano)

Sesión audio, rama `local/audio`. Fuentes: `ui_creature.md` §2.0-2.2, §2.6 y §3, `sfx_inventory.md`, y el desensamblado
de `Abode::InterfaceTap` 0x406830..0x406966 (con `Abode::GetAbodeType` 0x4061F0 y `Abode::InterfaceValidToTap` 0x406820),
`fn_004082F0` / `fn_00408340` (el clic de SetupBox), `GGame::Loop` 0x54D009..0x54D078 (el Logo),
`fn_0x005e5cd0` 0x5E61A1..0x5E6230, `fn_00827820`, `fn_00827210`, `InfluenceCircle::Draw` 0x826C90 / `Reset` 0x826C50 /
`Add` 0x826FA0, `GGame::Update3DInfluence` 0x5552A0, `GInterface::StartGrab` 0x5D1740 y `SendTap` 0x5D38A0.

### Qué suena ahora

| sitio | original | openblack |
|---|---|---|
| Clic de un control de los menús propios | `fn_004082F0` (desde el bucle de SetupBox `fn_00408340`, códigos 0xA/0xC en 0x408AA6, 0x408BBE, 0x408D6F): `G_MenuButton` InGame **159** 2D modo 3 + inmersión 0x2C | `MenuClick()` en `src/Debug/Gui.cpp`: envuelve cada `MenuItem`, `Button`, `Checkbox` y `Selectable` de la barra de menú y del menú de Mods (los diálogos propios de openblack). La inmersión no se porta (no hay force feedback) |
| Llamar a la puerta | `Abode::InterfaceTap` 0x406830: solo si `GetAbodeType() & 2` (LivingQuarters: casas A..F y molino), `G_KnockRoofMulti` **110 + [0xC4CC7C]** (0..8 rotando), 3D con track 0, dueño la casa, en el punto de la mano (status +0xC8) | `ecs::abodes::InterfaceTap` (`src/ECS/Abodes.{h,cpp}`), llamado desde `HandSystem::Update` cuando se pulsa el botón de acción sobre una casa: una casa no cabe en la mano (`Object::ValidForPlaceInHand` 0x402870 = 0), así que `StartGrab` la toca al momento, y el toque exige la mano dentro de la influencia (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1). Usa el contador `audio::Counter::KnockRoof` |
| Cruzar un anillo de influencia | `fn_0x005e5cd0` 0x5E61B0 por fotograma si no está en pausa: `fn_00827820` mira el punto de la mano contra los círculos de `GGame::Update3DInfluence` (uno por ciudadela y por pueblo con influencia) y, por cada jugador cuyo «la mano está dentro» cambió, busca con `fn_008277B0` un círculo suyo cuyo borde cruzó la mano entre el punto anterior ([0xEA9EF0], que cada llamada reescribe) y el actual; si lo hay, hace la onda y pone [0xEB9A6C]; entonces `G_HandThroughInfluence_01` InGame **52** 3D (track 0, sin dueño) en la mano + inmersión 6 | `influence::ProcessHandCrossing` (`src/ECS/Influence/InfluenceCircles.cpp`), llamada desde `Game.cpp` tras colocar la mano si el juego no está en pausa. El `.sad` le da modo 1 (canal nuevo por cruce), volumen 40 y min/max 100/300: entrar y salir suenan igual, y un círculo que aparece, crece o mengua bajo la mano quieta no suena (no hay borde cruzado). `influence::HandCrossedInfluence` es `fn_00827820` sin el sonido |
| Gritos al coger un aldeano | `GInterface::GenericPickup` 0x5D28C5..0x5D295D | ya estaba en B4 (`HandHolding.cpp`): dos tags, `G_PickUpObject` 10 y 180/187/194 + `GetRandomSample(7)` |

`Abode::GetAbodeType` 0x4061F0 lee el `GAbodeInfo` +0x120 de la casa; openblack guarda el `AbodeNumber` y la malla, así
que `abodes::TypeOf` busca el registro por esos dos (como `influence::AbodeInfoOf`) — **(inferido)**: todos los registros
de un mismo número de casa llevan el mismo bit de LivingQuarters.

### Lo que no tiene sitio en openblack (pendiente, con su dirección)

- **Logo** InGame 160: `GGame::Loop` 0x54D011 lo arranca una vez ([0xBEC27C]) y lo **para** al volver de `DoLogo`
  0x5FA070 (0x54D073). openblack no tiene esa secuencia de logo, así que ponerlo sonaría distinto (sonaría sin el logo);
  queda para cuando exista.
- **`G_ClickOnSpell_01` 42** de la arena (`ArenaSpellIcon` fn_00425660) y del poste de la correa
  (`LeashObj::InterfaceTap` 0x464490): openblack no tiene arena ni correa. El del icono de culto (0x726430 → fn_00726490,
  con su tono) ya está desde B5 (`Worship/WorshipSpellIcon.cpp`).
- **Conquistar un pueblo** `G_TakeOverTown_01` 205 (fn_00649810, desde `SetTownEmpty` 0x7410DA): `Town::owner` solo se
  escribe al crear el pueblo, no hay conquista.
- **Orden aceptada** `G_AcknowledgeCommand` 1: sus tres sitios (0x5D2E2C, 0x5D3044, 0x5D4162) son órdenes a la criatura
  con la correa y la arena (paquetes 0xF/0x10 y `fn_0048A490`/`fn_0048A420`); openblack no tiene criatura.
- **Influencia virtual** `G_VirtualInfluence_04` 129 (`GVirtualInfluence::Draw` 0x76D000, `Start` fn_0076CED0,
  `SetSoundFraction` 0x76CF90, `Stop` fn_0076CF60; bucle 2D con tono = fracción·100): openblack no tiene el estiramiento
  de la mano con ancla ni los sprites del camino de maná.
- **Cofre y pergaminos**: `Reward::InterfaceTap` 0x6E5D00 (173 `G_ClickSignpost`, 40 `G_RewardSting`, 41 `G_OpenChest`) y
  `ScriptHighlight::SetActivated` 0x70A630 / `InterfaceTap` 0x70AC70 (134 `G_ClickOnScroll`, 173): no existen esos
  objetos.
- Del `Abode::InterfaceTap` real falta todo lo que no es sonido: [0xC4CC6C] = el pueblo tocado (0x40683F),
  `HowManyPeople::KnockKnock` 0x829690, `Villager::SetStateWhenTappedOnAbode` 0x752B80 de los habitantes (+0xA0) y la
  animación 0x39 de la mano (`CHand::StartFixedPosAnimation` 0x46C050).
- El clic de menú no suena en los controles que se arrastran (los sliders de los menús propios): el original sí les da
  el código 0xA al soltarlos **(pendiente)**. La activación por teclado (código 0xC) tampoco.

### (Aproximado), (inferido) y pendiente de B8

- **(Aproximado)** [0xEB9A1C] (el borde de influencia de ese jugador ya ha aparecido: un pestillo por mapa que solo
  pone `fn_00883120` cuando el fundido del gráfico del borde llega a 1, 0x8831AD, y que borra `fn_00828A50` desde
  `LH3DIsland::Create` 0x803E85; `InfluenceCircle::Draw` solo lo lee, 0x826F15) se toma por puesto: openblack no dibuja
  el borde de influencia. Por eso, justo tras cambiar de mapa, openblack puede sonar en un cruce que el original
  callaría hasta que aparece el borde. La onda del cruce (fn_00827670 / fn_00827250) tampoco se hace.
- **(Aproximado)** con la mano fuera de la tierra `Game.cpp` no llama a `ProcessHandCrossing` (no hay punto); el
  original sigue pasando el último [0xE9A100].
- **(Aproximado)** el punto de la mano del cruce sale de `HandSystem::GetPlayerHandPositions()[0]`; el original usa
  [0xE9A100], que `GLandscape::Draw` 0x5E4395 rellena desde la mano de `MyInterface()`.
- **(Aproximado)** el clic de los menús: openblack no tiene SetupBox; sus diálogos son la barra de menú de ImGui y el
  menú de Mods, y suena cuando un control dice que se ha pulsado (como el código 0xA del original).
- **(Inferido)** todos los registros de un `AbodeNumber` llevan el mismo `ABODE_TYPE` (arriba).
- El pestillo «ya hay estado del fotograma anterior» ([0xEB9A68]) es estático del proceso, como en el original (nada lo
  borra entre mapas).

### Auditoría de B8 (sesión audio)

Comprobadas en el desensamblado: 0x406820 (`mov eax, 1`), 0x406830..0x40694A (vt +0x8C4 de Abode = `GetAbodeType`
0x4061F0, `test al, 2`, 0x6E + [0xC4CC7C] con vuelta a 0 en 9, +0x20 dueño, +0x08 = 1, +0x0C = 0, punto status
+0xC8), 0x4082F0 (0x429DA0 con dueño 0, 0x9F, modo 3, lazos 0, +0x10 0, 2D, banco +0x3AC; inmersión 0x2C), 0x5E61A6
(pausa), 0x5E61B0..0x5E621E (0x34 = 52, +0x20 = 0, 3D, track 0, sonido solo si [0xEB9A6C]), 0x827820, 0x8277B0,
0x827210 (estricto), 0x5552A0, 0x5508A0 (para en el jugador de tipo 3), 0x5D1740 (vt +0x6FC de Abode = 0x402870,
+0x714 = 0x4028A0), 0x5D38A0 (+0x740 = 0x406820, paquete 0x20), 0x828A50, 0x883120.

- **Corregido**: el cruce sonaba cada vez que cambiaba el bit «dentro» de un jugador, también cuando un círculo aparecía o
  crecía bajo la mano quieta; el original exige `fn_008277B0` (un círculo del jugador con el punto anterior [0xEA9EF0]
  y el actual a lados distintos del borde). Ahora se guarda ese punto anterior y se busca el círculo.
- **Corregido**: el comentario y la wiki decían que `InfluenceCircle::Draw` pone el pestillo [0xEB9A1C]; solo lo lee.
- **Corregido**: los tests del golpe y del cruce copiaban la llamada en vez de ejercitar el código; ahora llaman a
  `ecs::abodes::InterfaceTap` (casa y centro del pueblo, que calla) e `influence::ProcessHandCrossing` sobre el registro
  (entrar, quedarse, salir, círculo que crece, pueblo neutral), y el de los diez toques no depende del valor de partida
  del contador estático.
- Sin cambios: el clic de los menús (el PLAN B8 lo pide en los menús propios), ninguna dependencia ECS nueva en
  `src/Audio`, ninguna fuente/búfer AL nuevo, sin hilo de música. `audio::OnThingDeleted` no lo llama nadie (de B0, no
  de B8: los canales siguen por `ownerPosition`, que con ids versionados de entt da nullopt para una entidad borrada).
- En juego (`_audit/audio/b8_audit_knock.log`, `OPENBLACK_TEST_KNOCK="10,0,6,0.4"`, `OPENBLACK_SFX_TRACE=1`): los diez
  toques dan 110..118 y 110, 3D track 0, dueño la casa (cortados por el máximo 150 con la cámara a 220).

### Tests y juego

- `test_ui_sfx` (6 tests, 46 ejecutables en total): los parámetros de usuario reales de InGame.sad de las muestras de
  interfaz (0 en 42/134/159/160/173/205/40/41/119, 1 en 1/52/110..118, 2 en 46, 4 en 129) y el modo 1, volumen 40 y
  min/max 100/300 de la 52; **tocar una casa diez veces da 110..118 y vuelve a 110**, 3D en el punto de la mano con
  max 150; dentro de la ciudadela no suenan ni el `MenuButton` (parámetro 0) ni el golpe (1) pero sí el woosh (2); la 52
  toma un canal nuevo por cruce (modo 1 del `.sad`) y se corta a más de 300 de la cámara.
- En juego (Land 1, `_audit/audio/b8_knock3.log`, gancho `OPENBLACK_TEST_KNOCK="10,0,6,0.4"` con
  `OPENBLACK_SFX_TRACE=1`): los diez toques dan `InGame.sad/110, 111, 112, 113, 114, 115, 116, 117, 118, 110`, 3D con
  track 0, dueño la casa y en su punto. Durante **toda** la intro de Land 1 el guion tiene la pantalla ancha (la
  narración sigue a los 75 s, `b8_knock6.log`), así que GAudio los filtra por el parámetro de usuario 1 — como haría el
  original; el arranque audible está en el test.
- Coger un aldeano **suena dos veces** (`b8_pick2.log`, `OPENBLACK_TEST_PICK_VILLAGER="0,6"` con
  `OPENBLACK_TEST_VIEW_VILLAGER="0,5,0"`): `InGame.sad/10 G_PickUpObject` → canal 113 y `InGame.sad/187 G_PickUpMan_01`
  → canal 132, los dos tags de punto 3D en el aldeano (con la cámara lejos se cortan por sus máximos 180 y 160,
  `b8_pick.log`).
- El cruce de influencia salta solo (`b8_knock6.log`): dos `InGame.sad/52` con **modo 1**, 3D, track 0 y sin dueño
  mientras la cámara de la intro arrastra la mano por el borde del círculo de un pueblo (filtrados por la pantalla
  ancha, igual que el original). El clic de menú no se puede probar sin mover el ratón.

## Fase B: B9 y B10 implementados (Guidance y voces nocturnas)

Sesión audio, rama `local/audio`. Fuentes: el desensamblado entero de SoundGuidance.cpp 0x71AA90..0x71D480
(`tmp_dis\audio\voices_guidance_71ab10.txt` y 0x71AA90), de SpookyVoices.cpp 0x72E130..0x72E8B0
(`tmp_dis\audio\spooky_72e130.txt`), los llamadores (`callers.py`: 0x54E711..0x54E729, 0x5DC50D, 0x7506C0..0x7508F4,
0x4141A0, 0x406640..0x406786, 0x66F4D8..0x66F509, 0x63A9A0..0x63A9E6), HelpSystem::RunMessage 0x5C8CE0 /
StopHelpScriptsForNewHelp 0x5C8C40 / TriggerCategory 0x5C8280 / Reset 0x5C5580 / fn_005C6CF0, GScript::StartScript
0x6EB710, HelpSystemOn 0x6FBFD0, SetHelpSystem 0x6FC020, GRand 0x6DE570 / 0x6DE590, _LHRand 0x7DB600, la fase de la
luna fn_0086A7F0 y las tablas 0x980128..0x9804D0, 0x999434 (`tmp_dis\audio\b10_dump.py`; el azar sembrado de los tests: `b9_interval.py`, el Soundex: `b10_soundex.py`; los llamadores: `b9_callers.txt`).

### GGuidance (`src/Audio/Guidance.{h,cpp}`, `audio::guidance`)

**Fiel.** Un solo GGuidance (el original tiene uno por GInterfaceStatus, +0x30, y todos los llamadores usan el de
`MyInterfaceStatus`). Lo que lee del juego llega por `GameQueries` (sección B9); sin consulta, el valor neutro de un
juego sin ese sistema, y no suena nada.

| función | original | qué hace |
|---|---|---|
| `Init` | 0x71AC70 (GInterfaceStatus::Init 0x5DD1CB) | opciones propias (banco Guidance, 2D, track 0, modo 2); lastPlayed[t] = turno − LocalRand(base) si es > 0 (**con un segundo sorteo** para el valor que se guarda), si no 0; +0x98..+0xC8 = 0, +0xA8 = 30, los 7 «una vez» a 0, [0xC221CC] = 1. openblack lo llama al empezar cada tierra |
| `TimeSinceLastPlayed`, `Interval`, `PlayNow`, `HelpSpritesPlayNow` | 0x71ADF0, 0x71AEE0, 0x71AF50, 0x71AFF0 | ver arriba; `HelpSpritesPlayNow` = PlayNow(8) && PlayNow(t) (el tipo 8 nunca se actualiza: es una puerta global) |
| `PlaySample` | 0x71C6F0 | opciones **persistentes**: vol, tono, +0x2C, muestra (tabla de voz 0x96BA38 si es texto), dueño = nº del jugador; 3D: punto, max, min = max·0,333 (0x8D8734), máscara 0x180, track 0; **sin punto no suena ni marca**; 2D deja lo demás como lo dejó el último 3D; lastPlayed = turno |
| `HelpSpiritSay` | 0x71D270 | RunMessage(texto, texto, «MultiHelpJustTalkWithText» o «…NoText» para el 31) + TriggerCategory(8); lastPlayed y +0x2C = turno **aunque el guion no arranque** |
| `OneOff(k)` | fn_0071D0B0 | una vez por tierra, con probabilidad (tabla 0x980440: 3308 0,02 · 3317 0,0002 · 3318 0,01 · 3321 0,01 · 3325 0,05 · 3326 0,1 · 3328 0,025) → HelpSpiritSay(texto, 32) |
| `GetRandomSample`, `…BasedOnValue` | 0x71D300, 0x71D320 | listas HELP_SPRITES_GUIDANCE de info.dat (22 × 34; hasta el primer 0; 34 llenas cuentan 33) |
| `TimeSinceThingSeen`, `DesireSample`, `DesireScore` | fn_0071AE10, fn_0071AA90, fn_0071B410 | ver abajo |
| `ProcessTownDesireSFX` | 0x71B020 (+0x71B130, +0x71B270) | cada 10 turnos: el pueblo con almacén y gente más cerca de la cámara (< 200) y sus 17 deseos {valor +0x37C, tipo +0x380}; luego los 6 lugares de culto de mi ciudadela con fieles (comida, y +0x70 de la ciudadela) **pisan** al pueblo si puntúan (el de +0x70 siempre que no sea 0); valor > 0,3 → x = v − rand(v/2), 3D en el pueblo o la ciudadela, max 200·x; +0x98 = la muestra |
| `ProcessHeartBeatSFX`, `HeartBeat`, `SetHeartBeatOverride`, `HeartBeatPulse`, `StopHeartBeat` | 0x71C190, 0x71C460, fn_0071C3F0, fn_0071C430/450, fn_0071C650 | cada 10 turnos v = Σ deseo de protección + ((+0xC8 + 0,001)/(q + 0,001) − 1) + ((+0xC4 + 0,001)/(p + 0,001) − 1) + Σ por cada criatura enemiga cuyo pueblo más cercano es mío (d < 400) 1 − max(d − 100, 0)/400, recortado a 0..1; p, q suavizados 0,1; **cada turno** (el `jne` de 0x71C1AD salta a la llamada 0x71C3C1) fn_0071C460(v guardado): tono, fase (+tono·0,025·100 ms·0,001), pulso (1 − cos 2πφ)/2 |
| `HelpSpritesCheckMoonPhase` | 0x71D1C0 (estática) | cuenta atrás [0xC221D0]; de noche visual, fase − π; de noche real y \|·\| < 0,15 → OneOff(5) y 600000 turnos; si no, ftol((fase − π)²·12000) |
| `MoonPhase` | fn_0086A7F0 | 2π(1 − frac(días·0,03386318)), días = time()/86400 − 10962 (entero) |
| `ProcessGameTurn` | GGame::ProcessTurn 0x54E711..0x54E729 | GSpookyVoices::Process, la luna, los deseos y el latido de GInterfaceStatus::Process 0x5DC50D (orden **(aproximado)**) |
| `ResourceDropSFX`, `ResourceDropSample` | 0x71B570, 0x71B5F0 | el pueblo más cercano < 100; suma de sus tres valores del tipo ≥ 0,5 → PLEASED_x, < 0,25 → DISPLEASED_FOOD para comida y **PLEASED** para madera y lluvia (los DISPLEASED de madera y lluvia no se usan en W120); 3D, max 200 |
| `TownAttackSFX`, `StrongestEffect`, `AttackerSample`, `HelpSpritesTownBeingAttacked` | 0x71B7C0, fn_0071BE40, 0x71BC20..0x71BD50, 0x71C870 | 10 ATTACK, + 10 FIRE si el efecto 0 manda, + 10 veces la muestra del atacante (rayo / roca / criatura); una al azar; max 200·(min(+0xEC0·0,2, 1) + 1); y siempre el comentario si el pueblo es mío (PlayNow, no HelpSpritesPlayNow; agresión > 1) |
| `StartRaiseTotemSFX`, `EndRaiseTotemSFX` | 0x71BEB0, 0x71BED0 | el final no toca nada en W120 (calcula la clase de alineamiento y vuelve) |
| `MakeDiscipleSFX`, `DiscipleText`, `AlignmentClass` | 0x71BF10, fn_0071AB70, fn_0071C690 | 2D, vol 85; tabla 0x98040C; el discípulo 10 dice GOOD/EVIL_LIVE_HERE según la clase (±0,55) |
| `BeliefSFX`, `BeliefSample`, `BeliefVisibility` | 0x437F40, fn_0071BF70, fn_0071C0D0 | solo si la creencia del jugador está por debajo de la mayor; x = (b + 0,0001)/(máx + 0,0001) − rand(x/3); visibilidad t0·(1 − d²) > 0,3 |
| `DeathInVillageSFX` | fn_0071C810 | DEATH_IN_VILLAGE_06 + rand 5, 2D |
| `HelpSprites*` (16 funciones) | 0x71C930..0x71D070 | cada una su condición (gente, almacén que funciona, a < 300 de la mano GInterface+0x3B8, en pantalla) y su lista (tipo − 9) |
| `HelpSpritesAlignmentProcess` | 0x71CEB0 | acumulado +0xC0 = 0,95·+0xC0 + cambio; pasado 2·cambio máximo del jugador: mismo signo y \|a\| > 0,75 → bueno: tipo 29 lista 20, malo: tipo 28 lista 19 **(sic: los nombres de Enums.h los dan cruzados)**; signo opuesto y \|a\| > 0,4 → 25/16 o 26/17 |

**Azar**: LocalRand(n) ∈ [0, n) (0 para 0, sin sorteo) y LocalFloatRand(x) = x·LocalRand(0xFFFF)·(1/65535) como el
original; el generador es el de openblack **(aproximado)** salvo en los tests, que usan `LHRand` con semilla.

**Consultas nuevas** (`GameQueries.h`): `playgroundGame`, `multiplayerGame` (falso), `helpLevel` (HelpSystem; 3 sin él),
`localPlayerNumber` (PLAYER_ONE), `visualNight`, `handPosition` (la mano, **(inferido)** GInterface+0x3B8), `pointOnScreen`
(sin ella: falso), `desireTowns`, `worshipSites`, `townResourceNeeds`, `heartBeat` (sin ellas: nada), `helpRunMessage`,
`helpTriggerCategory`, `profileName`.

**HelpSystem** (A11/B7, de audio): `+0x45F8` interruptor (Reset 0x5C55FC = 1; **SET_HELP_SYSTEM 253** hecho), `+0x45F4`
nivel (3 sin perfil, 0x5C6DB6), **HELP_SYSTEM_ON 200** hecho (interruptor && nivel ≠ 0), `TriggerCategory` (+0x2D8, 9;
Reset los pone a 0), `+0x560`. `script_control::RunMessage` / `StopHelpScriptsForNewHelp`: no arranca si una tarea que no
es de ayuda (tipo sin 0x42) tiene el diálogo; si no, para los guiones de ayuda (0x4A), empuja los dos números como
float y arranca el guion con los tipos 0x7F. `chlapi::ScriptVm` sale en `CHLApi.h` con `pushFloat` y `startScript`.

**Conectado en openblack**: `ProcessGameTurn` en el turno (Game.cpp, antes de `audio::ProcessTurn`); `Init` en cada
`LoadMap`; las listas de info.dat al arrancar; `ResourceDropSFX` en `pot_resource::AddResourceToPos` (montón nuevo de la
mano local: RESOURCE_TYPE 1 → 2, 0 → 1) y en `HandSystem::DepositInStore` (con el punto y el GetGuidanceResourceType del **receptor**, el almacén: StoragePit hereda el 0 de GameThing 0x71BDD0, así que el original corre PlayNow y GetNearestTown y no dice nada). Hoy no suena nada
de esto en Land 1: los tipos no «siempre» callan en el Land 1 de la campaña, los pueblos no tienen deseos ni valores de
recursos (consultas neutras) y no hay corazón de ciudadela.

### GSpookyVoices (`src/Audio/SpookyVoices.{h,cpp}`, `audio::spooky`)

**Fiel** salvo el nombre. Objeto estático 0xDA0830: banco +0x8, opciones +0xC, muestra +0x10, contador +0x14, cuenta
atrás +0x18. La info de info.dat `GSpookyVoiceInfo` (5 entradas, 0xDA0850) no la lee nadie.

- **Soundex** (`SoundExCode` 0x72E4E0, tabla de saltos 0x72E54C): a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3,
  l 4, m n 5, r 6 y **h w y el propio carácter** (la entrada 0x72E548 devuelve eax). No letra (`_isalpha`) = 0
  ((inferido): la «C» locale, solo ASCII; «é», «ñ» dan 0).
- `GetNextSoundexCode` 0x72E5C0: salta códigos 0 hasta el fin o un espacio; tras una letra con código, si la siguiente
  tiene el mismo, **devuelve código + 1 sin pasarla** (rareza de W120: «Curro» da 7, 6, 0).
- `PerformSoundexComparison` 0x72E630: primer carácter idéntico (mayúsculas cuentan) y tres códigos iguales;
  `SoundexOverlap` 0x72E6E0: alguna palabra del nombre; `TrySoundex` 0x72E7E0: el primero de los 100 nombres (0x999434:
  4586..4685) que encaja → la muestra de la tabla de voz 0x984D48 (otra copia de 0x915D40). En la instalación española
  11 nombres se resuelven a uno anterior (Alfredo → Alberto, Manolo → Manuel, Jaime → Juan, María → Mario, Miriam →
  Mariano, Lucía y Luisa → Luis, Ángeles → Ángel, Julia → Julio, Rocío → Rosa, el segundo Alberto).
- `GetName` 0x72E740: nombre del perfil ([0xD4BF38] = PlayerProfile +0x200), luego el de la red ([0xD204D4]+0x70) y el
  `DefName` del registro «Software\Microsoft\MS Setup (ACME)\User Info»; **openblack no tiene perfiles**: `profileName`
  = `OPENBLACK_PLAYER_NAME` si está **(inferido)**; red y registro **no portados**. Sin nombre → muestra 0 → nunca suena.
- `Init` 0x72E2A0 (GGame::InitOneTimeOnly, una vez), `UpdatePlayerName` 0x72E870 (GGame::Init, cada tierra).
- `Process` 0x72E310: nada en los Land 1 y 2 ni sin muestra; cuenta atrás de 100 llamadas; de noche (fn_0072E3B0),
  r → ftol((1 − r³)·1000) < contador → `PlaySpooky`, si no OneOff(1); contador + 1. **Corrección**: `PlaySpooky` pone el
  contador a 0 (0x72E4D3), así que tras sonar vuelve a 1.
- `PlaySpooky` 0x72E3F0: tono 100·(1 + a³)^±1 con a = rand(0,65), +0x2C = rand(180), volumen ·(1 + b³)^±1 con b = rand(0,8)
  **acumulado** (las opciones persisten), 2D.

### B9/B10 en juego

Land 1, 2026-10-01 a las 23:20 (noche real), `OPENBLACK_GUIDANCE_TRACE=1 OPENBLACK_PLAYER_NAME=Mario
OPENBLACK_TEST_GUIDANCE_SAY=80:3326` (y otra con `--mod game.skip-intro` en el turno 200; logs `_audit\audio\b9_run*.log`):
`SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`; en el turno 80/200 `HelpSpiritSay(3326, type 32)
MultiHelpJustTalkWithText not started`: el guion de la tierra (tarea 19/22, tipo Script) tiene el diálogo y
`StopHelpScriptsForNewHelp` no se lo quita, como el original. Ni los deseos ni las voces nocturnas suenan en Land 1
(muteados por tipo y por tierra). Sin errores nuevos.

### (Aproximado), (inferido) y pendiente de B9/B10

- **(aproximado)**: distancias = longitud exacta x/z de los puntos del mundo (el original: hypotenuse 0x74F680 con la
  tabla 1/√ de _FUN_0074f620 sobre MapCoords 16.16); x87 en double; el generador; el orden de `ProcessGameTurn` respecto
  a GInterfaceStatus::Process; los puntos de PlaySample son puntos del mundo (sin el redondeo de MapCoords).
- **(inferido)**: [0xD01A38] = 100 ms por turno (como SoundTags); GInterface+0x3B8 = la mano; `_isalpha` ASCII; el nombre
  por `OPENBLACK_PLAYER_NAME`; el jugador local = PLAYER_ONE.
- **No modelado**: el +0x2C de las opciones (90, rand(180) en las voces nocturnas): se guarda, SamplePlay no lo usa.
- **Pendiente (sin llamador en openblack; la API ya está)**:
  - `Alignment.cpp` (Milagros): `GAlignment::ProcessForPlayer` 0x4141D9 llama cada turno, para el jugador local y antes
    de `Process`, a `HelpSpritesAlignmentProcess(GetMaxAlignmentChangePerGameTurn · pending, alignment, maxChange)`
    (también con pending 0: el acumulado decae);
  - `Villager::VillagerDead` 0x7506C0 (aldeanos, V12): fn_0071CE70 (KillingPeople, si lo mató el jugador local y la
    tabla 0x99A368 de la causa), fn_0071C810 (DEATH_IN_VILLAGE, aldeano mío, 0x99A370), fn_0071CFE0 (causa 4),
    LosingVillagers (pueblo con +0x618 > info+0x150, 0x99A36C) y LowOnPeople;
  - `Town::UpdateAggressor` 0x73C9B0 (TownAttackSFX, fn_0071C960, fn_0071C9F0), `Town::CalculateDesireForFood`
    0x747FA0 / 0x7481BC (LowOnFood/Wood), `TownDesire::Process` 0x745C8A (VillagerUnhappy), los deseos de los pueblos
    (V3 de mapa: `desireTowns`, `townResourceNeeds`, `heartBeat`), el corazón de la ciudadela;
  - `Abode::ApplyEffectsDueToPhysicalDestruction` 0x406781 (DestroyBuilding: +0x90 +0x18 < 0,4 y el jugador que lo
    rompió), `Object::InitialisePhysicsFromHand` 0x6372EA (MakeDiscipleSFX, TODO de HandHolding.cpp), el tótem
    0x738620/0x738666, `GBelief::AddToBelief` 0x437F2A, la criatura (0x45A772, 0x5039E7), fn_0071D100 (otras manos,
    multijugador), GatheringBox/EndGameBox/red (OneOff 3 y otros), fn_0064AF80 (burlas multijugador);
  - el `DefName` del registro y el nombre de red de GetName; `fn_0081F1D0` (punto en pantalla).

### Auditoría de B9/B10

Comprobadas en el desensamblado: las tablas 0x980190 (33 filas), 0x980328, 0x98040C, 0x980440 y las constantes
0x98011C..0x980188; Init 0x71AC70 (doble sorteo), Interval 0x71AEE0, PlayNow 0x71AF50, PlaySample 0x71C6F0,
ProcessTownDesireSFX 0x71B020, CheckWorshipSiteDesiresSFX 0x71B300..0x71B407 (la necesidad gana aunque puntúe menos),
ResourceDropSFX 0x71B570 / 0x71B5F0, fn_0071BF70, fn_0071C810, MakeDiscipleSFX 0x71BF10, fn_0071C460, 0x71C190,
0x71C990..0x71CAE0, 0x71CEB0..0x71D063, HelpSpiritSay 0x71D270, GetRandomSample 0x71D300..0x71D3A4, la luna 0x71D1C0 /
0x86A7F0, RunMessage 0x5C8CE0 / 0x5C8C40, CHL 200 / 253 (0x6FBFD0 / 0x6FC020), y GSpookyVoices entero (0x72E280..0x72E87F,
la tabla de saltos 0x72E54C). Dos correcciones:

- **El latido corre cada turno**: en `ProcessHeartBeatSFX` el `jne` de 0x71C1AD (turno no múltiplo de 10) salta a
  0x71C3B8, que llama a fn_0071C460 con el +0xA4 guardado; solo el valor se recalcula cada 10 turnos. B9 volvía sin
  latir, así que el tono se suavizaba y la fase avanzaba 10 veces más despacio. Test
  `HeartBeatRunsEveryTurnTheValueEveryTen`.
- **`DepositInStore` no dice nada**: en Object::DoDeleteObjectAndTakeResource 0x63A940 `this` (edi) es el receptor y
  esi el objeto (GetPos 0x63AA2B y ToBeDeleted 0x63AAB1 van a esi); ResourceDropSFX recibe edi+0x14 y el
  GetGuidanceResourceType de edi (vt +0xE0), que para StoragePit es el de GameThing 0x71BDD0 = 0. B9 pasaba el punto del
  árbol y «madera» (PLEASED_WOOD inventado en cuanto haya `townResourceNeeds`); ahora el punto del almacén y `None`:
  PlayNow y la búsqueda del pueblo corren y no suena nada, como el original.

En juego (Land 3, 00:02 de noche real, `OPENBLACK_PLAYER_NAME=Mario OPENBLACK_TEST_GUIDANCE_SAY=90:3326`, logs
`_audit\audio\b9_audit_land3*.log`): `SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`, y en el turno 90
`HelpSpiritSay(3326, type 32) ... not started` (también en Land 3 el guion de la intro tiene el diálogo). Sin errores
nuevos.

## Fases B y C

**B0..B10 y B11a hechos; falta la fase C** (PLAN §4-5). Milagros y agua ya están fusionados en `local/hand-hbn`:

| hito | contenido |
|---|---|
| B0 | **hecho** ([abajo](#fase-b-b0-y-b1-implementados)) |
| B1 | **hecho** salvo: el maestro no se guarda en disco (`AudioManager::PlayMusic` se retiró en B5; `MusicStream` va sobre `audio::device` desde B11a) |
| B2 | **hecho** ([arriba](#b2-los-anim-effects-en-el-núcleo)); desde B5 `SpellSounds` también va por `SamplePlayAnimEffect` |
| B3 | **hecho** ([arriba](#b3-soundtag-completo)); faltan los llamadores del original (molino, taller, tótem, credo, caída de árboles: B4/C3) y ATTACH/DETACH_SOUND_TAG (B6) |
| B4 | **hecho** ([arriba](#b4-los-llamadores-del-mundo-en-los-canales)); falta el volcán (`LandscapeVortex` 0x5FEE5A: openblack no lo tiene); el vapor (`FireGraphic` 0x731542) entró con B5 |
| B5 | **hecho** y **auditado** ([arriba](#fase-b-b5-implementado-los-milagros-en-canal), [auditoría](#auditoría-de-suposiciones-de-b5-team_guidelines-17)); los modificadores de PSys de F3 (`AddSoundToAtom` 0x69DCA0, `RemoveSoundFromAtom` 0x69DDD0, `StartStopSoundOnCondition` 0x69DC40) ya los tenía Milagros (`PSys/Rules/Sound.cpp`) |
| B6 | **hecho** ([arriba](#b6-chl-de-efectos)); el ambiente (`GSoundMap` 0x71D6F0, LHAtmos 0x428FE0 / 0x100018B0) ya era de agua y va por `audio::` |
| B7 | **hecho** ([arriba](#fase-b-b7-implementado-voces-en-canal)); falta la parte visual de los consejeros (modelos, vuelo, boca) |
| B8 | **hecho** ([abajo](#fase-b-b8-implementado-interfaz-y-mano)): clic de los menús propios 159, llamar a la puerta 110+c%9, cruzar un anillo de influencia 52 (los gritos 180/187/194+rand7 ya estaban, B4). Sin sitio en openblack (pendientes con su dirección): Logo 160 (no hay `DoLogo` 0x5FA070), ClickOnSpell 42 de la arena y del poste de la correa, conquista 205, orden aceptada 1 (criatura), influencia virtual 129, cofre y pergaminos |
| B9 | **hecho** ([arriba](#fase-b-b9-y-b10-implementados-guidance-y-voces-nocturnas)); sin llamador en openblack: alineamiento (Milagros), muerte de aldeanos (V12), agresor y deseos del pueblo, derribo, discípulos, tótem, creencia, criatura |
| B10 | **hecho** ([arriba](#gspookyvoices-srcaudiospookyvoiceshcpp-audiospooky)); el nombre del perfil, por `OPENBLACK_PLAYER_NAME` **(inferido)** |
| B11a | **hecho** ([abajo](#fase-b-b11a-un-solo-motor)): un solo motor; fuera `AudioManager*`, `AudioPlayer*`, `AlCheck`, `SoundGroup`, `Locator::audio`; `audio::device` y `audio::banks` |
| C1 | Criatura: cola de eventos, clave de 5 columnas, bancos por especie, filtro de jugador local / SET_CREATURE_SOUND; baile y pelea en GameMusic |
| C2 | Clima y alineamiento en el ambiente |
| C3 | Aldeanos, edificios y cánticos |
| C4 | Ciudadela interior y `ProcessCitadelMusic` |
| C5 | Vídeos (tráiler, `PlayFullScreenMovie`) |
| C6 | Guardar y cargar: `GAudio::Save` 0x428310 / `Load` 0x428480, `ThingMusicInfo::Save` 0x429950 / `Load` 0x429AE0, `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0 |
| C7 | GConfirmation (necesita `CameraModeNew3` 0x454900/30) |

Desde B5 no hay `PlaySound`/`CreateEmitter`/`PlayEmitter`/`PlayAt`/`PlayMusic`, y desde B11a tampoco `AudioManager`: todo
sonido nuevo va por `audio::` (`Audio.h`) con lo que pasa su llamada original.

## Fase B: B11a, un solo motor

Lo prometido: «un solo motor, no dos conviviendo». Fuera de `src/Audio` nadie usa ya `AudioManager` (no existe) ni
OpenAL (`grep` de `AudioManager|Locator::audio|AL/al.h|alGen|alSource` en `src`, `test` y `apps`: solo `Device.cpp`).

**Retirado**: `AudioManager.{h,cpp}`, `AudioManagerInterface.h`, `AudioManagerNoOp.h`, `AudioPlayer.{h,cpp}`,
`AudioPlayerInterface.h`, `AlCheck.{h,cpp}`, `SoundGroup.h` y `Locator::audio`. Lo que aún hacían pasa a:

| antes | ahora |
|---|---|
| `AudioPlayer::Initialize` (dispositivo, contexto, registro de OpenAL Soft, `AL_INVERSE_DISTANCE_CLAMPED`), `AudioManagerNoOp` si falla | `audio::device::Open()` desde `InitializeEngine` (`LH_AudioSystem::Create`); sin dispositivo, `NullSampleOutput` |
| `~AudioManager` (fuentes de los canales, búferes, contexto) | `audio::device::Close()` desde `ShutDownServices`, tras `audio::Shutdown` y `music::Shutdown` |
| `AudioManager::Update` (bucles finitos de los canales) | `sample_play::UpdateFrame()` al principio de `audio::UpdateFrame()` (mismo sitio del fotograma) |
| `AudioManager::UpdateListener` → `AudioPlayer::UpdateListener` | `device::SetListener(cámara, 0, forward, up)` en `sample_play::UpdateChannels` (LHListenerUpdate 0x10003960 desde fn_004270D0 0x4271EF; velocidad 0, 0x10015C1A) |
| `AudioManager::GetSampleOutput` | `device::Output()` |
| `AudioManager::GetSound`, `CreateSoundGroup`/`AddToSoundGroup`/`GetSoundGroups` (lista de bancos del panel y de LHAtmos) | `banks::Count/Path/Samples(BankId)`, `BankGroup`, `FindBank` |
| `SoundExists` mirando si la salida era `NullSampleOutput` | `device::IsOpen()` |

**Dispositivo** (`src/Audio/Device.{h,cpp}`, `audio::device`, capa 0): `Open`, `Close`, `IsOpen`, `Output`,
`SetListener`, `ListenerPosition`; fuentes (`CreateSource`, `DeleteSource`, `SetSourceBuffer/Pitch/Gain/Looping/Relative/
Position/Distance/Rolloff`, `Play/Stop/PauseSource`, `SourceStatus`, `SourceSampleOffset`, `SourceSecondOffset`,
`SourceBuffersProcessed`, `QueueSourceBuffer`, `UnqueueSourceBuffer`); búferes (`CreateBuffer`, `SetBufferLoopPoints`
con `AL_SOFT_loop_points`, `DeleteBuffer(s)`). Es el único archivo que incluye OpenAL y el único `alCheckCall`. El cambio
de ejes (x ↔ z, mundo de openblack zurdo, OpenAL diestro) se hace solo aquí: antes estaba repetido en `AudioPlayer`,
`AlSampleOutput` y `MusicStream`. Usuarios: `AlSampleOutput` (16 canales; voces y consejeros van por ellos),
`WaveBuffers` y `MusicStream` (6 pistas).

**Bancos** (`src/Audio/Banks.{h,cpp}`, `audio::banks`, capa 1, LHBankRegister 0x10002240): el registro
(`RegisterBank`, `SetBankSampleCount`, `BankSampleCount`, `Bank(SfxBank)` = GAudio+0x3A8 + 4·tipo de 0x9CB3F8,
`FindBank`, `BankGroup`, `SampleId`, que estaban en `AudioSystem`) y la carga:
- `banks::LoadAll()`, al final de `audio::Init` (GAudio ctor 0x426D40 → fn_00429CB0, InitAtmos fn_00428F30): cada .sad
  de `Audio\` en el orden del sistema de ficheros, como el bucle que había en `Game.cpp`. Mismo contenido: tablas de
  anim-effects (`anim_effects::RegisterTables`, 0x10002778..0x100029AB), nombres de onda de la tabla de voz (bancos 6, 7,
  10), muestras vacías saltadas (`continue`), bancos de música (ondas .mpg) fuera.
- Los de diálogo (tipos 6..10) siguen **perezosos**: solo cabeceras y `banks::ReadWave` lee la onda al primer uso
  (0x10011420 → fn_100032D0; antes `wave_buffers::ReadWave`).
- `banks::MusicBankOf(MusicType)` (0x9C9748, GAudio+0x2C + 4·tipo) registra cada banco de música al primer uso (antes en
  `MusicStream.cpp`); `music::GetBank` se lo cuenta al motor; `banks::ReleaseMusicBanks()` en `music::Shutdown`.
- `LHAtmos` (`AtmosBanks::Register`) busca sus 14 bancos con `FindBank("/<archivo>.sad")` y lee sus muestras con
  `banks::Samples`.
- `AnimEffectTable::Load(path)` solo lee el fichero para un banco no registrado (herramientas y tests).

**Panel de depuración** (`src/Debug/Audio.cpp`): la lista de bancos sale de `audio::banks`; la pestaña «Channels» muestra
el dispositivo (LHWaveIsInstalled), los 16 canales, los consejeros (`advisor::Speaker/Sentence/IsTalking/SentenceTime`,
dueño 0x270C) y las 6 pistas de LHMusic (estado +0x24, banco +0x58, trozo +0x48/+0x4C, volumen +0x34 → +0x30).

**Comprobación** (Land 1, 1800 fotogramas, las cinco trazas `OPENBLACK_AUDIO/SFX/MUSIC/ANIM/TEXT_TRACE`, el mismo
`Mods\`; logs `_audit\audio\b11a_base.log`, segunda pasada, y `b11a_after.log`): mismas líneas de arranque (Atmos 15
bucles y 400 sueltas, tabla de voz 6974/1922/1328/227, dos muestras vacías, WELCOME_DANCE sin fichero), la misma música
(`intro.sad`, MUSIC_TYPE_SCRIPT_INTRO) y los mismos tipos de evento; las diferencias son de número (sorteos de
anim-effects y unos turnos menos en la segunda pasada, que depende del tiempo real). Sin errores de OpenAL.

## Qué suena y cuándo

Cada página de tema dice qué suena y cuándo. Aquí solo está el motor:
- Coger y soltar, montones, vasijas: [objects-and-resources.md](objects-and-resources.md#sonidos-informe-tmp_dissoundnotestxt).
- Sonidos de los clips de animación y el banter: [animation.md](animation.md#sonidos-de-los-clips).
- Golpes, choques y lanzamientos: [physics.md](physics.md#sonidos-polvo-y-aspecto-de-los-golpes).
- Farolas (SoundTag, de noche): [day-night-weather.md](day-night-weather.md#luces-de-noche-informe-night_visualstxt).
- Árboles: hojas en [trees.md](trees.md#dibujado), caída en [trees.md](trees.md#soltar-y-replantar).
- Partículas de los milagros (SOUND_ACTION, PSysSound, spells.sad): [particles.md](particles.md#sonido-de-las-partículas-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp).
- Agua (la mano en el agua, golpes, ahogarse, barco, cascada y arca, ambiente del mar, la costa y los lagos):
  [water.md](water.md#audio-del-agua). Hechizos: [particles.md](particles.md) y las páginas de Milagros.
- Inventario completo de los efectos del original (cada llamada, banco y muestra): `tmp_dis\audio\sfx_inventory.md` y
  `sfx_inventory_tables.md`. Interfaz y criatura: `ui_creature.md`.

## Pendiente

- **Fase C** ([arriba](#fases-b-y-c)); lo que queda de B9/B10 está [en su sección](#aproximado-inferido-y-pendiente-de-b9b10). Lo que queda de B7 está [en su sección](#aproximado-inferido-y-pendiente-de-b7).
- **B4/B6, (inferido)/(aproximado)** (todos con su comentario en el código):
  - `PlayAt` de los árboles sin el árbol de dueño (la firma no cambia) **(aproximado)**; para arboles: el original pasa
    el árbol (0x74C4D4) y elige con `GetTickCount() % 9` (0x74C4B3), `Trees.cpp` usa el azar;
  - `audio::TickCount` = milisegundos del reloj del proceso como `GetTickCount`; el montón lee el reloj una vez y no
    dos (0x66D1D7 / 0x66D1E7) **(aproximado)**;
  - el grito al coger un aldeano: vivo = `LifeOf > 0` (una entidad válida se toma disponible) **(inferido)**;
  - la distancia de los choques sale de la cámara de las `GameQueries` (`LH3DTech::g_camera`) **(inferido**, como en B2);
  - el woosh de `SetFlight` mide desde el origen actual de la cámara del fotograma anterior (FlyToPosFoc lee la
    posición de la cámara, +0x118) **(inferido)**;
  - `SOUND_EXISTS` = dispositivo OpenAL **(aproximado)**; bancos fuera de 1..10 y textos fuera de la tabla de voz no
    suenan ni paran **(aproximado)**.
- **B4/B6, pendiente**: el volcán y el vapor; `G_OutOfBounds` 43 (fn_0045FA00, no hay límites de cámara); los demás
  llamadores originales de `tags::Create`/`Remove` (molino, taller, tótem, credo, caída de árboles: C3); B5 (Milagros:
  `HandSpellSeed` incluye `AudioManagerInterface.h` solo por su bucle de semilla); la traza `OPENBLACK_SFX_TRACE` no
  dice la dirección del sitio original (se compara por banco, muestra, 2D/3D y modo con `sfx_inv.json`).
- **B2/B3, (inferido)/(aproximado)** (todos con su comentario en el código):
  - `IsInScript` (vt +0x448) siempre falso: openblack no tiene aldeanos de guion **(inferido)**;
  - `GameThing::IsFunctional` y `Get3DSoundPos` ≠ 1 de la cosa de un tag = la entidad ya no tiene posición **(inferido)**;
  - [0xD01A38] = 100 ms por turno en `CheckDelay` **(inferido**, `villager_anims.md`);
  - el punto de un tag sin cosa en un arranque nuevo de un anim-effect: el original lee el +0x50 del canal recién
    asignado (0x427209, antes de que LHSamplePlay escriba el punto), un valor viejo; openblack da el punto del tag
    **(aproximado**; ningún llamador arranca un anim-effect con un tag de dueño);
  - las vueltas del canal (+0x40 de `LHSampleGetInfo`) son las del arranque y 0 tras `ReleaseLoop` (no se lee el contador de pasadas del DLL) **(inferido)**;
  - `LH_AudioSystem::Rand(n)` con Rand() = 32767 daría n (una más allá de la lista): se queda dentro **(aproximado)**;
    `GRand::LocalRand` de `RandomSample` con el generador de openblack **(aproximado)**;
  - las farolas reciben su tag en el `ProcessTurn` siguiente a crearse (no hay gancho de `CallVirtualFunctionsForCreation`) y ninguna tiene la marca UNAVAILABLE **(aproximado)**;
  - `PlayFromTable` no tiene argumento track: el sitio (doblar 0 / susurro 1) se distingue por el soundId de la clave (openblack).
- **B2/B3, pendiente**: `SpellSounds` por `SamplePlayAnimEffect` (B5); el tag de punto de ambiente fn_0071E920 (el
  trueno de GWeather, ver B3).
- **Auditoría B2-B3** (2026-10-01): comprobadas en el desensamblado 0x42A4B0, fn_00516510 (0x5165BC, 0x5166A4,
  0x51675D, 0x5167A8), Tree::Draw 0x74AFE1/0x74B1FA, 0x10014A20, 0x100146F0 (0x1001491C/0x10014990), 0x10015710,
  fn_10014610, 0x71E300, 0x71E4F0, 0x71E5F0, 0x71E640, fn_0071E680, 0x71E760, 0x71EA40, 0x71EB60, 0x71EBE0/0x71EC30,
  0x71EC90, 0x71ECB0/0x71ECD0, 0x71ED40, 0x734920/0x734965: cuadran. Corregidos solo comentarios (la forma de ambiente
  fn_0071E920 que faltaba, la escala de MapCoords, el punto viejo de 0x427209). Nota para arboles: `Trees.cpp` pasa
  la columna 1 de la clave = 2 y el original 0 (ebp = 0, 0x74AB6A); sin efecto audible, todas las filas de editor.sad
  tienen comodín en esa columna.
- **Auditoría B4-B6** (2026-10-01, commit 598b6dbd): comprobadas en el desensamblado 0x70F7F0 (POPs y campos +0x04
  / +0x08 / +0x0C / +0x20 / +0x24 / +0x30..+0x38 / +0x164), 0x70FA50 (isSay, tabla 0x942B3C/+0x40, 0x270C/0x270E/0x270D),
  0x710150 / 0x7101D0 (orden de POPs y argumentos de SoundTag::Create / Remove), 0x5D2800 (0x5D2881..0x5D295D: tag 10,
  gritos 180/194/187 + rand 7 con IsAlive 0x402610), 0x74B730, 0x74BC60, 0x63AA13, 0x6E74B8, 0x458967, 0x45E119..0x45E305,
  0x645BEE, 0x406511 (B = ebp = 0x10 de 0x40626C) / 0x406640, 0x66D1A0, 0x5D1933, 0x68F9E8, 0x5D1FC4, 0x5E0413: cuadran.
  DeadTree hereda de Rock (bw1-decomp DeadTree.h), no de Tree: no es IsTree y suena al cogerlo. Corregido solo un
  comentario de la cámara (la bandera del woosh del doble clic es [esp+0x23], no [esp+0x4B]; el caso [esp+0x4B], distancia
  1000, queda pendiente). En juego (`_audit\audio\audit_b4b6.log`): PLAY_SOUND_EFFECT(49/50/54, 5, punto, 1) con la cámara
  al lado → Scriptsfx 3D, track 0, dueño `key 0x31/0x32/0x36`, en canal. (Aproximado, sin cambio audible) los tags de
  recoger y arrancar usan el punto de la Transform, el original GetAltitude + la altura de su MapCoords (0x71EB60).
- **Auditoría B7** (2026-10-01, commit 6a67dd03): comprobadas en el desensamblado 0x5C36D0 (retardo |+0x3514| − 0,95
  doble 0x915438, (v+1)·250, tope 500), 0x5C3750, 0x5C3780 (+0x7C = ebp, 0x900D48 = otra copia idéntica de 0x915D40,
  0,9 doble 0x915440), 0x5C5290/0x5C52C0, 0x5BB340, 0x5BB530, 0x5BB610, 0x5BB730/0x5BB760/0x5BB7C0/0x5BB840,
  0x5BCD00, 0x428850 (bandas, la banda más fuerte, límites dt·rate·0,18 y el doble), 0x428A80, 0x428C60, constantes de
  four1 (0x8C49F8, 0x8AB260, 0x8C49F0), init 0x5C1EA1..0x5C1F24, 0x5C6025..0x5C60DB, 0x5C62F0, 0x5C6340, 0x70F8E0,
  0x70F9B0, 0x710280, 0x710C40, 0x710350 (tabla 0x710400), 0x5C6E20, 0x5C6A7E..0x5C6AAD, 0x5C6720 → 0x5C68A0 →
  0x5C4C20 → 0x5C5250, y en el DLL 0x10012BF0, 0x10012C50, 0x10012DF0, 0x1001439D/0x100143BC, 0x10014C00, 0x10015180:
  cuadran. Corregido: `advisor::Reset` decía ser «HelpSystem / el cambio de mapa», sin fuente (HelpSystem::Reset
  0x5C5580 no toca HelpDudeControl): ahora es solo de los tests y ningún código del juego la llama; quitado el argumento
  por defecto de `PackFile::ReadBlocks`. Precisión al PLAN §4 B7 («el clic corta villagers pero no HelpSprites»): el
  clic corta villagers/0x270F con 0x42A210 y además **para al consejero que habla** (fn_005C3780 → fn_005C3750, sin
  frase de interrupción en W120); la narración 0x270F de HelpSprites (narrador ≠ 2/3) no se corta. Sin fugas: el
  consejero solo decodifica PCM (ningún búfer AL), `ReadWave` cierra su flujo; las voces no tienen entidad dueña y
  `ClearMap` → LHSampleStopAll las corta; el hilo de música no toca `sample_play`.
- **A8**: guardar `AudioMusicMasterVolume` y `AudioSampleMasterVolume`, y dónde va el deslizador. Pregunta 4 de PLAN §6.
- **A9 en juego**: falta quién da el alineamiento en la cámara (GAudio+0x190, fn_005E2240 desde fn_0064AC30) y la tribu
  de los pueblos (Town +0x5B8). Hoy suena la genérica neutral.
- **Música**:
  - quién pone ThingMusicInfo+0x20;
  - `LandNumber` 6 y `g_game+0x205A0C`;
  - si `GetDistanceInMetres` es 2D o 3D (0x74CCB0);
  - quién llama a `LHMusicPause`;
  - el grupo 0 (`pos[-1]`).
- **Bucles**: N o N+1 pasadas (pregunta 2 de PLAN §6); hoy N+1 **(inferido)**.
- **B1, lo que queda (aproximado/inferido)**:
  - un búfer por registro de muestra y no por onda; sin presupuesto RAM/8;
  - las opciones de trabajo de GAudio (+0x240) con los valores del ctor en cada variante (nadie más las escribe:
    **inferido**);
  - `OwnerUnavailable` = `GameQueries::thingPosition` vacío (la entidad no es válida o no tiene Transform);
  - los dueños `Key` con track no se mueven (las voces pasan track 0); los `Tag` siguen a su cosa desde B3;
  - minimizado, el juego sigue corriendo y el ambiente sigue sonando (en el original todo se para:
    `ProcessWindowMessages` no sale mientras `AltTabbedAway`);
  - `OnThingDeleted` está, pero Game no lo llama: las entidades llevan versión, así que `thingPosition` ya da vacío
    para una destruida en el turno siguiente;
  - en pausa, el original también hace GSoundMap::Update y ProcessSoundTags (el bit 4 lo pone PauseGame); openblack
    solo hace AtmosProcess(0), porque su pausa no tiene reloj de turnos;
  - g_game / HelpSystem nulos (0x429E37..0x429E4F) no se modelan: openblack siempre los tiene;
  - `LanternSounds` y `AnimationSounds` (llamadores) leen el ECS; el núcleo (`SoundTags`, `AnimEffects`) no;
  - `SamplePlay.cpp` y `AudioSystem.cpp` incluyen `AudioManagerInterface.h`, que arrastra el componente
    `ECS/Components/AudioEmitter.h` (la interfaz vieja): se va cuando B2..B5 retiren `CreateEmitter`;
  - `LHSampleIsPlaying(info)` con el audio apagado (0x1001407A) y los ejes fijos de `LHSampleSet3DPosition`
    (0x10013BCC): ver la auditoría de B0-B1;
  - estéreo en 3D: OpenAL no espacializa los búferes estéreo.
- **B1, sin hacer**: guardar `AudioSampleMasterVolume`; retirar `AudioManager::PlayMusic`/`PlaySound`/`CreateEmitter`
  públicos (cuando B2..B5 muevan sus llamadores).
- **Texto**:
  - los valores 8/5 de info.dat;
  - GUIDE y MONK;
  - `GRand::LocalRand(14)`: 0..13 o 0..14 (0x6DE570);
  - la cámara de guion (CameraModeScript 0x461180, CameraModeNew3, FOV de salida) y los consejeros de
    `SpiritHome`;
  - la tecla [0xE85410];
  - el dibujo del texto (HelpText fn_005CCED0).
- **Sin volcar**: los disparadores de Guidance (PLAN §8.3 F4), el Soundex,
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
| `OPENBLACK_AUDIO_TRACE=1` | Cada arranque, robo, parada y corte de canal (`Sample play:`), cada búfer creado (`Wave buffer … N made`), los cambios de la pantalla ancha del guion, los anim-effects rechazados por distancia (`Anim effect: banco/n (onda) too far`) y las trazas viejas de `AudioManager` |
| `OPENBLACK_ANIM_TRACE=1` | Los sonidos de los clips y de los árboles (`Animation sound: clip … -> editor.sad/n`, `key … -> editor.sad/n`, `no row`, `banter n too far`), con el mismo formato que antes de B2 |
| `OPENBLACK_SOUND_TAG_TRACE=1` | Cada tag creado, borrado, soltado o con retardo, y cada 50 turnos el canal de cada tag de cosa |
| `OPENBLACK_SFX_TRACE=1` | Una línea `SFX:` por llamada a `GAudio::PlaySoundEffect` (y a los tags), a `SamplePlayAnimEffect` y a `StopSoundEffect`: banco/muestra (onda), 2D/3D, track, punto, modo y vueltas con que arranca, tono, dueño y qué pasó (canal, `culled`, `filtered (motivo)`); y los `PLAY_SOUND_EFFECT(...)` del guion |
| `OPENBLACK_AUDIO_TEST_VIEW="turno,n[,distancia]"` / `OPENBLACK_AUDIO_TEST_ANIM=<clip>` | En ese turno la cámara mira al aldeano n desde esa distancia (4), y todos los aldeanos tocan ese clip en bucle (437 bostezo, 354 sierra, 369 sentado) |
| `OPENBLACK_AUDIO_TEST_LANTERN="turno[,distancia]"` | En ese turno la cámara mira la punta de la primera farola desde esa distancia (3) |
| `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` | El audio no ve la pantalla ancha del guion (la intro de Land 1 la tiene hasta un clic), para comparar sin ese filtro. No es del original |
| `OPENBLACK_TEST_SAMPLE_VOLUME=<0..127>` | El maestro de efectos al arrancar |
| Pestaña «Channels» del panel de audio | Maestro de efectos (deslizador), LHWaveIsActive, búferes vivos/creados y los 16 canales (muestra, banco, dueño, prioridad, volumen, tono, 3D/track/ambiente, sonando) |
| `OPENBLACK_TEXT_TRACE=1` | Cada texto de RUN_TEXT/TEMP_TEXT en el log (`|` por cada salto de línea) |
| `OPENBLACK_AUDIO_TRACE=1` (voces) | `Advisor: dude n says HelpSprites m (s)` al arrancar cada frase de consejero; `Wave of … read from <banco>` cuando se lee una onda de un banco de diálogo |
| `OPENBLACK_TEST_TEXT_CLICK=1` | Cada turno, si un texto espera el clic, hace el clic izquierdo (`HelpSystem::ProcessInterface(true)`); ver map-loading.md |
| `OPENBLACK_TEST_BW_ROOT=<instalación>` | Para los tests con datos: `test_audio_tables`, `test_music_bank`, `test_music_stream`, `test_game_music`, `test_voice_table`, `test_help_system`, `test_sample_play`, `test_anim_effects`, `test_sound_tags`, `test_script_sound`, `test_voices`, `test_spooky_voices` |
| `OPENBLACK_GUIDANCE_TRACE=1` | `Guidance:` por cada Init, PlaySample (tipo, texto o muestra, 2D/3D, banco/muestra, canal) y HelpSpiritSay (texto, tipo, guion, arrancado o no); `SpookyVoices:` en Init y PlaySpooky |
| `OPENBLACK_TEST_GUIDANCE_SAY=<turno>:<HELP_TEXT>` | En ese turno, `HelpSpiritSay(texto, 32)`: prueba el guion `MultiHelpJustTalkWithText` (no es del original) |
| `OPENBLACK_PLAYER_NAME=<nombre>` | El nombre de perfil que lee GSpookyVoices::GetName (openblack no tiene perfiles) |
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
- Código de la fase B: `src/Audio/{Audio.h, AudioSystem, GameSfx.cpp, SamplePlay, SampleOutput.h, AlSampleOutput,
  QMixerLaws, WaveBuffers}`, `AudioManager`, `AtmosBanks` (UpdateBanks/Mix), `Resources/Loaders.cpp` (+0x108, +0x124,
  +0x138/+0x13C), `Debug/Audio.cpp` («Channels»); test `test/test_sample_play.cpp`.
- B2/B3: `src/Audio/{AnimEffects, AnimEffectBank.h, AnimationSounds, SoundTags, LanternSounds}`, `AudioSystem`
  (`SamplePlayAnimEffect`, `Get3DSoundPos`), `SamplePlay` (`Random`, `Loops`, el punto del canal de un tag); tests
  `test/test_anim_effects.cpp`, `test/test_sound_tags.cpp`; volcados `tmp_dis\mapa\d_soundtag.txt` y los de
  0x42A4B0, 0x516510, 0x10014670 / 0x100146F0 / 0x10014A20, 0x10012C50, 0x10015710, Tree::Draw 0x74AFDD..0x74B25C.
- B4/B6: `src/Audio/{Audio.h (TickCount, SfxTrace, SoundExists), AudioSystem (traza), GameSfx, ScriptSound}`,
  `src/CHLApi.cpp`, los sitios de la tabla de B4; test `test/test_script_sound.cpp`; desensamblado de 0x5D2800,
  0x74B730, 0x74BC60, 0x63AA13, 0x5D1FC4, 0x6E7480, 0x74C460, 0x458967, 0x45E0C3, 0x645B6D, 0x646860, 0x406240 /
  0x406511 / 0x406640, 0x66D1A0, 0x70F7F0, 0x70FA50, 0x710100..0x710280, 0x426D30, 0x42A280, 0x402610, 0x402320.
- B7: `src/Audio/{Voices, Advisor, SamplePlay (PlayPosition, PercentageDone, la rampa), SampleOutput.h,
  AlSampleOutput (StopRamped, PlayPositionMs), WaveBuffers (ReadWave), Sound.h (waveFile), AudioSystem
  (BankSampleCount)}`, `src/Help/HelpSystem` (SpiritWhoTalks, ConvertScriptSpiritToHelpSpirit, el orden del clic),
  `components/pack` (`ReadAudioHeaders`), `src/CHLApi.cpp` (340, 458, 246), `src/Game.cpp` (bancos perezosos,
  consultas y ganchos); test `test/test_voices.cpp`.
- B9/B10: `src/Audio/{Guidance, SpookyVoices, GameQueries.h (sección B9), AudioSystem (Queries)}`,
  `src/Help/{HelpSystem (+0x45F4/+0x45F8, TriggerCategory, +0x560), ScriptControl (RunMessage,
  StopHelpScriptsForNewHelp)}`, `src/CHLApi.{h,cpp}` (200, 253, `ScriptVm`), `src/ECS/PotResource.cpp`,
  `HandResources.cpp`, `src/Game.cpp`; tests `test/test_guidance.cpp`, `test/test_spooky_voices.cpp`; volcados
  `voices_guidance_71ab10.txt`, `spooky_72e130.txt`.
- Código: `src/Audio/{BankTables.h, GameQueries.h, MusicBank, MusicEngine, MusicStream, GameMusic, ThingMusic,
  ScriptAudioState, Voices}`, `src/Help/HelpSystem`, `src/Common/HelpText`, `src/Debug/Music`, `components/pack`
  (`AudioBankInfo`), `src/CHLApi.cpp`, `src/Game.cpp`; tests `test/test_{audio_tables, music_bank, music_engine,
  music_stream, game_music, voice_table, help_system}.cpp`.
