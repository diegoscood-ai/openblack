# Villager death

Villagers die of old age, starvation, fire, drowning, falls, predators, miracles and the creature. They fall, lie dead
for a while (as a skeleton when burnt), their soul rises, those nearby mourn them and the town buries them in its
graveyard if it has one.

**Progress: 27/28 done, 0 chosen, 1 partial, 0 inferred — 99%**

How the original does it, in our wiki: [Villagers: data, state machine and speed](../../../beings/villager-hand-and-death.md).

## Life and harm

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers have life, lowered by harm, and die when it runs out | done | the villager's life (`Villager::life`, `src/ECS/Life.cpp`) and `villager::VillagerDead` (`src/ECS/Villager/VillagerDeath.cpp`); `test/test_villager_death.cpp` |
| Harm to a villager is an attack on its town by whoever did it | done | a destructive effect records the town's aggressor and the aggression per player (`town_emergency::UpdateAggressor`, `town_aggression::Attacked`), and the town's player keeps the harm against the caster; `VillagerDeathTest.TheHarmAPlayersEffectDoesIsKeptAgainstThatPlayer`; see ../town/emergencies_and_aggression.md |
| Wounded villagers walk slower, badly wounded ones crawl | done | the wounded speeds and the crawl (`src/ECS/VillagerSpeed.cpp`); tests `VillagerSpeed.WoundedLoadedNeededAndSpedUp`, `VillagerAnimation.TheHurtCrawlOrLimp` |
| Hurt villagers sleep longer to recover | done | the sleep at home by the life (`src/ECS/Villager/VillagerHome.cpp`); tests `VillagerRoutine.SleepGoesOnWhileTheTownWantsItOrLifeIsLow`, `VillagerHomeTest.Sleeping` |
| Life slowly comes back | done | resting at home gives life back by the info's rate (`src/ECS/Villager/VillagerHome.cpp`); tests `VillagerRoutine.SleepRestoresLifeUpToFull`, `VillagerHomeTest.Sleeping` |
| Chanting at the worship site costs worshippers life, killing some | done | the chant's damage and its deaths (`src/ECS/Systems/Implementations/VillagerWorship.cpp`, reason Chant); See ../worship/; tests `WorshipperTest.TheChantCostsAWorshipperByItsOwnInfo`, `WorshipperRules.ChantingCostsLife`, `VillagerCoreTest.DeathReasons` |
| The heal miracle gives life back | done | the heal through `life::IncreaseLife` (`src/ECS/Effects/EffectValues.cpp`); See ../miracles/ |

## Ways to die

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Burnt to death by fire | done | `src/ECS/Systems/Implementations/VillagerFire.cpp` (`ON_FIRE`) and the fire's damage (`src/ECS/Fire`); tests `VillagerDeathTest.ABurntVillagerDiesByTheFiresPlayer`, `VillagerFire.ABurningVillagerRunsFourToTenMetresAtRandom` |
| Killed by a miracle or effect that destroys things | done | `villager::DestroyedByEffect` from `effects::ApplyEffect` (`src/ECS/Effects/EffectValues.cpp`); test `VillagerDeathTest.AnEffectKillsBeforeItMakesTheCrushedReaction` |
| Brought down and eaten by a predator | done | the predators' kill (`src/ECS/AnimalPredators.cpp`, reason Animal) and `BEING_EATEN`; see ../animal/wild_animals.md |
| Eaten, crushed or thrown by the creature | partial 80% | eaten: VillagerDead with no reason, put down to the creature's player, then killed (`CreatureObjectActionSystem.cpp`, `creature_eating::EatenPersonDeathOf`); crushed and thrown through the physics and the effects; the creature striker's kill count and its own alignment for the blow are not kept ([towns](../../../beings/towns.md)); See ../creature/ |
| Drowned when dropped or thrown into the sea | done | `DROWNING` and the drown death credited to the dropper (`src/ECS/VillagerDrowning.cpp`, reason PlayerInteractionDrown); tests `VillagerDeathTest.AnIndestructibleVillagerNeverFinishesDrowning`, `VillagerDeathTest.EndPhysicsInWater` |
| Killed by a fall from a throw | done | a hard landing kills with the thrower's player and reason PlayerInteraction (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); test `VillagerDeathTest.AThrownCorpseLandsDeadAgainWithItsCounterKept` |
| Dies of old age | done | old age (`src/ECS/Villager/VillagerAge.cpp`, reason OldAge); `test/test_villager_age.cpp` |
| Dies of hunger | done | starving (`src/ECS/Villager/VillagerFood.cpp`, reason Starving); tests `VillagerFoodTest.HungerInterrupts`, `VillagerFoodTest.APoisonedVillagerDiesOnlyOfHungerOrExhaustion` |
| Sacrificed at the altar | done | a villager the hand gives to a worship site's altar dies as a sacrifice to the player, worth 1.25 times more as a child, with the altar's light, the sacrifice sample, the man's or woman's cry and the red number (`villager::VillagerDead` with reason Sacrifice, `src/ECS/AltarOffering.cpp`, `src/ECS/AltarSacrifice.cpp`), in the exe's order ([worship](../../../magic/worship.md#a-sacrifice-at-the-altar)); `test/test_altar_sacrifice.cpp`; a sacrificed corpse goes at once (`DeadTick`); See ../worship/ |

