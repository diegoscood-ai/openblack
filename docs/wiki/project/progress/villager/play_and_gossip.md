# Villager play and social life

Villagers do more than work and sleep: they play football on the town's pitch when the town wants relaxation (with
the football add-on), sit and chill out in the evening, gossip, dance round the town's artefacts and celebrate.

**Progress: 5/16 done, 0 chosen, 2 partial, 2 inferred — 38%**

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
| With the football add-on, relaxing villagers walk to the pitch: a referee, two teams of five, and the rest to watch | pending | football is fixed off, as in the original without the add-on (`k_FootballEnabled`, `src/ECS/Scaffolds.cpp`); every football state is a TODO row; the whole feature: [../town/football.md](../town/football.md) |
| Goalkeepers, defenders and attackers each play their part: dribble, pass, shoot, lob, clear, mark and save | pending | the football states are TODO rows; see ../town/football.md |
| The match waits for kick off, pauses, restarts after the ball goes out (dead ball) | pending | see ../town/football.md |
| A goal is celebrated by the scoring side and mourned by the other | pending | see ../town/football.md |
| Spectators watch the match and do a Mexican wave | pending | `FOOTBALL_MEXICAN_WAVE` is a TODO row; see ../town/football.md |
| The ball's own physics, and the pitch making a new ball when it has none | pending | no ball; see ../town/football.md |
| The player or the creature can pick up the ball and throw it | pending | no ball; See ../hand/ |
| A pitch under construction draws villagers to build it | done | a pitch's site is built like any other (`src/ECS/Town/BuildingSites.cpp`); its own construction state does nothing, as in the original |

## Social life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers stop to gossip with each other | inferred | the only way into `VILLAGER_GOSSIPS` (169) is `CHECK_INTERACT_WITH_VILLAGER` (179), which sends two villagers of one player that cannot breed to meet and gossip; 179 is entered only from a disciple's inspection of what it was put down by, and whether any disciple put down by a villager reaches it is not traced; both are TODO rows |
| Villagers dance round the town's artefacts, which impresses the town | inferred | ported from raffclar PR #278 (`ArtefactSystem`); the town's artefact turn, the dance and its belief are his 1.20 reading, not checked in v1.42 (only the moved artefact's dance and footpath are: [towns](../../../beings/towns.md#the-towns-belief)); see ../town/artefacts.md |
| Villagers congregate in town after an emergency | done | states 242 and 243 (`src/ECS/Villager/VillagerEmergency.cpp`); `test/test_villager_repair.cpp`; See ../town/emergencies_and_aggression.md |
| Villagers dance round a newly finished building | partial 85% | the new building's reaction (15) sets out the small or large building's dance and its takers dance there in `DANCE_WHILE_REACTING` (203, `villager_dance::DanceWhileReacting`); tests `NewBuildingReactionTest.*` (`test/test_villager_reactions.cpp`); the 203 state itself has no test; see reactions.md |
| Villagers sing and chant while worshipping | done | the worship dance (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) and the tribe's chants (`GameMusic::ProcessChantMusic`, `src/Audio/GameMusic.cpp`); `test/test_worship.cpp`; See ../worship/ |
