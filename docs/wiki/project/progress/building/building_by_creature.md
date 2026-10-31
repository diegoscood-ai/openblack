# Building by the creature

A creature that has learnt to help a town can do its building work: carry scaffolds from the workshop to the town,
bring wood to building sites and the workshop, and build or repair with its own hands, copying what it has seen the
player do. It can also wreck buildings. This file covers the buildings' side; how the creature learns is in
../creature/.

**Progress: 2/13 done, 0 chosen, 8 partial, 1 inferred — 48%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## Helping

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature picks up a scaffold and carries it to a town | partial 60% | a scaffold out of its player's influence is stealable and taken to its own nearest town's storage pit (`creature_steal::IsStealable`, `creature_town_theft::StealAndPutInTown`; tests `CreatureSteal.OnlyAToyAScaffoldOrAOneShotMiracleCanBeStolen`, `test/test_creature_town_theft.cpp`), ported from raffclar PR #611, unverified against v1.42; bw1-decomp's action table has no action carrying a scaffold from the creature's own workshop (`CreatureAction.cpp`: StealScaffolding and StealAndPutInTown only) |
| It drops the scaffold where the town has room, making a building site | inferred | not checked: the original's scaffold makes a building on a gentle drop from the hand (Scaffold::EndPhysics 0x6E8780); whether a creature's put-down does is not read |
| It brings wood to a building site or the storage pit | partial 80% | ported from raffclar PRs #419 and #420: the plan actions GiveWoodFromTreeToBuildingSite and GiveWoodFromTreeToStoragePit (`creature_mind::GiveWoodToStore`, `src/Creature/CreatureKindAgendas.cpp`), the same steps as bw1-decomp's (tree reach 1.2, throw position at its height, face 0.1, throw in pile); tests `test/creature/test_creature_kind_actions.cpp`, scenarios `objects.give_wood`, `objects.give_wood_site`; that the thrown tree adds the site's wood is not confirmed |
| It brings wood to the workshop so it makes scaffolds | pending | the original's GiveWoodFromTreeToWorkshop (the storage pit's steps on a workshop) has no executor in `src/Creature/CreaturePlanActions.cpp` |
| It casts the wood miracle by a building site or a workshop | done | `CastMagicWoodByBuildingSite`, `CastMagicWoodByWorkshop` (`src/Creature/CreaturePlanActions.cpp`), as bw1-decomp's (the helpful spell, wood, on the site or workshop); tests `test/test_creature_casting.cpp`, `test/creature/test_creature_mind_casting.cpp`, scenario `miracles.creature_casts_wood_by_workshop`; see ../creature/creature_casting.md |
| It builds a building site up with its hands, much faster than villagers | pending | the original's HelpBuildHouse (casts wood on the building, then the build sub-action) has no executor; how fast it builds is not read |
| It repairs damaged buildings | done | ported from raffclar PR #420: the plan action HelpRepairHouse, on a town's building below its full life (`creature_mind::HelpRepairHouse`, `src/Creature/CreatureKindAgendas.cpp`), the same steps as bw1-decomp's (pick up a tree, go near, face, put down, repair, use the tree up); test `test/creature/test_creature_kind_actions.cpp`, scenario `objects.repair_house` |
| It copies the player's adding of food and wood to stores and workshops | partial 70% | the deeds are reported and the creature considers copying them (`object_resources::ReportResourceDeed` to `creature_mimic::Consider`, `src/ECS/ObjectResources.cpp`; tests `test/creature/test_creature_mimic.cpp`, `test/test_creature_deeds.cpp`), unverified against v1.42; the copy into a workshop has no action (see above); see ../creature/learning_by_observation.md |
| A town believes in the creature's player for its help | partial 30% | the villagers who see a creature are impressed by it (`villager_reactions::UpdateHowImpressed`, `src/Magic/Impressiveness.cpp`); test `Impressiveness.ACreaturesValueIsItsHeldSizeByItsWearinessDistanceDeedAndBalance`; belief for the help itself is not ported; see ../town/belief_and_conversion.md |
| It can steal scaffolds from other towns | partial 40% | a scaffold out of its player's influence can be taken to its temple or its own town (`creature_steal::StealAndPutByTemple`, `creature_town_theft::StealAndPutInTown`); tests `CreatureSteal.OnlyAToyAScaffoldOrAOneShotMiracleCanBeStolen`, `CreatureSteal.WithAnEmptyHandItPicksUpTheThingWalksHomeAndPutsItDown`, `CreatureTownTheft.WithAnEmptyHandItPicksUpTheThingAndPutsItDownByThePit`; the original's own StealScaffolding (a scaffold of another player, Scaffold::CanBeStolenByCreature 0x4E3FF0, carried to the creature's home) has no executor; openblack tests its own influence instead of the original's test that the scaffold's town is not the creature's player's; unverified against v1.42 |

## Wrecking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature kicks or stomps on buildings, damaging them | partial 60% | the plan actions `Stomp` and `Kick` (`src/Creature/CreaturePlanActions.cpp`) damage homes: the kick breaks pieces off a built home that is not burning with two blows of the foot, as the original (`KickHome`, `BuildingDamageSystem::StrikeWithoutBody`; test `CreatureObjectActionSystemTest.AKickedHomeTakesTwoBlowsOfTheFootWhenItIsBuiltAndNotBurning`); the stomp still gives the blow a thrown thing gives (`ecs::abodes::OnPhysicalDamage` from `src/ECS/Systems/Implementations/CreatureObjectActionSystem.cpp`), not one by the creature's size; see ../creature/ |
| It throws objects at buildings | partial 40% | it throws what it picks up (`ThrowAround`, `ThrowBallAtObject`, `src/Creature/CreaturePlanActions.cpp`; `test/creature/test_creature_kind_actions.cpp`), and a thrown thing damages a building through the physics; it does not aim at buildings; See ../creature/ and damage_and_repair.md |
| It can burn buildings with a fireball or by setting things alight | partial 50% | it casts fireballs (`CastFireball` executors, `creature_mind::CastFireballAt`, `src/Creature/CreatureCastAgenda.cpp`; `test/test_creature_casting.cpp`); the original's StartFire and SetFireToObject actions (setting a thing alight) have no executor; see ../creature/creature_casting.md |
