# Key bindings

The game's actions and the keys and mouse buttons they are bound to, the Controls page that lists and rebinds them,
and the one-press shortcuts. What each camera or hand action does once pressed belongs to ../camera/ and ../hand/.

**Progress: 10/26 done, 11 partial — 60%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## The bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's default keys and mouse inputs for each of its 33 actions, in the order the options list them | done | `src/Input/KeyBindings.h` (`k_DefaultKeyBindings`); tests `KeyBindings.TableIsTheGamesActionMap`, `KeyBindings.EveryActionOnce`, `KeyBindings.DefaultsDoNotConflict` |
| A key can need a held Ctrl, Shift or Alt; either side's modifier counts | done | `src/Input/KeyBindings.cpp`; tests `KeyBindings.EitherSidesModifierKeyCounts`, `KeyBindings.EitherSideOfAModifier` |
| A binding with a modifier wins over the same key without one (Ctrl+S saves rather than showing details) | done | tests `KeyBindings.ModifierBindingWinsOverPlainOne`, `GameActionMap.ChordReplacesThePlainKey` |
| Letting go of a key ends every action bound to it | done | test `KeyBindings.LettingGoOfAKeyEndsAllItsActions` |
| Mouse buttons and the wheel can be bound to actions | done | test `KeyBindings.MouseMapsToActions` |
| Double clicks and both buttons together are read as their own inputs and can't be rebound | done | `UnbindableActionMap` in `src/Input/BindableActions.h`, `src/Input/GameActionMap.cpp` |
| Land scripts can block actions for a while, which then read as not held | partial | `help::interface_interaction` (SET_INTERFACE_INTERACTION, `src/Help/InterfaceInteraction.cpp`) has the control map's action gate (test `InterfaceInteractionTest.ControlMapActionGate`), but only the bookmark keys ask it; `GameActionMap` does not yet |
| The keys and buttons in use are told apart so the cursor is held still while the mouse turns the camera | partial | only the middle button holds the cursor (relative mouse mode in `Game.cpp`); no general rule for the inputs in use |

## The Controls page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists every action with its key and mouse button, named as the game names them | done | the Controls page lists the actions with their keys and mouse pictures (`GameMenu::BuildControls` in `src/Gui/GameMenu.cpp`) |
| Picking an action and pressing a key or button binds it anew | partial | `GameActionMap::SetKeyBinding` rebinds (test `GameActionMap.RebindingMovesTheAction`), but the Controls page cannot redefine an action yet and the debug Key Bindings window (`src/Debug/KeyBindingsWindow.cpp`) only shows them |
| Some keys can't be bound (the ones the game keeps for itself) | todo | (unconfirmed which) |
| Binding a key already in use takes it off the other action | partial | conflicts are found (`FindConflicts`, test `KeyBindings.ConflictsAreFound`, shown in the debug window) but nothing resolves them |
| Load Defaults puts every binding back | partial | `GameActionMap::ResetKeyBindings` exists but nothing calls it; the page's Load Defaults button does nothing |
| The bindings are kept with the player's profile and read back when it is picked | todo | there are no profiles |
| A key's name is written as the game writes it, such as "Ctrl+S" | done | test `KeyBindings.Names` |

## One-press shortcuts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| F1, Help: asks the advisors about what the hand is over | todo | F1 toggles the renderer's debug view (`Game.cpp`); see [help_system.md](help_system.md) |
| T, Talk: opens the chat line in a network game | todo | see ../multiplayer/ |
| Space flies the camera to the temple, and a second tap back | partial | `ZoomToPlaces::PressTemple` is ported and tested (`src/Camera/ZoomToPlaces.cpp`, tests `ZoomToPlaces.*`) but no key calls it |
| F3 flies the camera over the whole land, and back | partial | `ZoomToPlaces::PressRealm` is ported and tested but no key calls it |
| F4 to F9 take the player inside the temple, to each room | done | `Game::ProcessTempleRoomKeys` takes the player into the temple at the room, or cuts to it inside; see [../temple/](../temple/) |
| C flies to the creature and F5 to its room | partial | C is read by `CreatureModeSystem` (`src/ECS/Systems/Implementations/CreatureModeSystem.cpp`) and F5 by `Game::ProcessTempleRoomKeys`; no creature is in play on Lands 1 and 2 yet; see ../creature/ |
| N toggles villagers' names and S their details over them | partial | the names and details are drawn by the debug overlay (`Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`), switched from the debug menu; N and S are not handled |
| L leashes or lets go of the creature; V and B pick the previous or next leash | partial | `src/Creature/LeashKeys.cpp` reads L, V and B (tests `LeashKeys.*`); no creature is in play on Lands 1 and 2 yet; see ../creature/ |
| Ctrl+S quick saves and Ctrl+L quick loads | todo | nothing handles QUICK_SAVE or QUICK_LOAD; see ../engine/ |
| Number keys jump to camera bookmarks and Ctrl with a number sets one | done | `CameraBookmarkSystem`, the 1..8 keys in `Game.cpp` (skipped under a script's wide screen or a control map switch off); see ../camera/ |
| An action's key whose feature isn't built yet says so in the log | partial | openblack only: held actions are logged at debug level (`GameActionMap.cpp`); there is no "not built yet" message |
