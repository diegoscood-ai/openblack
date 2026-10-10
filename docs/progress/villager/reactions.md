# Villager reactions

Villagers react to what happens around them: they flee frightening miracles and watch pleasing ones, gather round
the creature, fight fires, run from predators, point at flying objects, mourn the dead and are impressed, which gives
their town belief. Each reaction has a reach, an urgency and a time from the reaction table.

**Progress: 11/44 done, 5 partial — 31%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## How reactions work

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A reaction spreads to the villagers in its reach, each taking it up by urgency and distance | done | `effects::reactions::CreateReaction` spreads it once over its cells (`src/ECS/Effects/Reactions.cpp`) to the villagers' handler, scored by priority and distance (`src/ECS/Systems/Implementations/VillagerReactions.cpp`); `test/test_villager_reactions.cpp` |
| A villager remembers the last few kinds it reacted to and won't react to the same kind again too soon | done | the per-villager reaction records (`components::ReactionRecords`, `src/ECS/Effects/Reactions.cpp`) |
| A more urgent reaction of another kind takes over the current one | done | the switch rule from the current reaction (`src/ECS/Effects/Reactions.cpp`, `villager_reactions::ApplyReaction`) |
| Only villagers able and alive, in a state that allows it, react | done | the availability and the state's own answer (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| After reacting, the villager goes back to what it was doing | done | `ResetStateAfterReacting` and `StopReactingAndSetState` (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Impressive things give belief to the villager's town, with a belief symbol rising from it | todo | how impressed a villager is feeds no belief yet (TODO in `villager_reactions::AddReaction`); see ../town/belief_and_conversion.md |
| Awed villagers' voices are heard now and then near the hand | partial | the guidance's alignment remarks play (`src/Audio/Services/Guidance.cpp`); the awe that should time them is not ported; see ../audio/voices_and_speech.md |

## Miracles and magic

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flee a frightening miracle, straight away or across its path, then turn to watch it from a distance | todo | the miracle reaction states (`FLEEING_FROM_OBJECT_REACTION`, `FLEEING_AND_LOOKING_AT_OBJECT_REACTION`) are TODO rows; only fire, teleport, shield, death, food and wood reach a villager |
| Turn to watch a nice or impressive miracle | todo | `LOOKING_AT_OBJECT_REACTION` is a TODO row |
| Walk to a magic shield, stand amazed under it looking out while the town wants protection | partial | `src/ECS/Systems/Implementations/VillagerShield.cpp` (state 168, with its clip), gated by the town's protection desire, which stays 0, so only the homeless react in a normal game |
| React when a shield is struck or destroyed | todo | the shield's struck and destroyed reactions reach no villager |
| Walk (or run) into a teleport stone, jump and carry on | done | `src/ECS/Systems/Implementations/VillagerTeleport.cpp`; `test/test_teleport.cpp` |
| Be bewildered by a magic tree: turn to it and stare | todo | the magic tree states are TODO rows |
| Run to magic food or wood dropped nearby and take it | done | the food and wood reactions (`REACT_TO_FOOD`, `REACT_TO_WOOD`, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); `test/test_villager_reactions.cpp` |
| React to a fight won | todo |  |

## Fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers near a blaze react; their own town's come to beat it out when needed, the rest go round it | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`; `test/test_fire.cpp` |
| Firemen fetching water to put out fire give up at once, as the game does | done | `PutOutFireWithWater` goes straight to deciding (`src/ECS/Systems/Implementations/VillagerFire.cpp`) |
| A villager set on fire runs about until it is out or dies | done | `ON_FIRE` (`src/ECS/Systems/Implementations/VillagerFire.cpp`) |
| Villagers run to their burning abode | todo | choosing the town's abode on fire is not ported |
| A burning object in the hand alarms the villagers below | partial | picking up a burning thing makes the burning-object-in-hand reaction (`fire::StartedMoving`, `src/ECS/Fire/FireEffect.cpp`), but no villager handles it; see ../physics/fire.md |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers turn to face the creature, approach it and inspect it | todo | the creature reaction states are TODO rows |
| They respect it, worship it, or flee it, by their town's attitude to the creature | todo |  |
| A villager is scared stiff by a frightening creature | todo | `MAKE_SCARED_STIFF` and `SCARED_STIFF` are TODO rows |
| Villagers walk towards the creature when it does something interesting | todo |  |
| A villager controlled by the creature (picked up or led) does as it is made to | todo | `CONTROLLED_BY_CREATURE` is a TODO row; See ../creature/ |
| Villagers crowd round a creature fight and watch it | todo |  |

## The hand and objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Approach the hand when it is near | todo | `APPROACH_HAND_REACTION` is a TODO row |
| React to a villager in the hand | todo | the alarm at a villager in the hand is a TODO in `InterfaceSetInMagicHand` (`src/ECS/LivingPhysics.cpp`) |
| React to something dropped by the hand, or put in the storage pit | todo |  |
| Watch and point at a flying object thrown overhead | todo | the flying-object reaction is spread (`animal_ai::SpreadFlyingObjectReaction`), but no villager handles it; see [../physics/thrown_living.md](../physics/thrown_living.md) |
| Get out of the way of a falling tree | todo |  |
| Inspect an object new to them, and tell others about it | todo | the inspect and tell states are TODO rows |
| React to a newly built building and to a new scaffold | partial | the scaffold reaction is made (`scaffolds::BuildBuilding`) but no villager handles it; the new building reaction is not made yet (`abodes::Built` only logs it) |
| Run after the ball and pick it up | todo | See play_and_gossip.md |
| React to an object crushed nearby | partial | an effect's crush makes the crushed reaction (`src/ECS/Effects/EffectValues.cpp`), but no villager handles it |

## People and animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Flee a predator, then look to see if it is safe | todo | `FLEEING_FROM_PREDATOR_REACTION` and `LOOK_TO_SEE_IF_IT_IS_SAFE` are TODO rows; the predators' flee reaction reaches only animals (`src/ECS/AnimalFlee.cpp`) |
| Go and hide in a nearby building when danger comes | todo | `GO_AND_HIDE_IN_NEARBY_BUILDING` is a TODO row |
| Point at, go towards, look at and mourn a dead villager | done | `REACT_TO_DEATH` and the states 205 to 208 (`src/ECS/Villager/VillagerMourning.cpp`); See death.md |
| Faint, panic and be confused at shocking sights | todo | `FAINTING_REACTION`, `PANIC_REACTION` and the confused states are TODO rows |
| Crowd round to watch something | todo | `CROWD_REACTION` is a TODO row |
| React to a breeder | todo | `REACT_TO_BREEDER` is a TODO row |
| Cheer the town's celebration when it is won over | todo | See ../town/belief_and_conversion.md |
| React to a missionary | todo |  |
| Fight a villager of another town | todo | the fight reaction states are TODO rows |
