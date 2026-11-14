# Villager play and social life

Villagers do more than work and sleep: they play football on the town's pitch when the town wants relaxation (with
the football add-on), sit and chill out in the evening, gossip, dance round the town's artefacts and celebrate.

**Progress: 8/15 done, 0 chosen, 7 partial, 0 inferred — 78%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-states.md).

## Playtime and relaxation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town wants playtime once the game has run a while, and relaxation in the evening | done | `DesireForPlaytime` and `DesireForRelaxation` (`src/ECS/TownDesire.cpp`); `test/test_town_desire.cpp`; the playtime desire takes no villager, in the original (its 16-byte `CheckSatisfyPlaytimeDesire` returns 0) as in openblack (`CheckSatisfyPlaytimeDesire`, `src/ECS/Villager/VillagerSatisfy.cpp`) |
| Villagers taking up relaxation go to the best of the town's football pitch, its creature and its artefacts | partial 30% | `CheckSatisfyRelaxation` (`src/ECS/Villager/VillagerSatisfy.cpp`) through `ArtefactSystem::OfferRelaxation` and `relaxation_rules::Choose` (ported from raffclar PR #294, his 1.20 reading, not checked in v1.42): the artefacts' dance is offered; the creature draws no one, and a villager that chooses the pitch does nothing where the original walks it there to wait |
| Villagers sit and chill out outside their home or about the town in the evening | done | the idle branch's `GO_AND_CHILLOUT_OUTSIDE_HOME`, `SIT_AND_CHILLOUT` and `GO_AND_CHILLOUT_IN_TOWN` (`src/ECS/Villager/VillagerDecide.cpp`); see daily_routine.md; tests `VillagerDecideTest.GoAndChilloutOutsideHome`, `VillagerDecideTest.SitAndChillout`, `VillagerRoutine.SittingAboutTownIsWithinTheChillOutDistance` |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the football add-on, relaxing villagers walk to the pitch: a referee, two teams of five, and the rest to watch | partial 70% | football is on, as with the add-on (a chosen difference); `football::SetVillagerActivity` makes the referee, the two sides and the watchers and sends the players and watchers to their places; the match is played by `football::MatchTurn` and the football states (`VillagerFootball.cpp`), its rows on the football page; the whole feature: [../town/football.md](../town/football.md) |
| Goalkeepers, defenders and attackers each play their part: dribble, pass, shoot, lob, clear, mark and save | partial 60% | each side is ranked and given its positions, and each position plays from its own table (attackers 8 rows, defenders 8, the goalie 7: `villager::AssignFootballSubState`, states 73 to 75, `src/ECS/Systems/Implementations/VillagerFootball.cpp`), done on [the football page](../town/football.md) (row "At kick-off and every 15 s of play each side is re-ranked", run a2fb2sb, refs/story/evidence/a2fb2sb_football_match_stops.log line 395); the kicks' arcs are that page's row "Each kick launches the ball on an arc" (partial, no run) |
| The match waits for kick off, pauses, restarts after the ball goes out (dead ball) | partial 60% | ported (`football::Process` / `MatchTurn`, `WaitForPlayers`, the dead ball's states 78 to 82); on [the football page](../town/football.md): the wait and kick-off (row "With three or more a side, after 30 s", partial: the ball's move to the middle is not in the log), the pause after a goal (row "After a goal the scorers celebrate", done), the dead ball (row "When the ball leaves the pitch", partial: which first player is sent is not in the log) |
| A goal is celebrated by the scoring side and mourned by the other | done | `football::GoalHasBeenScored` puts the scorers in FOOTBALL_WON_GOAL and the others in FOOTBALL_LOST_GOAL for 70 turns, done on [the football page](../town/football.md) (row "After a goal the scorers celebrate and the others mourn"; quest_check PASS, runs a2fb1ssb, refs/story/evidence/a2fb1ssb_football_match.log line 941 and refs/story/evidence/a2fb1ssb_football_match_creature.log line 2129) |
| Spectators watch the match and do a Mexican wave | partial 50% | ported: watching (`villager::FootballWatchMatch`, state 83) and the wave (`villager::FootballMexicanWave`, driven by `MatchTurn`); on [the football page](../town/football.md) rows "They watch the pitch before kick-off" and "Mexican wave", both partial with no run |
| The ball's own physics, and the pitch making a new ball when it has none | partial 60% | the ball in flight is a physics body of its own (`DynamicsSystem`; [the football page](../town/football.md) row "A ball in flight is a physics body of its own", done); the pitch makes a new ball when it has none (`football::GetBall`, row "Whenever the pitch has no ball", partial: no run) |
| The player or the creature can pick up the ball and throw it | partial 40% | the hand grabs and throws the ball as any mobile object (`BallArchetype`, `HandPlacement`; [the football page](../town/football.md) row "The hand can pick up and throw the ball", partial: the pitch's own ball and its in-hand help missing); the creature picks it up and throws it at a goal through its catch (`GoalieCatchBall`, row "The creature can act on a villager who is playing", partial: no run) |
| A pitch under construction draws villagers to build it | done | a pitch's site is built like any other (`src/ECS/BuildingSites.cpp`); its own construction state does nothing, as in the original |

## Social life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers dance round the town's artefacts; a dance in a town that isn't the artefact owner's gives the owner belief | done | ported from raffclar PR #278 and checked in the v1.42 exe: the town's artefact turn and the dance (`ArtefactSystem::ProcessTown` / `Process`, as `TownArtifact::Process` 0x425FB0), the belief every 100 turns (`artefact_rules::DancingBelief`, as `Town::GiveBeliefForArtifactIfNecessary` and `TownArtifact::GetImpressiveValueForDancing` 0x4268E0); tests `ArtefactSystemTest.WhenTheDanceStartsEveryDancerDancesForTheDancesLength`, `ArtefactRules.ATownGivesBeliefForADanceByWorthImpressivenessAndDancersPerHead`; the rows are in [artefacts](../town/artefacts.md) (read 2026-11-05) |
| Villagers congregate in town after an emergency | done | states 242 and 243 (`src/ECS/Villager/VillagerEmergency.cpp`); `test/test_villager_repair.cpp`; See ../town/emergencies_and_aggression.md |
| Villagers dance round a newly finished building | done | the new building's reaction (15) sets out the small or large building's dance and its takers dance there in `DANCE_WHILE_REACTING` (203, `villager_dance::DanceWhileReacting`); tests `NewBuildingReactionTest.*` (`test/test_villager_reactions.cpp`); the 203 state itself: test `NewBuildingReactionTest.DancingWhileReactingWalksToItsPlaceInTheDanceOrDecidesAgainOutOfIt`; see reactions.md |
| Villagers sing and chant while worshipping | done | the worship dance (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) and the tribe's chants (`GameMusic::ProcessChantMusic`, `src/Sound/GameMusic.cpp`); `test/test_worship.cpp`; See ../worship/ |
