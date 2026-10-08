# Miracle gestures

Gestures are how the player works with miracles: a spiral calls up the miracle selection and the miracle's own gesture
brings it into the hand, a circle sizes a storm or a shield, and each miracle's power-up gestures make it stronger. The
miracles themselves, and what they do once cast, are in [../miracles/](../miracles/).

**Progress: 19/25 done, 2 partial — 80%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## Sizing with a circle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A circle is waited for while the hand holds a miracle sized by a circle (the storms, the tornado, the two shields) and the Action button is held | done | `ProcessPowerUpSystem` step (a) in `src/Magic/Gestures/PowerUpSystem.cpp` (a held seed with a `sizingGesture`, the action press latched) |
| The circle is waited for whether or not the miracle in the hand is ready to cast yet | done | Step (a) does not test whether the seed is ready |
| The circle's middle on the land and its size across the land are handed to the miracle, which is cast there at that size | done | The circle's point and size go into the cast (`circlePending` in the hand's apply, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`) |
| Once drawn, the circle is remembered for five seconds of game time and no new circle is waited for meanwhile | done | `k_CircleLife` 5 s and `circlePending` in `PowerUpSystem.cpp` (game time, not while paused) |
| Releasing a circle-sized miracle with no circle remembered fails, with the failure sound | done | `HandSpellSeed.cpp`: a sized seed with no circle goes to `FailApply` (`G_SpellCastFailure` and the failed-apply effect) |
| Starting the power-up system forgets a remembered circle | done | `TrySetupPowerUpGestures` clears the pending circle (`PowerUpSystem.cpp`) |

## Powering up the miracle in the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each miracle has up to three power-up gestures (spiral, inverse spiral and others), read from the game's tables | done | `GSpellSeedInfo::powerUpGestures` (`src/InfoConstants.h`), read by `TrySetupPowerUpGestures` |
| Power-up gestures are only waited for while the hand holds a ready, uncast miracle made at one of the player's worship icons | done | `HoldingChargingSeed` (ready, not cast, from an icon) in `PowerUpSystem.cpp`; the worship icons are in `src/Worship`, so this works on lands with worship sites (none on Land 1, as in the original) |
| Every power-up level's gesture is waited for except the level already asked for | done | `PowerUpGestures` skips `currentPowerUpGesture`; a level needs the player to have its magic and the icon to offer it (`PowerUpAvailable`) |
| Drawing a power-up gesture asks the worship icon for the extra power; the level only changes once the icon has charged it, and any surplus goes back to the site | done | `SetPowerUpCharge` makes the icon charge that level (`src/Worship/GestureIconProvider.cpp`, `src/Worship/WorshipSpellIcon.cpp`); the seed's `SetPowerUp` sends a surplus back to the site (`src/Magic/Core/SpellSeed.cpp`) |
| A help message is given when a power-up is asked for | done | Help event 18 is counted in the help profile (`src/Help/HelpProfile.cpp`) |

## Calling up a miracle by gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With nothing in the hand, a spiral opens the miracle selection for the player's own miracles | done | `ProcessPowerUpSystem` step (c) and `Selection::Open`, with the worship icons as the `IconProvider` (`src/Worship/GestureIconProvider.cpp`); test `Gestures.selectionWithFakeIcon`. Only where worship sites exist |
| An inverse spiral opens the selection for miracles meant for the creature | done | Step (c), category `selectionSystemGestureCreature` |
| With the selection open, drawing a miracle's own gesture brings that miracle from its worship site into the hand | done | `Selection::Stage` then `IconProvider::RequestSpell`, which charges the best icon (`src/Worship/PlayerSpellIcons.cpp`); test `Gestures.selectionWithFakeIcon` (FORK_RIGHT requests seed 4) |
| The selection only offers miracles the player has and whose worship site can supply them | done | `ForEachRequestableIcon` and `IconValidForRequest` (`icon::ValidForRequestSpell` in `src/Worship/WorshipSpellIcon.cpp`) |
| The selection closes by itself after 30 seconds | done | `Selection::Stage` with `selectionSystemTimeOut` from the tables |
| A scribble closes the selection | done | `Selection::Stage` (help event 21) |
| Drawing an R repeats the last miracle called up, if within 40 seconds | partial | Step (d) with `CanRepeat` and `RepeatLastSpell` (`src/Worship/PlayerSpellIcons.cpp`); the 40 s cap is not applied (our wiki has not found who reads it) |
| A key opens the miracle selection, and another repeats the last miracle, as the gestures do | todo | No key bindings for the selection or the repeat (`src/Input/BindableActions.h`) |
| A miracle called up only becomes active in the hand after a short delay (1.5 seconds) | done | `delayBeforeSeedActive` in `seed::ProcessInHand` (`src/Magic/Core/SpellSeed.cpp`) |
| While the selection is open the gestures that can be drawn are shown as icons on the screen | todo | The `LookingFor` table is filled each pass (`PowerUpSystem.cpp`) but the HUD gesture icons are not drawn |
| The tutorial counts when the selection is opened and when a miracle is called up by gesture, to know the player has learnt it | done | Help events 15, 16 and 17 are counted in the help profile (`src/Help/HelpProfile.cpp`), which the scripts read through the help event functions in `src/CHLApi.cpp` |

## Other rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle's gestures only work inside the player's influence | partial | Only the shake-out tests the influence (`HandStatus::inInfluence`); the selection tests that the hand is ready, not the influence |
| Every recognised gesture starts a force-feedback effect on supporting mice | n/a | Force-feedback mice are not supported (the immersion calls are not ported) |
| The game counts the gestures each player has drawn for its statistics | todo | No game statistics |
| Rival gods controlled by the computer call up miracles by gesture or from their icons too | todo | No computer players |
