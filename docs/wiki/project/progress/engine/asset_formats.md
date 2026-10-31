# Asset formats

Every kind of file Black & White reads from its install, and whether openblack reads it and uses what is in it. A
format only counts as done when the game's use of it works too; reading the file alone is partial.

Rows reachable only with a force-feedback device have the status `device`, counted apart and in no score (user, 2026-10-27; [progress/README.md](../README.md#status)).

**Progress: 29/38 done, 0 chosen, 3 partial, 0 inferred, 1 device-only (not scored) — 81%**

How the original does it, in our wiki: [Tools and formats](../../../formats/land-and-script-files.md).

## World and lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Landscapes (`.lnd`): heights, blocks, countries and their materials, noise and bump maps | done | `components/lnd`, `LevelLoader` (`src/Resources/Loaders.cpp`); drawing and use: see ../terrain/; tests `LandData.CopiesALandscapeFilesBlocksAndMaps`, `DrawListBlocks.Land1BlocksInFileOrder` |
| Footpath files (`.fot`) that come with each land | done | `src/Serializer/FotFile.cpp`, used by `src/ECS/Footpaths.cpp`; walking them: see ../villager/; tests `FotFormat.*` (`test/test_fot_format.cpp`: a file written in the test read back field by field, and the story lands' own, Land 4's holding links and no footpath) |
| Land scripts (`Land1.txt` …) that build a land's towns, villagers, forests and features | partial 90% | `src/LHScriptX` (`FeatureScriptCommands.cpp`, `VillagerCommands.cpp`); 19 of the 105 commands are still empty or log "not implemented", counted 2026-10-28, but every command the story lands' scripts use (`Land1.txt` to `Land5.txt`, `LandT.txt`) has its handler; a planned worship site makes nothing, as in the original, and 15 of the others (paths, creature pens, walls, pitches, temporary pots, country and height changes, furniture, a town's needs, fly-by files, brush size, the multiplayer debug, per-player land balance, the level editor) appear in no land script the game ships, so they are not progress. What is missing is what the playgrounds' scripts use: fireflies (CREATE_FIRE_FLY), the opening messages (START_GAME_MESSAGE, ADD_GAME_MESSAGE_LINE) and two commands with no handler (SET_A_TOWNS_INFLUENCE_MULTIPLIER, CREATE_BASE_WITH_ANGLE); tests in `test/test_land_script_reader.cpp`, `test_land_script_town_commands.cpp`, `test_land_script_mesh_commands.cpp`, `test_land_script_contents.cpp`; the commands one by one, and the map scripts, in script_vm.md |
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
| Recorded hand demonstrations (`Data/HandDemo/*.hnd`) played by the tutorial | done | `src/Input/HandDemo.cpp` (PLAY_HAND_DEMO); see ../story/tutorial.md; tests `HandDemoRecords.ReadsEveryFieldOfEachRecordInOrder`, `HandDemoCache.TheResourcesKeepTheRecordsTheFileGives` |
| Dance and letter shape files (`.DAN`, `Scripts/Dance/*.dat`) | done | the dance files are read as the game reads them (`components/dance`, `DanceFileLoader`; tests `test_dance_file`, `DanceFile.TheGamesDancesRead`); the `.DAN` shape files too, once a dance needs them (`DanceShapeFile`, `DanceSystem::Shapes`, ported from raffclar PR #488, unverified against v1.42) |
| The falling-spell camera file (`.cm2`) | done | `falling_spell::CameraPath::Parse` (`src/Magic/Objects/FallingSpell.cpp`); test `FallingSpell.RealFallCm2`; its camera is not applied yet, see ../video/bink_videos.md |

## Images and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Raw textures (`.raw`, with a separate alpha file) | done | `components/rawimage`, `Texture2DLoader`; test `RawImage.ExactSizesDecode` |
| 16-bit images (`.16b`) used by the temple outside and sky | done | `src/Common/Bitmap16B.cpp`, used by `TempleExteriorSystem.cpp`; tests `Bitmap16BFormat.*` (`test/test_bitmap16b_format.cpp`: an image written in the test, and the temple's) |
| Sky images in 555 colour (`.555`) | done | `src/ECS/Systems/Implementations/SkySystem.cpp`; see ../sky/; test `Bitmap16BFormat.realDataTheSkyPicturesAre256Square` (`test/test_bitmap16b_format.cpp`: the nine pictures, 256 by 256; the 555 blend is the sky's own) |
| Player symbols and tattoo palette (`PlayersSymbols.raw`, `tattoocols.raw`) | done | `CreatureSkinArtLoader`, the player symbols in `src/Game.cpp`; tests `CreatureSkinArtLoaderTest.BlankSymbolCellsTakeTheDefaults`, `CreatureSkinArtLoaderTest.WithoutPlayersSymbolsEveryCellIsTheDefault` |
| Creature portrait sets (`.cps`) and the colour lookup and contrast tables (`.clut`, `.cnv`) | pending | not read; the computer players' symbols use the symbol sheet instead (`src/Particles/TownBelief.cpp`), while the original copies a computer player's symbol from `Data/Textures/Lethis.cps` (or `LethisL.cps`) |
| Light glows (`.glw`) placed in the temple and lands | partial 40% | `LightLoader`, `src/3D/Light.cpp`; every `.glw` is loaded (`src/Game.cpp`) but only the temple places its glows; see ../rendering/; tests `LightEmitter.*`, `LightBeams.*` (`test/graphics/test_light_beams.cpp`) |
| The game font (`.fnt` with its metrics `.met`) | done | `src/Graphics/GameFont.cpp` (`GameFontLoader`); test `test/test_game_font_loader.cpp` |
| The other fonts (`.fff`, `.srf`, `Font0-*.bmp`, `HelpFont.bmp`) | pending | not read (unconfirmed which screens use them) |
| Help sprites (`Data/HelpSprite`) | done | the advisors' `.hd` files read by `help::LoadHelpDudeFile` (`src/Help/SpiritsRuntime.cpp`); test `test/test_help_dude_file.cpp`; see ../interface/ |
| Save pictures and screenshots kept in the profile (`.raw`) | pending | see saving_and_loading.md |

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
| Profile files: help statistics (`helpstats.dat`), the player's creature (`creature.lhp`) | partial 30% | the help record's file (`helpstats.dat`, the 49 counts with no header) is read and written by `components/helpstats` and converted by `src/Help/HelpProfileFile.cpp` (raffclar's PR #525, unverified against v1.42), but nothing reads or writes it yet, though each profile now has its own folder (`PlayerProfileSystem::CurrentPlayerFolder`); the player's creature file (`creature.lhp`) is not read; see options_and_settings.md |
| Weather and country tables (`weatherinfo.lhw`, `country.lhw`) | pending | not read; openblack takes its climates from `info.dat`. Read again 2026-10-28 in the frozen exe's decompilation: `weatherinfo.lhw` is opened only by the internet weather (its set-up for a game and its fetch), which is parked with the online work (see ../pc_integration/real_weather.md); the exe holds no `country.lhw` name or any other `.lhw` name, so whether the game reads it at all is still unconfirmed |
| Speech files (`Data/Language/blackhal.*`) | pending | not read (unconfirmed what they drive) |
