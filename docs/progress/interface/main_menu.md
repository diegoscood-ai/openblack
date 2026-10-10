# Main menu

The menu Escape brings up over the game (continue, skirmish, online, options, quit, with a statistics tab), the boxes
the game shows before play starts (choosing or making a player), and the dialog controls all of them are built from.

**Progress: 19/36 done, 3 partial — 57%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Opening and closing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape brings the menu up over the game, and the game's dialogs take the mouse and keys while it is open | done | `src/Gui/GameInterface.cpp`, `src/Gui/GameMenu.cpp`; the Escape rules (`gui::EscapeBlocked`) tested in `MenuEscape.*` |
| The menu fades in over half a second and out over a fifth | done | `k_FadeInSeconds`, `k_FadeOutSeconds` in `src/Gui/GameMenu.cpp` |
| The game is paused while the menu is open and goes back to how it was when it closes | done | `Game::HandleInterfaceAction` in `src/Game.cpp` pauses while open (not when opened inside the temple, which holds the pause) |
| Escape backs out of an options page to the first page, and closes the menu from there | done | `GameMenu::Escape`; tests `MenuEscape.OverAQuestion`, `MenuEscape.PageAfterAnAnswer` |
| The menu draws its own mouse pointer, over the debug windows too | done | `GameInterface::Draw` (`DialogPainter::DrawPointer`) |
| Inside the temple the options' first tab is the World Room, which closes them | done | `GameMenu::SetInsideTemple`, called from `src/3D/Implementations/TempleInterior.cpp` |
| Some boxes can't be closed with Escape (asked per box), and the menu can't be reopened while a box forbids it | partial | Escape is refused with Shift or Ctrl, over a film, under a script's wide screen and within 300 ms of closing (`gui::EscapeBlocked`, test `MenuEscape.FilmWideScreenAndDebounce`); no box forbids it yet |
| Closing the options writes the settings back so they are there next time | todo | nothing is saved; see [options.md](options.md) |

## The first page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Greets the player by their profile's name above the buttons | partial | greets by `OPENBLACK_PLAYER_NAME` or the computer's login name (`Game.cpp`); openblack has no profiles ([profiles.md](profiles.md)) |
| Continue Game closes the menu | done | `GameMenu::Action::Continue` |
| Start Skirmish Game | todo | logs "This part of the menu is not available yet" (`Game::HandleInterfaceAction`); see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Join Online Game | todo | logs "This part of the menu is not available yet"; see ../multiplayer/ |
| The first button reads Leave Skirmish Game during a skirmish and Exit Online Game during a network game | todo | noted in `GameMenu::BuildMain`, not done; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Options opens the options' pages | done | `GameMenu::OptionsTabs`, `ShowPage` |
| Quit asks "are you sure" in a smaller opaque box with Yes and No arrows; Yes quits | done | `GameMenu::Ask` with the Yes and No arrows; Yes sets `config.running` false (`Game::HandleInterfaceAction`) |
| The Statistics tab opens the game's statistics | todo | logs "This part of the menu is not available yet"; see [statistics.md](statistics.md) |
| Laid out as the original: box, five tabs, button places and text sizes | done | `GameMenu::GetButtonRect`, `BuildMain` (box, five tabs, button places and text sizes) |

## Before play starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game starts on a front end that asks which player is playing before the first land | todo | openblack loads straight into a land |
| A first run asks for a new player's name in its own box | todo |  |
| A skirmish game is set up in its own box (land, opponents) before it starts | todo | see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Changing the player during a game asks to restart | todo |  |
| A box tells the player the game is being saved while it saves | todo | see ../engine/ for saving itself |
| The front end has its own pointer and turns the hand off while a box is up | partial | the menu's pointer is drawn; there is no front end yet |

## Dialog look and controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dialogs are laid out in an 800 by 600 space, centred, and scaled down on smaller screens | done | `DialogPainter::k_Size` in `src/Gui/DialogPainter.h` |
| Boxes, bevels, tabs and shadows are drawn from the front end atlas, text in the game's font | done | `DialogPainter` (`Front_end_buttons.raw`), `src/Gui/GameFont.cpp`; tests `GameFontLoaderTest.*` |
| The control under the pointer lights up orange; a control acts when the button is let go over the one it went down on | done | `src/Gui/Dialog.cpp` (the pressed and hovered control) |
| Activating a control plays the menu button sound | done | `PlayButtonSound` in `src/Gui/GameInterface.cpp` (sample 159) |
| Buttons, big arrow buttons and square buttons | done | `Button`, `BigButton` in `src/Gui/Controls.cpp` |
| Sliders step by clicks or follow the knob as it is dragged | done | `Slider` in `src/Gui/Controls.cpp` |
| Check boxes and radio-style selectors | done | `CheckBox` and the menu's selectors (`src/Gui/Controls.cpp`, `GameMenu.cpp`) |
| Edit boxes take typed text with a caret | done | `EditBox` in `src/Gui/Controls.cpp`; test `EditBoxCaret.BlinksEveryQuarterSecond` |
| Lists scroll with the wheel and a bar, and select rows | done | `List` in `src/Gui/Controls.cpp` |
| A colour picker (for the tattoo colours) | todo |  |
| Line and bar graphs (the statistics) | todo | see [statistics.md](statistics.md) |
| Tab and the arrow keys move the focus between controls; Enter acts | todo |  |
| Controls show a tooltip when the pointer rests on them | todo |  |
