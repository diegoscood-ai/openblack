# The scribble

A quick back-and-forth scribble is the game's "no" gesture: it shakes a miracle out of the hand, calls off a power-up,
takes the leash off and closes the pickers. What it does depends on what the hand holds and what is open.

**Progress: 11/15 done, 1 partial — 77%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## Recognising a scribble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scribble is strokes back and forth that must be wider than they are tall | done | Two SCRIBBLE templates with `checkAspect` in `Data\Gestures.jty`, matched by `src/Magic/Gestures/GestureMatch.cpp` |
| A scribble is only recognised when there is something for it to do | done | `ProcessPowerUpSystem` (`src/Magic/Gestures/PowerUpSystem.cpp`) only tries the scribble while a power-up is asked for, something is held, the selection is open or an icon charges |
| A scribble leaves no trail on the land and makes no recognition sound | done | Every scribble path calls `gestures::Success(false)`, which lays no sparkles and plays no recognition sound (`PowerUpSystem.cpp`) |
| Scribbling makes the hand shake, with the shake sound and the bands flying off | partial | `hand_fx::RemoveHandSpellVisuals` (`src/Magic/Hand/HandMagicFX.cpp`) plays the shake sound and sends a band back; no shaking motion of the hand itself was found in our hand code |

## With a miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a power-up is being charged by the worship icon, a scribble calls it off instead of dropping the miracle | done | `PowerUpGestures` in `PowerUpSystem.cpp`: with a power-up asked for, the scribble powers down (`SetPowerUpCharge(icon, -1)`, help event 20) and keeps the seed |
| With a ready miracle from a worship icon in the hand and no power-up asked for, a scribble drops it back where it came from, even outside the player's influence | done | `PowerUpGestures`: with no power-up asked for, the scribble drops the icon seed with no influence test (`ForceDropHeld`, help event 22). Only where worship sites exist (none on Land 1, as in the original) |
| Any other miracle in the hand is only scribbled away inside the player's influence | done | `ProcessPowerUpSystem` step (b): the shake needs `inInfluence` and `validToShake` (`HandSystem::UpdateSeedInHand` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp`) |
| A miracle scribbled away gives its prayer power back to the worship site it came from | done | `HandSystem::ApplyForceDropHeld` sends the seed's charge back to its icon's worship site (`worship::OnSeedOutOfHand`, `src/Worship/Worship.cpp`) |
| A help message follows a scribble that drops a miracle or calls off a power-up | done | Help events 20 and 22 are counted in the help profile (`src/Help/HelpProfile.cpp`), which the scripts' help functions read; the plain shake-out of a seed with no power-up step sends none, as in our wiki's reading |

## With other things in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Inside the player's influence, a scribble shakes out anything the hand holds that may be shaken out, not only miracles | done | `validToShake` is any held object; `HandSystem::ForceDropHeld` drops it (`HandSpellSeed.cpp`) |
| With the hand holding the leash, a scribble takes the leash off the creature | todo | Not in `ProcessPowerUpSystem` (a TODO there); `LeashSystem::TakeOffHeldLeash` exists but only a hand demo's start calls it (`creature_loop::ReleaseLeashHeldInHand`). Our wiki differs: our reading of the scribble's step lists only the held object and the most charged icon, and whether a held leash is shaken off there is still unread ([page](../../bw1-notes/creature.md#pending)) |
| A leash tied to something rather than held in the hand is not taken off by a scribble | todo | The rule is in `creature_loop::ReleaseLeashHeldInHand` (`src/ECS/CreatureLoop.cpp`) but no scribble reaches it |
| With an empty hand while a worship icon is charging, a scribble cancels the charging | done | `ProcessPowerUpSystem` step (b): `CancelMostChargedIcon` through `src/Worship/GestureIconProvider.cpp` (`player::CancelMostRecentCharge`; which icon is cancelled is unverified in our wiki) |

## Closing pickers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash picker open, a scribble closes it and leaves the leash on | todo | No leash picker in our tree (see [leash_gestures.md](leash_gestures.md)) |
| With the miracle selection open, a scribble closes it | done | `Selection::Stage` closes on a scribble (help event 21) in `PowerUpSystem.cpp` |
