# Music

## Summary

The game's music follows the player. Over the land, it plays the music of the player's alignment (evil, neutral or
good), or the tribe of the town the camera is near. It changes to the citadel's music, to a chant at a dance, to
fight music when the creature fights, or to the music that a script or an object asks for. When one music changes to
another of the same family, the new one carries on at the same point of the melody instead of starting again. After a
piece of alignment music ends, the game stays silent for a while.

Two layers do this in the original. **LHMusic**, in the audio DLL, plays up to six tracks of MP2 segments: the
**master** (the newest track) fades in while the others fade out, and it handles synchronised changes and loops.
**GameMusic**, in the exe, feeds it: each turn `ProcessMusic` chooses between the script's music, the objects with
music, the citadel, the alignment music, the fights and the dances, from the 85 MUSIC_TYPE banks. The intro, the
trailer, the outro, the films and the menu have their own rules.

openblack has the engine on its own music thread and GameMusic's choices on the game turn, the creature's dance
included (ported from raffclar's tree and checked against the exe). The music volume
is set in the options box and kept for the computer, as the original keeps it.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Music

The music has two layers: the engine LHMusic in the DLL, which plays the tracks, and GameMusic in the exe, which
chooses what plays. The engine comes first, then the table of music banks, then GameMusic, then the special cases.

#### LHMusic: the 6-track engine

**Faithful.**

##### Channels and their states

- There are **6 channels** `LH_MusicInfo` (sys+0x84, 6 × 0x6C). A 7th track is rejected.
- A channel's status is 0 free, 1 playing, 2 paused, 3 finished, 4 last chunk queued.
- The **master** (0x10056284) is the last track started. It rises to its target, and all the others go down −3 per
  pass until 0 and are released.

##### Setup

The music part of the `LH_AudioSystem` init (0x1000DD50):

- The channels are malloc(0x288) at 0x1000DDD4.
- Each channel is free, with .sad volume 127 and its QMixer channel enabled in 2D (0x1000DDF5..0x1000DF13; the
  enable is at 0x1000DEF1).
- The master volume is 127 (0x1000DE08), and there is no master channel (0x1000DDFE).
- The music is marked installed (+0x1C) and active (+0x24) (0x1000DF43 / 0x1000DF46).
- The **"music" thread** 0x1000EB40 is launched with `_lhbeginthread("music", 0xF)` at 0x1000DF27.

##### The music banks

A music bank is registered with `LHBankRegister(path, 0)` 0x10002240:

- It makes a new LH_AudioBank and calls LHReleasedFile::Open(path, 2) (0x1000228A..0x100022FE). A failure logs and
  returns 0.
- It returns 0 when the file is missing, or has no LiOnHeAd, no LHFileSegmentBankInfo 0x10002368, no
  LHAudioWaveData or no LHAudioBankSampleTable. The early returns of the system itself (sys+8 or sys+4 null,
  0x1000226A/0x10002275; path null, 0x10002284) belong to the caller.
- LHAudioWaveData (0x100023C0..) is read with inMemory = 0, so bank+0 = 1 and the data is read from the file when it
  is used.
- The music engine's group count is updated at 0x100027AB..0x100027C7: sys+0x40 = max(sys+0x40, u16 @+0x118 of the
  first segment). So it is the maximum GetGroupId() of the registered music banks.

What the engine reads from a bank:

| item | where it is read | what it holds |
|---|---|---|
| is a music bank | `LHIsMusicBank` 0x10002EE0 | bank+4 = the 3rd u32 of LHFileSegmentBankInfo (0x100023BD); non-zero = music |
| segment count | `LHBankGetNumberOfSamples` 0x10002E90 | bank+8 = the low u16 of the table's first u32 (the high one, bank+0xC, counts the atmos samples and is 0 in music banks) |
| group | `LHBankGetMusicGroupId` 0x10002F30 | u16 @+0x118 of the first segment, −1 if not a music bank (0x10002F4A) |
| flags | `LHGetFirstBankSample` 0x10002EB0 (bank+0x14) | u32 @+0x244 of the first segment (see MusicBankFlag); read as a byte at 0x1000E1CC and as a dword at 0x1000E30A |
| sample rate | 0x1000E210 | u32 @+0x128 of the first segment: `LHMusicPlay` copies it to channel +0x50, the Hz of every chunk |
| volume | 0x1000E0C5, 0x1000E1D6..0x1000E1ED | channel +0x2C = 127 (0x1000E0C5), or with flag 0x20, u32 @+0x25C & 0xFFFF (0x1000E1DB..0x1000E1ED) |
| loops | 0x1000E13E, 0x1000E1F1..0x1000E206 | channel +0x10 = the requested loops (opts+0x18, 0x1000E13E), or with flag 0x40, i32 @+0x248 (0x1000E200) |
| distance mapping | 0x1000E2D9..0x1000E338 | read in this order: 0x100 max, 0x80 min, 0x200 scale. Max = f32 @+0x26C (0x1000E310, else opts+0x30); min = f32 @+0x268 (0x1000E31F, else opts+0x2C); scale = f32 @+0x270 (0x1000E32D, else opts+0x34) |

The markers are parsed at 0x1000D9E0:

- For i = n-1 .. 0 (0x1000DA04, 0x1000DB04): desc = record +0x140.
- Only if desc[0] == '!' (0x1000DA20), repeat: skip the '!', take the digits (isdigit 0x1001F64B, _DIGIT mask 4) ->
  sample = atoi (0x1001F47D); skip ONE character (the '=', 0x1000DA87); label = up to '\0' or '!'
  (0x1000DA92..0x1000DAA2); the node {next = head, sample, chunk = i + 1, label} becomes the head
  (0x1000DAE5..0x1000DAED). This goes on while the label ended at a '!' (0x1000DAF0).
- The node holds the label in 0x20 bytes (malloc 0x30, label at +0x10); the data has no longer label.

##### The play options

`LH_MusicPlayOptions` (0x50 bytes, ctor 0x1000D970):

- +0x00 bank, +0x04 volume (127, 0x1000D982), +0x08 / +0x0C / +0x10 copied to channel +0x18 / +0x1C / +0x20 (0 from the ctor, no game caller sets them and
  nothing reads the channel's copies, so openblack leaves them out, as raffclar's MusicPlayOptions does);
- +0x14 start chunk (1-based; 0 or more than the segments → 1, 0x1000ED6A..0x1000ED77), +0x18 loops, +0x1C sync,
  +0x20 fade, +0x24 is3D, +0x28 pitch;
- +0x2C / +0x30 / +0x34 min / max / scale (10, 100, 1), +0x38..+0x40 position, +0x48 the marker callback.

##### Starting a track: LHMusicPlay

**`LHMusicPlay`** 0x1000DF60:

- It returns 0 when the music is not installed (0x1000DF6C..0x1000DFC6). Without a music bank (no bank, or not a
  music bank), it does `LHMusicStop(1)` (0x1000DF90) and returns 0.
- If the bank is already playing on a channel (0x1000DFD4..0x1000DFEF), it is a **re-trigger**: it only changes the
  target, the fade, the sync and the master. The master takes the new fade, target and sync and plays again
  (0x1000E00D..0x1000E04A). An older channel becomes the master, and the others fade out (0x1000E06F..0x1000E0A5). A
  channel that has not queued its first chunk yet is left alone (0x1000E07E).
- Otherwise, it takes the first free channel (0x1000DFF1..0x1000E008). If there is none, it does not play.
- The new channel's volume comes from the bank (see the table above). Without a fade the channel's volume is the
  target at once (0x1000E0DA..0x1000E0F6, 0x1000E0E1).
- It reads the bank's group (`LHBankGetMusicGroupId`, 0x1000E117) and segment count (`LHBankGetNumberOfSamples`,
  0x1000E18C), and clears the status saved by a pause (+0x28 = 0, 0x1000E1A1).
