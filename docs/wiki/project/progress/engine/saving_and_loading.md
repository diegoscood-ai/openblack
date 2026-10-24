# Saving and loading

Saving the whole game to a slot and loading it back: the world with every object in it, the players and their creature,
the towns, the scripts, the camera, the weather and the temple. Saves live in the player's profile and are made from the
temple's save game room, by the quick save keys, by scripts, and by an autosave. openblack has no save-game system yet.

**Progress: 2/37 done, 0 chosen, 11 partial, 0 inferred — 14%**

## Slots

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Games are saved into numbered slots kept in the player's profile folder, one folder per slot | partial 20% | the slots' folders (14 to 18 with a folder per land) and their files' names follow the game's rules (`components/gamesave` `SaveSlots`), checked on the frozen profiles (test `SavedGamesData.EveryWorldFileStartsAsTheGameWritesItWithTheGameObjectsSum`); the save service (`Locator::saveGame`, `SaveGameSystem`) finds them in the current profile's folder and reads each slot's info file and world file (tests `SaveGameSystem.*`), with no profile kept (a run of the checks) it has none; nothing saves into them yet |
| A slot holds the world file, a second file with the scripts' state, a small info file the save room shows (the slot, the date and time, the title), one mind file per creature and the land's map pictures | partial 30% | the info file and the world file's start are read and written in the game's layout (`components/gamesave` `SaveGameInfo`, `VanillaSaveFile`; tests `SavedGamesData.*` on every frozen save), the creatures' mind files too (`components/creaturemind`); the world file is a binary stream of every object, not land-script lines, and its objects are not read yet; nothing writes a slot; see [saved games](../../../engine/saved-games.md#the-files-of-a-slot-in-the-order-they-are-written) |
| Each save keeps a picture of the moment, at a high and a low resolution, shown on the save room's slot | partial 30% | the pictures go into pages shared by the slots, in the profile's saved-games pictures folder: two slots to a large-picture page, eight to a small one, each page a 256 by 256 compressed (DXT1) texture; the files, the slots' cells and the game's steps from a screen to them are in `components/gamesave` (`SavePictures`, `SavePictureMaker`; every frozen picture writes back byte for byte, test `SavedPicturesData.EveryPictureOfTheSavedGamesWritesBackByteForByte`); nothing takes the picture or saves it yet; see [saved games](../../../engine/saved-games.md#the-pictures) |
| The temple's save game room lists the slots and saves, loads and deletes, asking the player to confirm each | partial 10% | the room's scroll is drawn with made-up facts (`src/3D/TempleScrolls.cpp`); the room itself: see ../temple/; re-measured 2026-10-21: the scroll is drawn with placeholder facts; the room's slots and actions are not ported |
| Quick save and quick load keys save to and load from a slot without going to the temple | pending | bound but marked not yet implemented in `src/Input/KeyBindings.h` |
| A challenge script can save the game into a given slot | pending | SAVE_GAME_IN_SLOT is a stub in `src/CHLApi.cpp` |

## Autosave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With autosave on, the game saves itself every 6000 turns (ten minutes of play) | pending | the menu's autosave box (`src/Gui/GameMenu.cpp`) is not acted on |
| The autosaves of each land take turns among three slots, so an older one always survives | pending |  |
| There is no autosave in a network game, while paused, while help is showing a sequence, or on the Gods' Playground land (land 6) | pending | the periodic autosave also needs the options' autosave box; see [saved games](../../../engine/saved-games.md#start-up-the-autosave-and-the-load-attempts) and [../multiplayer/skirmish.md](../multiplayer/skirmish.md): a skirmish does autosave. Once the game is over the automatic save and the quick-save on exit stop: see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A save can be forced at once (for example when leaving a land) whatever the interval | pending |  |
| The game can tell whether its own automatic save is there and readable, and load it to continue | partial 20% | `SaveGameSystem::IsAutoSaveValid`, as the game's (the file is there and opens; its counter and revision are not looked at; test `SaveGameSystem.TheAutosaveIsValidWhenItsWorldFileIsThereWhateverItsCounter`); nothing asks it or loads yet |

## What a save holds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A version string and the land's landscape file come first; loading opens that landscape again | partial 50% | `Game::LoadSavedGame` opens the saved land's file through the game's own `LoadLandscape` (test `SavedGameLoad.TheFrozenLandOneAutosaveLoadsWithItsTurnSeedsAndTimeOfDay` pins the landscape read); reached only from the debug bar's Load Island / Saved Games menu |
| Every object in the world is saved, and the links between objects are put back once all are loaded | partial 15% | the world file is read as the game loads it, from the game object, depth first, each object at its first reference, as far as its first classes (`components/gamesave` `WorldReader`): the players, their computer players and statistics, the citadel, the footpaths, the storage pit and its piles, the town, the forests and the trees, the abodes, the creche, the graveyard and the fields, the villagers, the town centres, the flocks, the fish farms, the building sites, the totem statues, the workshops, the scaffolds, the reactions, the towns' desire flags, the animals, the birds, the planned buildings, the players' interfaces, the sub-actions, the creatures, the fires, the rocks, the spell seeds, the worship sites, the dances, the particles, the spell icons and the destruction meshes; the game object's global lists and 42 classes they meet, and the game object's tail; every frozen save and the two made by the original read to their last byte with every stored sum equal and write back byte for byte the same (`apps/savetool`), and nothing loads into play; a tree can be made with its saved growth, drawing nothing (`TreeArchetype::Create` with a `Growth`); a load makes the forests, the big forests and the trees again, in the order the original puts the links back, each tree with its type, place, size, growth counter and forest and each big forest with its forest and wood, keeping its record (`ecs::saved_world_load`; tests `SavedWorldLoad.*`: the frozen Land 1 autosave's 1,346 trees, 9 forests and 3 big forests), with the census of what was made on the debug bar |
| The players, which one is the player, and the player's creature with its state | partial 10% | the players' records, their computer players with their trees of desires and attitudes, and their statistics are read and written in the game's layout (`components/gamesave` `SavedRecords`); the creature's and nothing goes into play yet |
| The game turn, the time of day and its speed, and the land's influence multipliers | partial 60% | `ecs::saved_game_load::StateOf` reads them from the game object's record (the turn and the real turn, both seeds, the render clock, the two influence multipliers, the visual time, its speed and its cycle; tests `SavedGameLoad.*`, pinned on two frozen saves) and `Apply` puts them back in `GGame::Load`'s order (the time of day forced with the sky, then its speed and cycle: `DayNightClock::ForceVisualTime`, `SetScale`); reached only from the debug bar |
| The help system's progress and the player's help statistics | pending |  |
| The scripts: every running script, its variables and where it is | partial 40% | the script VM writes and reads its state as the game's script library does (`SaveState`, `RestoreState`, `LHVMFile` in `components/ScriptLibrary`): every frozen save's script file is read and written back byte for byte, and synthetic goldens pin each part; a saved game's load from the debug bar restores the machine from the slot's `Script_<slot>.sav` (`Game::LoadSavedGame`), nothing saves yet, and the scripts' object table in the world file is not read or written; see [saved games](../../../engine/saved-games.md#the-script-machines-file) |
| The camera, the land's starting camera position, camera help settings and the camera bookmarks | pending |  |
| The weather's climate and the snow lying on the land | pending |  |
| The hand and what it holds | pending |  |
| The audio's state | pending |  |
| The temple: which rooms are open, the challenge room's list, the hints already seen | pending |  |
| The land's good and evil balance | pending |  |
| Objects in flight keep their position, speed and spin | pending |  |
| Miracles under way keep their place, strength, age and effects | pending |  |
| The random number streams' positions | partial 50% | the streams can be saved and restored (test `GameRandom.SaveLoad`); no save game uses it; see game_loop_and_clock.md; re-measured 2026-10-21: the streams save and restore; no save game uses it |

## Writing and reading safely

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A save is written to a temporary file and only replaces the old one once it is complete | partial 40% | `SaveGameSystem::WriteFiles`: each file to `<name>.tmp`; when all are written, each old file is removed and its temporary file renamed over it, neither result looked at; a file that fails keeps every old file, and the first not opening writes nothing (tests `SaveGameSystem.ASavesFilesGoThroughTemporaryFilesThenTakeTheirNames`, `SaveGameSystem.AFileThatFailsKeepsEveryOldFile`); the file system service writes, renames and removes a player's file (`FileSystemInterface::WriteFile`, `Rename`, `Remove`); nothing saves yet |
| A problem while saving is reported to the player | pending |  |
| A save that failed to load twice (the game crashed loading it) is not offered again | partial 40% | `SaveGameSystem::BeginLoad` refuses a world file whose counter is 2 or more (a file too short for one reads as 1000) and else writes the counter + 1 into it before the load; `EndLoad` sets it back to 0; the slot's summary says whether a load would begin (tests `SaveGameSystem.ALoadCountsItsAttemptAndTheEndOfTheLoadSetsItBack`, `SaveGameSystem.AWorldFileTooShortForItsCounterIsRefused`); nothing loads yet |
| Loading stops the scripts and clears the land before reading | partial 50% | `Game::LoadSavedGame` restarts the scripts and clears the map through the land path (`PrepareNewLand` with `land_load::Resets` {scripts, map clear, not a new game}, `ResetRegistryForNewLand`); the computer players and the dead list are not handled apart yet; reached only from the debug bar |
| After loading, the game clock carries on from the saved turn | partial 60% | `Apply` sets the turn and starts the clock again at the turn's time (`game_clock::SetTurn`, `ResetLocalTimer`), and the load counter goes back to 0 (`SaveGameSystem::EndLoad`); reached only from the debug bar |
| A "saving" and "loading" box shows while the game works | pending |  |
| A save's stored checksum is not checked on load, and older versions' saves are read as they are | partial 30% | the original writes but does not check them; the stream reader keeps each stored sum without refusing a file for it, and the revision text is read, not checked (`components/gamesave` `ObjectStream`, `VanillaSaveFile`); nothing loads a save yet |

## Creatures outside a save

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature, with its mind and body, goes with the player into the next land | partial 40% | what a vortex swallows is written as land-script lines for the next land (`src/ECS/VortexSave.cpp`), but with no save folder nothing is read back; the player's creature comes only from `--creature-file`; see ../creature/; re-measured 2026-10-21: the vortex writes what it swallows; nothing reads it back |
| Creature mind files can be read and written in the game's format | done | `components/creaturemind`; tests `CreatureMindFile.CurrentVersionRoundTrips`, `CreatureMindFile.EncryptionRoundTrips`; a saved game's `SavedCreature<n>` files are such mind files, and all 45 of the frozen saves write back byte for byte (`creaturemindtool --roundtrip`) |
| A land's script can create a computer player's creature from a mind file | done | `FeatureScriptCommands::CreateCreatureFromFile` (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| The player can load a creature from a mind file | partial 50% | `--creature-file` names the mind file loaded when a land asks for the player's creature (`src/ECS/PlayerCreature.cpp`), and the debug creature spawner's file dialog (`src/Debug/CreatureSpawnerMindFiles.cpp`); no profiles; re-measured 2026-10-21: loaded from --creature-file and the debug spawner; no profiles |
