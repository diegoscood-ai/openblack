# Giving resources by hand

The player's hand can take food and wood from piles, fields and forests, carry them and give them to a town, the worship
site or anyone else, which the creature watches and may copy.

**Progress: 6/16 done, 0 chosen, 8 partial, 1 inferred — 80%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## Taking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking from a pile takes a handful into a pot in the hand, drawn by how much it holds | done | Test `TakeResourceTest.PickUpFromAPileRecordsThePilesPlayer`; the hand demos; the press over a pile or store takes a handful into a hand pot (`src/ECS/Systems/Implementations/HandHolding.cpp`, `HandResources.cpp`), drawn by its amount (`PotArchetype`) |
| Holding the button on a pile keeps taking more | partial 90% | No test; the batch ramp, 8 + 62 t squared a turn up to 20000 (`src/ECS/Systems/Implementations/HandFish.cpp`, `HandTurn.cpp`); see `../hand/multi_pickup.md` |
| Tugging at a ripe field pulls up a handful of crop (wheat in the hand) | partial 90% | No test; the field scoop (`src/ECS/Systems/Implementations/HandFish.cpp`) |
| Pulling up a tree gives its branches or logs in the hand | pending | the uprooted tree itself is held (`HandHolding.cpp`); see `../nature/trees.md` |
| Grabbing a big forest takes a conifer's wood off it and puts a new conifer in the hand (it is not tugged) | partial 90% | No test; `CreateTreeFromForest` in `src/ECS/Systems/Implementations/HandTrees.cpp` (a tree made on the spot, wood taken off the forest); see `../nature/forests.md` |
| The hand may take only inside its player's influence | partial 90% | No test named here; `HandSystem::InInfluence` (`src/ECS/Systems/Implementations/HandSystem.cpp`) |
| Another player's store or field can be taken from | inferred | Not checked what the original allows; the steal deeds are there (`TakeResourceTest.FoodOfAnotherPlayerDrawsOnceForTheStealDeed`) |

## Giving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping food or wood on a store adds it to the store | done | `held_apply` and `resource_stores::DeleteObjectAndTakeResource` (`src/ECS/HeldApply.cpp`, `src/ECS/ResourceStores.cpp`); tests `ResourceStoreTable.AStoragePitTakesAnyType`, `ResourceStoresTest.*` |
| Dropping it on the worship site feeds the worshippers | done | a worship site takes food and wood (`worship::site::DeleteObjectAndTakeResource`); test `ResourceStoreTable.AWorshipSiteTakesFoodAndWoodOnlyWithASite`; see `../worship/worship_sites.md` |
| Dropping it on a building site or the workshop gives it wood | done | a building site and a workshop take wood (`src/ECS/ResourceStores.cpp`); tests `ResourceStoreTable.ABuildingTakesWoodOnlyOnItsBuildingSite`, `ResourceStoreTable.AWorkshopTakesWoodAndAny`; a scaffold on a building site takes wood for its site too (`StoreKind::Scaffold`, tests `ResourceStoreTable.AScaffoldTakesWoodOnlyForItsBuildingSite`, `ResourceStoresTest.AScaffoldTakesWoodWhileItBelongsToABuildingSite`); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Dropping it anywhere else makes a pile there | done | Scene `miracles.food_speedup_and_stores`; `HandSystem::PutDownHandPot` and `pot_resource::AddResourceToPos` (`src/ECS/Systems/Implementations/HandResources.cpp`, `src/ECS/PotResource.cpp`) |
| Throwing a pot sends it flying; it lands as a pile, or floats in the water | partial 90% | No test; the thrown pot lands through the physics (`HandPhysics.cpp`, `src/ECS/HeldApply.cpp` impact rules) |
| Giving to a town that wants it impresses it, more the more it wants | partial 90% | No test; `DoResourceAdding` (`src/ECS/ObjectResources.cpp`) adds belief by how much the town's desire drops |
| Giving food or wood is a good deed | partial 90% | No test; `effects::alignment::UpdateForResource` from `src/ECS/ObjectResources.cpp` |
| The creature sees its god adding to a store and may copy it | done | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`) and passed to `creature_mimic::Consider` (`ReportResourceDeed`); tests `TakeResourceTest.FoodOfAnotherPlayerDrawsOnceForTheStealDeed`, `OwnFoodAndAnyWoodDrawNothing`; see `../creature/` |
| Tooltips over a pile or store say what the hand can do | partial 50% | Scene `hand.tooltips` (the store's own text); missing: the other pile and store texts; a held object valid to apply gives its apply text (`HandSystem::HeldValidToApplyTo`); the other pile and store texts are not ported; see `../interface/` |
