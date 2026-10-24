# Holding a miracle and pouring

While the hand holds a miracle's seed it has a state of its own: it is placed like a hand holding something, measures its
own movement for the throw, takes the hold the miracle asks for, and can be lifted and tipped by a timed pour as the
food, wood and water miracles sprinkle from it, or as a creature miracle pours onto a creature. What each miracle does is
in [../miracles/](../miracles/).

**Progress: 16/23 done, 0 chosen, 4 partial, 0 inferred — 82%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/animation-and-placement.md).

## Holding a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding a seed, the hand is placed as a holding hand is, under the cursor | done | `HandSystem::Place` (the holding branch, `HandPlacement.cpp`): the hand's model origin is the grip point under the cursor |
| Each frame the hand's velocity is measured from how far it moved, for the throw and the spin | partial 90% | The grain state's update in `HandHolding.cpp`, with `magic::FilterHandVelocity` and `magic::StepHandSpin` (`src/Magic/HandMotion.cpp`): smoothed velocity and sideways acceleration, for the throw and the spin; test `HandMotion.TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond`. Missing: the original rounds its two smoothing rates to float once, ours casts the double at the end, so the last bits may differ (grab-hold-and-throw.md, marked approximate) |
| The miracle's hold picks the animation: from above, the idle hand, the horn-shaped hold or from the side | partial 75% | `HandSystem::ComputeHoldParameters` takes the seed info's hold type; `HandSystem::Update` plays Chold_above for ABOVE, Cwiggle at half its length for MAGIC and Chold_side for SIDE. Missing: the grain (horn-shaped) hold falls to Chold_side and Chorn is not used; which frame the original's holding state writes for that hold is not read (the wiki's holding column has no entry for it) |
| A seed not yet ready is held as one not yet ready, until it is | done | `ComputeHoldParameters`: the MAGIC hold until the seed is ready (Cwiggle held at half its length, `HandSystem::Update`) |
| The hand rises by how it holds, measured to the land under it | done | `HandSystem::Place`: ABOVE 0.2, MAGIC 3.2 x the hand's scale, the others max(lowering, 1.9) above the ground point |
| The cursor running ahead sways the hand up to three tenths of a radian | partial 85% | `HandSystem::HeldSway` (`HandHolding.cpp`) with `magic::hand_hold::CursorSway` (`src/Magic/HandHoldPose.cpp`): up to 0.3 rad from the smoothed mouse lag, clamped to 80 pixels; test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian`. Missing: the sway is taken about the grip, which stands for the original's hand point (grab-hold-and-throw.md, marked approximate) |
| The hand rolls back about the line to the camera | done | `HeldSway` with the grain state's tilt (`hand_grain::Tilt`) about the hand-to-camera axis (`magic::hand_hold::HeldUp`); test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| Upright, the seed takes the hand's turn; for a right hand it is turned half round | partial 40% | The held object takes the hand's matrix. Missing: the right hand's half turn of the held object and a seed's own turn from its info (issue #28; the mirrored hand itself is drawn, see states_overview.md) |
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
| A creature miracle cast on a creature pours onto it once, over four seconds | pending | Only the sprinkle rule starts the grain state; nothing pours onto a creature. The original's creature spell starts this pour from its process, for the local player's own casting (checked in the exe's callers of the grain start) |
| It raises the hand by 0.4 of the creature's height and tips it by one radian | pending | Not in our tree; the original's creature spell reads the creature's height and 0.4 when it starts the pour |
| Only the local player's own hand pours | done | The sprinkle rule starts it only when `IsMyInterfaceCasting` (`src/Particles/Rules/Sprinkle.cpp`) |

## Other players

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Pours started and ended are sent to the other players so their view of the hand pours too | pending | No multiplayer in our tree |
