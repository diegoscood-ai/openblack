# Players and diplomacy

How gods relate to each other: allies who share influence, enemies who attack, and the computer gods' attitude to the
player.

**Progress: 0/6 done, 2 partial — 17%**

## Allies and enemies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gods can be allies; an ally can use another's influence | todo | `SET_PLAYER_ALLY` and `GET_PLAYER_ALLY` are stubs in `src/CHLApi.cpp`; the ally fallback of `influence::CalculatePlayerInfluence` (`src/ECS/Influence/Influence.cpp`) gives 0 |
| Scripts set and ask who is allied | todo | `SetPlayerAlly` and `GetPlayerAlly` in `src/CHLApi.cpp` log "not implemented" |
| A computer god's attitude to each player drives whether it attacks | todo | no computer god mind in our tree; `SET_COMPUTER_PLAYER_ATTITUDE` is a stub; see [../rival_gods/ai.md](../rival_gods/ai.md) |
| Towns attack the towns of other players | todo | nothing in our tree makes a town attack another player's; a town only remembers its last attacker (`Town::aggressor`, `src/ECS/Components/Town.h`); see ../town/emergencies_and_aggression.md |
| Each player has their own colour on the map and their symbol | partial | each player's influence curtain is drawn in its colour (`influence::k_CircleColours`, `src/ECS/Influence/InfluenceCircles.cpp`); the belief symbols over towns are drawn per player (`src/Particles/TownBelief.cpp`), but computer players use a plain cell of the symbol sheet instead of their own images |
| Players can't cast or grab outside their influence | partial | the person's casts land only inside their influence (`src/Magic/CastRules.cpp`) and the hand knows when it is outside it; there are no other players' hands; see ../worship/influence.md |
