# Creature contact

The hand's side of touching a creature: taking hold of it, stroking and slapping it, how the hand rests on and turns to
the body, and what the creature is told when the hand lets go. What the creature learns from it is in
[../creature/](../creature/). Giving the creature something to hold or eat is in
[holding.md](holding.md).

**Progress: 24/46 done, 7 partial — 60%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Action button over a creature takes hold of it | done | `hand_press::Branch::Creature` -> `SendStartLockedSelect`; the turn locks it (`HandTurn.cpp`, `CreatureHandSystem::Grab`), then `HandSystem::UpdateCreatureLock` / `UpdateCreatureFrame`; tests `HandCreatureLock.*`, `HandCreatureFrame.*` (`test/test_hand_creature.cpp`) |
| Any player's creature can be held, not only the player's own, unless it is asleep or frozen; never an ogre | done | Only the player's own creature (`creature_hand::TakesPress`, `IsFriendlyTo`), not asleep or frozen, never an ogre (`creature_hand::MayHold`); test `CreatureHandSystemTest.OnlyAPlayersCreatureMayBeHeld`. Our wiki differs: the original holds only its own or an allied player's creature, not any god's ([page](../../bw1-notes/hand-and-interface.md#the-hand-on-a-creature)) |
| A press let go quickly, before stroking or slapping, is a click, which puts the leash on instead | done | A let-go within 450 ms of camera time on the own creature sends the click packet, which presses the leash key (`creature_hand::IsClick`, `hand_creature::StepFrame`, `CreatureHandPackets.cpp`); test `HandCreatureFrame.ALetGoWithinAClickOfTheOwnCreatureSendsTheClickThenTheFeedback` |
| The nearest creature along the line of sight is the one taken hold of | done | The cursor's pick takes creatures by capsules round their posed bones (`CreatureHandSystem::CreatureAlong`, called from `HandPlacement.cpp`), the nearest wins; test `CreatureHandSystemTest.TheNearestCreatureAlongASightAndHowFar` |
| Holding a creature can't be done while the hand holds a miracle | done | The creature branch needs an empty hand (`hand_press::EmptyHandOverNothing`), so a seed in the hand blocks it |
| Scripts and the testbed can put the hand on a creature and stroke or slap it | partial | `CreatureHandSystem::Grab` is public on `CreatureHandSystemInterface`, but only the hand's locked select calls it; no script command uses it |
| Taking hold starts the hand facing back towards the camera and settles it over a second | todo | The hand is placed on the body at once (`CreatureHandSystem::Update`) |
| Taking hold starts the hand with its stroking pose and a clean slate: no part stroked yet, no slap yet | done | `CreatureHandSystem::Grab` makes a fresh `HandOnCreature` (no part stroked, no slap, sum 0); the hand's clip stays the normal one (Cstroke) |
| Holding a creature zooms the camera in on it | todo | The interaction camera is not ported (`UpdateCreatureFrame`: "(not ported) the interaction camera") |
| Letting go stops any force-feedback effect of the contact | todo | No force feedback |

