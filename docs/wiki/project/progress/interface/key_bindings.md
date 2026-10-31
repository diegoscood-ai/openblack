# Key bindings

The game's actions and the keys and mouse buttons they are bound to, the Controls page that lists and rebinds them,
and the one-press shortcuts. What each camera or hand action does once pressed belongs to ../camera/ and ../hand/.

**Progress: 7/17 done, 0 chosen, 4 partial, 2 inferred — 49%**

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

## The Controls page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists every action with its key and mouse button, named as the game names them | partial 60% | the Controls page lists the actions with their keys and mouse pictures (`GameMenu::BuildControls` in `src/Gui/GameMenu.cpp`), the names tested (`KeyBindings.DefaultsMatchTheOptionsScreen`); the original's key binding box read for its layout, its items' text and its mouse pictures' place, which `List::Draw` now takes ([screen-and-readouts.md](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names), the Controls page's box); tests `GameMenu.ListsTheControls`, `GameMenu.AControlsMousePictureStandsAtItsItemsTopByTheTextBeforeItsGap`. Missing: whether the original's width measure scales with the screen (the picture's place at other screen sizes than the setup screens') is not read |
| Picking an action and pressing a key or button binds it anew | partial 20% | `GameActionMap::SetKeyBinding` rebinds in memory (test `GameActionMap.RebindingMovesTheAction`), but the Controls page cannot redefine an action yet and the debug Key Bindings window (`src/Debug/KeyBindingsWindow.cpp`) only shows them. The original's box takes the key with one Shift, Ctrl or Alt of either side |
| Some keys can't be bound (the ones the game keeps for itself) | pending | the original's binding box tests each key against a table of the keys it allows; the table has not been read yet |
| Binding a key already in use takes it off the other action | inferred | conflicts are found (`FindConflicts`, test `KeyBindings.ConflictsAreFound`, shown in the debug window) but nothing resolves them; the original's binding box puts up a message when the key is in use, and what its answer then does has not been read |
| Load Defaults puts every binding back | partial 10% | the page's Load Defaults asks first with its own label as the question, as the original's box does, and a yes calls `GameActionMap::ResetKeyBindings` (`Game::HandleInterfaceAction`; tests `GameMenu.LoadDefaultsAsksFirstWithItsOwnLabel`, `GameActionMap.RebindingMovesTheAction`; [the original's Load Defaults](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)); nothing in play rebinds a key yet, so the defaults are already in place; not proven through the real input path |
| The bindings are kept with the player's profile and read back when it is picked | pending | the original reads the current profile's keys (its control map's profile load); openblack keeps no bindings in `Profiles/Profiles.txt` |
| A key's name is written as the game writes it, such as "Ctrl + S" | partial 50% | the Controls page writes the modifier, " + " and the key, as the original's key text does, and so does `KeyChordName` (the debug window's names, test `KeyBindings.Names`); a binding has one modifier at most, so " + " is the only separator; the key names themselves are not compared with the original's |

## One-press shortcuts

The keys that move the camera or open a place are measured where they act: F1 in [help_system.md](help_system.md#asking-for-help), Space, F3, F4 to F9, the number keys and Tab in [../camera/places_and_bookmarks.md](../camera/places_and_bookmarks.md), C in [../camera/follow_cameras.md](../camera/follow_cameras.md), N and S in [on_screen_text.md](on_screen_text.md#other-overlays), and the middle button's held pointer in [../camera/world_camera_controls.md](../camera/world_camera_controls.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| T, Talk: opens the chat line in a network game | pending | see ../multiplayer/ |
| L leashes or lets go of the creature; V and B pick the previous or next leash | done | `src/Creature/LeashKeys.cpp` reads L, V and B as the original ([the leash keys](../../../creature/leash.md#the-leash-keys)); tests `LeashKeys.*`, `LeashKeysInputTest.*`, `LeashKeysPressed.*`; see ../creature/ |
| Ctrl+S quick saves and Ctrl+L quick loads | pending | nothing handles QUICK_SAVE or QUICK_LOAD; see ../engine/ |
