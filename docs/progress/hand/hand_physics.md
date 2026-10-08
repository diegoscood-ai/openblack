# Hand physics

How the hand's movement is turned into motion for what it holds: the spring that drags the hand (and what hangs from
it) after the cursor, the speed it lets go with, the hand's strength, and how the hand's motion is measured for
miracles. Pulling anchored things is in [tug.md](tug.md); the flight of thrown things is in [../physics/](../physics/).

openblack: the spring is in `HandPlacement.cpp` (`src/ECS/Systems/Implementations/`), run by
`HandSystem`; the hand's measured motion is `src/Magic/HandMotion.cpp`. Tests: `test/hand/test_hand_motion.cpp`,
`test/test_release_prediction.cpp`.

**Progress: 16/21 done, 4 partial — 86%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## The held thing's spring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Ready to throw, the hand doesn't move rigidly: a spring pulls it after its target, and the held thing hangs from it | done | `HandPlacement.cpp`: the holding spring runs from the second press (IN THROW, `_releaseArmed`) and the hand is drawn at the spring's point |
| The spring pulls with a stiffness of 260 per unit of stretch and is damped by 40 per unit of speed | done | `HandPlacement.cpp`: v += (260 d - 40 v) x 0.01 each step |
| It is stepped in fixed steps of 10 ms of game time, as many as the clock allows and at least one a frame, counting all the time held | done | `HandPlacement.cpp`: 10 ms steps, at least one a frame, until they catch up with the holding clock (which counts every holding frame's game ms, spring or not) |
| Its speed is capped at 124 units a second | done | `HandPlacement.cpp`: the speed is capped at 124 |
| The spring takes hold only once the hand is ready to throw, starting on its target with no speed the frame after; a refused release turns it off again | done | `HandPlacement.cpp`: the spring starts with zero velocity from the hand's last point at the second press; a refused release turns it off (`HandTurn.cpp`) |
| Things that follow the hand directly, such as a miracle's seed, skip the spring | done | `HandPlacement.cpp`: no spring for a spell seed (the grain state, `HandGrain.cpp`) |
| The hand is drawn where the spring puts it, lifted by what it holds | done | `HandPlacement.cpp`: the hand's Transform is the spring's point, at the grip height of the hold type; see [holding.md](holding.md) |

## Letting go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The spring's velocity at the moment of letting go is the throw's velocity | done | `HandSystem::SendRelease(_handVelocity)`: the spring's velocity; `physics::from_hand` throws when its ground speed squared is over 4 |
| A thrown thing leaves without spin; 180 ms later, whatever was let go and still has a body gets a one-turn twist of 1.6 × mass × speed about the level axis across the hand's motion | done | `HandTurn.cpp`: 180 ms after a release the release impulse packet; `physics::from_hand` adds the torque (1.6 x mass x speed) about the level axis across the hand's displacement (`src/ECS/Physics/FromHand.cpp`) |
| The point a fifth of a second of velocity ahead of the held thing is worked out but not used: the throw starts from the held pose | partial | The release prediction runs (`PredictRelease`, tests `ReleasePrediction.*` in `test/test_release_prediction.cpp`), but setting the object from the release pose is pending (`FromHand.h`) |
| The thing's turn as the hand held it is kept for its flight | partial | The thrown body starts from the held object's Transform; the release pose from the hand's angles is pending (`FromHand.h`) |
| The hand's velocity, spin, position and angles are what other players receive to replay the throw | partial | The throw packet carries the velocity, the angular momentum, the position and the angles (`HandTurn.cpp`), applied at the next turn; there are no other players |

## Strength

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's greatest pulling force is 600000 times one more than its strength, and nothing ever sets the strength above 0 | todo | Not ported: our tug is approximate, a tree comes out once the hand has moved weight / 1000 sideways (`HandTrees.cpp`) |
| The hand's size is kept between 0.05 and 2 times its standard size | done | Our hand has no size factor either (the hand's size is 1), so the clamp never matters |
| How much the hand can pull free depends on the thing's weight against that force | partial | `HandTrees.cpp`: the weight decides how far the hand must move before the tree comes out (approximate, not the game's force); see [tug.md](tug.md) |

## The hand's measured motion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's movement over each frame is smoothed into a velocity | done | `HandHolding.cpp`: the grain state (a spell seed in the hand) smooths the hand's per-frame velocity with k = -10 ln(0.2) (`magic::FilterHandVelocity`, `src/Magic/HandMotion.cpp`); test `HandMotion.TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond` |
| A miracle thrown from the hand (a fireball) leaves with that velocity | done | The grain state's velocity is the hand velocity the seed's cast and release take (`HandHolding.cpp`, `HandSpellSeed.cpp`); see [../miracles/](../miracles/) |
| Spinning the hand while holding a miracle gives it a spin from the hand's sideways acceleration | done | `magic::StepHandSpin` (`src/Magic/HandMotion.cpp`) from `HandHolding.cpp`: the angular velocity (0, -(a_s / v_s), 0) from the smoothed sideways acceleration |
| Below a small speed the hand is taken as still | done | `magic::StepHandSpin`: no angular velocity at a speed of 0.0001 or less (`magic::k_HandStill`) |
| A spring-smoothed copy of the cursor trails it, which leans the hand and tilts what it holds | done | `HandSystem::Update`: the smoothed mouse (clamped to 80 px) leans the hand, and `HandSystem::HeldSway` tilts what it holds, up to 0.3 rad (`magic::hand_hold::CursorSway`) |
| Fast hand movement blows smoke and bends trees | done | `chimney_smoke::UpdateHandWind` (`src/ECS/ChimneySmoke.cpp`); trees bend from what the hand carries (`UpdateTreeBends`, `src/ECS/Trees.cpp`). Our wiki differs: the trees bend away from what the hand carries, not from the hand's own movement ([page](../../bw1-notes/trees.md#drawing)) |
