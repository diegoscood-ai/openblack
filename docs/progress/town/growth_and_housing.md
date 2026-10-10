# Town growth and housing

A town is a collection of abodes and civic buildings around a town centre, with an area on the map. It houses its
people, plans new buildings as it grows, and grows outwards when the player gives it wood and scaffolds.

**Progress: 20/24 done, 3 partial — 90%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## The town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Towns are made by the land script with an owner and a tribe | done | `src/ECS/Archetypes/TownArchetype.cpp`, CREATE_TOWN in `src/LHScriptX/FeatureScriptCommands.cpp` |
| A town has an area worked out from its buildings, and a centre | done | the town rectangle from its abodes and fields (`town_placement::SetTownArea`, `ExtendTownArea`, `src/ECS/Town/TownPlacement.cpp`) and its centre (`Town::centre`); `test/test_town_area.cpp` |
| A town can be marked uninhabitable by script | done | SET_TOWN_UNINHABITABLE sets `Town::uninhabitable` (`src/LHScriptX/FeatureScriptCommands.cpp`), and an uninhabitable town takes no villager (`town_villagers::AddVillagerToTown`) |
| A town has a congregation point, which scripts can move | done | SET_TOWN_CONGREGATION_POS (`src/LHScriptX/FeatureScriptCommands.cpp`), read by the emergency's congregation (`src/ECS/Villager/VillagerEmergency.cpp`) |
| The town's own forests, fields, fish farms and flocks are assigned to it at load | done | the forests at the end of the load (`town_features::AssignTownFeatures`, `src/ECS/Town/TownFeatures.cpp`); fields and fish farms take their town when made, and CREATE_FLOCK puts the flock on its town's list (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A town knows its nearest towns | partial | a nearest town search (`map_cells::GetNearestTown`), used by scripts, scaffolds and placement; no per-town list of neighbours |
| Each town has its own influence | done | `src/ECS/Influence/InfluenceSources.cpp`; See ../worship/ |
| A town turn runs its steps in the game's order: needs, abodes, repairs, artefacts, spell icons, emergencies, dying | partial | `ProcessTown` runs the steps in the original's order (`src/ECS/Town/TownProcess.cpp`): plans, sites, abodes, desires, worship, repairs, emergency, pots, belief, shuffling; the artefacts, spell icons, desire flags, creature attitude, missionaries and the town's alignment step are TODO |

## Housing its people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers are housed in abodes with room for adults and for children | done | `abode_villagers::AddVillagerToAbode` and the room for adults and children (`src/ECS/Town/AbodeVillagers.cpp`); `test/test_villager_home.cpp` |
| The best abode for a new villager is scored by room and by who already lives there (wanting a man, a villager) | done | `CalculateScoreForAddingVillagerToAbode`, `CalculateDesireToGainMale` and `FindAbodeWithSpaceInTown` (`src/ECS/Town/AbodeVillagers.cpp`, `src/ECS/Town/TownVillagers.cpp`) |
| The town keeps a list of its homeless, newest first | done | `town_villagers::Homeless`, head first (`src/ECS/Town/TownVillagers.cpp`), filled by making a villager homeless (`src/ECS/Villager/VillagerHome.cpp`) |
| Villagers are shuffled between abodes to fit families | done | `town_villagers::ShuffleVillagersAroundAbodes`, one move a call, from the town turn (`src/ECS/Town/TownProcess.cpp`) |
| A town checks whether its villagers need a new abode | done | `CheckNeedNewAbode` (`src/ECS/Villager/VillagerHome.cpp`), from the villagers' decisions and growing up |
| Overcrowded abodes push villagers out | done | a villager of a too crowded abode moves to a better one or becomes homeless (`CheckNeedNewAbode`, `abode_villagers::IsTooCrowded`) |
| The town's population counts for its desires and influence | done | the turn's town stats (`town_stats::Compute`, `src/ECS/Town/TownStats.cpp`) feed the desires (`src/ECS/Town/TownDesire.cpp`) and the influence; `test/test_town_stats.cpp` |

## Growing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town keeps planned buildings: outlines where it means to build | done | the town's plan list (`ecs::plans`, `src/ECS/Town/BuildingSites.cpp`), filled by CREATE_PLANNED_ABODE, destroyed buildings and scaffolds |
| When it wants abodes or civic buildings it picks the best plan and opens a building site | done | `building_sites::RequestBestPlanned` and `GetDesireToBeBuilt`, asked by the builders' desires (`src/ECS/Villager/VillagerBuild.cpp`); See ../building/construction.md |
| It asks for a new abode when there is no plan | done | `building_sites::RequestANewAbode` takes the best house plan (`src/ECS/Town/BuildingSites.cpp`). Our wiki differs: the town never invents a plan; without a house plan no abode is asked for ([buildings](../../bw1-notes/buildings.md#plans-and-building-sites)) |
| It looks for clear ground suited to a building in its area | done | the plan's spot is checked (`town_placement::IsSuitableForFixedAbodeInTown`, `src/ECS/Town/TownPlacement.cpp`). Our wiki differs: the town searches for no ground of its own; only a plan's spot, a scaffold's or the rival's is checked ([buildings](../../bw1-notes/buildings.md#plans-and-building-sites)) |
| A script forces a planned building to be built | done | BUILD_BUILDING (`src/CHLApi.cpp`) through `building_sites::ForceBuildingOfPlannedAtPos` |
| A new building changes the town's area and stats and draws villagers' attention | partial | the town area is redone when a structure joins or leaves (`src/ECS/Archetypes/AbodeArchetype.cpp`, `src/ECS/Abodes.cpp`) and the stats each turn; the new building reaction is not made yet (only logged in `abodes::Built`) |
| Scaffolds dropped by the player in the town become its buildings | done | a scaffold offers the building the town wants and, put down, becomes its site (`src/ECS/Scaffolds.cpp`); See ../building/workshop_and_scaffolds.md |
| The town grows only as fast as its wood allows | done | builders carry wood from the storage pit to the site's pile, and the site only grows with the wood used (`src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Town/BuildingSites.cpp`) |
| Towns of the computer player grow and attack on their own | todo | no computer player; See ../multiplayer/ (the rival gods' towns in the story are scripted; unconfirmed how much is AI) |
