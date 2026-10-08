# Pulling things free

Grabbing a standing tree doesn't lift it at once: the hand pulls at it, the tree leans and stretches towards the hand
like a spring, and it only comes free once the pull is strong enough for its weight. Too heavy for the hand's strength,
it never comes free. Everything else the hand can take comes free as soon as the hand has faded into its pulling pose.

openblack: the pull is in `HandSystem` (`BeginTug`, `UpdateTug`, `HandTrees.cpp`), simpler than the game's spring: a tree
comes free once the hand has moved weight / 1000 sideways; the uprooted tree leaves its roots hole (`HandSystem::Uproot`).
No unit test covers the pull yet.

**Progress: 9/22 done, 8 partial — 59%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Taking hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only standing trees are really pulled; big forests, fields, piles and fireballs wait for the press to become a hold instead, and anything else comes free once the hand's pulling pose has faded in | partial | Trees are tugged from the press (`HandSystem::BeginTug`, `HandTrees.cpp`); a big forest waits the 225 ms timer, piles and fields start a locked select, other objects are taken once the 0.13 s pull pose blend is over (`_pendingTugHold` in `HandSystem::Update`). The magic fireball is not in our tree |
| The hand takes hold at the point it gripped, measured from the thing's base on the land | partial | `BeginTug` keeps the grab point (the action point) and its depth along the mouse ray, not an offset from the base |
| The slope of the land under the thing is taken as the plane the pull works in, set at a height found from the camera's ground distances to the base and to the hand | partial | Differs on purpose (user's choice, 2026-09-30): no pull plane; the hand is the mouse ray at the grab depth and only its horizontal move counts (`HandSystem::UpdateTug`) |
| The pull only starts once the hand's 0.13-second fade into the pulling pose is done | todo | The tree's tug starts at the press with no 0.13 s wait; the 0.13 s blend only gates the other objects' pick-up |
| The hand's hold distance is the thing's hold lowering times its height | done | `HandSystem::ComputeHoldParameters` from `BeginTug` |
| The hand holds it with the pose its kind is held with (palm, side, tree or villager grip) | done | The hand takes the tree's hold type and is placed at the grip of the leaning tree (`HandPlacement.cpp`, the tug branch of the hand's placement) |

## Pulling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's point follows the cursor across the plane of the land under the thing | partial | The ray at the grab depth, its horizontal move from the grab point (`UpdateTug`); no plane of the land |
| The pull is a spring of 1000 per unit between where the hand is and where it gripped the thing | partial | `UpdateTug` compares 1000 x the horizontal pull with the weight (threshold weight / 1000); no spring acting on the tree |
| The pull is capped at the hand's greatest force | todo | No force cap in our tug |
| The pull turns the thing about its base, with a heaviness of 1000 against turning and a damping of 4000 times its turning speed | partial | The tree leans towards the hand by 0.25 rad x pull / threshold at once (`UpdateTug`); no turning dynamics, heaviness or damping |
| As it is pulled the thing stretches along its height, up to 1.3 times, easing over 0.3 seconds | todo | No stretch: `HandDrawPose::upStretch` stays 1 (see the comment in `UpdateTug`) |
| Things of the rock material don't lean while pulled | done | Only trees are tugged in our tree too, so it never shows |
| The thing is drawn leaning and stretched, with the hand on it | partial | The tree is drawn leaning (a `HandDrawPose` on it, the Transform untouched) with the hand at its grip; no stretch |
| The pull gives the thing a velocity, kept for when it comes free | todo | The uprooted tree gets no velocity from the tug |

## Coming free

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tree comes free once the hand's greatest force is at least its weight (its mass times 9.81) and the pull beats its mass | partial | The tree comes free once the horizontal pull passes weight / 1000 (`UpdateTug`), so 1000 x pull > weight; the hand force cap test is missing |
| Something too heavy for the hand's force never comes free | todo | Any tree comes free once the hand moves far enough; no test of the hand's force against the weight |
| A tree that comes free is uprooted, leaving its roots hole | done | `HandSystem::Uproot`: the roots ground mark (`ecs::ground_marks::Create`, 15 s), then the place-in-hand packet; a firefly on it is freed and may leave a one-shot seed (`HandHolding.cpp`, `Worship/FireFlyReward.cpp`) |
| A person or animal comes free at once and is redrawn off the ground | done | Non-tree objects are taken once the 0.13 s pull pose blend is over (`_pendingTugHold` in `HandSystem::Update`), then the place-in-hand packet |
| Things that aren't trees come free at once | done | Same path as the previous row (`HandSystem::Update`) |
| Once free, the thing is held as anything else | done | See [holding.md](holding.md); `HandHolding.cpp` |
| Uprooting a tree plays its creak and counts towards the player's alignment | done | `HandHolding.cpp`: a random tree-break sound and `ecs::effects::alignment::UpdateForTree` (evil); no unit test |
| Letting go before it comes free leaves it rooted, leaning and stretched as it was last drawn, until its next growth step redraws it | done | `UpdateTug`'s end removes the tree's `HandDrawPose`, so it is drawn upright from its logic on the next frame, as our wiki says the original does. Our wiki differs: a release before the uproot draws the tree upright again on the next frame, since the tug never wrote its pose ([page](../../bw1-notes/trees.md#tug-handstatetug-enter-0x5b7df0--update-0x5b8070-in-handtreescpp)) |
