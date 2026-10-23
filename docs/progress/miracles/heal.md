# Heal

A miracle cast on the land that restores the life of the people, animals and creatures near the cast point and cures
poisoning. The power-up first raises a glowing mushroom and heals a much wider area a few seconds later.

Given by silver scrolls: a heal chest by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) and [Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md),
heal dispensers by [The Ogre](../story/silver_scrolls/the_ogre.md) and [The Pied Piper](../story/silver_scrolls/the_pied_piper.md), and a temple that heals all around it by
[The Beach Temple Puzzle](../story/silver_scrolls/the_beach_temple_puzzle.md).

**Progress: 15/36 done, 0 chosen, 18 partial, 0 inferred — 79%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/food-wood-water-heal.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't be cast where there is nobody to heal | done | `src/Magic/CastRules.cpp` (CanCastAt: the type's own FindHealTargets must find someone); a script cast checks nothing, as in the original. Scenario `miracles.heal_crowd` |
| Cost 6000 / 9000 to cast and 10 for each person healed | done | From the info tables (`src/Magic/MagicTables.cpp`); each chakra's heal event pays its cost per event (`src/Magic/Core/SpellEvent.cpp`, `src/Magic/Core/Chants.cpp`) Test `MagicTables.realInfoDat` (with the game's data) |
| Who is healed is chosen once, at the cast; anyone walking in later is not | partial 80% | `src/Magic/Spells/SpellHeal.cpp` (InitWithPos gives the effect its targets once) No test or check covers a late arrival |
| The miracle lasts as long as its effect | done | `src/Magic/Core/Spell.cpp` (the plain spell turn); the chakra rule keeps the effect alive until the spell closes (`src/Particles/Rules/Heal.cpp`; test `Heal.chakraWaitsForTargetsUntilClosed`) |
| The caster's tribal power widens the reach and raises the most it can heal | partial 90% | `src/Magic/CastRules.cpp` (FindHealTargets: radius and most both times the tribal power when there is a spell) `PlayerMagic::tribalPower` is never set in our tree, so it is always 1 (see [prayer_cost.md](prayer_cost.md)); no test |
| Plain heal: 10 m, at most 20; power-up: 35 m, at most 100, healing 3.5 s after the cast | done | Radius and most from the HEAL and HEAL_PU_ONE rows (`src/Magic/CastRules.cpp`); the power-up's delay and mushroom are the particle data (`src/Particles/Rules/KeyPoints.cpp`) Test `Heal.realData` (with the game's data); scenarios `miracles.heal_crowd`, `miracles.heal_powerup_crowd` |

