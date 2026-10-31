# Players and diplomacy

How gods relate to each other: allies who share influence, enemies who attack, and the computer gods' attitude to the
player.

**Progress: 2/6 done, 3 partial — 58%**

## Allies and enemies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gods can be allies; an ally can use another's influence | done | the alliances are kept for every player all game (`components::PlayerAlliances`, `src/ECS/PlayerAlliances.h`); outside their own influence a player has the first ally's above a strength of 0.1 (`influence::CalculatePlayerInfluence`, `ecs::alliances::AlliesInfluence`); `test/test_influence.cpp` |
| Scripts set and ask who is allied | done | `SetPlayerAlly` sets both ways (`ecs::alliances::SetAlliance` (`src/ECS/PlayerAlliances.cpp`)), `GetPlayerAlly` reads `ecs::alliances::Strength`; `test/test_player_alliances.cpp` |
| A computer god's attitude to each player drives whether it attacks | partial | the attitude is kept and set by the scripts (`ComputerMind::attitude`, `computer_player::SetAttitude`); no score function reads it yet; see [../rival_gods/ai.md](../rival_gods/ai.md) |
| Towns attack the towns of other players | pending | nothing in our tree makes a town attack another player's; a town only remembers the aggression held against each player and its last attacker (`Town::aggression`, `src/ECS/Components/Town.h`); see ../town/emergencies_and_aggression.md |
| Each player has their own colour on the map and their symbol | partial | each player's influence curtain is drawn in its colour (`influence::k_CircleColours`, `src/ECS/Influence/Influence.h`, drawn by `src/ECS/Influence/InfluenceCircles.cpp`); the belief symbols over towns are drawn per player (`src/Particles/TownBelief.cpp`), but computer players use a plain cell of the symbol sheet instead of their own images |
| Players can't cast or grab outside their influence | partial | the person's casts land only inside their influence (`src/Magic/CastRules.cpp`) and the hand knows when it is outside it; there are no other players' hands; see ../worship/influence.md |