## Where the hand rests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand meets the body where the line of sight through the cursor meets the creature's mesh | partial | Capsules round the posed bones (`creature_feedback::RayHit`), not the drawn mesh; test `CreatureFeedback.TheHandTouchesTheBodyWhereTheLineOfSightMeetsIt` |
| Missing the body, the hand tries again along the line through the middle of the cursor's last two places | todo | Missing the body, the hand goes to the upright plane through the creature instead (`CreatureHandSystem::Update`) |
| The hand is held a little short of the body, by about a twentieth of the creature's height, easing there over 0.3 seconds | partial | `CreatureHandSystem::Update` puts the hand at the touch point, which `HandPlacement.cpp` uses in the creature state; no gap and no ease |
| Coming nearer than the body allows, it snaps to the nearer place at once rather than easing | todo | Not ported |
| Away from the body, the hand keeps back from the camera by a quarter of the creature's height | todo | Away from the body the hand is on the plane through the creature's middle (`OnPlaneThrough`), not kept back |
| The hand is drawn larger with the creature's size, so it suits the creature | todo | The hand keeps its usual size |
| Far from the creature's middle (more than 0.6 of its height), the hand is drawn 0.3 of its height back towards the camera | todo | Not ported |
| On the body, the hand turns to face the body's surface over 0.9 seconds | todo | Not ported |
| Off the body, the hand turns back upright over 2.5 seconds | todo | Not ported |
| The body is felt as capsules from each joint to its parent, as posed | done | `creature_feedback::BodyCapsules` (openblack's stand-in for the mesh); test `CreatureFeedback.TheBodyIsACapsuleFromEachJointToItsParentsPlaced` |

## Stroking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the body the hand plays its tickling cycle as it strokes | partial | Our hand keeps the normal state's clip over a creature (Cstroke), not the tickle cycle; the creature state's own clip is not traced |
| The hand must rest on the body for a second, with the button held, before it strokes | done | `creature_feedback::k_StrokeHoldMs` (1000 ms) in `CreatureHandSystem::Update` |
| A stroke lands on the nearest of nine parts of the body: the head, each armpit, the belly, the groin, each foot and each hand | done | `creature_feedback::NearestPart` with the rig's action points; test `CreatureFeedback.AStrokeLandsOnTheNearestPart` |
| A new stroke needs another part than the last and two seconds since the last | done | `creature_feedback::StrokeDue`; test `CreatureFeedback.StrokesNeedANewPartAndTime` |
| The creature plays the pleased animation of the part stroked, mirrored for the left side | done | `creature_feedback::k_RewardAnimations`, `k_RewardMirrored`, played through `CreatureMindSystem::ForceAction` |
| It pulls a face for two seconds: an aah for the head and belly, an ooh for the groin, a smile elsewhere | done | `creature_feedback::k_RewardFaces`, `RewardFace` (2 s) |
| A stroke doesn't interrupt an action less than 0.8 of the way through | done | `creature_feedback::k_StrokeInterruptsAfter` passed to `ForceAction` |
| Each stroke adds 0.1 to the running sum, at most 1 | done | `creature_feedback::AfterStroke` (`k_StrokeAmount`) |

## Slapping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand moving across the creature faster than five of its heights a second shows the slapping cycle | partial | The slap is classed by speed (`creature_feedback::k_SlapSpeed`) and kept 0.3 s, but nothing reads the pose's slapping flag: the hand does not change its clip |
| A slap needs the hand between the ground and 1.1 of the creature's height | done | `creature_feedback::k_SlapAbove` in `ClassifySlap` |
| Below 0.4 of its height a slap hits the feet, below 0.7 the waist, above that the head | done | `creature_feedback::ClassifySlap`; test `CreatureFeedback.SlapsAreClassedByHeightAndSpeed` |
| Faster than nine heights a second it is a hard slap, slower a gentle one, with the gentle animations | done | `creature_feedback::k_SlapSpeed`, `k_HardSlapSpeed`, `k_GentleOffset` |
| The slap plays to the side the hand swept towards | done | `ClassifySlap` mirrors by the cursor's sweep (`CreatureHandSystem::Update`) |
| Slaps are at least a second apart | done | `creature_feedback::k_SlapIntervalMs` |
| The hand's sweep only counts as a slap as the cursor crosses the middle of the screen | todo | (unconfirmed reading of the game's test) |
| A slap doesn't interrupt an action less than 0.35 of the way through | done | `creature_feedback::k_SlapInterruptsAfter` |
| A gentle slap takes 0.1 off the sum and a hard one 0.2, twice as much if the creature was enjoying itself (sum past 0.25), never below -1 | done | `creature_feedback::AfterSlap`; test `CreatureFeedback.StrokesAndSlapsAddUp` |
| Every third hard slap in a row throws up an effect on the creature, stronger the harder the slap | todo | Not ported |
| With a force-feedback mouse a gentle and a hard slap each have their own jolt | todo | No force feedback |
| In a network game a hard slap knocks the creature's body about | todo | No network game. (unconfirmed) |
| Punching the creature | n/a | the punch cycle is in the hand's animations but the game never plays it |
| Tickling as a separate act | n/a | the game has no tickle apart from stroking; its tickle cycle is what the hand plays while stroking |

## Letting go and after

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go tells the creature's mind how it was treated, from -1 to 1 | done | `HandSystem::LeaveCreatureState` -> `CreatureHandSystem::Release` -> the creature feedback packet, applied at the next turn (`CreatureMindSystem`); test `CreatureHandSystemTest.LettingGoGivesHowTheCreatureWasTreated` |
| Feedback this slight only makes the creature look at the player | done | `creature_feedback::k_SlightFeedback` in `CreatureMindSystem.cpp` |
| The status panel shows the reward until the hand next takes hold | partial | `CreatureHandSystem::GetLastFeedbackSum` keeps it, but the status panel (`src/Creature/CreatureStatusPanel.cpp`) is not wired into the game |
| The creature reacts to each stroke and slap | done | `CreatureMindSystem::ForceAction` plays the pleased and slapped animations; learning in `CreatureMindLearning.cpp` |
| The creature looks at, follows, turns to or runs from the player's hand nearby | todo | Nothing in the creature's code reads the hand's position |
| The creature copies what it sees the hand do to others | partial | `ecs::creature_mimic::Consider` for some deeds (planting a tree from the hand, damaging a building by a throw, casting water on crops); not the rock's tap |
