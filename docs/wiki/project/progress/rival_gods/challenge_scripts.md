# Script control of computer gods

The land scripts and the challenge scripts create the computer gods and steer them: they choose which players are
computer gods, give them creatures, tune their minds, move their hands, force and queue their actions, set how they feel
about each other and switch them off when they are beaten. Script function names are given where needed to find them.

**Progress: 33/44 done, 0 chosen, 5 partial, 0 inferred — 81%**

All challenge script functions live in `src/CHLApi.cpp`; land script commands in
`src/LHScriptX/FeatureScriptCommands.cpp`. See [../story/](../story/) for the script language and
[../scripts/](../scripts/) for the rest of its functions.

## Land script commands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Making a player a computer god (`TOGGLE_COMPUTER_PLAYER`, with a player name and 1) | partial 40% | `FeatureScriptCommands::ToggleComputerPlayer` adds the player if it has none (`PlayerArchetype`) and, with on, marks it a computer player (`PlayerMagic::playerType` 2; off leaves it, as the original) and switches its mind on (`ComputerPlayerSystem::SwitchOn`), whose start then runs once on the land (`computer_player::Start`, tests `ComputerPlayerStart.*`); the mind's own turn weighs and acts each turn (`computer_player::TakeTurn`), but only 6 of its 59 actions are ported |
| Loading a god's creature from a mind file, of a species, at a place (`CREATE_CREATURE_FROM_FILE`) | partial 70% | `FeatureScriptCommands::CreateCreatureFromFile` (`CreatureArchetype`, the mind through the resource cache); it faces a fixed way and takes our starting size |
| Setting a god's personality weight from the land file | done | `FeatureScriptCommands::SetComputerPlayerPersonality` → `ComputerPlayerSystem::SetPersonality`, the player by name, the node's name and the weight (no shipped land uses it); see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-personality-file); test `PlayerSystem.theLandScriptsPersonalityKeywordsReachTheComputerPlayers` |
| Loading a god's personality from the land file | done | `FeatureScriptCommands::LoadComputerPlayerPersonality` → `ComputerPlayerSystem::LoadPersonality`, the player by number and the file's name (no shipped land uses it); see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-personality-file); test `PlayerSystem.theLandScriptsPersonalityKeywordsReachTheComputerPlayers` |
| Every land that makes a computer god runs the toggle before that god's own towns, temple and worship sites are made (the land's first town, player one's or a neutral one, may come before it, as on Lands 2 and 3) | done | `Script::Load` runs the commands in order; tests `LandScriptContents.AComputerGodsLinesAreTheTownsTempleAndSitesItOwns` and `LandScriptContents.realEveryComputerGodIsSwitchedOnBeforeItsTownsAreMade` |

