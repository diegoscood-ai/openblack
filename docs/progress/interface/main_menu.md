# Main menu

The menu Escape brings up over the game (continue, skirmish, online, options, quit, with a statistics tab), the boxes
the game shows before play starts (choosing or making a player), and the dialog controls all of them are built from.

**Progress: 17/35 done, 0 chosen, 10 partial, 1 inferred — 68%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/states.md).

## Opening and closing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape brings the menu up over the game, and the game's dialogs take the mouse and keys while it is open | done | `src/Gui/GameInterface.cpp`, `src/Gui/GameMenu.cpp`; the Escape rules (`gui::EscapeBlocked`) tested in `MenuEscape.*`; every box takes the mouse while it is shown, as the original ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)) |
| The menu fades in over 0.6 of a second and out over a fifth | partial 50% | `k_FadeInSeconds`, `k_FadeOutSeconds` in `src/Gui/GameMenu.cpp`, test `GameMenu.FadesInAndOut`. The fade out (0.2 s) matches; the exe's active box fades in over 0.6 s (the box switch's fade-in zoomer, read 2026-10-21), ours over 0.5 s: a difference to fix |
| The game is paused while the menu is open and goes back to how it was when it closes | partial 80% | `Game::HandleInterfaceAction` in `src/Game.cpp` pauses while open, as the original's Continue unpauses ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); no test, and leaving the pause alone when opened inside the temple is inferred |
| Escape backs out of an options page to the first page, and closes the menu from there | done | `GameMenu::Escape`; tests `GameMenu.EscapeBacksOut`, `MenuEscape.OverAQuestion`, `MenuEscape.PageAfterAnAnswer` |
| The menu draws its own mouse pointer while a box is up | partial 70% | `GameInterface::Draw` (`DialogPainter::DrawPointer`), over the debug windows too (ours); the original turns its pointer on while a box takes the mouse ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)); no test |
| Inside the temple the options' first tab is the World Room, which closes them | inferred | `GameMenu::SetInsideTemple`, called from `src/3D/Implementations/TempleInterior.cpp`; the exe's main menu tabs always label tab 0 with the same text ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)); the temple's own tab has not been read |
| Some boxes can't be closed with Escape (asked per box), and the menu can't be reopened while a box forbids it | partial 60% | each box of the original answers whether Escape closes it (the SkipBox does not: [skipping the tutorial](../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)); ours refuses Escape with Shift or Ctrl, over a film, under a script's wide screen and within 300 ms of closing (`gui::EscapeBlocked`, test `MenuEscape.FilmWideScreenAndDebounce`); no per-box answer yet |
| Closing the options writes the settings back so they are there next time | partial 70% | the volumes are kept per computer and the profile's options with the profile as they change (`machine_setup`, `PlayerProfileSystem::SetCurrentSettings`, tests `MachineSetup.*`, `GameMenu.OptionsAreKeptWithTheProfileAsTheyAre`); the detail level is not kept for the next start; see [options.md](options.md) |

## The first page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Greets the player by their profile's name above the buttons | done | the profile played's name (`Game::CreateInterface`), test `GameMenu.GreetsThePlayer`; the exe's main menu reads the current profile for its greeting line (MainMenu's Init and InitControls, 2026-10-21) |
| Continue Game closes the menu | done | `GameMenu::Action::Continue`; tests `GameMenu.ClicksOnEveryControlThatActs`; the original's Continue hides the box and unpauses ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)) |
| Start Skirmish Game | pending | logs "This part of the menu is not available yet" (`Game::HandleInterfaceAction`); see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Join Online Game | pending | logs "This part of the menu is not available yet"; see ../multiplayer/ |
| The first button reads Leave Skirmish Game during a skirmish and Exit Online Game during a network game | pending | the exe's main menu picks the first button's text by the kind of game (MainMenu's InitControls asks whether it is a multiplayer game); noted in `GameMenu::BuildMain`, not done; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Options opens the options' pages | done | `GameMenu::OptionsTabs`, `ShowPage`; test `GameMenu.OptionsOpenTheOptionsTabs` |
| Quit asks "are you sure" in a smaller opaque box with Yes and No arrows; Yes quits | done | `GameMenu::Ask` with the Yes and No arrows; Yes sets `config.running` false (`Game::HandleInterfaceAction`); test `GameMenu.QuittingAsksFirst`; the exe's Yes / No box ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)) |
| The Statistics tab opens the game's statistics | done | `GameMenu::OpenStatistics` opens the Game Statistics box (`src/Gui/StatisticsBox.cpp`), test `StatisticsBoxLayout.*` in `test_interface.cpp`; tab 1 is the statistics tab in the exe ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)); the box itself is in [statistics.md](statistics.md) |
| Laid out as the original: box, five tabs, button places and text sizes | done | `GameMenu::GetButtonRect`, `BuildMain`; test `GameMenu.IsLaidOutAsTheOriginal`; the exe's five 440 by 70 buttons at 180 across from 145 down, 80 apart, the greeting above them (MainMenu's Init, read 2026-10-21), five tabs (`AddMainMenuTabs`) |

