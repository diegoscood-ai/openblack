# Learning from feedback

The player teaches the creature with the hand: holding the action button on it puts the hand on its body, resting the hand
there strokes it and sweeping the hand across it slaps it. When the hand lets go, the creature credits the reward or
punishment to what it did lately and learns from it: how much to want the desire behind the action, which kinds of thing
to do it to and use, and what it thinks of the action itself.

**Progress: 52/73 done, 0 chosen, 14 partial, 3 inferred — 83%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../../creature/mind.md).

## Taking hold of the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can never pick a creature up; holding the action button on one puts the hand on it instead | done | `CreatureHandSystem::Grab` (`src/ECS/Systems/Implementations/CreatureHandSystem.cpp`), from the hand's press in `HandTurn.cpp`; tests `CreatureHandSystemTest.*` |
| A quick click on the player's own creature, before any stroke or slap, puts the leash on instead of holding it | done | let go within 450 ms of camera time is a click (`creature_hand::IsClick`, `src/Creature/CreatureHandRules.h`), sent as the leash key (`ECS/CreatureHandPackets.cpp`); tests `CreatureHand.LetGoWithinAClicksCameraTimeIsAClick`, `CreatureHandPacketsTest.TheClickIsThePlayersLeashKey`; see [leash.md](leash.md) |
| Only the player's own creature or an allied player's can be held, stroked and slapped, in or out of the influence; nobody's and an enemy god's cannot | done | `creature_hand::MayHold` refuses nobody's creature, and `creature_hand::TakesPress` takes only the pressing player's own or an allied player's (`creature_hand::IsFriendlyTo`), as the original's press and its hold test 0x476E10 ([states.md](../../../hand/states.md#the-hand-on-a-creature)); tests `CreatureHand.OnlyTheOwnersHandTakesHoldOnAPress`, `CreatureHand.ACreatureIsFriendlyToItsOwnPlayerAndToTheirAllies` |
| A creature frozen by the freeze spell can't be held | done | `creature_hand::MayHold`, frozen from `components::CreatureSpells` in `CreatureHandSystem`, as the original's freeze test ([states.md](../../../hand/states.md#the-hand-on-a-creature)); test `CreatureHand.TheHandHoldsAnyPlayersCreature` |
| A creature under a script's control, or in the first steps of its dead action, can't be held | pending | the original refuses both (Flags +0x25 & 4; mind +0xF60 = 9, the dead action, at +0xFB4 <= 2; [states.md](../../../hand/states.md#the-hand-on-a-creature)); `creature_hand::MayHold` refuses neither and refuses a sleeping creature instead (`Holdable::asleep`), which the original does not |
| Ogres can never be held | done | `creature_hand::MayHold`, as the original (info +0x1F4 != 13); test `CreatureHand.TheHandHoldsAnyPlayersCreature` |
| The hand's tooltip for interacting shows only over the player's own awake creature | inferred | the original's owner rule for the 0xE85 tooltip (0x5D7406) is not read ([overview.md](../../../creature/overview.md)); ours, `creature_hand::ShowsInteractTip` (used by `HandToolTips.cpp`), shows it over the own creature and over another player's in the influence, and does not test that it is awake (test `CreatureHand.TheInteractTipIsForTheOwnCreatureOrAnotherInTheInfluence`) |
| While held, the hand is drawn on the body where the cursor meets it, or beside the creature when the cursor is off it | partial 50% | `CreatureHandSystem::Update` gives the hand's pose on the body or on the upright plane through the creature, used by `HandSystem`; the original's gap off the body (a twentieth of the height, eased over 0.3 s), its second try along the cursor's last two places and its keeping back from the camera off the body are missing (see [../hand/creature_contact.md](../hand/creature_contact.md)) |
| The hand finds the point it touches by testing the creature's posed mesh each frame | partial 50% | openblack tests capsules round the posed bones (`creature_feedback::BodyCapsules`, `RayHit`), not the mesh; tests `CreatureFeedback.TheHandTouchesTheBodyWhereTheLineOfSightMeetsIt`, `TheBodyIsACapsuleFromEachJointToItsParentsPlaced` |
| The hand and camera close in on the creature by its size while it is held | partial 70% | the original builds its interaction camera mode as the hold starts (0x44A850) and places it in `CameraModeCtrInteract::Update` 0x44A960, read whole and run in the oracle ([states.md](../../../hand/states.md#the-hand-on-a-creature)): ours (raffclar's model, #348) takes that place, 1.5 drawn heights from the creature's middle in front of it, gliding 0.7 s (tests `CreatureTouch.TheCameraTakesTheOriginalsPlaceForTheCreature`, pinned to the oracle, `TheCameraLooksAtTheCreaturesMiddleFromInFront`, `TheCameraGlidesToTheCreatureInSevenTenthsOfASecond`); still not read: the hand drawn larger by the square root of the size; see [../hand/creature_contact.md](../hand/creature_contact.md) |
| A quick click on another player's creature is no click: it does not work the leash | done | the click packet goes only for the local player's own creature, as the original's 0x5F ([states.md](../../../hand/states.md#the-creature-state-and-its-packets)); test `HandCreatureFrame.AnotherPlayersCreatureIsNoClick` |

## Stroking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand must rest on the body for a second before the first stroke | done | `creature_feedback::k_StrokeHoldMs` (1000 ms of contact) in `CreatureHandSystem`, as the v1.42 exe's creature hand state, which adds the frame's time to its contact clock and strokes only past 1000 ms (0x5B2062..0x5B207D); test `CreatureHandSystemTest.Resting_TheHandStrokes_SweptFastItSlaps` |
| A stroke lands on the nearest of nine body parts: head, both armpits, belly, groin, both feet, both hands | done | `creature_feedback::NearestPart`; test `CreatureFeedback.AStrokeLandsOnTheNearestPart` |
| Each part has its own pleased animation; the left side plays the right side's mirrored | done | `k_RewardAnimations`, `k_RewardMirrored` in `src/Creature/CreatureFeedback.h`, played by `CreatureHandSystem` |
| The creature pulls a face for two seconds: an "aah" for the head and belly, an "ooh" for the groin, a smile elsewhere | done | `creature_feedback::RewardFace`, pulled by `CreatureHandSystem` |
| A new stroke needs another part than the last, a wait since the last stroke or slap ((1 - reward) x 1250 ms) and the hand ready (2 s after taking hold, 1 s after each stroke or slap) | done | `creature_feedback::StrokeDue` on `StrokeClocks`, `StrokeWaitMs`, as the original's creature hand state ([states.md](../../../hand/states.md#the-creature-states-update)); tests `CreatureTouch.AStrokeNeedsANewPartTheWaitAndTheHandReady`, `StrokesWaitLongerTheLessTheCreatureHasBeenRewarded` |
| A stroke only plays when the body is idle or more than four fifths through what it is doing | done | `k_StrokeInterruptsAfter`, `creature_feedback::MayInterrupt` in `CreatureMindSystem::ForceAction`; test `CreatureFeedback.AStrokeOrASlapBreaksInOnlyPastItsShare` |
| Each stroke that plays adds a tenth to the reward, up to the most | done | `creature_feedback::AfterStroke`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| The stroke sounds as the pleased animation plays | inferred | sounds come from the animation's sound events (`src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`); which events the pleased animations carry, and whether the original plays anything else for a stroke, is not checked |

## Slapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand slaps when it sweeps across the creature faster than five of its heights a second, weighed only as the hand comes onto the body or the cursor crosses the screen's middle, and a second after taking hold or the last stroke (slaps don't hold the next one back) | done | `k_SlapSpeed`, `creature_feedback::SlapConsidered`, `k_SlapReadyMs`, as the original ([states.md](../../../hand/states.md#the-creature-states-update)); tests `CreatureFeedback.SlapsAreClassedByHeightAndSpeed`, `CreatureTouch.ASweepIsWeighedAsASlapOnlyComingOntoTheBodyOrCrossingTheMiddle`, `CreatureHandSystemTest.Resting_TheHandStrokes_SweptFastItSlaps` |
| Where it lands goes by the hand's height: the feet below two fifths of the creature's height, the waist below seven tenths, the head above, nothing above eleven tenths | done | `creature_feedback::ClassifySlap`; test `CreatureFeedback.SlapsAreClassedByHeightAndSpeed` |
| Slower than nine heights a second the slap is gentle, with its own gentler animations | partial 80% | `k_HardSlapSpeed`, `k_GentleOffset` in `src/Creature/CreatureFeedback`; the speed is the hand's point across the land, not eased, where the original takes the cursor's two points projected at the hand's depth ([states.md](../../../hand/states.md#the-creature-states-update)); not shown to give the same speed |
| The slap plays to the left or right by which way the hand swept across the screen | done | `Slap::mirrored`, mirrored when the cursor moves left, as the original; test `CreatureTouch.ASlapPlaysMirroredWhenTheHandSweepsLeft` |
| A slap only plays when the body is idle or more than about a third through what it is doing | done | `k_SlapInterruptsAfter` (0.35, as the original); test `CreatureFeedback.AStrokeOrASlapBreaksInOnlyPastItsShare` |
| A gentle slap takes a tenth off the reward, a hard one a fifth, twice that when the creature was enjoying being stroked (past a quarter); the reward never goes below -1 | done | `creature_feedback::AfterSlap`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| Every third hard slap in a row at one height that lands on the body bruises the creature: it loses a hundredth of its life, never going below that, and a bruise is marked on its skin by the slap's speed | partial 60% | `creature_feedback::CountSlap`, the hard-slap packet `creature_hand_packets::ApplyHardSlap` and `LifeAfterBruise`, as the original's 0x63 ([states.md](../../../hand/states.md#the-creature-states-update)), its vanilla bug kept (a slap on another god's creature costs the player's own creature); tests `CreatureTouch.EveryThirdHardSlapInARowAtOneHeightOnTheBodyBruises`, `CreatureHandPacketsTest.AHardSlapsBruiseTakesAHundredthOfThePlayersCreaturesLife`; the bruise the original paints at the touch point and keeps among its marks (0x4866F0) is not drawn (`BruiseColumn` unused); see [marks.md](marks.md) |
| A force-feedback mouse jolts with each slap, harder for a hard one | pending | the original's creature hand state starts and stops its immersion effects ( [states.md](../../../hand/states.md#the-creature-state-and-its-packets)); no force feedback in openblack |
| The slap sounds as it lands | inferred | through the animation's sound events (`CreatureAudioSystem`); what the original plays for a slap is not checked |

## Delivering the feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the hand lets go, the creature's mind is told the reward, from -1 to 1 | done | `CreatureHandSystem::Release`, through `ECS/CreatureHandPackets.cpp` at the next turn, to `CreatureMindSystem::ReceiveFeedback`; tests `CreatureHandSystemTest.LettingGoGivesHowTheCreatureWasTreated`, `TheFeedbackGivenIsClampedToOne`, `CreatureHandPacketsTest.TheFeedbackReachesTheCreaturesMind` |
| Feedback too slight to count (a hundredth or less) only makes the creature look at the player, idling with it, and the hand offers to bring the creature | partial 80% | `ReceiveFeedback` forces LookAtCamera under IdleAroundWithPlayer at score 1 and forces the tooltip 3746 (`helptext::k_ToolTipFocusCreature`), as the original ([mind.md](../../../creature/mind.md#the-players-feedback)); test `PerceivedDesiresMindTest.AStrokeShowsCompassionAndASlapAngerByHowHard` covers only that it teaches nothing; the forced look and the tooltip have no test |
| Any feedback stops what the creature was doing | done | `ReceiveFeedback` ends the action, the walk, the agenda and the plan first, as the original's handler does first ([mind.md](../../../creature/mind.md#the-players-feedback)); test `PerceivedDesiresMindTest.AnyFeedbackFirstStopsWhatItWasDoing` |
| Its attitude to the player moves: seven tenths of what it was plus a twentieth of the feedback | done | `creature_feedback::AttitudeAfter`, in `CreatureMindSystem::UpdateAttitudeFromFeedback`, as the original's 0x4E1080 ([script-control.md](../../../creature/script-control.md)); test `CreatureFeedback.StrokesAndSlapsAddUp` |
| A running average of the feedback it gets, the newest counting most | done | `creature_feedback::AverageAfter`, in `UpdateAttitudeFromFeedback`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| A stroke holds back its sadness for a minute and a half | partial 80% | `k_StrokeSadnessSeconds` (90 s) in `CreatureMindLearning.cpp`, as the original ([mind.md](../../../creature/mind.md#the-players-feedback)); no test |
| A lesson from feedback counts as something to do with the player; the rest of the feedback leaves how long it has gone without the player's attention as it was | done | `LearnFromFeedback`'s lessons path sets `turnsSinceMadeToAct` back to none; nothing in the feedback touches `turnsNotLookedAt` |
| A stroke tells the creature its god wants compassion, a slap anger, by how hard | done | `CreatureMindSystem::UpdateAttitudeFromFeedback` after the slight-feedback test, as the original's 0x4E10D5 ([mimicry.md](../../../creature/mimicry.md)); a blow from a thing its own player threw comes the same way (`src/ECS/CreatureStruck.cpp`); tests `PerceivedDesiresMindTest.AStrokeShowsCompassionAndASlapAngerByHowHard`, `CreatureStruck.OnlyItsOwnPlayersThrowGivesFeedback` |
| Feedback in a networked game reaches the creature on every machine | pending | no network play; see [../multiplayer/](../multiplayer/) |

## Crediting what it did

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Feedback goes to the most recent thing it can learn from: fully while still doing it, less the longer ago it finished, nothing once the action's learning window has closed | done | `creature_learning::LearningPriority`, `BestContext` (`src/Creature/CreatureLearning.cpp`), used in `CreatureMindLearning.cpp`; test `CreatureLearning.CreditGoesToRecentActions` |
| Each action has its own learning window from the game's tables (eating 20 seconds, stomping 25, fighting 30, examining 5 to 10) | done | `src/Creature/CreatureMindTables.cpp` |
| Actions the tables mark as not learnable get no credit | done | `Context::learnable` |
| It remembers its last five actions to credit | done | `creature_learning::Remember`, `k_MaxContexts` (5, the original's ring of five, [mind.md](../../../creature/mind.md#the-players-feedback)); test `CreatureLearning.TheSixthActionMakesItForgetTheOldest` |
| Every action it can do is remembered for credit | partial 55% | only the actions openblack carries out are remembered: 187 of the game's 328 action rows have an executor (`src/Creature/CreaturePlanActions.cpp`) |
| Feedback weaker than a half counts as a half either way | done | `creature_learning::Strengthened`; test `CreatureLearning.OpinionsAndFeedbackStrength` |

## What it learns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| To want the desire more or less: its growing time is multiplied up or down, by an amount set per desire (the desires that matter most change slowly) | done | `creature_learning::LessonFactor`, `LearnDesireLesson`; test `CreatureLearning.LessonFactor` |
| The source that drove the desire most since it was last decided on drives it more or less readily | done | `creature_learning::DominantSource`; test `CreatureLearning.DominantSourceFollowsDrive` |
| The desire's maximum rises or falls by up to a tenth of its starting maximum | done | `k_MaxStep` in `src/Creature/CreatureLearning.h` |
| Its weight changes, for the desires whose tables allow it | done | `DesireRules::learnsWeight` |
| How slowly it fades changes a little | done | `k_DecayStep` |
| Desires that go with it grow faster too, opposed ones slower (compassion and anger oppose each other) | done | `LearnDesireLesson` with the dependency table; test `CreatureLearning.DesireLessonAndDependencies` |
| Desires the tables mark as not learnable aren't changed by feedback | done | `DesireRules::learnable` |
| Which kinds of thing to act on for that desire, and in proportion for the desires that depend on it | done | `creature_learning::Spread`, `creature_mind_model::Learn` (decision trees); tests `CreatureLearning.SpreadsLessonsByDependency`, `CreatureMindModel.LearningRebuildsTrees` |
| Which kinds of thing the plan was for (goal trees), and which to act on unless it acted on itself (act-on trees; for poo at most half as strongly); nothing about what it used | done | `creature_learning::ThingLessons`; test `CreatureMindModel.APlanTeachesWhatItWasForToTheGoalTreeAndWhatItWasDoneToToTheActOnTree` |
| Its opinion of the action moves four fifths of the way to the feedback | done | `creature_learning::OpinionAfter`; test `CreatureLearning.OpinionsAndFeedbackStrength` |
| Its decision trees are grown again from scratch after every new example, keeping at most sixteen examples each | partial 80% | `src/Creature/CreatureDecisionTree` (`k_MaxEpisodes` 16); openblack keeps the newest sixteen, while which the original drops when full is not read |
| Slapped for what it is doing, it gives it up and wants that desire least of all, and every held-down desire is let go | done | `creature_learning::MakeLeastDominant`, `CreatureMindSystem::LearnFromFeedback` |
| Stroked for something it did to a thing, it does it again to the thing of that kind most useful to act on | partial 60% | `LearnFromFeedback` replans the action on a thing of the same kind, forced, as the original's stroke repeat ([mind.md](../../../creature/mind.md#the-players-feedback)); only for kinds openblack can find (`Gather`), and the associated object (its town, citadel or forest) is not given; no test |
| Stroked while holding food during an action that allows it, it learns to eat what it is given and eats it | done | `LearnFromFeedback` ("I've learnt to eat what I'm given") |
| Stroked for running away from the player, it is forgiven: it stops, wants to run less, and its wish to run needs more to start | done | `LearnFromFeedback` (`k_RunAwayThresholdStep`) |
| Each lesson is put into words as the creature's thought ("I've learnt to ...") | partial 50% | `DesireLessonText`, `ObjectLessonText`, `ActionLessonText` make the lines; the good advisor tells the source and opinion lessons (see [lessons_and_help.md](lessons_and_help.md)); the original's own line per lesson (0x4E1670) is shown only in the debug spawner |
| What it learns shows in the Creature Cave's lists of what it likes and does | partial 50% | what it likes shows on the temple creature room's scroll (`creature_cave::FactsOf`, `src/3D/Implementations/TempleInterior.cpp`); the lessons list (`creature_cave::LessonsOf`, test `CreatureCave.LessonsAreTheStrongestOpinionsEachWay`) only in the debug cave window (`src/Gui/CreatureCaveScreen.cpp`); see [creature_cave.md](creature_cave.md) |

## Before it is old enough to learn

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before the second stage of growing up, feedback teaches no lessons: a stroke makes it more playful, showy and kind, a slap angrier and more fearful | done | `LearnFromFeedback` (`k_MinLessonPhase` = 2), the sources as the original's ([mind.md](../../../creature/mind.md#the-players-feedback)); test `CreatureLearning.FeedbackWithNothingWorthCreditingDoesNothingAndOnlyTheYoungFeelItPlainly` |
| At that stage it may also react at once, one time in two: stroked, it shows how it feels; slapped, it runs from the player one time in three, else is frightened on the spot | partial 80% | `CreatureMindState::afterFeedback`, `DoAfterFeedback`, done once it is free to choose, as the original's `GameRand(2)` and `GameRand(3)` ([mind.md](../../../creature/mind.md#the-players-feedback-in-openblack)); no test |
| The creature passes through the stages where it learns to eat from the hand and to be punished, as the first land's scripts move it on | partial 50% | the stage gates exist and `SET_CREATURE_DEV_STAGE` sets the stage (`ECS/PlayerCreature.cpp`), but a creature's mind starts fully grown (`CreatureMindState::developmentPhase`); see [development_phases.md](development_phases.md) |
| The player is told when a stroke or slap teaches it nothing | done | too young (or an action the tables don't know): the good advisor's Miscellaneous 4 / 5; see [lessons_and_help.md](lessons_and_help.md) (raffclar PR #354; checked against v1.42) |

## Other teachers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the aggression leash, what the player points it at teaches it anger towards such things and less compassion; the compassion leash the reverse | done | `CreatureMindSystem::LearnTurn` (`CreatureMindLearning.cpp`); test `LeashRules.LessonsOfWhatThePlayerShows`; see [leash.md](leash.md) |
| Leashed to another creature, it comes to find it nicer or nastier, and learns befriending or anger towards creatures like it | done | `creature_learning::ChangeHowNice`; tests `CreatureLearning.AttitudesToCreatures`, `LeashRules.LeashedCreaturesWarmOrCoolToEachOther` |
| An action forced on it teaches it, when it forgets it, the species' approval of orders about what it acted on and what it was for; watching alone teaches nothing | done | `CreatureMindSystem::RememberAction`, `creature_learning::ForgottenOrderApproval`, as the original's 0x4E1150 ([mind.md](../../../creature/mind.md#the-players-feedback)), its two vanilla bugs kept; tests `CreatureLearning.TheSixthActionMakesItForgetTheOldest`, `AForgottenForcedActionTeachesApprovalWhenItsDesireLearnsFromThem` |
| Scripts can teach it that a kind of thing is good or bad for a desire | pending | `CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT` is a stub in `src/CHLApi.cpp` |
| A new creature is given its first lesson: an enemy villager is good to eat, while its hunger act-on tree is still empty | done | as the original's starting lesson ([mind.md](../../../creature/mind.md#the-starting-lesson)); tests `CreatureMindModel.TheStartingLessonIsAnEnemyVillagerGoodToEat`, `ANewCreatureFindsEveryThingAsGoodToEatAsTheLessonsStep`, `TheStartingLessonIsOnlyForACreatureThatKnowsNothingAboutWhatToEat` |

## The reward on the status panel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the hand holds a creature, the status panel shows a reward bar below damage, hunger and tiredness | done | `src/Creature/CreatureStatusPanel`, chosen by `creature_panel_choice::Choose` in `Game.cpp` from the held creature, as the original's panel ([fights.md](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)); test `CreatureStatusPanel.LayoutWithTheReward`; see [creature_mode.md](creature_mode.md) |
| The reward bar is half full at nothing, full at the most reward, empty at the most punishment, with words for how much | done | drawn by `GameInterface`; tests `CreatureStatusPanel.RewardWords`, `BarsFillAndColour` |
| After letting go, the bar keeps the last sum until the hand takes hold of a creature again | done | `CreatureHandSystem::GetLastFeedbackSum`, as the original (the value stays until the interaction camera's start zeroes it, [fights.md](../../../creature/fights.md#the-creatures-status-panel-drawn-after-it)); tests in `test/creature/test_creature_hand_system.cpp` |
| While the camera follows a creature the panel shows no reward | done | test `CreatureStatusPanel.LayoutFollowingACreature`; see [creature_mode.md](creature_mode.md) |
