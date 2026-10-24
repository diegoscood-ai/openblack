# Civic buildings

Besides homes, a town has civic buildings: the town centre with its totem and spell icons, the storage pit, the
crèche, the workshop, the graveyard, the football pitch, the wonder and the spell dispensers. Each becomes functional
once built and does its own job for the town.

**Progress: 13/29 done, 0 chosen, 15 partial, 0 inferred — 80%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## All civic buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Civic buildings are placed by script like abodes, in each tribe's style | done | `src/ECS/Archetypes/AbodeArchetype.cpp`, the tribe's info by name; check: fidelity (Land 1's script places them) |
| A civic building becomes functional once built, setting up its job | partial 85% | `abodes::Built` then the building's MakeFunctional: the town centre's totem and icons, the storage pit, the creche, the graveyard, the workshop, the wonder (`src/ECS/Abodes.cpp`, `src/ECS/Archetypes/AbodeArchetype.cpp`; test `BuildingSitesTest.BuildByUntilBuilt`, `StoragePitMadeFunctionalWhenBuilt`); missing the football pitch's own part (Football::MakeFunctional 0x531380: the town's pitch, its ball made if it has none) |
| A damaged civic building stops working, and starts again when repaired | done | below its threshold a building stops working (`abodes::StopBeingFunctional`, `src/ECS/Abodes.cpp`) and is made functional again when repaired; tests `test/test_repairs.cpp`, `test/test_building_sites.cpp` (`RepairBranch`); see damage_and_repair.md |
| The town counts its civic buildings and wants more as it grows | done | `DesireForCivicBuildings` (`src/ECS/TownDesire.cpp`; `test/test_town_desire.cpp`); see ../town/town_desires.md |

