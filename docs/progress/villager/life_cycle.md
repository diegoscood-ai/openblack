# Villager life cycle

Villagers are born, grow up, pair off, have children and die of old age. Each belongs to a tribe and a town, and lives
in an abode with a family.

**Progress: 24/28 done, 2 partial — 89%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## Creation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers placed by the land script at a position, near an abode, with a tribe, job and age | done | CREATE_VILLAGER_POS (`src/LHScriptX/FeatureScriptCommands.cpp`) and the villager's constructor with its age, scale and food draws (`src/ECS/Archetypes/VillagerArchetype.cpp`, `villager::Construct`, `src/ECS/Villager/VillagerCore.cpp`); `test/test_villager_core.cpp` |
| Town villagers created straight into a town by script (by town, by type, special ones) | done | CREATE_TOWN_VILLAGER, CREATE_SPECIAL_TOWN_VILLAGER and CREATE_VILLAGER (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| A new villager joins the nearest town and an abode in it with room | done | the map script's handlers house it: the abode at the position, else an abode with room in the town, else the vagrants (`src/LHScriptX/FeatureScriptCommands.cpp`, `town_villagers::AddVillagerToTown`) |
| A villager made over water starts drowning instead of standing | done | made in the water it starts in `DROWNING` (`villager::Construct`) |
| A newly made villager waits a random while before its first decision | done | `CREATED`, then the first decision (`src/ECS/Villager/VillagerCore.cpp`) |
| Sex follows the job: housewives are women, the other jobs men | done | the villager info's sex (`src/ECS/Archetypes/VillagerArchetype.cpp`) |
| Each villager has its own small difference in speed and size from when it was made | done | the speed factor (`src/ECS/VillagerSpeed.cpp`) and the scale drawn for the age (`SetScaleForAge`, `src/ECS/Villager/VillagerAge.cpp`) |

## Age and growing up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Age counts in game time, a year every fixed number of turns, with birthdays | done | the age kept as the birth turn, 1500 turns a year (`src/ECS/Villager/VillagerAge.cpp`); `test/test_villager_age.cpp` |
| Children are smaller, scaled up with age until adult size | done | the child's scale by age, rescaled every quarter year (`src/ECS/Villager/VillagerAge.cpp`) |
| Children use the child models of their tribe | done | the child meshes of the tribe (`src/ECS/Archetypes/VillagerArchetype.cpp`) |
| A child becomes an adult at the age of growing up, takes a job and the town's counts change | done | the child grows up at its info's age, the counts move, and `GO_HOME_AND_CHANGE` gives it the adult mesh (`src/ECS/Villager/VillagerAge.cpp`, `src/ECS/Villager/VillagerHome.cpp`) |
| Old villagers walk slower; young ones too | done | the age factor of the speed (`src/ECS/VillagerSpeed.cpp`) |
| Villagers die of old age, more likely the older they get, checked at bed time | done | the old age death checked when going to bed (`CheckWhenGoingToBed`, `src/ECS/Villager/VillagerAge.cpp`, `src/ECS/Villager/VillagerHome.cpp`) |

## Families and children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A couple shares an abode; a man and a woman are spouses | done | the abode's man and woman slots (`abode_villagers::AddVillagerToAbode`, `src/ECS/Town/AbodeVillagers.cpp`) |
| Couples have sex at home at night, more often when the town wants children | partial | a woman falls pregnant at bed time with a man at home, by the town's desire for children (`CheckWhenGoingToBed`, `CheckGetPregnantAtHome`, `src/ECS/Villager/VillagerHome.cpp`); the at-home sex states are TODO rows |
| Couples meet outside and have sex when promiscuous or waiting for a mate | todo | `START_HAVING_SEX`, `HAVING_SEX` and `WAIT_FOR_MATE` are TODO rows |
| A woman falls pregnant and gives birth after a pregnancy, a child appearing at home | done | the pregnancy's count down and the birth states 110 to 112, the child at home (`src/ECS/Villager/VillagerBirth.cpp`); `test/test_villager_birth.cpp` |
| The town's desire for children drives how many are born | done | `WillHousewifeGetPregnant` reads the town's desire for children (`src/ECS/Villager/VillagerBirth.cpp`) |
| Children follow their mother about | done | `CHILD_FOLLOWS_MOTHER` (`src/ECS/Villager/VillagerDecide.cpp`) |
| Children go to the crèche when the town has one, and play there | done | `ChildGotoCreche` and `CHILD_AT_CRECHE` (`src/ECS/Villager/VillagerChild.cpp`); See ../building/civic_buildings.md |
| Children whose mother dies are orphaned and mourn her | done | `FindChildrenAndOrphanThem` and the mourning (`src/ECS/Villager/VillagerMourning.cpp`) |
| Children play in the town rather than work | partial | children follow their mother or go to the creche instead of working (`src/ECS/Villager/VillagerDecide.cpp`); their play is not ported; See play_and_gossip.md |
| Breeder disciples raise the birth rate of the town they are put in | todo | no breeder disciple; See disciples.md |
| Abodes swap a man for a woman between them so couples can form | done | `SwapMaleForFemaleFrom` in the town's shuffle (`src/ECS/Town/AbodeVillagers.cpp`) |

## Tribes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine tribes, each with its own villager models | done | the villager info table by tribe (`src/Enums.h`, `src/ECS/Archetypes/VillagerArchetype.cpp`) |
| Each tribe has its own speed and need tables in the info data | done | the info's speeds, food, life and age values are read by the villager code (`src/InfoConstants.h`, `src/ECS/VillagerSpeed.cpp`, `src/ECS/Villager/VillagerFood.cpp`, `src/ECS/Villager/VillagerAge.cpp`) |
| A villager that moves into a town of another tribe changes tribe | done | `ChangeTribeIfRequired` on leaving `GO_HOME_AND_CHANGE` (`src/ECS/Villager/VillagerHome.cpp`) |
| A town's tribe decides its buildings and its tribal power to the player | done | the town's tribe picks its buildings (the tribe's abode info) and the miracles read the player's per-tribe power (`magic::GetTribalPower`); see ../town/wonder.md |
