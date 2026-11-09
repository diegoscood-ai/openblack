# On-screen text

The words the game writes over the screen: what the advisors and characters say, text that land scripts put up, titles,
and the game's text database and fonts that all of it comes from. The script functions themselves are listed in
../story/.

**Progress: 18/24 done, 0 chosen, 1 partial, 2 inferred — 76%**

How the original does it, in our wiki: [The hand and the interface](../../../interface/screen-and-readouts.md).

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
| What is said appears over the screen in the dialogue box, the newest text sliding in over the older ones | done | the texts are shown over the screen (`src/Help/HelpTextDisplay.cpp`, `Renderer::DrawHelpText`) with the newest sliding in (tests `HelpTextDisplay.NewestFadesIn`, `HelpTextDisplay.StackScalesAndAlpha`, `HelpTextDisplay.FourOrSixEntries`), as the exe's dialogue box ([screen and read-outs](../../../interface/screen-and-readouts.md#the-dialogue-text-box)) |
| The missionaries' song lights its words one at a time, a box behind the sung word on the verse's beat | done | `help::karaoke` and `HelpTextSystem::FollowSong` (`src/ECS/Systems/Implementations/HelpTextSystem.cpp`, `Game.cpp`); test `HelpTextDisplay.TheSungWordIsTheNewestTextsWordAtTheVersesBeat`; testbed scenario `dialogue.song_karaoke`; read in the exe ([screen and read-outs](../../../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message), whose note still calls it pending) |
| The story text setting chooses no text, story text only, or all text | done | the Advanced page's Story text and the profile set the dialogue's TEXT_DRAW (`Game::ApplyPlayerSettings`, `help::DialogueText::SetPlayerChoices`), 1 without a profile; the gate is the exe's (test `HelpTextDisplay.ResetCloseAndGate`; [screen and read-outs](../../../interface/screen-and-readouts.md#the-dialogue-text-box)) |
| Text sits at the top of the screen, or at the bottom with the setting | done | the Advanced page's box and the profile set TEXT_TOPTOBOTTOM (`help::DialogueText::SetPlayerChoices`), off without a profile, as the exe; test `HelpTextDisplay.TopToBottom` |
| Text stays up for as long as it takes to read, by the player's read speed, then fades | done | `HelpSystem::IsTextRead` and the display's stack fade (tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpTextDisplay.StackScalesAndAlpha`) |
| Some text waits for a click to go on, shown by a prompt | done | with interaction the text waits for the click and the "click to continue" icon is shown (`HelpSystem::Draw3D`); test `HelpSystem.WithInteraction` |
| Codes inside a text change its colour or put in a picture of the key or mouse button to press | done | colour and new line codes are done (tests `HelpTextDisplay.ColourCode`, `HelpTextDisplay.NewLineCode`); a $M code's key or button picture is drawn at the right of the dialogue box, as the original draws it (`DialogueText::ShowControlIcons`) |
| A number can be filled into a text | done | `RUN_TEXT_WITH_NUMBER`, `TEMP_TEXT_WITH_NUMBER` in `src/CHLApi.cpp`; test `HelpTextDisplay.NumberCodes` |
| Text running when the player goes into the temple is put away and comes back on leaving | pending | raffclar's #468 does it, not read in the exe by us; see [help_system.md](help_system.md) |

## Script text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script runs a text by name, on one line or wrapped, with or without waiting for the player | done | `RUN_TEXT`, `TEMP_TEXT` in `src/CHLApi.cpp` (`HelpSystem::RunText`, `TempText`); tests `HelpSystem.QueueSingleLineAndClear`, `HelpSystem.TempText` |
| A script draws text anywhere on screen at a size, fading in, in a colour it sets | pending | `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT`, `SET_DRAW_TEXT_COLOUR` log "not implemented" in `src/CHLApi.cpp`; the exe's natives pop their arguments and make a drawn-text object (not read further) |
| A script fades all its drawn text out | pending | `FADE_ALL_DRAW_TEXT` logs "not implemented"; the exe has the native |
| A script asks whether the text has been read | done | `TEXT_READ` in `src/CHLApi.cpp` (`HelpSystem::IsTextRead`); tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpSystem.WithInteraction`; read in the exe |
| Land titles and challenge titles shown as a challenge begins | inferred | not found in the exe or our wiki: whether the engine shows such titles, or the scripts' own texts do, is not read; see ../story/ |
| Words shown over the screen by the temple's future room | done | `GameInterface::SetMessage`, from `src/3D/Implementations/TempleInterior.cpp`; measured in ../temple/future_room.md |
| Game messages a land's feature script starts and adds lines to | inferred | `START_GAME_MESSAGE`, `ADD_GAME_MESSAGE_LINE` in `src/LHScriptX/FeatureScriptCommands.cpp` do nothing (`Level.cpp` reads them for the land's description); the exe has both commands, what it does with them is not read |

## Other overlays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers' names, and their details, over their heads | partial 25% | N toggles the names (`input::ShortcutKeys`, the names key's switch as the exe), but they are drawn by the debug overlay (`Gui::ShowVillagerNames` in `src/Debug/Gui.cpp`), not as the game's name list in its font and look, and without its fade; the details (S, the hovered villager's or all) are not ported ([screen and read-outs](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names); issue #29) |
| The creature's status panel and the fight's panel | done | drawn by `GameInterface::DrawCreaturePanelLayer` from `src/Creature/CreatureStatusPanel.cpp` and `src/Creature/CreatureFightHud.cpp` (tests `CreatureStatusPanel.*`); measured row by row in ../creature/creature_mode.md and ../creature/fighting.md |
| Cinema bars and screen fades | done | `ScreenFade` (`src/3D/ScreenFade.cpp`: the script fade and the wide screen bars), drawn by `Renderer::DrawFinishFrameOverlays`; tests `ScreenFade.*`; read in the exe ([screen and read-outs](../../../interface/screen-and-readouts.md#screen-fade-and-cinema-bars)); see ../camera/ and ../story/ |
