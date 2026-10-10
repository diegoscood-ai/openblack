# Civic buildings

Besides homes, a town has civic buildings: the town centre with its totem and spell icons, the storage pit, the
crèche, the workshop, the graveyard, the football pitch, the wonder and the spell dispensers. Each becomes functional
once built and does its own job for the town.

**Progress: 16/28 done, 9 partial — 73%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## All civic buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Civic buildings are placed by script like abodes, in each tribe's style | done | `src/ECS/Archetypes/AbodeArchetype.cpp`, the tribe's info by name |
| A civic building becomes functional once built, setting up its job | done | `abodes::Built` then the building's MakeFunctional: the town centre's totem and icons, the storage pit, the creche, the graveyard, the workshop, the wonder (`src/ECS/Abodes.cpp`, `src/ECS/Archetypes/AbodeArchetype.cpp`) |
| A damaged civic building stops working, and starts again when repaired | done | below its threshold a building stops working (`abodes::StopBeingFunctional`, `src/ECS/Abodes.cpp`) and is made functional again when repaired; See damage_and_repair.md |
| The town counts its civic buildings and wants more as it grows | done | `DesireForCivicBuildings` (`src/ECS/Town/TownDesire.cpp`); see ../town/town_desires.md |

## Town centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town centre is placed by script with the town's worship share | done | CREATE_TOWN_CENTRE places it and sets the town's worship percentage (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| It has a door villagers go in by | partial | every abode has its door point (`abode_queries::GetArrivePos`, `src/ECS/Town/AbodeQueries.cpp`); nothing in our tree sends villagers into the town centre |
| It shows the town's spell icons, with power-up levels | done | the town centre's spell icons with their power-up levels (`src/Worship/TownCentreSpellIcon.cpp`); See ../miracles/ |
| A totem pole for the worship share and a creature statue stand by it | partial | the totem rises with the worship share (`src/Worship/WorshipPercentage.cpp`); no creature statue; See ../town/artefacts.md |
| A faint column of light in its owner's colour stands over it | todo | see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Belief symbols rise from its foot | done | the belief symbols over the town centre (`src/Particles/TownBelief.cpp`); See ../town/belief_and_conversion.md |
| Damaging it puts the town in a state of emergency | done | `abodes::ReduceLife` below the functional threshold (`src/ECS/Abodes.cpp`), or on fire (`src/ECS/Town/TownEmergency.cpp`) |
| Planned town centres can be built by a town | done | a town centre plan from CREATE_PLANNED_ABODE is civic and is built through `building_sites::RequestBestPlanned` (`src/ECS/Town/BuildingSites.cpp`) |

## Storage pit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pit with its wood and food piles | done | See ../town/storehouse.md |
| The desire flags fly over it | todo | no desire flags (`src/ECS/Town/TownProcess.cpp`); See ../town/town_desires.md |

## Crèche

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Children go to the crèche and stay there during the day | done | `ChildGotoCreche` and state 113 `CHILD_AT_CRECHE` by day (`src/ECS/Villager/VillagerChild.cpp`) |
| Children play at the crèche | done | the children walk the creche's promenade paths, with its sound (`GetNextDstPromemade`, `src/ECS/Villager/VillagerChild.cpp`) |
| Losing the crèche sends its children home | done | without a functional creche a child at it goes home (state 113, `src/ECS/Villager/VillagerChild.cpp`) |

## Graveyard

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dead of the town are buried in the graveyard | done | a death adds to the town's graveyard (`graveyard::AddDead` from `src/ECS/Villager/VillagerDeath.cpp`, `src/ECS/Town/Graveyard.cpp`) |
| Each burial adds a grave, which goes through stages | partial | the graves stage is kept on the graveyard (`src/ECS/Town/Graveyard.cpp`); what the draw shows for it is pending |
| Bodies are cleared away sooner when the town has a graveyard | done | a corpse lies for the info's dying time with or without a functional graveyard (`DyingTime`, `src/ECS/Villager/VillagerDeath.cpp`) |

## Football pitch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placed as a building, with its ball | partial | the pitch is placed (`src/ECS/Archetypes/AbodeArchetype.cpp`); its ball is not made |
| Villagers play matches on it at playtime | todo | See ../villager/play_and_gossip.md and [../town/football.md](../town/football.md) |

## Others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spell dispensers give out miracles | done | `src/Worship/SpellDispenser.cpp`, its turn from the town turn; See ../miracles/ |
| The workshop | partial | See workshop_and_scaffolds.md |
| The wonder | partial | See ../town/wonder.md |
| The citadel and the temple | partial | See ../temple/ |
| Worship sites | partial | See ../worship/ |
| Fields and fish farms | partial | See fields_and_fish_farms.md |
| Walls (planned wall sections) | n/a | no land or playground script uses the command |
| Creature pens | n/a | no land or playground script uses the command |
