# Villager daily routine

What a villager does when nobody is directing it: it offers itself to what its town wants most, sees to its own hunger
and tiredness, goes home at night and sleeps, and fills idle time by sitting out or wandering.

**Progress: 31/38 done, 0 chosen, 5 partial, 2 inferred — 91%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-decisions.md).

## Deciding what to do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every villager runs a state machine, one state a turn, with a state it is heading for | partial 60% | the state table (`k_VillagerStateTable`, `src/ECS/Systems/Implementations/LivingActionSystem.cpp`): 149 of the 254 states have their functions, the other 105 are TODO rows that log (the housewife's 100-109, which the original never reaches, the football, the shepherd, the tell-others, inspect, impressed and fight reactions, the magic tree, gossip, the CHECK_INTERACT_WITH_* rows but the field's and fish farm's, and the rest); save and load of the states are not ported |
| Entering and leaving states runs their entry and exit rules and transition animations | done | the top-state change runs the exit, entry and validate functions and the into and out-of clips (`villager::SetTopState`, `src/ECS/Villager/VillagerCore.cpp`; `src/ECS/VillagerAnimations.cpp`); the plain walks' validate (states 1, 47, 71, 154) checks nothing, as the original's, and their exit (states 1, 2, 222) lets go of the circle the walker held; a walk left for a reaction or a script is kept, to go on with; `test/test_villager_core.cpp`, `test/test_living_turn.cpp` |
| An idle villager decides what to do: what its town wants most that it can serve, past its own trigger | done | `DECIDE_WHAT_TO_DO` (`src/ECS/Villager/VillagerDecide.cpp`) and `town_desire::CheckVillagerNeededForTownDesire`; `test/test_villager_decide.cpp` |
| Children can serve only some desires | done | the child column of the desire table (`src/ECS/TownDesire.cpp`) and the child's own decision (`src/ECS/Villager/VillagerDecide.cpp`); tests `TownDesireTest.Table`, `VillagerDecideTest.DecideWhatToDo` |
| A villager's own hunger and life press it before the town's wants | done | the own desires (food and life against the trigger) are served before the town's (`src/ECS/Villager/VillagerDecide.cpp`); tests `VillagerDecideTest.CheckSatisfyOwnDesire`, `VillagerNeeds.TheTriggerIsTheGreaterNeedAndHalfTheLesser`, `VillagerNeeds.TheStrongerOwnNeedIsSeenToFirst` |
| The town's desires a villager can take up: food, wood, abodes, civic buildings, build, repair, workshop, worship supplies, playtime, relaxation, sleep | partial 90% | food, wood, abodes, civic buildings, building, repairs, the workshop (`CheckSatisfySupplyWorkshop`), worship supplies (`CheckSatisfySupplyWorship`) and sleep take villagers (`src/ECS/Villager/VillagerSatisfy.cpp`); playtime takes none, as the original's `CheckSatisfyPlaytimeDesire` is a literal 0; relaxation weighs the football pitch and the artefacts (`CheckSatisfyRelaxation`, `ArtefactSystem::OfferRelaxation`), but a villager given the pitch does nothing, as no match is played; `test/test_town_desire.cpp`, `test/test_villager_decide.cpp` |
| Villagers pause between decisions | done | `PAUSE_FOR_A_SECOND` and `WAIT_FOR_COUNTER` between decisions, with their clips (`src/ECS/Villager/VillagerCore.cpp`); tests `VillagerCoreTest.PauseForASecondChance`, `VillagerCoreTest.PauseEndsInTheFinalState`, `VillagerNeeds.TheLessLifeTheLikelierAPause` |
| The moving speed of each state, slowed by loads and wounds, quickened by needs, belief, magic food and emergencies | partial 95% | `src/ECS/VillagerSpeed.cpp` (`SetVillagerStateSpeed`): loads, wounds, needs, age, the emergency, the land's balance, the Indian tribal power and the town's belief in its player; the magic food factor is applied, but nothing sets it (see Food and dinner); `test/test_villager_speed.cpp` |
| Villagers walk round buildings and obstacles to their goal | done | the walk with wall hugging (`src/ECS/LivingWalkPath.cpp`, `src/ECS/WallHugRules.cpp`, `components::WallHug`); a walker going round with no circle held heads straight on and looks ahead, as the original's; `test/mobile_wall_hug/test_mobile_wall_hug.cpp`, `test/test_pathfinding_gone_obstacle.cpp` |
| Villagers keep to footpaths between buildings | partial 70% | the walk to a building takes its footpaths and walks them node by node (`villager::SetupMoveToOnFootpath`, `living_footpath::MoveOnFootpath`; `test/test_use_footpath.cpp`), for home, food, the jobs, building, the workshop, the breeder, cover, the creature dance, the worship walks and the hiding at the site; the altar rest, the footpath reaction walk, the dance walk and MOVE_TO_OBJECT (a TODO row) still walk straight ([routes](../../../beings/routes.md#pending), W4); See ../terrain/ |

## Home and sleep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At night the town wants sleep, and villagers go home | done | `DesireForSleep` by the day clock (`src/ECS/TownDesire.cpp`) and `GO_HOME` (`src/ECS/Villager/VillagerHome.cpp`); tests `TownDesireTest.Sleep`, `VillagerDecideTest.CheckNeededForTownDesireSleep`, `VillagerDecideTest.GoHome` |
| Villagers walk to their abode's door and go in, and are not drawn while inside | done | the walk to the door (`abode_queries::GetArrivePos`) and in (`ARRIVES_HOME`, `AT_HOME`, `src/ECS/Villager/VillagerHome.cpp`); `test/test_villager_home.cpp` |
| Inside, villagers go to bed and sleep while the town wants sleep, longer when hurt | done | `GOTO_BED_AT_HOME` and `SLEEPING_AT_HOME` (`src/ECS/Villager/VillagerHome.cpp`); tests `VillagerHomeTest.Sleeping`, `VillagerRoutine.SleepGoesOnWhileTheTownWantsItOrLifeIsLow` |
| Villagers wake in the morning and come out | done | `WAKE_UP_AT_HOME` (`src/ECS/Villager/VillagerHome.cpp`); tests `VillagerHomeTest.Sleeping`, `VillagerHomeTest.ExitAtHome` |
| Windows light up and chimneys smoke while someone is in | done | See ../building/abodes.md; tests `VillageLights.WindowsOfAHomeWithPeopleInLightUpAtNight`, `ChimneySmoke.LitWhileSomeoneIsHome` |
| Now and then an idle villager just goes home | done | the idle branch's go home (`src/ECS/Villager/VillagerDecide.cpp`, `src/ECS/Villager/VillagerHome.cpp`); tests `VillagerRoutine.NothingToDoIsOneInNineHomeThreeOutsideFiveInTown`, `VillagerRoutine.NothingToDoWithABrokenHomeGoesHomeOneTimeInTen` |
| Home checks: hunger at home, illness at home, needs at home | done | `CheckNeedsAtHome` and the at-home checks (`src/ECS/Villager/VillagerHome.cpp`); tests `VillagerHomeTest.HomeDecideWhatToDo`, `VillagerHomeTest.NeedsAtHomePregnantOrChild`, `VillagerHomeTest.ArrivesHomeHurtOrHungry` |
| A villager whose home is destroyed becomes homeless | done | the destroyed abode's villagers are made homeless and go on the town's homeless list (`abode_villagers::RemoveAllVillagersFromAbode`, `MakeHomeless`, `src/ECS/Villager/VillagerHome.cpp`); test `VillagerLifeWorld.AVillagerWhoseHomeIsDestroyedBecomesHomeless` (`test/test_villager_life_world.cpp`, on synthetic and the game's data) |
| Homeless villagers move into an abode with room when one is built | done | `CheckHomelessMoveIntoAbode` (`src/ECS/Villager/VillagerHome.cpp`); test `VillagerHomeTest.CheckNeedNewAbode` |
| Homeless villagers sleep in a tent by a tree | done | `SLEEP_IN_TENT` and the tent's place (`src/ECS/Villager/VillagerHome.cpp`); `VillagerHomeTest.SleepInTent`, `TentNextToTree`, `TentSpiral` |
| Homeless villagers eat their dinner outside | inferred | `HOMELESS_EAT_DINNER` is a TODO row; the original's state function is empty (returns 1, bw1-decomp `VillagerFood.cpp` and the v1.42 bytes at 0x75C0F0) and no setter of the state was found, so whether the game ever enters it is unverified; the homeless eat what they carry or from the store as anyone does (`ChangeStateToFindFoodToEat`) |
| Vagrants with no town wander until they find one to join | done | `VAGRANT_START` and the vagrants list (`src/ECS/Villager/VillagerHome.cpp`, `town_villagers::Vagrants`); tests `VillagerHomeTest.VagrantStroll`, `TownStateSystem.vagrantsGoInFrontOnce` |
| Villagers move house when their abode is too crowded or another suits better | done | `CheckNeedNewAbode` and the town's shuffle (`src/ECS/Villager/VillagerHome.cpp`, `town_villagers::ShuffleVillagersAroundAbodes`); tests `VillagerHomeTest.CheckNeedNewAbode`, `VillagerHomeTest.Shuffle` |
| A hurt villager goes home to recover | done | the every-turn check sends a villager whose life is below the info's threshold home, unless its state forbids it (`CheckEveryTime`, `src/ECS/Villager/VillagerCore.cpp`); `test/test_villager_home.cpp` |
| A villager tapped on its abode goes home or comes out | done | `villager::SetStateWhenTappedOnAbode` (`src/ECS/Villager/VillagerEmergency.cpp`): those inside come out; `test/test_villager_repair.cpp` |

## Food and dinner

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry over time | done | the hunger check drops the villager's food over the turns (`src/ECS/Villager/VillagerFood.cpp`); `test/test_villager_food.cpp` |
| Hungry villagers go and eat: at home, from the storage pit, or outside | done | `ChangeStateToFindFoodToEat`: at home, from the storage pit or what it carries (`src/ECS/Villager/VillagerFood.cpp`); tests `VillagerFoodTest.ChangeStateToFindFoodToEat`, `VillagerFoodTest.ChangeStateToFindFoodToEatWithLittle` |
| A meal takes the villager's meal amount of food from the abode | done | `GetFoodFromHome` takes the meal's amount from the abode (`src/ECS/Villager/VillagerFood.cpp`); tests `VillagerFoodTest.GetFoodFromHomeTakesTwice`, `VillagerFoodTest.EatingStates` |
| Hungry adults walk slower | done | `src/ECS/VillagerSpeed.cpp`, reading the villager's food; test `VillagerLifeWorld.AHungryGrownVillagerWalksSlower` (`test/test_villager_life_world.cpp`, on synthetic and the game's data) |
| Starving villagers lose life and die of hunger | done | a hungry villager loses life and dies with reason Starving when its life runs out (`CheckHungry`, `HungerBatch`, `src/ECS/Villager/VillagerFood.cpp`); `test/test_villager_food.cpp` |
| A villager lies weak on the ground | inferred | `WEAK_ON_GROUND` is a TODO row; the original's state function is empty (returns 1, in bw1-decomp's `VillagerScript.cpp` and the v1.42 bytes at 0x768A10) and info.dat flags it with the script states; no setter was found, so whether hunger (or anything but a script) puts a villager in it is unverified |
| Food poisoned by the player harms those who eat it | done | poisoned food makes the eater lose life and show it (`SHOW_POISONED`, `src/ECS/Villager/VillagerFood.cpp`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md); Lethys poisons a village's food in [The Plague](../story/silver_scrolls/the_plague.md); tests `VillagerFoodTest.HungerDamage`, `VillagerFoodTest.APoisonedVillagerShowsItAfterAMealAtHomeSteppingOutFirst`, `VillagerResourcesTest.TakingFromAPoisonedStoreOrPilePoisonsTheTaker` |
| Magic food speeds up those who eat it for a while | partial 50% | the speed rule (times the info's food power-up) and its countdown, one every 10 turns (`src/ECS/VillagerSpeed.cpp`, `ProcessFoodSpeedup`); taking food from a speed-up pile never sets it, where the original's `GetResourceFrom` does ([jobs](../../../beings/villager-jobs.md)) |
| Villagers pick up food they find and are interested in | done | the food reaction (`GOTO_FOOD_REACTION`, `ARRIVES_AT_FOOD_REACTION`, `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`); `test/test_villager_reactions.cpp` |

## Idle time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing to do, a villager goes home, sits out by its home, or walks to a spot near the town's meeting point (by a draw of one in nine, three in nine, five in nine) | done | the idle branch (`SetupNothingToDo` and the chill-out walks, `src/ECS/Villager/VillagerDecide.cpp`); `test/test_villager_decide.cpp` |
| They sit and chill out outside their home or about the town, at least 500 turns, looking for something to do every 101 | done | `GO_AND_CHILLOUT_OUTSIDE_HOME`, `SIT_AND_CHILLOUT` and `GO_AND_CHILLOUT_IN_TOWN` (`src/ECS/Villager/VillagerDecide.cpp`); tests `VillagerDecideTest.GoAndChilloutOutsideHome`, `VillagerDecideTest.SitAndChillout`, `VillagerDecideTest.GetChillOutPos` |
| Villagers pause for a second now and then | done | `PAUSE_FOR_A_SECOND` (`src/ECS/Villager/VillagerCore.cpp`); tests `VillagerCoreTest.PauseForASecondChance`, `VillagerCoreTest.PauseEndsInTheFinalState`, `VillagerNeeds.TheLessLifeTheLikelierAPause` |
| Villagers yawn, stretch and play idle animations | done | each state's clip, the into and out-of clips and the hard-coded idle clips (`src/ECS/VillagerAnimations.cpp`, `src/ECS/VillagerAnimation.cpp`, `src/ECS/Systems/Implementations/VillagerAnimate.cpp`); `test/test_villager_draw_golden.cpp` |
