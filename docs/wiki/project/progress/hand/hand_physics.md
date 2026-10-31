# Hand physics

How the hand's movement is turned into motion for what it holds: the spring that drags the hand (and what hangs from
it) after the cursor, the speed it lets go with, the hand's strength, and how the hand's motion is measured for
miracles. Pulling anchored things is in [tug.md](tug.md); the flight of thrown things is in [../physics/](../physics/).

openblack: the spring's rules are `hand_grab::HandSpring` (`src/Hand/HandGrabRules.h`), stepped from `HandPlacement.cpp`
(`src/ECS/Systems/Implementations/`), run by `HandSystem`; the hand's measured motion is `src/Magic/HandMotion.cpp`.
Tests: `test/hand/test_hand_grab.cpp`, `test/hand/test_hand_motion.cpp`, `test/hand/test_hand_tug.cpp`,
`test/test_release_prediction.cpp`.

**Progress: 18/21 done, 0 chosen, 3 partial, 0 inferred — 95%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/states.md).

## The held thing's spring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ready to throw, the hand doesn't move rigidly: a spring pulls it after its target, and the held thing hangs from it | done | `HandPlacement.cpp`: the holding spring runs from the second press (IN THROW, `_releaseArmed`) and the hand is drawn at the spring's point; tests `HandHoldPose.TheHandTrailsADragOnItsSpringAndThrowsAtTheDragsSpeed`, `HandHoldPose.TheHeldModelHangsBelowTheHandAlongItsUp` |
| The spring pulls with a stiffness of 260 per unit of stretch and is damped by 40 per unit of speed | done | `hand_grab::HandSpring` (`k_SpringStiffness`, `k_SpringDamping`, `src/Hand/HandGrabRules.h`): v += (260 d - 40 v) x 0.01 each step; test `HandGrab.TheSpringSettlesOnItsTargetAndIsCapped` |
| It is stepped in fixed steps of 10 ms of game time, as many as the clock allows and at least one a frame, counting all the time held | done | `hand_grab::HandSpring` from `HandPlacement.cpp`: 10 ms steps, at least one a frame, until they catch up with the holding clock (which counts every holding frame's game ms, spring or not); tests `HandGrab.TheSpringStepsAtLeastOnceAFrame`, `HandGrab.TheSpringTakesAsManyStepsAsTheGameTimeAllows`, `HandGrab.TheSpringCatchesUpWithTheTimeHeldBeforeItTookHold` |
| Its speed is capped at 124 units a second | done | `hand_grab::k_MaxThrowSpeed`: the speed is capped at 124; test `HandGrab.TheSpringSettlesOnItsTargetAndIsCapped` |
| The spring takes hold only once the hand is ready to throw, starting on its target with no speed the frame after; a refused release turns it off again | done | `HandPlacement.cpp`: the spring starts with zero velocity from the hand's last point at the second press; a refused release turns it off (`HandTurn.cpp`); test `HandPlace.TheSpringStartsAtTheSecondPressWithNoSpeedAndAMiraclesSeedSkipsIt` |
| Things that follow the hand directly, such as a miracle's seed, skip the spring | done | `HandPlacement.cpp`: no spring for a spell seed (the grain state, `HandGrain.cpp`); test `HandPlace.TheSpringStartsAtTheSecondPressWithNoSpeedAndAMiraclesSeedSkipsIt` |
| The hand is drawn where the spring puts it, lifted by what it holds | done | `HandPlacement.cpp`: the hand's Transform is the spring's point, at the grip height of the hold type; see [holding.md](holding.md); test `HandHoldPose.TheHandRisesByHowItHolds` |

## Letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The spring's velocity at the moment of letting go is the throw's velocity | done | proven through the real input path (2026-10-31, act 7 of `refs/realinput/hand_pages.log`, `tools/checks/realinput/hand_pages.txt`); `HandSystem::SendRelease(_handVelocity)`: the spring's velocity; `physics::from_hand` throws when its ground speed squared is over 4; tests `HandGrab.AThrowIsFastAcrossTheGround`, `HandHoldPose.TheHandTrailsADragOnItsSpringAndThrowsAtTheDragsSpeed` |
| A thrown thing leaves without spin; 180 ms later, whatever was let go and still has a body gets a one-turn twist of 1.6 × mass × speed about the level axis across the hand's motion | done | `HandTurn.cpp`: 180 ms after a release the release impulse packet; `physics::from_hand` adds the torque (1.6 x mass x speed) about the level axis across the hand's displacement (`src/ECS/Physics/FromHand.cpp`); tests `FromHandSpin.TheTwistIsOnePointSixTimesTheMassAndSpeedAboutTheLevelAxisAcrossTheHandsMotion`, `HandGrab.TheTwistIsCancelledWhenItsCountdownEndsOnNothing` |
| The point a fifth of a second of velocity ahead of the held thing is worked out but not used: the throw starts from the held pose | partial 70% | The throw leaves from where the thing hangs from the hand: `HandSystem::PushThrowData` passes the held object's drawn pose (`magic::hand_hold::ReleasePlace`) to `PredictRelease` (tests `ReleasePrediction.*` in `test/test_release_prediction.cpp`; [where a throw leaves from](../../../hand/grab-hold-and-throw.md)); missing: the point 0.2 s of velocity ahead is not written by our holding state |
| The thing's turn as the hand held it is kept for its flight | partial 60% | The release prediction builds its body at the drawn pose, its rotation included (`PredictRelease` from `HandTurn.cpp`); missing: `from_hand::Throw` does not yet set the object's matrix from the release pose (the hand's angles and position), noted pending in `FromHand.h` |
| The hand's velocity, spin, position and angles are what other players receive to replay the throw | partial 70% | The throw packet carries the velocity, the angular momentum, the position and the angles, as the original's throw packet (`HandTurn.cpp`), applied at the next turn; missing: the prediction's angular velocity and the order of the angles are inferred in the wiki, and there are no other players to replay it |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's greatest pulling force is 600000 times one more than its strength, and nothing ever sets the strength above 0 | done | `hand_grab::MaxForce` (`src/Hand/HandGrabRules.cpp`), called with strength 0 by the tug (`HandTrees.cpp`); test `HandTug.TheConstantsAreTheOriginals` (`test/hand/test_hand_tug.cpp`); [the uproot](../../../hand/grab-hold-and-throw.md) |
| The original has no hand size but its standard one: its hand is given size 1 as it loads and nothing found sets another, so the clamp of its size to 0.05..2 never matters | done | Our hand has no size factor either (the hand's size is 1): its model's scale is 3.2 times the hand scale over the bind height, with nothing else ([the hand's size](../../../hand/grab-hold-and-throw.md)); test `HandHandedness.LeftHandIsTheMeshAsItIs` |
| How much the hand can pull free depends on the thing's weight against that force | done | `hand_grab::PullAt` from `HandTrees.cpp`: a tree comes out once the hand can lift it (greatest force at least 9.81 x its weight) and pulls harder than it weighs; anything but a tree comes free at once; tests `HandTug.ATreeComesFreeOnlyWhenTheHandCanLiftItAndPullsHarderThanItWeighs` and the pull bit for bit in `test/hand/test_hand_grab.cpp`; see [tug.md](tug.md) |

## The hand's measured motion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's movement over each frame is smoothed into a velocity | done | `HandHolding.cpp`: the grain state (a spell seed in the hand) smooths the hand's per-frame velocity with k = -10 ln(0.2) (`magic::FilterHandVelocity`, `src/Magic/HandMotion.cpp`); test `HandMotion.TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond` |
| A miracle thrown from the hand (a fireball) leaves with the hand's velocity | done | The spell takes the hand's turn velocity, synced once a turn (each turn 0.6 of the way to the synced hand's move times the turns in a second), in both games, not the grain state's per-frame one: `HandSystem::GetSpellInfo` (`HandSpellSeed.cpp`, `HandTurn.cpp`); world test `HandInputWorld.AMiracleCastFromTheHandLeavesWithTheHandsTurnVelocity` (`test/hand/test_hand_input_world.cpp`); the fireball's pitch up: `test_fireball_direction`; see [../miracles/](../miracles/) |
| Spinning the hand while holding a miracle gives it a spin from the hand's sideways acceleration | done | `magic::StepHandSpin` (`src/Magic/HandMotion.cpp`) from `HandHolding.cpp`: the angular velocity (0, -(a_s / v_s), 0) from the smoothed sideways acceleration; test `HandMotion.AHeldMiracleSpinsByHowFastTheHandsPathTurnsSideways` |
| Below a small speed the hand is taken as still | done | `magic::StepHandSpin`: no angular velocity at a speed of 0.0001 or less (`magic::k_HandStill`); test `HandMotion.AHeldMiracleSpinsByHowFastTheHandsPathTurnsSideways` |
| A spring-smoothed copy of the cursor trails it, which leans the hand and tilts what it holds | done | `HandSystem::Update`: the smoothed mouse (clamped to 80 px) leans the hand, and `HandSystem::HeldSway` tilts what it holds, up to 0.3 rad (`magic::hand_hold::CursorSway`); test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian` |
| Fast hand movement blows chimney smoke about; trees bend from what the hand carries, never from the hand itself | done | `ChimneySmokeSystem::UpdateHandWind` (`src/ECS/Systems/Implementations/ChimneySmokeSystem.cpp`, test `ChimneySmoke.HandStirsTheSmokeNearby`); trees bend from what the hand carries (`UpdateTreeBends`, `src/ECS/Trees.cpp`, tests `TreeBends.*`), as the original ([page](../../../world/trees.md#drawing)) |
