# Villager looks, voices and special villagers

How villagers look and sound: their tribe's models, their animations, what they carry, their names, the special
characters that wander the lands, and the voices of villagers talking and praising the player.

**Progress: 8/19 done, 5 partial — 55%**

How the original does it, in our wiki: [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md), [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## Models and drawing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each villager draws its tribe's model for its job, high or low detail by distance | n/a | the job's mesh of the tribe (`src/ECS/Archetypes/VillagerArchetype.cpp`). Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../../bw1-notes/rendering-objects.md#villager-blobs-object-reflections-and-lod)) |
| Women, men and children each have their own models | done | the info's adult and child meshes (`src/ECS/Archetypes/VillagerArchetype.cpp`, `villager::SetAge`) |
| Villagers animate: walking synced to the ground covered, working, eating, dancing, idling, dying | done | the state's clip, the walk synced to the ground covered (`src/ECS/VillagerAnimations.cpp`, `src/ECS/Animations.cpp`); `test/test_villager_draw_golden.cpp` |
| Clips play into and out of states, and the state waits for them | done | the into and out-of clips, with the state waiting for them (`src/ECS/VillagerAnimations.cpp`) |
| What a villager carries shows in its hand (tools, logs by tree type, food, the ball) | done | the carried object on the hand bone (`src/ECS/CarriedProps.cpp`); See tools_and_carried_items.md |
| Villagers are drawn smoothly between game turns and lean with slopes | done | the smooth drawing between turns (`components::DrawPosition`, `src/ECS/MobileDrawing.cpp`) and the axes from the ground (`src/ECS/LivingAngle.cpp`); `test/test_drawn_position.cpp`, `test/test_living_axes.cpp` |
| Each foot casts a short ground shadow | done | `src/Graphics/GroundBlobs.cpp`; `test/test_ground_blobs.cpp`; see ../rendering/ |
| Poisoned villagers show they are poisoned | done | `SHOW_POISONED` and its clip (`src/ECS/Villager/VillagerFood.cpp`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Disciple icon over a disciple's head | todo | See disciples.md |
| Villagers at home are hidden | done | a villager inside is out of the map and not drawn (`src/ECS/Villager/VillagerHome.cpp`) |

## Names and information

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only named villagers have a name (about 2 in 10 new villagers; see [special_villagers.md](special_villagers.md)), shown under the hand | todo | the key bindings exist (`src/Gui/GameMenu.cpp`); special villagers are not ported (`RollSpecialVillager`, `src/ECS/Villager/VillagerCore.cpp`), so nobody has a name |
| A villager's details show on request | todo | the editor's inspector shows the component (`src/Editor`), not the game's display |
| Tooltip for what a villager is doing | todo | See ../interface/ |

## Special villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Now and then a created villager is a special one with its own look and name (hermit, sculptor, piper, sailor, priest, healer and the rest) | todo | the roll is drawn (`RollSpecialVillager`) but a normal villager is always made; see [special_villagers.md](special_villagers.md) |
| Special villagers speak with a speech bubble | todo |  |
| Story characters are villagers driven by the challenge scripts | partial | the scripts move, animate and release villagers (`src/ECS/Villager/VillagerScript.cpp`) and draw the intro family in high detail (`src/ECS/SuperVillager.cpp`); See ../story/ |

## Voices and speech

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Awed villagers praise or fear the player aloud, good or evil, more awed as their town nears being won | partial | the guidance's alignment and desire remarks (`src/Audio/Services/Guidance.cpp`); the impression that should drive them is not ported (`villager_reactions::AddReaction`); see ../audio/voices_and_speech.md |
| Villagers talk to each other in their tribe's babble | partial | the banter bank from the clips' sound events, by man, woman or child (`src/Audio/Services/AnimationSounds.cpp`); see ../audio/voices_and_speech.md |
| Villagers shout and scream when fleeing, burning or dying | partial | the thrown screams and the pick-up scream from the clips' sound events and the hand (`src/Audio/Services/AnimationSounds.cpp`); See ../audio/ |
| Villagers call out tribe-specific lines when the town needs something | partial | the town desire remarks (`src/Audio/Services/Guidance.cpp`); see ../audio/voices_and_speech.md |
