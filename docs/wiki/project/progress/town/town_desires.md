# Town desires

Each town works out, every game turn, how much it wants each of seventeen things: food, wood, playtime, protection,
mercy, abodes, civic buildings, worship supplies, children, building, rain, sun, repairs, workshop supplies, a wonder,
relaxation and sleep. Idle villagers serve the most wanted. The storage pit shows the town's wants as flags.

**Progress: 20/24 done, 0 chosen, 3 partial, 0 inferred — 95%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../beings/towns.md).

## Working out the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town counts its adults, children, abodes, room, civic buildings, disciples, food and wood each turn | partial 90% | `town_stats::Compute` each town turn (`src/ECS/Town/TownStats.cpp`), food and wood carried and at building sites included (`test/test_building_sites.cpp`, fidelity); recomputed from the entities, where the original adds and removes as things join and leave (`stats.Add` in Town::AddVillagerToTown): same counts, the float sums in another order (#53) |
| Food wanted: what the town's people need for dinner against what it has | done | `DesireForFood` (`src/ECS/TownDesire.cpp`); `test/test_town_desire.cpp` |
| Wood wanted grows with the town's buildings past a number | done | `DesireForWood` (`src/ECS/TownDesire.cpp`); craftsmen: [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Playtime wanted only after the game has run a while | done | `DesireForPlaytime` (`src/ECS/TownDesire.cpp`) |
| Protection and mercy wanted from the town's memory of attacks | done | `DesireForProtection` and `DesireForMercy` read the sums that step 13 makes from the aggression slots (`town_aggression::ProcessTurn`, `Town::ProcessPlayerInteract` 0x73DEC0); `TownAggression.ItFadesEachTurnAndANeutralTownWantsOnlyProtection`; see [emergencies_and_aggression.md](emergencies_and_aggression.md) |
| Abodes wanted for the people without room | done | `DesireForAbodes` (`src/ECS/TownDesire.cpp`) |
| Civic buildings wanted by population against the civic buildings it has | done | `DesireForCivicBuildings` (`src/ECS/TownDesire.cpp`); the pitch with football off: [football.md](football.md) |
| Children wanted, less as food is wanted more | done | `DesireForChildren` (`src/ECS/TownDesire.cpp`) |
| Building wanted from the town's building sites | done | `DesireToBuild` from the town's building sites (`building_sites::DesireInputsOf`, `src/ECS/Town/BuildingSites.cpp`) |
| Repairs wanted for damaged abodes not empty | done | `DesireToRepair` (`src/ECS/TownDesire.cpp`) |
| Worship supplies, workshop supplies, rain and sun are never wanted, as in the game | done | their functions return 0 as the original's (`src/ECS/TownDesire.cpp`) |
| A wonder wanted by the town's belief, less while food, wood and abodes are wanted | done | `DesireToBuildWonder` (`src/ECS/TownDesire.cpp`) |
| Relaxation comes up in the evening; sleep comes up at nightfall | done | `DesireForRelaxation` and `DesireForSleep` by the day clock (`src/ECS/TownDesire.cpp`) |
| Each desire is scaled by the town's tribe | done | the tribe's scale per desire (`src/ECS/TownDesire.cpp`) |
| What villagers are already doing for a desire lowers it | done | the modification functions take off what the villagers already do for food, wood, building and the rest (`src/ECS/TownDesire.cpp`) |
| The desires are sorted most-wanted first, both as felt and after what is being done | done | the original's own sort (`town_desire::MsvcQsort`, `src/ECS/TownDesire.cpp`) |
| Every 50 turns the town works out how unhappy its people are | done | `WarnVillagersUnhappy` every 50 turns (`src/ECS/TownDesire.cpp`) |
| The guidance read the town's biggest need | done | the help remarks read each town's sorted desires (`src/ECS/AudioQueries.cpp`); see ../story/ for the advisors |

## Acting on the desires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An idle villager offers itself to the desires in order until one takes it | done | `town_desire::CheckVillagerNeededForTownDesire`, from the villager's decision (`src/ECS/Villager/VillagerDecide.cpp`) |
| Each desire's job: fetch food, fetch wood, build abodes and civic buildings, build, repair, supply, play, relax, sleep | partial 85% | food (fields, fishing), wood, abodes, civic buildings, building, repairs, worship and workshop supplies, relaxation at the artefacts and sleep take villagers (`src/ECS/Villager/VillagerSatisfy.cpp`; `VillagerBuildTest.CheckSatisfyToBuildAndRepair`, `VillagerResourcesTest.CheckSatisfyFoodDesire`); playtime takes none as in the game (0x763130 returns 0); missing: the shepherd's flocks and relaxation round the creature; see ../villager/daily_routine.md |
| Scripts boost a desire, and can resort the order at once | partial 95% | TOWN_DESIRE_BOOST (`src/LHScriptX/FeatureScriptCommands.cpp`) and SET_TOWN_DESIRE_BOOST with the re-sort (`town_desire::ScriptSetTownDesireBoost`, `src/CHLApi.cpp`; `TownDesireTest.Scripts`); a negative desire writes nothing, where the original writes outside the array (#53) |

## Desire flags

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit flies a flag for each thing the town wants, rising with how much it wants it | done | ported from raffclar PR #275 and fitted to the v1.42 reading of the flags (wiki ✅, `test/test_town_desire_flags.cpp`): `ECS/TownDesireFlags`, made with the town (`TownDesireFlagArchetype::CreateForTown`) and in the map's cells as the game's are (`map_cells::InsertMapObject`: a plain object of type 24 PRAYER at the tail of its cell's fixed list, which every walker meets and treats as the game's do), stepped by `TownDesireSystem::DrawFlag` on the frames the object draw list draws it (the desire the town's turn left the flag, eased on a `Zoomer`, `TownDesireSystem::ProcessFlags`); see ../../../beings/towns.md#the-desire-flags |
| Each flag has a tooltip naming the desire | done | 0xE88 "Need" in the flag's own words, 0xECE + desire (0x5D75D5, [tooltips](../../../interface/tooltips.md)); `hand_tooltips::TownDesireFlagToolTip`, `test/hand/test_hand_tool_tips.cpp`; see ../interface/ |
| Hovering a flag gives help about what the town wants | pending | the flag's help texts are its desire info's (`TownDesireFlags::GetQueryFirstEnumText` 0x746EC0 reads info +0x7C); a TODO in `src/ECS/HelpQueryFacts.cpp` |
