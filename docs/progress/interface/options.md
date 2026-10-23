# Options

The options' pages of the menu: sound and video, the advanced page for help, text, tooltips and the hand, and how the
settings take effect and are kept. The player's page is in [profiles.md](profiles.md) and the controls page in
[key_bindings.md](key_bindings.md).

**Progress: 5/23 done, 0 chosen, 15 partial, 1 inferred — 66%**

How the original does it, in our wiki: [The in-game menu's options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause).

## The pages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The options are tabs of one box: Options, Players, Advanced and Controls, with a Main Menu tab and a Back arrow to the first page | done | `GameMenu::OptionsTabs`; test `GameMenu.OptionsOpenTheOptionsTabs`; the exe's options pages are separate boxes sharing their tabs (the sound and video box, the advanced box, the profile editor and the key bindings box) |
| A Quit arrow on the options page | partial 70% | `GameMenu.cpp`; test `GameMenu.SelectorsAndCheckBoxesChangeTheSettings` asks to quit from the Options page; the exe's Options page has one button of its own besides its arrows (the sound and video box), not read further |
| With the mod loader and a read mods folder, a sixth tab, Mods, lists the mods (name, version, author) with a switch each, kept in the mods folder's enabled.txt; the user's choice of 2026-10-21, not the original's | chosen | `GameMenu::BuildMods`, `MenuModsFromLoader`, `ModLoader_ListMods` and `ModLoader_SetModEnabled`; tests `GameMenu.GivenTheModsASixthTabListsThemWithASwitchEach`, `ModLoaderTest.*`, `ModManifest.*`; not yet seen in a run; see [mod-loader](../../mod-loader.md#the-minimal-mod-loader) |
| A switched-on mod's files replace the game's files of the same path, the first mod in the switches' order winning, from the next start; the user's choice of 2026-10-22, not the original's | chosen | `src/FileSystem/ModSearchRoots.h`, `DefaultFileSystem::FindPath`, `ModLoader_EnabledModFolders`; tests `ModSearchRoots.*`, `ModLoaderTest.TheEnabledModsFoldersAreGivenInTheListsOrder` |
| The options room inside the temple shows a small version of the same settings | partial 50% | the options room opens the menu's Options page once the camera is in the room, and goes back to the main room when it closes (`TempleInterior::UpdateOptionsAndFutureRooms`); whether the room uses another box is not read; see ../temple/ |

## Sound and video

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Effects volume slider changes the sound effects' volume at once | done | `Game::HandleInterfaceAction` sets `audioSampleMainVolume` (value x 127, truncated) from the slider at once, as the exe ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); tests `GameMenu.SlidersStepOrFollowTheKnob`, `MachineSetup.TheVolumesAreTakenAsTheAudioTakesThem` |
| Music volume slider changes the music's volume at once | done | same, `audioMusicMainVolume` |
| The menu starts with the volumes the game is playing at | partial 90% | `Game::CreateInterface` fills `MenuSettings` from the configuration's main volumes / 127 and the running detail level, as the exe's sound and video box does ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); no test of the fill itself |
| Detail level selector from minimum to maximum detail, stepped by its button and arrows | done | the selector (`GameMenu.cpp`), the entry shown from the running level (`graphics::detail_level::MenuEntryOf`); tests `GameMenu.SelectorsAndCheckBoxesChangeTheSettings`, `DetailLevel.TheOptionsBoxTopEntryIsTheTopLevel`, `DetailLevel.TheOptionsBoxShowsMaximumFromLevelFourUp` |
| A changed detail level takes effect the next time the game starts, and the game says so | partial 50% | the message is as the exe's (`GameMenu::TellVideoChange`, HELP_TEXT_DIALOG_VIDEOCHANGE, [the detail change message](../../../video/playback.md#the-detail-change-message)); but openblack applies the level as the Options page is left (`graphics::detail_level::LevelAfterOptions`, test `DetailLevel.MaximumIsTheTopLevelAndTheOthersChangeOnlyWhenStepped`), where the exe keeps it for the next start |
| Auto save check box turns the land's automatic saves on or off | partial 30% | the box works and is kept with the profile (test `GameMenu.SelectorsAndCheckBoxesChangeTheSettings`); nothing reads it: there are no automatic saves yet (the exe's is a bit of the game's flags) |
| Push scrolling check box makes the camera move when the hand pushes against the screen's edge | partial 30% | the box works and is kept with the profile; nothing reads it (the exe's is a profile value the camera reads) |

