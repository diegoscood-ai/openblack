# Wood miracle

Held at the side of the hand like the horn, the wood miracle pours logs for four seconds; they land as wood piles or
go into a storage pit, a workshop or a building site. There is no extreme version. Wood used for building is in
[../resources/](../resources/).

**Progress: 17/23 done, 4 partial — 83%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wood is held at the side of the hand and pours while the right button is held | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Hand/HandCasting.cpp`) |
| A pour lasts 4 s (the player's timer) | done | The player's timer from the WOOD row (`src/Magic/MagicTables.cpp`), aged in `src/Magic/Core/Spell.cpp` |
| It holds a finite pool that is never refilled | done | A seed without a worship icon lives off its first charge (`src/Magic/Core/Chants.cpp`) |
| Each drop pays 3 per unit: the first brings 500 wood, each later one 20 | done | `src/Magic/Spells/SpellResource.cpp` (ResourceEvent); tests `FoodWood.resourceEventCosts`, `FoodWood.realInfoDat` |
| Wood lands only on dry land; the amount is multiplied by the tribal power | done | `src/Magic/Spells/SpellResource.cpp` (dry land only; amount times the tribal power) |
| Pressing again needs enough prayer for a first drop | done | `src/Magic/Spells/SpellResource.cpp` (HasEnoughChantsForResourceRecast) |
| There is no extreme version | n/a | the game has none |

## The hand and the logs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips over 4 s exactly as for food, rolling about the line to the camera | done | `src/ECS/Systems/Implementations/HandGrain.cpp`, `HandPlacement.cpp` (the same raise as food); test `FoodWood.grainRaiseLoopsOverTotalTime` |
| Stopping eases the hand back over one game turn | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (blended from the last turn's by the turn fraction) |
| Logs fall from the hand tumbling about the axis across their movement | done | `src/Particles/Rules/Sprinkle.cpp` (the tumble appearance rule), logs drawn by `src/Particles/Creators/Mesh.cpp` |
| The pouring sound loops and fades out softly when let go | done | The particle sound rules (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds`) |

## Where the wood goes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing log looks outwards in a spiral of nine cells for something to take it | done | `src/ECS/PotResource.cpp` (AddResourceToPos) |
| A storage pit takes it into its wood piles in order | done | `src/ECS/PotResource.cpp`, `src/ECS/StoragePitStore.cpp` |
| A workshop, building site or scaffold takes it | partial | Building sites and workshops keep their wood in a wood pile (`src/ECS/Town/BuildingSites.cpp`), which takes a log like any pile; they are not stores of their own in `src/ECS/PotResource.cpp`, and a pit under construction does not pass its wood on |
| Existing wood piles take it, never beyond full | done | `src/ECS/PotResource.cpp` (the first pile that qualifies) |
| Where nothing takes it a new wood pile appears and rises out of the ground over a second | done | `src/Magic/Objects/MagicWood.cpp`, `src/ECS/Archetypes/PotArchetype.cpp` (the one-second rise) |
| A wood pile grows evenly with what it holds, never past full size | done | `src/ECS/ObjectMetrics.cpp` (the wood pile's proportion raised) |
| A wood pile never rises above the ground and follows the land | partial | Set on the land when made (`src/ECS/Archetypes/PotArchetype.cpp`); not re-checked in the game |
| Thuds play as wood lands, small under 200 and big above, heard only within their range | done | `src/ECS/PotResource.cpp` (PileSoundSample); test `FoodWood.pileHelpers` |
| The local player hears the advisor remark on the drop | done | `src/ECS/PotResource.cpp` calls `audio::guidance::PlayResourceDropRemark` (`src/Audio/Services/Guidance.cpp`), for a new pile |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new wood pile, impressed by how much their town wants wood | todo | The log's look-at-nice-miracle reaction has no villager handler, and a miracle's new pile starts no wood reaction (`src/ECS/PotResource.cpp`) |
| Villagers fetch the wood for building | partial | Villagers fetch wood for building sites (`src/ECS/Systems/Implementations/VillagerBuildingSites.cpp`) and pick up wood they react to (`VillagerResourceReactions.cpp`), but a miracle's pile starts no wood reaction |
| The creature can learn from the player putting wood in a storage pit or by a building site | partial | The storage-pit deed is worked out (`src/ECS/StoragePitStore.cpp`) but the put-down's deed is not passed on (`src/ECS/PotResource.cpp`); no building-site deed for a cast |
| Wood piles are saved with the game | todo | No game save system |
