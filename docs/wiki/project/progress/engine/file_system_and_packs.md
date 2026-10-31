# File system and packs

How the game finds its files in the install, how it handles the paths its own scripts write, and the pack files that
bundle many meshes, animations, textures or sounds into one. Each file format is listed in
[asset_formats.md](asset_formats.md).

**Progress: 6/13 done, 0 chosen, 5 partial, 0 inferred — 66%**

## The install

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's files are found under its install folder: `Data`, `Scripts`, `Audio`, `Profiles`, `Plug Ins` and their sub-folders | done | `src/FileSystem/FileSystemInterface.h` (named game folders, `filesystem::Path`); test `GameInitialize.initializeOnly` |
| The install folder comes from the registry or the command line | done | `--game-path`, else the registry's GameDir, read only without it (`game_path::Choose`, `src/GamePath.cpp`, from `src/main.cpp`); tests `GamePath.*` (`test/test_game_path.cpp`); see options_and_settings.md |
| A folder's files can be listed, to load every creature mesh, texture, temple camera or land in it | done | `FileSystemInterface::Iterate`, used in `Game::Initialize`; test `CreatureRigsData.EverySpeciesMeasuresAsTheGameMeasuresIt` |
| The story's lands are the land scripts in `Scripts` | partial 60% | found by listing the folder (`Game::Initialize`); the lands should come from the challenge script |
| Extra lands put in `Scripts/Playgrounds` can be played as playgrounds | partial 60% | loaded into the level list (`Game::Initialize`); starting one is only through the command line or the debug tools ([openblack's debug controls](../../not-in-the-original.md#openblacks-debug-controls-kept-exempt-from-removal)) |
| Downloaded multiplayer lands go into `Online Maps` | pending | see ../multiplayer/ |
| The game writes only into the player's profile folder (saves, screenshots, statistics) | pending | openblack writes nothing to the install yet; see saving_and_loading.md |

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
| The game's text and speech are in the language of the install | partial 60% | whatever text the install has is used; choosing a language in the map script is pending |
