# Decision making

How a creature decides what to do next: it plans for its desires, picks the thing to act on and the action to take,
turns the action into an agenda of small steps, carries them out, and copes when they fail. Scripts and the computer
gods can also take control of a creature.

**Progress: 18/63 done, 21 partial — 45%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Planning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each desire gets a plan: a thing to act on (the goal) and an action to take | done | `creature_planner::PlanDesire` (`src/Creature/CreaturePlanner.h`) from `CreatureMindSystem::PlanCreature` (`CreatureMindLearning.cpp`); test `CreaturePlanner.PlansTheMostUsefulGoal` |
| A couple of desires are planned again each turn, going round the ones that can be planned | done | `creature_planner::NextGoals` (two a turn) in `CreatureMindSystem::PlanTurn`; test `CreaturePlanner.GoesRoundTheEligibleDesires` |
| The goal is the thing the creature has learnt is most useful for the desire, the nearer the better, up to 200 away | done | `creature_planner::DistancePriority`, `k_MaxGoalDistance` in `mind_detail::Gather`, the trees through `Usefulness` in `CreatureMindLearning.cpp`; test `CreaturePlanner.DistancePriority` |
| Something it already holds is preferred as a goal | done | `creature_planner::k_HeldPriority` (1.6), `held` set in `PlanCreature` |
| A thing it knows nothing about is worth a little | done | `creature_planner::k_DefaultUsefulness` (0.1) |
| Among the actions that satisfy the desire, its opinion of each decides | done | `creature_planner::ActionPriority`; test `CreaturePlanner.OpinionBreaksTies` |
| An action not done for a long time gets a little extra novelty | done | `creature_planner::Novelty`, `k_NoveltyTurns`; test `CreaturePlanner.ActionPriorityAndNovelty` |
| A plan's priority is the desire's strength times how useful the goal, action and things used are | done | `creature_planner::Priority`; test `CreaturePlanner.PriorityAndSwitching` |
| The creature changes what it does only for a plan more than twice as pressing | done | `creature_planner::ShouldSwitch`; test `CreaturePlanner.ChoosesOnlyWhatIsPressingEnough` |
| A plan must reach a minimum priority to be taken up at all | partial | `k_MinPlanPriority` (15) in `CreatureMindLearning.cpp`, a stand-in for the idle activities the game weighs through its planner (unconfirmed) |
| Actions are weighed by the kind of thing they are done to, so each has a goal it can be done to | done | groups by `creature_plan_actions::Target` in `CreatureMindSystem::PlanCreature` |
| Actions it can't do now (no water near, no target, can't afford a miracle) are left out | partial | `creature_plan_actions::Possible` (no water near, no hurl target); creatures cast no miracles in our tree, so there is no miracle cost check |
| Only actions the creature knows can be chosen | partial | every action with an executor can be chosen; there is no per-creature known-action list, and creatures cast no miracles in our tree (see [creature casting](creature_casting.md)) |
| The planner can carry out the game's whole range of actions | partial | about 45 of the game's actions have an executor (`src/Creature/CreaturePlanActions.cpp`); miracles, town work, building, dancing, friends, games and stealing have none; see [object actions](object_actions.md) and [town actions](town_actions.md) |
| Something shown to it on the leash is the only goal it plans for | done | `mind.leash.actOn` in `CreatureMindSystem::PlanCreature`; see [leash](leash.md) |
| When free, the creature plans every desire at once before falling back on idling | done | `PlanCreature(entity, mind, true)` in `CreatureMindSystem::ProcessTurn` |
| The idle choices (sitting, hanging around, being idle) are weighed through the planner as the game does | partial | a stand-in random choice (`creature_mind::k_ActivityLots` in `src/Creature/CreatureIdleMind.cpp`); see [idle behaviour](idle_behaviour.md) |
| Needs (hunger, thirst, sleep, poo) are weighed against everything else through the planner | partial | a stand-in: a need over 0.3 is seen to first (`creature_mind::k_ActOnNeed`) |
| What it chose for itself runs to its end unless it was only idling | partial | `Interruptible` in `CreatureMindLearning.cpp` (unconfirmed) |
| The plan it takes up is remembered, with what it acted on, for the player's feedback to be credited to | done | `creature_learning::Remember` in `CreatureMindSystem::Adopt`; see [learning](learning_from_feedback.md) |
| How far it can see limits what it plans for | partial | goals are gathered within `creature_planner::k_MaxGoalDistance` of it (`mind_detail::Gather`); the vision range by size (`creature_look::LookRange`) is only used for looking |

