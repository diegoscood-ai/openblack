# Pulling things free

Grabbing a standing tree doesn't lift it at once: the hand pulls at it, the tree leans and stretches towards the hand
like a spring, and it only comes free once the pull is strong enough for its weight. Too heavy for the hand's strength,
it never comes free. Everything else the hand can take comes free as soon as the hand has faded into its pulling pose.

openblack: the pull is in `HandSystem` (`BeginTug`, `UpdateTug`, `HandTrees.cpp`) with the original's rule (the user's
choice, 2026-10-16) as pure functions in `hand_grab` (`TugPlaneAt`, `TugHandOnPlane`, `PullAt`), pinned in
`test/hand/test_hand_tug.cpp`; the uprooted tree leaves its roots hole (`HandSystem::Uproot`). Testbed:
`hand.tug_at_grip`, `hand.tug_at_crown`, `hand.tug_let_go`, `hand.tug_heavy_tree`.

**Progress: 18/21 done, 0 chosen, 3 partial, 0 inferred — 98%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/grab-hold-and-throw.md#tug).

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only standing trees are really pulled; big forests, fields, piles and fireballs wait for the press to become a hold instead, and anything else comes free once the hand's pulling pose has faded in | done | proven through the real input path (2026-10-31, act 4 of `refs/realinput/hand_pages.log`, `tools/checks/realinput/hand_pages.txt`); Trees are tugged from the press (`HandSystem::BeginTug`, `HandTrees.cpp`); a big forest and a magic fireball wait the 225 ms timer (`_pendingTimer`), piles and fields start a locked select, other objects are taken once the 0.13 s pull pose blend is over (`_pendingTugHold` in `HandSystem::Update`); tests `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree`, `HandTug.ThePullWaitsForTheHandsFade` |
| The hand grips the thing a tenth of its height up from its base (at the start no closer than 0.3 of the hand's height, which no pull sees) | done | `hand_grab::HoldDistance` from `BeginTug`, gripped again at the hold lowering times the height every frame (`UpdateTug`); tests `HandTug.TheGripsLeastHangIsTheHandsHeightTimesItsSizeThenTheShare`, `HandTug.TheGripIsUpTheUnitAxisAsTheGameMakesItUnit` |
| The slope of the land under the thing is taken as the plane the pull works in, set at a height found from the camera's ground distances to the base and to the hand | partial 90% | `hand_grab::TugPlaneAt` from `HandSystem::BeginTug` (`HandTrees.cpp`); tests `HandTug.ThePlaneIsAtTheHandsHeightSeenAtTheBase`, `AHandOnTheLineThroughTheBaseMakesAPlaneThroughTheBase`. Missing: it is taken at the press frame, one frame before the original's pulling state starts (wiki: approximate) |
| The pull only starts once the hand's 0.13-second fade into the pulling pose is done | done | `hand_grab::StepTugFade` holds the pull until the frame after the fade is over (`HandSystem::UpdateTug`); test `HandTug.ThePullWaitsForTheHandsFade` |
| The hand's hold distance is the thing's hold lowering times its height | done | `HandSystem::ComputeHoldParameters` from `BeginTug`; tests `HandTug.TheGripsLeastHangIsTheHandsHeightTimesItsSizeThenTheShare` |
| The hand holds it with the pose its kind is held with (palm, side, tree or villager grip) | done | The hand takes the tree's hold type and is placed at the grip of the leaning tree (`HandPlacement.cpp`, the tug branch of the hand's placement); tests `HandPickUp.EachKindOfThingHasItsOwnHold`, `HandPickUp.SomeStaticsAreHeldFromTheSideTheRestOnThePalm`, `HandTug.TheGripIsUpTheUnitAxisAsTheGameMakesItUnit` |

