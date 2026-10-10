# Skirmish opponents

The nameless computer gods of the skirmish lands (the land scripts in `Scripts/Playgrounds`) and of any temple no
person plays in a network game. They use the same computer player mind as the story's rival gods, with creatures from
the game's generic computer creature file. Everything about them comes from the land script: the skirmish box offers no
opponent, difficulty or team settings.

**Progress: 3/38 done, 14 partial — 26%**

See [ai.md](ai.md) for how they decide, [creatures.md](creatures.md) for their creatures and
[../multiplayer/skirmish.md](../multiplayer/skirmish.md) for the skirmish itself.

## How a slot gets a computer god

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player slot has a computer god's mind, switched off. The land script's `TOGGLE_COMPUTER_PLAYER` switches it on and marks that player as a computer player | partial | the command adds the player (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`); there is no mind to switch on |
| The switch is ignored while a network game's map is being loaded, so the people's slots stay theirs | todo | our tree has no network play; see [../multiplayer/network_play.md](../multiplayer/network_play.md) |
| A temple on a skirmish land whose player isn't switched on gets no god: nothing plays it | partial | nothing plays such a temple in our tree, as in the game, but nothing plays any other god's temple either |
| There is no difficulty setting anywhere. A computer god's strength comes only from its land and from matching the person's creature (below) | partial | our tree has no difficulty setting either; the sizing to the person's creature is not done (below) |
| No skirmish land sets a personality, and no script runs in a skirmish, so computer gods keep their built-in personality | todo | no computer god mind in our tree; see [ai.md](ai.md) |
| No skirmish land sets allies, so every god is left to its own attitude to each player (unconfirmed what that starts at) | todo | no computer god mind in our tree; no attitudes in our tree; see [../multiplayer/player_diplomacy.md](../multiplayer/player_diplomacy.md) |
| Messages, taunts and gifts to other players are in the mind but were never made | n/a | see [ai.md](ai.md) |
| The player's own colour and each god's colour mark their influence and their creatures (unconfirmed for creatures) | done | each player's influence is drawn in its colour (`influence::k_CircleColours`, `src/ECS/Influence/InfluenceCircles.cpp`); the creatures' colours are unconfirmed |

## The skirmish lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Skirmish lands can be chosen and loaded | partial | the debug menu's "Playground Islands" loads them (`src/Debug/Gui.cpp`); there is no skirmish box ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| [Two Gods](../multiplayer/maps/two_gods.md): player two is a computer god with a leopard | partial | the player and its creature are made by the land script (`ToggleComputerPlayer`, `CreateCreatureFromFile`); no mind drives the god |
| [Three Gods](../multiplayer/maps/three_gods.md): players two and three, with a lion and a horse | partial | the player and its creature are made by the land script (`ToggleComputerPlayer`, `CreateCreatureFromFile`); no mind drives the god |
| [Four Gods](../multiplayer/maps/four_gods.md): players two to four, with a horse, a tortoise and a zebra | partial | the player and its creature are made by the land script (`ToggleComputerPlayer`, `CreateCreatureFromFile`); no mind drives the god |
| [Island Wars](../multiplayer/maps/island_wars.md) (a fan's rework of one of Lionhead's later online maps, not a game install file): players two to four, with a leopard, a gorilla and a brown bear, all from Nemesis's mind file | partial | as above (the file is not in our game folder) |
| [Death Comes To Those That Wait](../multiplayer/maps/death_comes_to_those_that_wait.md) (a fan-made land by "Fat Omen", not a game install file): players three and four are computer gods with a tortoise and a zebra. Player two (a horse) and players five to eight get creatures but no temple and are never switched on | partial | as above; the creatures of the temple-less players are made too |
| [Firestorm](../multiplayer/maps/firestorm.md) has temples for players two and three but switches neither on, so they stand idle (as far as found) | partial | as in the game, nothing plays them; but nothing plays any god in our tree |
| Each computer god's towns, temple and influence come from the land script, as in the story | done | towns, temples and owners (`src/LHScriptX/FeatureScriptCommands.cpp`), influence (`src/ECS/Influence`) |
| Each skirmish land sets the player and town influence multipliers (0.4 to 1.2 for towns) | done | `SET_TOWN_INFLUENCE_MULTIPLIER`, `SET_PLAYER_INFLUENCE_MULTIPLIER` |
| The land's balance settings (impressiveness, speed and belief speed scales) apply to every god | partial | `SET_GLOBAL_LAND_BALANCE` is kept (`land_balance::Set`); only numbers 4 (villagers' speed, `src/ECS/VillagerSpeed.cpp`) and 5 (trees) are read; no god acts on them |

## Their creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script loads each god's creature from the generic computer creature file, of the species it names, at a given place | partial | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`); it faces a fixed way and takes our starting size |
| When the computer god starts, its creature is sized to the person's creature: theirs times 0.9 plus a random amount up to 0.3 (so 0.9 to 1.2 times) | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| The land names which player's creature to copy, but the name only turns the sizing on: it always copies the creature of the person at this machine | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| The sizing only happens when the computer god isn't the local player and the person has a creature | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| A god marked as a computer player has its creature learn at once every miracle the god has, plus heal, food, wood and water always | todo | nothing marks a computer player in our tree; see [creatures.md](creatures.md) |
| Those miracles count as seen as often as needed to learn them, so it can cast them straight away | todo | as above |
| In a skirmish (not a network game), a computer god with a temple but no creature gets one from the generic file, of one of 17 species at random, at its temple's creature spot | todo | no skirmish start in our tree |
| The creature's own mind then runs as any creature's | partial | creatures from files think with the creature mind (see [../creature/decision_making.md](../creature/decision_making.md)); nothing of their god steers them |
| The creature fights other creatures with the computer's tactics | partial | any creature not the local player's fights by the computer's moves (`fight::ChooseMove`); see [../creature/fighting.md](../creature/fighting.md) |

## Temples nobody plays (network games)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| In a network game, any player with a temple who is neither a person nor a computer god becomes a computer god | todo | our tree has no network play; see [../multiplayer/network_play.md](../multiplayer/network_play.md) |
| Its creature is one of 11 species at random, made at its temple's creature spot | todo | no network play |
| It is named with a line picked at random from the first 99 of the game's text | todo | no network play; in the shipped text those lines are villagers' grumbles ("Work, work, work. I'm sick of it."), not names; unconfirmed that players saw this |
| It is taken through all 13 stages of growing up at once | todo | no network play |

## Their hands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god's hand starts at its temple; without a temple it starts at a fixed spot (2150, 2620) | todo | only the person's hand exists (`HandSystem`) |
| The hand is drawn on the land so the person can watch it work (unconfirmed how it looks) | todo | only the person's hand exists (`HandSystem`) |
| The hand can only act inside its god's influence | todo | only the person's hand exists |
| It moves at its god's speed in metres per second, which scripts can change | todo | `SET_COMPUTER_PLAYER_SPEED` is a stub |

## Out of the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god among players one to four with no temple left is out of the game (how a temple is lost is unconfirmed here) | todo | no rule in our tree takes a god out of the game; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md); how a temple is damaged and destroyed: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A god that is out has its mind switched off and its creature taken off the land | todo | as above |
| The person is told "<name> is out of the game."; when the last other god goes, they have won | todo | as above; see [../multiplayer/skirmish.md](../multiplayer/skirmish.md) |
| Players five to eight are never counted out | todo | as above |
