# Options

The options' pages of the menu: sound and video, the advanced page for help, text, tooltips and the hand, and how the
settings take effect and are kept. The player's page is in [profiles.md](profiles.md) and the controls page in
[key_bindings.md](key_bindings.md).

**Progress: 5/20 done, 12 partial — 55%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## The pages

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The options are tabs of one box: Options, Players, Advanced and Controls, with a Main Menu tab and a Back arrow to the first page | done | `GameMenu::OptionsTabs` |
| A Quit arrow on the options page | done | `GameMenu.cpp` |
| The options room inside the temple shows a small version of the same settings | partial | the options room opens the menu's Options page once the camera is in the room, and goes back to the main room when it closes (`TempleInterior::UpdateOptionsAndFutureRooms`); our wiki has not read the original's room, so whether it is a smaller version is open; see ../temple/ |

## Sound and video

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Effects volume slider changes the sound effects' volume at once | done | `Game::HandleInterfaceAction` sets `audioSampleMainVolume` (value x 127) from the slider at once |
| Music volume slider changes the music's volume at once | done | same, `audioMusicMainVolume` |
| The menu starts with the volumes the game is playing at | done | `Game.cpp` fills `MenuSettings` from the configuration's main volumes and the running detail level |
| Detail level selector from minimum to maximum detail, stepped by its button and arrows | partial | the selector works (`GameMenu.cpp`) but changes nothing; the detail level is only set by `--detail-level` on the command line |
| A changed detail level takes effect the next time the game starts, and the game says so | partial | the menu says so (`GameMenu::TellVideoChange`, HELP_TEXT_DIALOG_VIDEOCHANGE) but the level is not kept for the next start |
| Auto save check box turns the land's automatic saves on or off | partial | the box works but nothing reads it; there are no automatic saves yet |
| Push scrolling check box makes the camera move when the hand pushes against the screen's edge | partial | the box works but nothing reads it |
| Screen resolution and the 3D card are picked outside the game, in its setup | n/a | openblack takes `--width`, `--height` and `--backend-type` on the command line |

## Advanced

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Help level from no help to all help sets how often the advisors offer help | partial | the selector works; the help system's level is not set from it ([help_system.md](help_system.md)) |
| Story text: none, story only, or all text shown on screen as people speak | partial | the selector works; the help text display's TEXT_DRAW gate is not set from it ([on_screen_text.md](on_screen_text.md)) |
| Tooltips: none, minimum, intelligent or all | partial | the selector works and the tooltips have the four levels (`help::tooltips::SetLevel`), but the menu's choice is never passed to them |
| Creature help check box turns on the help about what the creature is learning | partial | the box works; nothing reads it |
| Left handed swaps the hand's look to the left hand | partial | the box works (`MenuSettings::leftHandedHand`), but nothing reads it; the hand is always made the same way (`HandSystem`) |
| Left handed also swaps which mouse button picks up and which acts | todo | (unconfirmed which buttons swap) |
| Text from the bottom of the screen instead of the top | partial | the box works; the help text display can run top to bottom (`HelpTextDisplay`) but is not set from it |

## Keeping the settings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The detail level, and the highest detail the computer was found to manage, are kept per computer | todo | nothing is kept between runs |
| Tooltip level, story text, help level, creature help, hand orientation, text position, push scrolling and auto save are kept with the player's profile, auto save on and the rest off or at the game's defaults when the profile has none | todo | there are no profiles: the settings start at `MenuSettings`' defaults every run |
| How fast the player reads (which sets how long text stays up) is kept with the profile | partial | the Advanced page's read speed slider sets the help's READ_SPEED when the menu closes (`Game.cpp`), but there is no profile to keep it |
| Force feedback mouse settings (strength and on/off) | n/a | for a long gone force feedback mouse; openblack does not support one |
