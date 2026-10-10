# Scooping handfuls

Holding the Action button over a pile, a village store, a field or a fish farm scoops up a handful of food or wood that
grows the longer the button is held, with a stream of particles flowing into the hand. The hand hovers over the
source, tipped down towards it, until the button is let go.

openblack: `HandSystem` scoops from food and wood piles, storage pits' piles included, fields and fish farms
(`HandResources.cpp`, `HandFish.cpp`), with the game's amounts (`ProcessInInteractPile`) and its rising sound
(`HandSystem::UpdatePickupSound`). Test: `HandMultiPickUp.RampsWithTheSquareOfTheTurns`.

**Progress: 20/24 done, 2 partial — 88%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Starting a scoop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scoop starts at once (no wait) from a food pile, a wood pile, a village store's food or wood, a field with crops or a fish farm with fish | done | The press starts the locked select at once (`HandSystem::Update`, the Field, Hovered and FishFarm branches -> `SendStartLockedSelect`); `HandSystem::ApplyStartLockedSelect` (`HandTurn.cpp`) for piles, storage pits' piles, fields and fish farms (`TryPickUpField`, `TryPickUpFish` in `HandFish.cpp`) |
| A pile only gives the resource it holds, and only while it holds some | done | `HandSystem::Update`: a pile of the hand's own pot type is not scooped; `ProcessInInteractPile` (`HandResources.cpp`) takes the source's resource while it has some |
| The first grab makes a handful (the hand's food or the hand's wood) of 25, or what the source has, and puts it in the hand | done | `HandSystem::PickUp` (`HandHolding.cpp`): n = min(the hand pot's amount picked up at first, the source's resource) into a new HandFood or HandWood pot in the hand |
| A handful from a poisoned pile is poisoned too | done | `HandHolding.cpp`: the hand pot is poisoned when the source was; see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| A field gives a first handful of 25 (or what it has), halved when the crop is ripe | done | `HandFish.cpp` `TryPickUpField`: min(25, food), halved when ripe; the turn's ramp halved too |
| The hand's distance from the camera is set to reach the source as the scoop starts | done | `HandPlacement.cpp`: while the select lasts the hand stays at its x, z, at the ground + the source's height for the hand + the hold height |
| The cursor is pinned while scooping, and freed when the scoop ends | partial | The hand stays over the source while the select lasts (`HandPlacement.cpp`); our tree draws no system cursor to pin |
| Scooping starts a force-feedback effect for the kind of resource | todo | No force feedback; see [clicking_and_activating.md](clicking_and_activating.md) |
| Scooping starts a stream of particles from the source into the hand, by the kind of resource | done | `HandSystem::UpdatePickupParticles` (`HandEffects.cpp`): 8 a second rising over 1 s from the ground under the hand to the hand, wood, food or fish sprites, from the select's start to its end |

## While the button is held

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A handful starts at 25 | done | The hand pots' info (`amountPickedUpInitially`), read in `HandSystem::PickUp` |
| Each game turn it takes 8 + 62·t² more, t rising from 0 to 1 over 60 turns (the info's 6 seconds turned into turns) | done | `HandSystem::ProcessInInteractPile`: (int)(perTurn + (perTurnEnd - perTurn) t^2), t over the info's ramp time in turns |
| A single handful can hold up to 20000 | done | `ProcessInInteractPile`: at most `maxAmountCanBePickedUp` (20000) in the hand |
| The handful's mesh grows with its amount | partial | The hand's pots keep scale 1 (`PotArchetype::SetSize`); only the hand's grip opens as the food grows |
| The scooping sound rises as the handful fills | done | `HandSystem::UpdatePickupSound` (`HandEffects.cpp`): pitch 60 + 180 t^2 |
| The source loses what is scooped, and is gone once empty (a store's own pile stays) | done | `ProcessInInteractPile`: a storage pit's pile takes from the pit, a loose pile from itself, and an emptied loose pile goes (`ToBeDeleted`) |
| The hand hovers over the source at its height plus the source's height | done | `HandPlacement.cpp`: the ground at the hand + `object::GetHeightForHandAboveInteractObject` of the source + the hold height |
| The hand is tipped down towards the source at 78.75 degrees, further when it is lower than 2.5 units | todo | Not found in our tree |
| The hand holds the handful from the side while scooping | done | A hand pot is held from the side (`HandSystem::ComputeHoldParameters`: SIDE for pots) |
| The hand turns with the camera's heading as it scoops | done | The hand's matrix takes the heading from the camera-to-cursor ray every frame (`HandMatrixRotation`) |

## Ending a scoop

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Letting go ends the scoop, as does the source emptying or the hand leaving the player's influence, and stops its particles and force feedback | done | `HandSystem::ProcessLockedSelect` (`HandTurn.cpp`): it ends when the source is gone, its interaction says so or the influence at the hand is 0; the release sends the end packet; the particles and the loop stop with it |
| The handful is then held like anything else | done | The hand pot stays in the hand; see [holding.md](holding.md) |
| A handful let go slowly is poured out, streaming down into a pile where the hand is | done | `HandSystem::Release` -> `physics::from_hand`: a hand pot slow enough is put down at once (`PutDownHandPot`, `pot_resource::AddResourceToPos`); see [throwing.md](throwing.md) |
| A handful pressed onto a store, or a pile of the same resource, goes into it | done | `ecs::held_apply` (`HandApplyToObject.cpp`): a hand pot pressed onto a store or a same-resource pile goes into it; building sites are pending |
| The food and wood miracles pour from the hand the same way | done | The food and wood seeds are poured from the hand (IN_HAND seeds, one apply a turn, `HandSpellSeed.cpp`); see [../miracles/](../miracles/) |
