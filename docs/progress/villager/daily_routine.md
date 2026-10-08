# Villager daily routine

What a villager does when nobody is directing it: it offers itself to what its town wants most, sees to its own hunger
and tiredness, goes home at night and sleeps, and fills idle time by sitting out or wandering.

**Progress: 28/36 done, 8 partial — 89%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## Deciding what to do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every villager runs a state machine, one state a turn, with a state it is heading for | partial | the state table (`k_VillagerStateTable`, `src/ECS/Systems/Implementations/LivingActionSystem.cpp`): 98 of the 255 states have their functions, the other 157 are TODO rows that log |
| Entering and leaving states runs their entry and exit rules and transition animations | done | the top-state change runs the exit, entry and validate functions and the into and out-of clips (`villager::SetTopState`, `src/ECS/Villager/VillagerCore.cpp`; `src/ECS/VillagerAnimations.cpp`); `test/test_villager_core.cpp` |
| An idle villager decides what to do: what its town wants most that it can serve, past its own trigger | done | `DECIDE_WHAT_TO_DO` (`src/ECS/Villager/VillagerDecide.cpp`) and `town_desire::CheckVillagerNeededForTownDesire`; `test/test_villager_decide.cpp` |
| Children can serve only some desires | done | the child column of the desire table (`src/ECS/Town/TownDesire.cpp`) and the child's own decision (`src/ECS/Villager/VillagerDecide.cpp`) |
| A villager's own hunger and life press it before the town's wants | done | the own desires (food and life against the trigger) are served before the town's (`src/ECS/Villager/VillagerDecide.cpp`) |
| The town's desires a villager can take up: food, wood, abodes, civic buildings, build, repair, workshop, worship supplies, playtime, relaxation, sleep | partial | food, wood, abodes, civic buildings, building, repairs and sleep take villagers (`src/ECS/Villager/VillagerSatisfy.cpp`); the workshop, worship supplies, playtime and relaxation are TODO and take none |
| Villagers pause between decisions | done | `PAUSE_FOR_A_SECOND` and `WAIT_FOR_COUNTER` between decisions, with their clips (`src/ECS/Villager/VillagerCore.cpp`) |
| The moving speed of each state, slowed by loads and wounds, quickened by needs, belief, magic food and emergencies | partial | `src/ECS/VillagerSpeed.cpp` (`SetVillagerStateSpeed`): loads, wounds, needs and the emergency; the belief and wonder terms are fixed at 1 and nothing gives the magic food speed-up |
| Villagers walk round buildings and obstacles to their goal | done | the walk with wall hugging (`src/ECS/MobileWalkPaths.cpp`, `components::WallHug`); `test/mobile_wall_hug/test_mobile_wall_hug.cpp` |
| Villagers keep to footpaths between buildings | partial | a walk takes a footpath when one serves (`living_footpath`, `src/ECS/LivingFootpath.cpp`), but `MOVE_ON_PATH` only walks to the first node and stops (a TODO row); See ../terrain/ |

## Home and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the town wants sleep, and villagers go home | done | `DesireForSleep` by the day clock (`src/ECS/Town/TownDesire.cpp`) and `GO_HOME` (`src/ECS/Villager/VillagerHome.cpp`) |
| Villagers walk to their abode's door and go in, and are not drawn while inside | done | the walk to the door (`abode_queries::GetArrivePos`) and in (`ARRIVES_HOME`, `AT_HOME`, `src/ECS/Villager/VillagerHome.cpp`); `test/test_villager_home.cpp` |
| Inside, villagers go to bed and sleep while the town wants sleep, longer when hurt | done | `GOTO_BED_AT_HOME` and `SLEEPING_AT_HOME` (`src/ECS/Villager/VillagerHome.cpp`) |
| Villagers wake in the morning and come out | done | `WAKE_UP_AT_HOME` (`src/ECS/Villager/VillagerHome.cpp`) |
| Windows light up and chimneys smoke while someone is in | done | See ../building/abodes.md |
| Now and then an idle villager just goes home | done | the idle branch's go home (`src/ECS/Villager/VillagerDecide.cpp`, `src/ECS/Villager/VillagerHome.cpp`) |
| Home checks: hunger at home, illness at home, needs at home | done | `CheckNeedsAtHome` and the at-home checks (`src/ECS/Villager/VillagerHome.cpp`) |
| A villager whose home is destroyed becomes homeless | done | the destroyed abode's villagers are made homeless and go on the town's homeless list (`abode_villagers::RemoveAllVillagersFromAbode`, `MakeHomeless`, `src/ECS/Villager/VillagerHome.cpp`) |
| Homeless villagers move into an abode with room when one is built | done | `CheckHomelessMoveIntoAbode` (`src/ECS/Villager/VillagerHome.cpp`) |
| Homeless villagers eat their dinner outside and sleep in a tent by a tree | partial | the homeless sleep in a tent (`SLEEP_IN_TENT`, `src/ECS/Villager/VillagerHome.cpp`); `HOMELESS_EAT_DINNER` is a TODO row |
| Vagrants with no town wander until they find one to join | done | `VAGRANT_START` and the vagrants list (`src/ECS/Villager/VillagerHome.cpp`, `town_villagers::Vagrants`) |
| Villagers move house when their abode is too crowded or another suits better | done | `CheckNeedNewAbode` and the town's shuffle (`src/ECS/Villager/VillagerHome.cpp`, `town_villagers::ShuffleVillagersAroundAbodes`) |
| A villager tapped on its abode goes home or comes out | done | `villager::SetStateWhenTappedOnAbode` (`src/ECS/Villager/VillagerEmergency.cpp`): those inside come out; `test/test_villager_repair.cpp` |

