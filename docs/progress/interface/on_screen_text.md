# On-screen text

The words the game writes over the screen: what the advisors and characters say, text that land scripts put up, titles,
and the game's text database and fonts that all of it comes from. The script functions themselves are listed in
../story/.

**Progress: 12/24 done, 6 partial — 62%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Text and fonts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's texts are read from its info scripts by name, later scripts replacing earlier ones | done | `src/Gui/TextDatabase.cpp` (the menus' names) and `helptext` (`src/Common/HelpText.h`, the numbered help texts); tests `HelpTextLoader.*` |
| Each text says who speaks it (narrator, good or evil advisor, a character) | done | each ADD_TEXT's narrator is read (`helptext::Entry::narrator`) and picks the voice and the colour (`HelpSystem::SayText`, test `HelpTextDisplay.NarratorColourAndFont`) |
| The game's fonts are read and drawn as the game draws them, with a question mark for missing letters | done | `src/Gui/GameFont.cpp`; tests `GameFontLoaderTest.*` |
| Lines wrap after spaces and hyphens and at line breaks | done | `GameFont::Wrap`; the help text's own word wrap in `HelpTextDisplay` (test `HelpTextDisplay.GreedyWrapAndMaxLines`) |
| Text in other languages (the game's other language files) | done | the installed game's text scripts are read, whatever their language (tests `HelpTextLanguage.*`) |

## Spoken text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is said appears over the screen, a word at a time in step with the voice | partial | the texts are shown over the screen (`src/Help/HelpTextDisplay.cpp`, `Renderer::DrawHelpText`) with the newest sliding in (test `HelpTextDisplay.NewestFadesIn`); no word-at-a-time reveal in step with the voice is in our code or wiki |
| The story text setting chooses no text, story text only, or all text | partial | the display has the TEXT_DRAW gate (`HelpSystem::GetTextDraw`, test `HelpTextDisplay.ResetCloseAndGate`), but the menu's Story text choice is not passed to it |
| Text sits at the top of the screen, or at the bottom with the setting | partial | top to bottom or bottom to top is a display input (`GetTextTopToBottom`, test `HelpTextDisplay.TopToBottom`), but the menu's box is not passed to it |
| Text stays up for as long as it takes to read, by the player's read speed, then fades | done | `HelpSystem::IsTextRead` and the display's stack fade (tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpTextDisplay.StackScalesAndAlpha`) |
| Some text waits for a click to go on, shown by a prompt | done | with interaction the text waits for the click and the "click to continue" icon is shown (`HelpSystem::Draw3D`); test `HelpSystem.WithInteraction` |
| Codes inside a text change its colour or put in a picture of the key or mouse button to press | partial | colour and new line codes are done (tests `HelpTextDisplay.ColourCode`, `HelpTextDisplay.NewLineCode`); the key or button picture of a $M code is not drawn in the help text |
| A number can be filled into a text | done | `RUN_TEXT_WITH_NUMBER`, `TEMP_TEXT_WITH_NUMBER` in `src/CHLApi.cpp`; test `HelpTextDisplay.NumberCodes` |
| Text running when the player goes into the temple is put away and comes back on leaving | todo |  |

## Script text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script runs a text by name, on one line or wrapped, with or without waiting for the player | done | `RUN_TEXT`, `TEMP_TEXT` in `src/CHLApi.cpp` (`HelpSystem::RunText`, `TempText`); tests `HelpSystem.QueueSingleLineAndClear`, `HelpSystem.TempText` |
| A script draws text anywhere on screen at a size, fading in, in a colour it sets | todo | `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT`, `SET_DRAW_TEXT_COLOUR` log "not implemented" in `src/CHLApi.cpp` |
| A script fades all its drawn text out | todo | `FADE_ALL_DRAW_TEXT` logs "not implemented" |
| A script asks whether the text has been read | done | `TEXT_READ` in `src/CHLApi.cpp` (`HelpSystem::IsTextRead`) |
| Land titles and challenge titles shown as a challenge begins | todo | see ../story/ |
| Words shown over the screen by the temple's future room | done | `GameInterface::SetMessage`, from `src/3D/Implementations/TempleInterior.cpp`; see ../temple/ |
| Game messages a land's feature script starts and adds lines to | todo | `START_GAME_MESSAGE`, `ADD_GAME_MESSAGE_LINE` in `src/LHScriptX/FeatureScriptCommands.cpp` do nothing |

## Other overlays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Text shown while the game is paused | todo | (unconfirmed what it says) |
| Villagers' names, and their details, over their heads | partial | drawn by the debug overlay (`Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`), not in the game's font or look |
| The creature's status panel and the fight's panel | partial | the panels' values and layout are ported (`src/Creature/CreatureStatusPanel.cpp`, `src/Creature/CreatureFightHud.cpp`; tests `CreatureStatusPanel.*`) but nothing draws them yet; see ../creature/ |
| Cinema bars and screen fades | done | `ScreenFade` (`src/3D/ScreenFade.cpp`: the script fade and the wide screen bars), drawn by `Renderer::DrawFinishFrameOverlays`; see ../camera/ and ../story/ |
