# Town emergencies and aggression

A town remembers who has harmed it. Harm by its own god makes it want mercy, harm by others makes it want protection.
When something important in it is damaged the town falls into a state of emergency: its people run, hide in buildings
and afterwards gather in the town.

**Progress: 5/16 done, 4 partial — 44%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Aggression

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each harm to a town's villager or building adds aggression against whoever did it, the first harm more | todo | only the last aggressor is recorded (`town_emergency::UpdateAggressor`, `src/ECS/Town/TownEmergency.cpp`); the per-player aggression slots are not ported |
| The more often a town is attacked, the less each attack adds | todo | the per-player aggression slots and their decaying factor are not ported (`UpdateAggressor`) |
| Aggression fades each turn into the wants of mercy (from its owner) and protection (from others) | todo | the protection and mercy desires read the town's `protectionDesire` and `mercyDesire` (`src/ECS/Town/TownDesire.cpp`), which stay 0: the town turn's player interaction step is a TODO (`src/ECS/Town/TownProcess.cpp`) |
| The town remembers its last attacker and when | partial | `Town::aggressor` and `aggressorTurn` (`src/ECS/Components/Town.h`), written by a destructive effect (`src/ECS/Effects/EffectValues.cpp`) and the physical shield; fire, crushed buildings and other harm write none yet |
| Villagers shelter under a magic shield while the town wants protection | partial | villagers walk under a shield and stand amazed (`src/ECS/Systems/Implementations/VillagerShield.cpp`), gated by the town's protection desire, which stays 0, so only the homeless react in a normal game; See ../villager/reactions.md |
| Damage done to a town's villagers lowers its belief in the attacker | todo |  |
| The town's attitude to the creature changes when the creature harms it | todo | See ../creature/ |

## Emergencies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Damage to the town centre, the storage pit or an abode on fire puts the town in a state of emergency | done | a storage pit or town centre knocked below its functional threshold (`abodes::ReduceLife`, `src/ECS/Abodes.cpp`), or either on fire (`town_emergency::ProcessTownEmergency`, `src/ECS/Town/TownEmergency.cpp`), starts it for 1200 turns |
| In an emergency villagers run at fleeing speed | done | (GameFloatRand(0.5) + 0.75) times the fleeing speed while the town is in its emergency (`src/ECS/VillagerSpeed.cpp`) |
| Villagers go and hide in a nearby building, then look to see if it is safe | todo | `GO_AND_HIDE_IN_NEARBY_BUILDING` and `LOOK_TO_SEE_IF_IT_IS_SAFE` are TODO rows of the villager state table (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`) |
| Villagers rush to fight the town's most important fire | partial | firefighting reactions (`src/ECS/Systems/Implementations/VillagerFire.cpp`); choosing the town's most important abode on fire is not ported |
| After the emergency, villagers gather at the town's congregation point, then go back to their lives | done | the call to every housed villager, the congregation walk and the stops (states 242 and 243, `src/ECS/Villager/VillagerEmergency.cpp`, `town_emergency::CallAllVillagersToTownEmergency`); `test/test_villager_repair.cpp` |
| While the emergency lasts the town stops worshipping, and its worship share comes back when it ends | done | (added) the worship percentage is saved and set to 0 during the emergency, then restored (`town_emergency::ProcessTownEmergency`, `src/ECS/Town/TownEmergency.cpp`) |
| Some villagers always react to their town's emergency, whatever they are doing | done | the original's per-state answer column (`k_TownEmergencyReaction`, `src/ECS/Villager/VillagerOriginalFns.h`) for every row |
| The town counts its dying and its recent deaths | partial | the deaths by reason and player and the last death's turn (`components::TownDeaths`, written by `src/ECS/Villager/VillagerDeath.cpp`); the dying are not counted |
| A town with no buildings left is completely destroyed and empties | todo | the empty town countdown runs but its end is a TODO (`src/ECS/Town/TownProcess.cpp`); `town_belief::SetTownEmpty` is not called |
