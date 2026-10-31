# Villager life cycle

Villagers are born, grow up, pair off, have children and die of old age. Each belongs to a tribe and a town, and lives
in an abode with a family.

**Progress: 27/29 done, 0 chosen, 2 partial, 0 inferred — 98%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-life.md).

## Creation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers placed by the land script at a position, near an abode, with a tribe, job and age | done | CREATE_VILLAGER_POS (`src/LHScriptX/FeatureScriptCommands.cpp`) and the villager's constructor with its age, scale and food draws (`src/ECS/Archetypes/VillagerArchetype.cpp`, `villager::Construct`, `src/ECS/Villager/VillagerCore.cpp`); `test/test_villager_core.cpp` |
| Town villagers created straight into a town by script (by town, by type, special ones) | done | CREATE_TOWN_VILLAGER, CREATE_SPECIAL_TOWN_VILLAGER and CREATE_VILLAGER (`src/LHScriptX/FeatureScriptCommands.cpp`); tests `ScriptVillagerCommands.InfoFromText`, `ScriptVillagerCommandsAbodes.FindVillagerInfoFirstMatch` |
| A new villager joins the nearest town and an abode in it with room | done | the map script's handlers house it: the abode at the position, else an abode with room in the town, else the vagrants (`src/LHScriptX/FeatureScriptCommands.cpp`, `town_villagers::AddVillagerToTown`); tests `ScriptVillagerCommandsAbodes.FindAbodeAt`, `ScriptVillagerCommandsAbodes.FindAbodeAtNeutralLast`, `VillagerHomeTest.ScoreAndFindAbode` |
| A villager made over water starts drowning instead of standing | done | made in the water it starts in `DROWNING` (`villager::Construct`); test `VillagerCoreTest.ConstructorCounterStateAndWater` |
| A newly made villager waits a random while before its first decision | done | `CREATED`, then the first decision (`src/ECS/Villager/VillagerCore.cpp`); tests `VillagerCoreTest.ConstructorCounterStateAndWater`, `VillagerCoreTest.CreatedCountsDown` |
| Sex follows the job: housewives are women, the other jobs men | done | the villager info's sex (`src/ECS/Archetypes/VillagerArchetype.cpp`); test `VillagerTableData.HousewivesAreWomenAndTheOtherJobsMen` |
| Each villager has its own small difference in speed and size from when it was made | done | the speed factor (`src/ECS/VillagerSpeed.cpp`) and the scale drawn for the age (`SetScaleForAge`, `src/ECS/Villager/VillagerAge.cpp`); tests `VillagerSpeed.EachVillagersOwnWayOfGoing`, `VillagerAgeTest.ScaleForAge`, `VillagerAge.AnAdultKeepsItsSizeUnlessTheDrawIsBiggerThenDrawsAgain` |

## Age and growing up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Age counts in game time, a year every fixed number of turns | done | the age kept as the birth turn, 1500 turns a year (`src/ECS/Villager/VillagerAge.cpp`); `test/test_villager_age.cpp` |
| Children are smaller, scaled up with age until adult size | done | the child's scale by age, rescaled every quarter year (`src/ECS/Villager/VillagerAge.cpp`); tests `VillagerAge.ChildrenStartAtTheSizeOfTheAgeBeforeTheirsAndAdultsANearlyFullOne`, `VillagerAge.AChildGrowsUpToThreeQuartersOfTheWayToTheNextAgesSize`, `VillagerAge.ChildrenGrowFourTimesAYear` |
| Children use the child models of their tribe | done | the child meshes of the tribe (`src/ECS/Archetypes/VillagerArchetype.cpp`); tests `VillagerAge.AVillagerGivenAnAgeWearsItsKindsModelsForItAtEveryDistance`, `VillagerAge.AScriptMakingAGrownVillagerYoungGivesItTheChildsModel` |
| A child becomes an adult at the age of growing up, takes a job and the town's counts change | done | the child grows up at its info's age, the counts move, and `GO_HOME_AND_CHANGE` gives it the adult mesh (`src/ECS/Villager/VillagerAge.cpp`, `src/ECS/Villager/VillagerHome.cpp`); tests `VillagerAgeTest.ChildGrowsUp`, `VillagerHomeTest.GoHomeAndChange` |
| Old villagers walk slower; young ones too | done | the age factor of the speed (`src/ECS/VillagerSpeed.cpp`); test `VillagerSpeed.EachVillagersOwnWayOfGoing` |
| Villagers die of old age, more likely the older they get, checked at bed time | done | the old age death checked when going to bed (`CheckWhenGoingToBed`, `src/ECS/Villager/VillagerAge.cpp`, `src/ECS/Villager/VillagerHome.cpp`); tests `VillagerAgeTest.OldAge`, `VillagerAge.OnlyThePastOldAgeDieOfItAndTheOlderTheLikelier`, `VillagerHomeTest.CheckWhenGoingToBed` |