## Dying and after

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The villager falls dying, then lies dead for a fixed time and goes | done | `SET_DYING`, `DYING` and `DEAD` with their clips and the corpse's time (`src/ECS/Villager/VillagerDeath.cpp`); tests `VillagerDeathTest.Dying`, `VillagerDeathTest.Dead`, `VillagerDeathPure.DyingTimeAndClips`, `VillagerDeathPure.DeadTick` |
| A dead villager lies as a skeleton (every corpse a script does not hold, burnt or not) | done | from its first DEAD turn the corpse takes the skeleton mesh, as the original's DEAD state does (`src/ECS/Villager/VillagerDeath.cpp`; `VillagerDeathTest.Dead`); a dead villager and one given the skeleton look take the skeleton as their high mesh too, so the top detail level draws it (`SetObjectMesh`; test `VillagerDeathTest.TheSkeletonIsAlsoItsHighMesh`) |
| The body stays shorter when the town has a graveyard | done | 120 turns with a functional graveyard, 600 without (`DyingTime`, `src/ECS/Villager/VillagerDeath.cpp`); tests `VillagerDeathPure.DyingTimeAndClips`, `VillagerDeathTest.OrderAndFields` |
| A dying villager drops its wood as a log (unless the creature eats it); its food is lost | done | `CreateDroppedResource`, `DropWood` and `DropFood` in `VillagerDead` (`src/ECS/Villager/VillagerDeath.cpp`); test `VillagerLifeWorld.ADyingVillagerDropsItsWoodAsALogAndLosesItsFood` (`test/test_villager_life_world.cpp`, on synthetic and the game's data); See [tools_and_carried_items.md](tools_and_carried_items.md) |
| The villager's soul rises to heaven or hell, fading out | done | the soul with its heaven or hell clip, fading in its last 500 ms (`src/ECS/Villager/VillagerSoul.cpp`); test `VillagerDeathPure.Soul` |
| A death message or help sprite shows for a death the player caused | done | the help sprites by reason and the town's size (`events::VillagerDeathHelp`, `src/ECS/Villager/VillagerDeath.cpp`); tests `VillagerDeathPure.HelpTable`, `VillagerDeathTest.ReportsToGuidance` |
| The owner's alignment moves for a death they caused | done | `alignment::UpdateForDeath` from `VillagerDead` by the death's reason (`src/ECS/Effects/Alignment.cpp`); See ../worship/; tests `VillagerDeathPure.AlignmentAndMourningTurns`, `VillagerDeathTest.OwnerKillerAndCounters` |
| The town counts its deaths and its people | done | the town's death counters (`components::TownDeaths`) and the people counted each town turn (`town_stats::Compute`); test `VillagerDeathTest.OwnerKillerAndCounters` |
| Its abode and town forget the dead villager | done | `DeleteDependants`: out of the abode, the town or the vagrants (`src/ECS/Villager/VillagerDeath.cpp`); tests `VillagerDeathTest.DeleteDependants`, `VillagerDeathTest.DeleteDependantsOfAWoman` |
| Nearby villagers point at, go to, look at and mourn the dead | done | `REACT_TO_DEATH` and the states 205 to 208 (`src/ECS/Villager/VillagerMourning.cpp`); `test/test_villager_death.cpp` |
| A dead mother's children are orphaned and mourn | done | `FindChildrenAndOrphanThem` and the orphans' mourning (`src/ECS/Villager/VillagerMourning.cpp`); tests `VillagerDeathTest.Orphans`, `VillagerDeathTest.MourningStates` |
| The dead are buried in the graveyard, which grows a grave for each | done | each death adds to the graveyard's count (`graveyard::AddDead`) and its graves are drawn by its stage; See ../building/civic_buildings.md; tests `BuildingSitesTest.AWorkingGraveyardOfATownCountsItsDeadUpToFifty`, `BuildingSitesTest.AGraveyardsGravesStageIsItsDeadTimesPointEighteen` |
