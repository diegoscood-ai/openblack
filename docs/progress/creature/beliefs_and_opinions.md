# Beliefs and opinions

What a creature knows and thinks about the world: what each kind of thing is like and whether it is good to act on for
each desire (its decision trees), its opinion of each action, how it feels about the player and about other creatures,
and how the towns feel about it.

**Progress: 23/56 done, 0 chosen, 15 partial, 6 inferred — 56%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/overview.md).

## Decision trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| For each desire it keeps two decision trees: which things are worth making a plan for, and which things to act on | done | `creature_tree` in `src/Creature/CreatureDecisionTree.h`; `TreeKind::Goal` and `ActOn` in `src/Creature/CreatureMindModel.h`, the mind file's two trees in its order; used by `CreatureMindSystem` (`CreatureMindLearning.cpp`); tests `CreatureMindModel.ALessonAboutWhatThePlanWasForTeachesTheGoalTreeOneAboutWhatItWasDoneToTheActOnTree`, `CreatureMindFile.ModelKeepsAFileItWasLoadedFrom` |
| Each tree learns from the newest sixteen examples of things acted on and the feedback for them | done | `creature_tree::AddEpisode` through `creature_mind_model::Learn` in `CreatureMindSystem::LearnTurn`; test `CreatureDecisionTree.KeepsTheNewestSixteenExamples` |
| Feedback is sorted into eleven steps from -1 to 1 | done | `creature_tree::BucketOf`; test `CreatureDecisionTree.BucketsFeedbackByTheFirstStepWithinAQuarter` |
| The tree is grown again from scratch after each new example, testing at each step the attribute that best separates the feedback | done | `creature_tree::Build`, `Gain`, `Entropy` (`src/Creature/CreatureDecisionTree.cpp`); tests `CreatureDecisionTree.GainPicksTheAttributeThatSeparatesFeedback`, `CreatureDecisionTree.BuildsAndEvaluates` |
| A tree with no examples knows nothing, and a thing it knows nothing about is worth a little | done | tests `CreatureDecisionTree.EmptyTreeKnowsNothing`, `CreatureDecisionTree.UsefulnessFromUtility` |
| Each desire's trees may only test the attributes the game's tables allow it | partial 80% | `tables->attributes` (`creature_mind_tables`) passed to every `Learn` in `CreatureMindLearning.cpp`; no test pins the gate |
| The trees decide which food it eats first, and which thing it picks for every plan | done | the act-on tree for food (`mind_detail::FoodUsefulness`, in `NearestFood`) and for what a plan is done to, the goal tree for what a plan is for (`Usefulness` and `ObjectCandidate::goalUsefulness`), in `CreatureMindLearning.cpp` and `CreatureMindSystem.cpp`; test `CreatureAnimalFoodTest.HowGoodAFoodIsToEatIsWhatItLearntOfActingOnSuchThings` |
| The trees are saved in and restored from the mind file | partial 50% | restored: a creature made from a mind file takes its trees up (`CreatureMindSystem::TakeUpFile`, `creature_mind_model::FromFile`, `RebuildTrees`); test `CreatureMindModel.LearningRebuildsTrees`. Saving: `CreatureMindSystem::SaveMind` writes them, but nothing in the game calls it, where the original saves the mind with the profile (`SaveMind` 0x4E7820) |

