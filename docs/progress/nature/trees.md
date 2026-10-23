# Trees

The single trees and bushes on every land: their kinds, how they grow, sway and burn, and what the hand, the creature and
the villagers do with them.

**Progress: 16/30 done, 0 chosen, 10 partial, 2 inferred — 76%**

How the original does it, in our wiki: [Trees and forests](../../../world/trees.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place trees of each kind (beech, birch, cedar, conifer, oak, olive, palm, pine, cypress, bushes, copses, hedges, burnt) and their variants, turned and sized | done | `TreeArchetype` (`src/ECS/Archetypes/TreeArchetype.cpp`), CREATE_TREE and CREATE_NEW_TREE (`src/LHScriptX/FeatureScriptCommands.cpp`); scenes `sky.trees_by_day`, `sky.trees_at_night` |
| The scripts place dead trees, a land script's for the player its line names, a challenge script's for no one | done | `DeadTreeArchetype::Create` keeps the player in `DeadTree::owner`: the land script's CREATE_DEAD_TREE (`FeatureScriptCommands::CreateDeadTree`, the name read as `PlayerFromText`), the CHL CREATE of a dead tree (`src/CHLApi.cpp`) and a dropped log the neutral player's; test `LandScriptSave.ALandScriptDeadTreeIsMadeForThePlayerItNames`; see [../../../world/trees.md](../../../world/trees.md#tree-details) |
| A tree has a size now and a largest size it grows to | done | `components::Tree` (`src/ECS/Components/Tree.h`); scene `miracles.water_trees` (young trees grow to their full size) |
| Scripts mark some trees as non-scenic; the flag is kept and set again on replanting | partial 50% | `Tree::isNonScenic`, set again on replanting (`HandTrees.cpp`); missing: what the original reads it for is not traced (the forester's tree search does not: [trees.md](../../../beings/villager-jobs.md#searches-for-trees-and-forests-for-the-villagers)); no test |
| Trees stand in the way of walkers, each as a circle on the ground | done | The route planner's obstacle circles (`src/ECS/RoutePlanWorld.cpp`); scene `examples.villager_tree_gone` |
| Trees are lit by the land's light and drawn unlit where the game does | done | `ecs::TreeBrightness` and the cell light (`src/ECS/Trees.cpp`, `RenderingSystem.cpp`); test `test/test_tree_brightness.cpp` |
| Trees cast the static shadows of fixed objects, by night as by day | done | The static shadows, trees with alpha test (`RenderPass::StaticShadow`); the exe's `Tree::IsCastShadowAtNight` (vt+0x1A0) has no caller; scene `light.shadows`; see ../rendering/ |

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree of a forest (not a town's scenic one) short of its full size grows by its kind's amount every so many turns; loose trees never grow | partial 90% | `ProcessTreesTurn` and `GrowTree` (`src/ECS/Trees.cpp`), as [trees.md](../../../world/trees.md#growth); no test |
| Trees grow faster in the rain and on good land | done | `ecs::TreeGrowthAmount` (the rain and the land's alignment); test `test/test_spell_forest.cpp` (`TreeGrowthAmount`) |
| The water miracle makes trees grow faster | done | `ecs::ApplyWaterSpell` (`src/ECS/Trees.cpp`); test `test/test_spell_forest.cpp`; scene `miracles.water_trees` |
| The water miracle plants a young tree of the same kind by a full-grown one of a forest | done | `ecs::ApplyWaterSpell` with `PlantTreeNear`; see [forests](forests.md); scene `miracles.water_forest` |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees sway in the wind, in one of sixteen sways chosen by the way they face | partial 50% | `Tree::windSlot` and `VegetationSystem` (`RenderingSystem.cpp`); missing: the phase step and lean are ours, not read in the exe (see ../weather/wind.md); test `test/test_vegetation.cpp` |
| Trees bend away from the hand passing among them, and creatures bend small trees as they walk through | partial 70% | `UpdateTreeBends` (`src/ECS/Trees.cpp`) for the held object and flying bodies; tests `TreeBends.*`, `TreeSystem.*`; missing: the creature's slot is not fed, though openblack has creatures now |
| Bent trees crash about, louder the further they bend; tall trees near the camera rustle and creak now and then | done | `ecs::UpdateTrees` (the bend's crash, the rustle and creak of tall trees near the camera); tests `TreeRustle.*` (`test/audio/test_anim_effects.cpp`) |
| Snow settles on trees | pending | No lying snow in our tree; see ../weather/snow.md |

## Fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees catch fire and burn, their flames kept to the trunk's middle | done | `src/ECS/Fire/FireGraphic.cpp` (trees' flames in the lower half, 0.2 of the height); test `FireGraphic.BurningTreeAlphaRefMatchesTheExe`; scenes `miracles.fireball_trees`, `miracles.forest_fire` |
| A burning tree is drawn burning | partial 80% | `graphic::TreeDrawColour` and the flames (`src/ECS/Fire/FireGraphic.cpp`); test `FireGraphic.CharringGreyMatchesTheExe`; missing: the foliage thinning, see [../physics/fire.md](../physics/fire.md) |
| Fire spreads from tree to tree by the heat each passes to its neighbours | done | Test `FireModel.AFireHeatsItsNeighbourByTheDifferenceUpToHalfItsHeat`; scene `miracles.forest_fire`. Fire spreads by heat (`src/ECS/Fire/FireEffect.cpp`, the heat passed by the model's `fire::HeatTransfer` in `src/Fire/FireModel.cpp`), as our wiki describes ([trees.md](../../../world/fire.md#fire-trees)); see ../physics/ |
| A tree burnt down goes | done | `fire::traits::DestroyedByEffect`, as our wiki gives it ([trees.md](../../../world/fire.md#fire-trees)); test `FireModel.ItCharsAsItsLifeRunsLowAndGoesOnceColdAndClean` |
| A magic tree on fire stops being something people come to look at | partial 90% | `magic_tree::StartOnFire` and `EndOnFire` (`src/Magic/Objects/MagicTree.cpp`); no test |

## Hand, creature and villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand pulls up a tree; held, it shows as branches or logs by its kind | partial 70% | The tug and the uprooting are done (`HandTrees.cpp`; scenes `hand.tug_at_crown`, `hand.tug_heavy_tree`); a firefly hiding in it leaves a one-shot seed ([fireflies.md](fireflies.md)); missing: the held tree is drawn as itself, not by its carried type; the tug's plane height (#87, ../hand/tug.md) |
| A tree dropped on the land is planted where it falls; planting and pulling up move the alignment | done | `HandSystem::Replant` (`src/ECS/Systems/Implementations/HandTrees.cpp`, with the alignment's good push; the uprooting's evil one; a town desire flag within reach counts as its town's, 2026-10-21); tests `TreeReplant.*`; scenes `physics.tree_replanted_good`, `physics.tree_replanted_evil` |
| A thrown tree flies and lands as a dead tree lying as it came down | done | Test `test/test_particle_carried_objects.cpp` (`TreeBecomesDeadTree`), scene `objects.kick_tree`. The tree's end of physics (`HandPhysics.cpp`); the dead tree it becomes tells the villagers there is wood, with the player of the throw (`ecs::TreeBecomesDeadTree`, `src/ECS/Trees.cpp`); see ../physics/throwing_and_landing.md |
| Uprooted trees lie as dead trees, with their roots showing, and can be picked up and burnt | partial 90% | No test of the roots; The tree becomes a dead tree in its landed pose with its roots (`HandPhysics.cpp`, `UpdateRoots`); dead trees can be picked up and burn |
| The creature pulls up, eats, throws and plays with trees, and must be big enough for a tree | partial 30% | The creature's kick sends a tree flying, to land as a thrown tree does, and its knock-down leaves it standing, as the original (`CreatureObjectActionSystem.cpp`); its pick-up test takes a tree or dead tree by its tree row's weight (`CreatureObjectActionSystem::PickUpTest`); it eats fruit from a tree tall enough (`EatFromTree`, [../creature/object_actions.md](../creature/object_actions.md)) and takes wood from a tree to a store or a building site ([../creature/town_actions.md](../creature/town_actions.md)); missing: pulling up, eating and throwing trees as such are not checked against the exe, and the share is not measured again since these landed; see ../creature/ |
| Foresters fell trees for wood; trees are worth wood by kind and size | done | `src/ECS/Villager/VillagerForester.cpp`, `ecs::FellTree`, `ecs::TreeWoodValue`; tests `test/test_villager_forester.cpp`, `test/test_store_rules.cpp`; see [../resources/wood.md](../resources/wood.md) |
| Villagers sleep under trees and shelter by them | inferred | A guess, not found in the original; nothing in our tree |
| Footprints are worn round trees on busy routes | inferred | A guess, not found in the original; nothing in our tree |
| Trees on a building site block the town's clearing | pending | see ../building/ |
| Tooltips over trees say what the hand can do | partial 50% | Missing: the tooltips over a standing tree are not checked; The held tree's own drop tooltip, Plant (`HandToolTips.cpp`); the tooltips over a standing tree are not checked; see ../interface/ |