- It sets up the QMixer channel (0x1000E24C..0x1000E3F7): 3D with the bank's distances over the options', or 2D. 2D
  is flag 0x100 (0x1000E3F7); 3D is 0x110 / 0x111 | 0x100 (0x1000E2D1), with QSWaveMixSetDistanceMapping (0x1000E355)
  and QSWaveMixSetSourcePosition (0x1000E38A). In 3D QMixer's distance law is applied.
- The channel becomes status 1 (0x1000E437).
- It asks for a fresh decoder: bank+0x138 +0x58 = 1 (0x1000E43B..0x1000E448).
- With a marker callback, it makes a new marker list whose clock starts at max(startChunk, 1)
  (0x1000E472..0x1000E4E3).

##### The music thread: one pass

One pass of the music thread 0x1000EB40 walks the 6 channels at GetTickCount() = nowMs, and then does `Sleep(120)`
(0x1000F726), or `Sleep(5000)` if a read failed (0x1000F71F). A failed read stops the pass there
(0x1000EE64 -> 0x1000F6FA, 0x1000F6FA..0x1000F728). For each channel, in order:

1. Only the master's markers are dispatched (0x1000EBBA..0x1000EBEE).
2. A channel in status 3 is freed and its end callback runs with the user data, and the master becomes −1
   (0x1000EBF3..0x1000EC94, from 0x1000EC05). QSWaveMixIsChannelDone (0x1000EC5A...) and QSWaveMixFlushChannel
   (0x1000EC77...) are called here.
3. Chunks are queued (below).
4. The fades are applied (below).

###### Queueing chunks

- Queueing (0x1000EC9E..0x1000F4D5) happens only for a channel in status 1, while the music is active and fewer than
  4 chunks are queued (0x1000EC9E..0x1000ECD8, 0x1000ECC1). The loop at 0x1000F4CD goes back to the active check
  while fewer than 4 are queued. So the thread queues up to **4 chunks** per channel (0x1005628C).
- **Group sync** (0x1000ED03..0x1000ED7E, 0x1000F385): with `sync`, the start chunk comes from the first other
  channel in status 1 of the same group (the chunk read at 0x1000ED34..0x1000ED38). Only group −1 (not a music bank)
  is left out (0x1000ED13..0x1000ED1E): two banks of group 0 sync too, and then the start chunk is not used. Without
  such a channel, the start chunk is used if it is 1..n (unsigned compare), else 1 (0x1000ECF3..0x1000ED8B).
- The first chunk of a synced channel starts at the other channel's play position (`QSWaveMixGetPlayPosition`, used
  as lStart at 0x1000F385), and the channel becomes the master (0x1000F337..0x1000F3A8). So the new track starts at
  the **same chunk and the same sample** as the other track, and the good↔evil or tribe change does not restart the
  melody.
- After the last segment: with loops 0 the channel goes to its last chunk (status 4); otherwise the loops drop by one
  and it goes back to chunk 1 (0x1000EDB6..0x1000EE19).
- Each read is malloc(size + 4) (0x1000EE23). It reads size bytes at LHAudioWaveData + offset from the open file
  (0x1000EE19..0x1000EE64); a failure ends the pass.
- Each segment is decoded with the DLL's own MPEG decoder (0x1000F740), frame after frame up to the end of the
  segment's bytes. The "overrun detected" assert of 0x1000F884 guards the output buffer.
- The decoder state is continuous between chunks. It is only reset when `LHMusicPlay` asks for it (0x1000E448): the
  next read rebuilds it (0x1000F0CF..0x1000F1C5). The state is cleared and set up again from the next segment's first
  header (0x1000F0DA..0x1000F1C5), and the request is cleared (+0x58 = 0, 0x1000F0D4..0x1000F0E1). The decoder belongs
  to the bank (LH_AudioBank +0x138 +0x58), and a bank plays on one channel at a time.
- The channel's WAVEFORMAT is built from the MPEG frame header at 0x1000FA34..0x1000FA9D.
- Before each chunk: a 2D channel is centred with QSWaveMixSetPolarPosition(0, 0, 0) (0x1000F3F0); the frequency is
  set to Hz · pitch / 100 (0x1000F3F6..0x1000F41D) with QSWaveMixSetFrequency (0x1000F41D, 0x1000E9EF); the volume is
  set (0x1000F423..0x1000F472).
- The chunk is queued with QSWaveMixOpenWaveEx 0x1000F2FF + QSWaveMixPlayEx(0x400 = queue) 0x1000F4C4, starting at
  lStart (QMIXPLAYPARAMS lStart, 0x1000F395). The callback data +4 says whether it is the last chunk (0x1000F326).
  QSWaveMixPlayEx is given the QMixer callback 0x1000DC80 (0x1000F314), which receives the end of each queued chunk.
- The start tick of the marker clock is taken when the first chunk is queued (list +0x10, 0x1000F48D).

###### Volume

- The QMixer volume of one channel is `floor(floor(cur·sad·258/127)·master/127)` (0x1000F423..0x1000F464), that is,
  ((cur * sad * 258) / 127) * master / 127. The gain is cur/127 · vol.sad/127 · master/127.
- It is done in unsigned 32-bit arithmetic, with the magic 0x2040811 for each /127 (0x1000F429..0x1000F464). The same
  code is at 0x1000E8F7 and 0x1000F53D..0x1000F664.

###### Fades

- Fades (0x1000F4DB..0x1000F6E2) run only for channels in status 1 or 4 while the music is active
  (0x1000F4E8..0x1000F4FF).
- The master rises **+4** per pass (0x1000F59F; the last step is clamped at 0x1000F5A2, so 0→127 takes 127 / 4
  rounded up) only if opts+0x20 asks for it. Otherwise, it jumps straight to the target volume, once, the first time
  (0x1000F534).
- The other channels, or the master above its target, go down **−3** (0x1000F613). A step down is max(cur − 3, 0):
  towards 0, not towards the target (0x1000F613..0x1000F620).
- Durations: 0→127 in 32 passes (≈ 3.8 s), 0→80 in 20, 127→0 in 43 (≈ 5.2 s) and 80→0 in 27. The passes are
  faithful; the real duration (120 ms plus the processing time) is **🔍 inferred**.

###### End of a chunk

- The **end-of-chunk callback** 0x1000DC80 advances the audible chunk (+0x48, wrapping after the last segment,
  0x1000DCF6..0x1000DD04) and, on the last one, moves the channel to state 3.
- QMixer seems to call it for each flushed chunk inside QSWaveMixFlushChannel, before it returns: `LHMusicClose`
  spins on the queued count right after the flush (0x1000E854..0x1000E869), so QMixer must notify inside the flush
  (QSWaveMixFlushChannel 0x18001810 -> 0x18006294 with notify = 1 -> 0x1800A380). This is **🔍 inferred** from the
  spin.

###### Markers

The marker clock 0x1000DB90:

- samples = ftol((GetTickCount − start) · Hz · 0.001f) ([0x10030420]); chunk = samples / 0x5E80 + the base chunk;
  sample = samples % 0x5E80 (0x1000DBD9..0x1000DC1D).
- The position (chunk << 32) | sample is compared as a 64-bit value (0x1000DC13..0x1000DC52), and a marker fires when
  previous < marker ≤ now (0x1000DC38..0x1000DC63).
- The cursor moves to the last node of an earlier chunk (0x1000DC2E).
- The list is a 0x18-byte header (0x1000DB30) at channel +0x68.

##### Stopping, pausing and closing

- `LHMusicStop(fade)` 0x1000E530: with 1 they all go down. It sets each target to 0 (0x1000E57A), the thread fades
  them out, and it clears the master (0x1000E5E6..0x1000E5EF). With 0 they are cut (0x1000E57F..0x1000E5F3).
- `LHMusicStop(info, fade)` 0x1000E620: for the master, it stops all of them (0x1000E6B2). A fade on a channel that is
  not the master does nothing (0x1000E6C1). The cut of one channel is 0x1000E6C3..0x1000E730 (the master index is not
  touched here).