## Advanced

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help level from no help to all help sets how often the advisors offer help | partial 80% | the selector sets the help system's level at once and it is kept with the profile, read back at start and after each help system's start (`Game::ApplyPlayerSettings`, `help::HelpSystem::SetHelpLevel`), 3 without a profile value as the exe ([advisors and help](../../../interface/advisors-and-help.md)); no test of the menu reaching the help system; what each level gates is in [help_system.md](help_system.md) |
| Story text: none, story only, or all text shown on screen as people speak | partial 80% | the selector sets the dialogue's TEXT_DRAW at once and it is kept with the profile (`Game::ApplyPlayerSettings`, `help::DialogueText::SetPlayerChoices`), 1 without a profile value as the exe ([advisors and help](../../../interface/advisors-and-help.md)); tests `HelpTextDisplay.ResetCloseAndGate` (the gate); no test of the menu reaching it |
| Tooltips: none, minimum, intelligent or all | partial 80% | the selector sets the tooltips' level at once (`help::tooltips::SetLevel` from `Game::ApplyPlayerSettings`) and it is kept with the profile, 2 without a value; the levels themselves are in [tooltips.md](tooltips.md); no test of the menu reaching it |
| Creature help check box turns on the help about what the creature is learning | partial 80% | the box turns the creature help system's switch on or off at once and follows SET_CREATURE_HELP while the menu is closed (`Game::HandleInterfaceAction`, `Locator::creatureHelpSystem`); test `GameMenu.TheCreatureHelpBoxShowsTheSwitchAndThePlayerChangesItFromThere`; the creature's help messages are in [help_system.md](help_system.md) |
| Left handed swaps the hand's look to the left hand | partial 70% | the box mirrors the hand at once (`EngineConfig::rightHandedHand`, `hand_detail::HandModelScale`; tests `HandHandedness.*`); the exe mirrors the same hand ([hand placement](../../../hand/animation-and-placement.md)); which way round the box and the default hand go is not compared |
| Left handed also swaps which mouse button picks up and which acts | inferred | (unconfirmed whether the exe swaps the buttons with the hand) |
| Text from the bottom of the screen instead of the top | partial 80% | the box sets the dialogue's TEXT_TOPTOBOTTOM at once and it is kept with the profile (`help::DialogueText::SetPlayerChoices`), off without a profile value as the exe; test `HelpTextDisplay.TopToBottom` (the display); no test of the menu reaching it |
| Help text speed slider sets how long text stays up | partial 80% | the slider sets the help's READ_SPEED as the options close, as the exe ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); the slider starts at the help's read speed without a profile value; no test of the menu reaching it |
| A force feedback mouse's effects and their strength, on their own page | pending | the exe has a force feedback box with a level choice (the immersion box); to port as the original's (user, 2026-10-19) |

## Keeping the settings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The effects and music volumes are kept per computer | done | `Profiles/Setup.txt`, next to the profiles (`machine_setup`, `audio::main_volumes`): read at start and as the Options page shows, kept as it goes and as the game ends, as the original's setup key; a run of the checks keeps none; tests `MachineSetup.*`; see ../audio/voices_and_speech.md |
| The detail level, and the highest detail the computer was found to manage, are kept per computer | pending | the exe saves the detail and reads it at start ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); nothing is kept between runs |
| Tooltip level, story text, help level, creature help, hand orientation, text position, push scrolling and auto save are kept with the player's profile, auto save on and the rest off or at the game's defaults when the profile has none | partial 70% | all are kept in `Profiles/Profiles.txt` with the profile (`player_profiles::Settings`, `gui::ToProfileSettings`; tests `PlayerProfiles.SettingsAreTheGamesOwnUntilKeptAndAreReadBackAsWritten`, `GameMenu.OptionsAreKeptWithTheProfileAsTheyAre`); the help level, story text, text position and read speed defaults are the exe's ([advisors and help](../../../interface/advisors-and-help.md)); the tooltip, creature help, hand, push scroll and auto save defaults are not read against the exe; a run of the checks keeps none |
| How fast the player reads (which sets how long text stays up) is kept with the profile | partial 80% | kept with the profile and read back at start (`Game::ApplyPlayerSettings`), 0.5 without a value as the exe; test `PlayerProfiles.SettingsAreTheGamesOwnUntilKeptAndAreReadBackAsWritten` |
