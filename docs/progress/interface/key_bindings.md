# Key bindings

The game's actions and the keys and mouse buttons they are bound to, the Controls page that lists and rebinds them,
and the one-press shortcuts. What each camera or hand action does once pressed belongs to ../camera/ and ../hand/.

**Progress: 11/26 done, 0 chosen, 8 partial, 2 inferred — 57%**

How the original does it, in our wiki: [The one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names), [the speed keys](../../../engine/game-clock.md#the-speed-keys) and [the control map's switches](../../../hand/picking.md).

## The bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's default keys and mouse inputs for each of its 33 actions, in the order the options list them | done | `src/Input/KeyBindings.h` (`k_DefaultKeyBindings`), the original's default table slot by slot ([the action table](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)); tests `KeyBindings.TableIsTheGamesActionMap`, `KeyBindings.EveryActionIsInTheTableOnce`, `KeyBindings.DefaultsDoNotConflict`, `KeyBindings.DefaultsMatchTheOptionsScreen` |
| A key can need a held Ctrl, Shift or Alt; either side's modifier counts | done | `src/Input/KeyBindings.cpp`; the original matches the key and its one modifier, left and right alike ([the speed keys](../../../engine/game-clock.md#the-speed-keys)); tests `KeyBindings.EitherSidesModifierKeyCounts`, `KeyBindings.EitherSideOfAModifier` |
| A binding with a modifier wins over the same key without one (Ctrl+S saves rather than showing details) | done | the original passes one modifier with the key and a binding matches only with its own modifier; tests `KeyBindings.ModifierBindingWinsOverPlainOne`, `GameActionMap.ChordReplacesThePlainKey` |
| Letting go of a key ends every action bound to it | inferred | test `KeyBindings.LettingGoOfAKeyEndsAllItsActions`; this is raffclar's rule: the original's key path for a let-go (and for a modifier changing while a key is held) has not been read ([debugging](../../debugging.md#key-bindings)) |
| Mouse buttons and the wheel can be bound to actions | done | the original maps a mouse button to its action as it maps a key ([the control map](../../../hand/picking.md)); test `KeyBindings.MouseMapsToActions` |
| Double clicks and both buttons together are read as their own inputs and can't be rebound | done | `UnbindableActionMap` in `src/Input/BindableActions.h`, `src/Input/GameActionMap.cpp`; the original's left double click is an event of its own, outside the 33 actions ([hand states](../../../hand/states.md)); test `GameActionMap.ADoubleClickChangesOnlyOnTheFrameItHappens` |
| Land scripts can block actions for a while, which then read as not held | done | SET_INTERFACE_INTERACTION's control map switches (`help::interface_interaction`) tell the action map (`GameActionMap::SetBlockedActions`, from `src/Game.cpp`), asked at every query as the original's ([the control map's switches](../../../hand/picking.md)); tests `GameActionMap.BlockedActionsReadAsNotHeld`, `InterfaceInteractionTest.ActionMapIsToldOnSet`, `InterfaceInteractionTest.ControlMapActionGate` |
| The pointer is held still while the middle button, or both buttons, turn the camera | done | the input's cursor freeze (`GameActionMap::AllowCursorFreeze`, switched on by `Game.cpp` for the player's camera), as the original's ([player camera: the mouse](../../../camera/player-camera.md#player-camera-the-mouse)); tests `GameActionMapCursor.*` (`test/input/test_cursor_freeze.cpp`) |

## The Controls page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists every action with its key and mouse button, named as the game names them | partial 60% | the Controls page lists the actions with their keys and mouse pictures (`GameMenu::BuildControls` in `src/Gui/GameMenu.cpp`), the names tested (`KeyBindings.DefaultsMatchTheOptionsScreen`); the original's key binding box (a box of its own) has not been read for its layout, and no test reads the page's list |
| Picking an action and pressing a key or button binds it anew | partial 20% | `GameActionMap::SetKeyBinding` rebinds in memory (test `GameActionMap.RebindingMovesTheAction`), but the Controls page cannot redefine an action yet and the debug Key Bindings window (`src/Debug/KeyBindingsWindow.cpp`) only shows them. The original's box takes the key with one Shift, Ctrl or Alt of either side |
| Some keys can't be bound (the ones the game keeps for itself) | pending | the original's binding box tests each key against a table of the keys it allows; the table has not been read yet |
| Binding a key already in use takes it off the other action | inferred | conflicts are found (`FindConflicts`, test `KeyBindings.ConflictsAreFound`, shown in the debug window) but nothing resolves them; the original's binding box puts up a message when the key is in use, and what its answer then does has not been read |
| Load Defaults puts every binding back | partial 10% | `GameActionMap::ResetKeyBindings` exists but nothing calls it, and it has no test; the page's Load Defaults button does nothing. The original asks first in a box, then loads the defaults |
| The bindings are kept with the player's profile and read back when it is picked | pending | the original reads the current profile's keys (its control map's profile load); openblack keeps no bindings in `Profiles/Profiles.txt` |
| A key's name is written as the game writes it, such as "Ctrl + S" | partial 50% | the Controls page writes the modifier, " + " and the key, as the original's key text does; but `KeyChordName` (the debug window's names, test `KeyBindings.Names`) writes "Ctrl+S" with no spaces, a difference from the original to fix; the key names themselves are not compared with the original's |

## One-press shortcuts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F1, Help: asks the advisors about what the hand is over | done | `input::ShortcutKeys` reads the Help action (the original's slot 0); test `ShortcutKeys.TheHelpKeyAsksTheAdvisors`; the renderer's debug view moved to F11 (`Game.cpp`, and the debug bar's View menu); see [help_system.md](help_system.md) |
| T, Talk: opens the chat line in a network game | pending | see ../multiplayer/ |
| Space flies the camera to the temple, and a second tap back | partial 70% | `input::ShortcutKeys` and `ZoomToPlaces::PressTemple` (tests `ShortcutKeys.TheTempleKeyZoomsToTheTemple`, `ShortcutKeys.ASingleTapDoesNotMoveTheCameraWatchingAFight`, `ZoomToPlaces.*`), blocked under a script's bars, inside the temple and by the control map's switches, as the original; not as the original yet: the single tap's point, the flights' times, the key repeat, the camera mode switch ([the one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)) |
| F3 flies the camera over the whole land, and back | partial 70% | `ZoomToPlaces::PressRealm` from `input::ShortcutKeys` (tests `ShortcutKeys.TheRealmKeyZoomsOverTheRealm`, `ShortcutKeys.TheRealmIsHalfwayBetweenTheFirstAndLastBlocks`, `ZoomToPlaces.TheRealmKeyFliesOverTheRealmAndBack`); the flight's time and the camera mode switch are not as the original yet |
| F4 to F9 take the player inside the temple, to each room | done | `Game::ProcessTempleRoomKeys` takes the player into the temple at the room, or cuts to it inside; the original's citadel takes only these room keys; tried by the temple testbed scenarios (`src/Debug/TestbedScenarioTemple.cpp`); see [../temple/](../temple/) |
| C flies to the creature and F5 to its room | done | C is read by `CreatureModeSystem` (`src/ECS/Systems/Implementations/CreatureModeSystem.cpp`; tests `CreatureModeSystemTest.CLocksOntoThePlayersCreatureAndLetsGoAgain`, `CreatureModeSystemTest.WithoutALookupNoPlayerHasACreatureAndCDoesNothing`; [the C key](../../../creature/overview.md)) and F5 by `Game::ProcessTempleRoomKeys`; see ../creature/ |
| N toggles villagers' names and S their details over them | partial 40% | N toggles the names (`input::ShortcutKeys`, test `ShortcutKeys.TheNamesKeyTogglesTheNames`), but they are drawn by the debug overlay (`Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`), not as the game's name list; S (the original toggles the hovered villager's details) is not handled ([the one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)) |
| L leashes or lets go of the creature; V and B pick the previous or next leash | done | `src/Creature/LeashKeys.cpp` reads L, V and B as the original ([the leash keys](../../../creature/leash.md#the-leash-keys)); tests `LeashKeys.*`, `LeashKeysInputTest.*`, `LeashKeysPressed.*`; see ../creature/ |
| Ctrl+S quick saves and Ctrl+L quick loads | pending | nothing handles QUICK_SAVE or QUICK_LOAD; see ../engine/ |
| Number keys jump to camera bookmarks and Ctrl with a number sets one | partial 50% | `CameraBookmarkSystem`, the 1..8 keys in `Game.cpp` (skipped under a script's wide screen or a control map switch off); no test, and the flight is not compared with the original's ([player camera](../../../camera/player-camera.md)); see ../camera/places_and_bookmarks.md |
| Tab flies the camera to the player's next town, Shift+Tab and Ctrl+Tab step to the previous and the next bookmark | pending | the original's key handling ([player camera](../../../camera/player-camera.md)): the next town after the last one focused, at its storage pit or else its town centre, setting bookmark 8 there; nothing in openblack reads Tab |
