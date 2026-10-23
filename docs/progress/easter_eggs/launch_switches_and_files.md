# Launch switches and stray files

The original game's command-line switches, the alternative settings file that can point it at other challenge
scripts, and the files it quietly writes about the player's creature. openblack's own switches are in
[../debug/command_line_tools.md](../debug/command_line_tools.md); online services (the creature web site, the lobby)
are in [../pc_integration/](../pc_integration/) and [../multiplayer/](../multiplayer/).

**Progress: 0/13 done, 1 partial — 4%**

## Command-line switches

Each switch starts with `-` or `/` and is matched by its start, in capitals, anywhere on the command line.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `NEWGAME` skips the main menu and starts a new game at once | partial 60% | our tree's `--start-level` loads a land directly (`src/main.cpp`), but there is no main-menu flow to skip; re-measured 2026-10-21: --start-level starts a land at once; there is no main-menu flow in front of it to skip |
| `MULTIPLAYER` goes straight to the online lobby (ignored if `NEWGAME` came first) | pending | our tree has no such switch; no online lobby |
| `SKIRMISH` goes straight to the skirmish set-up box | pending | our tree has no such switch; `NEWGAME` overrides it, and it overrides `MULTIPLAYER`; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| `PREINTROVIDEO` plays the video before the intro, which otherwise plays only while no player profile exists | pending | our tree has no such switch; the pre-intro is not played; see [../video/](../video/) |
| `VERSION` writes `mpver.txt` beside the game holding the program's checksum (the number multiplayer uses to match versions), then carries on | pending | our tree has no such switch |
| `NOLOADMUSIC` skips loading the music | pending | our tree has no such switch |
| `SETTINGS:<file>` reads that file (up to the next space) instead of the usual settings file | pending | our tree has no such switch |
| In that settings file, a line `QUESTDIR <folder>` makes the game load its challenge scripts from another folder, so a modded challenge file can be played without replacing the original | pending | our tree always reads `Scripts/Quests/challenge.chl` (`Game.cpp`); lines starting with `//` are comments in the game |
| `CONVERT` is read when a game starts | pending | our tree has no such switch; what it does is unconfirmed |

## Files the game writes about the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| About once a minute of play (every 600 game turns), the game writes a web page about the player's own creature, `html/<profile name>.html` in the profile's folder, from the template `Scripts/creature_html.tmpl` | pending | no creature web page in our tree; the same feature is one row in [../creature/lessons_and_help.md](../creature/lessons_and_help.md); the exact conditions are partly unconfirmed |
| The page, titled "Creature Cave", has a parchment frame and a floating spirit animation, and lists the creature's name, age, alignment, strength, health, energy, villagers killed and battles won; the template can also show mushrooms eaten, poos done, battles fought, creatures, animals and people killed, amount of poo, itchiness, thirst, tiredness, fatness, its beliefs, its desires and the miracles it has received | pending | no creature web page in our tree; the pictures it shows (`creatureshot_N.jpg`) are not made by the game |
| At the same moment the creature's mind and body are saved to `Scripts/CreatureMind/<id>.erc` and `Scripts/CreatureMind/Physique<id>.erc` | pending | nothing saves the creature's mind once a minute in our tree; the mind can be written (`CreatureMindSystem::SaveMind`, `src/ECS/Systems/Implementations/CreatureMindLearning.cpp`); see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| Ctrl+Alt+T writes `creaturesnapshot.csn` | pending | see [hidden_keys_and_cheats.md](hidden_keys_and_cheats.md) |