## Agendas and steps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An action becomes an agenda of small steps (go near, turn to face, play an action, wait, let go …) | done | `creature_mind::Step` (`src/Creature/CreatureIdleMind.h`), `creature_plan_actions::Agenda`; test `CreaturePlanActions.BuildsAgendas` |
| The game has well over a hundred kinds of step; all of them are available | partial | going, following, fleeing, turning, picking up, putting down, holding, eating, throwing, destroying, sleeping, sitting and emotes exist (`creature_mind::Step`, `ObjectOrder`); casting, dancing, building, repairing, kissing, talking, fishing, fire, totems, disciples, mind swaps and teleports do not |
| Going near a thing, keeping clear of it by both their sizes | partial | walking up to a thing to within a distance (`CreatureLocomotionSystem::MoveToObject`); the clearance by both their sizes is not in our tree |
| Getting away from a thing to a clear area | partial | fleeing from a point to a valid spot (`CreatureLocomotionSystem::FleeFrom`, `land_avoid::NearestValid`); no search for a clear area |
| Turning to face a thing, the camera or the hand | partial | facing a point and the camera (`Movement::Kind::TurnToFace`, `Build::FaceCameraEmote`); facing the hand and another player's camera do not exist |
| Steps start only when the body is free | done | test `CreatureIdleMind.NothingStartsWhileTheBodyIsBusy` |
| A step with a time limit, such as following, ends when its time is up | done | test `CreatureIdleMind.AFollowStepStopsWhenItsTimeIsUp` |
| An action finishes successfully and satisfies its desire, or unsuccessfully and doesn't | partial | carried out to its end, the desire is satisfied (`Satisfied` in `CreatureMindSystem::FollowAgenda`); a failed agenda is given up (`gaveUp`); the game's separate unsuccessful ending (with a lesson and help) is missing |
| Being slapped stops what it is doing | done | `CreatureMindSystem::ReceiveFeedback` cancels the hands, stops it and clears the agenda |
| Stopping what it does lets go of a miracle it holds and ends its animation | partial | `CreatureMindSystem::Replan` ends its animation and movement; creatures hold no miracles in our tree |
| Waiting for another creature to be free, or for a thing to come within reach | todo | |
| Repeating its last action | todo | |

## When things go wrong

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't find a route to its goal | partial | the locomotion refuses (`MoveResult::InvalidDestination` in `CreatureLocomotionSystem`) and the agenda is given up; the explanation shown to the player is missing (see [lessons and help](lessons_and_help.md)) |
| Its route is blocked | partial | routes plan round things through the route planner (`RouteFollower` in `CreatureLocomotionSystem`); no reaction to a blocked route |
| Its goal is out of reach (cut off land) | partial | an invalid destination is refused (`IsValid` in `CreatureLocomotionSystem::StartMove`); no explanation |
| The opponent refused to fight | todo | see [fighting](fighting.md) |
| The other creature refused to play | todo | |
| The other creature refused to be friends | todo | |
| The leash stopped it | partial | the leash stops it (`mind.leash.obeying`; see [leash](leash.md)); no explanation |
| It lost sight of what it was tracking | todo | |
| Stuck in one place for a while, it gives up and holds back that desire | todo | not in our tree |
| Trapped in an enclosed space, it gets itself out | todo | |
| Something in the way, it finds an action to clear it (knocking it down, moving it) | todo | |
| It responds to emergencies first: being on fire, fainting, being in danger | partial | fainting when exhausted, starved or out of life works (`CreatureMindSystem::ProcessTurn`); putting out fire on itself and fleeing danger first do not |
| It weighs how dangerous a thing is and whether anything scary is near it, its home or where it is going | partial | running from a thing or the player exists (`Build::RunFromObject`, `Build::RunFromPlayer` in `CreaturePlanActions.cpp`); danger is not weighed, and there is no frightening-thing target |
| Lacking skill, it sometimes messes an action up (a throw, a miracle, fishing, a dance, making fire, impressing, a totem) | todo | nothing fails for lack of skill; creatures cast no miracles in our tree |
| It answers requests from other creatures (to fight, to play, to be friends, to learn) | todo | see [friends](friends_and_other_creatures.md) |

## Control by scripts and the computer gods

The script language is the story domain's: see [../story/](../story/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script makes a creature do an action, to a thing or at a place | todo | CREATURE_DO_ACTION is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script moves a creature to a place | todo | MOVE_GAME_THING on a creature is `NotImplemented` in `src/CHLApi.cpp` |
| A script plays one of a creature's animations, once or held | todo | the script state and animation natives skip creatures (`NotImplemented` in `src/CHLApi.cpp`) |
| A script sets the priority of what the creature is doing | todo | CREATURE_SET_AGENDA_PRIORITY is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script forces a creature to finish what it is doing | todo | CREATURE_FORCE_FINISH is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A creature under a script's control does nothing of its own until released | todo | SET_FOCUS and the walks skip creatures in `src/CHLApi.cpp` |
| A script teaches a creature everything, or a single action | todo | CREATURE_LEARN_EVERYTHING, CREATURE_LEARN_EVERYTHING_EXCLUDING and CREATURE_SET_KNOWS_ACTION are `NotImplemented` stubs in `src/CHLApi.cpp` |
| A script asks how often a creature did an action, and resets the count | todo | the two NUM_TIMES natives are `NotImplemented` stubs in `src/CHLApi.cpp` |
| A script asks whether a thing is in a creature's hand | todo | IN_CREATURE_HAND is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script creates a creature next to another | todo | CREATURE_CREATE_RELATIVE_TO_CREATURE is a `NotImplemented` stub in `src/CHLApi.cpp` |
| A script gives a creature to another player, or swaps creatures | todo | CREATURE_SET_PLAYER and SWAP_CREATURE are `NotImplemented` stubs in `src/CHLApi.cpp` |
| A script calls the player's creature to a place | done | CALL_PLAYER_CREATURE (`CallPlayerCreature` in `src/CHLApi.cpp`) hands the script the creature the player leads, through the leash service. Our wiki differs: the native only finds the player's creature for the script, it does not call it to a place ([creature.md](../../bw1-notes/creature.md#the-players-creature)) |
| A script puts a creature in its mind's development stage | partial | SET_CREATURE_DEV_STAGE (`src/CHLApi.cpp`, `player_creature::SetDevelopmentStage`) sets the stage; the desires follow at the mind's next turn, the original's at once; see [development phases](development_phases.md) |
| The computer gods command their creatures to do things and teach them | todo | |
| The computer gods' creatures have their knowledge balanced for the difficulty | todo | |
| Multiplayer locked selection of a creature across the network | n/a | network play is not supported; see [../multiplayer/](../multiplayer/) |
