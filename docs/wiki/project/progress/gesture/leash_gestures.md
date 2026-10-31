# Leash gestures

The player puts the leash on their creature and picks which leash it wears by drawing gestures. What each leash does
to the creature is in [../creature/leash.md](../creature/leash.md).

**Progress: 15/15 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/gestures.md).

A scribble takes the leash off or closes the picker: see [scribble.md](scribble.md).

## Putting the leash on

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The square spiral is the leash gesture, taken from the game's tables | done | `ProcessPowerUpSystem`'s last step (`src/Magic/Gestures/PowerUpSystem.cpp`) waits for `leashSelectionStart` (`LeashSelectionTables::FromInfo`); test `LeashPicker.TheTablesGiveTheSquareSpiralTheTimeOutAndTheLeashesGestures` |
| The leash gesture is only waited for when the player has a creature that is not fighting and no picker is open | done | The creature, the leash picker and the miracle selection as the original; the fight is `CurrentFight().fighting`, the local player's creature duelling (`src/Magic/Gestures/PowerUpSystem.cpp`), the original's fight flag of the pass ([gestures](../../../magic/gestures.md#what-is-looked-for-and-when)); test `LeashPicker.TheFightIsTheLocalPlayersCreaturesDuel`; the gesture window shows the fight |
| It is also not waited for while the miracle selection is open | done | `SelectionOpenAndHandReady` in the same condition (`PowerUpSystem.cpp`) |
| An unleashed creature needs to know at least one leash, a leashed one at least two | done | `LeashStartWanted` (`PowerUpSystem.h`), tested in `test_gestures.cpp` (`LeashPicker.OpensOnlyWithMoreThanOneLeashKnown`) |
| On an unleashed creature the gesture puts the leash on | done | `LeashSelection::Start` sends the click on the creature's packet (`CreatureLeashClick`, 0x5F), so the leash goes on at the next game turn through the leash key, as the original; test `LeashPicker.OpensOnlyWithMoreThanOneLeashKnown` |
| If the creature knows two or more leashes, the leash picker opens as well | done | `LeashSelection::Start` opens it with the leash-on pending, which the 0x5F handler clears (`creature_hand_packets::ApplyLeashClick`); tested in `LeashPicker.ThePendingLeashSurvivesUntilItsTurn` |
| On a creature already leashed, the gesture opens the leash picker | done | `LeashSelection::Start`; opening closes the miracle selection (`LeashPicker.OpeningClosesTheMiracleSelection`) |
| Recognising the leash gesture shows its trail on the land with the recognition sound | done | `gestures::Success(true)`, as every recognised gesture ([gesture_effects.md](gesture_effects.md)); test `GestureSampling.OnlyARecognisedGestureLeavesATrailAndEitherWayThePathIsWipedForAPause` |

## The leash picker

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The picker waits for the gesture of each leash the creature knows, except the one it wears | done | `LeashSelection::Stage` (`src/Magic/Gestures/PowerUpSystem.cpp`), the one picked or worn left out; tested in `LeashPicker.APickSendsTheLeashAndCloses` |
| The leash of aggression is a vertical scribble, the leash of learning an E, the leash of compassion a heart | done | `leashSelectionGestures` from info.dat (`LeashSelectionTables::FromInfo`); test `LeashPicker.TheTablesGiveTheSquareSpiralTheTimeOutAndTheLeashesGestures` |
| Drawing a leash's gesture changes the creature's leash to it and closes the picker | done | The pick sends the leash type's packet (0x65) as the original, so at the next game turn the temple's leash pick changes it (`temple_leash::ApplyLeashType`, refused under the compassion and anger spells). Unless refused, the message also wipes the local player's gesture path, as the frozen exe's handler ([gestures](../../../magic/gestures.md#the-leash-picker)) (`GestureSystemInterface::ForgetPath`; `CitadelPlanTest.TheLocalPlayersNewLeashWipesTheGesturePath`) |
| The picker closes by itself after 25 seconds | done | `LeashSelection::Stage`: strictly more than `leashSelectionSystemTimeOut`, not while paused, and that frame still looks for the leashes (`LeashPicker.TwentyFiveSecondsIsStrictAndThePassGoesOn`, `LeashPicker.APausedTimerStands`) |
| The picker closes when the leash comes off the creature | done | `LeashSelection::Active`, unless the leash-on is pending; a tied leash keeps it (`LeashPicker.ClosesWithoutACreatureOrWithTheLeashOff`). A charging seed and the miracle selection close it too (`FirstStage`) |
| While the picker is open, the gestures of the leashes on offer are shown as icons | done | The leashes' entries of the table (types 4 to 6, with the leash) become gesture icons as every other ([gestures](../../../magic/gestures.md#the-gesture-icons)): `GestureIconList` and `StepIcons` (`src/Magic/Gestures/GestureIcons.h`), drawn by `Renderer::DrawGestureIcon`; tests `GestureIcons.*`. Inside each, the leash's mesh turning, as the original: the depth of the box above the icon reset, the leash collar's mesh in its band of the texture, its spin kept for each leash until the map is cleared and turned at each draw, drawn with a camera of its own and moved on the screen to that box (`src/Magic/Gestures/GestureIconModels.h`, `Renderer::DrawGestureIconModel`, program `ObjectIconInstanced`); tests `GestureIconModels.*` with the original's bits; the gesture window lists the icons and the model each shows |
| On the first land in a single-player game, drawing a square wave in the picker starts a hidden script | done | `LeashSelection::Stage` with `SecretScriptLand` (`src/Magic/Gestures/PowerUpSystem.h`): the pass wipes the gesture and starts `HiddenScript` through the script runtime, and goes on; tested in `test_gestures.cpp` (`LeashPicker.ASquareWaveStartsTheSecretScriptOnlyOnTheFirstSinglePlayerLand`, `ThePassGoesOnAfterTheSquareWave`, `ASquareWaveOutsideThePickerDoesNothing`) |
