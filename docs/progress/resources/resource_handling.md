# Giving resources by hand

The player's hand can take food and wood from piles, fields and forests, carry them and give them to a town, the worship
site or anyone else, which the creature watches and may copy.

**Progress: 12/16 done, 2 partial — 81%**

How the original does it, in our wiki: [Objects and resources](../../bw1-notes/objects-and-resources.md).

## Taking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Picking from a pile takes a handful into a pot in the hand, drawn by how much it holds | done | the press over a pile or store takes a handful into a hand pot (`src/ECS/Systems/Implementations/HandHolding.cpp`, `HandResources.cpp`), drawn by its amount (`PotArchetype`) |
| Holding the button on a pile keeps taking more | done | the batch ramp, 8 + 62 t squared a turn up to 20000 (`src/ECS/Systems/Implementations/HandFish.cpp`, `HandTurn.cpp`); see `../hand/multi_pickup.md` |
| Tugging at a ripe field pulls up a handful of crop (wheat in the hand) | done | the field scoop (`src/ECS/Systems/Implementations/HandFish.cpp`) |
| Pulling up a tree gives its branches or logs in the hand | todo | the uprooted tree itself is held (`HandHolding.cpp`); see `../nature/trees.md` |
| Tugging at a big forest pulls out a tree | done | `CreateTreeFromForest` in `src/ECS/Systems/Implementations/HandTrees.cpp` (a tree made on the spot, wood taken off the forest); see `../nature/forests.md` |
| The hand may take only inside its player's influence | done | `HandSystem::InInfluence` (`src/ECS/Systems/Implementations/HandSystem.cpp`) |
| Another player's store or field can be taken from (unconfirmed what is allowed) | todo | (unconfirmed what is allowed) |

## Giving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dropping food or wood on a store adds it to the store | done | `held_apply` and `resource_stores::DeleteObjectAndTakeResource` (`src/ECS/HeldApply.cpp`, `src/ECS/ResourceStores.cpp`); tests `ResourceStoreTable.AStoragePitTakesAnyType`, `ResourceStoresTest.*` |
| Dropping it on the worship site feeds the worshippers | done | a worship site takes food and wood (`worship::site::DeleteObjectAndTakeResource`); test `ResourceStoreTable.AWorshipSiteTakesFoodAndWoodOnlyWithASite`; see `../worship/worship_sites.md` |
| Dropping it on a building site or the workshop gives it wood | done | a building site and a workshop take wood (`src/ECS/ResourceStores.cpp`); tests `ResourceStoreTable.ABuildingTakesWoodOnlyOnItsBuildingSite`, `ResourceStoreTable.AWorkshopTakesWoodAndAny`; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Dropping it anywhere else makes a pile there | done | `HandSystem::PutDownHandPot` and `pot_resource::AddResourceToPos` (`src/ECS/Systems/Implementations/HandResources.cpp`, `src/ECS/PotResource.cpp`) |
| Throwing a pot sends it flying; it lands as a pile, or floats in the water | done | the thrown pot lands through the physics (`HandPhysics.cpp`, `src/ECS/HeldApply.cpp` impact rules) |
| Giving to a town that wants it impresses it, more the more it wants | done | `DoResourceAdding` (`src/ECS/ObjectResources.cpp`) adds belief by how much the town's desire drops |
| Giving food or wood is a good deed | done | `effects::alignment::UpdateForResource` from `src/ECS/ObjectResources.cpp` |
| The creature sees its god adding to a store and may copy it | partial | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`) but not passed on to the creature; see `../creature/` |
| Tooltips over a pile or store say what the hand can do | partial | a held object valid to apply gives its apply text (`HandSystem::HeldValidToApplyTo`); the other pile and store texts are not ported; see `../interface/` |
