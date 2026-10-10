# Holding

How the hand carries what it has picked up: where the thing hangs, how the hand lifts to make room for it, how it sways
and turns with the mouse, and what happens when the thing goes away or the hand meets the creature. How it moves is
in [hand_physics.md](hand_physics.md).

openblack: `HandSystem` holds what the hand picks up (`HandHolding.cpp`, `HandPlacement.cpp` in
`src/ECS/Systems/Implementations/`), hanging and posing it by the game's hold rules (`magic::hand_hold::HoldTypeOf`,
`HoldLift`, `src/Magic/HandHoldPose.cpp`), the same rules the miracle seeds use (`magic::hand_hold::SeedHang`). Tests:
`test/test_hand_hold_pose.cpp`.

**Progress: 20/28 done, 1 partial — 73%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Where the held thing hangs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each thing says how it is held: on the palm (most objects), the magic grip (miracle seeds), from the side (pots, piles, mobile objects, some statics), the tree grip or the villager grip (people and animals) | done | `HandSystem::ComputeHoldParameters` (`HandHolding.cpp`): ABOVE by default, MAGIC for a seed until ready, then its info's hold (`magic::hand_hold::HoldTypeOf`, `src/Magic/HandHoldPose.cpp`), SIDE for mobile objects, pots and some statics, TREE for trees, VILLAGER for villagers and animals; test `HandHoldPose.ASeedIsHeldAsAMiracleNotYetReadyUntilItIsReady` |
| Held on the palm, the thing hangs 0.2 units under the hand | done | `magic::hand_hold::HoldLift` (`src/Magic/HandHoldPose.cpp`) from `HandPlacement.cpp`: ABOVE hangs 0.2; test `HandHoldPose.TheHandRisesByHowItHolds` |
| Held in the magic grip (a miracle's seed), it hangs 3.2 times the hand's size under it | done | `magic::hand_hold::HoldLift`: MAGIC hangs 3.2 x the hand's scale; test `HandHoldPose.TheHandRisesByHowItHolds` |
| Held from the side or in the tree or villager grip, it hangs its own lowering times its height under the hand, at least 1.9 units | done | `magic::hand_hold::SeedHang` and `HoldLift` (`src/Magic/HandHoldPose.cpp`) from `HandPlacement.cpp`: max(lowering x height, 1.9) for the SIDE, TREE and VILLAGER holds; test `HandHoldPose.TheHandRisesByHowItHolds` |
| A standing tree hangs a further tenth of its height lower | done | `HandPlacement.cpp`: plus 0.1 x height for a rooted object (a tree) |
| The hand rises by the whole of that hanging distance (as measured the frame before); the cursor's point on the land is raised by 0.6 of it | done | `HandPlacement.cpp`: the hand's origin is the grip point; the cursor's search ray aims at the land point + 0.6 x the grip height |
| The held thing is drawn at its own size, and the hand's pose opens by how big it is next to the hand (up to the hand's full span) | done | `HandSystem::Update`: `Chold_above` / `Chold_side` at a frame set by the hold radius against 3.2 x the hand's scale |
| Each hold plays its own still pose: palm, magic, side, tree or villager | done | `HandSystem::Update`: `Chold_above` for ABOVE, `Cwiggle` at half for MAGIC, `Chold_side` for SIDE, TREE and VILLAGER, as our wiki gives them |
| Every game turn the hold is asked again of the thing, so a thing that changes (a pile growing) changes grip | done | `HandSystem::GameTurnUpdate` (`HandTurn.cpp`) asks the hold type again each turn (the radius, lowering and height stay the pick-up's, approximate); a food pot's radius follows its amount every frame |
| A held handful is drawn as the in-hand pile, sized by its amount | partial | The hand's pots keep scale 1 (`PotArchetype::SetSize`); the hand opens as the food in it grows (`ComputeHoldParameters`) |
| Held wood and branches use their own in-hand meshes | done | The hand's wood pot is drawn with its own pot info's mesh (`PotArchetype`) |

## Swaying and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held thing tilts as the cursor runs ahead of the hand: up to 0.3 radians each way, at 80 pixels of lag | done | `HandSystem::HeldSway` (`HandHolding.cpp`) with `magic::hand_hold::CursorSway` and `HeldUp` (`src/Magic/HandHoldPose.cpp`): tiltX and tiltY = 0.3 / 80 x the clamped lag, about the camera line and its level side axis; test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian` |
| A thing held on the palm, or from the side, turns half round (or a quarter round) over 0.4 seconds to face the creature it is offered to | todo | Giving to the creature is not ported |
| A right hand turns it the other way from a left hand | todo | Our hand is not mirrored and has no left-handed mode |
| The held thing faces the camera, rolled about the line to it | done | `HandSystem::HeldSway`: the roll about the line to the camera (`magic::hand_hold::HeldUp`); the held object's matrix from the heading (`magic::hand_hold::HeldBasis`); test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |

## Giving to the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a creature that would take what the hand holds, the hand offers it for 1.1 seconds after leaving it | todo | Not ported (giving to the creature); see [../creature/](../creature/) |
| While the creature reaches for it, the hand is drawn to the creature's hand over half a second | todo | Not ported |
| Moving the hand more than 15 times the creature's size away calls the giving off | todo | Not ported |
| The creature takes what is held, then looks at it, eats it, plays with it or throws it as it likes | todo | Not ported; see [../creature/](../creature/) |
| Things the creature took from the hand teach it the player wanted it to have them | todo | unconfirmed: giving alone teaches nothing in the code traced; see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |

## Losing what is held

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every turn, a held thing that no longer exists, or is no longer in the hand's care, is dropped from it | done | `HandSystem::ValidateHands` (`HandTurn.cpp`): a held object no longer interactable leaves the hand with no physics; `GameTurnUpdate` lets go of one no longer available |
| A held thing that is destroyed leaves the hand, which empties and stops holding | done | `HandSystem::Update`: a destroyed held object leaves the hand at once; `ValidateHands` for one marked deleted |
| Each turn the held thing is kept at the hand's position on the map, so it stays in the right place for the game | done | `HandTurn.cpp`: each turn the held object is placed at the synced hand's map coordinates (the altitude above the land kept as a float) |
| A held miracle that runs out leaves the hand | done | A seed marked for deletion is no longer interactable: `ValidateHands` takes it out (`SeedLeftHand`) |
| A scribble shakes the held thing out of the hand | done | `gestures` (`src/Magic/Gestures/PowerUpSystem.cpp`) calls the hand's `ForceDropHeld` on a scribble: a drop with no speed, applied at the next turn (`HandSpellSeed.cpp`); see [../gesture/scribble.md](../gesture/scribble.md) |
| People react to a burning thing held near them (reach 30, running 20–50 m) | done | `fire::StartedMoving` when the object enters the hand (`HandHolding.cpp`); a held burning object spreads its fire only inside the holder's influence (`HandTurn.cpp`); see [../physics/fire.md](../physics/fire.md) |
| A held thing carried through trees bends them | done | `UpdateTreeBends` (`src/ECS/Trees.cpp`) takes the object the hand carries as a source |
| The camera can still be moved while holding something | done | The camera's land grip (`DefaultWorldCameraModel`) does not depend on what the hand holds; the hand state is 20 whatever it holds |
