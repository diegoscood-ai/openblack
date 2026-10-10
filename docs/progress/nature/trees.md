# Trees

The single trees and bushes on every land: their kinds, how they grow, sway and burn, and what the hand, the creature and
the villagers do with them.

**Progress: 21/30 done, 4 partial — 77%**

How the original does it, in our wiki: [Trees and forests](../../bw1-notes/trees.md).

## Kinds and placing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place trees of each kind (beech, birch, cedar, conifer, oak, olive, palm, pine, cypress, bushes, copses, hedges, burnt) and their variants, turned and sized | done | `TreeArchetype` (`src/ECS/Archetypes/TreeArchetype.cpp`), CREATE_TREE and CREATE_NEW_TREE (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A tree has a size now and a largest size it grows to | done | `components::Tree` (`src/ECS/Components/Tree.h`) |
| Scripts mark some trees as scenery, which villagers leave alone (unconfirmed what else it changes) | done | The flag is kept (`Tree::isNonScenic`) and set again on replanting (`HandTrees.cpp`). Our wiki differs: the forester's tree search reads no such flag, so villagers do not leave those trees alone ([trees.md](../../bw1-notes/trees.md#searches-for-trees-and-forests-for-the-villagers)) |
| Trees stand in the way of walkers, each as a circle on the ground | done | The route planner's obstacle circles (`src/ECS/RoutePlanWorld.cpp`) |
| Trees are lit by the land's light and drawn unlit where the game does | done | `ecs::TreeBrightness` and the cell light (`src/ECS/Trees.cpp`, `RenderingSystem.cpp`); test `test/test_tree_brightness.cpp` |
| Trees cast shadows, including at night (unconfirmed) | done | The static shadows, trees with alpha test (`RenderPass::StaticShadow`); see ../rendering/ |

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree short of its full size grows by its kind's amount every so many turns, the first wait a random part of it | done | `ProcessTreesTurn` and `GrowTree` (`src/ECS/Trees.cpp`, the first wait random). Our wiki differs: only the trees of a forest grow; the land scripts' loose trees never do ([trees.md](../../bw1-notes/trees.md#growth-treeprocess-0x74a290-treegrow-0x74a3f0)) |
| Trees grow faster in the rain and on good land | done | `ecs::TreeGrowthAmount` (the rain and the land's alignment) |
| The water miracle makes trees grow faster | done | `ecs::ApplyWaterSpell` (`src/ECS/Trees.cpp`) |
| The water miracle plants a young tree of the same kind by a full-grown one of a forest | done | `ecs::ApplyWaterSpell` with `PlantTreeNear`; see [forests](forests.md) |
| Each kind can seed only so many new trees (unconfirmed) | todo | Not confirmed in the game; nothing in our tree |

## Moving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees sway in the wind, in one of sixteen sways chosen by the way they face | done | `Tree::windSlot` and `ecs::WindSway` (`src/ECS/Fields.cpp`, `RenderingSystem.cpp`) |
| Trees bend away from the hand passing among them, and creatures bend small trees as they walk through | partial | `UpdateTreeBends` (`src/ECS/Trees.cpp`) for the held object and flying bodies; the creature's slot is not fed |
| Bent trees crash about, louder the further they bend; tall trees near the camera rustle and creak now and then | done | `ecs::UpdateTrees` (the bend's crash, the rustle and creak of tall trees near the camera) |
| Snow settles on trees | todo | No lying snow in our tree; see ../weather/snow.md |

## Fire

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees catch fire and burn, their flames kept to the trunk's middle | done | `src/ECS/Fire/FireGraphic.cpp` (trees' flames in the lower half, 0.2 of the height) |
| A burning tree is drawn burning | done | `graphic::TreeDrawColour` and the flames (`src/ECS/Fire/FireGraphic.cpp`); the foliage thinning is not ported, see [../physics/fire.md](../physics/fire.md) |
| Fire spreads from tree to tree, each checking its neighbours now and then, with a random chance | done | Fire spreads by heat (`src/ECS/Fire/FireEffect.cpp`, the heat passed by the model's `fire::HeatTransfer` in `src/Fire/FireModel.cpp`), as our wiki describes ([trees.md](../../bw1-notes/trees.md#fire)); see ../physics/ |
| A tree burnt down leaves a burnt tree (unconfirmed) or goes | done | It goes (`fire::traits::DestroyedByEffect`), as our wiki gives it ([trees.md](../../bw1-notes/trees.md#fire)) |
| A magic tree on fire stops being something people come to look at | done | `magic_tree::StartOnFire` and `EndOnFire` (`src/Magic/Objects/MagicTree.cpp`) |

## Hand, creature and villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand pulls up a tree; held, it shows as branches or logs by its kind | partial | The tug and the uprooting are done (`HandTrees.cpp`); the held tree is drawn as itself; a firefly hiding in it leaves a one-shot seed ([fireflies.md](fireflies.md)) |
| A tree dropped on the land is planted where it falls; planting and pulling up move the alignment | done | `HandSystem::Replant` (`HandTrees.cpp`, with the alignment's good push; the uprooting's evil one) |
| A thrown tree flies and lands as a dead tree lying as it came down | done | The tree's end of physics (`HandPhysics.cpp`); see ../physics/throwing_and_landing.md |
| Uprooted trees lie as dead trees, with their roots showing, and can be picked up and burnt | done | The tree becomes a dead tree in its landed pose with its roots (`HandPhysics.cpp`, `UpdateRoots`); dead trees can be picked up and burn |
| The creature pulls up, eats, throws and plays with trees, and must be big enough for a tree | partial | The creature's knock-down action fells a tree (`CreatureObjectActionSystem.cpp`); it does not pick up, eat or throw trees; see ../creature/ |
| Foresters fell trees for wood; trees are worth wood by kind and size | done | `src/ECS/Villager/VillagerForester.cpp`, `ecs::FellTree`, `ecs::TreeWoodValue`; see [../resources/wood.md](../resources/wood.md) |
| Villagers sleep under trees and shelter by them (unconfirmed) | todo | Not confirmed in the game; nothing in our tree |
| Footprints are worn round trees on busy routes (unconfirmed) | todo | Not confirmed in the game; nothing in our tree |
| Trees on a building site block the town's clearing | todo | see ../building/ |
| Tooltips over trees say what the hand can do | partial | The held tree's own drop tooltip, Plant (`HandToolTips.cpp`); the tooltips over a standing tree are not checked; see ../interface/ |