## Who is healed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Map cells are walked outward from the cast cell; the plain heal looks in only a 2 by 2 block of cells | partial 80% | `src/Magic/CastRules.cpp` (FindHealTargets: ceil(2R/10) squared cells along the map spiral, so 4 for the plain heal) No test covers the walk; scenario `miracles.heal_crowd` |
| Each cell's people are measured from the cast point moved into that cell | partial 80% | `src/Magic/CastRules.cpp` (both distance tests measure from the spiral's moving map point) No test covers it |
| Distance is flat (height ignored) and strictly inside the reach | partial 80% | `src/Magic/CastRules.cpp` (flat distance, strict test) No test covers it |
| Within a cell the most recent arrivals are taken first, up to the limit | partial 80% | `src/Magic/CastRules.cpp` (each cell's mobile list from its head, `ecs::map_cells::MobileInCell`, stops at the most) No test covers it |
| Only living villagers and animals, and any creature; doves never; the dead are not revived | partial 80% | Villagers, animals and creatures, no bird of the dove class (`CanBeHealedByHealSpell`, `src/Magic/CastRules.cpp`); scenarios `miracles.heal_crowd`, `miracles.heal_creatures`. Not checked: the dead (openblack has no dead test, the dying states are not ported), and the citadel's dove and bat are unidentified ([wiki Pending](../../../magic/food-wood-water-heal.md#pending)) |
| Someone held in the hand is not healed | partial 60% | A held villager's chakra ends at once (`src/Particles/Rules/Heal.cpp`, using the hand's held object instead of the original's unavailable flag); whether one held at the cast is skipped as a target is not checked |
| Two heals never work on the same person at once | partial 80% | `src/Particles/Rules/Heal.cpp` (a target that already has a chakra is skipped) No test or check covers it |

## What the heal does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each person is healed the moment their glow appears, up to full life at full strength | done | `src/Particles/Rules/Heal.cpp` sends one heal event per chakra; `src/ECS/Effects/EffectValues.cpp` (ApplyEffect: life up to 1) Scenario `miracles.heal_crowd` |
| It cures poisoning (for example from poisoned food) | done | `src/Magic/Core/SpellEvent.cpp` removes the poison for HEAL and HEAL_PU_ONE (`src/ECS/Life.cpp`) Scenario `miracles.heal_crowd` (the poisoned) |
| Buildings, trees and fields are not healed | partial 80% | Only villagers and animals are targets (`src/Magic/CastRules.cpp`) No test or check covers it |
| A creature in a fight gets fight health and stamina back, and its wounds mend | pending | A creature out of a fight is healed (row below); in a fight its life does not change, and the fight's part (the fight health, the knock-out) is not ported (`CreatureApplyEffect`, `src/ECS/Effects/EffectValues.cpp`); issue #173 |
| A creature out of a fight is healed, the marks on its skin mending by the heal's strength | done | `CreatureApplyEffect` (`src/ECS/Effects/EffectValues.cpp`) heals its life and mends its marks through the skin system; tests `CreatureSpellHooksTest.AHealHealsACreaturesLifeAndMendsItsMarks`, `CreatureSpellHooksTest.TheMarksMendByWholeCountsAndNotWithoutAHeal`; scenarios `miracles.heal_creature`, `miracles.heal_creatures` |
| A creature's cuts and scars fade when healed | partial 50% | The skin's marks mend (row above); the cuts and scars of a fight are not ported (`CreatureApplyEffect`, `src/ECS/Effects/EffectValues.cpp`); issue #173 |
| A creature healed by another creature thinks better of it | pending | A creature's opinion of a creature that applies an effect to it is not ported (`CreatureApplyEffect`, `src/ECS/Effects/EffectValues.cpp`); issue #173 |
| Healing counts as a good deed for the caster's alignment | partial 80% | `src/ECS/Effects/EffectValues.cpp` (ApplyEffect moves the caster player's alignment), `src/ECS/Effects/Alignment.cpp`. No test covers the heal's own alignment step |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle (one reaction per miracle) | done | The look-at-nice-miracle reaction is started (`src/Magic/Core/SpellEvent.cpp`) and handled by the villagers (`k_LookAtNiceSpell`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`); tests in `test/test_villager_reactions.cpp`; scenario `miracles.reactions_nice` |
| A creature watching a player's or creature's heal is impressed and can learn it | done | `ReactToNiceMagic` (`src/ECS/Systems/Implementations/CreatureReactions.cpp`) over `creature_reaction_rules::NiceMagic`; tests in `test/creature/test_creature_reactions.cpp`; see [miracle_core.md](miracle_core.md) |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chakra follows each healed person's middle, sized to them | done | `src/Particles/Rules/Heal.cpp` (the chakra follows its target's middle, scaled by its 2D radius) Scenario `miracles.heal_crowd` |
| The healed person glows pale cyan-white, rising over 1.5 s and gone by 3 s | done | `src/Particles/Rules/Heal.cpp` (ChakraFade into `components::SpecularColour`), drawn by `src/ECS/Systems/Implementations/RenderingSystem.cpp`; test `Heal.chakraFade` |
| Five spinning sparkles rise and spread from each person | done | `src/Particles/Rules/Heal.cpp` (the fused spherical burst); test `Heal.fusedSphericalExplode` |
| The chakra ends when its person goes or is picked up | partial 70% | `src/Particles/Rules/Heal.cpp` (the target gone or held ends the chakra and clears the glow) Openblack has no hand's flag, so it uses the object the hand holds ([wiki](../../../magic/food-wood-water-heal.md#openblack)); no test |
| Power-up: a tall glowing mushroom grows, turns and collapses over 6 s | done | `src/Particles/Rules/KeyPoints.cpp`, `src/Particles/Creators/Mesh.cpp` (the animated mesh creator) Scenario `miracles.heal_powerup_crowd` |
| Power-up: seven stars shoot up about 37 m and fade | done | Particle data of the power-up effect, run by `src/Particles/PSys.cpp`. Scenario `miracles.heal_powerup_crowd` |
| Two sparkles swing to and fro around the miracle held in the hand | done | `src/Particles/Rules/Heal.cpp` (the in-hand wiggle); test `Heal.healInHandWiggle` |
| Effect on a holder of the miracle | partial 60% | Data-driven holder effect (`src/Worship/SpellSeedGraphic.cpp` makes the seed's holder particle); no test or check, not compared with the original |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One heal sound per miracle, when the first chakra appears | partial 80% | `src/Particles/Rules/Heal.cpp` (the first atom's heal sound, once) No test covers it |
| Power-up: the mushroom's sound at the cast, stopped at once if its effect goes | partial 80% | The mesh's create sound (`src/Particles/PSys.cpp`), released with the effect (`src/Particles/Rules/Sound.cpp`) No test covers it |
| Power-up: the heal sound follows about 3.5 s later | partial 80% | Particle data of the power-up effect (the chakra's heal sound comes with the delayed chakras); no test covers it |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast heal at a hurt villager | partial 70% | `CastHealSpell` and `CastHealSpellPU1` at `Target::HurtVillager` (`CreaturePlanActions.cpp`); see [../creature/creature_casting.md](../creature/creature_casting.md) Scenarios `miracles.creature_casts_heal`, `miracles.creature_heals_town_villager`; not yet checked against the original (issue #172) |
| A heal in progress is kept in a saved game | pending | No game save system |
