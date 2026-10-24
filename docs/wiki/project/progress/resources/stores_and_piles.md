# Stores and piles

Where food and wood are kept: each town's store (its storage pit) with its piles, loose piles and pots on the land, and
the piles the miracles make. The game has only these two resources.

**Progress: 9/24 done, 0 chosen, 11 partial, 2 inferred — 75%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A store has one food pile and five wood piles, at the points its model marks, turned with it | partial 70% | Missing: the original makes a pile when something goes in and deletes it when emptied; ours makes all six with the pit (#86); the store's piles made at its mesh's points (`src/ECS/Archetypes/AbodeArchetype.cpp`) |
| Wood fills the piles in turn; every pile but the last holds no more than it is drawn full at | done | Test `StoreRules.APileGivesOnlyWhatIsWithinItsStoresMaximum`; `StoragePitStore::AddResource`: wood piles one to five, the last with no cap (`src/ECS/StoragePitStore.cpp`) |
| The town keeps count of the food and wood its store holds | done | Test `VillagerResourcesTest.PickupDropAndTotals`; `StoragePitStore::SyncTotals` keeps the abode's totals with the piles |
| A store can hold more than its maximum for a while; what is over is taken first | done | Test `TakeResourceTest.WhatAStoreHoldsOverItsMaximumIsTakenFirst` (`test/test_take_resource.cpp`: wood over five full piles, food over one, and a pile asked gives only what is not over, the store the rest from its last pile, as [the store](../../../world/objects.md#store-storagepit)); `StoragePitStore::AmountOverMaximum` (the last wood pile has no cap; what is over is taken first) |
| A damaged store causes an emergency in its town | pending | see `../town/` |
| A damaged store stops working until repaired | inferred | Not checked in the original |
| A store's contents can be poisoned, tinting its piles | done | Test `ScriptPoisonNatives.AStoragePitIsPoisonedByItsPilesAndOnlyItsFoodPileIsEverPoisoned`; the poison flag on the store's piles (`object_resources::IsPoisoned`) and the poisoned colour of a pile (`src/ECS/Systems/Implementations/RenderingSystem.cpp`); see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| The store's desire flags stand by it | partial 90% | at the store's special points 12 + slot (test `TownDesireFlags.AFlagFliesAtTheStoresPointAfterTheFirstTwelve`), moved there as the pit is made, a plan's or a whole one (`TownDesireSystem::OnStoragePitMade` from `AbodeArchetype::Create`), as the v1.42 exe's `StoragePit::Create` does ([the desire flags](../../../beings/towns.md#the-desire-flags), read in the exe); no test of the move as the pit is made: the move reads the store mesh's points from the mesh cache and runs only inside the abode's making, so a test needs a mesh with its points and a whole abode made; see `../town/` |
| Villagers' taking food and wood from the store shrinks its piles | done | Test `VillagerFoodTest.ArrivesAtStoragePitForFood`; villagers take from the store (`GetResourceFrom`, `src/ECS/Villager/VillagerResources.cpp`) through `StoragePitStore::RemoveResource`, wood piles five to one |

## Piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pile rises out of the ground as it fills and sinks as it empties, easing over a second | done | Scene `miracles.food_speedup_and_stores`; `PotArchetype::SetSize` with its zoomer (`src/ECS/Archetypes/PotArchetype.cpp`, `src/ECS/PotResource.cpp`) |
| Food rises fast at first then slows; wood rises evenly; neither beyond full | done | `pot_resource::PileFoodProportionRaised` and the wood's even rise; test `test/test_food_wood.cpp` |
| A pile is drawn only while some of it is above ground | done | Test `DrawGate.AWoodOrFoodPileIsDrawnOnlyWhileSomeOfItIsAboveTheLand` (`test/test_draw_list_gate.cpp`); `draw_gate::PileBuried` in `src/ECS/DrawList/DrawGate.cpp` leaves a pile whose sink is at or below minus its height out of the draw, as the original's pile draws ([the land](../../../world/terrain.md#where-each-class-takes-its-drawn-height)) |
| The grain on a food pile flows down it as it rises | done | test `FoodWood.grainSplineKeyPoints` |
| Food piles follow the land's shape | done | Test `TakeResourceTest.OnlyFoodPilesFollowTheLandsShape` (`test/test_take_resource.cpp`: a food pile is made melting onto the land, a wood pile and a plain pot are not; in the exe only the food pile classes are morphable, [meshes stuck to the ground](../../../rendering/meshes-and-lod.md)); `MorphWithTerrain` on food piles (`src/ECS/Archetypes/PotArchetype.cpp`) |
| A pile thuds as it is made or added to, louder for 200 or more; piles in the hand don't | done | Test `FoodWood.pileHelpers`; scene `miracles.food_speedup_and_stores`; the pile sound by amount (`PileSoundSample`, `src/ECS/PotResource.cpp`), its food sample from a first reading of the clock and its wood sample from a second, as the original (`pot_resource::PlayPileSound`, pinned by `FoodWood.pileHelpers`) |
| Food and wood poured at a point go to the stores and piles in its cell, then the cells around, as far as each reaches | done | Tests `FoodMiracleDrop.AGrainLooksForATakerInItsOwnCellThenTheEightAroundItAndNoFurther`, `FoodMiracleDrop.AWorshipSitesFoodPotTakesFoodWithinTwiceItsRadius`, `FoodMiracleDrop.PilesInReachTakeItInTurnAndOnlyAMagicPileTakesMoreThanItsMost` (`test/test_food_miracle.cpp`: the nine cells, the reach of twice a pot's radius, the piles in turn up to their most), and for the stores `ResourceStoresTest.ABuildingTakesWoodWhileItHasABuildingSite`, `AScaffoldTakesWoodWhileItBelongsToABuildingSite`; `pot_resource::AddResourceToPos` (`src/ECS/PotResource.cpp`) |
| What nothing takes makes a new pile, unless it lands in water | partial 90% | No test; `pot_resource::AddResourceToPos`. The new pile is made by the miracles' pile maker on land the island says is dry, so a test needs a land with its blocks and the magic pile's making together |
| A new pile makes the nearby people come and look | partial 90% | No test; a pot made or put down offers its reaction (`src/ECS/PotResource.cpp`, `HandHolding.cpp`). The reaction is offered by the miracles' pile maker and the hand's put-down, a test of their own there |
| Pots and piles placed by the land scripts | partial 90% | No test; `CreatePot` (`src/LHScriptX/FeatureScriptCommands.cpp`). The command is reached only through the feature script's reader and its place check; a test needs that reader with a land |
| A town's temporary pots placed by the land scripts | pending | `CreateTownTemporaryPots` is empty (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A pile or pot with something in it can catch fire | partial 70% | Missing: whether the food or wood is lost is not checked; piles and pots burn (`src/ECS/Fire`), and emptying one puts its fire out (`src/ECS/ObjectResources.cpp`); unconfirmed whether the food or wood is lost |
| Pots are physical: they can be thrown, roll and land | partial 90% | No test; pots are physics objects (`src/ECS/Physics/PhysicsObjects.cpp`) and react to their impact (`src/ECS/HeldApply.cpp`); see `../physics/`, where its test belongs |
| An emptied loose pile goes | inferred | an emptied pile stays at 0, its reaction removed (`src/ECS/ObjectResources.cpp`); whether a loose one goes is unconfirmed |
| Hovering over a pile shows how much it holds | partial 50% | Over a store the hand shows what it holds (scene `hand.tooltips`); missing: a loose pile's text; see `../interface/` |
