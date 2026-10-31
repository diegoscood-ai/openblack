# Hidden keys and cheats

Keys the game reacts to that are not in its list of bindable actions, the developers' own key menus and cheats that were
left in the program but cut off from the player, and what remains of cheating in the online lobby. The ordinary,
rebindable keys are owned by [../interface/key_bindings.md](../interface/key_bindings.md); camera bookmarks on the
number keys by [../camera/places_and_bookmarks.md](../camera/places_and_bookmarks.md). Date-based surprises (special
days, the night voices, real weather, villagers named from the address book) are in [../pc_integration/](../pc_integration/).

**Progress: 0/5 done, 0 chosen, 1 partial, 0 inferred — 16%**

## Keys outside the bindings

These are tested only after a key has failed to match any bindable action, so rebinding an action onto one of them
hides it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| **Ctrl+Alt+T** saves a "creature snapshot": a file `creaturesnapshot.csn` in the current profile's folder, then plays a confirmation sound and a force-feedback jolt on a force-feedback mouse | pending | not in our tree; works in single player and online alike in the game; the force-feedback part (the jolt) is not ported (user, 2026-10-27, device): the exe starts every force-feedback effect through `GInterface::StartImmersion` (0x5D8A10), which returns at once while no force-feedback mouse was set up |
| The snapshot holds the camera's position and focus, the time of day and the sky it gives, the camera lens, whether the game is online (and the online game's name), then for each of up to eight human players who has a creature: their online player record, the creature's body and size, its head size, its colours, its whole 3D state, and the animation it is playing with its frame | pending | not in our tree; nothing in the game reads the file back |
| **R** (when unbound) repeats the last miracle, if the hand is free to take it | pending | no keyboard miracle keys in our tree (unconfirmed which of R and M is which in the game) |
| **M** (when unbound) starts choosing a miracle with the keyboard, if the hand is free to take it | pending | see the row above |
| **P** with no Shift, Ctrl or Alt held pauses and unpauses the game | partial 80% | P pauses and unpauses (`game_clock::Pause`, `src/Game.cpp`), but also with Shift, Ctrl or Alt held |

## Developer key menus left in the program

Four developers had their own debug key menus, each with a help page. The code is still in the released program, but
nothing calls it: no pointer to it and no call to it exists anywhere in the executable, so no profile name, switch or
key reaches it.

