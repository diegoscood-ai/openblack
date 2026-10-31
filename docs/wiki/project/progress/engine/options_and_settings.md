# Options and settings

Where the game keeps what the player chose and who the player is: the install's settings in the Windows registry, the
graphics setup, the options set from the menu, the player profiles with their own folders, and the key bindings. The
options screens themselves are in [../interface/](../interface/).

**Progress: 4/17 done, 0 chosen, 10 partial, 0 inferred — 55%**

## Install and setup

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The setup program's graphics choices (screen mode, resolution, device, vertical sync) are kept in the registry and used at start | partial 40% | openblack takes them from its command line (`-W`, `-H`, `--window-mode`, `--backend-type`, `--no-vsync` (on by default, as the original)) and does not read the registry's |
| A sound override setting in the registry changes how the audio starts (unconfirmed what it sets) | pending | not read; see ../audio/ |

## Options

The sound effects and music volumes, applied from the menu and kept between runs, are the volume rows of
[../audio/sound_effects.md](../audio/sound_effects.md) and [../audio/music.md](../audio/music.md); the install folder
found through the registry is a row of [file_system_and_packs.md](file_system_and_packs.md). This page's own rows for
them are merged there.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu's options (help level, story text, tooltips, films) are read from the registry at start and written back when changed | partial 70% | help level, story text and tooltips are kept with the player's profile, as the exe keeps them, and read back when it is picked (`player_profiles::Settings`, `gui::ToProfileSettings`; see ../interface/options.md); the films are not among them |
| The detail level chosen in the menu takes effect the next time a land loads, and is kept | partial 60% | (`GameMenu.cpp`); `--detail-level` sets it at start; re-measured 2026-10-21: the Options page sets the level as it is left; it is not kept between runs |
| Each detail level turns parts of the drawing on and off (water tiling, clouds, fog, weather and the rest of the table) | partial 80% | water tiling, land reflection, clouds, fog and the hand shadow on objects follow the level (`src/Graphics/DetailLevel.h`); the Weather column is read once at start-up by `weather::StartAtmos` (no rain or falling snow when off) and every frame by the sky dome; the atmosphere's screen effect it also gates is not read yet (../../../world/weather.md#pending); see ../rendering/ |
| Autosave and push scrolling are options kept with the others | partial 30% | boxes in the menu (`MenuSettings::autoSave`, `pushScrolling`), kept with the player's profile (`player_profiles::Settings`); push scrolling is read by nothing, as the camera has no edge scroll yet; autosave itself: see saving_and_loading.md |
| Left- or right-handed hand is an option of the player | partial 50% | a box in the menu (`MenuSettings::leftHandedHand`), written into the profile's settings as the Options page is left (`player_profiles::Settings::leftHanded`, `gui::ToProfileSettings`); the hand takes the side the profile keeps only as a page shows after that leave, the box's tick alone changing nothing (`EngineConfig::rightHandedHand`, set in `Game::HandleInterfaceAction`, read by `HandGiving.cpp` and `HandPlacement.cpp`; test `GameMenu.TheLeftHandedBoxReachesTheHandOnlyAsAPageShowsAfterTheLeave`); the hand's mirrored drawing is not checked against the original |

## Player profiles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There can be several players, each with a name, a symbol and a creature, created, chosen and deleted on the Players page | partial 50% | the profiles' store (`PlayerProfileSystem`, `src/Common/PlayerProfiles.*`; tests `PlayerProfiles.*`, `PlayerProfileSystem.PickingDeletingAndEditingProfilesIsKept`) made, picked and deleted from the Players page; the page's buttons are not read against the exe, see ../interface/profiles.md |
| The player chosen last is remembered and greeted next time | done | the current profile is kept in `Profiles/Profiles.txt` and picked at start (`PlayerProfileSystem`; test `PlayerProfileSystem.PickingDeletingAndEditingProfilesIsKept`), and the menu greets the player by its name (`src/Game.cpp`); a text file in place of the registry, see ../interface/profiles.md |
| Each player has a folder of their own for saved games, the current land's screenshots, help statistics and their creature | partial 50% | each profile has its own folder (`PlayerProfileSystem::CurrentPlayerFolder`; test `PlayerProfileSystem.EachProfileKeepsItsFilesInAFolderOfItsOwn`) holding its creature's mind (`player_creature::CurrentMindFolder`) and its saved games (the save game system's folder, `src/Locator.cpp`); the screenshots and help statistics are not kept there |
| Profile folder names are written so that upper and lower case survive on any file system | done | `player_profiles::ProfileKey`, the game's own rule (a plain letter or digit after an underscore, anything else as four lower-case hexadecimal digits); tests `PlayerProfiles.AProfilesFolderIsNamedByItsKey`, `PlayerProfiles.AProfilesFolderIsNamedAsTheGamesOwnProfileFolders` |
| The help the player has already seen is saved with their profile so it isn't shown again | pending | see ../interface/ |
| The player's symbol and creature name are kept with their profile | done | edited on the Players page and kept with the profile (`PlayerProfileSystem::EditCurrentProfile`, `player_profiles::Edit`; tests `PlayerProfileSystem.PickingDeletingAndEditingProfilesIsKept`, `PlayerProfiles.TheCreaturesNameIsThePlayersUntilOneIsGivenAndTheSymbolOneOfSixteen`); the world's symbols don't use it yet, see ../interface/profiles.md |
| Whether the current profile already has a creature decides if the game starts at the creature choice | partial 80% | CURRENT_PROFILE_HAS_CREATURE answers from the profile's creature file (`PlayerProfileSystem::CurrentProfileHasCreature`, `player_creature::ProfileHasCreature`; test `test/creature/test_player_creature.cpp`); the story's use of it: see ../story/ |

## Key bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game action has a default key or mouse binding | done | `src/Input/KeyBindings.cpp`; test `test/input/test_key_bindings.cpp` |
| The player can rebind an action and set the defaults back | partial 30% | the action map can rebind an action and set the defaults back (`GameActionMap::SetKeyBinding`, `ResetKeyBindings`, `src/Input/GameActionMap.cpp`; test `GameActionMap.RebindingMovesTheAction`), but nothing in the game calls them: the menu's Controls page (the original's key binding box) lists the default bindings and cannot change them (`GameMenu::BuildControls`, `src/Gui/GameMenu.cpp`), and the key bindings debug window only shows them (`src/Debug/KeyBindingsWindow.cpp`); re-measured 2026-10-28 (was done); see ../interface/ |
| Bindings are kept between runs | pending | held in memory only |
