# Town emergencies and aggression

A town remembers who has harmed it. Harm by its own god makes it want mercy, harm by others makes it want protection.
When something important in it is damaged the town falls into a state of emergency: its people run, stop worshipping
and afterwards gather in the town.

**Progress: 12/14 done, 0 chosen, 2 partial, 0 inferred — 97%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## Aggression

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each harm to a town's villager or building adds aggression against whoever did it, the first harm more | done | the town's aggression record keeps an amount per player, the first harm adding the town info's bonus (`town_aggression::Attacked`, `src/ECS/TownAggression.cpp`, from `town_emergency::UpdateAggressor`); `test/test_town_aggression.cpp` (OnlyTheFirstHarmOfAPlayerGetsTheAddition) |
| The more often a town is attacked, the less each attack adds | done | each attack counts by its player's multiplier, worn down by a tenth, which recovers while the want is small (`town_aggression::Attacked`, `town_aggression::ProcessTurn`, `src/ECS/TownAggression.cpp`); `test/test_town_aggression.cpp` (AnAttackRisesByItsMultiplierWhichThenWearsDown) |
| Aggression fades each turn into the wants of mercy (from its owner) and protection (from others) | done | `town_aggression::ProcessTurn` at the town turn (`src/ECS/Town/TownProcess.cpp`) fades the aggression into the protection and mercy that the desires read next turn (`DesireForProtection`, `DesireForMercy`, `src/ECS/TownDesire.cpp`); `test/test_town_aggression.cpp` (ItFadesEachTurnAndANeutralTownWantsOnlyProtection) |
| The town remembers its last attacker and when | done | the aggressor, the turn and what each player's last attack did (`town_emergency::UpdateAggressor`, `src/ECS/Town/TownEmergency.cpp`), written by a destructive effect, its things burning, a villager of it hurt by a physics impact and a physical shield of it struck, which names the thing that struck it; `test/test_repairs.cpp`, `test/test_town_aggression.cpp` (EachPlayersLastAttackKeepsWhatItDid) |
| Villagers shelter under a magic shield while the town wants protection | partial 85% | villagers walk under a shield and stand amazed (`src/ECS/Systems/Implementations/VillagerShield.cpp`), gated by the town's protection desire (now fed by the aggression) and its last attack's turn; `test/test_shield.cpp` covers only a town with no protection; (approximate) the turns limit is read from villager info row 0, not the villager's own row; See ../villager/reactions.md |
| The town's attitude to the creature changes when the creature harms it | done | the town fears a creature whose last carried-out action was a hurl, stomp, kick, aggressive miracle or setting fire, or eating one of its people, and respects gifts, building, repairs, kind miracles, dances and games (`town_creature_attitude::UpdateAttitudes`, `src/ECS/Town/TownCreatureAttitude.cpp`); `test/test_town_creature_attitude.cpp`; see ../creature/ |

## Emergencies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit or the town centre put out of use or on fire puts the town in a state of emergency, renewed while it is attacked | done | a storage pit or town centre knocked below its functional threshold (`abodes::ReduceLife`, `src/ECS/Abodes.cpp`), or either on fire (`town_emergency::ProcessTownEmergency`, `src/ECS/Town/TownEmergency.cpp`, as `Town::ProcessTownEmergency` 0x7477A0), starts it for 1200 turns; `test/test_repairs.cpp` |
| In an emergency villagers run at fleeing speed | done | (GameFloatRand(0.5) + 0.75) times the fleeing speed while the town is in its emergency (`src/ECS/VillagerSpeed.cpp`); `test/test_villager_speed.cpp` (WoundedLoadedNeededAndSpedUp) |
| Villagers come to beat out a blaze in their town | done | firefighting reactions (`src/ECS/Systems/Implementations/VillagerFire.cpp`); `test/test_fire.cpp`; the town's most important abode on fire (`Town::GetMostImportantAbodeOnFire` 0x4E4D00) is chosen only by the creature (its one caller fn_0047CFB0), see ../villager/reactions.md |
| After the emergency, villagers gather at the town's congregation point, then go back to their lives | done | the call to every housed villager, the congregation walk and the stops (states 242 and 243, `src/ECS/Villager/VillagerEmergency.cpp`, `town_emergency::CallAllVillagersToTownEmergency`); `test/test_villager_repair.cpp` (CallToTownEmergency, GotoCongregateInTownAfterEmergency, CongregateInTownAfterEmergency) |
| While the emergency lasts the town stops worshipping, and its worship share comes back when it ends | done | (added) the worship percentage is saved and set to 0 during the emergency, then restored (`town_emergency::ProcessTownEmergency`, `src/ECS/Town/TownEmergency.cpp`); `test/test_repairs.cpp` |
| Some villagers always react to their town's emergency, whatever they are doing | done | the original's per-state answer column (`k_TownEmergencyReaction`, `src/ECS/Villager/VillagerOriginalFns.h`) for every row; `test/test_villager_repair.cpp` (TownEmergencyReactionTable, ReactsToTownEmergencyByFinalState) |
| The town counts its deaths, by reason and by killer, and its last death's turn | done | every counter `TownStats::VillagerDead` keeps (`components::TownDeaths`, written by `src/ECS/Villager/VillagerDeath.cpp`); `test/test_villager_death.cpp` |
| A town whose last villager has left empties 50 turns later: its belief reset and the town given back to the neutral player | partial 70% | the countdown is set when the last villager leaves (`src/ECS/Town/TownVillagers.cpp`) and runs (`src/ECS/Town/TownProcess.cpp`), and `town_belief::SetTownEmpty` holds the belief part, but its end is a TODO: SetTownEmpty is never called (`Town::Process` 0x747559 calls it); no test |