- `SetMasterVolume` 0x1000E890: the same value does nothing; above 127 gives 127 (an unsigned compare, 0x1000E8C6).
  The new volume is reapplied to the channels at status 1 or 4 (0x1000E8ED..0x1000E8F5).
- `LHMusicGetMasterVolume` 0x1000E950 returns −1 when the music is not active. `LHMusicIsInstalled` 0x1000EAE0 and
  `LHMusicIsActive` 0x1000EAF0 read the two flags.
- `LHMusicSetPitch` 0x1000E970 clamps the pitch to 50..250 (0x1000E9AC / 0x1000E9BB).
- `Set3DPosition` 0x1000FBA0 calls QSWaveMixSetSourcePosition (0x1000FBE8).
- Pause and restart are QSWaveMixPauseChannel / QSWaveMixRestartChannel (0x1000EA46 / 0x1000EAB6). Channel +0x28
  holds the status from before `LHMusicPause` (0x1000EA5C).
- `LHMusicClose` 0x1000E7A0: the thread is told to end and waited for (0x1000E7C1..0x1000E7DB). Then the channel part
  (after the thread has ended, 0x1000E7BC..0x1000E7DB): every target to 0, volume 0 and flush; the DLL then spins
  until nothing is queued (0x1000E854); the music is no longer installed (0x1000E870).

##### The functions

| function | address | what it does |
|---|---|---|
| `LHMusicStop(fade)` | 0x1000E530 | with 1 they all go down; with 0 they are cut |
| `LHMusicStop(info, fade)` | 0x1000E620 | for the master, stops all |
| `GetInfo` | 0x1000E750 | |
| `GetStatus` | 0x1000FC00 | |
| `GetCurrentChunk` | 0x1000FB40 | |
| `GetMasterInfo` | 0x1000FB70 | |
| `GetTotalGroups` | 0x1000FB60 | |
| `SetMasterVolume` | 0x1000E890 | unsigned: above 127 gives 127 |
| `SetPitch` | 0x1000E970 | between 50 and 250 |
| `Set3DPosition` | 0x1000FBA0 | |
| `Pause` / `Restart` | 0x1000EA10 / 0x1000EA80 | |
| `Switch` | 0x1000EB00 | |
| `Close` | 0x1000E7A0 | |

#### MUSIC_TYPE table

**Faithful**. It is table 0x9C9748: 85 entries `{path, "MUSIC_TYPE_…"}` of 8 bytes, up to 0x9C99F0, at GAudio+0x2C + 4·type.
The ctor registers them all: 0x426E82..0x426F1F. If `-NOLOADMUSIC` exists (byte 0xD46AC3), none is registered. If
`GetFileAttributes` fails, it uses the path `"%c:\%s"` on the CD (`g_GameDriveCharacter` 0xC2B9E8).

| types | contents |
|---|---|
| 1..3 | `align/evil|neutral|good.sad` (generic) |
| 4..27 | tribe × alignment. **22..24 (Norse) point to the same strings as 4..6 (Celtic)**: 0x9CAE4C, 0x9CAE0C, 0x9CADCC |
| 28..43 | normal/_vox chants per tribe |
| 44..46 | `citadel.sad`, the same string 0x9CA498 |
| 47..53 | piper, hermit, missionaries. 51..53 are `audio/dialogue/MissionariesVerse1..3.sad`, which are music banks |
| 54 | intro |
| 55..84 | scripts. **56 WELCOME_DANCE points to `FollowUsWelcome.sad`, which is not installed**: the bank stays null and no script uses it. 74 CREATURE_FIGHT, 75 CREATURE_BIG_FIGHT, 77 OUTRO |

`Audio\Music\script\MissionariesSad.sad` is on disk but not in the table. The bw1-decomp values from 84 onwards do not exist in the original.

#### GameMusic: ProcessMusic and its sources

**Faithful.**

##### GAudio's music fields

| field | contents |
|---|---|
| +0x18 | pos[group] (malloc of `4·TotalGroups`) |
| +0x1C | alignment type that is playing (−1 = none) |
| +0x20 | silence turns |
| +0x24 | last finished type |
| +0x28 | script type |
| +0x2C | banks[85] |
| +0x180 | script music started |
| +0x184 / +0x188 | ThingMusicInfo list |
| +0x18C | current town |
| +0x190 | alignment at the camera |

##### Setup

- The ctor (fn_00426D40, music part) registers the 85 banks at 0x426E8E..0x426F1F: GAudio+0x2C[i] =
  LHBankRegister(path, 0) for the entries with a path. Then it sets GAudio+0x18 = malloc(LHMusicGetTotalGroups() · 4)
  (0x426F2B..0x426F40). The game's banks give 13 groups, so GAudio+0x18 holds 13 positions.
- The reset (0x426CA0) sets +0x18C = 0 (0x426CA6).

##### Each turn: ProcessMusic

**`ProcessMusic`** 0x427DF0 is called every turn. It tries the sources in priority order and the first one that takes the music
wins:
1. Video (`g_game+0x250188`) → "nothing" without stopping.
2. No LHMusic (LHMusicIsInstalled, 0x427E09), or `LandNumber == 6` (0x427E1D) → return.
3. `ProcessCitadelMusic` 0x427B60.
4. Script music fn_00427CA0 (0x427E37; then +0x1C = −1).
5. Creature fight fn_00427660 (0x427E42).
6. Chant `ProcessChantMusic` 0x427790.
7. Creature dance 0x427EC0 (0x427E58).
8. Object music fn_00429790 (0x427E63).
9. Alignment and tribe 0x4279C0 (0x427E6E; the symbol "LoginBox::ControlCallback" is misnamed); then +0x180 = 0
   (0x427E77).
10. If there is none: `SavePositions` (0x427E85) and `LHMusicStop(1)` (0x427E8F), fade to silence.

- "Nothing" (0x427E95..0x427EB3) sets "Music Playing=NONE", +0x180 = 0 and +0x1C = −1, without stopping anything.
- The debug texts are set with GDebug::SetMessage: "Music Playing=%s" (0x9CAFA0) with the MUSIC_TYPE name table
  0x9C974C, and "Music Playing=NONE" (0x9CAFB4).
- `SavePositions` (fn_004281C0) goes before each `LHMusicPlay`. It runs only with LHMusicIsActive (0x4281C6). It saves
  pos[group−1] = audible chunk + 2 of each channel in state 1 with group > 0 (signed) and group − 1 < TotalGroups
  (unsigned) (0x4281E4..0x4281FD).
- A source that starts at its group's position reads it with `mov edx, [GAudio+0x18]; mov edx, [edx + group * 4 - 4]`
  (0x427AB6, 0x4275FE, 0x4295CA). Group 0 (BigFight, the 3D banks) reads the word before the array, the heap's.

##### Script music

- **Script** fn_00427CA0. It uses vol 127, start 1, no sync or fade, 2D, and the volume and loops from the .sad.
- It picks its bank with fn_00427C90(fn_00426C80(Discrete(+0x190))), which ignores the alignment and returns +0x28
  (0x427CED..0x427D0D). The user data is the type (0x427D2F).
- The end callback 0x426B80 is set at 0x427D51: if GAudio+0x28 == data, GAudio+0x28 = 0.
- The marker callback 0x426BA0 is set at 0x427D59. It switches on label[0] − 'L' over 0..0x2B: byte table 0x426C10 →
  jump table 0x426C04. `L<n>` → GScript+0x98 = n (atoi(label + 1), 0x426BBE..0x426BE5), +0x9C = 1; `P`/`W` →
  +0x9C++.
- `StartScriptMusic` 0x428230 does +0x28 = type (0x428236); with a type, +0x180 = 0, and it is relaunched even if it
  is the same one.
- With +0x28 == 0 and +0x180 active: `LHMusicStop(1)`. The script music was stopped, so it fades out
  (0x427DC4..0x427DDD).
