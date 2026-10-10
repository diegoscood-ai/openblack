# Asset formats

Every kind of file Black & White reads from its install, and whether openblack reads it and uses what is in it. A
format only counts as done when the game's use of it works too; reading the file alone is partial.

**Progress: 30/41 done, 2 partial — 76%**

How the original does it, in our wiki: [Tools and formats](../../bw1-notes/tooling.md).

## World and lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Landscapes (`.lnd`): heights, blocks, countries and their materials, noise and bump maps | done | `components/lnd`, `LevelLoader` (`src/Resources/Loaders.cpp`); drawing and use: see ../terrain/ |
| Footpath files (`.fot`) that come with each land | done | `src/Serializer/FotFile.cpp`, used by `src/ECS/Footpaths.cpp`; walking them: see ../villager/ |
| Land scripts (`Land1.txt` …) that build a land's towns, villagers, forests and features | partial | `src/LHScriptX` (`FeatureScriptCommands.cpp`, `VillagerCommands.cpp`); about 36 of the 106 commands are empty or log "not implemented", see script_vm.md |
| Map scripts that set up the game (player count, date, time, language, which land and scripts to load) | todo | `src/LHScriptX/MapScriptCommands.cpp` throws for every command and is never run |
| Camera exclusion zone files (`Data/Zones/*.exc`) | done | read by SET_CAMERA_ZONE (`player_camera::SetCameraZone`, `src/Camera/PlayerCameraScript.cpp`); keeping the camera out: see ../camera/ |
| Multiplayer and online maps (`Online Maps/*.map`, `.thm`) | todo | see ../multiplayer/ |

## Models and animation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh pack (`AllMeshes.g3d`) with all object meshes and their textures | done | `components/pack`, `L3DLoader` (`src/Resources/Loaders.cpp`) |
| Loose meshes (`.l3d`): creatures, the hand, temple rooms, misc objects, sun and moon | done | `components/l3d`, `L3DLoader` |
| Zipped meshes and files (`.zzz`) in the temple and spell folders | done | `src/Common/Zip.cpp`, `src/Resources/Loaders.cpp` |
| The animation pack (`AllAnims.anm`) and loose animations (`.anm`) | done | `components/anm`, `L3DAnimLoader`; how each is played: see ../creature/, ../villager/, ../animal/ |
| Creature body files (`.cbn`) and their morph between good/evil and fat/thin | done | `components/morph`, the body blend in `src/3D/CreatureBody.cpp`; see ../creature/ for appearance |
| Creature animation spec (`ctrspec27.txt`) naming the mind's animations | done | `CreatureRigLoader` reads `ctrspec27.txt` (`src/Resources/Loaders.cpp`) |
| The hand's animation bank (`hh.hbn`) and its spec (`hndspec5.txt`) | done | `src/ECS/Systems/Implementations/HandSystem.cpp`, `src/3D/HandAnimator.cpp` |
| The mesh and sound-action name headers (`AllMeshes.h`, `SoundAction.h`) the particle files refer to | done | `EnumHeaderLoader` (`psys::EnumNames`, `src/Resources/Loaders.cpp`) |
| Recorded hand demonstrations (`Data/HandDemo/*.hnd`) played by the tutorial | done | `src/Input/HandDemo.cpp` (PLAY_HAND_DEMO); see ../story/tutorial.md |
| Dance and letter shape files (`.DAN`, `Scripts/Dance/*.dat`) | todo | the dance files are not read (the real dances are not ported) |
| The falling-spell camera file (`.cm2`) | done | `falling_spell::CameraPath::Parse` (`src/Magic/Objects/FallingSpell.cpp`); test `FallingSpell.RealFallCm2`; its camera is not applied yet, see ../video/bink_videos.md |

