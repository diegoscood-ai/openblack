# Script control of computer gods

The land scripts and the challenge scripts create the computer gods and steer them: they choose which players are
computer gods, give them creatures, tune their minds, move their hands, force and queue their actions, set how they feel
about each other and switch them off when they are beaten. Script function names are given where needed to find them.

**Progress: 8/49 done, 4 partial — 20%**

All challenge script functions live in `src/CHLApi.cpp`; land script commands in
`src/LHScriptX/FeatureScriptCommands.cpp`. See [../story/](../story/) for the script language and
[../scripts/](../scripts/) for the rest of its functions.

## Land script commands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Making a player a computer god (`TOGGLE_COMPUTER_PLAYER`, with a player name and 1) | partial | `FeatureScriptCommands::ToggleComputerPlayer` adds the player (`PlayerArchetype`) and ignores the on or off; nothing marks it as a computer god or gives it a mind |
| Loading a god's creature from a mind file, of a species, at a place (`CREATE_CREATURE_FROM_FILE`) | partial | `FeatureScriptCommands::CreateCreatureFromFile` (`CreatureArchetype`, the mind through the resource cache); it faces a fixed way and takes our starting size |
| Sizing a god's creature like another player's (`SET_COMPUTER_PLAYER_CREATURE_LIKE`) | todo | `SetComputerPlayerCreatureLike` is empty; see [skirmish_opponents.md](skirmish_opponents.md) |
| Setting a god's personality weight from the land file | todo | `FeatureScriptCommands::SetComputerPlayerPersonality` is empty (no shipped land uses it) |
| Loading a god's personality from the land file | todo | `FeatureScriptCommands::LoadComputerPlayerPersonality` is empty (no shipped land uses it) |
| Every land that makes a computer god runs the toggle before its towns are made | done | `Script::Load` runs the commands in order |

## Taking over the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving a god's hand to a place at a speed, optionally at a fixed height; this takes the hand from its mind | todo | `MOVE_COMPUTER_PLAYER_POSITION` is a stub in `src/CHLApi.cpp` |
| Setting a god's hand straight to a place, optionally at a fixed height | todo | `SET_COMPUTER_PLAYER_POSITION` is a stub in `src/CHLApi.cpp` |
| Reading where a god's hand is, for markers and for casting from it | todo | `GET_COMPUTER_PLAYER_POSITION` is a stub that gives the origin |
| Asking whether a god's hand is ready (its action finished) | todo | `COMPUTER_PLAYER_READY` always answers no, so scripts that wait on it stall |
| Releasing a god back to its mind | todo | `RELEASE_COMPUTER_PLAYER` is a stub in `src/CHLApi.cpp` |
| Pausing and unpausing a god | todo | `ENABLE_DISABLE_COMPUTER_PLAYER` (the pausing form) is a stub |
| Switching a god off for good (Khazar, Lethys and Nemesis when beaten) | todo | the other `ENABLE_DISABLE_COMPUTER_PLAYER` is a stub |
| Setting a god's hand speed (150 to 350 in the story) | todo | `SET_COMPUTER_PLAYER_SPEED` is a stub in `src/CHLApi.cpp` |
| A place a given distance in front of the camera, where gods come to speak | done | `GET_FACING_CAMERA_POSITION` (`GetFacingCameraPosition`, `src/CHLApi.cpp`): the camera's position plus the distance along its view to the focus (that direction is inferred) |
| Getting a god as an object, for "get computer player 2" | todo | `CALL_COMPUTER_PLAYER` is a stub that gives nothing |

## The camera and gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera's focus follows a god's hand | todo | `SET_FOCUS_FOLLOW_COMPUTER_PLAYER` is a stub; the script camera's computer-hand branch is not ported (`src/Camera/ScriptCamera.cpp`); see [../camera/](../camera/) |
| The camera's position follows a god's hand | todo | `SET_POSITION_FOLLOW_COMPUTER_PLAYER` is a stub in `src/CHLApi.cpp` |
| Asking whether the hand is near the camera within a radius | todo | depends on the computer god's hand position, a stub |

## Tuning the mind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setting a personality weight by a need's name, 0 to 1 | todo | `SET_COMPUTER_PLAYER_PERSONALITY` is a stub in `src/CHLApi.cpp` |
| Names the story uses: expanding influence, defeating a player, winning a town, destroying a town, defending a town, attacking a creature with miracles, reacting to an aggressive creature | todo | as given in the scripts: "ExpandInfluence", "DefeatPlayer", "TakeOverTown", "DestroyTown", "DefendTown", "AttackCreatureWithSpells", "ReactToAggressiveCreature"; the setter is a stub |
| Setting a need's suppression by name | todo | `SET_COMPUTER_PLAYER_SUPPRESSION` is a stub in `src/CHLApi.cpp` |
| Saving and loading a god's personality | todo | `SAVE_COMPUTER_PLAYER_PERSONALITY` and `LOAD_COMPUTER_PLAYER_PERSONALITY` are stubs |
| A list of every need's name ships for script writers' help | n/a | a text list in the game's script sources, not read by the game |

