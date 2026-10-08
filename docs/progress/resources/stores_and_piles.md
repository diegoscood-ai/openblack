# Stores and piles

Where food and wood are kept: each town's store (its storage pit) with its piles, loose piles and pots on the land, and
the piles the miracles make. The game has only these two resources.

**Progress: 17/24 done, 1 partial — 73%**

How the original does it, in our wiki: [Objects and resources](../../bw1-notes/objects-and-resources.md).

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A store has one food pile and five wood piles, at the points its model marks, turned with it | done | the store's piles made at its mesh's points (`src/ECS/Archetypes/AbodeArchetype.cpp`) |
| Wood fills the piles in turn; every pile but the last holds no more than it is drawn full at | done | `StoragePitStore::AddResource`: wood piles one to five, the last with no cap (`src/ECS/StoragePitStore.cpp`) |
| The town keeps count of the food and wood its store holds | done | `StoragePitStore::SyncTotals` keeps the abode's totals with the piles |
| A store can hold more than its maximum for a while (unconfirmed what happens to the excess) | done | `StoragePitStore::AmountOverMaximum` (the last wood pile has no cap; what is over is taken first) |
| A damaged store causes an emergency in its town | todo | see `../town/` |
| A damaged store stops working until repaired | todo |  |
| A store's contents can be poisoned, tinting its piles | done | the poison flag on the store's piles (`object_resources::IsPoisoned`) and the poisoned colour of a pile (`src/ECS/Systems/Implementations/RenderingSystem.cpp`); see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| The store's desire flags stand by it | todo | only the workshop has its needs sign (`src/ECS/ShowNeeds.cpp`); see `../town/` |
| Villagers' taking food and wood from the store shrinks its piles | done | villagers take from the store (`GetResourceFrom`, `src/ECS/Villager/VillagerResources.cpp`) through `StoragePitStore::RemoveResource`, wood piles five to one |
| Only food and wood exist; there is no ore | n/a | the game has no other resource |

## Piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pile rises out of the ground as it fills and sinks as it empties, easing over a second | done | `PotArchetype::SetSize` with its zoomer (`src/ECS/Archetypes/PotArchetype.cpp`, `src/ECS/PotResource.cpp`) |
| Food rises fast at first then slows; wood rises evenly; neither beyond full | done | `pot_resource::PileFoodProportionRaised` and the wood's even rise; test `test/test_food_wood.cpp` |
| A pile is drawn only while some of it is above ground | done | `src/ECS/Systems/Implementations/RenderingSystem.cpp` |
| The grain on a food pile flows down it as it rises | done | test `FoodWood.grainSplineKeyPoints` |
| Food piles follow the land's shape | done | `MorphWithTerrain` on food piles (`src/ECS/Archetypes/PotArchetype.cpp`) |
| A pile thuds as it is made or added to, louder for 200 or more; piles in the hand don't | done | the pile sound by amount (`PileSoundSample`, `src/ECS/PotResource.cpp`) |
| Food and wood poured at a point go to the stores and piles in its cell, then the cells around, as far as each reaches | done | `pot_resource::AddResourceToPos` (`src/ECS/PotResource.cpp`) |
| What nothing takes makes a new pile, unless it lands in water | done | `pot_resource::AddResourceToPos` |
| A new pile makes the nearby people come and look | done | a pot made or put down offers its reaction (`src/ECS/PotResource.cpp`, `HandHolding.cpp`) |
| Pots and piles placed by the land scripts | done | `CreatePot` (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A town's temporary pots placed by the land scripts | todo | `CreateTownTemporaryPots` is empty (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A pile or pot with something in it can catch fire | partial | piles and pots burn (`src/ECS/Fire`), and emptying one puts its fire out (`src/ECS/ObjectResources.cpp`); unconfirmed whether the food or wood is lost |
| Pots are physical: they can be thrown, roll and land | done | pots are physics objects (`src/ECS/Physics/PhysicsObjects.cpp`) and react to their impact (`src/ECS/HeldApply.cpp`); see `../physics/` |
| An emptied pile goes (unconfirmed) | todo | an emptied pile stays at 0, its reaction removed (`src/ECS/ObjectResources.cpp`); whether a loose one goes is unconfirmed |
| Hovering over a pile shows how much it holds | todo | the hover texts are not ported; see `../interface/` |
