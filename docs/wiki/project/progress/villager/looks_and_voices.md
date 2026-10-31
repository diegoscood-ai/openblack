# Villager looks, voices and special villagers

How villagers look and sound: their tribe's models, their animations, what they carry, their names, the special
characters that wander the lands, and the voices of villagers talking and praising the player.

**Progress: 13/15 done, 0 chosen, 1 partial, 0 inferred — 88%**

How the original does it, in our wiki: [Skeletal animation (villagers and animals)](../../../beings/animation.md), [Villagers: data, state machine and speed](../../../beings/villager-states.md).

## Models and drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Women, men and children each have their own models | done | the info's adult and child meshes (`src/ECS/Archetypes/VillagerArchetype.cpp`, `villager::SetAge`); tests `VillagerAge.AVillagerGivenAnAgeWearsItsKindsModelsForItAtEveryDistance`, `VillagerAge.AChildGivenAGrownAgeWearsItsAdultModel`, `VillagerAge.AScriptMakingAGrownVillagerYoungGivesItTheChildsModel` |
| Villagers animate: walking synced to the ground covered, working, eating, dancing, idling, dying | done | the state's clip, the walk synced to the ground covered (`src/ECS/VillagerAnimations.cpp`, `src/ECS/Animations.cpp`); `test/test_villager_draw_golden.cpp` |
| Clips play into and out of states, and the state waits for them | done | the into and out-of clips, with the state waiting for them (`src/ECS/VillagerAnimations.cpp`); tests `VillagerAnimation.EveryStateChoosesItsClipsAsTheStateTableDoes`, `VillagerCoreTest.OutOfClipIntoAStateWithNoIntoClip` |
| What a villager carries shows in its hand (tools, logs by tree type, food, the ball) | done | the carried object on the hand bone (`src/ECS/CarriedProps.cpp`); See tools_and_carried_items.md |
| Villagers are drawn smoothly between game turns and lean with slopes | done | the smooth drawing between turns (`components::DrawPosition`, `src/ECS/MobileDrawing.cpp`) and the axes from the ground (`src/ECS/LivingAngle.cpp`); `test/test_drawn_position.cpp`, `test/test_living_axes.cpp` |
| Each foot casts a short ground shadow | done | `src/Graphics/GroundBlobs.cpp`; `test/test_ground_blobs.cpp`; see ../rendering/ |
| Poisoned villagers show they are poisoned | done | `SHOW_POISONED` and its clip (`src/ECS/Villager/VillagerFood.cpp`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md); tests `VillagerFoodTest.EatingStates`, `VillagerFoodTest.APoisonedVillagerShowsItAfterAMealAtHomeSteppingOutFirst`, `VillagerAnimation.APoisonedVillagerPausesLookingPoisonedWithoutADraw` |
| Disciple icon over a disciple's head | done | ported from raffclar PR #261 (`src/ECS/DiscipleMarks.cpp`); its place, size and yellow checked against v1.42 (2026-10-20): see disciples.md; tests `DiscipleRules.Symbols`, `DiscipleRules.TheSymbolIsYellow`, `DiscipleRules.GlowAndSymbolStandOverTheDrawnVillager` |
| Villagers at home are hidden | done | a villager inside is out of the map and not drawn (`src/ECS/Villager/VillagerHome.cpp`) |

## Names and information

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager's details show on request: the details key toggles them for the villager under the hand, or for all when none is | pending | the editor's inspector shows the component (`src/Editor`), not the game's display; what the original draws there is not traced ([screen and readouts](../../../interface/screen-and-readouts.md)); named villagers and their names: [special_villagers.md](special_villagers.md) |

## Special villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Story characters are villagers driven by the challenge scripts | partial 20% | the scripts move, animate and release villagers (`src/ECS/Systems/Implementations/VillagerScript.cpp`) and draw the intro family in high detail (`src/ECS/SuperVillager.cpp`), the share counted here; the characters' challenges have not been played through against the original (the trainer's, the lost brother's and his sister's, and the sculptor's are played in openblack by a quest run of Choose Your Creature played to the choice (quest=SetupLand1/ProtectGateKeysFlag=1/ProtectGateKeys/ChooseYourCreature, 2026-10-27)); each character's row is in [special_villagers.md](special_villagers.md); See ../story/ |

## Voices and speech

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Awed villagers praise or fear the player aloud, good or evil, more awed as their town nears being won | done | the guidance's alignment and desire remarks (`src/Audio/Services/Guidance.cpp`), and the awe: an impressed villager adds belief with itself as the thing (`villager_reactions::UpdateHowImpressed` → `town_stores::AddToBelief`), and while the player is believed in less than the town's strongest the people say one of the three good or evil awe lines, the stronger the nearer the player's belief is to the strongest (`guidance::PlayBeliefRemark`; test `GuidanceTest.ATownsPeopleAreAwedByAPlayerTheyBelieveInLessThanTheStrongest`); see ../audio/voices_and_speech.md |
| Villagers babble as they go about, in a man's, a woman's or a child's voice, the same for every tribe | done | the one banter bank from the clips' sound events, by man, woman or child (`src/Audio/Services/AnimationSounds.cpp`); tests `ClipEventVoice.*`, `AnimEffectsTest.BanterAtTheAbodeWithTheVillagersDistance`; the original has no tribe's babble ([world sounds](../../../audio/world-sounds.md#clip-sounds)); see ../audio/voices_and_speech.md |
| Villagers shout and scream when fleeing, burning, thrown, picked up or dying | done | the original has no scream of its own for these besides the pick-up's and the sacrifice's: they are the clips' sound events ([world sounds](../../../audio/world-sounds.md#villagers-buildings-and-trees-own-sounds)), played from every clip a ported state plays (`src/Audio/Services/AnimationSounds.cpp`), the pick-up scream from the hand; tests `ClipEventVoice.*`, `AnimationSoundsTest.*`; the states still missing are in reactions.md; See ../audio/ |
| Villagers call out lines when the town needs something | done | the town desire remarks (`src/Audio/Services/Guidance.cpp`); test `GuidanceTest.DesireSampleAndScore`; no tribe-specific line was found: the lines go by the desire ([voices](../../../audio/voices.md)); see ../audio/voices_and_speech.md |
