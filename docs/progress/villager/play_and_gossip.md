# Villager play and social life

Villagers do more than work and sleep: they play football on the town's pitch when the town wants playtime, sit and
chill out in the evening, gossip, dance round the town's artefacts and celebrate.

**Progress: 4/19 done, 1 partial — 24%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../bw1-notes/villagers.md).

## Playtime and relaxation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town wants playtime once the game has run a while, and relaxation in the evening | done | `DesireForPlaytime` and `DesireForRelaxation` (`src/ECS/Town/TownDesire.cpp`); `test/test_town_desire.cpp` |
| Villagers taking up relaxation sit and chill out outside their home or in town | partial | villagers sit and chill out from the idle branch (`GO_AND_CHILLOUT_OUTSIDE_HOME`, `SIT_AND_CHILLOUT`, `src/ECS/Villager/VillagerDecide.cpp`); relaxation itself sets an activity of the pitch, the creature or the artefacts, none of them ported (`CheckSatisfyRelaxation`, `src/ECS/Villager/VillagerSatisfy.cpp`) |
| Villagers kick a ball about when one lies near | todo | `CHECK_INTERACT_WITH_BALL` is a TODO row |
| Children play rather than work | todo |  |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers walk to the football pitch when it is playtime and take positions in two teams | todo | the pitch is a building only (`src/ECS/Archetypes/AbodeArchetype.cpp`) and football is off; the whole feature: [../town/football.md](../town/football.md) |
| Goalkeepers, defenders and attackers each play their part: dribble, pass, shoot, lob, clear, mark and save | todo | the football states are TODO rows |
| The match waits for kick off, pauses, restarts after the ball goes out (dead ball) | todo |  |
| A goal is celebrated by the scoring side and mourned by the other | todo |  |
| Spectators watch the match and do a Mexican wave | todo |  |
| A referee and the ball's own physics | todo |  |
| The player or the creature can pick up the ball and throw it | todo | no ball; See ../hand/ |
| A pitch under construction draws villagers to build it | done | a pitch's site is built like any other (`src/ECS/Town/BuildingSites.cpp`); its own construction state does nothing, as in the original |

## Social life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers stop to gossip with each other | todo | `VILLAGER_GOSSIPS` is a TODO row |
| Housewives gossip round the storage pit | n/a | `HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT` is a TODO row. Our wiki differs: the housewife's day, the gossip included, is dead code in this version of the original ([villagers](../../bw1-notes/villagers.md#pregnancy-and-births)) |
| Villagers tell others about something interesting they found, walking up to them | todo | the telling states (`INITIALISE_TELL_OTHERS_ABOUT_OBJECT` and the rest) are TODO rows |
| Villagers dance round the town's artefacts, which impresses the town | todo | See ../town/artefacts.md |
| Villagers congregate in town after an emergency | done | states 242 and 243 (`src/ECS/Villager/VillagerEmergency.cpp`); See ../town/emergencies_and_aggression.md |
| Villagers celebrate when their town is won over | todo | the take-over's celebration is not ported (`town_belief::TakeOverTown`) |
| Villagers dance while reacting to something wonderful | todo | `DANCE_WHILE_REACTING` is a TODO row |
| Villagers sing and chant while worshipping | done | the worship dance (`src/ECS/Systems/Implementations/VillagerWorship.cpp`) and the tribe's chants (`GameMusic::ProcessChantMusic`, `src/Audio/Services/GameMusic.cpp`); See ../worship/ |
