# Beliefs and opinions

What a creature knows and thinks about the world: what each kind of thing is like and whether it is good to act on for
each desire (its decision trees), its opinion of each action, how it feels about the player and about other creatures,
and how the towns feel about it.

**Progress: 17/56 done, 15 partial — 44%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Decision trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| For each desire it keeps two decision trees: which things to act on, and which things to use doing so | done | `creature_tree` in `src/Creature/CreatureDecisionTree.h`; `TreeKind` in `src/Creature/CreatureMindModel.h`; used by `CreatureMindSystem` (`CreatureMindLearning.cpp`) |
| Each tree learns from the newest sixteen examples of things acted on and the feedback for them | done | `creature_tree::AddEpisode` through `creature_mind_model::Learn` in `CreatureMindSystem::LearnTurn`; test `CreatureDecisionTree.KeepsTheNewestSixteenExamples` |
| Feedback is sorted into eleven steps from -1 to 1 | done | `creature_tree::BucketOf`; test `CreatureDecisionTree.BucketsFeedbackByTheFirstStepWithinAQuarter` |
| The tree is grown again from scratch after each new example, testing at each step the attribute that best separates the feedback | done | `creature_tree::Build`, `Gain`, `Entropy` (`src/Creature/CreatureDecisionTree.cpp`); tests `CreatureDecisionTree.GainPicksTheAttributeThatSeparatesFeedback`, `CreatureDecisionTree.BuildsAndEvaluates` |
| A tree with no examples knows nothing, and a thing it knows nothing about is worth a little | done | tests `CreatureDecisionTree.EmptyTreeKnowsNothing`, `CreatureDecisionTree.UsefulnessFromUtility` |
| Each desire's trees may only test the attributes the game's tables allow it | done | `tables->attributes` (`creature_mind_tables`) passed to every `Learn` in `CreatureMindLearning.cpp` |
| The trees decide which food it eats first, and which thing it picks for every plan | done | `mind_detail::FoodUsefulness` (food, in `NearestFood`) and `Usefulness` for plans, in `CreatureMindLearning.cpp` and `CreatureMindSystem.cpp` |
| The trees are saved in and restored from the mind file | partial | restored: a creature made from a mind file takes its trees up (`CreatureMindSystem::TakeUpFile`, `creature_mind_model::FromFile`, `RebuildTrees`); `CreatureMindSystem::SaveMind` writes them but nothing in the game calls it; test `CreatureMindModel.LearningRebuildsTrees` |
| The trees can be read as text, as the game's own mind viewer shows them | partial | `creature_tree::Describe`, only shown in the debug Creature spawner (`Debug/CreatureSpawnerMind.cpp`) |

## What it knows about each kind of thing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every thing has seven common attributes: whose it is, natural or made, alive or not, which player, whether stronger than the creature, which species, and its kind | done | `mind_detail::BeliefOf` in `CreatureMindLearning.cpp`; test `CreatureDecisionTree.SlotsFollowTheKindOfThing` |
| Villagers: sex, job, alive, on fire, tribe | partial | `BeliefOf`: sex, job, alive and tribe are set; on fire is always no, and a villager is always nobody's rather than its town's player's |
| Other creatures: stronger than it, species, height, the miracles they know, what they carry, their dominant desire | partial | `BeliefOf`: stronger, species, height, carrying and dominant desire are set; the miracles a creature knows are always read as none |
| Homes: their kind, alive, on fire, being built | partial | `BeliefOf`: kind and alive are set; on fire and being built are always no |
| Trees, features, and loose objects | done | `BeliefOf` (trees, features, mobile objects and pots) |
| The temple | partial | `BeliefOf` knows the temple as the creature's own player's; the temple's parts give no belief |
| Towns: their belief in the gods, what they need most, their size | todo | towns give no belief in `BeliefOf` |
| Forests: their size | todo | forests give no belief |
| Flocks of animals | todo | flocks give no belief |
| Single animals | partial | `BeliefOf` gives an animal its owner and whether it is alive, nothing of its species; test file `test/creature/test_creature_animal_food.cpp` |
| Fields | todo | fields give no belief |
| Miracles and spell seeds | todo | miracles and spell seeds give no belief |
| The place and situation it acts in (its context) | todo | no context is kept |
| It remembers what it believes about particular things it has met, not just their kind | partial | beliefs are worked out afresh each time from the thing (`BeliefOf`); no list per thing is kept |
| It weighs how edible, how damaged, how dangerous, how interesting and how impressed a thing is | partial | food value (`CreatureObjectActionSystem::FoodValueOf`) and interest by kind (`creature_look`) exist; damage, danger and impressedness are not weighed |
| It weighs how useful a thing is for a nice or a nasty purpose | todo | |
| A script teaches it a distinction about a kind of thing | todo | CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT is a `NotImplemented` stub in `src/CHLApi.cpp` |