## Taking over the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god's hand works through a line of steps, one stage a turn, with a move put in front of the steps that need one | done | `computer_hand::Step` (`src/ECS/ComputerHand.h`) on the player's `ComputerHand`, the original's table of moves in front, stepped by `ComputerPlayerSystem::ProcessTurn` while the mind is on, not paused and not held by the wide screen; see [../../../hand/computer-hands.md](../../../hand/computer-hands.md); tests `ComputerHand.TheMoveGoesInFrontWithACopyOfTheFirstArgument`, `ComputerHand.AStageGoingOnStaysOnItsStage` |
| Moving a god's hand to a place at a speed, optionally at a fixed height; this takes the hand from its mind | done | `MOVE_COMPUTER_PLAYER_POSITION` → `ComputerPlayerSystem::MoveHand`: switches the mind on, the hand held, across the land 10 m above it or straight (`computer_hand::MoveOverGround` / `MoveStraight`, pinned to the original); tests `ComputerHandMoves.OverTheGroundAcrossTheLandHeldAboveIt`, `ComputerHandMoves.StraightGoesUpAndDownToo` |
| Setting a god's hand straight to a place, optionally at a fixed height | done | `SET_COMPUTER_PLAYER_POSITION` → `ComputerPlayerSystem::PutHand` (`computer_hand::PutAt` at the hand's next turn); test `ComputerHandMoves.PutAtHoversOrKeepsThePointsHeight` |
| Reading where a god's hand is, for markers and for casting from it | done | `GET_COMPUTER_PLAYER_POSITION` → `HandOnGround`: the land under the hand as of its last turn (the original reads its map place, not its height); test `ComputerHandScript.TheLandUnderTheHandAsOfItsLastTurn` |
| Asking whether a god's hand is ready (its action finished) | done | `COMPUTER_PLAYER_READY` → `IsHandReady`: the hand has no step left, as after a forced action; test `ComputerHand.ReadyOnlyWithoutSteps` |
| Releasing a god back to its mind | done | `RELEASE_COMPUTER_PLAYER` → `Release`: not paused, the hand no longer held, its steps at the player's speed, the steps and the busy flag kept, as the original ([computer hands](../../../hand/computer-hands.md#the-scripts-natives)); the mind's own choices run, with 6 of its 59 actions ported ([ai.md](ai.md)); tests `ScriptQueryNatives.ReleaseAndClearComputerPlayerActionsAskForTheGamesPlayer`, `ComputerPlayerScripts.ReleasingTheHandUnpausesTheMindAndGivesTheStepsThePlayersSpeed` |
| Pausing and unpausing a god | done | `ENABLE_DISABLE_COMPUTER_PLAYER` (native 345) → `ComputerPlayerSystem::Pause`: the mind paused, the hand held, its steps at the player's speed; a paused mind takes no turn (`ComputerPlayerSystem::ProcessTurn`); tests `ComputerHandScript.APauseHoldsTheHandAndGivesThePlayersSpeed`, `ComputerPlayerStart.APausedMindWaitsToStartUntilItGoesOn` |
| Switching a god off for good (Khazar, Lethys and Nemesis when beaten) | done | `ENABLE_DISABLE_COMPUTER_PLAYER` (native 311) → `ComputerPlayerSystem::SwitchOn`, the player's type kept; test `ComputerPlayerStart.SwitchingOnMakesTheMindDueAndSwitchingOffStopsIt` |
| Setting a god's hand speed (150 to 350 in the story) | done | `SET_COMPUTER_PLAYER_SPEED` → `SetHandSpeed`: the player's speed and the steps under way; test `ComputerHandScript.TheSpeedIsThePlayersAndTheSteps` |
| A place a given distance in front of the camera, where gods come to speak | done | `GET_FACING_CAMERA_POSITION` (`GetFacingCameraPosition`, `src/CHLApi.cpp`): the camera's position plus the distance along its view direction, as the original rounds it (since 2026-10-26, `ScriptCameraDetails.AFacingPositionGoesAlongTheViewDirectionAsTheOriginalRoundsIt`) |
| Getting a god as an object, for "get computer player 2" | done | `CALL_COMPUTER_PLAYER`: the player's entity as a found thing, 0 without one, as the original gives its computer player ([computer hands](../../../hand/computer-hands.md#the-scripts-natives); 🔍 a player with no entity, there); test `ScriptQueryNatives.CallComputerPlayerGivesThePlayersEntityAsAFoundThing` |

## The camera and gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera's focus follows a god's hand | pending | `SET_FOCUS_FOLLOW_COMPUTER_PLAYER` is a stub; the script camera's computer-hand branch is not ported (`src/Camera/ScriptCamera.cpp`); see [../camera/](../camera/) |
| The camera's position follows a god's hand | pending | `SET_POSITION_FOLLOW_COMPUTER_PLAYER` is a stub in `src/CHLApi.cpp` |
| Asking whether the hand is near the camera within a radius | pending | the hand's position is real now (the land under the hand as of its last turn; the native matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))); not checked in play |

## Tuning the mind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Names the story uses: expanding influence, defeating a player, winning a town, destroying a town, defending a town, attacking a creature with miracles, reacting to an aggressive creature | done | as given in the scripts: "ExpandInfluence", "DefeatPlayer", "TakeOverTown", "DestroyTown", "DefendTown", "AttackCreatureWithSpells", "ReactToAggressiveCreature"; each is a row of the mind's tree (`computer_player::k_MindRows`), and the setter sets every node the name names (`computer_player::FindNodes`); test `ComputerMindScripts.APersonalityGoesToEveryNodeTheNameNames` (Land 2's "ExpandInfluence") |

