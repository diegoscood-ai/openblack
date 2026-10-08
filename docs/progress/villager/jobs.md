# Villager jobs

Every adult has a job from the villager tables: housewife, farmer, forester, fisherman, shepherd, leader or trader. Jobs
gather food and wood for the town, and any adult can be sent to build, fetch or supply when the town wants it. A villager
put to work by the hand becomes a disciple (see disciples.md).

**Progress: 18/27 done, 3 partial — 72%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

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
| Shepherds find a flock, take control of it, lead it to food and water and bring it back | todo | the shepherd's states are TODO rows; `VillagerBecomesShepherd` returns 0 (`src/ECS/Villager/VillagerSatisfy.cpp`) |
| Shepherds fetch strays and wait for the flock | todo |  |
| Shepherds pick an animal for slaughter and slaughter it for food | todo |  |
| Housewives fetch food from the storage pit, gossip round it, and cook dinner at home | n/a | the housewife's states are TODO rows. Our wiki differs: the housewife's day (states 100 to 109, with the storage pit, the gossip and the dinner) is dead code in this version of the original ([villagers](../../bw1-notes/villagers.md#pregnancy-and-births)) |
| Housewives do housework when nothing else needs doing | n/a | `HOUSEWIFE_DOES_HOUSEWORK` is a TODO row; part of the housewife's day, dead code in the original (row above) |
| The town decides when it is harvest time | done | the field's activity by its growth: sow, then harvest when ripe (`fields::GetFieldActivity`, `src/ECS/Fields.cpp`) |

## Wood and building jobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters go to the town's forest and chop trees for wood | done | `FORESTER_GOTO_FOREST`, `FORESTER_ARRIVES_AT_FOREST`, `FORESTER_CHOPS_TREE` (`src/ECS/Villager/VillagerForester.cpp`); `test/test_villager_forester.cpp` |
| Foresters chop wood for a building site and take it there | done | a builder may fetch its wood from a forest (`DecideHowToGetWood`, `src/ECS/Villager/VillagerBuild.cpp`); the for-building chop states are never entered in the original either ([villagers](../../bw1-notes/villagers.md#builders)) |
| Villagers take wood from big forests, from rocks the game counts as wood, from pots, and from fallen trees | partial | big forests (`ARRIVES_AT_BIG_FOREST`), pots (`TAKE_WOOD_FROM_POT`), trees (`TAKE_WOOD_FROM_TREE`) and loose wood (the wood reaction, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); the rocks' rows are TODO |
| Builders fetch wood from the storage pit and walk to a building site | done | `ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS` and the walk to the site (`src/ECS/Villager/VillagerBuild.cpp`); See ../building/construction.md |
| Builders stand round the site and build it up, using wood per stroke | done | the builders' ring and `BUILDING`, wood per cycle (`src/ECS/Villager/VillagerBuild.cpp`); `test/test_villager_build.cpp` |
| Builders never carry scaffolds: the game leaves that empty | n/a | See [tools_and_carried_items.md](tools_and_carried_items.md) |
| Only craftsman disciples bring wood from the storage pit to the workshop; ordinary villagers never supply it | partial | ordinary villagers never supply it, as the desire is 0 (`src/ECS/Town/TownDesire.cpp`); there are no craftsman disciples yet; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) and [disciples.md](disciples.md) |
| Builders repair damaged buildings when the town wants repairs | done | a repairer is a builder (`CheckSatisfyToRepair`, `src/ECS/Villager/VillagerBuild.cpp`); `test/test_villager_repair.cpp` |
| A villager waits for wood when the site has none and none can be found | n/a | the state is a TODO row. Our wiki differs: the wait for wood is never entered in this version of the original, as a game flag it needs is never set ([villagers](../../bw1-notes/villagers.md#builders)) |

## Worship and trade

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worshippers walk to the worship site, dance and pray, rest at the altar and come home | partial | the walk, the dance and the worship (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) and the walk home; the altar rest's rows are TODO; See ../worship/ |
| Villagers carry food from the storage pit to the worship site for the worshippers | todo | the supply states are TODO rows; the town's desire for worship supplies is none (`src/ECS/Town/TownDesire.cpp`) |
| Worshippers hide at the worship site when not dancing | done | `HIDING_AT_WORSHIP_SITE` (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) |
| Traders take a town's excess food or wood from its storage pit and trade it in abodes, food for wood or wood for food | todo | the trader's states are TODO rows |
| Traders pick up and drop off excess from abodes | todo |  |
| Leaders have no work of their own: they are never needed for anything and live as ordinary villagers | done | a leader lives as any villager (`src/ECS/Villager/VillagerDecide.cpp`) |
