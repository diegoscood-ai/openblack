# Main menu

The menu Escape brings up over the game (continue, skirmish, online, options, quit, with a statistics tab), the boxes
the game shows before play starts (choosing or making a player), and the dialog controls all of them are built from.

**Progress: 19/32 done, 0 chosen, 5 partial, 1 inferred — 69%**

How the original does it, in our wiki: [The hand and the interface](../../../hand/states.md).

## Opening and closing

How the options are kept as the box is left is measured in [options.md](options.md#keeping-the-settings).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape brings the menu up over the game, and the game's dialogs take the mouse and keys while it is open | done | `src/Gui/GameInterface.cpp`, `src/Gui/GameMenu.cpp`; the Escape rules (`gui::EscapeBlocked`) tested in `MenuEscape.*`; every box takes the mouse while it is shown, as the original ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)) |
| The menu fades in over half a second and out over a fifth | done | `k_FadeInSeconds`, `k_FadeOutSeconds` in `src/Gui/GameMenu.cpp`, test `GameMenu.FadesInAndOut`. The box being opened fades in over 0.5 s and the box being left fades out over 0.2 s, in real seconds, as the exe's box switch sets them; the box switch also starts a 0.6 s fade that is stepped every frame but never read, so it shows nothing |
| The game is paused while the menu is open and goes back to how it was when it closes | done | `gui::StepMenuPause` from `Game::HandleInterfaceAction` pauses as the menu opens, as the original's main menu as it shows, and Continue unpauses ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)); inside the temple the menu leaves the pause alone, read 2026-10-24: the main menu never shows there and no options page pauses ([the temple's pause](../../../interface/temple-interior.md)); test `GameMenu.TheMenuPausesTheGameAsItOpensAndContinueUnpausesItButNotInsideTheTemple` |
| A page shown over another cross-fades with it (the old one out over a fifth of a second under the new one, in over half a second); a tab drops the old page at once and fades its page in from nothing | done | `gui::BoxFades` in `src/Gui/BoxFades.h`, used by `GameMenu::ShowPage`, `Open`, `Close` and `Draw`; tests `BoxFades.AShownBoxCrossFadesOverTheOneItReplaces`, `BoxFades.ATabReplacesTheBoxAtOnceAndFadesItInAgainFromNothing`, `BoxFades.HidingFadesTheBoxShownOutAndShowsNone`, `GameMenu.FadesInAndOut` ([how the menu's boxes fade](../../../interface/screen-and-readouts.md#how-the-menus-boxes-fade-into-one-another)) |
| Escape backs out of an options page to the first page, and does nothing on the first page | done | `GameMenu::Escape`: the original's first page refuses Escape (read 2026-10-24, [the box switches](../../../interface/screen-and-readouts.md); until then openblack's Escape closed the menu from there); tests `GameMenu.EscapeBacksOut`, `MenuEscape.OverAQuestion`, `MenuEscape.PageAfterAnAnswer` |
| The menu draws its own mouse pointer while a box is up | partial 70% | `GameInterface::Draw` (`DialogPainter::DrawPointer`), over the debug windows too (ours); the original turns its pointer on while a box takes the mouse ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)); no test |
| Inside the temple the options' first tab is the World Room, which closes them | inferred | `GameMenu::SetInsideTemple`, called from `src/3D/Implementations/TempleInterior.cpp`; the exe's main menu tabs always label tab 0 with the same text ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)); the temple's own tab has not been read |
| Some boxes can't be closed with Escape (asked per box), and the menu can't be reopened while a box forbids it | partial 60% | each box of the original answers whether Escape closes it (the SkipBox does not: [skipping the tutorial](../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)); ours refuses Escape with Shift or Ctrl, over a film, under a script's wide screen and within 300 ms of closing (`gui::EscapeBlocked`, test `MenuEscape.FilmWideScreenAndDebounce`); no per-box answer yet |

## The first page