## Forcing and queueing actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Forcing an action by name on one or two objects, ahead of the god's own choice | todo | `FORCE_COMPUTER_PLAYER_ACTION` is a stub in `src/CHLApi.cpp` |
| Queueing an action by name on one or two objects | todo | `QUEUE_COMPUTER_PLAYER_ACTION` is a stub in `src/CHLApi.cpp` |
| Clearing a god's queued actions | todo | `GAME_CLEAR_COMPUTER_PLAYER_ACTIONS` is a stub in `src/CHLApi.cpp` |
| Actions the story forces: picking up and dropping an object at a place, casting wood on a town, getting its creature to help a town, winning, destroying and defending a town | todo | as given: "DIRECTPickupAndDropObject", "ShallCastWoodSpell", "GetCreaturetoHelp", "TakeOverTown", "DestroyTown", "DefendTown"; the force function is a stub |
| Waiting until a god is ready after a forced action | todo | stalls: `COMPUTER_PLAYER_READY` always answers no |

## Feelings and alliances

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setting one god's attitude to a player (kept doubled) | todo | `SET_COMPUTER_PLAYER_ATTITUDE` is a stub in `src/CHLApi.cpp` |
| Reading it back (halved) | todo | `GET_COMPUTER_PLAYER_ATTITUDE` is a stub that gives 0 |
| Allying two players by a percentage | todo | `SET_PLAYER_ALLY` is a stub in `src/CHLApi.cpp` |
| Asking how allied two players are | todo | `GET_PLAYER_ALLY` is a stub that gives 0 |

## Questions the story asks about gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A player's influence, or raw influence, at a place | done | `GET_INFLUENCE` (`magic::script::GetInfluence`, `src/Magic/Script/CHLInfluence.cpp`) through `influence::CalculatePlayerInfluence`, with or without allies (no alliances in our tree, so the same) |
| How many towns a player has | todo | `GET_PLAYER_TOWN_TOTAL` is a stub that gives 0 |
| Who owns a town, and a town by its number | todo | `GET_TOWN_WITH_ID` is a stub that gives no town; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| How much a town believes in a player | todo | `BELIEF_FOR_PLAYER` is a stub that gives 0 |
| Setting a town's belief, or its belief relative to now, for a player | todo | `SET_PLAYER_BELIEF` and `OBJECT_RELATIVE_BELIEF` are stubs |
| How long since a player attacked an object | todo | `GET_TIME_SINCE_OBJECT_ATTACKED` is a stub that gives 0 in our tree |
| A player's creature, by player number | done | `CALL_PLAYER_CREATURE` (`ecs::player_creature::PlayersCreature`, `src/ECS/PlayerCreature.cpp`) |
| A player's alignment, to choose a god's lines | done | `GET_ALIGNMENT` (`ecs::effects::alignment::Get`) |

## Gods' creatures and miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loading a creature of a species from a mind file for a player at a place | todo | `LOAD_CREATURE` is a stub in `src/CHLApi.cpp` |
| Naming it, growing it up, scaling it, teaching it, setting its body and alignment, making friends | partial | `SET_CREATURE_DEV_STAGE` works (`ecs::player_creature::SetDevelopmentStage`); `SET_CREATURE_NAME`, `CREATURE_AUTOSCALE`, `CREATURE_SET_KNOWS_ACTION` and `CREATURE_FORCE_FRIENDS` are stubs; see [creatures.md](creatures.md) |
| Casting a miracle from a god's hand at a place | partial | `SPELL_AT_POS` works (`magic::script::SpellAtPos`, `src/Magic/Script/CHLSpells.cpp`), cast by the neutral player from the point given; the god's hand position is a stub that gives the origin |
| Casting a miracle at an object, such as a creature, from a place | done | `SPELL_AT_THING` (`magic::script::SpellAtThing`), as the neutral player (unconfirmed whose it is in the game) |
| Topping up an icon's prayer power and reading a miracle's cost | done | `GAME_SET_MANA` (`magic::script::GameSetMana`, the worship site's prayer power) and `GET_MANA_FOR_SPELL` (`magic::script::GetManaForSpell`, the miracle's cost to create) |

## The story's own scripts for gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge program starts with the first land's control script, which runs the rest | done | `Game.cpp` starts the script machine on `LandControlAll` of the game's challenge program |
| The second land's script sets up both gods, their weights, attitudes, alliance and creatures | todo | the story never reaches the land: `LOAD_MAP` is empty in `src/CHLApi.cpp`; the script also waits on the computer player stubs above; see [khazar.md](khazar.md), [lethys.md](lethys.md) |
| The third and fifth lands' set-up scripts do the same for Lethys and Nemesis | todo | the story never reaches the land: `LOAD_MAP` is empty in `src/CHLApi.cpp`; the script also waits on the computer player stubs above; see [lethys.md](lethys.md), [nemesis.md](nemesis.md) |
| A shared script brings a god to the camera to speak, with a 10 second limit | todo | needs the computer god's hand (stubs above) |
| A step-by-step battle plan for Nemesis: every 30 seconds, by which towns the player holds and whether their belief is rising, it impresses, destroys or defends a town or attacks the player's creature, retuning his weights each time | n/a | the script ships but nothing runs it |
| Debug scripts start single story scripts on their own | n/a | developer tools in the script sources |
