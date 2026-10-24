# Spiritual shield

A sphere the player draws with a circle gesture. It stops other miracles (fireballs, lightning, blasts, flocks) at its
surface, keeps other gods' influence out, and the caster's villagers shelter under it. It drains the caster's prayer
power for as long as it stands and breaks when an attack empties it.

**Progress: 15/37 done, 0 chosen, 21 partial, 0 inferred — 84%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/shields.md).

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it | done | `src/Magic/Gestures/PowerUpSystem.cpp` (the circle sets point and size), `src/ECS/Systems/Implementations/HandSpellSeed.cpp`; see [../gesture/](../gesture/) Scenarios `miracles.shield_sizes`, `miracles.hand_circle_casts`, `miracles.gesture_circle_shield` |
| Size kept between 5 and 1000 m | done | `src/Magic/Spells/SpellShield.cpp` (`ClampShieldRadius`: from above, then from below); test `Shield.radiusClamp` |
| Cast only on land within the caster's influence | done | `src/Magic/CastRules.cpp` (the row's cast rule) Scenario `miracles.hand_press_outside_influence` |
| Upkeep each turn grows with the square of the size and is eased by tribal power | done | `SpellShield.cpp` (`ShieldCostToMaintain`), `src/Magic/Core/Chants.cpp` (divided by tribal power); tests `Shield.upkeepGrowsWithTheSquareOfTheRadius`, `SpellChants.shieldDividesByTribalPower` |
| A player's shield stands while their prayer power pays for it, then fails | done | `src/Magic/Core/Chants.cpp` (the creator pays: a worship site's icon, else the seed's own chants); see [prayer_cost.md](prayer_cost.md) Scenario `miracles.shield_runs_out` |
| On close-down the shield goes at once with its rings and reactions | done | `SpellShield.cpp` (`ShieldCloseDown`, `ToBeDeleted`: the objects set dying, the anti rings and reactions removed); the invisible shield object goes at once (`src/Magic/Objects/MapShield.cpp` `SetDying`) Tests `MagicShieldSystemTest.AShieldThatHasGoneKeepsNoReactionOff`, `MagicShieldSystemTest.ATurnPassesOverAShieldThatHasGone` |