## Families and children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A couple shares an abode; a man and a woman are spouses | done | the abode's man and woman slots (`abode_villagers::AddVillagerToAbode`, `src/ECS/Town/AbodeVillagers.cpp`); test `VillagerHomeTest.AbodeList` |
| Couples have sex at home at night, more often when the town wants children | done | a woman falls pregnant at bed time with a man at home, by the town's desire for children (`CheckWhenGoingToBed`, `CheckGetPregnantAtHome`, `src/ECS/Villager/VillagerHome.cpp`); the three at-home sex states do nothing in the original either, and nothing sets them (`SexAtHome`; [the original](../../../beings/villager-life.md#love-outside-the-home-and-the-breeder)); `test/test_villager_birth.cpp` |
| Couples meet outside and have sex when promiscuous or waiting for a mate | partial 50% | `START_HAVING_SEX`, `HAVING_SEX` and `STOP_HAVING_SEX` with the partner as the target thing, the meeting gap and the woman's pregnancy draw as the exe has them (`src/ECS/Villager/VillagerBreeding.cpp`, `src/ECS/VillagerBreedingRules.h`; `test/test_villager_breeding.cpp`), reached through a breeder disciple; the call a held villager makes to the others (reaction 32, `WAIT_FOR_MATE`) and the meeting of `CHECK_INTERACT_WITH_VILLAGER` are TODO rows ([villagers](../../../beings/villager-life.md#love-outside-the-home-and-the-breeder)) |
| A birth sounds a baby's cry where the child appears | done | one of ten samples, 3D, in the game bank (`HousewifeGivingBirth`, `src/ECS/Villager/VillagerBirth.cpp`); `test/test_villager_birth.cpp` |
| A woman falls pregnant and gives birth after a pregnancy, a child appearing at home | done | the pregnancy's count down and the birth states 110 to 112, the child at home (`src/ECS/Villager/VillagerBirth.cpp`); `test/test_villager_birth.cpp` |
| The town's desire for children drives how many are born | done | `WillHousewifeGetPregnant` reads the town's desire for children (`src/ECS/Villager/VillagerBirth.cpp`); tests `VillagerBirthTest.WillHousewifeGetPregnant`, `TownDesireTest.ForChildren` |
| Children follow their mother about | done | `CHILD_FOLLOWS_MOTHER` (`src/ECS/Villager/VillagerDecide.cpp`); test `VillagerDecideTest.ChildFollowsMother` |
| Children go to the crèche when the town has one, and play there | done | `ChildGotoCreche` and `CHILD_AT_CRECHE` (`src/ECS/Villager/VillagerChild.cpp`); See ../building/civic_buildings.md; tests `VillagerBirthTest.ChildGotoCreche`, `VillagerBirthTest.ChildAtCrecheByDay`, `VillagerBirthTest.ChildAtCrecheByNight` |
| Children whose mother dies are orphaned and mourn her | done | `FindChildrenAndOrphanThem` and the mourning (`src/ECS/Villager/VillagerMourning.cpp`); tests `VillagerDeathTest.Orphans`, `VillagerDeathTest.DeleteDependantsOfAWoman`, `VillagerDeathTest.MourningStates` |
| Children play in the town rather than work | partial 80% | children follow their mother or go to the creche instead of working, and take only the child column of the town's desires (`ChildDecideWhatToDo`, `src/ECS/Villager/VillagerDecide.cpp`; `test/test_villager_decide.cpp`); their play is the relaxation's artefact dance (playtime takes no one in the original, and no one under 15 plays football), ported from raffclar PR #294 and not yet compared with the exe in play; See play_and_gossip.md |
| Breeder disciples raise the birth rate of the town they are put in | done | the breeder calls the villagers of the other sex to it while its town is not too far over full, its spot within 300 m of the town's nearest building, as the exe's (`src/ECS/Villager/VillagerBreeding.cpp`; [the original](../../../beings/villager-life.md#love-outside-the-home-and-the-breeder)); `test/test_villager_breeding.cpp`, `test/test_villager_reactions.cpp`; See disciples.md |
| Abodes swap a man for a woman between them so couples can form | done | `SwapMaleForFemaleFrom` in the town's shuffle (`src/ECS/Town/AbodeVillagers.cpp`); test `VillagerHomeTest.Shuffle` |

## Tribes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine tribes, each with its own villager models but the Africans, who wear the Aztecs' (the game has no African villager model) | done | the villager info table by tribe (`src/Enums.h`, `src/ECS/Archetypes/VillagerArchetype.cpp`); test `VillagerTableData.NineTribesOfSevenAndTheAfricansWearTheAztecsModels` (the row said each tribe had its own; corrected 2026-10-26 from info.dat) |
| Each tribe has its own speed and need tables in the info data | done | the info's speeds, food, life and age values are read by the villager code (`src/InfoConstants.h`, `src/ECS/VillagerSpeed.cpp`, `src/ECS/Villager/VillagerFood.cpp`, `src/ECS/Villager/VillagerAge.cpp`); tests `VillagerInfoRow.TwoRowsOfOneTribeAndJobKeepTheirOwnData`, `VillagerInfoRow.AnOrdinaryVillagerReadsTheRowItsTribeAndJobFind` |
| A villager that moves into a town of another tribe changes tribe | done | `ChangeTribeIfRequired` on leaving `GO_HOME_AND_CHANGE` (`src/ECS/Villager/VillagerHome.cpp`); test `VillagerHomeTest.ChangeInfoKeepsTheNewRow` |
| A town's tribe decides its buildings and its tribal power to the player | done | the town's tribe picks its buildings (the tribe's abode info) and the miracles read the player's per-tribe power (`magic::GetTribalPower`); see ../town/wonder.md; test `MagicSystemTest.ATribesPowerIsThePlayersAndOneForNoTribe` |
