# On-screen text

The words the game writes over the screen: what the advisors and characters say, text that land scripts put up, titles,
and the game's text database and fonts that all of it comes from. The script functions themselves are listed in
../story/.

**Progress: 14/19 done, 0 chosen, 1 partial, 2 inferred — 75%**

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

The story text and text position settings are measured in [options.md](options.md#advanced), how long a text stays up in [help_system.md](help_system.md#running-help), and the text put away inside the temple there too.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is said appears over the screen in the dialogue box, the newest text sliding in over the older ones | done | the texts are shown over the screen (`src/Help/HelpTextDisplay.cpp`, `Renderer::DrawHelpText`) with the newest sliding in (tests `HelpTextDisplay.NewestFadesIn`, `HelpTextDisplay.StackScalesAndAlpha`, `HelpTextDisplay.FourOrSixEntries`), as the exe's dialogue box ([screen and read-outs](../../../interface/screen-and-readouts.md#the-dialogue-text-box)) |
| The missionaries' song lights its words one at a time, a box behind the sung word on the verse's beat | done | `help::karaoke` and `HelpTextSystem::FollowSong` (`src/ECS/Systems/Implementations/HelpTextSystem.cpp`, `Game.cpp`); test `HelpTextDisplay.TheSungWordIsTheNewestTextsWordAtTheVersesBeat`; testbed scenario `dialogue.song_karaoke`; read in the exe ([screen and read-outs](../../../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message), whose note still calls it pending) |
| Some text waits for a click to go on, shown by a prompt | done | with interaction the text waits for the click and the "click to continue" icon is shown (`HelpSystem::Draw3D`); test `HelpSystem.WithInteraction` |
| Codes inside a text change its colour or put in a picture of the key or mouse button to press | done | colour and new line codes are done (tests `HelpTextDisplay.ColourCode`, `HelpTextDisplay.NewLineCode`); a $M code's key or button picture is drawn at the right of the dialogue box, as the original draws it (`DialogueText::ShowControlIcons`) |
| A number can be filled into a text | done | `RUN_TEXT_WITH_NUMBER`, `TEMP_TEXT_WITH_NUMBER` in `src/CHLApi.cpp`; test `HelpTextDisplay.NumberCodes` |

## Script text

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script runs a text by name, on one line or wrapped, with or without waiting for the player | done | `RUN_TEXT`, `TEMP_TEXT` in `src/CHLApi.cpp` (`HelpSystem::RunText`, `TempText`); tests `HelpSystem.QueueSingleLineAndClear`, `HelpSystem.TempText` |
| A script draws text anywhere on screen at a size, fading in, in a colour it sets | done | `GAME_DRAW_TEXT`, `GAME_DRAW_TEMP_TEXT`, `SET_DRAW_TEXT_COLOUR` in `src/CHLApi.cpp` through `CinematicDirectorSystem::WriteText` / `SetTextColour` (raffclar's `gui::script_text`), drawn after the fight's panel, centred and broken as the original's `GatheringText::DrawText`; checked against v1.42 ([../../../interface/screen-and-readouts.md](../../../interface/screen-and-readouts.md#the-scripts-screen-texts)); tests `ScriptTexts.*`, `CinematicDirectorTexts.*` |
| A script fades all its drawn text out | done | `FADE_ALL_DRAW_TEXT`: `CinematicDirectorSystem::FadeAllTexts`, each text fading to nothing over the time and then gone, at once for none ([../../../interface/screen-and-readouts.md](../../../interface/screen-and-readouts.md#the-scripts-screen-texts)); test `CinematicDirectorTexts.AllFadeOutTogetherAndGoOnceFaded` |
| A script asks whether the text has been read | done | `TEXT_READ` in `src/CHLApi.cpp` (`HelpSystem::IsTextRead`); tests `HelpSystem.ReadingTimeWithoutVoice`, `HelpSystem.WithInteraction`; read in the exe |
| Words shown over the screen by the temple's future room | done | `GameInterface::SetMessage`, from `src/3D/Implementations/TempleInterior.cpp`; measured in ../temple/future_room.md; tests `FutureRoom.TheWordsWaitUntilTheCameraHasComeIn`, `FutureRoom.TheWordsFadeInInWholeStepsToHalfOverTwoSeconds` |
| Game messages a land's feature script starts and adds lines to | pending | read in the exe, not in this tree: they fill the land information box (`StartGameBox`, made by `FrontEnd::Init`): `START_GAME_MESSAGE` (command 99, its case 0x717BCD) hides every box, writes its text as the box's title (a land's title, as "GOOD AND EVIL MACHINE"), empties its list and shows it; `ADD_GAME_MESSAGE_LINE` (command 100, 0x717C9C) adds its text at the list's end; with the number 0 the text is the script's own, with 3 help text 0x1B37, 4 0x1B38, any other 0x1B36. Only the playground lands' scripts use them. In this tree both do nothing (`src/LHScriptX/FeatureScriptCommands.cpp`; `Level.cpp` reads them for the land's description); the box is `gui::LandInfoBox` of the skirmish work, not landed (its wiki's skirmish page, "The land information box") |

## Other overlays

The cinema bars and the script fades are measured in [../camera/cinematics.md](../camera/cinematics.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers' names, and their details, over their heads | partial 25% | N toggles the names (`input::ShortcutKeys`, the names key's switch as the exe; test `ShortcutKeys.TheNamesKeyTogglesTheNames`), drawn as the game's names list in its font and look, with its fade (`gui::villager_labels`, `Renderer::DrawVillagerName`; [the villagers' names](../../../interface/screen-and-readouts.md#the-villagers-names)); the details (S, the hovered villager's or all) are not ported ([screen and read-outs](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names); issue #29; merged with the former key_bindings row "N toggles villagers' names and S their details") |
| The creature's status panel and the fight's panel | done | drawn by `GameInterface::DrawCreaturePanelLayer` from `src/Creature/CreatureStatusPanel.cpp` and `src/Creature/CreatureFightHud.cpp` (tests `CreatureStatusPanel.*`); measured row by row in ../creature/creature_mode.md and ../creature/fighting.md |
