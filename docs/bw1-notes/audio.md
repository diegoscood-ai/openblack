# Audio: el motor, los bancos, la música, las voces y el guion

Esta página explica cómo suena Black & White 1. Cubre el motor del original (GAudio en `runblack.exe` sobre LHaudiodllR
y QMixer), los bancos y sus formatos (.sad, .sas y la música MP2 en segmentos), la música (LHMusic y la parte de música
de GAudio), las voces y los textos, y las funciones CHL de audio. Para cada tema se dice qué hace openblack: la fase A
y los hitos B0..B4, B6 y B7 de la fase B están hechos; el resto de la fase B y la C quedan pendientes.
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

**Fases B0..B4, B6 y B7 hechas** ([B0-B1](#fase-b-b0-y-b1-implementados), [B2-B3](#fase-b-b2-y-b3-implementados),
[B4-B6](#fase-b-b4-y-b6-implementados), [B7](#fase-b-b7-implementado-voces-en-canal)). Hay un solo motor de canales: los 16 canales de
`audio::sample_play` (LHSamplePlay), cada uno con su fuente OpenAL propia y **fuera del registro ECS**, detrás de los
filtros de GAudio (`AudioSystem`) y de la API pública `src/Audio/Audio.h`. Desde B4 todo el mundo (mano, árboles, rocas,
cámara, física, edificios, barco, montones) y los CHL de efectos van por ahí. Siguen fuera de los canales, hasta B5,
los reproductores viejos de Milagros sobre `AudioManager::CreateEmitter`/`PlaySound` (está prohibido añadirles llamadores):
- `SpellSounds`, `FireSound`, el bucle de la semilla de `HandSpellSeed`, `Gesture`, `HandMagicFX`, `SpellSeed`,
  `WorshipSpellIcon`, `MagicTeleport`, `Fireball`, `OneOffSpellSeed` (B5, Milagros);
- el panel de depuración «Sound».
Esos también ganan los arreglos de B0 (un búfer por muestra, RIFF 0x50, puntos de bucle), porque comparten
`wave_buffers`.

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
  onda se lee del fichero al decodificarla (`Sound::waveFile`, `wave_buffers::ReadWave`), como `LHBankRegister(path, 0)`.
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
  16 canales. Los reproductores viejos (`AudioManager::CreateEmitter`: el fuego, las farolas, los árboles…) comparten el
  búfer, pero lo ponen en cola (`alSourceQueueBuffers`, fuente de streaming) y OpenAL Soft solo usa los puntos de bucle
  en fuentes estáticas: allí el bucle sigue siendo la onda entera, como antes (hasta que B2..B5 los pasen a canales).

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

`SpellSounds` (Milagros, B5) sigue con su `AnimEffectBank`, pero su `Load(ruta)` ya no lee el fichero: copia las tablas
que el núcleo leyó al registrar spells.sad (`test_anim_effects` `MiraclesBankCopiesTheCoresTables`: mismas filas, listas y
muestras). Su comportamiento no cambia.

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

## Fases B y C

**B0..B4, B6 y B7 hechos; el resto pendiente** (PLAN §4-5). Milagros y agua ya están fusionados en `local/hand-hbn`:

| hito | contenido |
|---|---|
| B0 | **hecho** ([abajo](#fase-b-b0-y-b1-implementados)) |
| B1 | **hecho** salvo: `MusicStream` sigue con su propio uso del contexto de `AudioPlayer`, `AudioManager::PlayMusic` no se ha retirado y el maestro no se guarda en disco |
| B2 | **hecho** ([arriba](#b2-los-anim-effects-en-el-núcleo)); `SpellSounds` copia las tablas del núcleo hasta B5 |
| B3 | **hecho** ([arriba](#b3-soundtag-completo)); faltan los llamadores del original (molino, taller, tótem, credo, caída de árboles: B4/C3) y ATTACH/DETACH_SOUND_TAG (B6) |
| B4 | **hecho** ([arriba](#b4-los-llamadores-del-mundo-en-los-canales)); faltan el volcán (`LandscapeVortex` 0x5FEE5A: openblack no lo tiene) y el vapor (`FireGraphic` 0x731542, Milagros/B5) |
| B5 | Milagros: SpellSounds, FireSound, gesto 3D, PlayTapSound, SpellDialogue por canal; PSys `AddSoundToAtom` 0x69DCA0… (F3) |
| B6 | **hecho** ([arriba](#b6-chl-de-efectos)); el ambiente (`GSoundMap` 0x71D6F0, LHAtmos 0x428FE0 / 0x100018B0) ya era de agua y va por `audio::` |
| B7 | **hecho** ([arriba](#fase-b-b7-implementado-voces-en-canal)); falta la parte visual de los consejeros (modelos, vuelo, boca) |
| B8 | Interfaz y mano: MenuButton 159, Logo 160, ClickOnSpell 42, conquista 205, orden aceptada 1, llamar a la puerta 110+c%9, influencia 52/129 (los gritos 180/187/194+rand7 ya están, B4) |
| B9 | GGuidance, BeliefSFX, latido |
| B10 | GSpookyVoices (antes hay que volcar el Soundex 0x72E4E0..0x72E870) |
| C1 | Criatura: cola de eventos, clave de 5 columnas, bancos por especie, filtro de jugador local / SET_CREATURE_SOUND; baile y pelea en GameMusic |
| C2 | Clima y alineamiento en el ambiente |
| C3 | Aldeanos, edificios y cánticos |
| C4 | Ciudadela interior y `ProcessCitadelMusic` |
| C5 | Vídeos (tráiler, `PlayFullScreenMovie`) |
| C6 | Guardar y cargar: `GAudio::Save` 0x428310 / `Load` 0x428480, `ThingMusicInfo::Save` 0x429950 / `Load` 0x429AE0, `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0 |
| C7 | GConfirmation (necesita `CameraModeNew3` 0x454900/30) |

Mientras tanto, se prohíbe añadir llamadores nuevos a `AudioManager::PlaySound`/`CreateEmitter` (solo los usan los
archivos de Milagros hasta B5).

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

- **Fases B (B5, B8..B10) y C** ([arriba](#fases-b-y-c)). Lo que queda de B7 está [en su sección](#aproximado-inferido-y-pendiente-de-b7).
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
| `OPENBLACK_TEST_BW_ROOT=<instalación>` | Para los tests con datos: `test_audio_tables`, `test_music_bank`, `test_music_stream`, `test_game_music`, `test_voice_table`, `test_help_system`, `test_sample_play`, `test_anim_effects`, `test_sound_tags`, `test_script_sound`, `test_voices` |
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
- Código: `src/Audio/{BankTables.h, GameQueries.h, MusicBank, MusicEngine, MusicStream, GameMusic, ThingMusic,
  ScriptAudioState, Voices}`, `src/Help/HelpSystem`, `src/Common/HelpText`, `src/Debug/Music`, `components/pack`
  (`AudioBankInfo`), `src/CHLApi.cpp`, `src/Game.cpp`; tests `test/test_{audio_tables, music_bank, music_engine,
  music_stream, game_music, voice_table, help_system}.cpp`.
