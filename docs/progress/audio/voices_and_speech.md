# Voices and speech

Spoken lines: the advisors and story characters reading their dialogue, villagers talking and singing, the creature's
voice, and the whispering voices that say the player's name.

**Progress: 14/15 done, 0 chosen, 0 partial, 0 inferred — 93%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../../audio/voices.md).

## Dialogue

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each line of dialogue has a recording, played as the line shows | done | `src/Audio/Services/Voices.cpp` (the text to voice table built from the dialogue banks' wave names), said by `help::HelpSystem` (`src/Help/HelpSystem.cpp`) and the dialogue commands in `src/CHLApi.cpp`; tests `VoiceTable.*`, `VoicesTest.*` |
| The advisors' lines from their own banks (guidance and help sprites) | done | `src/Help/AdvisorVoices.cpp` on the help sprites bank, `src/Audio/Services/Guidance.cpp` on the guidance bank, driven by `src/Help/SpiritsRuntime.cpp`; tests `VoicesTest.SpiritTextsGoToTheirAdvisor`, `GuidanceTest.*`; see ../story/advisors.md |
| The advisors' lips move with their voice | done | `help::AdvisorVoices::ApplyLipSync` and `GetLipSyncKey` (`src/Help/AdvisorVoices.cpp`), run from each spirit's update (`src/Help/Spirits.cpp`, `src/Help/SpiritsRuntime.cpp`); tests `LipSync.*`, `AdvisorVoices.*`, `Spirits.*LipSync*` |
| A line can be skipped and the voice stops | done | `voices::CutByClick` from the help system's click; test `VoicesTest.ClickCutsOnlyTheVillagersNarration` |
| The effects and music volumes are kept between runs, and the story text with the player's profile; there is no speech volume | done | the two main volumes are kept for the computer in `Profiles/Setup.txt` (`machine_setup`, `audio::main_volumes`): read as the game starts and as the options' page shows, kept as it goes and as the game ends, as the original keeps them in its setup's registry key; a run of the checks keeps none (tests `MachineSetup.*`; Debug > Audio > Channels reads or keeps them). The story text (TEXT_DRAW) is kept with the profile, see ../interface/options.md. The original has only the two main volumes, effects and music, and no speech volume ([audio](../../../audio/banks-and-channels.md#master-volumes-focus-and-reset)) |
| Miracle lines spoken when a miracle is cast | done | the power-up and fully charged lines from the spell dialogue bank (`src/Magic/Core/SpellSeed.cpp`, `src/Worship/WorshipSpellIcon.cpp`) |
| The missionaries' three sung verses, with the words to follow | done | the three verses are music banks started by the script, with their line markers (LAST_MUSIC_LINE, `src/Audio/GameMusic.cpp`), and the words drawn with the sung one boxed (`help::karaoke`, `src/Help/Karaoke.cpp`; testbed `dialogue.song_karaoke`); see [the_explorers.md](../story/silver_scrolls/the_explorers.md) |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers babble as they go about their lives, in a man's, a woman's or a child's voice (the same babble for every tribe) | done | the banter bank plays from the villagers' clip sound events, by man, woman or child (`src/Audio/Services/AnimationSounds.cpp`; tests `ClipEventVoice.*`, `AnimEffectsTest.BanterAtTheAbodeWithTheVillagersDistance`, `AnimationSoundsTest.AMansClipPlaysItsSoundsInHisVoiceAndHisHomesBanterFromHisHome`). Our row said each tribe had its babble: the original has one banter bank and its key has no tribe ([world-sounds.md](../../../audio/world-sounds.md#clip-sounds)); see ../villager/looks_and_voices.md |
| Villagers praise or fear the player aloud | done | `src/Audio/Services/Guidance.cpp` plays the alignment remarks, town desires and resource drop remarks (tests `GuidanceTest.AlignmentRemarks`, `GuidanceTest.DesireSampleAndScore`), a town's attack remark (`PlayTownAttackRemark` from `town_emergency::UpdateAggressor`), the death remarks (`src/ECS/Villager/VillagerDeath.cpp`) and the awe: a villager impressed by a miracle, a creature, a scaffold or an object adds belief with itself as the thing and the reaction's alignment, and the town's people are awed aloud when the player is believed in less than the strongest (`villager_reactions::UpdateHowImpressed` → `town_stores::AddToBelief` → `guidance::PlayBeliefRemark`; test `GuidanceTest.ATownsPeopleAreAwedByAPlayerTheyBelieveInLessThanTheStrongest`); see ../villager/looks_and_voices.md |
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
| The hidden phone box's recorded messages | pending | the phone box mesh is listed (`src/3D/AllMeshes.cpp`), and the messages are played by the hidden script through the script's own sound functions, which are ported and tested (`ScriptSoundTest.*`); the square wave in the leash picker now starts the script, and its natives that were stubs, `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, are real (`src/CHLApi.cpp`, unverified against v1.42). What is missing is not in the audio: no run has reached the hidden script's calls yet, the scripts row [A hidden script lets the player ring the phone boxes](../scripts/challenge_scripts.md) (partial 40%); see [../easter_eggs/hidden_content.md](../easter_eggs/hidden_content.md) |
