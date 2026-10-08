# Town desires

Each town works out, every game turn, how much it wants each of seventeen things: food, wood, playtime, protection,
mercy, abodes, civic buildings, worship supplies, children, building, rain, sun, repairs, workshop supplies, a wonder,
relaxation and sleep. Idle villagers serve the most wanted. The storage pit shows the town's wants as flags.

**Progress: 19/26 done, 2 partial — 77%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Working out the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town counts its adults, children, abodes, room, civic buildings, disciples, food and wood each turn | done | `town_stats::Compute` each town turn (`src/ECS/Town/TownStats.cpp`), food and wood carried and at building sites included; `test/test_town_stats.cpp` |
| Food wanted: what the town's people need for dinner against what it has | done | `DesireForFood` (`src/ECS/Town/TownDesire.cpp`); `test/test_town_desire.cpp` |
| Wood wanted grows with the town's buildings past a number | done | `DesireForWood` (`src/ECS/Town/TownDesire.cpp`); craftsmen: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Playtime wanted only after the game has run a while | done | `DesireForPlaytime` (`src/ECS/Town/TownDesire.cpp`) |
| Protection and mercy wanted from the town's memory of attacks | partial | `DesireForProtection` and `DesireForMercy` read the town's inputs, which stay 0: the aggression slots are not ported (`src/ECS/Town/TownProcess.cpp`) |
| Abodes wanted for the people without room | done | `DesireForAbodes` (`src/ECS/Town/TownDesire.cpp`) |
| Civic buildings wanted by population against the civic buildings it has | done | `DesireForCivicBuildings` (`src/ECS/Town/TownDesire.cpp`); the pitch with football off: [football.md](football.md) |
| Children wanted, less as food is wanted more | done | `DesireForChildren` (`src/ECS/Town/TownDesire.cpp`) |
| Building wanted from the town's building sites | done | `DesireToBuild` from the town's building sites (`building_sites::DesireInputsOf`, `src/ECS/Town/BuildingSites.cpp`) |
| Repairs wanted for damaged abodes not empty | done | `DesireToRepair` (`src/ECS/Town/TownDesire.cpp`) |
| Worship supplies, workshop supplies, rain and sun are never wanted, as in the game | done | their functions return 0 as the original's (`src/ECS/Town/TownDesire.cpp`) |
| A wonder wanted by the town's belief, less while food, wood and abodes are wanted | done | `DesireToBuildWonder` (`src/ECS/Town/TownDesire.cpp`) |
| Relaxation comes up in the evening; sleep comes up at nightfall | done | `DesireForRelaxation` and `DesireForSleep` by the day clock (`src/ECS/Town/TownDesire.cpp`) |
| Each desire is scaled by the town's tribe | done | the tribe's scale per desire (`src/ECS/Town/TownDesire.cpp`) |
| What villagers are already doing for a desire lowers it | done | the modification functions take off what the villagers already do for food, wood, building and the rest (`src/ECS/Town/TownDesire.cpp`) |
| The desires are sorted most-wanted first, both as felt and after what is being done | done | the original's own sort (`town_desire::MsvcQsort`, `src/ECS/Town/TownDesire.cpp`) |
| Every 50 turns the town works out how unhappy its people are | done | `WarnVillagersUnhappy` every 50 turns (`src/ECS/Town/TownDesire.cpp`) |
| The guidance read the town's biggest need | done | the help remarks read each town's sorted desires (`src/ECS/AudioQueries.cpp`); see ../story/ for the advisors |

## Acting on the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An idle villager offers itself to the desires in order until one takes it | done | `town_desire::CheckVillagerNeededForTownDesire`, from the villager's decision (`src/ECS/Villager/VillagerDecide.cpp`) |
| Each desire's job: fetch food, fetch wood, build abodes and civic buildings, build, repair, supply, play, relax, sleep | partial | food (fields, fishing), wood, abodes, civic buildings, building, repairs and sleep take villagers (`src/ECS/Villager/VillagerSatisfy.cpp`); the shepherd, supplies, playtime and relaxation are TODO and take none; see ../villager/daily_routine.md |
| Scripts boost a desire, and can resort the order at once | done | TOWN_DESIRE_BOOST (`src/LHScriptX/FeatureScriptCommands.cpp`) and SET_TOWN_DESIRE_BOOST with the re-sort (`town_desire::ScriptSetTownDesireBoost`, `src/CHLApi.cpp`) |
| A town's needs shown at a place a script picks | todo | TOWN_NEEDS_POS is an empty stub (`src/LHScriptX/FeatureScriptCommands.cpp`) |

## Desire flags

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit flies a flag for each thing the town wants, rising with how much it wants it | todo | no desire flags: the town turn's flag step is a TODO (`src/ECS/Town/TownProcess.cpp`) |
| Each flag has a tooltip naming the desire | todo | See ../interface/ |
| The flags ripple in the wind | todo |  |
| Hovering a flag gives help about what the town wants | todo |  |
