# Damage, burning, repair and ruins

Buildings take damage from thrown rocks, the creature, fire and miracles. A damaged building loses chunks, stops
working when badly hurt and empties of people; it can burn down, and towns repair what is damaged. A building
destroyed outright goes, and the town keeps a plan to rebuild it.

**Progress: 20/24 done, 3 partial — 90%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Damage

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A building has life from 1 down to 0, lowered by harm | done | the building's life (`ecs::life`), lowered through `abodes::ReduceLife` (`src/ECS/Abodes.cpp`) |
| A building being hurt sends its people out | done | `abodes::ReduceLife` taps every inhabitant out (`villager::SetStateWhenTappedOnAbode`) |
| A building left with no life stays standing, useless, when a miracle destroys it | done | an effect only destroys villagers and animals (`src/ECS/Effects/EffectValues.cpp`); a building at no life keeps standing with its building site (`abodes::ReduceLife`) |
| Harm to a building is an attack on its town | partial | a destructive effect records the town's aggressor (`town_emergency::UpdateAggressor`); the per-player aggression is not ported; See ../town/emergencies_and_aggression.md |
| A hit from a thrown object damages it by the impact | done | a rock's blow by momentum (`Buildings::ReactToPhysicsImpact`, `src/ECS/Physics/Buildings.cpp`) and the life lost (`abodes::OnPhysicalDamage`); see ../physics/impact_damage.md |
| Rocks knock real holes in the building: the mesh is cut, pieces fly off and fall, larger ones landing as rubble | done | `FragMesh::Impact` and the pieces (`src/ECS/Physics/FragMesh.cpp`, `src/ECS/Physics/Buildings.cpp`); see [../physics/impact_damage.md](../physics/impact_damage.md) |
| A badly damaged building stops working until repaired | done | below the info's threshold `abodes::StopBeingFunctional` (`src/ECS/Abodes.cpp`), working again once repaired |
| Damage to the town centre or storage pit raises an emergency | done | `abodes::ReduceLife` below the functional threshold starts the emergency (`src/ECS/Abodes.cpp`, `src/ECS/Town/TownEmergency.cpp`) |
| The creature kicking or stomping on a building damages it | partial | the creature's stomp and kick give the blow a thrown thing gives (`src/ECS/Systems/Implementations/CreatureObjectActionSystem.cpp`), not one by its size; See ../creature/ |
| Miracles that destroy things damage or flatten buildings | partial | a miracle's damage lowers a building's life (`effects::ApplyEffect` to `abodes::ReduceLife`), but no building is flattened by it; see ../miracles/ |
| Heal restores a building's life | done | the heal's `abodes::IncreaseLife` (`src/ECS/Effects/EffectValues.cpp`); see ../miracles/ |
| A totem, the temple and teleport stones can't be destroyed by miracles | done | no effect destroys anything but villagers and animals (`src/ECS/Effects/EffectValues.cpp`) |

## Burning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Buildings catch fire, burn by their material and spread fire to what is near | done | `src/ECS/Fire` (the materials from info.dat, the spread); `test/test_fire.cpp`; see ../physics/fire.md |
| A large burning building lights the land round it | done | the fire light map on multi-cell fixed objects over 2 m (`src/ECS/Fire/FireGraphic.cpp`) |
| Villagers come to beat the fire out | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`; See ../villager/reactions.md |
| A building burnt down flickers out as a ghost of itself and goes | todo | no ghost: a building burnt to nothing keeps standing with its building site (`abodes::ReduceLife`); see ../physics/fire.md |
| Villagers inside a burning building come out | done | `abodes::ReduceLife` taps every inhabitant out of a burning building |
| Rain cools burning buildings | done | the rain cooling multiplier in `fire::Step` (`src/Fire/FireModel.cpp`), read by `src/ECS/Fire/FireEffect.cpp`; see ../weather/ |

## Repair and ruins

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The town wants repairs for its damaged, lived-in abodes | done | `DesireToRepair` (`src/ECS/Town/TownDesire.cpp`); see ../town/town_desires.md |
| At most one repair site opens per town turn | done | `building_sites::ProcessTownRepairs` (`src/ECS/Town/BuildingSites.cpp`), from the town turn |
| Builders repair the building with wood, back to whole | done | a repairer is a builder: the wood back to full life (`src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Villager/VillagerHome.cpp`); `test/test_villager_repair.cpp` |
| A repaired building works again and its damage is removed | done | `abodes::Repaired` deletes the site, removes the damage model and makes the building functional (`src/ECS/Abodes.cpp`) |
| A destroyed building leaves a plan to rebuild it, so the town can rebuild | done | the rebuild plan (`plans::CreateFromBuilding` through `MoveAbodeToPlannedAbodes`, `src/ECS/Abodes.cpp`) |
| Its people become homeless and its stores lose their piles | done | the abode's villagers become homeless (`abode_villagers::RemoveAllVillagersFromAbode`) and a storage pit's piles go with it (`abodes::OnToBeDeleted`, `src/ECS/Abodes.cpp`) |