## Opinions of actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an opinion of every action, from -1 to 1, which the player's feedback changes | done | `creature_learning::OpinionAfter` from `CreatureMindSystem::LearnFromFeedback`; test `CreatureLearning.OpinionsAndFeedbackStrength` |
| Opinions break ties between actions for a desire | done | `creature_planner` in `CreatureMindSystem::PlanTurn`; test `CreaturePlanner.OpinionBreaksTies` |
| It remembers how long since it last did each action | done | `Learnt::turnsSinceDone` (`src/Creature/CreatureMindModel.h`), counted up each turn in `CreatureMindSystem::ProcessTurn` |
| It counts how often it did each action | todo | CREATURE_GET_NUM_TIMES_ACTION_PERFORMED and CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION are `NotImplemented` stubs in `src/CHLApi.cpp` |
| It has a skill at each action, which rises with practice | todo | |

## How it feels about the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an attitude to the player, from hating to loving, moved by strokes and slaps | partial | `creature_feedback::AttitudeAfter` in `CreatureMindSystem::ReceiveFeedback` (mostly what it was, nudged by the feedback; unconfirmed against the game) |
| A poor attitude makes it want to run away from the player | partial | `ReadSource` (`k_RunAwayFromPlayer`) in `CreatureMindSystem.cpp` (unconfirmed) |
| Its attitude shows on its face when it reflects on the player | done | `creature_face` (smile or sad, by `attitudeToPlayer`); see [face, eyes and hair](face_eyes_hair.md) |
| It judges how abusive the player is | todo | |
| It judges how neglectful the player is | todo | |
| It remembers whether the player has been nasty to it | todo | |
| It keeps the most recent feedback the player gave it | done | `CreatureMindState::lastFeedback`, set in `CreatureMindSystem::ReceiveFeedback`; the hand's sum in `CreatureHandSystem::GetLastFeedbackSum` |
| It guesses what the player wants from what it sees the player do, and that guess fades | todo | not in our tree: the player-desire events (`creature_mimic::EmpathiseWithTownDesire`) are published but nothing listens yet ([creature.md](../../bw1-notes/creature.md#town-and-villager-hooks)) |
| It sees the player's deeds only within two thirds of a half turn either way, or in its own cell | todo | nothing models what the creature sees of the player's desires |
| It is cross with the player and shows it | todo | |
| It shows the player how nice it thinks he is | todo | |
| Its opinion of its god shows in the Creature Cave | done | `creature_cave` (`opinionOfGod`) from `CreatureCaveSystem`; test file `test/temple/test_creature_cave.cpp`; see [creature cave](creature_cave.md) |
| Its attitude is saved in its mind file | partial | read from the file in `CreatureMindSystem::TakeUpFile`; `creature_mind_model::ToFile` writes it, but nothing in the game saves the mind |

## How it feels about other creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps how nice it thinks each creature it has met is, from -1 to 1 | done | `creature_learning::AttitudeTo`, `ChangeHowNice`, used in `CreatureMindSystem::LearnTurn`; test `CreatureLearning.AttitudesToCreatures` |
| How nice it thinks a creature is moves its desires towards being friends or angry with it | partial | only a leash to another creature changes the attitude (`mind.leash.attitudes` in `CreatureMindSystem::LearnTurn`); meeting or fighting does not |
| It judges whether another creature seems friendly | todo | |
| It judges whether another creature is the dominant one | todo | |
| It judges how much stronger an opponent is, how much more life it has and how many dangerous miracles it knows | todo | see [fighting](fighting.md) |
| It reacts to another creature only so often, by how it feels about it | todo | |
| It shows a creature it hates it | todo | see [friends](friends_and_other_creatures.md) |

## Its thoughts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps its newest thoughts (what it learnt, what it saw the player do) | done | `creature_mind_model::Think`, `k_MaxThoughts` (8), from `CreatureMindSystem::LearnTurn`; test `CreatureLearning.ThoughtsAndLikes` |
| It knows the thing it likes most, with its picture and name, for the Creature Cave | partial | the Cave's likes are per desire (`creature_learning`, `CreatureCave` `likes`), not a liked thing with its picture and name; see [creature cave](creature_cave.md) |

## How the towns feel about it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Towns watch what a creature does with fear or respect, and change their view of it | partial | `creature_object_actions::AttitudeTo` sets `CreatureTownAttitude` (fear 30 s, respect 10 s) on the creature in `CreatureObjectActionSystem`, one view for every town, and no town reads it yet; see [town actions](town_actions.md) and [../town/](../town/) |
| A creature impresses villagers by what it does, which adds to belief | todo | towns do not count a creature's deeds; see [town actions](town_actions.md) and [../worship/](../worship/) |
| The creature is told how a town feels about it | todo | see [lessons and help](lessons_and_help.md) |