## What it knows about each kind of thing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every thing has seven common attributes: whose it is, natural or made, alive or not, which player, whether stronger than the creature, which species, and its kind | done | `mind_detail::BeliefOf` in `CreatureMindLearning.cpp`; test `CreatureDecisionTree.SlotsFollowTheKindOfThing`. "Stronger" is a creature drawn taller than the creature of the believer's player, 0 for every other thing, as `AttributeIsHarderThanMe::UpdateValue` 0x4D40E0 ([mind.md](../../../creature/mind.md#the-attributes-a-belief-reads); `creature_tree::HarderThanMeValue`, tests `CreatureDecisionTree.AnotherCreatureIsHarderOnlyWhenTallerThanThePlayersCreature`, `CreatureMindSystemTest.AnotherCreatureIsHarderWhenDrawnTallerThanTheBelieversPlayersCreature`) |
| Villagers: sex, job, alive, on fire, tribe | partial 80% | `BeliefOf`: sex, job, alive (above a fifth of its life, `creature_tree::LifeValue`, as the exe's `AttributeLife::UpdateValue` 0x4D39A0: vt+0x11C above 0.2), on fire and tribe are set; a villager is always nobody's rather than its town's player's |
| Other creatures: stronger than it, species, height, the miracles they know, what they carry, their dominant desire | partial 80% | `BeliefOf`. As the exe: the species; the caps of the miracles known (42), the thing carried (44, 29 for nothing: `AttributeCreatureWhatHeIsCarrying::UpdateValue` 0x4D3D80, the held thing's info +0x10) and the dominant desire (40), the ranges 0x2B, 0x2D and 0x29 of `ValueRange` 0x4D5E50, 0x4D5EB0, 0x4D5D80; the desire through `mind_detail::HeldPlan`. The height bands the size it is drawn at (below 0.25 → 0, below 0.75 → 1, below 1.25 → 2, else 3), as `AttributeCreatureHeight::UpdateValue` 0x4D3C70 ([mind.md](../../../creature/mind.md#the-attributes-a-belief-reads); `creature_tree::CreatureHeightValue`, test `CreatureDecisionTree.AnotherCreaturesHeightIsInFourBandsOfTheSizeItIsDrawnAt`); "stronger" is as the row above. Not traced: the miracle count is mind +0x1AA10 in the exe (0x4D3D31), ours the number of miracles known |
| Homes: their kind, alive, on fire, being built | done | `BeliefOf`: kind, alive (above a fifth of its life, as `AttributeLife` 0x4D39A0), on fire and being built (`BeingBuiltValue`, as `AttributeAbodeBeingBuilt::UpdateValue` 0x4D3FC0); test `CreatureMindSystemTest.EachThingIsBelievedInByTheKindItsClassGivesIt` |
| Trees, features, and loose objects | done | `BeliefOf` (trees, features, mobile objects and pots), their kinds as [mind.md](../../../creature/mind.md#the-kind-of-belief-each-class-gives); tests `CreatureObjectBeliefs.ALooseThingIsBelievedInByItsRowsKind`, `CreatureObjectBeliefs.AMovableStaticIsARockOnlyOfTheRockClass` |
| The temple | partial 50% | `BeliefOf` knows the temple as the creature's own player's (the citadel's belief, type 2). A worship site and the altar at its head are temple parts the planner may act on (`Target::TemplePart`, test `CreatureCastTargetsTest.ATemplePartIsATempleAWorshipSiteOrTheAltarAtItsHead`); their beliefs (type 4, `CitadelPart` 0x464AA0) and the altar's place in the map cells, by which the creature sees it, are not in yet |
| Towns: their belief in the gods, what they need most, their size | pending | towns give no belief in `BeliefOf` |
| Forests: their size | pending | forests give no belief |
| Flocks of animals | pending | flocks give no belief |
| Single animals | partial 40% | `BeliefOf` gives an animal its owner and whether it is alive: above a fifth of its life, as the exe's `AttributeLife` 0x4D39A0 for every object ([mind.md](../../../creature/mind.md#the-attributes-a-belief-reads); `creature_tree::LifeValue`, test `CreatureAnimalFoodTest.WhatACreatureKnowsOfAnAnimal`); animals have no belief class of their own in the exe (type 7, the common attributes), and which of those ours leaves unset is not checked against it; test file `test/creature/test_creature_animal_food.cpp` |
| Fields | pending | fields give no belief |
| Miracles and spell seeds | pending | miracles and spell seeds give no belief |
| The place and situation it acts in (its context) | pending | no context is kept |
| It remembers what it believes about particular things it has met, not just their kind | partial 40% | it remembers which things it has come across, by kind, with the game's pool sizes (`creature_object_beliefs::Store`, swept from its eyes each turn; tests `CreatureObjectBeliefs.*`), and when it last looked another creature over; what it believes about them (the attributes and the believed place) is still worked out afresh each time (`BeliefOf`) |
| Each thing it comes across is remembered by the kind its class gives it: a rock as a rock, poo as poo, magic food as magic food, a spell icon as a spell icon, and a big forest, a lantern, a shield, a highlight or the singing stone's base as a thing of no kind of its own | done | `mind_detail::BeliefKindOf` (`CreatureMindBeliefs.cpp`), `creature_object_beliefs::MobileObjectBeliefKind` and `MobileStaticBeliefKind`, `object_physics::IsOfRockClass`; tests `CreatureObjectBeliefs.ALooseThingIsBelievedInByItsRowsKind`, `CreatureObjectBeliefs.AMovableStaticIsARockOnlyOfTheRockClass`, `CreatureMindSystemTest.EachThingIsBelievedInByTheKindItsClassGivesIt`; evidence in [mind.md](../../../creature/mind.md#the-kind-of-belief-each-class-gives) |
| It weighs how edible, how damaged, how dangerous, how interesting and how impressed a thing is | inferred | the exe has `HowImpressedIsObject` 0x4C3EC0, `HowMuchDangerNearObject` 0x4C3FF0 and their kind, but xref finds no reference to any of them (no call, no pointer), so whether the game weighs them is unverified; ours has food value (`CreatureObjectActionSystem::FoodValueOf`) and interest by kind (`creature_look`) |
| It weighs how useful a thing is for a nice or a nasty purpose | inferred | no function of the exe found for it (xref names searched: Purpose, Useful, Nice, Nasty) |
| A script teaches it a distinction about a kind of thing | pending | CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT is a `NotImplemented` stub in `src/CHLApi.cpp` |

## Opinions of actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an opinion of every action, from -1 to 1, which the player's feedback changes | done | `creature_learning::OpinionAfter` from `CreatureMindSystem::LearnFromFeedback`; test `CreatureLearning.OpinionsAndFeedbackStrength` |
| Opinions break ties between actions for a desire | done | `creature_planner` in `CreatureMindSystem::PlanTurn`; test `CreaturePlanner.OpinionBreaksTies` |
| It remembers how long since it last did each action | partial 80% | `Learnt::turnsSinceDone` (`src/Creature/CreatureMindModel.h`), counted up each turn in `CreatureMindSystem::ProcessTurn` and read by the planner (the `CreaturePlanner` candidates in `test_creature_learning.cpp`); no test covers the count itself |
| It counts how often it did each action | done | `creature_training::CountCarriedOut` / `TimesCarriedOut` on the mind's `actionCounts`, counted as an action is carried out unless a script controls it, read by the script's number-of-times native (`GetActionCount`); test `CreatureTraining.EachActionCarriedOutCountsUnlessAScriptControlsIt`. The reset native and the second getter are not called by the shipped scripts ([../scripts/challenge_natives_creature.md](../scripts/challenge_natives_creature.md)) |
| It has a skill at each action, which rises with practice | pending | the exe keeps one (`CreatureLearning::GetSkillAtAction` 0x4E22E0, five callers) |

## How it feels about the player

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has an attitude to the player, from hating to loving, moved by strokes and slaps | done | `creature_feedback::AttitudeAfter` (0.7 × old + 0.05 × feedback, as `UpdateAttitudeToPlayerFromFeedback` 0x4E1080 → fn_004C81D0, [script-control.md](../../../creature/script-control.md)) in `CreatureMindSystem::UpdateAttitudeFromFeedback`, from the hand's feedback and from being struck by its player's thrown thing (`CreatureStruck.cpp`); test `CreatureFeedback.StrokesAndSlapsAddUp` |
| A poor attitude makes it want to run away from the player | partial 50% | the gate as the exe: running away from the player is planned only below an attitude of 0 or for no number (`HasPlayerBeenNasty` 0x4DF150; test `CreaturePlanner.SomeDesiresArePlannedOnlyInTheirState`); the desire's source 43 is ours `Clamp01(-attitude)` (`CreatureMindSystem.cpp`), whose getter in the exe is not found, and the `RunAwayFromPlayer` agenda (`Build::RunFromPlayer`) is not checked against the exe's builder 0x4A3F10 |
| Its attitude shows on its face when it reflects on the player | done | `creature_face` (smile or sad, by `attitudeToPlayer`); see [face, eyes and hair](face_eyes_hair.md) |
| It judges how abusive the player is | inferred | the exe has `HowAbusiveIsPlayer` 0x4C3F40 (minus the attitude to the player), but xref finds no reference to it, so whether the game uses it is unverified |
| It judges how neglectful the player is | inferred | the exe has `HowNeglectfulIsPlayer` 0x4C3F60 (on the turns not looked at), but xref finds no reference to it, so whether the game uses it is unverified |
| It remembers whether the player has been nasty to it | done | in the exe this is the attitude below 0 (`HasPlayerBeenNasty` 0x4DF150, the run-away gate), not a memory of its own: the attitude is kept (`CreatureMindState::attitudeToPlayer`) and the gate tested (`CreaturePlanner.SomeDesiresArePlannedOnlyInTheirState`) |
| It keeps the most recent feedback the player gave it | done | `CreatureMindState::lastFeedback`, set in `CreatureMindSystem::ReceiveFeedback`; the hand's sum in `CreatureHandSystem::GetLastFeedbackSum`; tested in `test/creature/test_creature_hand_system.cpp` |
| It guesses what the player wants from what it sees the player do, and that guess fades | partial 70% | `creature_perceived_desires` (`Creature/PerceivedDesires`, the mind's `perceivedDesires`): the add, the fade each turn and the scroll's reader as the original ([mimicry.md](../../../creature/mimicry.md#the-perceived-desires-in-openblack)); tests `PerceivedDesires.*`, `PerceivedDesiresMindTest.*`. Reported: the villager sites, the spell site and the feedback; not yet the other writers (`Villager::EndPhysics` 0x5F0B53, `Rock::InterfaceTap` 0x6E7541, the worship-site and abode deeds) |
| It sees the player's deeds only within two thirds of a half turn either way, or in its own cell | done | `creature_perceived_desires::CanSeePos`, `LookYaw`; tests `PerceivedDesires.ACreatureSeesTwoThirdsOfAHalfTurnEitherWayOrInItsCell`, `PerceivedDesires.ItLooksWhereItsHeadLooksUnlessAtNothingOrWhereItStands` |
| It is cross with the player and shows it | done | below an attitude of -0.1 the plan action `BeCrossWithPlayer` may be chosen, as the exe's `IsCrossWithPlayer` 0x4D31D0 ([0x8C9B2C] = -0.1); it faces the camera and makes the rude sign (222) or, on one draw of two, taunts (70), as `ConstructSubActionsForBeCrossWithPlayer` 0x4B4160; test `CreaturePlayerAgendas.BeingCrossFollowsTheCameraThenMakesARudeSignOrTaunts` ([object actions](object_actions.md)) |
| It shows the player how nice it thinks he is | done | plan action `ShowPlayerHowNiceYouThinkHeIs`: it faces the camera, then waves (72) above an attitude of 0, else plays the sad clip (56), as `ConstructSubActionsForShowPlayerHowNiceYouReckon` 0x4AF660; test `CreaturePlayerAgendas.HowNiceItThinksThePlayerIsIsAWaveOrTheSadClip` |
| Its opinion of its god shows in the Creature Cave | done | `creature_cave` (`opinionOfGod`) from `CreatureCaveSystem`; tests in `test/test_creature_mode.cpp`; see [creature cave](creature_cave.md) |
| Its attitude is saved in its mind file | partial 50% | read from the file in `CreatureMindSystem::TakeUpFile`; `creature_mind_model::ToFile` writes it where the exe's `SaveMind` does (+0x18C58, 0x4E7D89), but nothing in the game saves the mind |

## How it feels about other creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps how nice it thinks each creature it has met is, from -1 to 1 | done | `creature_learning::AttitudeTo`, `ChangeHowNice`, used in `CreatureMindSystem::LearnTurn`; test `CreatureLearning.AttitudesToCreatures` |
| How nice it thinks a creature is moves its desires towards being friends or angry with it | partial 30% | what moves the feeling: a leash to another creature (`mind.leash.attitudes` in `CreatureMindSystem::LearnTurn`; test `LeashRules.LeashedCreaturesWarmOrCoolToEachOther`), and an action that ends well, by the action table's `creatureNicenessChange` on the creature acted on, itself for an action on no thing (vanilla bug), as the exe's 0x475643 into `UpdateHowNiceIsCreature` 0x4C4D50 ([mind.md](../../../creature/mind.md#the-creature-action-table)); meeting or fighting does not; how the feeling then moves its desires about that creature is the planner's own weighing, not the original's |
| It judges whether another creature seems friendly | partial 50% | the rule as the exe's object filter (`IsCreatureWhoSeemsFriendly`, the Creature override 0x4E45E0): neither wants to be angry with or afraid of the other (`creature_mind::SeemsFriendly`; test `CreatureCastAgenda.AnotherCreatureSeemsFriendlyUnlessItIsAngryOrAfraidOrTheCreatureIsAngry`); the desire each wants most about the other is the planner's own weighing ([casting.md](../../../creature/casting.md)) |
| It judges whether another creature is the dominant one | pending | the exe has `Creature::IsDominantCreature` 0x4792F0 (vt+0x2F4, one call site) |
| It judges how much stronger an opponent is, how much more life it has and how many dangerous miracles it knows | inferred | the exe has `HowMuchStrongerIsOpponentThanMe` 0x4C3BF0, `HowMuchMoreLifeHasOpponentThanMe` 0x4C3C70 and `HowManyDangerousSpellsOpponentKnows` 0x4C3D00, but xref finds no reference to them, so whether the game uses them is unverified; see [fighting](fighting.md) |
| It reacts to another creature only so often, by how it feels about it | partial 60% | another creature is urgent only once due a look-over: never looked over, or not for more than the info's inspect-creature seconds (`GCreatureInfo` +0x2CC; tests `CreatureReactionRules.AnotherCreatureIsUrgentOnceDueALookOver`, `ACreatureIsDueALookOverOnlyAfterTheWait`; [mind.md](../../../creature/mind.md)), and taken up only when the other is of use to its curiosity; whether how it feels about the other also weighs is not read |
| It shows a creature it hates it | pending | the exe's action `ShowCreatureYouHateHim` (builder 0x4AC780); see [friends](friends_and_other_creatures.md) |

## Its thoughts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It keeps its newest thoughts (what it learnt, what it saw the player do) | done | `creature_mind_model::Think`, `k_MaxThoughts` (8), from `CreatureMindSystem::LearnTurn`; test `CreatureLearning.ThoughtsAndLikes` |
| It knows the thing it likes most, with its picture and name, for the Creature Cave | inferred | the Cave's likes are per desire (`creature_learning`, `CreatureCave` `likes`); that the original's Cave shows one liked thing with its picture and name is not checked against the exe; see [creature cave](creature_cave.md) |

## How the towns feel about it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Towns watch what a creature does with fear or respect, and change their view of it | partial 30% | `creature_object_actions::AttitudeTo` sets `CreatureTownAttitude` (fear 30 s, respect 10 s) on the creature in `CreatureObjectActionSystem` (test `CreatureObjectActions.TheTownFearsThrowingAndEatingVillagers`), one view for every town; nothing gives respect, and no town reads it yet (the exe's side: `CanBeImpressedByCreature`, `UpdateHowImpressed`); see [town actions](town_actions.md) and [../town/](../town/) |
| A creature impresses villagers by what it does, which adds to belief | pending | towns do not count a creature's deeds; see [town actions](town_actions.md) and [../worship/](../worship/) |
| The creature is told how a town feels about it | pending | see [lessons and help](lessons_and_help.md) |