## Before play starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A first run asks for a new player's name in its own box | partial 70% | with no profile kept, before the first land: the name's box, the symbol and OK over the front end's swirling backdrop, fading in from black and out to it; a name taken or empty brings up a message (`Game::AskForNewPlayer`, `gui::NewPlayerBox`, `gui::menu_backdrop`, `gui::text_edit`; tests `NewPlayerBox.*`, `MenuBackdrop.*`, `TextEdit.*`); the original does it when it has no profile at all ([playback](../../../video/playback.md)); the box's own layout is not read against the exe. Never in a run of the checks; try it with `--profile-folder` on an empty folder |
| A skirmish game is set up in its own box (land, opponents) before it starts | pending | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Changing the player during a game asks to restart | partial 60% | the Players page: see [profiles.md](profiles.md) |
| A box tells the player the game is being saved while it saves | pending | the original's save message box; see ../engine/ for saving itself |
| The hand is turned off while a box is up, and the box's pointer shown | partial 60% | the hand's action ends while the menu or the SkipBox is up ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)) and the menu's pointer is drawn; not every box of the original is in openblack yet |

## Dialog look and controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dialogs are laid out in an 800 by 600 space, centred, and scaled down on smaller screens | done | `DialogPainter::k_Size` in `src/Gui/DialogPainter.h`; tests `ScreenFit.*`; the exe's boxes are placed in the same 800 by 600 space (MainMenu's buttons, the options' selectors) |
| Boxes, bevels, tabs and shadows are drawn from the front end atlas, text in the game's font | done | `DialogPainter` (`Front_end_buttons.raw`), `src/Gui/GameFont.cpp`; tests `GameFont.*` |
| The control under the pointer lights up orange; a control acts when the button is let go over the one it went down on | done | `src/Gui/Dialog.cpp`; test `GameMenu.ButtonsActWhenLetGoOverTheOneTheyWentDownOn`; the exe clicks on a release over the control with the focus (xref note on the setup box's draw) |
| Activating a control plays the menu button sound | partial 90% | `PlayButtonSound` in `src/Gui/GameInterface.cpp` (sample 159), as the exe's click noise (sample 159, xref note); no test |
| Buttons, big arrow buttons and square buttons | done | `Button`, `BigButton` in `src/Gui/Controls.cpp`; tests `GameMenu.ClicksOnEveryControlThatActs`, `GameMenu.PlayersSquareButtonsSpreadOutWithEditTattooOrWithout`; the exe's button and big button controls |
| Sliders step by clicks or follow the knob as it is dragged | done | `Slider` in `src/Gui/Controls.cpp`; test `GameMenu.SlidersStepOrFollowTheKnob` |
| Check boxes and radio-style selectors | done | `CheckBox` and the menu's selectors (`src/Gui/Controls.cpp`, `GameMenu.cpp`); test `GameMenu.SelectorsAndCheckBoxesChangeTheSettings`; the exe's selectors are a button between two big arrows ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)) |
| Edit boxes take typed text with a caret | done | `EditBox` in `src/Gui/Controls.cpp`; tests `EditBoxCaret.ShownOnTheOddFifthsOfASecondOfTheTickCount`, `TextEdit.*` |
| Lists scroll with the wheel and a bar, and select rows | done | `List` in `src/Gui/Controls.cpp`; test `GameMenu.PlayersPickAProfileAndLeavingStartsTheGameAgain` |
| A colour picker (for the tattoo colours) | done | `ColourPicker` in `src/Gui/Controls.cpp`, in the tattoo editor; test `TattooEditor.ThePaletteCellSpansTheWholeControl`; see [../creature/creature_tattoos.md](../creature/creature_tattoos.md) |
| Line and bar graphs (the statistics) | partial 70% | the box's `BarGraph` and `LineGraph` (`src/Gui/StatisticsBox.cpp`, test `GameStats.HistoryAveragesAndHalves` for the line's points), as the exe's vertical bar, stacked bar and line graph controls; how the bars and lines are drawn is not compared; see [statistics.md](statistics.md) |
| The keyboard works the controls: the focus moves between them and keys act on the one with it | pending | the exe's controls each take keys (the button, slider and list each have their own key handling) and a box has a focus control; how the focus moves is not read |
| Controls show a tooltip when the pointer rests on them | pending | the exe's controls each can carry a tooltip |
