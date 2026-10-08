# Building by the creature

A creature that has learnt to help a town can do its building work: carry scaffolds from the workshop to the town,
bring wood to building sites and the workshop, and build or repair with its own hands, copying what it has seen the
player do. It can also wreck buildings. This file covers the buildings' side; how the creature learns is in
../creature/.

**Progress: 0/13 done, 3 partial — 12%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Helping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature picks up a scaffold and carries it to a town | todo | no creature town work (see ../creature/town_actions.md) |
| It drops the scaffold where the town has room, making a building site | todo |  |
| It brings wood to a building site or the storage pit | todo |  |
| It brings wood to the workshop so it makes scaffolds | todo |  |
| It builds a building site up with its hands, much faster than villagers | todo |  |
| It repairs damaged buildings | todo |  |
| It copies the player's adding of food and wood to stores and workshops | todo | the copying rules exist but the store deeds are not reported (`StoragePitStore::DoCreatureMimicAfterAddingResource` only returns the deed); see ../creature/learning_by_observation.md |
| A town believes in the creature's player for its help | todo | See ../town/belief_and_conversion.md |
| It can steal scaffolds from other towns | todo |  |

## Wrecking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature kicks or stomps on buildings, damaging them | partial | the plan actions `Stomp` and `Kick` (`src/Creature/CreaturePlanActions.cpp`) give a home the blow a thrown thing gives (`ecs::abodes::OnPhysicalDamage` from `src/ECS/Systems/Implementations/CreatureObjectActionSystem.cpp`), not one by the creature's size; see ../creature/ |
| It throws objects at buildings | partial | it throws what it picks up (`ThrowAround`, `ThrowBallAtObject`, `src/Creature/CreaturePlanActions.cpp`), and a thrown thing damages a building through the physics; it does not aim at buildings; See ../creature/ and damage_and_repair.md |
| It can burn buildings with a fireball or by setting things alight | todo | no creature casting and no setting fire to a town (see ../creature/creature_casting.md) |
| A town hurt by the creature fears it and wants protection | partial | eating, holding or throwing villagers frightens their town (`creature_object_actions::AttitudeTo`); the town's protection desire stays 0 (`src/ECS/Town/TownDesire.cpp`) |