## Food and dinner

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry over time | done | the hunger check drops the villager's food over the turns (`src/ECS/Villager/VillagerFood.cpp`); `test/test_villager_food.cpp` |
| Hungry villagers go and eat: at home, from the storage pit, or outside | done | `ChangeStateToFindFoodToEat`: at home, from the storage pit or what it carries (`src/ECS/Villager/VillagerFood.cpp`) |
| Housewives fetch food from the storage pit, make dinner, serve it and clear it away | n/a | the housewife's states are TODO rows. Our wiki differs: the housewife's day (fetching food, cooking, serving and clearing dinner, housework, gossip) is dead code in this version of the original ([villagers](../../bw1-notes/villagers.md#pregnancy-and-births)); See jobs.md |
| Villagers sit down to dinner at home, waiting for it when the housewife is late | n/a | `SITS_DOWN_TO_DINNER` and `WAIT_FOR_DINNER` are TODO rows. Our wiki differs: the housewife's meal call, which makes villagers wait for dinner, is dead code in this version of the original ([villagers](../../bw1-notes/villagers.md#pregnancy-and-births)) |
| A meal takes the villager's meal amount of food from the abode | done | `GetFoodFromHome` takes the meal's amount from the abode (`src/ECS/Villager/VillagerFood.cpp`) |
| Hungry adults walk slower | done | `src/ECS/VillagerSpeed.cpp`, reading the villager's food |
| Starving villagers lose life and lie weak on the ground | partial | a hungry villager loses life (`src/ECS/Villager/VillagerFood.cpp`); `WEAK_ON_GROUND` is a TODO row |
| Food poisoned by the player harms those who eat it | done | poisoned food makes the eater lose life and show it (`SHOW_POISONED`, `src/ECS/Villager/VillagerFood.cpp`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md); Lethys poisons a village's food in [The Plague](../story/silver_scrolls/the_plague.md) |
| Magic food speeds up those who eat it for a while | partial | the speed rule and its countdown (`src/ECS/VillagerSpeed.cpp`, `ProcessFoodSpeedup`); eating never sets it |
| Villagers pick up food they find and are interested in | done | the food reaction (`GOTO_FOOD_REACTION`, `ARRIVES_AT_FOOD_REACTION`, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); `test/test_villager_reactions.cpp` |

## Idle time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing to do, villagers wander near their town | done | the idle branch (`NOTHING_TO_DO` and the chill-out walks, `src/ECS/Villager/VillagerDecide.cpp`) |
| They sit and chill out outside their home or about the town in the evening | done | `GO_AND_CHILLOUT_OUTSIDE_HOME`, `SIT_AND_CHILLOUT` and `GO_AND_CHILLOUT_IN_TOWN` (`src/ECS/Villager/VillagerDecide.cpp`) |
| Villagers notice interesting things nearby (animals, trees, rocks, pots, the ball, fields, abodes, other villagers) and interact | partial | the checks on abodes, fields and fish farms are ported (`CHECK_INTERACT_WITH_ABODE`, `_FIELD`, `_FISH_FARM`); those on animals, trees, rocks, pots, the ball, worship sites and other villagers are TODO rows |
| Villagers pause for a second now and then | done | `PAUSE_FOR_A_SECOND` (`src/ECS/Villager/VillagerCore.cpp`) |
| Villagers yawn, stretch and play idle animations | done | each state's clip, the into and out-of clips and the hard-coded idle clips (`src/ECS/VillagerAnimations.cpp`, `src/ECS/VillagerAnimationTable.h`); `test/test_villager_draw_golden.cpp` |
