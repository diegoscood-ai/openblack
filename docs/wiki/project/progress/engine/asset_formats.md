# Asset formats

Every kind of file Black & White reads from its install, and whether openblack reads it and uses what is in it. A
format only counts as done when the game's use of it works too; reading the file alone is partial.

Rows reachable only with a force-feedback device have the status `device`, counted apart and in no score (user, 2026-10-27; [progress/README.md](../README.md#status)).

**Progress: 31/36 done, 0 chosen, 3 partial, 0 inferred, 1 device-only (not scored) — 91%**

How the original does it, in our wiki: [Tools and formats](../../../formats/land-and-script-files.md).

## World and lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Landscapes (`.lnd`): heights, blocks, countries and their materials, noise and bump maps | done | `components/lnd`, `LevelLoader` (`src/Resources/Loaders.cpp`); drawing and use: see ../terrain/; tests `LandData.CopiesALandscapeFilesBlocksAndMaps`, `DrawListBlocks.Land1BlocksInFileOrder` |
| Footpath files (`.fot`) that come with each land | done | `src/Serializer/FotFile.cpp`, used by `src/ECS/Footpaths.cpp`; walking them: see ../villager/; tests `FotFormat.*` (`test/test_fot_format.cpp`: a file written in the test read back field by field, and the story lands' own, Land 4's holding links and no footpath) |
| Land scripts (`Land1.txt` …) that build a land's towns, villagers, forests and features | done | `src/LHScriptX` (`FeatureScriptCommands.cpp`, `VillagerCommands.cpp`); 19 of the 105 commands were empty or logged "not implemented", counted 2026-10-28, 18 since fireflies came in, but every command the story lands' scripts use (`Land1.txt` to `Land5.txt`, `LandT.txt`) has its handler; a planned worship site makes nothing, as in the original, and 15 of the others (paths, creature pens, walls, pitches, temporary pots, country and height changes, furniture, a town's needs, fly-by files, brush size, the multiplayer debug, per-player land balance, the level editor) appear in no land script the game ships, so they are not progress. Two commands the playgrounds' scripts use, SET_A_TOWNS_INFLUENCE_MULTIPLIER and CREATE_BASE_WITH_ANGLE, are in no command table of the frozen exe, so their lines run nothing in the original, as in openblack ([not in the original](../../not-in-the-original.md#scriptsland_script_commands), 2026-11-07); the opening messages (START_GAME_MESSAGE, ADD_GAME_MESSAGE_LINE) have their handlers. The last command the playgrounds' scripts use, fireflies (CREATE_FIRE_FLY), now has its handler (`FeatureScriptCommands::CreateFireFly`, `ecs::PlaceFireFly`); the row is finished by [land_script_commands.md](../scripts/land_script_commands.md) "`CREATE_FIRE_FLY`: Creates a firefly", "`START_GAME_MESSAGE`: The title of a skirmish land", "`ADD_GAME_MESSAGE_LINE`: A line of a skirmish land", [on_screen_text.md](../interface/on_screen_text.md) "Game messages a land's feature script starts", [script_vm.md](script_vm.md) "Version, land number, landscape, starting camera" (CREATE_BASE_WITH_ANGLE has no row of its own), each done and read against the exe there; how faithfully each command builds its thing is on land_script_commands.md's own rows; tests in `test/test_land_script_reader.cpp`, `test_land_script_town_commands.cpp`, `test_land_script_mesh_commands.cpp`, `test_land_script_contents.cpp`; the commands one by one, and the map scripts, in script_vm.md |
| Camera exclusion zone files (`Data/Zones/*.exc`) | done | read by SET_CAMERA_ZONE (`player_camera::SetCameraZone`, `src/Camera/PlayerCameraScript.cpp`); keeping the camera out: see ../camera/; tests `CameraZoneSystem.AFilesZonesAreKeptToWhateverItSays`, `CameraZoneSystem.AFileThatCantBeReadLeavesNoZones` |
| Multiplayer and online maps (`Online Maps/*.map`, `.thm`) | pending | see ../multiplayer/ |

## Models and animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh pack (`AllMeshes.g3d`) with all object meshes and their textures | done | `components/pack`, `L3DLoader` (`src/Resources/Loaders.cpp`); tests `PackFormats.AMeshPackWrittenInTheTestGivesItsMeshesBackInOrder`, `PackFormats.realDataTheMeshPackHoldsEveryMeshAndItsTextures` (`test/test_pack_formats.cpp`) |
| Loose meshes (`.l3d`): creatures, the hand, temple rooms, misc objects, sun and moon | done | `components/l3d`, `L3DLoader`; tests `L3DSubmeshNames.ReadsEachSubmeshsRecord`, `L3DLightmaps.ReadsTheCoordinatesAndEachSubmeshsLightmap`, `CreatureRigsData.EverySpeciesMeasuresAsTheGameMeasuresIt` |
| Zipped meshes and files (`.zzz`) in the temple and spell folders | done | `src/Common/Zip.cpp`, `src/Resources/Loaders.cpp`; test `SpellSounds.realDataSynthetic` |
| The animation pack (`AllAnims.anm`) and loose animations (`.anm`) | done | `components/anm`, `L3DAnimLoader`; how each is played: see ../creature/, ../villager/, ../animal/; tests `PackFormats.ALooseAnimationWrittenInTheTestGivesItsKeyframesBack`, `PackFormats.realDataTheAnimationPackHoldsEveryAnimationWhole` (`test/test_pack_formats.cpp`) |
| Creature body files (`.cbn`) and their morph between good/evil and fat/thin | done | `components/morph`, the body blend in `src/3D/CreatureBody.cpp`; see ../creature/ for appearance; tests `MorphCreatureBlock.Version21HasActionPointsEyesTattooSitesAndBank`, `CreatureMorph.AlignmentIsTheEvilGoodAxis`, `CreatureMorph.FatnessRunsFromThinToFat` |
| Creature animation spec (`ctrspec27.txt`) naming the mind's animations | done | `CreatureRigLoader` reads `ctrspec27.txt` (`src/Resources/Loaders.cpp`), through the blob cache since 2026-10-26 ([the wiki](../../../creature/pose-and-animation.md#openblack)); tests `CreatureRigLoaderTest.AnimationsFollowTheSpecFile`, `CreatureRigLoaderTest.TheSpecFileGivenByItsNameGivesTheSameRig` |
| The hand's animation bank (`hh.hbn`) and its spec (`hndspec5.txt`) | done | `src/ECS/Systems/Implementations/HandSystem.cpp`, `src/3D/HandAnimator.cpp`; tests `MorphCreatureBlock.TheHandsFileHasNoCreatureBlock`, `SkeletalFromMorph.TheHandsClipFieldsReadTheSameBytes` |
| The mesh and sound-action name headers (`AllMeshes.h`, `SoundAction.h`) the particle files refer to | done | `EnumHeaderLoader` (`particles::EnumNames`, `src/Resources/Loaders.cpp`); test `SpellSounds.parseEnumHeader` |
| Recorded hand demonstrations (`Data/HandDemo/*.hnd`) played by the tutorial | done | `src/Hand/HandDemo.cpp` (PLAY_HAND_DEMO); see ../story/tutorial.md; tests `HandDemoRecords.ReadsEveryFieldOfEachRecordInOrder`, `HandDemoCache.TheResourcesKeepTheRecordsTheFileGives` |
| Dance and letter shape files (`.DAN`, `Scripts/Dance/*.dat`) | done | the dance files are read as the game reads them (`components/dance`, `DanceFileLoader`; tests `test_dance_file`, `DanceFile.TheGamesDancesRead`); the `.DAN` shape files too, once a dance needs them (`DanceShapeFile`, `DanceSystem::Shapes`, ported from raffclar PR #488, unverified against v1.42) |
| The falling-spell camera file (`.cm2`) | done | `falling_spell::CameraPath::Parse` (`src/Magic/Objects/FallingSpell.cpp`); test `FallingSpell.RealFallCm2`; its camera is not applied yet, see ../video/bink_videos.md |

## Images and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Raw textures (`.raw`, with a separate alpha file) | done | `components/rawimage`, `Texture2DLoader`; test `RawImage.ExactSizesDecode` |
| 16-bit images (`.16b`) used by the temple outside and sky | done | `src/Common/Bitmap16B.cpp`, used by `TempleExteriorSystem.cpp`; tests `Bitmap16BFormat.*` (`test/test_bitmap16b_format.cpp`: an image written in the test, and the temple's) |
| Sky images in 555 colour (`.555`) | done | `src/ECS/Systems/Implementations/SkySystem.cpp`; see ../sky/; test `Bitmap16BFormat.realDataTheSkyPicturesAre256Square` (`test/test_bitmap16b_format.cpp`: the nine pictures, 256 by 256; the 555 blend is the sky's own) |
| Player symbols and tattoo palette (`PlayersSymbols.raw`, `tattoocols.raw`) | done | `CreatureSkinArtLoader`, the player symbols in `src/Game.cpp`; tests `CreatureSkinArtLoaderTest.BlankSymbolCellsTakeTheDefaults`, `CreatureSkinArtLoaderTest.WithoutPlayersSymbolsEveryCellIsTheDefault` |
| Creature portrait sets (`.cps`) | partial 60% | a computer god's symbol cell is made from `Data/Textures/Lethis.cps`, `Kazarr.cps` or `nemesis.cps` (the player's number less one, Khazar's on Land 3 and Nemesis's on Land 5), each byte's top four bits in every channel, as the original's start of a game makes it (`player_symbol_sheet::MakeComputerSymbols` in `src/3D/PlayerSymbolSheet.cpp`, from `src/ECS/Systems/Implementations/LandSystem.cpp`; [computer hands](../../../hand/computer-hands.md)); tests `PlayerSymbolSheet.*` (`test/test_player_symbol_sheet.cpp`); what is missing: the low-resolution set (`LethisL.cps`, `KazarrL.cps`, `nemesisL.cps`, 32 by 32), which the original reads instead with its low-resolution textures on (detail levels 0 and 1, or the custom level without high textures), a setting openblack does not have; the colour lookup and contrast tables (`.clut`, `.cnv`) are read by nothing: [not in the original](../../not-in-the-original.md#engineasset_formats) |
| Light glows (`.glw`) placed in the temple | partial 40% | `LightLoader`, `src/3D/Light.cpp`; the temple interior's `.glw` files are loaded (`src/Simulation/WorldStart.cpp`) and the temple places its glows; see ../rendering/; tests `LightEmitter.*`, `LightBeams.*` (`test/graphics/test_light_beams.cpp`). The lands have no glows of their own in the original ([not-in-the-original.md](../../not-in-the-original.md#engineasset_formats)) |
| The game font (`.fnt` with its metrics `.met`) | done | `src/Graphics/GameFont.cpp` (`GameFontLoader`); test `test/test_game_font_loader.cpp` |
| The other fonts (`.fff`, `Font0-*.bmp`) | pending | not read. The original loads them at start-up (`start_system`): `Data/Font0-0.bmp` as its text library's font, and `SmlFont.fff`, `MedFont.fff`, `LrgFont.fff` (each a width, a height, 95 glyphs and their pixels); the small and middle fonts are drawn straight onto the screen by the debug button panel, the camera editor, a level-of-detail screen and the player text messages (football goals, network packets), whose drawing has no code caller; which of these a player sees in play is still undetermined (read 2026-11-07); `.srf` and `HelpFont.bmp` are named nowhere in the exe: [not in the original](../../not-in-the-original.md#engineasset_formats) |
| Help sprites (`Data/HelpSprite`) | done | the advisors' `.hd` files read by `help::LoadHelpDudeFile` (`src/Help/SpiritsRuntime.cpp`); test `test/test_help_dude_file.cpp`; see ../interface/ |
| Save pictures and screenshots kept in the profile (`.raw`) | partial 60% | see saving_and_loading.md; follows the row it points at: [saving_and_loading.md](saving_and_loading.md) "Each save keeps a picture of the moment", at 60% since its pictures' pages, files and the save's steps to them landed with their tests (2026-11-13; the temporary picture and a slot's pictures written in play: `refs/realinput/auto_save_box.log` lines 559-560) |

## Effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Particle effect files (`Data/Spells/ZSpellFiles`) | done | `src/Particles/ParticleSpellFile.cpp`, `ParticleFileLoader`; test `test/test_particle_file_loader.cpp`; effects themselves: see ../rendering/ |
| Gesture templates (`Gestures.jty`) | done | `GestureTemplatesLoader` (`src/Resources/Loaders.cpp`); test `test/test_gestures.cpp`; recognition: see ../gesture/ |
| Camera paths (`.cam`): temple rooms, symbols, fly-bys | done | `components/cam`, `CameraPathLoader`, `src/3D/CameraPath.cpp`; playing them: see ../camera/; tests `CameraPath.SamplesBetweenItsPointsAndHoldsTheLast`, `CameraPath.RefusesCamFilesShortOfTheirPoints` |
| Force-feedback mouse effects (`Data/Immersion/*.ifr`) | device | to port as the original's (user, 2026-10-19); only with a force-feedback device: the force-feedback mouse and its `.ifr` effects are set up only when `CImmMouse::Initialize` succeeds (0x5D86A0, the check at 0x5D870B), which it never does without one |

## Sound and video

The sound banks (`.sad`) with their sample tables, and the music stored in banks as MPEG audio, are one row each in
[../audio/sound_banks.md](../audio/sound_banks.md), where this page's own rows for them are merged.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The small sounds bank (`SmallSounds.SAS`) | done | `src/Audio/Services/AnimationSounds.cpp` (the clips' sound events); see ../audio/; tests `AnimationSoundsTest.OnlyTheSoundsTheClipPassesPlay`, `AnimationSoundsSyntheticTest.OnlyTheSoundsTheClipPassesPlaySynthetic` |
| Films (`.bik`): intro, the falling spell and others | done | `components/bink/src/BinkFile.cpp`, our own Bink decoder (`components/bink/src/BinkDecoder.cpp`); see ../video/; tests `BikFile.ParsesTheLayout`, `BikFile.GameFiles` |

## Scripts, text and minds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The compiled challenge script (`challenge.chl`) | done | loaded into the script VM by `src/Game.cpp`; running it: see script_vm.md; tests `ChlRoundTrip.ChallengeChlWithoutHeaders`, `LhvmProgram.WrittenProgramsReadBack` |
| The game's text in UTF-16 info scripts (`InfoScript2.txt` and its patch and multiplayer versions) | done | `src/Common/HelpText.cpp`, `src/Gui/TextDatabase.cpp`; tests `TextDatabase.ReadsTheAddTextLines`, `HelpTextLoader.ParsesTheTextsOfAScript` |
| The info table file (`info.dat`) | done | `src/Parsers/InfoFile.cpp`; see info_tables.md; test `AlignmentMusic.TownTriggerOfTheGameSynthetic` |
| Creature mind files (no extension in `Scripts/CreatureMind`, `.erc` for creatures saved between lands) | done | `components/creaturemind`, `CreatureMindLoader`, the `.erc` read by `--creature-file` (`src/Game.cpp`, `src/Creature/CreatureMindModel.cpp`); see saving_and_loading.md; tests `CreatureMindFile.CurrentVersionRoundTrips`, `CreatureMindLoaderTest.AWrittenMindIsReadBack` |
| Profile files: help statistics (`helpstats.dat`) | done | the help record's file (`helpstats.dat`, the 49 counts with no header, `components/helpstats`) is kept in the profile's own folder (`PlayerProfileSystem::CurrentPlayerFolder`) and read and written at the original's moments, read against the exe 2026-10-30 (`help::profile::KeepInProfile` / `TakeFromProfile` in `src/Help/HelpProfileFile.cpp`; tests `HelpStatsFile.TheProfilesFileIsReadAsFarAsItGoes`, `SaveGameSystem.ASaveKeepsTheHelpRecordAfterTheMindsAndBeforeTheScripts`; [when the file is read and written](../../../interface/advisors-and-help.md#timers-help-events-and-field-of-view)); the player's creature file (`creature.lhp`), which the original writes with the creature's web page and never reads back ([the creature's mind](../../../creature/mind.md)), is the online creature upload's archive: online, parked until the end (user, 2026-10-24), it has its own row in [online_services.md](../pc_integration/online_services.md#uploading-the-creature) "At the same time the creature is packed into one file" (split off 2026-11-09); see options_and_settings.md |
