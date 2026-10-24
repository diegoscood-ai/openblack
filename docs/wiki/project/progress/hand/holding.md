# Holding

How the hand carries what it has picked up: where the thing hangs, how the hand lifts to make room for it, how it sways
and turns with the mouse, and what happens when the thing goes away or the hand meets the creature. How it moves is
in [hand_physics.md](hand_physics.md).

openblack: `HandSystem` holds what the hand picks up (`HandHolding.cpp`, `HandPlacement.cpp` in
`src/ECS/Systems/Implementations/`), hanging and posing it by the game's hold rules (`magic::hand_hold::HoldTypeOf`,
`HoldLift`, `src/Magic/HandHoldPose.cpp`), the same rules the miracle seeds use (`magic::hand_hold::SeedHang`). Tests:
`test/test_hand_hold_pose.cpp`.

**Progress: 24/30 done, 0 chosen, 6 partial, 0 inferred — 91%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/grab-hold-and-throw.md).

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
| Every game turn the hold is asked again of the thing, so a thing that changes (a pile growing) changes grip | partial 70% | `HandSystem::GameTurnUpdate` (`HandTurn.cpp`) asks the hold type again each turn; a food pot's radius follows its amount every frame. Missing: the radius, lowering and height stay the pick-up's (approximate), where the original asks them again |
| A held handful is drawn as the in-hand pile at its own scale, the hand opening as the food in it grows | done | The original keeps the held pile's mesh at scale 1 and opens the hand with the food in it (PileFood's raised proportion, fully open at 1600; [states.md](../../../hand/states.md)): our hand pots keep scale 1 (`PotArchetype::SetSize`) and `ComputeHoldParameters` takes the radius x `object::GetProportionRaised` (`src/ECS/ObjectMetrics.cpp`). Whether the pile's sink shows while held is not read (wiki Pending) |
| Held wood and branches use their own in-hand meshes | done | The hand's wood pot is drawn with its own pot info's mesh (`PotArchetype`) |

## Swaying and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held thing tilts as the cursor runs ahead of the hand: up to 0.3 radians each way, at 80 pixels of lag | done | `HandSystem::HeldSway` (`HandHolding.cpp`) with `magic::hand_hold::CursorSway` and `HeldUp` (`src/Magic/HandHoldPose.cpp`): tiltX and tiltY = 0.3 / 80 x the clamped lag, about the camera line and its level side axis; test `HandHoldPose.TheCursorRunningAheadSwaysTheHandUpToThreeTenthsOfARadian` |
| A thing held on the palm, or from the side, turns half round (or a quarter round) over 0.4 seconds to face the creature it is offered to | partial 60% | `creature_giving::OfferTurn` eased by `HandGiving::turn`, rolled in `HandSystem::HeldSway` (raffclar's giving). The half and quarter turns and the turn back with 300 ms left are on the wiki; the 0.4 s ease is not read against v1.42 |
| A right hand turns it the other way from a left hand | partial 50% | `creature_giving` and `magic::hand_hold::SeedTurn` take the debug menu's Right Handed option (`config.rightHandedHand`); test `HandHoldPose.TheSeedTurnsHalfRoundForARightHandAndByItsOwnTurn`. Missing: the wiki reads the original's hand as right-handed by default (its mirror flag 0), our default is the unmirrored mesh with the option off; which way round is the original's is not settled (issue #28) |
| The held thing faces the camera, rolled about the line to it | done | `HandSystem::HeldSway`: the roll about the line to the camera (`magic::hand_hold::HeldUp`); the held object's matrix from the heading (`magic::hand_hold::HeldBasis`); test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera` |
| A held miracle seed turns by its own hold turn, half round more for a right hand | partial 30% | `magic::hand_hold::SeedTurn` (`src/Magic/HandHoldPose.cpp`) with test `HandHoldPose.TheSeedTurnsHalfRoundForARightHandAndByItsOwnTurn`; not applied to the drawn seed yet (no caller), and the seed info's own turn is not read (issue #28) |
| The held thing's matrix is the original's: the hand's heading and up, a mirror image for either hand | partial 50% | `magic::hand_hold::HeldBasis` from the heading and the held up (`HandHolding.cpp`); test `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera`. Missing: the third row's mirror by the hand's side and its scale ([grab-hold-and-throw.md](../../../hand/grab-hold-and-throw.md#pending), issue #28) |

## Giving to the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Over a creature that would take what the hand holds, the hand offers it for 1.1 seconds after leaving it | done | `creature_giving::Offer` (1100 ms), `KeepOffer`; test `CreatureGivingHand.OverACreatureThatWouldTakeItTheHandOffersAndTurnsItOver`; the 1100 ms offer timer read in the exe (xref note on the hand's required position, giving) |
| While the creature reaches for it, the hand is drawn to the creature's hand over half a second | partial 60% | `creature_giving::DrawToCreature` through `ReachFrame` in `HandSystem::Place` (`GivingTarget`). Missing: the half-second window before the take moment is raffclar's, not read against v1.42 |
| Moving the hand more than 15 times the creature's size away calls the giving off | done | `creature_giving::CallsOff`, packet 0x52 `GiveCallOff`; the 15 body sizes and the call-off packet read in the exe (xref notes on the giving and on the packet) |
| The creature takes what is held, then looks at it, eats it, plays with it or throws it as it likes | done | given at the press itself, as the original (`creature_giving::OnPress`); the take at the action's moment (`HandSystem::HandOver`), then `CreatureMindSystem::UseGift`: examine, eat, or throw at the camera; the give handler, the take and the gift's choice read in the exe ([grab-hold-and-throw.md](../../../hand/grab-hold-and-throw.md)); test `CreaturePlanner.AGiftIsWeighedByHowUsefulTheThingItHoldsIsToActOn` |

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
| A miracle's seed, a worship icon's seed or a caught fireball goes into the hand only while it holds nothing and no creature is locked for it; pulling a tree does not stop it | done | `HandSystem::IsHandReadyForObject` over `hand_creature::IsHandReadyForObject`, the original's two conditions in order (test `HandReadyForObject.SpaceInTheHandsAndNoCreatureLockedIsAllItAsks`); an object offered during a tug is refused by the place in hand (`ApplyPlaceInHand`); the Gestures debug window shows it; see [the hand's readiness](../../../hand/states.md#each-turn) |
