# Villager jobs

Every adult has a job from the villager tables: housewife, farmer, forester, fisherman, shepherd, leader or trader. Jobs
gather food and wood for the town, and any adult can be sent to build, fetch or supply when the town wants it. A villager
put to work by the hand becomes a disciple (see disciples.md).

**Progress: 22/25 done, 0 chosen, 0 partial, 0 inferred — 88%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-jobs.md).

## Common to all jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tribe has the seven jobs, each with its own model, tool and speeds | done | the villager table's jobs by tribe (`src/Enums.h`, `src/InfoConstants.h`), their meshes (`src/ECS/Archetypes/VillagerArchetype.cpp`) and speeds; the tools in the hand (`src/ECS/CarriedProps.cpp`) |
| A villager carries the tool or load of what it is doing | done | the carried object drawn on the hand bone (`src/ECS/CarriedProps.cpp`); See tools_and_carried_items.md |
| Villagers carry food and wood up to their job's capacity, slowed by the load | done | `Villager::resourceHeld` up to the job's capacity (`src/ECS/Villager/VillagerResources.cpp`), with the load slowdown (`src/ECS/VillagerSpeed.cpp`); `test/test_villager_resources.cpp` |
| Gatherers take what they gather to the storage pit and drop it off | done | the drop-off states (`GOTO_STORAGE_PIT_FOR_DROP_OFF`, `ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF`, `src/ECS/Villager/VillagerResources.cpp`) |
| A villager who is thrown or dies drops only wood (over 50, as a log); food and the rest are lost, never a pile | done | `CreateDroppedResource` (the log), `DropWood` and `DropFood` (`src/ECS/Villager/VillagerResources.cpp`, `src/ECS/Villager/VillagerDeath.cpp`); See [tools_and_carried_items.md](tools_and_carried_items.md) |
| Without a storage pit, the town keeps its resources in temporary pots or in abodes | done | the town's temporary pots (`town_stores::GetTemporaryResourceStorePotOrPos`) and the abodes' own food; See ../town/storehouse.md |
| An unemployed villager looks for work its town has room for | done | the food desire's job checks make a villager a farmer or fisherman where the town has room (`CheckSatisfyFoodDesire`, `src/ECS/Villager/VillagerSatisfy.cpp`) |

## Food jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Farmers look for the town's best field, walk to it and plant the crop | done | `FindBestField`, `FARMER_ARRIVES_AT_FARM`, `FARMER_PLANTS_CROP` (`src/ECS/Villager/VillagerFarmer.cpp`); `test/test_villager_farming.cpp` |
| Farmers harvest ripe crops at harvest time and dig up what is left | done | `FARMER_DIGS_UP_CROP` and the harvest (`src/ECS/Villager/VillagerFarmer.cpp`) |
| Fishermen look for water or the town's fish farm, fish there and bring fish home | done | `FishermanLookForWater`, `FISHERMAN_ARRIVES_AT_FISHING`, `FISHING` and the catch (`src/ECS/Villager/VillagerFisherman.cpp`) |
| Shepherds find a flock, take control of it, lead it to food and water and bring it back | pending | the shepherd's states are TODO rows; `VillagerBecomesShepherd` returns 0 (`src/ECS/Villager/VillagerSatisfy.cpp`) |
| Shepherds fetch strays and wait for the flock | pending |  |
| Shepherds pick an animal for slaughter and slaughter it for food | pending |  |
| The town decides when it is harvest time | done | the field's activity by its growth: sow, then harvest when ripe (`fields::GetFieldActivity`, `src/ECS/Fields.cpp`) |

## Wood and building jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters go to the town's forest and chop trees for wood | done | `FORESTER_GOTO_FOREST`, `FORESTER_ARRIVES_AT_FOREST`, `FORESTER_CHOPS_TREE` (`src/ECS/Villager/VillagerForester.cpp`); `test/test_villager_forester.cpp` |
| Foresters chop wood for a building site and take it there | done | a builder may fetch its wood from a forest (`DecideHowToGetWood`, `src/ECS/Villager/VillagerBuild.cpp`); the for-building chop states are never entered in the original either ([villagers](../../../beings/villager-jobs.md#builders)) |
| Villagers take wood from big forests, from pots, from trees and from fallen trees | done | big forests (`ARRIVES_AT_BIG_FOREST`), pots (`TAKE_WOOD_FROM_POT`), trees (`TAKE_WOOD_FROM_TREE`) and loose wood (the wood reaction, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); `test/test_take_resource.cpp`; wood from rocks is not in the original (its arrival and check only return 1) |
| Builders fetch wood from the storage pit and walk to a building site | done | `ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS` and the walk to the site (`src/ECS/Villager/VillagerBuild.cpp`); See ../building/construction.md |
| Builders stand round the site and build it up, using wood per stroke | done | the builders' ring and `BUILDING`, wood per cycle (`src/ECS/Villager/VillagerBuild.cpp`); `test/test_villager_build.cpp` |
| Only craftsman disciples bring wood from the storage pit to the workshop; ordinary villagers never supply it | done | ordinary villagers never supply it, as the desire is 0 (`src/ECS/Town/TownDesire.cpp`); craftsman disciples do (`CheckSatisfySupplyWorkshop`, ported from raffclar PR #310; matches the exe's 0x7593A0); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Builders repair damaged buildings when the town wants repairs | done | a repairer is a builder (`CheckSatisfyToRepair`, `src/ECS/Villager/VillagerBuild.cpp`); `test/test_villager_repair.cpp` |

## Worship and trade

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worshippers walk to the worship site, dance and pray, eat from the site's pot and come home | done | the walk, the dance, the worship, the meal from the site's pot (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) and the walk home (ported from raffclar PR #308, checked against v1.42 on 2026-10-20); `test/test_worship.cpp`; the altar rest is in the game's table but nothing sends a villager to it ([worship](../../../magic/worship.md)); See ../worship/ |
| Worshippers beyond the dancers the site shows hide at the site's door | done | `HIDING_AT_WORSHIP_SITE` at the site's door (`src/ECS/Systems/Implementations/VillagerWorship.cpp`); `WorshipperTest.TheHiddenWorshipAtTheSitesDoor` |
| Traders carry food or wood, whichever the trade town wants more, from their own town's storage pit to the trade town's | done | ported from raffclar PR #310, checked against v1.42 on 2026-10-21 (`CheckTrader`): the trader fetches from its own town's store and delivers to its trade town's (`src/ECS/Villager/VillagerTrader.cpp`); `test/test_disciple_work.cpp`; the trade in abodes is not in the original (its two states only return 1) |
| Leaders have no work of their own: they are never needed for anything and live as ordinary villagers | done | a leader lives as any villager (`src/ECS/Villager/VillagerDecide.cpp`) |