## What it blocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blocking sphere is a little bigger than the circle and full size from the first turn; its centre never moves | done | `src/Particles/Rules/Shield.cpp` (the defensive sphere, magnitude x 1.11062, made once at the effect's origin); tests `Shield.sphereHelpers`, `Shield.defensiveSphereRegistryAndDeflection` |
| Fireballs bounce off it like a mirror | done | `particles::shields::DoAnyShieldDeflections` (`src/Particles/Rules/Shield.cpp`) called from `src/Particles/Rules/Fireball.cpp`; see [fireball.md](fireball.md) Test `Shield.defensiveSphereRegistryAndDeflection` |
| Lightning forks stop at its surface | done | `src/Particles/Rules/Lightning.cpp` (a fork split inside a sphere stops at the intersection); see [lightning.md](lightning.md); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.ALightningForkEndsWhereItEntersAShieldThatHolds` |
| A blast centred inside it is cancelled | done | `src/Particles/Rules/Explosion.cpp`; see [blast.md](blast.md); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.ABlastCentredInsideAShieldThatHoldsIsCancelled` |
| A flock member flying in dies | partial 80% | `src/Magic/Spells/SpellFlock.cpp`; see [flocks.md](flocks.md) No test or check covers it |
| Each blocked attack drains the shield by the attacker's own cost; when it runs dry the attack gets through and the shield breaks | done | `src/Magic/Core/SpellEvent.cpp` (spell hits spell: the shield pays strength x costPerShieldCollide, struck or destroyed reaction) Scenario `miracles.shield` (blocking); test `test/test_shield_miracle.cpp` `ShieldMiracleSpells.EachBlockedAttackDrainsTheShieldByTheAttackersCostUntilItRunsDryAndBreaks` |
| An area effect that only meets a shield costs the attacker but doesn't drain the shield (as the game does) | done | `src/Magic/Core/SpellEvent.cpp` (the shield check sends the event to the attacker itself); test `test/test_shield_miracle.cpp` `ShieldMiracleSpells.AnAreaEffectThatOnlyMeetsAShieldCostsItsCasterAndNotTheShield` |
| Other gods' hands have no influence inside it, so they can't cast there | partial 80% | `SpellShield.cpp` (an anti-influence ring per other player, `src/ECS/Influence/InfluenceRings.cpp`) No test covers the anti-influence ring |
| Other players' creatures walk round it | partial 80% | `map_shield::CreatureMustAvoid` (`src/Magic/Objects/MapShield.cpp`) asked by the creature's route planner (`src/ECS/RoutePlanWorld.cpp`); test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic`. The original creature path's own use of the test is still unread ([wiki Pending](../../../magic/shields.md#pending)) |
| Someone inside ignores reactions to things outside it | done | `src/ECS/Effects/Reactions.cpp` (`map_shield::IsReactionBlockedByShield` for every living class); switching reactions is ported (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) Tests in `test/test_villager_reactions.cpp` |

## Villagers' and creatures' reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers near the shield (its size plus 30 m) and their town within 500 m take notice | done | `SpellShield.cpp` (reaction 13, radius r + 30; nearest town under 500 m), `src/ECS/Systems/Implementations/VillagerShield.cpp`. Scenario `miracles.shield_people` |
| They shelter only when their town was attacked lately | done | `VillagerShield.cpp` (`ReactToMagicShieldPriority`: the town's protection desire from `src/ECS/TownDesire.cpp` and the town's aggressor turn); test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic`; scenario `miracles.shield_people`. An attack made on the very first turn reads differently (a minor difference, [wiki](../../../magic/shields.md#openblack)) |
| Reaction length and the wait before reacting again follow the table, whatever the distance | partial 80% | `VillagerShield.cpp` (`MayReactAgain`, the reaction records) No test covers it. Not pinned: the original's wait goes through the type's own rule (`villager_shield::TurnsBeforeReactingToShieldAgain`: never again for a town not attacked lately, at once when the dome does not cover the walk's goal, else the standard turns scaled by the distance with info.dat's 2000 turns, 35 m and 0.1), while the record test here passes the table's 2000 unscaled; the original's spread for this type is not read yet to tell which holds |
| Sheltering villagers walk well inside on their own side and face outward | done | `villager_shield::SetupReactToMagicShield` (`VillagerShield.cpp`) Scenario `miracles.shield_people` |
| Sheltering villagers point, look at the hand or stand | done | `villager_shield::AmazedByMagicShieldReaction` and `UpdateAmazedClip` (`VillagerShield.cpp`) play the amazed clips, pointing then talking and pointing. Scenario `miracles.shield_people` |
| A struck shield gives a "struck" reaction | partial 40% | `spell_shield::UpdateStruckReaction` (`SpellShield.cpp`) creates and refreshes reaction 35, but no villager takes it up. ([wiki Pending](../../../magic/shields.md#pending)) |
| When it breaks, villagers run from where it stood | partial 80% | Reaction 36 is taken up (`villager_cover::SetupReactToMagicShieldDestroyed`, `src/ECS/Systems/Implementations/VillagerCover.cpp`): a villager hides, or is set to run from where it stood; the run is given up on the next turn unless it was watching something, as the original (see ../vanilla_bugs/villagers_and_towns.md); tests in `test/test_villager_hiding.cpp`; scenarios `miracles.shield_fall_hide`, `miracles.shield_fall_panic`. Imported from raffclar's tree; its listener is not yet read against the exe ([wiki Pending](../../../magic/shields.md#pending)) |
| A broken shield impresses four times as much; a shield over the attacker's own town impresses nothing | partial 80% | `magic::ShieldImpressiveValue` (`src/Magic/Impressiveness.cpp`), used by `src/ECS/Systems/Implementations/VillagerReactions.cpp`; test in `test/test_magic_foundation.cpp` (nothing over the attacker's own town). The first-turn aggressor test differs a little ([wiki](../../../magic/shields.md#openblack)) |
| The caster's creature is impressed by it as a nice miracle and can learn it | partial 70% | Creatures react to nice miracles and learn from them (`ReactToNiceMagic`, `src/ECS/Systems/Implementations/CreatureReactions.cpp`; tests in `test/creature/test_creature_reactions.cpp`); the shield's own case is not covered |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fifteen player-coloured patches orbit the sphere, cut off at the ground | done | SF_DefenseSphere through `src/Particles/ParticleEffect.cpp` (sphere surface tracer) and `src/Particles/Creators/Mesh.cpp` (player colour, cut by the ground plane) Scenario `miracles.shield` |
| The sphere grows from a tenth to full size over 2 s and keeps the spin of the casting hand | done | data-driven, with the initial spin from the cast's curl (`src/Particles/ParticleEffect.cpp`, `src/Particles/Rules/Shield.cpp`); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.TheDomeGrowsFromATenthToFullSizeOverTwoSecondsAndKeepsTheCastsSpin` (on SF_DefenseSphere's rules; the first step's age is openblack's step order) |
| Patches fade in over about 8.5 s and fade with the shield's strength, never below a floor | done | `src/Particles/Rules/Shield.cpp` (vapour end effect: alpha = age x 30; collection alpha = strength x 255, 40 to 255); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.ThePatchesFadeInOverEightAndAHalfSecondsAndWithTheStrengthDownToAFloor` |
| A hit raises up to four crackling spark arcs that wiggle and fade over 3 s | done | `src/Particles/Rules/Shield.cpp` (shield spark rule, its defaults the original's: a 3 s life and a wiggle of 0.08 of the radius, which the dome's file leaves to the rule); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.ASparkLivesThreeSecondsAndItsArcWigglesByAnEighthOfATenthOfTheRadius`; scenario `miracles.shield` |
| On close-down it shrinks over 2 s and is gone at 2.2 s | done | Data-driven (SF_DefenseSphere: `AR_FadeOutOnceConditionTrue`, `RemoveRuleAfterCloseDown` in `src/Particles/ParticleEffect.cpp`); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.OnCloseDownTheDomeShrinksOverTwoSecondsAndGoesAtTwoPointTwo` |
| Orbiting patches round the miracle held in the hand | done | the seed's in-hand effect (`src/Magic/Hand/HandMagicFX.cpp`); the patches turned outwards to form one spinning ball (`src/Particles/ParticleEffect.cpp`, orient to surface) Scenario `miracles.hand_hold_poses` |
| Effect on a holder of the miracle | partial 60% | The seed's holder effect is made from the seed's row (`src/Worship/SpellSeedGraphic.cpp`); no test or check for the shield's, not compared with the original |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A looping hum, one of three sizes by the shield's size | done | data-driven particle sound (the dome's root create rule in `src/Particles/ParticleEffect.cpp`, `spell_sounds::SizeFromRadius` in `src/Audio/Services/SpellSounds.cpp`); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.TheHumLoopsSizedByTheDomeAndStopsAtOnceWhenTheDomeGoes` |
| The hum stops at once when the shield goes | done | `src/Audio/Services/SpellSounds.cpp` (stop or soft release by the sound's flag when its atom dies); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.TheHumLoopsSizedByTheDomeAndStopsAtOnceWhenTheDomeGoes` |
| Each hit plays a spark sound | done | `src/Particles/Rules/Shield.cpp` (SoundSpark of each spark atom); test `test/test_shield_miracle.cpp` `ShieldMiracleParticles.EachHitOnTheDomePlaysASparkSound` |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast the spiritual shield (it's its defensive miracle in fights) | partial 70% | The creature's plan casts it (`CastShield`, `CastShieldPU1`, `CastShieldAroundTown`, `src/Creature/CreaturePlanActions.cpp`); scenarios `miracles.creature_casts_shield`, `miracles.creature_shield_idles`; not yet checked against the original (issue #172) |
| Shields and their reactions are kept in a saved game | pending | openblack has no game saving |
