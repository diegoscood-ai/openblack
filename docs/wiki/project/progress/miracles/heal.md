# Heal

A miracle cast on the land that restores the life of the people, animals and creatures near the cast point and cures
poisoning. The power-up first raises a glowing mushroom and heals a much wider area a few seconds later.

Given by silver scrolls: a heal chest by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) and [Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md),
heal dispensers by [The Ogre](../story/silver_scrolls/the_ogre.md) and [The Pied Piper](../story/silver_scrolls/the_pied_piper.md), and a temple that heals all around it by
[The Beach Temple Puzzle](../story/silver_scrolls/the_beach_temple_puzzle.md).

**Progress: 32/36 done, 0 chosen, 4 partial, 0 inferred — 95%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/food-wood-water-heal.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't be cast where there is nobody to heal | done | `src/Magic/CastRules.cpp` (CanCastAt: the type's own FindHealTargets must find someone); a script cast checks nothing, as in the original. Scenario `miracles.heal_crowd` |
| Cost 6000 / 9000 to cast and 10 for each person healed | done | From the info tables (`src/Magic/MagicTables.cpp`); each chakra's heal event pays its cost per event (`src/Magic/Core/SpellEvent.cpp`, `src/Magic/Core/Chants.cpp`) Test `MagicTables.realInfoDat` (with the game's data) |
| Who is healed is chosen once, at the cast; anyone walking in later is not | done | `src/Magic/Spells/SpellHeal.cpp` (InitWithPos gives the effect its targets once); the chakra rule heals only the targets it was given (`src/Particles/Rules/Heal.cpp`); test `HealMiracleChakras.SomeoneWhoWalksInAfterTheCastIsNotHealed` |
| The miracle lasts as long as its effect | done | `src/Magic/Core/Spell.cpp` (the plain spell turn); the chakra rule keeps the effect alive until the spell closes (`src/Particles/Rules/Heal.cpp`; test `Heal.chakraWaitsForTargetsUntilClosed`) |
| The caster's tribal power widens the reach and raises the most it can heal | done | `src/Magic/CastRules.cpp` (FindHealTargets: radius and most both times the tribal power when there is a spell) `PlayerMagic::tribalPower` is never set in our tree, so it is always 1, as every tribe's is in the original (see [prayer_cost.md](prayer_cost.md)); test `test/test_magic_spells.cpp` `SpellBehaviours.TheHealGivesItsEffectThePeopleRoundIt` |
| Plain heal: 10 m, at most 20; power-up: 35 m, at most 100, healing 3.5 s after the cast | done | Radius and most from the HEAL and HEAL_PU_ONE rows (`src/Magic/CastRules.cpp`); the power-up's delay and mushroom are the particle data (`src/Particles/Rules/KeyPoints.cpp`) Test `Heal.realData` (with the game's data); scenarios `miracles.heal_crowd`, `miracles.heal_powerup_crowd` |

