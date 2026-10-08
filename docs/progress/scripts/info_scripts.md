# Info scripts and tables

The rest of `Scripts/`: the game's text in three UTF-16 text scripts (`InfoScript2.txt`, `InfoScriptPatch2.txt`,
`InfoScriptMultiplayer2.txt`), the table file `info.dat`, the two tables for the real-world weather feature
(`weatherinfo.lhw`, `country.lhw`) and the template for the creature's web page. What `info.dat`'s tables hold and how
openblack reads them is [../engine/info_tables.md](../engine/info_tables.md); this file covers only what is specific to
the script files.

**Progress: 11/20 done, 4 partial — 65%**

## The game's text (`InfoScript2.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text script is UTF-16 with a byte order mark, one `ADD_TEXT` line per text: a flag, a narrator, the text's name and its English words | done | `helptext::Parse` (`src/Common/HelpText.cpp`), and `gui::TextDatabase::AddScript` (`src/Gui/TextDatabase.cpp`) by name; `test/test_help_text_loader.cpp` |
| It holds 6,974 texts, the count given by its own constant at the top | done | all read; `helptext::k_TextCount` is the bound the scripts' text numbers are checked against |
| A `\n` in a text is a line break | done | `helptext::ConvertScriptText` turns it into a line break (and the tilde forms into the game's hard space) |
| Texts are numbered in the order they come in, and the challenge scripts, advisors and help ask for them by number | done | `helptext::Get` and `GetEntry` by the text's place in the file; the challenge scripts' say-text functions, the advisors and help use them |
| Each text names who speaks it: nobody (2,587 texts, mostly interface and villagers), the default voice (728), the good advisor (1,127), the evil advisor (851), a man (791), a woman (152), Nemesis (200), Khazar (160), Lethys (49), the trainer (143), the guide (118), the monk (24), a big voice (19), a boy (18) or the ogre (7) | done | `helptext::Entry::narrator`, resolved with the file's own narrator lines; saying a text and the advisor who speaks it read it (`help::SpiritWhoTalks`); the voices are the audio domain's (see ../audio/) |
| Two of the narrators used (the guide and the monk) aren't among the narrator numbers the script declares | n/a | data quirk: our reader gives them an undeclared narrator (`helptext::k_NarratorUndeclared`); which voice the game gives them is not known |
| The first number of each line (1 for 6,214 texts, 0 for 760) | done | kept as `helptext::Entry::arg0`: whether the text is drawn in the dialogue box (`HelpSystem::AddText` passes it to `HelpTextDisplay::Add`) |
| The texts cover new-game and tutorial lines (986 starting "new"), creature lines (755), the creature's questions (620), villagers' lines and banter (619), dialogue boxes (388), creature states (260), tooltips (160), pause menu (152), did-you-know hints (107), multiplayer (99), temple rooms (96), and each challenge's lines | partial | read; shown where our tree has the interface for them (the dialogue box, the advisors, tooltips, temple scrolls and signs, the creature cave screen) |
| Text is looked up by the game's language (the shipped file is English) | partial | `helptext::Language` reads the game folder's `Country.txt` (used for the text size); only the English text file is read |

## The patch and multiplayer text (`InfoScriptPatch2.txt`, `InfoScriptMultiplayer2.txt`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The patch's 12 texts: the start-up choices added by the patch (start normally, skip to choosing the creature, skip the creature's training, keep the old creature) and the online lobby's labels | done | `helptext::GetPatch` and `TextDatabase` |
| The multiplayer script's 29 texts: the winning conditions (villagers killed, eaten, born; houses, wonders, food, wood, towns, prayer power, belief, trees grown and more) and the lobby's labels | done | read by `TextDatabase` |
| These are kept apart from the main texts and numbered from nought on their own | partial | the patch texts have their own numbers (`helptext::GetPatch`); the multiplayer texts are only in `TextDatabase`'s single table by name |
| The patch's start-up choices act on the story (skip the tutorial, skip the creature's training, keep the old creature) | done | the start-up requester's answer sets the skip flags in `Game.cpp`, and the challenge functions read them (`CanSkipTutorial`, `CanSkipCreatureTraining`, `IsKeepingOldCreature`; see challenge_natives_game_flow.md) |

## The table file (`info.dat`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game reads every table of objects, buildings, miracles, creatures, villagers and dances from `Scripts/info.dat` when it starts | done | `Game::Run` (`InfoFile::LoadFromFile`); see ../engine/info_tables.md |
| The dance table names the dance scripts | partial | read, unused; see dance_scripts.md |
| The challenge language's constant names come from the same tables | done | `components/lhvmlang` (`ConstantTable::LoadInfo`); test `ChlLanguage.ConstantsFromInfoTables` (`test/lhvm/test_chl_language.cpp`) |

## The weather feature's tables (`weatherinfo.lhw`, `country.lhw`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `country.lhw` lists 242 countries by their two-letter code, for the player to say where they live | todo | not read; the real-weather feature is the weather domain's (see ../weather/scripted_weather.md) |
| The game asks a weather server for the weather in the player's country, area and city, and gets back a weather type, a temperature and the visibility | todo | none |
| `weatherinfo.lhw` turns each of 13 weather types into the land's weather (rain, snow, overcast, wind, temperature and the like, as ranges) | todo | not read |

## The creature's web page (`creature_html.tmpl`, `html/`)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game writes a web page about the player's creatures from the template, filling in each creature's name, age, alignment, strength, energy and health with a picture of it and the images in `Scripts/html/` | todo | none; the creature cave and profiles are the creature domain's (see ../creature/) |
| The pictures of the creatures for the page are taken by the game | todo | none |