- `CitadelHeart::Built` 0x4650D1 launches EPIC_01 (61) if `LandNumber != 1`.

##### Fight music

**Fight** 0x427660 / 0x427590: interface state 0x10 or a `GArena` < 100 away; 75 BIG_FIGHT if `g_game+0x205A0C`, if
it is multiplayer or if `LandNumber ≥ 4`; otherwise, 74.

- 0x427660 takes the local player's creature (GPlayer +0xA4C of slot g_game+0x205A59); none → 0. Interface state +0x44
  == 0x10 → play. Else it walks the arena list g_game+0x205C7C (next +0x48): +0x34 set, the creature one of the
  fighters +0x38 / +0x3C (`IsCreatureInArena` 0x425060), and the arena's `GetPos` (vt +0x100) as a point (x, z ×
  10/65536, y = `GetAltitude` + its altitude) minus the camera `LH3DTech::g_camera` (not GetCamera's), with
  `sqrt((dz² + dy²) + dx²)` below 100 ([0x8AB41C]; `test ah, 1`: a NaN plays too).
- 0x427590 plays with the options vol 127, start = pos[group − 1] read before `StoreMusicStyleChunks`, sync 0, fade 1,
  2D, pitch 100 (0x4275D9..0x427618). `LandNumber` is compared unsigned (`jb` 0x4275CA). Without the type's bank
  nothing plays, and the branch still returns 1. The message always names 74 (`_MusicTypes+0x254`, 0x427633), the big
  fight too.
- Script arenas are reused too: +0x34 is set by fn_00424AB0.

##### Chants

- **Chant** 0x427790: a citadel < 150 from the camera (`GetNearestCitadel` 0x602200); of its six sites, the one that
  has dancers with the dance centre nearest, < 100 away (fn_004639A0); |camera height − ground at the centre| < 100.
- That is MapCoords::GetNearestCitadel(150) at GetCamera()+0x14, its fn_004639A0(camera, 100) (the nearest of its
  sites with dancers) and that site's dance (+0xA0) (0x4277A3..0x4277F6).
- The type is table 0x9C9A30 `{28,28,30,…,42}` + (dancers > 8) = the _vox version (fn_00427430). It plays with vol
  127, sync 1, no fade, 3D at `GetSpecialPos(8)`.
- With no bank for the tribe's chant, nothing plays (0x427882).
- A script's music silences the chant while it plays: `ProcessMusic` tries the script music (0x427E37) before the
  chant (0x427E4D), and the script branch returns 1 every turn its music plays. On Land 2, `LearnWorshipping` starts
  `START_MUSIC 68` (MUSIC_TYPE_SCRIPT_KHAZAR, `audio/music/script/khazar.sad`; refs/chl/LearnWorshipping.txt ip 77559)
  as its cinema begins and stops it with `STOP_MUSIC` (ip 77809) as it ends. khazar.sad's first segment has the flags
  0x60 and the loops −1 (+0x244, +0x248), so it loops until stopped. The villagers' worship that the cinema shows
  (the Worship script and the flock at the site) is all heard under Khazar's music, with no chant; this is the
  original's. In a played Land 2 log (2026-10-29) the music ran from turn 8643 to turn 10018. The chant can start only
  after that, with the camera near the temple and the site still holding dancers.
- Details in [Chants and heartbeat](magic-and-weather-sounds.md#chants-and-heartbeat-in-openblack).

##### Creature dance

- **Creature dance** `GAudio::ProcessCreatureDanceMusic` 0x427EC0, read in the frozen v1.42 exe: of every creature
  (`Creature::CreatureList`), the nearest to the camera whose sub-action agenda (mind +0xFA8) is at a step
  (+0xFB4 below the count +0xFBC, the step's state +0xFB8 below 3) whose sub-action (+0xFD8) is 0x17, DANCE in
  `CREATURE_SUB_STATE_ACTIONS`, with a dance (+0x1074) whose first member (`FindFirstDanceMember` 0x5981D0) is a
  Villager (a dynamic cast; any other first member leaves that creature out). The distance is
  `GetDistanceInMetres(camera +0x14, creature +0x14)`, strictly below 75 at the start (0x427ED0) and below the
  nearest so far (0x427F82).
- Then the camera's height (land at its MapCoords + its altitude +0x1C) less the land under the creature, as an absolute
  value, below 60 [0x8C36A8] (0x427FC2..0x42800F), or nothing.
- The type is `GetChantMusicType`(the villager's tribe, the dance's +0x90, its dancers) (0x42801D..0x428086); with no
  bank for it nothing plays (0x428093). (The code after a first member that is not a villager, the nearest town's tribe
  with 1 dancer (0x42804B..0x428082), is never reached: such a creature was left out.)
- It plays with volume 127, the group's saved start chunk (+0x14), sync 1, fade 1 and 3D (+0x1C, +0x20, +0x24),
  pitch 100, the default distances, after `StoreMusicStyleChunks`, and `LHMusicSet3DPosition` puts it at the
  creature's MapCoords as a world point: x and z × 10 / 65536, y = the land there + the creature's altitude
  (0x4280AB..0x42814A). "Music Playing=%s" is set (0x428150) and it returns 1.

##### Object music (ATTACH_MUSIC)

- **Objects** (ATTACH_MUSIC) fn_00429790 / fn_00429500.
- Each `ThingMusicInfo` takes 0x38 B: +0x14 type, +0x18 object, +0x1C active, +0x20 finished, +0x24 started,
  +0x28/+0x2C own position. It has vtable 0x8C4A14 (AudioMusicThing.cpp).
- The list is searched from the head for info+0x18 == thing (0x42918E..0x42919A).
- `AddThingMusic` 0x429230 (ATTACH_MUSIC) accepts only an available thing (0x42923E). A thing that already has music
  only changes its type (0x42924C..0x42925E). A new info cuts its bank if that bank is already playing
  (fn_004296C0(info, 0), which calls LHMusicStop(info, fade) at 0x4296F4) and goes at the head of the list
  (0x429265..0x4292E8; the info at 0x429265..0x4292AC, the head at 0x4292D7..0x4292E8, 0x429296). A new info starts
  enabled, with everything else 0.
- `RemoveThingMusic` 0x429340 stops nothing. It takes the info of GetThingMusicInfo, then every node pointing at it
  (0x429349..0x4293A3). Two infos of one thing cannot exist (AddThingMusic reuses the first), so this is the first
  one.
- ENABLE_DISABLE_MUSIC (fn_004298A0) sets enabled = on, finished = started = 0 (0x4298B4..0x4298BA).
- `RestartMusicThing` 0x429910 sets finished = started = 0 and cuts the music with fade 0 (0x429943).
- `IsMusicThingFinished` of a thing with no info returns 1 (0x429904).
- SetPlayPosition 0x4298C0 (SET_MUSIC_PLAY_POSITION): +0x28 = 1, +0x2C = MapCoords(point). MapCoords::Set 0x603340
  keeps x and z as ftol(v * 6553.6) and the height above the land; fn_00429500 adds GetAltitude back
  (0x42954C..0x42955D) and multiplies x and z by 1 / 6553.6 (0x8AA3A4). So the point comes back with x and z
  truncated to the 16.16 grid and the same y (6553.6f at 0x8AC400, 0x39200000 = 10 / 65536 exactly at 0x8AA3A4).
- GET_MUSIC_OBJ_DISTANCE (fn_004293B0) gives GetPlayDistance of the thing's type, or 0 when it has no info.
- `GetPlayDistance` 0x4293E0: 100 (0x8AB41C) without a bank. Otherwise `LHBankGetMusicMaxDistance` (f32 @+0x26C of
  segment 0), with a negative value giving 100 and 0 staying 0 (0x4293FA..0x429409).
- A thing is in range when GetPlayDistance > its distance (strict, 0x4294CD..0x4294DC). It plays in 3D if the camera
  is closer than `GetPlayDistance`, with sync and fade = (group > 0).
- fn_00429500 measures the range at the thing even when a play position is set (0x42953F); it plays at
  SET_MUSIC_PLAY_POSITION's point if there is one, otherwise at the thing.
- Then: with no channel holding the bank, and both started and finished set, it returns 0. With finished set, it
  returns 1 without playing. Otherwise it calls LHMusicPlay and Set3DPosition (0x429656) and sets started = 1
  (0x42964F). So each turn it does a re-trigger of `LHMusicPlay` + `Set3DPosition`.
- fn_00429790 deletes the info of a thing that is no longer available (0x4297FE..0x42984A). The first info that plays
  takes the music (0x4297D4..0x4297DF).
- A thing whose music is disabled (ENABLE_DISABLE_MUSIC off) but in range plays nothing, and the alignment music does
  not take over either (0x4297F7). So an inactive object in range still blocks the alignment music.
- Nobody sets +0x20 = 1 in 0x429180..0x429950 (open question).

##### Alignment and tribe music

**Alignment and tribe** 0x4279C0. It requires:

- that there is a camera;
- that the script widescreen is not on (HelpSystem +0x45E8 && +0x45EC) and the bars are not moving (fn_005C6C50);
- GScript+0x94 ≠ 0;
- turn > 20.

The type comes from fn_00427460:

- a = fn_00426C80(`Discrete`(GAudio+0x190)) (see [The discrete alignment](#the-discrete-alignment)): 0 evil,
  1 neutral, 2 good.
- The nearest town within 400 (fn_00602160), with the camera < 400 high above the ground:
  - a ≤ 300 → the town's tribe;
  - with hysteresis, the previous town while it is < 400 away.
- If there is no town, generic a + 1.
- Tribe(a, t) = t ≥ 9 ? 5 : {4,4,7,10,13,16,19,22,25}[t] + a (0x427410, table 0x9C9A0C).
- 300 and 400 are `townTriggerDistance` / `townTriggerOffDistance` from info.dat (0xD9A934 / 0xD9A938).

It is played with vol **80**, start pos[g−1], sync 1, fade 1, 2D, loops = (start ≥ n/2) (0x427AF9..0x427B01).

- The end callback is 0x426B40 → 0x4279A0. It sets pos[group − 1] = 1 (0x426B61), then +0x24 = the type (0x4279A3),
  +0x20 = 0 (0x4279A6) and +0x1C = −1 (0x4279AD).
- When a track ends, **3500 turns of silence** follow, as long as the type does not change.
- ProcessAlignmentMusic clears +0x24 at 0x427A6E.

##### Citadel music

- **Citadel** 0x427B60: 44 + the discrete alignment of the local player (0x427B9A..0x427BDA: 44 +
  fn_00426C80(GAlignment::GetDiscreteAlignmentValue(GPlayer::GetAlignmentValue of the local player))):
  CITADEL_EVIL / NEUTRAL / GOOD, the three on citadel.sad (group 4).
- `LHSampleStopAll` once (flag 0xC56164); vol 127, sync 1, fade 1.
- Its start chunk is the group's position GAudio+0x18 (0x427BEB..0x427BFA, milestone A5).

##### The discrete alignment

- `Discrete` is 0x414730: `ftol(min((a+1)/2·k, 6))`. Its constants are −1.0 (0x8AB640), 1.0 (0x8AB644), k =
  6.9999995 (0x8AB688, bits 0x40DFFFFF, the float just below 7) and 6.0 (0x8AB35C); the result is truncated by
  `__ftol` 0x7A1400, which on an SSE2 CPU (`LH3DP3::HasSSE2` [0xE83A20]) is `cvttsd2si`. The cap's `fcom` lets a NaN
  through, so a NaN, minus infinity or a value far below the range gives the integer indefinite 0x80000000. Oracle:
  0xBE124923 → 2 and 0x3F36DB6E → 5, where a factor of 7 would give 3 and 6 (test
  `ScriptSpirits.DiscreteAlignmentMatchesTheOriginalAtTheStepsEdges`; until 2026-10-19 ours multiplied by 7 and held
  a NaN at 6). `help::script_spirits::DiscreteAlignment` is the one implementation for every caller.
- `AlignmentIndex` fn_00426C80 reads table 0x9C99F0 `{0,0,1,1,1,2,2}`, which gives 0 evil, 1 neutral, 2 good, with
  thresholds ±3/7. An index of 7 or more (signed compare, 0x426C87) gives 1; only then is the table 0x9C99F0 read.
- The temple's scrolls pick their alignment texts with it too: `TempleRoom::AddText` 0x79A910 calls 0x414730 three
  times (0x79AB4C, the creature's alignment for label 1162; 0x79B514, label 1193; 0x79BD37, label 1197, through
  fn_0079CC80) and adds 0x46A (1130, HELP_TEXT_ALIGNMENT_01) to the result before `GetTextL` 0x4707F0. No lower clamp.
  What the second and third sites pass is not read.

##### Script commands

- START_MUSIC: a type below 0 or above 0x55 gives an error, and it goes on (0x70FB34..0x70FB47).
- LAST_MUSIC_LINE: ftol(line), then +0x98 >= it, unsigned (cmp; sbb; inc) (0x71007A..0x71009C, the compare at
  0x710098).
- ATTACH_MUSIC 0x70FBF0: an error outside 1..84 (1..0x54, 0x70FC31..0x70FC3F), then AddThingMusic all the same if the
  script's thing is valid (0x70FC47..0x70FC53).
- GET_MUSIC_ENUM_DISTANCE: outside 1..0x54, "Invalid music man!" and a push of 0.0 (0x70FDF4..0x70FE28).
- The music-played query reads 0x70FBC9..0x70FBD0.

#### Intro, trailer, outro, videos and menu

**Faithful except where marked.**
- **Trailer**: `PlayPreIntroVideo` 0x6426F0 plays `pre_intro.bik` without Bink sound and plays `audio\music\intro\trailer.sad`
  (0xBFEB44) with `LHMusicPlay` (vol 127, start 1, no fade, 2D). When it ends, `LHMusicStop(0)` (0x642907) (confirmed: xref immediates).
- **Intro**: START_MUSIC 54 in the script `followus.txt::FollowUs`.
- **Outro**: START_MUSIC 77 in `landcontrol5.txt::VillageWavingSequence` (`Outro.sad`, 160 kbps, loop −1).
- **Videos**: `PlayFullScreenMovie(path, sad)` 0x54D920 registers an optional bank and fn_0054A9B0 starts it at
  frame 3 (vol 127, start 1, fade 1). It also sets GAudio+0x1C = −1.
- **Main menu**: none of the sites that call `LHMusicPlay` belongs to the menu, so the menu has no music
  of its own: the menu is boxes on top of the loaded world and its normal music plays (confirmed by the user).
- `Data\intro.wav` does not appear in the exe.
- `MusicMoodController` 0x633EF0 is not audio: they are network packets.

## openblack

### Counterparts in the code

What openblack does at each step of the original described above, by section.

#### The engine (LHMusic)

- `MusicEngine` (`src/Audio/Engine/MusicEngine.{h,cpp}`, ✅ faithful) is the DLL's logic over an `IMusicSink` interface (what
  QMixer does). That way it is tested without OpenAL (`test_music_engine`).
- `MusicStream` + `MusicSystem` (`src/Audio/Engine/MusicStream.{h,cpp}`):
  - They use the **audio device** (`audio::device`), without opening another one. They have one source per channel, with its queue of
    buffers and a continuous MPEG decoder per track (`MusicSegmentDecoder`, `audio::codec::MpegFrameDecoder`).
  - The thread does one pass and then waits 120/5000 ms. It pumps every 20 ms.
  - There is a recursive lock, which stands in for the critical section 0x100562B0.
  - Gain = QMixer volume / 32767, times QMixer's 3D law.
  - The MUSIC_TYPE banks are registered by `banks::MusicBankOf` on first use.
- The bank's decoder reset (LH_AudioBank +0x138 +0x58) is `resetDecoder`, kept on the channel: a bank plays on one
  channel at a time.
- `MusicSegmentDecoder::Decode` decodes with a std::vector, which does not need the original's overrun assert.
- **⚠️ approximate**:
  - A segment that does not decode gives a frame of silence, so that the callback arrives.
  - If the OpenAL queue empties, the source stops and resumes as soon as there is data; QMixer plays the next wave as
    soon as it is there.
  - Before the first frame there is no WAVEFORMAT. Before any frame has decoded (0x1000FA34..0x1000FA9D builds it
    from the frame header), `MusicStream::QueueChunk` sends the silent frame out as stereo at 22050 Hz, the rate every
    music bank has (music_sad_table.md).
  - Lowering the volume within the pass: the original does not look at the state again inside the loop (MusicEngine.cpp:536).
- **🔍 inferred**:
  - The unit of `lStart` and of `QSWaveMixGetPlayPosition` is samples.
  - `FlushChannel` reports each flushed chunk before returning: this is deduced from the wait in `LHMusicClose`, 0x1000E854.
  - The marker clock runs in double (0x1001F874).

What each engine function mirrors:

- **MusicBank** (`src/Audio/Engine/MusicBank.*`):
  - `MusicBank::Register` / `Register`: LHBankRegister(path, 0) 0x10002240, 0x1000228A..0x100022FE, 0x100023C0..,
    0x100027AB..0x100027C7; it returns nullptr where the original returns 0. The group count is left to the caller.
  - `IsMusic`: 0x10002EE0. `GetSegmentCount`: 0x10002E90. `GetGroupId`: 0x10002F30.
  - `MusicBank::GetFlags` / `GetFlags`: 0x1000E1CC, 0x1000E30A, 0x10002EB0.
  - `MusicBank::GetSampleRate` / `GetSampleRate`: 0x1000E210.
  - `MusicBank::GetVolume` / `GetVolume`: 0x1000E0C5, 0x1000E1D6..0x1000E1ED, 0x1000E1DB..0x1000E1ED.
  - `MusicBank::GetLoops` / `GetLoops`: 0x1000E13E, 0x1000E1F1..0x1000E206, 0x1000E200 (at
    `if (!HasFlag(MusicBankFlag::Loops))`).
  - `MusicBank::GetDistanceMapping`: 0x1000E2D9..0x1000E338; `MinDistance` 0x1000E31F, `MaxDistance` 0x1000E310,
    `Scale` 0x1000E32D.
  - `MusicBank::ParseMarkers`: 0x1000D9E0.
- **Pure helpers**: `MixerVolume` (0x1000F429..0x1000F464, 0x1000E8F7, 0x1000F53D..0x1000F664); `MarkerPosition`
  (0x1000DB90, 0x1000DC13..0x1000DC52); `k_MusicQueueDepth` (0x1000ECC1, 0x1000F4CD); `k_MusicMinPitch`
  (0x1000E970, 0x1000E9AC / 0x1000E9BB); k_MusicPassSleepMs and k_MusicRetrySleepMs (0x1000F6FA..0x1000F728).
- **MusicEngine**:
  - `MusicEngine` (the constructor): the music part of 0x1000DD50 (0x1000DE08, 0x1000DDFE, 0x1000DEF1,
    0x1000DF43/0x1000DF46).
  - `MusicEngine::NoteBankRegistered`: 0x100027AB..0x100027C7.
  - `MusicEngine::Play`: 0x1000DF6C..0x1000DFC6, 0x1000DFD4..0x1000DFEF, 0x1000E00D..0x1000E04A,
    0x1000E06F..0x1000E0A5, 0x1000E0BB, 0x1000E0D6, 0x1000E0DA..0x1000E0F6, 0x1000E103, 0x1000E110, 0x1000E117
    (LHBankGetMusicGroupId), 0x1000E12C, 0x1000E148, 0x1000E155, 0x1000E161, 0x1000E16E, 0x1000E17B, 0x1000E18C
    (LHBankGetNumberOfSamples), 0x1000E1A1 (+0x28 = 0, at `ch.savedStatus = MusicStatus::Free;`), 0x1000E1AF,
    0x1000E1B9, 0x1000E24C..0x1000E3F7, 0x1000E3FF..0x1000E41B, 0x1000E42D, 0x1000E437 (at
    `ch.status = MusicStatus::Playing;`), 0x1000E43B..0x1000E448, 0x1000E454, 0x1000E461, 0x1000E46E.
  - `MusicEngine::Stop`: 0x1000E57A, 0x1000E6B2, 0x1000E6C3..0x1000E730. `Cut` (the cut of LHMusicStop(0) for one
    channel): 0x1000E57F..0x1000E5F3, 0x1000E6C3..0x1000E730.
  - `MusicEngine::SetMainVolume`: 0x1000E890, 0x1000E8C6, 0x1000E8ED..0x1000E8F5.
  - `MusicEngine::GetMainVolume`: 0x1000E950. `IsInstalled`: 0x1000EAE0 / 0x1000EAF0.
  - `MusicEngine::Close` / `Close`: 0x1000E7A0, 0x1000E7BC..0x1000E7DB, 0x1000E803, 0x1000E81D, 0x1000E84E,
    0x1000E854, 0x1000E870.
  - `MusicEngine::DispatchMarkers` / `DispatchMarkers` (fire the markers between the last position and the clock's):
    0x1000DB90, 0x1000DC2E, 0x1000DC36, 0x1000DC6E / 0x1000DC71.
  - `MusicEngine::QueueChunks` / `QueueChunks` (the queueing loop of one channel; false when a read failed):
    0x1000EC9E..0x1000F4D5, 0x1000EC9E..0x1000ECD8, 0x1000ED4E, 0x1000EE19..0x1000EE64, 0x1000F337..0x1000F3A8,
    0x1000F3AB..0x1000F3C1, 0x1000F3DB..0x1000F3F0, 0x1000F423..0x1000F472, 0x1000F478..0x1000F49D,
    0x1000F0D4..0x1000F0E1, 0x1000F1F2 + 0x1000F4C4.
  - `MusicEngine::Process` / `Process` (one pass; it returns false when a segment could not be read): 0x1000EB40,
    0x1000EBBA..0x1000EBEE (at
    `if ((ch.status == MusicStatus::Playing || ch.status == MusicStatus::LastQueued) && _master == i && _active &&`),
    0x1000EBF3..0x1000EC94 (at `if (ch.status == MusicStatus::Finished)`), 0x1000EE64 -> 0x1000F6FA,
    0x1000F4DB..0x1000F6E2, 0x1000F534, 0x1000F592, 0x1000F59F..0x1000F5AB, 0x1000F682..0x1000F6E2.
- **IMusicSink** (what QMixer does; `MusicStream` implements it):
  - the end-of-chunk notification: the QMixer callback 0x1000DC80 of QSWaveMixPlayEx (0x1000F314);
  - `SetListener`: where the end-of-chunk callbacks go; FlushChannel calls it for each flushed chunk before returning
    (🔍 inferred, see [End of a chunk](#end-of-a-chunk));
  - `QueueChunk`: decode one segment (with a fresh decoder if resetDecoder) and queue it (0x1000F2FF, 0x1000F4C4)
    starting at startSample (0x1000F395); last is the callback data +4 (0x1000F326);
  - `SetCentred` (0x1000F3F0), `SetFrequency` (0x1000F41D, 0x1000E9EF), `IsChannelDone` (0x1000EC5A...),
    `FlushChannel` (0x1000EC77...), `SetSourcePosition` (0x1000FBE8), `PauseChannel` (0x1000EA46 / 0x1000EAB6);
  - `GetChannels`: of the last frame decoded (0 before any), the WAVEFORMAT that 0x1000FA34..0x1000FA9D builds from the
    header.
- **MusicStream and MusicSystem**: `MusicSegmentDecoder::Reset` (0x1000F0DA..0x1000F1C5), `MusicSegmentDecoder::Decode`
  (0x1000F740, 0x1000F884), `MusicStream::QueueChunk` (the silent frame above), `MusicSystem::~MusicSystem`
  (LHMusicClose 0x1000E7A0: the thread is told to end and waited for, 0x1000E7C1..0x1000E7DB, then the channels).

#### GameMusic

- `GameMusic` (`src/Audio/GameMusic.{h,cpp}`, ✅ faithful): ctor fn_00426D40 (music part), `Reset` 0x426CA0,
  `UpdateTurn` (was `ProcessAudioGameTurn`) 0x427080, `ProcessMusic` 0x427DF0, `StartScriptMusic` 0x428230, `ProcessScriptMusic`
  0x427CA0, the callbacks 0x426B40/0x426B80/0x426BA0, `ProcessAlignmentMusic` 0x4279C0 and fn_00427460, `DiscreteAlignment`
  0x414730, `AlignmentIndex` fn_00426C80, `TribeMusicType` fn_00427410, `SavePositions` fn_004281C0, `ResetPositions`
  fn_00428190, the object music fn_00429790/fn_00429500/fn_00429420/fn_00429680/fn_004296C0, `PurgeThingMusic`
  fn_00429700 and the CHL functions.
- `ThingMusicList` (`src/Audio/Services/ThingMusic.{h,cpp}`): 0x429180, 0x429230, 0x429340, 0x4291B0, fn_00429880,
  fn_004298A0, 0x4298C0, 0x4298F0, and the `MapCoords` round trip (0x603340, 6553.6 at 0x8AC400, 10/65536 at 0x8AA3A4,
  as `ecs::map_coords::Quantise`).
- `Game.cpp` hooks: `GAudio::ProcessAudioGameTurn` after turn 5 (0x54E997) and `Reset` in `LoadMap`.
- The fight branch is `GameMusic::ProcessFightMusic` / `PlayFightMusic` with its game side
  `GameQueries::creatureFightMusic` (`CreatureFightMusic` in AudioQueries.cpp): the local player's primary creature
  (`PlayerSystemInterface::GetPrimaryCreature`) and `CreatureFightSystem::GetArenas` (maker / opponent, the middle as
  a map position at the land's height) against the render camera's origin, through the pure
  `ecs::audio_queries::FightsNearCamera`. The 0x10 branch comes first: `ecs::audio_queries::LocalPlayerDirectsFight`,
  the pure `DirectsFight`, holds while the interface is active (`interface_active::IsActive`, GInterface::IsActive
  0x5CE2E0: bit 0 of +0x28 clear), the player's camera watches a fight (`camera::FightWatch::IsWatchingFight`, the
  New3 mode's `HasFight` +0x44), the local player's creature is in its duel (`CreatureFightSystemInterface::IsDuelling`,
  for `Creature::IsFighting` 0x47B1C0: its 3D body's opponent +0x528C set) and the hand holds nothing
  (`HandSystemInterface::GetHeldObject`, for `GetFirstObjectInCurrentHand`): the conditions under which
  `GInterface::ResetActionState` 0x5D29C0 sets state 0x10 (0x5D29D3..0x5D2A54; fn_005D2990's g_game+0x250538 is set
  from `GGame::Init` 0x54FCDC to `Close` 0x54ECE4, a whole game). The same predicate is `GameQueries::interfaceState`
  (0x10, else 0), so the samples of userParam 4 are quiet in it. **⚠️ approximate**: the original enters the state only
  when an idle hand calls `ResetActionState` (states 0 and 1 do every frame, 0x5D5193) and leaves it as soon as the
  camera or the duel fails (0x5D4B01..0x5D4B19); a press in it goes to the fight (0x5D4B1F..0x5D4BD6, FIGHT_APPLY
  0x16 / FIGHT_SELECT 0x17), which openblack's hand takes in its own fight state since 2026-10-29
  ([the player's presses](../creature/fights.md#the-players-presses)); the music's predicate still counts a hand busy
  with another action as idle. The arenas the music reads are the fight system's (`GetArenas`): reusing a nearby arena
  and the scripts' arenas belong to the fights ([fighting.md](../project/progress/creature/fighting.md)). No land run
  has two creatures, so the checks do not see it. Tests: `GameMusicTest.FightMusicWhileTheCreatureFightsNearTheCamera`,
  `GameMusicTest.FightMusicWithoutItsBankStillTakesTheMusic`, `FightMusic.TheLocalCreaturesArenaNearTheCamera`,
  `FightMusic.TheInterfacesCreatureFightStateNeedsAllFour`; the Music debug panel shows the state.
- The creature dance branch (0x427EC0) is `GameMusic::ProcessCreatureDanceMusic`, fed by `GameQueries::creatureDance`
  (`CreatureDanceHeard` in `src/ECS/AudioQueries.cpp`): the nearest creature whose agenda is at its dance, with a
  villager as the dance's first dancer, closer than 75 to the camera (`creature_dance::k_MusicFurthest`) and with the
  camera less than 60 above or below the land under it (`k_MusicHighest`); it plays the chant of that villager's tribe
  for the dancers, 3D at the creature's map coordinates (`audio_queries::CreatureDanceChantPoint`), volume 127,
  sync 1, fade 1, from the group's saved position, as the original's. Ported with the creature's dance (raffclar's PR
  #488, 2026-10-17); tests `GameMusicTest.ChantMusicAtACreaturesDance`,
  `CreatureDanceMusic.TheChantIsPlacedAtTheCreaturesMapCoordinates`. The chant one is
  `GameMusic::ProcessChantMusic` and the citadel one `GameMusic::ProcessCitadelMusic`. The camera's
  alignment (GAudio+0x190) and the town with its tribe come from `ecs::map_cells` (see [Weather and alignment](magic-and-weather-sounds.md#weather-and-alignment-in-openblack)):
  with the camera over a town with a centre, its tribe's music plays, otherwise the generic alignment one, at volume 80,
  from turn 20 on lands ≠ 6.
- The chant's site (0x4277A3..0x4277F6) is the game's side, `GameQueries::chantSite`.
- The temple's scrolls: openblack's `TempleScrolls::Write` calls `help::script_spirits::DiscreteAlignment` for it (its
  0 floor only guards a value below −1).
- **⚠️ approximate**:
  - A type outside 0..84 has no bank: the original reads the following fields.
  - Group 0 reads `pos[-1]`, outside the array: here it gives 0, and the DLL starts at chunk 1
    (0x1000ED6A..0x1000ED77).
  - Outside −1..1 the tribe reads outside its table: the same as from 9 onwards is taken. For the alignment the
    original itself gives 1 for an index of 7 or more (0x426C87); only a negative index reads outside the table
    0x9C99F0, and neutral is taken.
  - Without a camera, an object is out of range.
  - `LHWaveIsActive` = the music engine exists and is active.
  - `GameThing::IsAvailable` = a valid entity with Transform. Its float position replaces the MapCoords, without the
    16.16 rounding.
- **🔍 inferred**: openblack's turn (which goes back to 0 on each `LoadMap`) stands in for `g_game+0x205A40`, and the render
  camera stands in for GGame's.

What each GameMusic function mirrors:

- `GameMusic::GameMusic`: 0x426E8E..0x426F1F. `GameMusic::Reset`: 0x426CA6 (+0x18C = 0).
- `PlayingMessage`: ProcessMusic's GDebug::SetMessage texts, 0x9CAFA0 with 0x9C974C.
- `GameMusic::SavePositions`: fn_004281C0, 0x4281C6.
- `GameMusic::GroupPosition`: 0x427AB6, 0x4275FE, 0x4295CA.
- `GameMusic::StartScriptMusic`: 0x428236. `GameMusic::ProcessScriptMusic`: 0x427DC4..0x427DDD.
  `GameMusic::OnScriptMusicMarker`: 0x426BA0. `GameMusic::OnAlignmentMusicFinished`: 0x426B61.
- `GameMusic::ProcessCitadelMusic`: 0x427B9A..0x427BDA. `GameMusic::ProcessChantMusic`: 0x4277A3..0x4277F6.
- `GameMusic::AddThingMusic`: 0x42923E. `GameMusic::RestartThingMusic`: 0x429927. `GameMusic::StopThingMusic`:
  0x4296F4. `GameMusic::ThingMusicInRange`: 0x4294CD..0x4294DC. `GameMusic::PlayThingMusic`: 0x429520.
  `GameMusic::ProcessThingMusic`: 0x4297FE..0x42984A.
- `GameMusic::GetMusicObjDistance` / `GetMusicObjDistance`: fn_004293B0, 0x4293C7 / 0x4293D0.
  `GameMusic::GetPlayDistance`: 0x4293E0, 0x8AB41C, 0x4293FA..0x429409.
- `GameMusic::ScriptStartMusic`: 0x70FB34..0x70FB47. `GameMusic::ScriptMusicPlayed`: 0x70FBC9..0x70FBD0.
  `GameMusic::ScriptLastMusicLine`: 0x71007A..0x71009C. `GameMusic::ScriptMusicTypeDistances`:
  0x70FDF4..0x70FE28.
- `GameMusic::ScriptAttachMusic` / `ScriptAttachMusic`: 0x70FBF0, 0x70FC31..0x70FC3F, 0x70FC47..0x70FC53; nullopt
  when GetScriptGameThing gave none (0x70FC47).
- `ThingMusicList::Get`: 0x42918E..0x42919A. `ThingMusicList::AddFront` / `AddFront`: 0x429296,
  0x429265..0x4292AC, 0x4292D7..0x4292E8 (the caller has checked that the thing has no info yet).
  `ThingMusicList::Remove`: 0x429349..0x4293A3. `ThingMusicList::Move`: 0x429892. `ThingMusicList::Enable`:
  0x4298B4. `ThingMusicList::SetPlayPosition` / `SetPlayPosition`: 0x4298C0, 0x4298D2. `ThingMusicList::IsFinished`:
  0x4298FE / 0x429904.

#### MUSIC_TYPE table

- `audio::MusicType` and `k_MusicBanks` in `BankTables.h`. The tests name the types they use, such as
  `MusicType::CelticChant` (the chant table) and `MusicType::ScriptMissionariesVerse1` (a script bank whose
  `LAST_MUSIC_LINE` compares `>=`, 0x710098).

#### Intro, trailer, outro, videos and menu

- The videos play (`videoPlaying`: the ambience is silenced and the music continues underneath; the 11 sounds of fall.bik and `MusicStop(1)` at 43.9 s, checked, see [Bink video](../video/bink.md)); the trailer plays with the pre-intro (`--pre-intro`, see [Startup and loading screen](../video/playback.md#startup-and-loading-screen)); the main menu has no music of its own in the original (its world's music plays under it); whether openblack's menu does the same is not checked here.

### Music in openblack

The chain is `Game::Initialize` → `audio::music::Start()` (`LH_AudioSystem` init 0x1000DD50,
on the `audio::device` device) → `audio::game_music::Start(GameQueries, townTrigger)`. Per frame,
`audio::music::Update()` applies the master from the configuration and the test hook. On the turn
`game_music::ProcessTurn` runs. On shutdown, `game_music::Shutdown` and then `music::Shutdown` (`LHMusicClose` 0x1000E7A0),
before releasing the context. Debug panel: the "Music" window (`src/Debug/Music.{h,cpp}`), with the master, the 6
channels, a player for any MUSIC_TYPE (with or without sync and fade; stop with fade or abruptly), the state of
GameMusic (+0x28, +0x180, +0x1C, +0x20, +0x24, +0x18C, pos[group]), GScript +0x84..+0x9C and the object list.

### Music and texts in openblack: files and tests

The music, script and text pieces, with their files and tests:

| piece | state | files | test |
|---|---|---|---|
| Tables | done | `src/Audio/Game/BankTables.h`, `components/pack` (`AudioBankInfo`) | `test_audio_tables` |
| MusicBank | done | `src/Audio/Engine/MusicBank.*` | `test_music_bank` |
| MusicEngine | done | `src/Audio/Engine/MusicEngine.*` | `test_music_engine` (0→127 in 32 passes, 127→0 in 43, 80→0 in 27, sync, 7th track, master) |
| MP2 streaming | done, on `audio::device` | `src/Audio/Engine/MusicStream.*`, `src/Debug/Music.*`, `Debug/Gui.cpp` (two lines) | `test_music_stream` |
| Script music | done | `src/Audio/GameMusic.*`, `CHLApi.cpp` | `test_game_music` (its helper `WriteBank` writes synthetic banks and registers their groups as LHBankRegister does at 0x100027AB) |
| Script state | done | `src/Audio/Services/ScriptAudioState.*`, `CHLApi.cpp` | `test_game_music` |
| ThingMusic | done | `src/Audio/Services/ThingMusic.*`, `CHLApi.cpp` | `test_game_music` |
| Music volume | done: `EngineConfig::audioMusicMainVolume`, the options box's slider (`Game::HandleInterfaceAction`) and the debug slider; kept for the computer in `Profiles/Setup.txt` as the original keeps it in its setup's registry key | `src/EngineConfig.h`, `src/Debug/Music.cpp`, `src/Common/MachineSetup.cpp`, `src/Audio/MainVolumesSetup.cpp` | trace `gain=`; `test_machine_setup` |
| Alignment and tribe | done: alignment at the camera and town/tribe via `ecs::map_cells` (`ECS/AudioQueries.cpp`); audited: camera height as in UpdateGameThingWithPosData 0x442EF0 | `src/Audio/GameMusic.*`, `src/Audio/GameQueries.h` | `test_game_music` (with fake queries) |
| Voice table | done | `src/Common/HelpText.*`, `src/Audio/Services/Voices.*` | `test_voice_table` (6974 / 3477 / 1922 / 1328 / 227, WORKSHOP_10 → villagers 399) |
| HelpSystem (text) | done, without drawing | `src/Help/HelpSystem.*`, `src/Help/DialogueText.*`, `CHLApi.cpp`, `Game.cpp` (click) | `test_help_system` |

The tests that read data use `OPENBLACK_TEST_BW_ROOT` and do `GTEST_SKIP` if there is no installation.

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

- OPENBLACK_TEST_MUSIC="<type>[,<type>@<s>][,stop@<s>][,cut@<s>]" (`ParseTestHook`): the first play goes like the
  trailer's (vol 127, chunk 1, no sync, no fade, 2D: PlayPreIntroVideo 0x6426F0); each later one with sync and fade,
  as ProcessCitadelMusic 0x427B60 asks (vol 127, sync 1, fade 1), to hear a synced change; stop = LHMusicStop(1),
  cut = LHMusicStop(0). It is a test hook, not a behaviour of the original: ProcessCitadelMusic's start chunk is the
  group's position GAudio+0x18 (0x427BEB..0x427BFA, milestone A5), here always 1 (only used when no other channel of
  the group plays).

## Pending

### From the audio engine's list

- The fight music's interface state 0x10: the music reads the predicate, not the hand's own state, see above.
- Music: who sets ThingMusicInfo+0x20; `LandNumber` 6 (g_game+0x205A0C is the playground flag, `GGame::Init`
  0x54F75E); whether `GetDistanceInMetres` is 2D or 3D (0x74CCB0); group 0 (`pos[-1]`).
- `MusicEngine::MusicEngine`: "every channel free with sad volume 127, and its QMixer channel enabled in 2D (0x110)".
  This conflicts with `EnableChannel`: "QSWaveMixEnableChannel 2D (0x100, 0x1000E3F7) or 3D (0x110/0x111 | 0x100)". The
  2D flag at init is unclear.
- `GameMusic::ProcessAlignmentMusic` `_finishedType = 0; // 0x427A6E`: whether +0x24 is cleared when the 3500 silent
  turns end or when the type changes is not stated.
