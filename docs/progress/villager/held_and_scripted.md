# Villagers in the hand and under script

The player can pick villagers up, carry, drop and throw them, and the challenge scripts can take villagers over to walk
them, play animations and gather crowds. These rows cover the villager's side; the hand and physics own the rest.

**Progress: 8/16 done, 6 partial — 69%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## In the hand and thrown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager can be picked up by the hand, and stops what it was doing | partial | `ecs::living::InterfaceSetInMagicHand` (`src/ECS/LivingPhysics.cpp`) marks it held and sets `IN_HAND`; its previous state is not stored at the pick-up (see ../hand/picking_up.md) |
| A held villager struggles, and the villagers around react | partial | `IN_HAND`'s clip and the scream (`HandSystem::GenericPickupSounds`); the alarm of those around is a TODO in `InterfaceSetInMagicHand` |
| A villager put down carefully stands and goes back to deciding (or becomes a disciple) | partial | a gentle drop stands it up through `LANDED` and it decides again (`src/ECS/LivingPhysics.cpp`, `villager::Landed`); no disciple is made; See disciples.md |
| A thrown villager flies, lands, and gets up or dies from the fall | done | `FLYING`, the landing pose and clip, then it decides or dies of the fall (`src/ECS/LivingPhysics.cpp`); see ../physics/thrown_living.md |
| A villager dropped in the sea drowns | done | `DROWNING` in a water cell (`src/ECS/VillagerDrowning.cpp`) |
| Picking up and throwing villagers moves the player's alignment | partial | a villager the hand throws or drowns to death moves its owner by that reason (`alignment::UpdateForDeath`); picking up and throwing alone move nothing; See ../worship/ |
| A villager can be picked up by the creature and controlled by it | partial | the creature can pick up and hold villagers (`CreatureObjectActionSystem::CanPickUp`); `CONTROLLED_BY_CREATURE` is a TODO row; See ../creature/ |
| A villager carried off by a tornado | done | the tornado carries villagers and lets them go to die where they land (`src/Particles/Rules/Storm.cpp`); See ../miracles/ |

## Under script

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script moves a villager to a place and waits until it arrives | done | MOVE_GAME_THING's walk (`src/ECS/Villager/VillagerScript.cpp`, `src/CHLApi.cpp`); `test/test_script_villager_commands.cpp` |
| A script plays an animation on a villager and waits for it to finish | done | `SCRIPT_PLAY_ANIM` and `WAIT_FOR_ANIMATION`, PLAYED (`src/ECS/Villager/VillagerScript.cpp`) |
| A script makes a villager wander round a point | todo | `SCRIPT_WANDER_AROUND_POSITION` is a TODO row of the state table |
| A script makes a villager walk along a path | done | WALK_PATH along a camera track (`src/CHLApi.cpp`); `SCRIPT_GO_AND_MOVE_ALONG_PATH` is a TODO row |
| A script puts villagers in a crowd | partial | the script flocks and containers are ported (`src/ECS/ScriptContainers.cpp`, `src/ECS/Villager/VillagerFlock.cpp`; `test/test_script_flocks.cpp`); `SCRIPT_IN_CROWD` is a TODO row |
| A script releases a villager, which goes back to its own life | done | RELEASE_FROM_SCRIPT (`villager::ReleaseFromScript`, `src/ECS/Villager/VillagerScript.cpp`); `test/test_release_from_script.cpp` |
| Script-made dances (the dance editor's dances) | todo | DANCE_CREATE is a stub (`src/CHLApi.cpp`) |
| Script asks whether a villager is drowning, at home, a child and the like | done | GET_PROPERTY's living properties (drowning, age and the rest, `src/CHLApi.cpp`); `test/test_script_living_props.cpp`; See ../story/ |