## Town centre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town centre is placed by script with the town's worship share | done | CREATE_TOWN_CENTRE places it and sets the town's worship percentage (`src/LHScriptX/FeatureScriptCommands.cpp`); check: fidelity (Land 1's script) |
| It has a door villagers go in by | partial 20% | the original's town centre has its own door, a fixed point (2, 0, -12) through its matrix (TownCentre::GetDoorPos 0x744940), and arrives at its own position (GetArrivePos 0x7449E0); ours uses the generic mesh door (`abode_queries::GetArrivePos`, `src/ECS/Town/AbodeQueries.cpp`) and nothing sends villagers to it |
| It shows the town's spell icons, with power-up levels | partial 90% | the town centre's spell icons with their power-up levels (`src/Worship/TownCentreSpellIcon.cpp`); the power-up levels are tested: an icon shows the highest of its three levels held, none at -1, as `TownCentreSpellIcon::UpdateGraphicWithPULevels` 0x748ED0 (test `TownBeliefTest.ACentreSpellIconShowsTheHighestPowerUpLevelHeld`, `test/test_town_belief.cpp`), and so are the new owner's icons (`GraphicPlayers`). Missing: no test of an icon made at the town centre's special points (`town_centre::AddSpell` reads them from the centre's mesh, and a test has no mesh with special points); see ../miracles/ |
| A totem pole for the worship share and a creature statue stand by it | partial 60% | the totem (plinth and icon) rises with the worship share (`src/Worship/WorshipPercentage.cpp`; `test/test_village_totem.cpp`, scenario `objects.village_totem_worship`); the creature's icon on it and its facing of the worship site: see ../town/artefacts.md and fork issue #178 |
| A faint column of light in its owner's colour stands over it | partial 85% | `src/Graphics/RendererLightColumns.cpp`, `light_column::Shown` (tests `LightColumn.*`); the colour lacks the original's remap of the players by land; see [../rendering/light_beams.md](../rendering/light_beams.md) |
| Belief symbols rise from its foot | done | the belief symbols over the town centre (`src/Particles/TownBelief.cpp`; `test/test_town_belief.cpp`); see ../town/belief_and_conversion.md |
| Damaging it puts the town in a state of emergency | done | `abodes::ReduceLife` below the functional threshold (`src/ECS/Abodes.cpp`), or on fire (`src/ECS/Town/TownEmergency.cpp`); tests `test/test_repairs.cpp`, `test/test_villager_repair.cpp` |
| Planned town centres can be built by a town | partial 90% | a town centre plan from CREATE_PLANNED_ABODE is civic and is built through `building_sites::RequestBestPlanned` (`src/ECS/Town/BuildingSites.cpp`); the civic mask is tested on a house and a citadel plan (`BuildingSitesTest.PlansListAndRepairDesire`, `test/test_citadel_plan.cpp`), and a town centre plan is civic and chosen by it, wanted whole while the town has no centre, totem or site of one (case 0x404 of GetDesireToBeBuilt, 0x73A4A6, [the desire to be built](../../../world/building-sites.md#the-desire-to-be-built); test `BuildingSitesTest.ATownAsksForTheMostWantedPlanOfTheKindItBuilds`, `test/test_building_sites.cpp`). Missing: no test of a town centre plan built (its building comes from `AbodeArchetype::Create`, which reads the mesh's box, and a test has no meshes) |

## Storage pit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pit with its wood and food piles | done | `test/test_resource_stores.cpp`; see ../town/storehouse.md |
| The desire flags fly over it | done | ported from raffclar PR #275: `TownDesireSystem::DrawFlag`, from the object draw list; `test/test_town_desire_flags.cpp` pins the exe's numbers (the start angle step 0.3696 of Town::GetTownNeedsPos 0x73DDE0); see ../town/town_desires.md |

## Crèche

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Children go to the crèche and stay there during the day | done | `ChildGotoCreche` and state 113 `CHILD_AT_CRECHE` by day (`src/ECS/Villager/VillagerChild.cpp`); tests `VillagerBirthTest.ChildGotoCreche`, `ChildAtCrecheByDay` |
| Children play at the crèche | done | the children walk the creche's promenade paths, with its sound (`GetNextDstPromemade`, `src/ECS/Villager/VillagerChild.cpp`); tests `VillagerBirthTest.PromenadeStep`, `TheCrecheSoundsFromItsMapCoordinates` |
| Losing the crèche sends its children home | done | without a functional creche a child at it goes home (state 113, `src/ECS/Villager/VillagerChild.cpp`); tests `VillagerBirthTest.ChildGotoCreche` (a creche at half life refused), `ChildAtCrecheByNight` (the go-home branch) |

## Graveyard

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dead of the town are buried in the graveyard | done | a death adds to the town's graveyard (`graveyard::AddDead` from `src/ECS/Villager/VillagerDeath.cpp`, `src/ECS/Town/Graveyard.cpp`), as Graveyard::ProcessNewDeath 0x595E50 (functional, below 50 dead: the exe's 50.0 at 0x8C6CA4; [graveyard](../../../world/building-sites.md#graveyard)); test `BuildingSitesTest.AWorkingGraveyardOfATownCountsItsDeadUpToFifty` (`test/test_building_sites.cpp`) |
| Each burial adds a grave, which goes through stages | done | the graves stage (dead x 0.18, at least 1, as 0x595E50; the exe's factor 0.18 at 0x9003E0, kept in 3 bits on the model) is kept on the graveyard (`src/ECS/Town/Graveyard.cpp`; test `BuildingSitesTest.AGraveyardsGravesStageIsItsDeadTimesPointEighteen`, `test/test_building_sites.cpp`) and its graves are drawn up to it (`graphics::mesh_status`, `test/graphics/test_mesh_status.cpp`); see [the graves on the model](../../../world/building-sites.md#the-graves-on-the-model) |
| Bodies are cleared away sooner when the town has a graveyard | done | a corpse lies for the info's dying time with or without a functional graveyard (`DyingTime`, `src/ECS/Villager/VillagerDeath.cpp`; `test/test_villager_death.cpp`, 120 with, 600 without) |

## Football pitch

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Placed as a building, with its ball | partial 40% | the pitch is placed (`src/ECS/Archetypes/AbodeArchetype.cpp`); its MakeFunctional (the town's pitch, CreateBall 0x5313E0) is not ported, so no ball |
| Villagers play matches on it at playtime | pending | See ../villager/play_and_gossip.md and [../town/football.md](../town/football.md) |

## Others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Spell dispensers give out miracles | done | `src/Worship/SpellDispenser.cpp`, its turn from the town turn; tests `test/test_magic_spells.cpp`, `test/test_miracles_menu.cpp`, scenario `miracles.dispensers`; see ../miracles/ |
| The workshop | partial 72% | See workshop_and_scaffolds.md |
| The wonder | partial 74% | See ../town/wonder.md |
| The citadel and the temple | partial 60% | the planned temple and its six-pile building site (`CitadelArchetype::CreatePlanned`, `test/test_citadel_plan.cpp`); on land load the temple is still created built with the land flattened then, and its entrance on the terrain, partial drawing, blend and alignment flock are missing (fork issue #178); see ../temple/ |
| Worship sites | partial 58% | See ../worship/ |
| A worship site its citadel asks for is built by villagers through a building site, the citadel's slots given as its sites are built | partial 30% | a requested site starts at 0 % and is drawn by its percent (`citadel::FindOrCreateWorshipSite`, `src/Worship/Citadel.cpp`; the original's Citadel::RequestANewWorshipSite 0x4633F0); no StandardBuildingSite is made for it, villagers do not build it, and the slots come at once (fork issue #175); see [../../../magic/worship.md](../../../magic/worship.md#script) |
| Fields and fish farms | partial 85% | See fields_and_fish_farms.md |