## Pulling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's point follows the cursor across the plane of the land under the thing | done | `hand_grab::TugHandOnPlane`: where the pointer's line meets the pull's plane (`UpdateTug`); test `HandTug.TheHandIsWhereThePointersLineMeetsThePlane` |
| The pull is a spring of 1000 per unit between where the hand is and where it gripped the thing | done | `hand_grab::PullAt`: a spring of `k_TugStiffness` (1000) from the grip to the hand; `test/hand/test_hand_tug.cpp` |
| The pull is capped at the hand's greatest force | done | `PullAt` caps the spring at `hand_grab::MaxForce` (600000); test `HandTug.TheConstantsAreTheOriginals` |
| The pull turns the thing about its base, with a heaviness of 1000 against turning and a damping of 4000 times its turning speed | done | `PullAt` turns it about its base with `k_TugTurnInertia` (1000) and `k_TugTurnDamping` (4000); tests `HandTug.TheTurnIsThePullTimesTheInverseOfTheHeaviness`, `HandTug.TheLeanIsTheGamesAxisAngleTurnOfEachAxis` |
| As it is pulled the thing stretches along its height, up to 1.3 times, easing over 0.3 seconds | done | The stretch, at most `k_TugMaxStretch` (1.3) over `k_TugStretchSeconds` (0.3 s), goes into `HandDrawPose::upStretch` (`UpdateTug`); tests `HandGrab.APulledTreeLeansAndStretchesTowardsTheHand`, `HandTug.TheConstantsAreTheOriginals` |
| Things of the rock material don't lean while pulled | done | Only trees are tugged in our tree too, so it never shows; test `HandGrab.APulledTreeLeansAndStretchesTowardsTheHand` |
| The thing is drawn leaning and stretched, with the hand on it | done | The tree is drawn leaning and stretched (a `HandDrawPose` on it, the Transform untouched) with the hand at its grip (`hand_grab::TugHandPoint`); tests `HandTug.TheGripIsUpTheUnitAxisAsTheGameMakesItUnit`, `HandTug.TheLeanIsTheGamesAxisAngleTurnOfEachAxis` |

## Coming free

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree comes free once the hand's greatest force is at least its weight (its mass times 9.81) and the pull beats its mass | done | proven through the real input path (2026-10-31, act 4 of `refs/realinput/hand_pages.log`, `tools/checks/realinput/hand_pages.txt`); `PullAt`'s `comesFree`: weight x 9.81 within the hand's greatest force and the pull beating the weight; then `HandSystem::Uproot`; test `HandTug.ATreeComesFreeOnlyWhenTheHandCanLiftItAndPullsHarderThanItWeighs` |
| Something too heavy for the hand's force never comes free | done | `PullAt` never frees a tree whose weight x 9.81 is beyond `hand_grab::MaxForce`; the testbed's `hand.tug_heavy_tree`; tests `HandTug.ATreeComesFreeOnlyWhenTheHandCanLiftItAndPullsHarderThanItWeighs`, `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree` |
| A tree that comes free is uprooted, leaving its roots hole and throwing up dirt and smoke | done | `HandSystem::Uproot`: the roots ground mark (`ecs::ground_marks::Create`, 15 s), whose making throws up the uproot's own white dust (`disappear_smoke` mode 1, size 1), then the place-in-hand packet; a firefly on it is freed and may leave a one-shot seed (`HandHolding.cpp`, `Worship/FireFlyReward.cpp`); the frame it comes free the tree still leans at its base and the hand stays at its grip, as on every pull, until the state holds it (world test `HandInputWorld.TheFrameATreeComesFreeItStillLeansAtItsBaseWithTheHandAtItsGrip`); test `Explosion.aGroundMarkThrowsUpItsDustWhereItIsMadeEvenWithoutItsMesh`; see [the tug in openblack](../../../hand/grab-hold-and-throw.md#the-tug-in-openblack) |
| A person or animal comes free at once and is redrawn off the ground | done | Non-tree objects are taken once the 0.13 s pull pose blend is over (`_pendingTugHold` in `HandSystem::Update`), then the place-in-hand packet; tests `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree`, `HandTug.ThePullWaitsForTheHandsFade` |
| Things that aren't trees come free at once | done | Same path as the previous row (`HandSystem::Update`); test `HandGrab.OnlyATreeTheHandCanLiftIsPulledFree` |
| Once free, the thing is held as anything else | done | proven through the real input path (2026-10-31, act 4 of `refs/realinput/hand_pages.log`, `tools/checks/realinput/hand_pages.txt`); See [holding.md](holding.md); `HandHolding.cpp`; world test `HandPickUpWorld.ATreePulledFreeIsHeldAndLetGoAsAnythingElse` (`test/hand/test_hand_pick_up_world.cpp`) |
| Uprooting a tree plays its creak and counts towards the player's alignment | done | `HandHolding.cpp`: a random tree-break sound and `ecs::effects::alignment::UpdateForTree` (evil); testbed `hand.tug_at_crown`; world test `HandPickUpWorld.PullingUpATreeCreaksAndCountsAsAnEvilAct` (`test/hand/test_hand_pick_up_world.cpp`) |
| Letting go before it comes free leaves it rooted, and it is drawn upright again on the next frame | done | `UpdateTug`'s end removes the tree's `HandDrawPose`, so it is drawn upright from its logic on the next frame, as the original, whose tug never writes the tree's pose ([page](../../../hand/grab-hold-and-throw.md#tug)); testbed `hand.tug_let_go`; world test of its staying rooted, through the hand's frame on the game's data, `HandInputWorld.ATreeLetGoBeforeItComesFreeStaysRootedAsItStood` (`test/hand/test_hand_input_world.cpp`) |
