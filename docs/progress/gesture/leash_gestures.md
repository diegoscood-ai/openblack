# Leash gestures

The player puts the leash on their creature and picks which leash it wears by drawing gestures. What each leash does
to the creature is in [../creature/leash.md](../creature/leash.md).

**Progress: 12/15 done, 2 partial — 87%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

A scribble takes the leash off or closes the picker: see [scribble.md](scribble.md).

## Putting the leash on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The square spiral is the leash gesture, taken from the game's tables | done | `ProcessPowerUpSystem`'s last step (`src/Magic/Gestures/PowerUpSystem.cpp`) waits for `leashSelectionStart` (`LeashSelectionTables::FromInfo`) |
| The leash gesture is only waited for when the player has a creature that is not fighting and no picker is open | partial | The creature, the leash picker and the miracle selection are tested as the original; the creature's fight is not ported, so `fighting` is always false |
| It is also not waited for while the miracle selection is open | done | `SelectionOpenAndHandReady` in the same condition (`PowerUpSystem.cpp`) |
| An unleashed creature needs to know at least one leash, a leashed one at least two | done | `LeashStartWanted` (`PowerUpSystem.h`), tested in `test_gestures.cpp` (`LeashPicker.OpensOnlyWithMoreThanOneLeashKnown`) |
| On an unleashed creature the gesture puts the leash on | done | `LeashSelection::Start` sends the click on the creature's packet (`CreatureLeashClick`, 0x5F), so the leash goes on at the next game turn through the leash key, as the original |
| If the creature knows two or more leashes, the leash picker opens as well | done | `LeashSelection::Start` opens it with the leash-on pending, which the 0x5F handler clears (`creature_hand_packets::ApplyLeashClick`); tested in `LeashPicker.ThePendingLeashSurvivesUntilItsTurn` |
| On a creature already leashed, the gesture opens the leash picker | done | `LeashSelection::Start`; opening closes the miracle selection (`LeashPicker.OpeningClosesTheMiracleSelection`) |
| Recognising the leash gesture shows its trail on the land with the recognition sound | done | `gestures::Success(true)`, as every recognised gesture ([gesture_effects.md](gesture_effects.md)) |

## The leash picker

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The picker waits for the gesture of each leash the creature knows, except the one it wears | done | `LeashSelection::Stage` (`src/Magic/Gestures/PowerUpSystem.cpp`), the one picked or worn left out; tested in `LeashPicker.APickSendsTheLeashAndCloses` |
| The leash of aggression is a vertical scribble, the leash of learning an E, the leash of compassion a heart | done | `leashSelectionGestures` from info.dat (`LeashSelectionTables::FromInfo`) |
| Drawing a leash's gesture changes the creature's leash to it and closes the picker | partial | The pick sends the leash type's packet (0x65) as the original, so at the next game turn the temple's leash pick changes it (`temple_leash::ApplyLeashType`, refused under the compassion and anger spells). The original's message also wipes the local player's gesture strokes; ours does not (queued as a fix) |
| The picker closes by itself after 25 seconds | done | `LeashSelection::Stage`: strictly more than `leashSelectionSystemTimeOut`, not while paused, and that frame still looks for the leashes (`LeashPicker.TwentyFiveSecondsIsStrictAndThePassGoesOn`, `LeashPicker.APausedTimerStands`) |
| The picker closes when the leash comes off the creature | done | `LeashSelection::Active`, unless the leash-on is pending; a tied leash keeps it (`LeashPicker.ClosesWithoutACreatureOrWithTheLeashOff`). A charging seed and the miracle selection close it too (`FirstStage`) |
| While the picker is open, the gestures of the leashes on offer are shown as icons | todo | The `LookingFor` table gets the leashes' entries (types 4 to 6), but the HUD gesture icons are not drawn |
| Only the player who leashed the creature can change or remove the leash by gesture | n/a | Only one player owns a creature in openblack's single-player game |
| On the first land in a single-player game, drawing a square wave in the picker starts a hidden script | done | `LeashSelection::Stage` with `SecretScriptLand` (`src/Magic/Gestures/PowerUpSystem.h`): the pass wipes the gesture and starts `HiddenScript` through the script runtime, and goes on; tested in `test_gestures.cpp` (`LeashPicker.ASquareWaveStartsTheSecretScriptOnlyOnTheFirstSinglePlayerLand`, `ThePassGoesOnAfterTheSquareWave`, `ASquareWaveOutsideThePickerDoesNothing`) |