## Forcing and queueing actions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Forcing an action by name on one or two objects, ahead of the god's own choice | partial 30% | `FORCE_COMPUTER_PLAYER_ACTION` → `ComputerPlayerSystem::ForceAction` (`computer_hand::ForceAction`, the mind's node names in the game's order); six of the mind's actions are ported (`computer_hand::HandActions`), the story's two pick-up-and-drop ones among them |
| Queueing an action by name on one or two objects | done | `QUEUE_COMPUTER_PLAYER_ACTION` → `QueueAction`; a free hand forces the next one in its turn; an entry whose thing is gone is dropped; test `ComputerHandActions.TheQueueKeepsItsOrderAndDropsGoneThings` |
| What is seen of a god's hand: no hand, only the thing it holds, drawn where the hand was as its last turn of steps began | done | `RenderingSystem` draws each computer hand's held thing at `computer_hand::HeldDrawPoint` (the original's `Object::DrawInHand`); no hand, symbol or colour, as the original; tests `ComputerHandHeldDraw.ATurnStartsTheSlideAtTheTurnsPoint`, `ComputerHandHeldDraw.ItSlidesOverTheTimeAndStopsAtTheHand` |
| A god's hand picks a thing up, carries it and drops it gently at a place, as its steps for the story's actions | done | `computer_hand::PickUpTurn` / `DropTurn`, `GameHandRunner::TakeIntoHand` / `LetGo` (`src/ECS/Systems/Implementations/ComputerPlayerSystem.cpp`); a thing already in a hand is refused, as the original; tests `ComputerHandHolding.APickUpTakesTheThingIntoTheHand`, `ComputerHandHolding.TheDropLetsGo` |
| Clearing a god's queued actions | done | `GAME_CLEAR_COMPUTER_PLAYER_ACTIONS` → `ClearActions`: the hand's steps given up (it lets go of what it holds) and the queue dropped, the hold, the busy flag and the speeds kept, as the original ([computer hands](../../../hand/computer-hands.md#the-scripts-natives)); tests `ScriptQueryNatives.ReleaseAndClearComputerPlayerActionsAskForTheGamesPlayer`, `ComputerPlayerScripts.ClearingTheActionsGivesUpTheStepsAndTheQueueButNotTheScriptsHold` |
| Actions the story forces or queues: picking up and dropping an object at a place, placing a scaffold, and casting wood on a town | partial 60% | "DIRECTPickupAndDropObject" and "FindPlaceToUseScaffoldForBuilding" are ported (`computer_hand::HandActions`; tests `ComputerHandActions.TheDirectPickUpAndDropGivesUpTheStepsAndGivesTwo`, `ComputerHandActions.TheScaffoldGoesToTheMarker`); the queued "ShallCastWoodSpell" (🔍 whether it runs, see the page) is not. "GetCreaturetoHelp", "TakeOverTown", "DestroyTown" and "DefendTown" are forced only by Nemesis's battle plan, which nothing runs ([not in the original](../../not-in-the-original.md#rival_godschallenge_scripts)) |

## Feelings and alliances

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Setting one god's attitude to a player (kept doubled) | done | `SET_COMPUTER_PLAYER_ATTITUDE` (378) → `computer_player::SetAttitude` (`ComputerMind::attitude`); see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-scripts-set-the-mind); test `ComputerMindScripts.AnAttitudeIsKeptAsTwiceTheValueAndReadAsHalf` |
| Reading it back (halved) | done | `GET_COMPUTER_PLAYER_ATTITUDE` (379) → `computer_player::Attitude`: a mind no script set reads 0.5, a number naming no player 0; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-scripts-set-the-mind); test `ComputerMindScripts.AnAttitudeIsKeptAsTwiceTheValueAndReadAsHalf` |
| Asking how allied two players are | done | `GET_PLAYER_ALLY`: the second player's strength towards the first (`ecs::alliances::Strength`), 0 without both players; tests `PlayerAllyNatives.TheStrengthIsTheScriptsValueAsItIs`, `PlayerAlliances.WithoutAPlayerSystemNobodyIsAllied` |

## Questions the story asks about gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A player's influence, or raw influence, at a place | done | `GET_INFLUENCE` (`magic::script::GetInfluence`, `src/Magic/Script/CHLInfluence.cpp`) through `influence::CalculatePlayerInfluence`, with or without allies (the allies' influence where the player has none of its own, `ecs::alliances::AlliesInfluence`); test `InfluenceTest.aScriptReadsAPlayersInfluenceAtAPlaceWithItsAlliesUnlessRaw` |
| How many towns a player has | done | `GET_PLAYER_TOWN_TOTAL` (`src/CHLApi.cpp`): the size of `map_cells::TownsOf(player)`, as the original's owner's list count; see [../../../beings/towns.md](../../../beings/towns.md#town-lists-and-queries); test `MapCells.TownsByPlayerThenAge` |
| Who owns a town, and a town by its number | done | `GET_TOWN_WITH_ID` gives the town with that script number (`ecs::town_queries::FindTownWithID`), and a town's player is read through its owner (`ThingPlayerProperty`, `src/CHLApi.cpp`); unverified against v1.42; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md); tests `TownScriptIdTest.AScriptTownIsFoundByItsId`, `ScriptQueryNatives.PlayerOfATownIsItsOwnerNumberedFromOne` |
| How much a town believes in a player | done | `BELIEF_FOR_PLAYER` gives a town's belief in the player (`ecs::town_belief::GetBeliefInPlayer`); read against v1.42 ([town belief and conversion](../town/belief_and_conversion.md#conversion)); test `ScriptQueryNatives.BeliefForPlayerOfATownIsItsBeliefInTheGamePlayer` |
| Setting a town's belief, or its belief relative to now, for a player | done | ported from raffclar PR #236, read against v1.42 ([town belief and conversion](../town/belief_and_conversion.md#conversion)): `SET_PLAYER_BELIEF` and `OBJECT_RELATIVE_BELIEF` (`src/CHLApi.cpp`); test `TownBeliefTest.ScriptSetsBeliefAsAShareOfBeliefInTheOwner` |
| How long since a player attacked an object | done | `GET_TIME_SINCE_OBJECT_ATTACKED` gives the seconds since the player last attacked the town (`ecs::town_aggression::SecondsSinceAttacked`); unverified against v1.42; test `TownAggression.TheScriptsAreToldTheSecondsSinceAnAttackStillHeld` |
| A player's creature, by player number | done | `CALL_PLAYER_CREATURE` (`CallPlayerCreature` in `src/CHLApi.cpp`: the player's primary creature, `PlayerSystemInterface::GetPrimaryCreature`, which the leash's `PlayersCreature` also gives); test `PlayerCreatureNativesTest.CallPlayerCreatureGivesTheCreatureThePlayerLeads` |
| A player's alignment, to choose a god's lines | done | `GET_ALIGNMENT` (`ecs::effects::alignment::Get`); test `Alignment.scriptsReadTheAlignmentAndAddToItWithinMinusOneAndOne` |

## Gods' creatures and miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loading a creature of a species from a mind file for a player at a place | done | `LOAD_CREATURE` (`src/CHLApi.cpp` → `ecs::player_creature::ScriptLoadCreature`, `src/ECS/PlayerCreature.cpp`): the overwrite message, nothing for a player the land has not made, then the shared `MakeFromMindFile`; tests `ScriptLoadCreatureTest.TheScriptsSpeciesIsMadeAtThePointOnTheGroundThenLedAndGrownUp`, `ScriptLoadCreatureTest.APlayerWhoLeadsACreatureIsToldSoAndTheNewOneIsLedInstead` |
| Casting a miracle from a god's hand at a place | partial 60% | the scripts' cast at a place works as the original's (`magic::script::SpellAtPos`, `src/Magic/Script/CHLSpells.cpp`, [spell-core](../../../magic/spell-core.md#chl-script)), cast by the neutral player from the point given; the god's hand position is the land under his hand as of its last turn (the hand's position native matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))) |
| Naming it, growing it up, scaling it, teaching it, setting its body and alignment, making friends | partial 80% | the stage move is the original's (`ecs::player_creature::SetDevelopmentStage`, also over the mind file the creature takes up after it); the naming (`player_creature::SetName`), the autoscale (`player_creature::Autoscale`), `CREATURE_SET_KNOWS_ACTION` and `CREATURE_FORCE_FRIENDS` are real; see [creatures.md](creatures.md) |
| Casting a miracle at an object, such as a creature, from a place | done | `SPELL_AT_THING` (`magic::script::SpellAtThing`), as the neutral player (unconfirmed whose it is in the game); test `MiracleCoreScriptCast.AScriptMiracleOnAThingIsTheNeutralPlayersWithTheScriptsRadiusTimeAndCurl` |
| Topping up an icon's prayer power and reading a miracle's cost | done | `GAME_SET_MANA` (`magic::script::GameSetMana`, the worship site's prayer power) and `GET_MANA_FOR_SPELL` (`magic::script::GetManaForSpell`, the miracle's cost to create); test `MiracleCoreScript.TheManaForAMiracleIsTheCostOfTheEffectRowItsRecordNames` |

## The story's own scripts for gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge program starts with the first land's control script, which runs the rest | done | `Game.cpp` starts the script machine on `LandControlAll` of the game's challenge program; test `IslandLoad.LandOneStartsTheWholeStorysControlScriptAsANewGame` |
| The second land's script sets up both gods, their weights, attitudes, alliance and creatures | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the computer player natives it waits on are real now, but for the camera's following of a hand; see [khazar.md](khazar.md), [lethys.md](lethys.md) |
| The third and fifth lands' set-up scripts do the same for Lethys and Nemesis | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the computer player natives it waits on are real now, but for the camera's following of a hand; see [lethys.md](lethys.md), [nemesis.md](nemesis.md) |
| A shared script brings a god to the camera to speak, with a 10 second limit | pending | the god's hand natives are real now; the camera's following of the hand is a stub (above); not reached in play |
