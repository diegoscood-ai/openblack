# Learning from feedback

The player teaches the creature with the hand: holding the action button on it puts the hand on its body, resting the hand
there strokes it and sweeping the hand across it slaps it. When the hand lets go, the creature credits the reward or
punishment to what it did lately and learns from it: how much to want the desire behind the action, which kinds of thing
to do it to and use, and what it thinks of the action itself.

**Progress: 49/77 done, 17 partial — 75%**

How the original does it, in our wiki: [The creature: groundwork, random streams and what is unknown](../../bw1-notes/creature.md).

## Taking hold of the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can never pick a creature up; holding the action button on one puts the hand on it instead | done | `CreatureHandSystem::Grab` (`src/ECS/Systems/Implementations/CreatureHandSystem.cpp`), from the hand's press in `HandTurn.cpp`; tests `CreatureHandSystemTest.*` |
| A quick click on the player's own creature, before any stroke or slap, puts the leash on instead of holding it | done | let go within 450 ms of camera time is a click (`creature_hand::IsClick`, `src/Creature/CreatureHandRules.h`), sent as the leash key (`ECS/CreatureHandPackets.cpp`); tests `CreatureHand.LetGoWithinAClicksCameraTimeIsAClick`, `CreatureHandPacketsTest.TheClickIsThePlayersLeashKey`; see [leash.md](leash.md) |
| Any player's creature can be held, stroked and slapped, other gods' too; a creature of nobody cannot | done | `creature_hand::MayHold` refuses nobody's creature, and `creature_hand::TakesPress` takes only the pressing player's own (no alliances in openblack); tests `CreatureHand.ACreatureOfAnyPlayerLetsTheHandHoldIt`, `CreatureHand.OnlyTheOwnersHandTakesHoldOnAPress`. Our wiki differs: the hand holds only its own or an allied player's creature, not other gods' ([hand-and-interface.md](../../bw1-notes/hand-and-interface.md#the-hand-on-a-creature)) |
| A sleeping creature can't be held | done | `creature_hand::MayHold`; test `CreatureHandSystemTest.OnlyAPlayersCreatureMayBeHeld` |
| A creature frozen by the freeze spell can't be held | done | `creature_hand::MayHold`, frozen from `components::CreatureSpells` in `CreatureHandSystem` |
| Ogres can never be held | done | `creature_hand::MayHold`; test `CreatureHand.ACreatureOfAnyPlayerLetsTheHandHoldIt` |
| The hand's tooltip for interacting shows only over the player's own awake creature | partial | `creature_hand::ShowsInteractTip`, used by `HandToolTips.cpp`; ours also shows it over another player's creature in the influence and does not check it is awake (test `CreatureHand.TheInteractTipIsForTheOwnCreatureOrAnotherInTheInfluence`) |
| While held, the hand is drawn on the body where the cursor meets it, or beside the creature when the cursor is off it | done | `CreatureHandSystem::Update` gives the hand's pose (on the body or on the upright plane through the creature), used by `HandSystem` |
| The hand finds the point it touches by testing the creature's posed mesh each frame | partial | openblack tests capsules round the posed bones (`creature_feedback::BodyCapsules`, `RayHit`), not the mesh; tests `CreatureFeedback.TheHandTouchesTheBodyWhereTheLineOfSightMeetsIt`, `TheBodyIsACapsuleFromEachJointToItsParentsPlaced` |
| The hand and camera close in on the creature by its size while it is held | todo | not modelled (hand pose and camera belong to [../hand/](../hand/)) |
| A clicked creature belonging to another player still answers the leash's refusal | partial | `LeashSystem::TapCreature` exists but nothing calls it |
| Letting go of another creature to hold a new one delivers the first's feedback first | done | `CreatureHandSystem` lets go of a held creature before taking another |

## Stroking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand must rest on the body for a second before the first stroke | done | `creature_feedback::k_StrokeHoldMs`; test `CreatureFeedback.StrokesNeedANewPartAndTime` |
| A stroke lands on the nearest of nine body parts: head, both armpits, belly, groin, both feet, both hands | done | `creature_feedback::NearestPart`; test `CreatureFeedback.AStrokeLandsOnTheNearestPart` |
| Each part has its own pleased animation; the left side plays the right side's mirrored | done | `k_RewardAnimations`, `k_RewardMirrored` in `src/Creature/CreatureFeedback.h`, played by `CreatureHandSystem` |
| The creature pulls a face for two seconds: an "aah" for the head and belly, an "ooh" for the groin, a smile elsewhere | done | `creature_feedback::RewardFace`, pulled by `CreatureHandSystem` |
| A new stroke needs a different part from the last and two seconds since it | done | `creature_feedback::StrokeDue`; test `CreatureFeedback.StrokesNeedANewPartAndTime` |
| A stroke only plays when the body is idle or more than four fifths through what it is doing | done | `k_StrokeInterruptsAfter`, `CreatureMindSystem::ForceAction` |
| Each stroke that plays adds a tenth to the reward, up to the most | done | `creature_feedback::AfterStroke`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| The stroke sounds as the pleased animation plays | partial | sounds come from the animation's sound events (`src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`); which events the pleased animations carry is not checked (unconfirmed) |

## Slapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand slaps when it sweeps across the creature faster than five of its heights a second, at most once a second | done | `k_SlapSpeed`, `k_SlapIntervalMs`; tests `CreatureFeedback.SlapsAreClassedByHeightAndSpeed`, `CreatureHandSystemTest.Resting_TheHandStrokes_SweptFastItSlaps` |
| Where it lands goes by the hand's height: the feet below two fifths of the creature's height, the waist below seven tenths, the head above, nothing above eleven tenths | done | `creature_feedback::ClassifySlap`; test `CreatureFeedback.SlapsAreClassedByHeightAndSpeed` |
| Slower than nine heights a second the slap is gentle, with its own gentler animations | done | `k_HardSlapSpeed`, `k_GentleOffset` in `src/Creature/CreatureFeedback` |
| The slap plays to the left or right by which way the hand swept across the screen | done | `Slap::mirrored` |
| A slap only plays when the body is idle or more than about a third through what it is doing | done | `k_SlapInterruptsAfter` |
| A gentle slap takes a tenth off the reward, a hard one a fifth, twice that when the creature was enjoying being stroked (past a quarter); the reward never goes below -1 | done | `creature_feedback::AfterSlap`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| A force-feedback mouse jolts with each slap, harder for a hard one | todo | no force feedback in openblack |
| The slap sounds as it lands | partial | through the animation's sound events (`CreatureAudioSystem`); not checked against the original (unconfirmed) |

## Delivering the feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the hand lets go, the creature's mind is told the reward, from -1 to 1 | done | `CreatureHandSystem::Release`, through `ECS/CreatureHandPackets.cpp` at the next turn, to `CreatureMindSystem::ReceiveFeedback`; tests `CreatureHandSystemTest.LettingGoGivesHowTheCreatureWasTreated`, `TheFeedbackGivenIsClampedToOne`, `CreatureHandPacketsTest.TheFeedbackReachesTheCreaturesMind` |
| Feedback too slight to count (a hundredth or less) only makes the creature turn to the player and show the interaction tip | partial | `ReceiveFeedback` turns it to face the camera; the original plans its "look at the player" action and shows a tooltip |
| Any feedback stops what the creature was doing | partial | `ReceiveFeedback` stops it only when slapped; the original finishes the current action on a stroke too |
| Its attitude to the player moves: seven tenths of what it was plus a twentieth of the feedback | done | `creature_feedback::AttitudeAfter`, in `ReceiveFeedback` |
| A running average of the feedback it gets, the newest counting most | done | `creature_feedback::AverageAfter`, in `ReceiveFeedback` |
| A stroke holds back its sadness for a minute and a half | done | `k_StrokeSadnessSeconds` in `CreatureMindLearning.cpp` |
| Feedback resets how long it has gone without the player's attention | done | `ReceiveFeedback` resets `secondsAlone` |
| Next it shows the player its pleasure (stroked) or sorrow (slapped) | done | `ReceiveFeedback` clears the show-desire wait; `src/Creature/CreatureIdleMind.cpp` |
| A stroke tells the creature its god wants compassion, a slap anger | todo | no perceived desires in our tree |
| Feedback in a networked game reaches the creature on every machine | todo | no network play; see [../multiplayer/](../multiplayer/) |

## Crediting what it did

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Feedback goes to the most recent thing it can learn from: fully while still doing it, less the longer ago it finished, nothing once the action's learning window has closed | done | `creature_learning::LearningPriority`, `BestContext` (`src/Creature/CreatureLearning.cpp`), used in `CreatureMindLearning.cpp`; test `CreatureLearning.CreditGoesToRecentActions` |
| Each action has its own learning window from the game's tables (eating 20 seconds, stomping 25, fighting 30, examining 5 to 10) | done | `src/Creature/CreatureMindTables.cpp` |
| Actions the tables mark as not learnable get no credit | done | `Context::learnable` |
| It remembers its last few actions to credit | done | `creature_learning::Remember`, `k_MaxContexts` (the original's stack size is unconfirmed) |
| Every action it can do is remembered for credit | partial | only the actions openblack carries out are remembered (`src/Creature/CreaturePlanActions.cpp`); building, dancing, fishing, casting, fighting by choice and others can't be rewarded |
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
| Which kinds of thing to use, when it used something other than what it acted on; for poo at most half as strongly | done | `k_PooUseFeedback` in `CreatureMindLearning.cpp` |
| Its opinion of the action moves four fifths of the way to the feedback | done | `creature_learning::OpinionAfter`; test `CreatureLearning.OpinionsAndFeedbackStrength` |
| Its decision trees are grown again from scratch after every new example, keeping at most sixteen examples each | partial | `src/Creature/CreatureDecisionTree` (`k_MaxEpisodes` 16); openblack keeps the newest sixteen, while the original may drop the newest when full (unconfirmed) |
| Slapped for what it is doing, it gives it up and wants that desire least of all, and every held-down desire is let go | done | `creature_learning::MakeLeastDominant`, `CreatureMindSystem::LearnFromFeedback` |
| Stroked for something it did to a thing, it does it again to something like it nearby | partial | `LearnFromFeedback` replans the action on a thing of the same kind; only for kinds openblack can find (`Gather`) |
| Stroked while holding food during an action that allows it, it learns to eat what it is given and eats it | done | `LearnFromFeedback` ("I've learnt to eat what I'm given") |
| Stroked for running away from the player, it is forgiven: it stops, wants to run less, and its wish to run needs more to start | done | `LearnFromFeedback` (`k_RunAwayThresholdStep`) |
| Each lesson is put into words as the creature's thought ("I've learnt to ...") | partial | `DesireLessonText`, `ObjectLessonText`, `ActionLessonText`; shown only in the debug spawner, not to the player (see [lessons_and_help.md](lessons_and_help.md)) |
| What it learns shows in the Creature Cave's lists of what it likes and does | partial | what it likes shows on the temple creature room's scroll (`creature_cave::FactsOf`, `src/3D/Implementations/TempleInterior.cpp`); the lessons list (`creature_cave::LessonsOf`) only in the debug cave window (`src/Gui/CreatureCaveScreen.cpp`); see [creature_cave.md](creature_cave.md) |

## Before it is old enough to learn

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before the second stage of growing up, feedback teaches no lessons: a stroke makes it more playful, showy and kind, a slap angrier and more fearful | done | `LearnFromFeedback` (`k_MinLessonPhase` = 2) |
| At that stage it may also react happily or sadly at once, one time in two | todo | not modelled |
| A slap marks it as punished, which its reactions follow | todo | not modelled (unconfirmed what it changes) |
| The creature passes through the stages where it learns to eat from the hand and to be punished, as the first land's scripts move it on | partial | the stage gates exist and `SET_CREATURE_DEV_STAGE` sets the stage (`ECS/PlayerCreature.cpp`), but our creatures start fully grown (`CreatureMindState::developmentPhase`); see [development_phases.md](development_phases.md) |
| The player is told when a stroke or slap teaches it nothing | todo | see [lessons_and_help.md](lessons_and_help.md) |

## Other teachers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the aggression leash, what the player points it at teaches it anger towards such things and less compassion; the compassion leash the reverse | done | `CreatureMindSystem::LearnTurn` (`CreatureMindLearning.cpp`); test `LeashRules.LessonsOfWhatThePlayerShows`; see [leash.md](leash.md) |
| Leashed to another creature, it comes to find it nicer or nastier, and learns befriending or anger towards creatures like it | done | `creature_learning::ChangeHowNice`; tests `CreatureLearning.AttitudesToCreatures`, `LeashRules.LeashedCreaturesWarmOrCoolToEachOther` |
| Seen to help, an action teaches it a little about what it acted on and used | todo | not modelled |
| Doing something while the player watches and doesn't punish it counts as approval | todo | not modelled (exact rule unconfirmed) |
| Scripts can teach it that a kind of thing is good or bad for a desire | todo | `CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT` is a stub in `src/CHLApi.cpp` |
| A new creature is given its first lessons, such as that villagers are good to eat | todo | not modelled |
| Stroke and slap buttons in the debug spawner and the testbed deliver feedback directly | done | `src/Debug/CreatureSpawnerMind.cpp` (openblack only; no testbed in our tree; see [../debug/](../debug/)) |

## The reward on the status panel

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the hand is over or holding a creature, the status panel shows a reward bar below damage, hunger and tiredness | partial | dormant: `src/Creature/CreatureStatusPanel` lays it out (test `CreatureStatusPanel.LayoutWithTheReward`) but nothing draws the panel |
| The reward bar is half full at nothing, full at the most reward, empty at the most punishment, with words for how much | partial | tests `CreatureStatusPanel.RewardWords`, `BarsFillAndColour`; nothing draws the panel |
| After letting go, the bar keeps the last sum until the hand takes hold of a creature again | partial | `CreatureHandSystem::GetLastFeedbackSum` keeps it; nothing draws the panel |
| While the camera follows a creature the panel shows no reward | partial | test `CreatureStatusPanel.LayoutFollowingACreature`; nothing draws the panel; see [creature_mode.md](creature_mode.md) |