## Who is healed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Map cells are walked outward from the cast cell; the plain heal looks in only a 2 by 2 block of cells | done | `src/Magic/CastRules.cpp` (FindHealTargets: ceil(2R/10) squared cells along the map spiral, so 4 for the plain heal); scenario `miracles.heal_crowd`; test `test/test_heal_miracle.cpp` `HealMiracle.ThePlainHealLooksOnlyInATwoByTwoBlockOfCellsWalkedOutwardFromTheCastCell` |
| Each cell's people are measured from the cast point moved into that cell | done | `src/Magic/CastRules.cpp` (both distance tests measure from the spiral's moving map point); test `test/test_heal_miracle.cpp` `HealMiracle.EachCellsPeopleAreMeasuredFromTheCastPointMovedIntoThatCell` |
| Distance is flat (height ignored) and strictly inside the reach | done | `src/Magic/CastRules.cpp` (flat distance, strict test); test `test/test_heal_miracle.cpp` `HealMiracle.TheDistanceIsFlatAndStrictlyInsideTheReach` |
| Within a cell the most recent arrivals are taken first, up to the limit | done | `src/Magic/CastRules.cpp` (each cell's mobile list from its head, `ecs::map_cells::MobileInCell`, stops at the most); test `test/test_heal_miracle.cpp` `HealMiracle.WithinACellTheLatestArrivalsAreTakenFirstUpToTheMost` |
| Only living villagers and animals, and any creature; doves never; the dead are not revived | partial 80% | Villagers, animals and creatures, no bird of the dove class (`CanBeHealedByHealSpell`, `src/Magic/CastRules.cpp`); scenarios `miracles.heal_crowd`, `miracles.heal_creatures`. The dead, dying or not functional villager or animal is not healed (`villager::IsDead`, `animal_ai::IsDead`, as the original's `!IsDead`; test `HealMiracle.TheDeadAreNotHealed`). Not checked: the citadel's dove and bat are unidentified ([wiki Pending](../../../magic/food-wood-water-heal.md#pending)) |
| Someone held in the hand is not healed | partial 60% | A held villager's chakra ends at once (`src/Particles/Rules/Heal.cpp`, using the hand's held object instead of the original's unavailable flag); whether one held at the cast is skipped as a target is not checked |
| Two heals never work on the same person at once | done | `src/Particles/Rules/Heal.cpp` (a target that already has a chakra is skipped); test `test/test_heal_miracle.cpp` `HealMiracleChakras.TwoHealsNeverWorkOnTheSamePersonAtOnce` |

## What the heal does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each person is healed the moment their glow appears, up to full life at full strength | done | `src/Particles/Rules/Heal.cpp` sends one heal event per chakra; `src/ECS/Effects/EffectValues.cpp` (ApplyEffect: life up to 1) Scenario `miracles.heal_crowd` |
| It cures poisoning (for example from poisoned food) | done | `src/Magic/Core/SpellEvent.cpp` removes the poison for HEAL and HEAL_PU_ONE (`src/ECS/Life.cpp`) Scenario `miracles.heal_crowd` (the poisoned) |
| Buildings, trees and fields are not healed | done | Only villagers, animals and creatures are targets (`CanBeHealedByHealSpell`, `src/Magic/CastRules.cpp`, [page](../../../magic/food-wood-water-heal.md)); test `test/test_magic_world.cpp` `GameMagicWorldObjects.TheHealHealsNoBuildingTreeOrField` |
| A creature in a fight gets fight health and stamina back, and its wounds mend | done | with an opponent the heal mends its marks and goes on its fight health, kept to 0..1, as the original's (the original's heal goes to the fight health alone): `CreatureApplyEffect` (`src/ECS/Effects/EffectValues.cpp`) through `CreatureFightSystem::TakeMiracle` (`creature_fight::TakeMiracle`); tests `CreatureFight.AMiraclesHealThenItsHarmGoToTheFightHealth`, `CreatureSpellHooksTest.InAFightAMiracleReelsTheFighterAndGoesToItsFightHealth`; [fights.md](../../../creature/fights.md#miracles-blocking-and-fainting); [spell-core.md](../../../magic/spell-core.md#a-creature-taking-an-effect) |
| A creature out of a fight is healed, the marks on its skin mending by the heal's strength | done | `CreatureApplyEffect` (`src/ECS/Effects/EffectValues.cpp`) heals its life and mends its marks through the skin system; tests `CreatureSpellHooksTest.AHealHealsACreaturesLifeAndMendsItsMarks`, `CreatureSpellHooksTest.TheMarksMendByWholeCountsAndNotWithoutAHeal`; scenarios `miracles.heal_creature`, `miracles.heal_creatures` |
| A creature's cuts and scars fade when healed | done | the marks on its skin, a fight's wounds (`CreatureFightSystem`) and an effect's cuts (`effects::CutAndScar` (`src/ECS/Effects/EffectValues.cpp`): the line, kind, column and draws of `creature_effect_cut::Plan` (`src/Creature/CreatureEffectCut.cpp`), the skin's place by `creature_skin::CutPlace` (`src/ECS/CreatureSkin.cpp`), the wound through `CreatureSkinSystem::AddWound`), mend by the heal's strength (`MendMarks` in `CreatureApplyEffect`, `CreatureSkinSystem::Heal`); tests `CreatureSpellHooksTest.AHealHealsACreaturesLifeAndMendsItsMarks`, `CreatureMarks.WoundsFadeFromFreshToOld`; [spell-core.md](../../../magic/spell-core.md#a-creature-taking-an-effect) |
| A creature healed by another creature thinks better of it | done | `ChangeOpinionOfApplier` in `CreatureApplyEffect` (`src/ECS/Effects/EffectValues.cpp`): the burn, crush, hit and heal above 0 times the alignment info's creature column, through `ChangeHowNice`, at most once in more than 600 turns (the leash's stamp, `CreatureLeashAttitude`); test `CreatureSpellHooksTest.ACreatureApplyingAnEffectIsThoughtOfByItsWeightsAtMostEvery600Turns`; [spell-core.md](../../../magic/spell-core.md#a-creature-taking-an-effect) |
| Healing counts as a good deed for the caster's alignment | done | `src/ECS/Effects/EffectValues.cpp` (ApplyEffect moves the caster player's alignment), `src/ECS/Effects/Alignment.cpp`; test `test/test_heal_miracle.cpp` `HealMiracleAlignment.HealingAVillagerIsAGoodDeedForTheCaster` |

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
| The chakra ends when its person goes or is picked up | done | `src/Particles/Rules/Heal.cpp` (the target gone or held ends the chakra and clears the glow) Openblack has no hand's flag, so it uses the object the hand holds ([wiki](../../../magic/food-wood-water-heal.md#openblack)); test `HealMiracleChakras.AChakraEndsWhenItsPersonGoesOrIsPickedUp` |
| Power-up: a tall glowing mushroom grows, turns and collapses over 6 s | done | `src/Particles/Rules/KeyPoints.cpp`, `src/Particles/Creators/Mesh.cpp` (the animated mesh creator) Scenario `miracles.heal_powerup_crowd` |
| Power-up: seven stars shoot up about 37 m and fade | done | Particle data of the power-up effect, run by `src/Particles/ParticleEffect.cpp`. Scenario `miracles.heal_powerup_crowd` |
| Two sparkles swing to and fro around the miracle held in the hand | done | `src/Particles/Rules/Heal.cpp` (the in-hand wiggle); test `Heal.healInHandWiggle` |
| Effect on a holder of the miracle | partial 60% | Data-driven holder effect (`src/Worship/SpellSeedGraphic.cpp` makes the seed's holder particle). The start is pinned against the original's (its row's file, at the point raised by holder height times the size, at that size, on the local stream, the graphic its target): test `SeedHolderEffects.realInfoDatEachSeedShowsItsOwnHolderEffectAtItsHolderHeight` (`test/test_miracle_visuals.cpp`, with the game's info.dat); still open: its mists and meshes in the gesture icons ([gestures](../../../magic/gestures.md#seeds-the-help-and-the-addresses)) |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One heal sound per miracle, when the first chakra appears | done | `src/Particles/Rules/Heal.cpp` (the newest chakra's heal sound, once, a step after the chakras appear); test `HealMiracleChakras.OneHealSoundPerMiracleWhenItsChakrasAppear` |
| Power-up: the mushroom's sound at the cast, stopped at once if its effect goes | done | The mesh's create sound (`src/Particles/ParticleEffect.cpp`), released with the effect (`src/Particles/Rules/Sound.cpp`): a sound without SOFTRELEASE is stopped at once when its atom goes (test `PhysicalShieldMiracleEffect.AGoneAtomsSoundRingsOutWithSoftReleaseAndStopsAtOnceWithout`), and the powered-up heal's file gives the mushroom's create sound none (test `HealMushroomSound.realDataTheMushroomSoundsAtItsMakingAndStopsAtOnce`, with the game's data) |
| Power-up: the heal sound follows about 3.5 s later | done | Particle data of the power-up effect: its chakras, and with them their one heal sound, wait until their collection is 3.5 s old (the rule's condition, `src/Particles/ParticleEffect.cpp`; `src/Particles/Rules/Heal.cpp`); test `HealMiracleChakras.realDataThePoweredUpHealsSoundComesThreeAndAHalfSecondsAfterTheCast` (`test/test_heal_miracle.cpp`, with the game's data) |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast heal at a hurt villager | done | `CastHealSpell` and `CastHealSpellPU1` at `Target::HurtVillager` (`CreaturePlanActions.cpp`), a villager hurt to seven tenths of its life or less as `Villager::CanBeHealedByCreature` 0x4E4C90, cast through the helpful construct 0x4A6910 ([casting.md](../../../creature/casting.md#the-attack-shield-and-impress-constructs)); test `CreaturePlanActionsCasting.TheGamesCastsInTheirStyles`; scenarios `miracles.creature_casts_heal`, `miracles.creature_heals_town_villager`; see [../creature/creature_casting.md](../creature/creature_casting.md) |
| A heal in progress is kept in a saved game | partial 30% | written: the running heal's record from its entity, of the healing spells' class (`src/ECS/SavedWorld/WriteSpells.cpp`; test `SpellWrite.ThePlainTheStormAndTheHealingSpellsWriteEverySpellsValuesOnly`), with the game's list of spells, which a load makes again with its values and its effect (`saved_spells::MakeSpells`; test `SpellLoad.AHealingSpellIsMadeAgainAndWrittenBackAsRead`); missing: a spell whose effect has made its reaction still stops the write, and the in-play round trip of 2026-11-14 (refs/story/evidence/save_rt_1114/README.md) held no healing spell: still to be seen ([saved games](../../../engine/saved-games.md#writing-the-world-file)) |
