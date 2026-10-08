# Throwing and letting go

What happens when the Action button is let go with something in the hand: a gentle put-down where the hand is, a throw
with the hand's speed, or the handful poured out onto the ground. Where the thing lands then matters: a store, a
building site, a worship site, the creature, or a job for a villager.

openblack: `HandSystem::Release` (`HandHolding.cpp`) lets go into the game's physics through
`physics::from_hand::Throw` (`src/ECS/Physics/FromHand.cpp`); slow handfuls pour
(`HandSystem::PutDownHandPot`) and a held thing can be given straight to a store or pile under the hand
(`ecs::held_apply`). The other targets (sacrifice, building sites, disciples) are not ported. Tests:
`test/test_hand_apply.cpp`, `test/test_release_prediction.cpp`.

The first land teaches throwing with a silver scroll, [Throwing Stones](../story/silver_scrolls/throwing_stones.md).

**Progress: 20/30 done, 0 partial — 67%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Putting down or throwing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The thing leaves the hand where it is drawn in the hand, turned as the hand held it and at its held size, only lifted out of the ground | done | `HandSystem::ThrowObjectFromHand` (`HandHolding.cpp`) builds the matrix from the synced hand pose (or the drawn pose), its scale kept; `physics::from_hand::InitialisePhysicsFromHand` (`src/ECS/Physics/FromHand.cpp`) only raises it out of the ground when thrown |
| It takes the hand's spring velocity at the moment of letting go, and no spin; 180 ms later it gets a one-turn twist | done | The spring's velocity (`HandSystem::SendRelease`); 180 ms after, the release impulse packet (`HandSystem::UpdateReleaseImpulse`, `HandTurn.cpp`) gives the twist (`physics::from_hand::ApplyReleaseSpin`) |
| Let go moving across the land at no more than 2 units a second, it is put down: lowered onto the ground along the slope and lifted clear of anything it overlaps | done | `from_hand::IsThrown` (horizontal speed squared above 4); a put-down goes through `AdjustToGroundLevel` and `RaiseUntilNotIntersecting` |
| Faster than that it is thrown and flies | done | `from_hand::InitialisePhysicsFromHand`; see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| There is no separate "set down on another thing" rule: a put-down raised onto something stays in the physics and settles there | done | `InitialisePhysicsFromHand`: a raised put-down is not landed and stays in physics |
| A tree put down upright (tilted under 0.2 radians) stays where it is put; tilted further it falls | done | `InitialisePhysicsFromHand`: a tree tilted more than 0.2 rad (or not to be replanted) stays in physics; trees.md |
| A person or animal put down on a slope steeper than about 45 degrees falls instead of standing | done | `InitialisePhysicsFromHand`: a villager, animal or fence lands only where the land normal's y is at least 0.7 |
| A thing put down over the sea (or anywhere not dry land) falls in rather than standing | done | `InitialisePhysicsFromHand`: only on dry land or a cell above altitude 1 |
| A handful of food or wood let go at a speed squared of at most 5 is poured out where the hand is, adding to stores and piles within 3×3 cells or making a pile, lost in water, and the handful is gone | done | `from_hand::Throw` (speed squared at most 5) then `HandSystem::PutDownHandPot` (`HandResources.cpp`): the 3 x 3 cells' stores and pots, else a pile; lost off the map |
| Faster than that, the handful is thrown as a pot | done | `from_hand::Throw` throws it as a pot otherwise |
| A thrown villager drops what it was carrying, which flies on as its own thing | done | `InitialisePhysicsFromHand` calls `ecs::villager::CreateDroppedResource` when a villager does not land |
| Anything thrown or dropped that doesn't land is offered to creatures nearby to catch | todo | `PhysicsObjects::CheckAllCreaturesForCatching` only calls a hook nobody sets (`SetCreatureCatchHook`) |
| Anything thrown or dropped that doesn't land sends a flying-object reaction out; any animal nearer than twice its speed runs from it | done | `animal_ai::SpreadFlyingObjectReaction` from `InitialisePhysicsFromHand` when it does not land |
| The first throw of the player's triggers a help message | done | `HandSystem::ThrowObjectFromHand` triggers `help_profile::Trigger` with Throw |
| A put-down toy can teach the creature to play with it | todo | A TODO in `InitialisePhysicsFromHand` |
| A put-down thing can teach the creature to copy what the player did with it | todo | A TODO in `InitialisePhysicsFromHand` |
| The game remembers the last thing the hand dropped | done | `HandSystem::RenderHandRelease` sets `_lastReleased`, read by the release impulse |
| When the hand lets go, its pose eases back to the empty hand's over 0.13 seconds | done | The change back to the NORMAL state starts the 0.13 s state blend (`HandCrossFade`, `src/3D/HandCrossFade.h`) |

## Where it lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Food or wood let go onto a store adds to it | done | A store takes a held object on the press (`HandSystem::ApplyHeldToObject`, `ecs::held_apply`) or when a thrown body hits it (`HandPhysics.cpp`); stores are storage pits, workshops and worship sites (`ecs::resource_stores`); tests `HeldApplyTest.*`, `HeldClassApply.*` (`test/test_hand_apply.cpp`). A release itself gives nothing, as the original |
| Wood let go on a building site or scaffold builds with it | todo | The release's building-site branch is pending (hand-and-interface.md) |
| A scaffold let go on another scaffold combines them | todo | Not in our tree |
| A tree put down is replanted, joining a forest nearby or starting one | done | `HandSystem::Replant` from the tree's end of physics (`HandPhysics.cpp`, `HandTrees.cpp`), with the forest search; trees.md |
| A villager let go on a built worship site's altar is sacrificed, for (0.5 × life + 0.5) of its sacrifice value (1.25 times for a child); animals and trees too, by their life | todo | The sacrifice is not ported (hand-and-interface.md, Pending) |
| Food let go at a worship site, and wood at a workshop, supplies it | done | Worship sites and workshops are stores of `ecs::resource_stores` (`worship::site::DeleteObjectAndTakeResource`, `workshops::`) |
| A villager put down near a job (chosen while held from the objects within 3×3 cells, by a 6 m falloff times each object's pull) becomes a disciple doing it: farmer, forester, fisherman, builder, breeder (with a villager of the other sex), trader, missionary, worshipper, craftsman, or moving house | todo | No disciple from the hand (TODOs in `src/ECS/LivingPhysics.cpp`); disciples exist only from scripts (SET_DISCIPLE) |
| Making a disciple plays the advisor's line for that kind of disciple | todo | Not in our tree |
| A villager put down beside another house of its player moves in; near its own home nothing special happens | todo | Not in our tree |
| Thrown people impress or frighten the villages that watch them fly (the flying-object reaction, reach 25, weighed by each watcher's distance), which also sways the player's alignment | todo | Not in our tree |
| What lands from a throw hurts what it hits and is hurt by its fall | done | The impact rules of `src/ECS/Physics/PhysicsObjects.cpp` and `src/ECS/LivingPhysics.cpp` (`HurtByImpact`); see [../physics/impact_damage.md](../physics/impact_damage.md) |
| Every throw from the hand is credited to the player, whatever its size | done | The physics object keeps `byPlayer` for a throw from the hand (`PhysicsObjects::AddObjectFromHand`) |
