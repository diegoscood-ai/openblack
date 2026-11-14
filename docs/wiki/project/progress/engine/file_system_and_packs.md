# File system and packs

How the game finds its files in the install, how it handles the paths its own scripts write, and the pack files that
bundle many meshes, animations, textures or sounds into one. Each file format is listed in
[asset_formats.md](asset_formats.md).

**Progress: 8/13 done, 0 chosen, 3 partial, 0 inferred — 72%**

## The install

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's files are found under its install folder: `Data`, `Scripts`, `Audio`, `Profiles`, `Plug Ins` and their sub-folders | done | `src/FileSystem/FileSystemInterface.h` (named game folders, `filesystem::Path`); test `GameInitialize.initializeOnly` |
| The install folder comes from the registry or the command line | done | `--game-path`, else the registry's GameDir, read only without it (`game_path::Choose`, `src/GamePath.cpp`, from `src/main.cpp`); tests `GamePath.*` (`test/test_game_path.cpp`); see options_and_settings.md |
| A folder's files can be listed, to load every creature mesh, texture, temple camera or land in it | done | `FileSystemInterface::Iterate`, used in `Game::Initialize`; test `CreatureRigsData.EverySpeciesMeasuresAsTheGameMeasuresIt` |
| The story's lands are the land scripts in `Scripts` | partial 60% | found by listing the folder (`Game::Initialize`); the lands should come from the challenge script |
| Extra lands put in `Scripts/Playgrounds` can be played as playgrounds | done | loaded into the level list (`Game::Initialize`); since 2026-11-07 the skirmish box lists every `.txt` file of the folder each time it opens, an added land included, and starts the one picked as a playground, as the original's (`skirmish_box::ListLands`; tests `SkirmishBoxRules.*`; [the skirmish box](../../../interface/skirmish.md#the-skirmish-box), [../multiplayer/skirmish.md](../multiplayer/skirmish.md) "The list holds every `.txt` file"); the command line and the debug tools start one too ([openblack's debug controls](../../not-in-the-original.md#openblacks-debug-controls-kept-exempt-from-removal)); seen through the real input path (`refs/realinput/skirmish_menu_path.log`): the box lists the folder's 18 lands (line 208), Start Game on `Scripts/Playgrounds/TwoGods.txt` (210-211) loads it (244) and it is played until Leave Skirmish Game (255) |
| Downloaded multiplayer lands go into `Online Maps` | pending | see ../multiplayer/ |
| The game writes only into the player's profile folder (saves, screenshots, statistics) | pending | openblack writes nothing to the install yet; see saving_and_loading.md. The original writes outside the profile too: an empty `MemoryLeaks.txt` ("Memory leak log:") in the folder it runs from, at start-up (a static initialiser of its memory-leak log), and its screen grabs to `.\Screen Shots\B&W Screen %i.bmp` (`LHScreen::SaveBitmap`); the profile gets `weather.dat` (the internet weather) and the creature's `chat.log`; the row is to be rewritten from what the original writes where (read 2026-11-07) |

## Paths and letter case

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Paths in the game's scripts (backslashes, a leading `.\`, any mix of upper and lower case) still find their file | partial 50% | `FileSystemInterface::FixPath` fixes backslashes and the case of a few known folder names only |
| Any file is found whatever the case of its name, on file systems where case matters | partial 30% | `DefaultFileSystem::FindPath` checks the path as given; there is no general case-insensitive lookup |
| Names looked up in the game's tables (abodes, features, statics) ignore case, as the game's do | done | the info tables' lookups by name compare case-insensitively (`src/InfoConstants.cpp`); tests `InfoNames.*` (`test/test_info_names.cpp`) |

## Packs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is a file of named blocks, each read on its own | done | `components/pack`; tests `PackBankInfo.ASoundPackReadWholeHasItsSamples`, `PackBankInfo.TheThirdWordMarksAMusicBank` |

The mesh pack, the animation pack, the zipped files and the info table file are one row each in
[asset_formats.md](asset_formats.md), and the sound banks (their headers, their data, their animation tables and how
their waves are read) in [../audio/sound_banks.md](../audio/sound_banks.md); this page's own rows for them are merged
there.

## Patches and languages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The patch's text script is read after the game's, its texts replacing the game's of the same name | done | `helptext::GetPatch` (`src/Common/HelpText.cpp`), `src/Gui/TextDatabase.cpp`; test `TextDatabase.ReadsTheAddTextLines` |
| The game's text and speech are in the language of the install | done | whatever text the install has is used; the map script's LOAD_LANGUAGE does not choose it (it starts the chat plug-in, which no shipped map script asks for: [not in the original](../../not-in-the-original.md#engineasset_formats)); the original's sound banks and voice table have no language folder or switch either ([banks](../../../audio/banks-and-channels.md), [voices](../../../audio/voices.md)). Seen in the user's natural play on the Spanish install (2026-11-13): the voice table read from the install (`refs/story/played/u_20261108_192803.log` line 100), and a quest reminder shown in the install's Spanish with its voice sample from the install's bank (lines 1262-1263: text 4460, "Hay un Pergamino Dorado con Historia sobre el que tienes que hacer clic, Jefe.", voice bank 6 sample 827) |
