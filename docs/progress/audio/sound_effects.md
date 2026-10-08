# Sound effects

The game's sound effects: the hand, miracles, the temple, buildings, trees, villagers, animals and the creature, each
from the game's sound banks and most placed in the world.

**Progress: 23/32 done, 6 partial — 81%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../bw1-notes/audio.md).

## How effects play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An effect plays with its bank's volume, pitch, random pitch change, loop count and distances | done | the play variants in `src/Audio/Game/GameSfx.cpp` on the 16 channels of `src/Audio/Engine/SamplePlay.cpp` (bank fields override the caller's where flagged; start pitch with its random deviation); tests `SamplePlayTest.*`, `FixedClockAudio.*` |
| Animation effects pick a sound by keys (action, size, surface, alignment …) at random | done | `src/Audio/Engine/AnimEffects.cpp` (key rows and random sample lists of each bank); tests `AnimEffectsTest.*` |
| A sound marked to play once doesn't start again while its owner or its voice group plays it | done | play mode 2 and clone groups in `src/Audio/Engine/SamplePlay.cpp`; test `SamplePlayTest.ModesTwoAndThree` |
| Loops follow their owner and stop with it | done | tracked channels and sound tags (`src/Audio/Services/SoundTags.cpp`); tests `SoundTagTest.GoneThingReleasesItsLoop`, `SoundTagTest.GoneThingOneShotGoesAtOnce` |
| Particle effects start, fade and let go of their sounds | done | `src/Audio/Services/SpellSounds.cpp`, `src/Particles/SoundAction.cpp`, `src/Particles/Rules/Sound.cpp`; tests `SpellSounds.*`, `SamplePlayTest.OwnerChannelAndTheFadeOfAPSysSound` |
| Sounds on objects (sound tags) restart while the camera is in reach | done | `src/Audio/Services/SoundTags.cpp`; tests `SoundTagTest.ThingTagMode2ReplaysOnlyWhenSilent`, `SoundTagTest.ThingTagMode3RestartsEveryTurn` |
| A sound at a distant point arrives late, at the speed of sound | done | delayed point tags in `src/Audio/Services/SoundTags.cpp`; test `SoundTagTest.DelayedPointTagWaitsForTheSound` |
| A sound effects volume setting | done | `audio::SetSampleMainVolume` through the options box slider (`src/Game.cpp`) and the debug Audio Player; not saved to disk yet |
| Scripts play, stop and ask about sounds and attach them to objects | done | PLAY_SOUND_EFFECT, STOP_SOUND_EFFECT, ATTACH_SOUND_TAG, DETACH_SOUND_TAG, GAME_SOUND_PLAYING, SOUND_EXISTS, SET_GAME_SOUND in `src/CHLApi.cpp`, `src/Audio/Services/ScriptSound.cpp`; tests `ScriptSoundTest.*` |

