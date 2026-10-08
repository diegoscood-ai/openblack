# Food miracle

The horn of plenty: held at the side of the hand and poured with the right button, it showers grain that lands as food
piles or goes straight into a storage pit. Its extreme version makes speed-up food. Grain on villagers' tables is in
[../resources/](../resources/).

**Progress: 26/32 done, 4 partial — 88%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The horn is held at the side of the hand and pours while the right button is held | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Hand/HandCasting.cpp`): one apply per turn while held, released on letting go |
| The horn holds a finite pool (5000 prayer, 8000 extreme) that is never refilled; the pour ends when it is spent | done | A seed without a worship icon lives off its first charge and nothing refills it (`src/Magic/Core/Chants.cpp`). Our wiki differs: the charge is the miracle's cost to create, 7000 for food and 10000 for extreme food ([page](../../bw1-notes/magic.md#structure-and-data)) |
| Each grain pays 7 per unit: the first brings 200 food, each later one 18 (20 extreme) | done | `src/Magic/Spells/SpellResource.cpp` (ResourceEvent); test `FoodWood.resourceEventCosts`, with the real rows `FoodWood.realInfoDat` |
| A grain becomes food only on dry land and while the miracle has strength; over water it is paid for and lost | done | `src/Magic/Spells/SpellResource.cpp` (paid first; off the map, on water or with no strength nothing lands) |
| The amount is multiplied by the tribal power | done | `src/Magic/Spells/SpellResource.cpp` (amount times the tribal power, truncated) |
| Letting go keeps the rest in the horn; pressing again needs enough for a first grain | done | `src/Magic/Spells/SpellResource.cpp` (HasEnoughChantsForResourceRecast); test `FoodWood.resourceEventCosts` |

## The hand while pouring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips the horn over 4 s along a curve flat at both ends (up about 20 units and 121 degrees), then starts over | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (a natural spline through four key points, height 10 m and tilt 1.07 rad at 1, looping over 4 s); tests `FoodWood.grainSplineKeyPoints`, `FoodWood.grainRaiseLoopsOverTotalTime`. Our wiki differs: the curve is not flat at its ends and peaks at 1.61, so 16.1 m and 1.73 rad ([page](../../bw1-notes/miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)) |
| The tip is a roll about the line to the camera, applied at once | done | `src/ECS/Systems/Implementations/HandPlacement.cpp` (rotates by minus the tilt about the hand to camera axis) |
| The hand is pinned where the pour began | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (ClampedPosition), used by `HandPlacement.cpp` |
| Stopping eases the hand back over one game turn | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (height and tilt blended from the last turn's by the turn fraction) |
| The pour stops whenever the local player's miracle ends | done | `src/Magic/Core/Spell.cpp` (the close-down of a spell this computer cast stops the hand's raise) |

## Grain in the air

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grain streams from the horn, more as the hand moves, falls, lands and fades | done | `src/Particles/Rules/Sprinkle.cpp` (18 grains a second even from a still hand, more as it moves; each landing grain is a spell event); test `FoodWood.sprinkleEmitsAtTheRateEvenWhenStill` |
| The pouring sound loops and fades out softly when let go | done | The particle sound rules (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds`) |

## Landing and piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing grain looks outwards in a spiral of nine cells for something to take it | done | `src/ECS/PotResource.cpp` (AddResourceToPos: nine cells along the spiral, fixed list then mobile list) |
| A storage pit takes it (1.2 times as much) into its own food pile | done | `src/ECS/PotResource.cpp`, `src/ECS/StoragePitStore.cpp`. Our wiki differs: 1.2 is how far a store reaches (its radius times 1.2), not more food ([page](../../bw1-notes/miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)) |
| A worship site's food pot takes it (twice as much) | done | The site's food pot (`src/Worship/WorshipSite.cpp`) is taken like any food pot (`src/ECS/PotResource.cpp`); worship sites only exist after Land 1. Our wiki differs: a pot reaches twice its radius, it does not get twice the food ([page](../../bw1-notes/miracles.md#food-and-wood-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource)) |
| Existing food piles in reach take it in turn, never beyond full | done | `src/ECS/PotResource.cpp` (the first pile that qualifies, not the nearest; clipped at its most only for non-magic piles, as the original's rule) |
| Where nothing takes it and the land is dry, a new food pile appears, with no puff | done | `src/ECS/PotResource.cpp`, `src/Magic/Objects/MagicFood.cpp` |
| A new pile rises out of the ground over one second and is drawn only once above it | done | `src/ECS/Archetypes/PotArchetype.cpp` (the pile's sink, a one-second zoomer) |
| A food pile grows quickly at first and never past full size | done | `src/ECS/ObjectMetrics.cpp` (PileFoodProportionRaised); test `FoodWood.pileHelpers` |
| The grain texture flows while the pile is still sinking | done | `src/ECS/Archetypes/PotArchetype.cpp` (the grain's UV scroll while the pile sinks) |
| Piles sit on the land and follow it | partial | Set on the land when made and morphed with it (`src/ECS/Archetypes/PotArchetype.cpp`); not re-checked in the game |
| A thud plays as food lands: small ones under 200 food, big ones above, heard only within their range | done | `src/ECS/PotResource.cpp` (PileSoundSample, PlayPileSound); test `FoodWood.pileHelpers` |
| A magic food pile receives no projected shadow; a magic wood pile does | done | (added) `src/ECS/Systems/Implementations/RenderingSystem.cpp` (ReceivesDynamicShadow), `src/Graphics/ShadowList.cpp`; see `src/Magic/Objects/MagicFood.cpp` |
| The local player hears the advisor remark on the drop | done | `src/ECS/PotResource.cpp` calls `audio::guidance::PlayResourceDropRemark` (`src/Audio/Services/Guidance.cpp`, the town's needs from `src/ECS/AudioQueries.cpp`), for a new pile |

## Extreme food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The extreme horn starts with more prayer and drops 20 per grain | done | FOOD_PU1's row (`src/Magic/MagicTables.cpp`); test `FoodWood.realInfoDat` |
| A new pile from extreme food is speed-up food and sparkles for as long as it lasts | done | `src/Magic/Spells/SpellResource.cpp` (speed-up, not poisoned), `src/ECS/PotResource.cpp` (SetSpeedUp: the speed-up sparkles for good) |
| Villagers who eat speed-up food work four times as fast for 2550 turns | partial | The villager's speed-up counter and its faster pace are there (`src/ECS/Villager/VillagerCore.cpp`, `src/ECS/VillagerSpeed.cpp`), but nothing sets the counter when a villager takes speed-up food |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new food pile, impressed by how much their town wants food | todo | The grain's look-at-nice-miracle reaction has no villager handler, and a miracle's new pile starts no food reaction (`src/ECS/PotResource.cpp` does not call `ecs::animal_ai::SetupPotReaction`) |
| Villagers take food from the piles | partial | Villagers reacting to a food pile take food from it (`src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`), but a miracle's pile starts no such reaction |
| The creature can learn from the player feeding a storage pit, a worship site or a building site | partial | The storage-pit deed is worked out (`src/ECS/StoragePitStore.cpp`) but the put-down's deed is not passed on (`src/ECS/PotResource.cpp`); no worship-site or building-site deed for a cast |
| Food piles are saved with the game | todo | No game save system |
