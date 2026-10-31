# The scribble

A quick back-and-forth scribble is the game's "no" gesture: it shakes a miracle out of the hand, calls off a power-up,
takes the leash off and closes the pickers. What it does depends on what the hand holds and what is open.

**Progress: 15/15 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/hand-casting.md).

## Recognising a scribble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scribble is strokes back and forth that must be wider than they are tall | done | Two SCRIBBLE templates with `checkAspect` in `Data\Gestures.jty`, matched by `src/Magic/Gestures/GestureMatch.cpp`; test `GestureMatch.AScribbleTemplateThatChecksItsAspectNeedsStrokesWiderThanTall` |
| A scribble is only recognised when there is something for it to do | done | `ProcessPowerUpSystem` (`src/Magic/Gestures/PowerUpSystem.cpp`) only tries the scribble while a power-up is asked for, something is held, the leash is held in the hand, the selection is open or an icon charges; test `PowerUpSystem.TheScribbleShakesAHeldLeashOff` |
| A scribble leaves no trail on the land and makes no recognition sound | done | Every scribble path calls `gestures::Success(false)`, which lays no sparkles and plays no recognition sound (`PowerUpSystem.cpp`) |
| Scribbling shows the hand's shake-off: the shake sound and a band flying off | done | `hand_fx::RemoveHandSpellVisuals` (`src/Magic/Hand/HandMagicFX.cpp`) plays the shake sound and sends a band back, the original's remove-from-hand visual ([hand casting](../../../magic/hand-casting.md)), which does nothing else: no motion of the hand comes with it (read in the frozen v1.42 exe). Every scribble branch calls it (`PowerUpSystem.cpp`); the branches are pinned by `PowerUpStage.*`, `Selection.AScribbleClosesIt`, `LeashPicker.AScribbleClosesItBeforeAnyShake` and `PowerUpSystem.TheScribbleShakesAHeldLeashOff` |

## With a miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a power-up is being charged by the worship icon, a scribble calls it off instead of dropping the miracle | done | `PowerUpStage` in `PowerUpSystem.cpp`: with a power-up asked for, the scribble powers down (`SetPowerUpCharge(icon, -1)`, help event 20) and keeps the seed, as the original; test `PowerUpStage.AScribbleWithAPowerUpAskedForPowersDownAndKeepsTheSeed` |
| With a ready miracle from a worship icon in the hand and no power-up asked for, a scribble drops it back where it came from, even outside the player's influence | done | `PowerUpStage`: with no power-up asked for, the scribble drops the icon seed with no influence test (`ForceDropHeld`, help event 22), as the original; test `PowerUpStage.AScribbleWithNoPowerUpAskedForShakesTheSeedOutWhereverTheHandIs`. Only where worship sites exist (none on Land 1, as in the original) |
| Any other miracle in the hand is only scribbled away inside the player's influence | done | `ProcessPowerUpSystem` step (b): the shake needs `inInfluence` and `validToShake` (`HandSystem::UpdateSeedInHand` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp`); test `PowerUpSystem.TheScribbleShakesAHeldLeashOff` |
| A miracle scribbled away gives its prayer power back to the worship site it came from | done | `HandSystem::ApplyForceDropHeld` sends the seed's charge back to its icon's worship site (`worship::OnSeedOutOfHand`, `src/Worship/Worship.cpp`); test `WorshipTest.ASeedLeftOutOfTheHandGivesItsChargeBackToItsIconsSite` |
| A help message follows a scribble that drops a miracle or calls off a power-up | done | Help events 20 and 22 are counted in the help profile (`src/Help/HelpProfile.cpp`), which the scripts' help functions read; the plain shake-out of a seed with no power-up step sends none, as in our wiki's reading; test `PowerUpStage.EachOutcomeRaisesItsHelpEvent` |

## With other things in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Inside the player's influence, a scribble shakes out anything the hand holds that may be shaken out, not only miracles | done | `validToShake` is any held object; `HandSystem::ForceDropHeld` drops it (`HandSpellSeed.cpp`); test `PowerUpSystem.TheScribbleShakesAHeldLeashOff` |
| With the hand holding the leash, a scribble takes the leash off the creature | done | `ProcessPowerUpSystem` step (b) (`ScribbleCancelFor`, `PowerUpSystem.cpp`): with nothing in the hand, a leash held in it is shaken off with no influence test, before any icon charging. As in the original it sends the click on the creature's packet (`CreatureLeashClick`, 0x5F), so the leash comes off at the next game turn through the leash key; `LeashSystem::Shake` stays for the debug windows and the hand demos. Tested in `test_gestures.cpp` (`PowerUpSystem.TheScribbleShakesAHeldLeashOff`); testbed `leash.shake` |
| A leash tied to something rather than held in the hand is not taken off by a scribble | done | `ScribbleCancelFor` passes a tied leash by, to the icon charging (`PowerUpSystem.cpp`), as the original |
| With an empty hand while a worship icon is charging, a scribble cancels the charging | done | `ProcessPowerUpSystem` step (b): `CancelMostChargedIcon` through `src/Worship/GestureIconProvider.cpp` (`player::CancelMostRecentCharge`: the charge that started last, never one started on turn 0, as the original, [gestures](../../../magic/gestures.md#what-is-looked-for-and-when)); the branch is tested in `PowerUpSystem.TheScribbleShakesAHeldLeashOff` |

## Closing pickers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the leash picker open, a scribble closes it and leaves the leash on | done | `LeashSelection::Stage` takes the scribble before step (b) can shake the leash off, and the pass ends (`src/Magic/Gestures/PowerUpSystem.cpp`); tested in `LeashPicker.AScribbleClosesItBeforeAnyShake` |
| With the miracle selection open, a scribble closes it | done | `Selection::Stage` closes on a scribble (help event 21) in `PowerUpSystem.cpp` |
