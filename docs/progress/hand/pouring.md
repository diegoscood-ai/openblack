# Holding a miracle and pouring

While the hand holds a miracle's seed it has a state of its own: it is placed like a hand holding something, measures its
own movement for the throw, takes the hold the miracle asks for, and can be lifted and tipped by a timed pour as the
food, wood and water miracles sprinkle from it, or as a creature miracle pours onto a creature. What each miracle does is
in [../miracles/](../miracles/).

**Progress: 18/23 done, 2 partial — 83%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Holding a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a seed, the hand is placed as a holding hand is, under the cursor | done | `HandSystem::Place` (the holding branch, `HandPlacement.cpp`): the hand's model origin is the grip point under the cursor |
| Each frame the hand's velocity is measured from how far it moved, for the throw and the spin | done | The grain state's update in `HandHolding.cpp`, with `magic::FilterHandVelocity` and `magic::StepHandSpin` (`src/Magic/HandMotion.cpp`): smoothed velocity and sideways acceleration, for the throw and the spin; test `HandMotion.TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond` |
| The miracle's hold picks the animation: from above, the idle hand, the horn-shaped hold or from the side | partial | `HandSystem::ComputeHoldParameters` takes the seed info's hold type, but `HandSystem::Update` only plays Chold_above or Chold_side (the horn and grain holds fall to Chold_side; Chorn is not used) |
| A seed not yet ready is held as one not yet ready, until it is | done | `ComputeHoldParameters`: the MAGIC hold until the seed is ready (Cwiggle held at half its length, `HandSystem::Update`) |
| The hand rises by how it holds, measured to the land under it | done | `HandSystem::Place`: ABOVE 0.2, MAGIC 3.2 x the hand's scale, the others max(lowering, 1.9) above the ground point |
| The cursor running ahead sways the hand up to three tenths of a radian | done | `HandSystem::HeldSway` (`HandHolding.cpp`) with `magic::hand_hold::CursorSway` (`src/Magic/HandHoldPose.cpp`): up to 0.3 rad from the smoothed mouse lag, clamped to 80 pixels; test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian` |
| The hand rolls back about the line to the camera | done | `HeldSway` with the grain state's tilt (`hand_grain::Tilt`) about the hand-to-camera axis (`magic::hand_hold::HeldUp`); test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| Upright, the seed takes the hand's turn; for a right hand it is turned half round | partial | The held object takes the hand's matrix; our hand is drawn unmirrored, so the right hand's half turn and a seed's own turn are not ported (hand-and-interface.md, Pending) |
| Taking a seed fades the drawn hand over 0.13 seconds | done | The change to the GRAIN state starts the 0.13 s state blend (`HandCrossFade`, `src/3D/HandCrossFade.h`) |

## Pours of the sprinkling miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pour follows a natural cubic spline through four points, flat at both ends | done | `magic::CubicSpline` through `magic::k_PourKeyPoints` (`src/Magic/HandMotion.cpp`), natural at both ends; tests `FoodWood.grainSplineKeyPoints` (`test/test_food_wood.cpp`), `HandMotion.ANaturalSplinePassesThroughItsPointsWithoutBendingAtTheEnds` |
| The curve rises from rest, swells to almost twice its points' height in the middle and comes back to rest | done | Same spline, peak 1.61 times its points; tests `FoodWood.grainSplineKeyPoints`, `HandMotion.ThePoursCurveIsFlatAtBothEndsAndSwellsToAlmostTwiceItsPoints` |
| Food and wood pour over four seconds, raising the hand by 10 and tipping it by 1.07 radians at the points, over and over | done | `hand_grain::Start` (`magic::StartPour`, `src/Magic/HandMotion.cpp`) from the sprinkle rule (`src/Particles/Rules/Sprinkle.cpp`) with the spell file's values (SF_Food, SF_Wood), looping; test `HandMotion.FoodAndWoodLiftTheHandOverFourSecondsAndStartOver` |
| Food and wood keep the hand where the pour began, so everything lands in one place | done | `hand_grain::ClampedPosition` (`magic::PinnedHand`), used by `HandSystem::Place`; test `HandMotion.APourThatClampsTheHandKeepsItWhereItBegan` |
| Water pours over eight seconds, raising the hand by 8 without tipping it, and the hand stays free to move | done | The same rule with the water spell file's values (ClampHand 0, so the hand follows the cursor) |
| The pour is stepped each game turn and drawn between turns | done | `hand_grain::GameTurnUpdate` (`magic::StepPour`) from `src/Magic/MagicLoop.cpp`; `hand_grain::Height` and `Tilt` interpolate by the turn fraction (`magic::PourPoseAt`) |
| A pour that doesn't loop stops at its end | done | `magic::StepPour` stops it past t = 1 when not looping; tests `FoodWood.grainRaiseLoopsOverTotalTime`, `HandMotion.APourThatDoesntLoopStops` |
| Stopping the pour lets the hand back to rest over the next game turn | done | `hand_grain::Stop` (`magic::StopPour`) zeroes the height and tilt; the drawn values ease to 0 over the next turn by the interpolation (`magic::PourPoseAt`) |
| The pour stops when the miracle closes down | done | `src/Magic/Core/Spell.cpp` calls `hand_grain::Stop` when the spell closes down |
| The pour is drawn in the hand's pose: the lift raises the hand, the tip tips it forward | done | `HandSystem::Place` adds `hand_grain::Height` to the grip, and `HeldSway` the tilt |

## Pours onto a creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature miracle cast on a creature pours onto it once, over four seconds | todo | Only the sprinkle rule starts the grain state; nothing pours onto a creature |
| It raises the hand by 0.4 of the creature's height and tips it by one radian | todo | Not in our tree |
| Only the local player's own hand pours | done | The sprinkle rule starts it only when `IsMyInterfaceCasting` (`src/Particles/Rules/Sprinkle.cpp`) |

## Other players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pours started and ended are sent to the other players so their view of the hand pours too | todo | No multiplayer in our tree |
