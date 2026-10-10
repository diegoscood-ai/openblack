# Picking up

The Action button over a loose thing takes it into the god hand: villagers, animals, rocks, trees, logs, piles of food
and wood, scaffolds, toys and more. The hand holds one thing at a time; a handful of food or wood grows while the button
is held (see [multi_pickup.md](multi_pickup.md)) and trees are pulled free first (see [tug.md](tug.md)).
openblack: `HandSystem` (`src/ECS/Systems/Implementations/HandHolding.cpp`, the press's choice in
`src/ECS/HandPressChain.h`, the object under the hand in `HandPlacement.cpp`) picks things up with the game's checks, in its
order. Tests: `test/test_hand_press_chain.cpp`, `test/test_hand_pick_reject.cpp`.

**Progress: 24/35 done, 5 partial — 76%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## When the hand may take something

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand holds a single thing at a time; with something in it, nothing else can be picked up | done | `HandSystem::ApplyPlaceInHand` refuses with something in the hand; while holding, the press takes the held branch (`hand_press::Choose`, `src/ECS/HandPressChain.h`; test `HandPressChain.EachBranchAloneHolds`) |
| Only things that say they can be picked up, and that the hand is allowed to hold, are taken | done | `HandSystem::FindObjectUnderHand` (the class filter, rocks, animals by species, reachable villagers) and `HandSystem::ValidForPlaceInHand`; the failed cases tap instead (`HandSystem::Update`) |
| Things already in a hand, carried by a tornado, or that a script made unpickable can't be taken | done | `HandSystem::ApplyPlaceInHand`: interactable, space in the hand, `object_flags::IsCannotBePickedUp` (SET_ID_PICKUPABLE) and not carried by a particle system (`particle_carried_objects::IsCarried`) |
| The hand only picks things up inside the player's influence, tested at the hand's point on the land | done | `HandSystem::InInfluence` at the action point, tested at the press and again when the grab completes (`HandSystem::Update`) |
| Things that can't be pulled (big forests, fields, piles, fireballs) and things in flight wait until the press has lasted 225 ms; anything else starts being pulled at once. A press let go within 225 ms is a tap | partial | `HandSystem::Update`: a tree is tugged at the press, a big forest or a free flying object waits 225 ms, piles and fields start a locked select, anything else is taken after the 0.13 s pull blend; a release within 225 ms taps. The magic fireball is not in our tree |
| A thing flying through the air can be grabbed out of it (a catch), tested again every frame of the wait | partial | A free flying object waits the 225 ms timer and is then caught, but whether it is free is decided only at the press (`_pendingTimer`), not every frame of the wait |
| The first pick-up triggers its help message (the catch's own message never fires: it is tested after the thing has already left the physics) | done | `HandSystem::PickUp` triggers `help_profile::Trigger` with PickUp, never Catch, tested after the object left the physics |
| Picking something up sends a pick-up reaction around the hand that people and animals respond to | todo | The pick-up reaction is not sent (`HandSystem::ApplyPlaceInHand` lists it as pending) |
| A burning thing picked up keeps burning and its fire is told it has started moving (in the hand, unless it is a villager) | done | `fire::StartedMoving` from `HandSystem::PickUp` (in the hand unless a villager) |
| A firefly hiding exactly where the picked tree or rock stands is destroyed, and the land's firefly reward table gives a one-shot miracle seed there (nothing when the table is all zero) | done | `worship::OnPlacedInMagicHand` from `HandSystem::PickUp` (`src/Worship/FireFlyReward.cpp`) |

## What can and can't be picked up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers can be picked up, unless at home, already in a hand, or hiding in a building | done | `HandSystem::FindObjectUnderHand` keeps a villager only when `ecs::villager::IsReachable` (not at home, not held, not hiding) |
| Animals can be picked up only if their species allows it | done | `ecs::animal_ai::ValidForPlaceInHand` (the species' playerCanPickUp) in `FindObjectUnderHand` |
| Loose mobile objects (pots, logs, branches, toys, balls) can be picked up | done | Mobile objects, pots and mobile statics pass the class filter (`FindObjectUnderHand`); the holds per kind in `ComputeHoldParameters` |
| Among fixed things only trees can be picked up (not bushes or features) | partial | Trees, dead trees, big forests, fields and piles pass the filter; bushes are trees in our tree too (the 22 tree types, trees.md), features do not |
| Rocks can be picked up unless they are wider than 3.6 units | done | `Rocks::ValidForPlaceInHand` (2D radius at most 3.6); a bigger rock is tapped (split) instead |
| A field can be grabbed only while it has food in it | done | `HandSystem::FieldValidForLockedSelect` (growth above 0 and food above 1, `HandFish.cpp`) |
| A fish farm can be grabbed only while it has fish in it | done | `ecs::FindFishFarmAt` only finds a farm by one of its visible fish (`src/ECS/FishShoals.cpp`); the fish farm branch of `hand_press::Choose` |
| A food or wood pile or store can be grabbed only while it has some of what it holds | done | `HandSystem::PickUp` takes min(the hand pot's first amount, what the pile or store has) and nothing when it has none |
| A scaffold can be picked up while loose, or while its site hasn't started building and it was put down recently enough | todo | No scaffold pick-up in our tree (the scaffolds' tooltip and tap are listed as not ported) |
| A big forest gives one of its trees, made on the spot, and counts one tree fewer | done | `HandSystem::ApplyPlaceInHand` with `CreateTreeFromForest` (a Conifer, 350 wood off the forest, `HandTrees.cpp`; trees.md) |
| Another player's fireball can be caught; the player's own slips through | todo | No magic fireball in our tree |
| A miracle's one-shot bubble goes into the hand as a ready miracle | done | A one-shot orb passes the class filter and is picked up after the 225 ms hold (`HandSystem::Update`); its tap gives its charged seed |
| Creatures can't be picked up: the hand takes hold of them to stroke and slap instead | done | A creature takes the creature branch (a locked select through `CreatureHandSystem`), never the pick-up; see [creature_contact.md](creature_contact.md) |
| Bonfires, rewards and chests, fragments, help spirits, script highlights, shields and vortices can't be picked up | done | The class filter of `FindObjectUnderHand` and `HandSystem::ValidForPlaceInHand` leave them out |
| Buildings can't be picked up | done | Abodes are only tapped (`hand_press::Choose`, the tap-only branch) |

## What happens to the thing taken

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager in the hand is marked as held, remembers what it was doing and goes into its in-hand state | partial | `ecs::living::InterfaceSetInMagicHand` (`src/ECS/LivingPhysics.cpp`) marks it held and sets IN_HAND; its previous state is not stored at the pick-up |
| Picking up a villager of the hand's player in its breeding years alarms those around it | todo | Not in our tree (a TODO in `InterfaceSetInMagicHand`) |
| Villagers and animals in the hand switch to their in-hand animation and cry out | partial | Villagers scream (`HandSystem::GenericPickupSounds`) and take IN_HAND's clip; animals take their in-hand clip (`src/ECS/AnimalAnimations.cpp`); no animal cry found |
| An animal picked up is taken out of its flock into a flock of its own (the old flock goes when empty) | done | `ecs::animal_ai::PlaceInHand` (`src/ECS/AnimalAI.cpp`): out of its flock into a flock of its own, the old one deleted when empty unless a script's |
| Pulling up a tree plays a creak from the tree's sounds and counts towards the player's alignment | done | `HandSystem::PickUp`: a random tree-break sound and `ecs::effects::alignment::UpdateForTree` (evil) |
| A field gives a first handful of 25 (or what it has), halved when the crop is ripe | done | `HandSystem::TryPickUpField` (`HandFish.cpp`): min(25, food), halved when ripe |
| A pile gives a handful of what it holds, poisoned if the pile was | done | `HandSystem::PickUp` makes the hand pot poisoned when the pile was; a store pile offers the store's total |
| What is picked up leaves the physics (without its landing) and the map's cells, and stops what it was doing | done | `HandSystem::PickUp`: `PhysicsObjects::RemoveObject`, `map_cells::RemoveMapObject`, `reactions::SetUnavailableInHand` |
| A creature being dragged on the leash by what is picked up is let go | todo | Not in our tree |
| Things the hand holds are kept in the save and restored on load | todo | No save of the hand in our tree |