## By kind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand: gripping the land, picking up, dipping in water, passing through influence | done | grip and splash in `src/ECS/Systems/Implementations/HandFish.cpp`, the pick-up loop of food and wood in `HandEffects.cpp`, influence edge crossing in `src/ECS/Influence/InfluenceCircles.cpp`; tests `UiSfxTest.InfluenceCrossing*` |
| Gestures and casting: recognised, failed, power up bands, bubbles popping | done | `src/Particles/Rules/Gesture.cpp`, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Hand/HandMagicFX.cpp`, `src/Magic/Core/SpellSeed.cpp`, `src/Worship/WorshipSpellIcon.cpp` |
| Miracles: fireball, lightning, shields, forests growing, teleport, water | done | the spells bank through the particle sound actions (`src/Audio/Services/SpellSounds.cpp`), `src/Particles/Rules/Fireball.cpp`, `src/Magic/Objects/MagicTeleport.cpp`, `src/Magic/Core/OneOffSpellSeed.cpp`; see ../miracles/ |
| Fire crackling and steam | done | `src/ECS/Fire/FireSound.cpp` (crackle following the burning thing), steam in `src/ECS/Fire/FireGraphic.cpp` |
| Thunder claps with lightning | partial | the storm miracle's thunder (`src/Particles/Rules/Storm.cpp`) and the rain bank's thunder one-shots (`src/Audio/Services/AtmosBanks.cpp`) play; the weather's own thunder at a strike's point is not ported; see ../weather/storms.md |
| The camera's whoosh when moving fast | done | `src/Camera/DefaultWorldCameraModel.cpp`, `src/Camera/TempleCameraModel.cpp` |
| The temple: doors, buttons, scrolls squeaking, the cave's waterfall and fire | partial | doors, buttons, scrolls and the scroll whoosh play (`src/3D/Implementations/TempleInterior.cpp`, `src/3D/TempleToggles.cpp`, `TempleSounds.h`); the creature cave's waterfall and fire are still silent; see ../temple/ |
| The leash: attaching and clicking | done | `src/ECS/Systems/Implementations/LeashSystem.cpp` |
| Lanterns crackling | done | `src/Audio/Services/LanternSounds.cpp` (a sound tag on each lit street lantern) |
| Trees rustling, bending, falling, breaking and being turned to mulch | partial | rustle and bend (`src/ECS/Trees.cpp`, `AnimationSounds::PlayFromTable`), breaking when uprooted (`HandHolding.cpp`), mulch at a store (`src/ECS/ObjectDelivery.cpp`); the falling tree's sounds are not played |
| Planting trees, scaffolds appearing, ready and tapped, the workshop | done | tree planting (`HandPhysics.cpp`), scaffold planting, tapping and joining (`src/ECS/Scaffolds.cpp`), the workshop's work loop and ready horn (`src/ECS/Town/Workshops.cpp`); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Food and wood piles and picking them up | done | the pile sound by type and amount (`src/ECS/PotResource.cpp`), the hand's pick-up loop (`HandEffects.cpp`) |
| Villagers: footsteps, work (axe, saw, hammer), screams, babies crying, the village bell | partial | footsteps by surface, work and thrown screams from the clips' sound events (`src/Audio/Services/AnimationSounds.cpp` from `src/ECS/Animations.cpp`), banter, the birth sound (`src/ECS/Villager/VillagerBirth.cpp`); the village bell and the totem are not played; see ../villager/looks_and_voices.md |
| Crowds impressed, or losing belief | todo | the impressed reactions are not ported (`LivingActionSystem.cpp` lists them as todo) |
| Animals: their calls and footsteps by species | done | the same clip sound events as the villagers, with each species' sound group (`src/Audio/Services/AnimationSounds.cpp`); see ../animal/ |
| The creature: breath, footsteps by ground, roars, snores, sneezes, eating, pooing, fight blows | done | `src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`, `src/Creature/CreatureAudio.cpp`, run by `src/ECS/CreatureLoop.cpp`; tests `CreatureAudio.*`, `CreatureAudioSystemTest.*` |
| Objects landing and breaking by what they are and what they hit | done | `src/ECS/Physics/CollisionSounds.cpp` (editor bank key by what hits what), `src/ECS/Rocks.cpp`, building crash in `src/ECS/Abodes.cpp`; see ../physics/ |
| Rewards, chests, the reward sting and the acknowledgement of a command | partial | the scroll and signpost taps play (`src/ECS/ScriptHighlight.cpp`); no chest, reward sting or command acknowledgement |
| The hand at the edge of the world | todo | no camera bounds in our tree, so the out-of-bounds sound is never played |
| The advisors' slapstick (knocking the glass, slapping, a fart, a gun) | todo | the advisors' own sound effects are not played; see voices_and_speech.md |
| Windmills and running water | partial | the running water ambience bank (`src/Audio/Services/SoundMap.cpp`) and the water flow tags of script markers (`src/ECS/DesignedScenery.cpp`); windmills are silent |
| The story's own sound effects bank | done | the script bank (ScriptSfx) through PLAY_SOUND_EFFECT (`src/Audio/Services/ScriptSound.cpp`); test `ScriptSoundTest.PlayOwnsItsChannelByTheSampleNumber` |
| The menu's button click | done | `src/Gui/GameInterface.cpp`; test `UiSfxTest.InGameRecordsOfTheInterface` |
