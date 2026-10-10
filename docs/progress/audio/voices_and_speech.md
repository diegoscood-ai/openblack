# Voices and speech

Spoken lines: the advisors and story characters reading their dialogue, villagers talking and singing, the creature's
voice, and the whispering voices that say the player's name.

**Progress: 10/15 done, 4 partial — 80%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../bw1-notes/audio.md).

## Dialogue

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each line of dialogue has a recording, played as the line shows | done | `src/Audio/Services/Voices.cpp` (the text to voice table built from the dialogue banks' wave names), said by `help::HelpSystem` (`src/Help/HelpSystem.cpp`) and the dialogue commands in `src/CHLApi.cpp`; tests `VoiceTable.*`, `VoicesTest.*` |
| The advisors' lines from their own banks (guidance and help sprites) | done | `src/Audio/Services/Advisor.cpp` on the help sprites bank, `src/Audio/Services/Guidance.cpp` on the guidance bank, driven by `src/Help/SpiritsRuntime.cpp`; tests `VoicesTest.SpiritTextsGoToTheirAdvisor`, `GuidanceTest.*`; see ../story/advisors.md |
| The advisors' lips move with their voice | done | `audio::advisor::LipSyncThisFrame` and `LipSyncKey` (`src/Audio/Services/Advisor.cpp`) read by `src/Help/SpiritsRuntime.cpp`; tests `LipSync.*` |
| A line can be skipped and the voice stops | done | `voices::CutByClick` from the help system's click; test `VoicesTest.ClickCutsOnlyTheVillagersNarration` |
| Speech volume and subtitles settings | partial | the options box has the effects and music volumes and the help text speed (`src/Gui/GameMenu.cpp`); none of them is saved to disk. Our wiki differs: the original has only two main volumes, effects and music, and no speech volume ([audio](../../bw1-notes/audio.md#master-volumes-focus-and-reset)); see ../interface/options.md |
| Miracle lines spoken when a miracle is cast | done | the power-up and fully charged lines from the spell dialogue bank (`src/Magic/Core/SpellSeed.cpp`, `src/Worship/WorshipSpellIcon.cpp`) |
| The missionaries' three sung verses, with the words to follow | partial | the three verses are music banks started by the script, with their line markers (LAST_MUSIC_LINE, `src/Audio/Services/GameMusic.cpp`); no words drawn to follow; see [the_explorers.md](../story/silver_scrolls/the_explorers.md) |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers talk in their tribe's babble | partial | the banter bank plays from the villagers' clip sound events, by man, woman or child (`src/Audio/Services/AnimationSounds.cpp`); see ../villager/looks_and_voices.md |
| Villagers praise or fear the player aloud | partial | `src/Audio/Services/Guidance.cpp` plays the alignment remarks, town desires and resource drop remarks (tests `GuidanceTest.AlignmentRemarks`, `GuidanceTest.DesireSampleAndScore`); the remarks on deaths, attacks, lost villagers and destroyed buildings are not connected yet; see ../villager/looks_and_voices.md |
| Villagers sing while they dance and worship | done | the chants, each tribe's and its sung version, by the nearest dancing worship site (`GameMusic::ProcessChantMusic`); test `GameMusicTest.ChantMusicAtTheDance`; see ../worship/ |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's voice from its species' bank, by size and alignment | done | `creature_audio::VoiceBankStem`, `CreatureAudioSystem` (`src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`); tests `CreatureAudio.VoiceBankStem`, `CreatureAudio.SizeKey` |
| Its voice is heard only when close enough | done | `creature_audio::IsHeard` (`src/Creature/CreatureAudio.cpp`); test `CreatureAudio.Gate` |
| Scripts change the creature's sounds | done | SET_CREATURE_SOUND in `src/CHLApi.cpp` sets the script audio state that `CreatureAudioSystem::AreOtherVoicesEnabled` reads; test `CreatureAudio.TheScriptLetsOtherVoicesBeHeard` |

## Other voices

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spooky whispering voices now and then say the player's name, matched by how it sounds | done | `src/Audio/Services/SpookyVoices.cpp` (the player's name matched by its sound code, at night), run from `Guidance.cpp`; tests `SpookySoundex.*`, `SpookyTest.*` |
| The hidden phone box's recorded messages | todo | the phone box mesh is listed (`src/3D/AllMeshes.cpp`), nothing plays its messages; see ../story/land_1.md |