The Statistics tab and the box it opens are measured in [statistics.md](statistics.md#getting-there).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Greets the player by their profile's name above the buttons | done | the profile played's name (`Game::CreateInterface`), test `GameMenu.GreetsThePlayer`; the exe's main menu reads the current profile for its greeting line (MainMenu's Init and InitControls, 2026-10-21) |
| Continue Game closes the menu | done | `GameMenu::Action::Continue`; tests `GameMenu.ClicksOnEveryControlThatActs`; the original's Continue hides the box and unpauses ([options and pause](../../../interface/screen-and-readouts.md#the-in-game-menus-options-and-pause)) |
| Start Skirmish Game | pending | logs "This part of the menu is not available yet" (`Game::HandleInterfaceAction`); see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Join Online Game | pending | logs "This part of the menu is not available yet"; see ../multiplayer/ |
| The first button reads Leave Skirmish Game during a skirmish and Exit Online Game during a network game | pending | the exe's main menu picks the first button's text by the kind of game (MainMenu's InitControls asks whether it is a multiplayer game); noted in `GameMenu::BuildMain`, not done; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Options opens the options' pages | done | `GameMenu::OptionsTabs`, `ShowPage`; test `GameMenu.OptionsOpenTheOptionsTabs` |
| Quit asks "are you sure" in a smaller opaque box with Yes and No arrows; Yes quits | done | `GameMenu::Ask` with the Yes and No arrows; Yes sets `config.running` false (`Game::HandleInterfaceAction`); test `GameMenu.QuittingAsksFirst`; the exe's Yes / No box ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)) |
| Laid out as the original: box, five tabs, button places and text sizes | done | `GameMenu::GetButtonRect`, `BuildMain`; test `GameMenu.IsLaidOutAsTheOriginal`; the exe's five 440 by 70 buttons at 180 across from 145 down, 80 apart, the greeting above them (MainMenu's Init, read 2026-10-21), five tabs (`AddMainMenuTabs`) |

## Before play starts

Changing the player during a game is measured in [profiles.md](profiles.md#the-players-page).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A first run asks for a new player's name in its own box | partial 70% | with no profile kept, before the first land: the name's box, the symbol and OK over the front end's swirling backdrop, fading in from black and out to it; a name taken or empty brings up a message (`Game::AskForNewPlayer`, `gui::NewPlayerBox`, `gui::menu_backdrop`, `gui::text_edit`; tests `NewPlayerBox.*`, `MenuBackdrop.*`, `TextEdit.*`); the original does it when it has no profile at all ([playback](../../../video/playback.md)); the box's own layout is not read against the exe. Never in a run of the checks; try it with `--profile-folder` on an empty folder |
| A skirmish game is set up in its own box (land, opponents) before it starts | pending | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| A box tells the player the game is being saved while it saves | pending | the original's save message box; see ../engine/ for saving itself |
| The hand is turned off while a box is up, and the box's pointer shown | partial 60% | the hand's action ends while the menu or the SkipBox is up ([a box takes the mouse](../../../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog)) and the menu's pointer is drawn; not every box of the original is in openblack yet |

## Dialog look and controls

The statistics' line and bar graphs are measured in [statistics.md](statistics.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dialogs are laid out in an 800 by 600 space, centred, and scaled down on smaller screens | done | `DialogPainter::k_Size` in `src/Gui/DialogPainter.h`; tests `ScreenFit.*`; the exe's boxes are placed in the same 800 by 600 space (MainMenu's buttons, the options' selectors) |
| Boxes, bevels, tabs and shadows are drawn from the front end atlas, text in the game's font | done | `DialogPainter` (`Front_end_buttons.raw`), `src/Gui/GameFont.cpp`; tests `GameFont.*` |
| The control under the pointer lights up orange; a control acts when the button is let go over the one it went down on | done | `src/Gui/Dialog.cpp`; test `GameMenu.ButtonsActWhenLetGoOverTheOneTheyWentDownOn`; the exe clicks on a release over the control with the focus (xref note on the setup box's draw) |
| Activating a control plays the menu button sound | done | `PlayButtonSound` in `src/Gui/GameInterface.cpp` plays `gui::k_ButtonSound` (sample 159 of the InGame bank, mode 3, 2D), as the exe's click noise ([interface sounds](../../../audio/creature-and-interface-sounds.md)); tests `GameMenu.TheClickIsTheMenuButtonsSound`, `GameMenu.ClicksOnEveryControlThatActs` |
| Buttons, big arrow buttons and square buttons | done | `Button`, `BigButton` in `src/Gui/Controls.cpp`; tests `GameMenu.ClicksOnEveryControlThatActs`, `GameMenu.PlayersSquareButtonsSpreadOutWithEditTattooOrWithout`; the exe's button and big button controls |
| Sliders step by clicks or follow the knob as it is dragged | done | `Slider` in `src/Gui/Controls.cpp`; test `GameMenu.SlidersStepOrFollowTheKnob` |
| Check boxes and radio-style selectors | done | `CheckBox` and the menu's selectors (`src/Gui/Controls.cpp`, `GameMenu.cpp`); test `GameMenu.SelectorsAndCheckBoxesChangeTheSettings`; the exe's selectors are a button between two big arrows ([the pause menu's boxes](../../../interface/tooltips.md#the-pause-menus-boxes-around-the-help)) |
| Edit boxes take typed text with a caret | done | `EditBox` in `src/Gui/Controls.cpp`; tests `EditBoxCaret.ShownOnTheOddFifthsOfASecondOfTheTickCount`, `TextEdit.*` |
| Lists scroll with the wheel and a bar, and select rows | partial 50% | `List` in `src/Gui/Controls.cpp` (`ScrollTo`, `Wheel`, the bar in `MouseDown` and `Drag`); the selection is tested (`GameMenu.PlayersPickAProfileAndLeavingStartsTheGameAgain`). Missing: no test of the wheel or the bar, and where the exe's list takes the wheel is not read ([statistics.md](statistics.md#the-box)) |
| A colour picker (for the tattoo colours) | done | `ColourPicker` in `src/Gui/Controls.cpp`, in the tattoo editor; test `TattooEditor.ThePaletteCellSpansTheWholeControl`; see [../creature/creature_tattoos.md](../creature/creature_tattoos.md) |
| The keyboard works the controls: the focus moves between them and keys act on the one with it | pending | read in the exe, not ported: the box's key (`SetupBox::Key`, bw1-decomp `alexmfc.cpp`) steps the focus with Tab, back with Shift and Tab (`SetFocusNext` 0x411090 / `SetFocusPrev` 0x411100, to the next control that is a tab stop and not hidden), and gives any other key to the control with the focus and then to the box's own handler; a slider takes Home, End and the left and right arrows (0, 1, a tenth down or up), a list Home, End and the up and down arrows (round from one end to the other); a button, a big button, a tab, a picture, a colour picker and a check box pass the key to the box's click key, which no box has (an empty function, 0x409170), so a key acts on them only through each box's own handler, which is not read. openblack gives a key only to the control with the focus (`Dialog::KeyDown`, `src/Gui/Dialog.cpp`), with no Tab |
| Controls show a tooltip when the pointer rests on them | pending | read in the exe, not ported: a control's tooltip is a help text (`SetupControl::SetToolTip` 0x409210); each frame of the front end (0x642520) hands the tooltip of the control under the pointer in the active box (its +0xBC) to the pointer's text (fn_0053BEA0 0x53BEA0: the text kept and shown from 0.1 to full), and the front end's pointer draws it beside itself (fn_0053C0F0, [the pointer](../../../rendering/textures.md)); which boxes give their controls a tooltip, and how the text is drawn, are not read. openblack's pointer draws no text (`DialogPainter::DrawPointer`) |
