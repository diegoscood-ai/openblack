# How a computer god wins and loses towns

A computer god spreads its influence by winning towns the way a person does: impressing them, meeting their needs and
attacking its rivals' towns; it looks after its own towns and worshippers so they keep believing and praying. The story
lands add their own rules on top, holding some towns back until the right moment.

**Progress: 7/26 done, 0 chosen, 8 partial, 0 inferred — 37%**

See [../town/belief_and_conversion.md](../town/belief_and_conversion.md) for belief and conversion,
[../worship/influence.md](../worship/influence.md) for influence and [ai.md](ai.md) for how the god decides.

## What it starts with

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts give each god its towns, by owner name | done | `CREATE_TOWN` (`FeatureScriptCommands::CreateTown`, `TownArchetype`, the town's owner); test `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Land scripts give each god its temple | done | `CREATE_CITADEL` (`CitadelArchetype`); test `CitadelPlanTest.CreateCitadelIsBuilt` |
| Land scripts give each god worship sites by its temple, one per tribe | done | `CREATE_WORSHIP_SITE` (`magic::script::CreateWorshipSite`); tests `LandScriptMeshCommandsTest.AWorshipSiteIsTheTribesSiteOfThePlayersTempleNotTheLinesPlace` and `LandScriptMeshCommandsTest.EachTribeGetsItsOwnWorshipSiteAroundTheTemple` |
| Land scripts set how much each town believes in each god, and the most it can | done | `SET_TOWN_BELIEF` and `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetBeliefInPlayer`, `SetCap`), read by the belief rules; test `TownBeliefTest.Scripts` |
| A god's influence covers its temple and its towns | done | `src/ECS/Influence`; tests `InfluenceTest.townRadiusAndPlayer`, `InfluenceTest.citadelStoryInfluence` |
| Its influence ring is drawn in its colour | done | `influence::k_CircleColours` (`src/ECS/Influence/InfluenceCircles.cpp`), drawn by `src/Graphics/RendererInfluence.cpp`; test `InfluenceTest.circlesHideTheirContact` |

## Choosing a town

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It finds the town it most wants to win, from every player's towns | partial 30% | the score rule's choice (`computer_player::BestTownToTake`, `DesireToBeTakenOver`, `src/ECS/ComputerPlayerScores.h`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#defeatplayer) |
| It judges how much it wants to win or destroy each town | partial 30% | the score rules (`computer_player::Score`, `DesireToBeTakenOver`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#defeatplayer) |
| It finds which town of its own most needs defending, food, wood or people | partial 30% | the score rules (`computer_player::Score`: TownWithHighestFoodDesire, TownWithHighestWoodDesire, the town harmed most lately by `scores::HighestRecentHarm`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#towninteract) |
| It notices which of its towns has just been attacked | partial 30% | the score rules read a town's harm lately (`scores::HighestRecentHarm`, from each player's last attack the town keeps, `town_aggression::Record::lastEffects`), pinned to the oracle (`ComputerMindScores.ATownsHarmLatelyIsTheOriginals`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#playerinteract-and-reacttoplayer); a town remembers its last attacker (`Town::aggressor`), and `GET_TIME_SINCE_OBJECT_ATTACKED` reads how long ago (`ecs::town_aggression::SecondsSinceAttacked`) |
| It finds another town to take food or wood from | partial 30% | the score rule (`scores::ScoreTakeFromOtherTown`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#towninteract) |

## Winning towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| By sending its creature to impress the town | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)); see [creatures.md](creatures.md) |
| By throwing things near the town to impress it | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |
| By casting impressive miracles at it | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)); see [magic.md](magic.md) |
| By meeting its food needs: food miracles, fields, fish farms or food carried from another town to its storehouse | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |
| By meeting its wood needs (never chosen when expanding, see [ai.md](ai.md)) | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |
| Belief it wins shows over the town in its colour, as the person's does | partial 60% | the belief symbols over each town centre are drawn for every player in its colour (`particles::town_belief`, `src/Particles/TownBelief.cpp`), but no god wins belief; see [../town/belief_and_conversion.md](../town/belief_and_conversion.md) |
| A town changes hands when its belief in the god passes what it needs | done | the same rule as for the person: `town_belief::Fold` and `town_belief::TakeOverTown` (`src/ECS/Town/TownBelief.cpp`); tests `TownBeliefTest.FoldConversion`, `TownBeliefTest.TakeOverTownDoesTheRest` |

## Looking after its towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It places scaffolds from its workshop and combines small ones into bigger ones | partial | placing one is ported: the scaffold is picked up and dropped at the place its node chose (`computer_hand::HandActions`, FindPlaceToUseScaffoldForBuilding, run by the mind's turn, `computer_player::TakeTurn`; test `ComputerHandActions.TheScaffoldGoesToTheMarker`); combining scaffolds is not; its score rules are written ([ai.md](ai.md)) |
| It fills its workshop with wood | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |
| It makes breeder disciples to grow its towns | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |
| It makes disciples to meet a town's needs | pending | the mind's turn runs (`computer_player::TakeTurn`), but the action is not ported (`computer_hand::HandActions`); its score rules are written ([ai.md](ai.md)) |

## Story rules

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the second land, a script checks each of eleven towns for a change of owner every tenth of a second and makes the gods react | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [khazar.md](khazar.md) and [lethys.md](lethys.md) |
| On the fifth land, Nemesis's home town also counts as won if it shrinks to six or fewer (unconfirmed whether this script runs) | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Scripts count each player's towns to decide when gods die and vortexes open | done | `GET_PLAYER_TOWN_TOTAL` (`src/CHLApi.cpp`): the size of `map_cells::TownsOf(player)`; test `MapCells.TownsByPlayerThenAge` |
| Scripts compare each player's influence at the camera to make gods react to trespassing | partial 50% | dormant: the influence native works (`magic::script::GetInfluence`; it matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))), but play has not yet reached these lands (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached) |
| The person gets "virtual influence" on the third and fifth lands | pending | the virtual influence native is real and matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)); see [../worship/influence.md](../worship/influence.md) |