## Images and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Raw textures (`.raw`, with a separate alpha file) | done | `components/rawimage`, `Texture2DLoader` |
| 16-bit images (`.16b`) used by the temple outside and sky | done | `src/Common/Bitmap16B.cpp`, used by `TempleExteriorSystem.cpp` |
| Sky images in 555 colour (`.555`) | done | `src/3D/Implementations/Sky.cpp`; see ../sky/ |
| Player symbols and tattoo palette (`PlayersSymbols.raw`, `tattoocols.raw`) | done | `CreatureSkinArtLoader`, the player symbols in `src/Game.cpp` |
| Creature portrait sets (`.cps`) and the colour lookup and contrast tables (`.clut`, `.cnv`) | todo | not read; the computer players' symbols use the symbol sheet instead (`src/Particles/TownBelief.cpp`) |
| Light glows (`.glw`) placed in the temple and lands | partial | `LightLoader`, `src/3D/Light.cpp`; every `.glw` is loaded (`src/Game.cpp`) but only the temple places its glows; see ../rendering/ |
| The game font (`.fnt` with its metrics `.met`) | done | `src/Graphics/GameFont.cpp` (`GameFontLoader`); test `test/test_game_font_loader.cpp` |
| The other fonts (`.fff`, `.srf`, `Font0-*.bmp`, `HelpFont.bmp`) | todo | not read (unconfirmed which screens use them) |
| Help sprites (`Data/HelpSprite`) | done | the advisors' `.hd` files read by `help::LoadHelpDudeFile` (`src/Help/SpiritsRuntime.cpp`); test `test/test_help_dude_file.cpp`; see ../interface/ |
| Save pictures and screenshots kept in the profile (`.raw`) | todo | see saving_and_loading.md |

## Effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Particle effect files (`Data/Spells/ZSpellFiles`) | done | `src/Particles/PSysFile.cpp`, `PSysFileLoader`; test `test/test_psys_file_loader.cpp`; effects themselves: see ../rendering/ |
| Gesture templates (`Gestures.jty`) | done | `GestureTemplatesLoader` (`src/Resources/Loaders.cpp`); test `test/test_gestures.cpp`; recognition: see ../gesture/ |
| Camera paths (`.cam`): temple rooms, symbols, fly-bys | done | `components/cam`, `CameraPathLoader`, `src/3D/CameraPath.cpp`; playing them: see ../camera/ |
| Force-feedback mouse effects (`Data/Immersion/*.ifr`) | n/a | hardware no longer made |

## Sound and video

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sound banks (`.sad`) with their sample tables | done | `src/Audio/Game/Banks.cpp`, `SoundLoader`; behaviour: see ../audio/ |
| Music stored in banks as MPEG audio | done | `src/Audio/Engine/MusicBank.cpp` with our own MPEG decoder (`src/Audio/Codec/MpegAudio.cpp`); see ../audio/ |
| Loose wave files (`.wav`) | n/a | our wave decoder reads them (`src/Audio/Codec/WaveFile.cpp`), but the game has none to read. Our wiki differs: the original plays no loose wave files, every sound is in a bank ([audio](../../bw1-notes/audio.md#sas-and-animation-tables)) |
| The small sounds bank (`SmallSounds.SAS`) | done | `src/Audio/Services/AnimationSounds.cpp` (the clips' sound events); see ../audio/ |
| Films (`.bik`): intro, the falling spell and others | done | `src/Video/BikFile.cpp`, our own Bink decoder (`src/Video/BinkDecoder.cpp`); see ../video/ |

## Scripts, text and minds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The compiled challenge script (`challenge.chl`) | done | loaded into the script VM by `src/Game.cpp`; running it: see script_vm.md |
| The game's text in UTF-16 info scripts (`InfoScript2.txt` and its patch and multiplayer versions) | done | `src/Common/HelpText.cpp`, `src/Gui/TextDatabase.cpp` |
| The info table file (`info.dat`) | done | `src/Parsers/InfoFile.cpp`; see info_tables.md |
| Creature mind files (no extension in `Scripts/CreatureMind`, `.erc` for creatures saved between lands) | done | `components/creaturemind`, `CreatureMindLoader`, the `.erc` read by `--creature-file` (`src/Game.cpp`, `src/Creature/CreatureMindModel.cpp`); see saving_and_loading.md |
| Profile files: help statistics (`helpstats.dat`), the player's creature (`creature.lhp`) | todo | see options_and_settings.md |
| Weather and country tables (`weatherinfo.lhw`, `country.lhw`) | todo | not read; openblack takes its climates from `info.dat` (unconfirmed whether the game reads these at all) |
| Speech files (`Data/Language/blackhal.*`) | todo | not read (unconfirmed what they drive) |
| The script library and language plug-ins (`Plug Ins/*.dll`) | n/a | openblack builds the script VM in |
