# Physical shield

A solid dome the player draws with a circle gesture. It rises out of the land, spins down and bobs, and stops thrown
rocks and other physical objects; each blow drains the caster's prayer power. Miracles pass through it.

Given by gold scroll: [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md).

**Progress: 21/30 done, 0 chosen, 9 partial, 0 inferred — 91%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/shields.md).

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it, held to the side in the hand | done | `src/Magic/Gestures/PowerUpSystem.cpp` (the circle), `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (HAND_GESTURE cast on release), hold pose from the seed's row; see [casting_and_globes.md](casting_and_globes.md) Scenarios `miracles.shield_sizes`, `miracles.hand_circle_casts`, `miracles.gesture_circle_shield` |
| Size kept between 5 and 1000 m; upkeep grows with the square of the size | done | `src/Magic/Spells/SpellShield.cpp` (`ClampShieldRadius`, `ShieldCostToMaintain`); tests `Shield.radiusClamp`, `Shield.upkeepGrowsWithTheSquareOfTheRadius` |
| Other gods lose their influence inside it; their creatures walk round it | partial 80% | `SpellShield.cpp` (an anti-influence ring per other player, `src/ECS/Influence/InfluenceRings.cpp`); creatures: `map_shield::CreatureMustAvoid` asked by `src/ECS/RoutePlanWorld.cpp`. Test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic` covers the avoidance test; no test covers the anti-influence ring, and the creature path's own use of it in the original is still unread ([wiki Pending](../../../magic/shields.md#pending)) |
| A creature can cast it as a helpful miracle at a target | done | `CastPhysicalShield` at anything, cast the way the heal is (`Build::CastHelpful`, `src/Creature/CreaturePlanActions.cpp`), as the original's helpful construct 0x4A6910 builds PhysicalShield 184 ([casting.md](../../../creature/casting.md#the-attack-shield-and-impress-constructs)), and the original gives creatures their own lifetime for it ([page](../../../magic/shields.md#the-spell)); test `CreaturePlanActionsCasting.EachShieldIsCastAtAnythingItsOwnWay`; see [../creature/creature_casting.md](../creature/creature_casting.md). The row said creatures never cast it, which the original does not bear out |

## The dome

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sized by the circle and sunk into the land | done | `src/Magic/Objects/MapShield.cpp` (`Create`, `ProcessPhysical`: scale 0.017 r, height from the shield row); test `Shield.realInfoDat`; scenario `miracles.shield_sizes` |
| Hidden for half a second, then grows over 1.5 s, spins down from the hand's spin over 6 s and bobs | done | `MapShield.cpp` (`CurvesAt`, `ProcessPhysical`); test `Shield.physicalShieldCurves`; scenario `miracles.shield_physical_fade` |
| A dying dome fades over 1.5 s and is gone at 2.25 s | done | `MapShield.cpp` (`SetDying`, die time 1.5 s, deleted at 2.25 s); test `Shield.physicalShieldCurves`; scenario `miracles.shield_physical_fade` |
| Drawn by its strength, never fainter than a floor | done | `MapShield.cpp` (`DrawPhysical`: alpha max(40, strength x 255), not drawn before 0.5 s); test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracle.ItIsDrawnFromHalfASecondAndNeverFainterThanTheFloor` |
| The solid shell mesh, its outer layer drawn additive | done | `MapShield.cpp` (`ReplaceMaterialType(5, 13)` and `(4, 13)` on mesh 554), `src/Graphics/Renderer.cpp`; test `Shield.physicalShieldMaterialTypes`. The order of the submeshes within the object is inferred and hardly matters ([wiki](../../../magic/shields.md#openblack)) |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its solid shape is a cone under the dome | done | `map_shield::IsPointDefinitelyWithinShieldVolume` (`MapShield.cpp`); the physics body is the mesh's physics sub-mesh, met by the rigid-body contact in `src/ECS/Systems/Implementations/DynamicsSystem.cpp`; test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracle.ItsSolidShapeIsAConeOfItsRadiusAndHeight` |
| A thing crosses its hull only moving against a face | partial 80% | The ported rigid-body contact against the shield's collision mesh (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`); no test covers it |
| Thrown objects bounce off with the game's spring response | partial 80% | `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (springs of physics constants row 10, weight from the shield row); scenario `miracles.shield_physical_throw`. A very fast rock passes the shell; not proven to differ from the original, which needs a capture of it ([wiki Pending](../../../magic/shields.md#pending)) |
| The hull follows the dome's size as it grows | done | `map_shield::CollisionScale` used by `src/ECS/Systems/Implementations/DynamicsSystem.cpp`: the collision scale jumps behind the drawn one (more than 0.3 apart), as the original (0.68, 0.276, 0.597 for a radius of 40); test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracle.TheCollisionScaleJumpsBehindTheDrawnOne` |
| A blow by a rock costs the dome by the rock's speed and mass | done | `map_shield::ReactToPhysicsImpact` (`MapShield.cpp`): pays speed x mass x chantCostPerImpactMomentum x 0.0001, forced. Scenario `miracles.shield_physical_throw`; no unit test |
| A blow counts a small good deed at the point and marks the thrower as the town's attacker | partial 80% | `map_shield::ReactToPhysicsImpact`: a spell event at the hitter's point, and the town's aggressor recorded through `Town::UpdateAggressor` with its per-player slots ([wiki](../../../magic/shields.md#openblack)); no test covers the shield's call |
| Fireballs and other particle miracles pass through it | done | Correctly absent: the deflection spheres belong to the spiritual shield's effect, the physical shield makes none (`src/Particles/Rules/Shield.cpp`; [page](../../../magic/shields.md#the-particles)); test `Shield.defensiveSphereRegistryAndDeflection` (the spheres) |
| The hand can't pick up or feel the dome | partial 80% | the hand does not feel a shield's mesh (`src/ECS/Systems/Implementations/HandPlacement.cpp`); `HandSystem::ValidForPlaceInHand` is an allowlist that does not name shields, so a shield never goes into the hand, as in the original |
| The shield is in the map's object grid, so effects at a point reach it (the physical one takes effects other than burning) | partial 60% | Both shields are put at the head of their cell's fixed list as they are made (`map_cells::InsertMapObject` in `src/Magic/Objects/MapShield.cpp`), so the effects read from the map's cells find them; the physical one takes an effect that does not burn, the spiritual one none (`src/ECS/Effects/EffectValues.cpp`; test `RepairsTest.NoEffectReachesAWorshipIconAVortexAHighlightOrASpiritualShieldNorFireABareField`); 60% by the share rule: not seen in play ([shields](../../../magic/shields.md#pending); issue #56) |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers shelter under it as under the spiritual shield | done | the same reaction 13 as the spiritual shield (`SpellShield.cpp`, `src/ECS/Systems/Implementations/VillagerShield.cpp`) Test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic`; scenario `miracles.shield_people` |
| A blow gives a "struck" reaction; emptying it gives a "destroyed" one | done | `spell_shield::UpdateStruckReaction` / `SetUpDestroyedReaction` create the strike and destruction reactions (`UpdateStruckReaction` 0x72B780, [shields](../../../magic/shields.md#the-villagers-reactions)); villagers take up the destroyed one (`SetupReactToMagicShieldDestroyed`, `VillagerCover.cpp`), and a villager amazed by a shield takes up the struck one without changing its state (`villager_shield::SetupReactToMagicShieldStruck`, as `Villager::SetupReactToMagicShieldStruck` 0x7674B0 with its priority 0x767540); tests `ShieldMiracleSpells.EachBlockedAttackDrainsTheShieldByTheAttackersCostUntilItRunsDryAndBreaks`, `VillagerShieldPure.AStruckShieldDrawsOnlyAVillagerAmazedByAShieldLookingElsewhere`, `VillagerReactionsTest.AVillagerAmazedByAShieldTurnsToAStruckShieldWithoutChangingState`. Creatures and animals take neither, as the original (read 2026-11-09): `Creature` and every animal class keep `Living`'s `ReactToMagicShieldStruckPriority` 0x417230 and `ReactToMagicShieldDestroyedPriority` 0x417240 (both 0, vtable +0xAB4 / +0xAB8) and its empty `SetupReactToMagicShieldStruck` 0x4171C0 / `SetupReactToMagicShieldDestroyed` 0x4171D0 (+0xA24 / +0xA28); only the villagers override them |
| Over the attacker's own town it impresses nothing | done | its people take the reaction its spell starts, as the spiritual shield's, and the spell's impression gives nothing to a town whose last attacker is the shield's player: `magic::ShieldImpressiveValue` from `MiracleImpressiveValue` (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) for every spell of the Shield class, the physical shield's included, as the original's shield spell for both ([the impression](../../../magic/shields.md)); test `ImpressionTest.APhysicalShieldImpressesNothingTheTownItsPlayerAttacked`; raffclar's #661 shield piece is ours already |
| The caster's creature is impressed and learns the spiritual shield from it | partial 50% | What it teaches is ported: a creature taking up a shield's reaction learns the spiritual shield whatever the shield's kind (`ReactToNiceMagic` with the spiritual shield, `src/ECS/Systems/Implementations/CreatureReactions.cpp`; test `CreatureReactionHandlerTest.AShieldTeachesTheSpiritualShield`). Missing: how impressed the creature is (the shield object's own impressive value is reached by no reaction it starts, [shields](../../../magic/shields.md#pending)) is not checked |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing points ride the dome, trailing player-coloured sparkles | done | SF_PhysicalShieldFX made by `MapShield.cpp` and stepped each turn; its five atoms at the five points round the dome's base, the solid shield mesh's extra metrics (`AtomsAtEPTarget`, `src/Particles/Rules/Shield.cpp`; test `Shield.atomsAtTheMeshsExtraPoints`; [shields](../../../magic/shields.md#the-particles)), drawn in the player's colour. Scenario `miracles.shield_physical_fade` |
| The sparkles grow with the dome's size and fade down over 6 s | done | Data-driven (SF_PhysicalShieldFX, magnitude = radius); test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracleEffect.TheSparklesGrowWithTheRadiusAndFadeDownOverSixSeconds` |
| The effect fades with the dome | done | `MapShield.cpp` (`DrawPhysical`: the effect's alpha is the shield's, cut while dying as the original) Scenario `miracles.shield_physical_fade` |
| The dome and its effect, at the same point, are drawn in the original's order | partial 60% | both go to the one back-to-front queue, which keeps two equal keys in the order they came ([the single transparent queue](../../../rendering/transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter), `NewZObject` 0x83F310; openblack's `zsort::Queue`, `src/Graphics/ZSort.h`, test `ZSorter.farToNearAndStable`), and the effect comes first: the original queues the particles (`PSysGlobal::DrawLoop` 0x68F5E0) and the fireflies, then the domes (`Spell::DrawSpells` 0x7203F0 → fn_0072BF50, the newest shield first); openblack's renderer holds a queued dome back from the models and submits it after the particle effects and the sprites, the newest shield first (`src/Graphics/Renderer.cpp`, [wiki](../../../magic/shields.md#openblack)); not yet tested: missing a test that queues a dome and a particle effect with equal keys and checks that the effect is drawn first |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound when it is made | done | data-driven particle sound (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`); test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracleEffect.ASoundWhenItIsMadeSizedByItsRadius` |
| A looping hum, let go to ring out when the dome is deleted | done | `src/Audio/Services/SpellSounds.cpp` (release of the loop when its atom dies); test `test/test_physical_shield_miracle.cpp` `PhysicalShieldMiracleEffect.ALoopingHumLetGoSoftlyWhenTheShieldGoes` |
| A collision sound when things hit it | done | `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (`CollideSoundType`: the shield row's collide sound, the physical shield's the second row, [page](../../../magic/shields.md)); test `test/test_particle_carried_objects.cpp` `ParticleCarriedObjectsTest.AShieldSoundsWhenStruckAsItsOwnShieldRowSays` |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dome is kept in a saved game | partial 60% | written and made again: the shield spell and its physical shield (`src/ECS/SavedWorld/WriteSpells.cpp`, tests `SpellWrite.AShieldSpellWritesItsWorldShieldItsRingsAndItsTwoReactions`, `SpellWrite.APhysicalShieldWritesItsValuesAndItsMatricesAsTheGameKeepsThem`), its effect written with its file named by the physical shield's particle type (test `ParticleWrite.APhysicalShieldsEffectNamesItsFileByThePhysicalShieldsParticleType`); a load makes the physical shield again with its values, its two matrices and its effect, and its spell with its rings and its two reactions (`saved_spells::MakePhysicalShields`, `saved_spells::MakeSpells`, test `SpellLoad.APhysicalShieldAndItsSpellAreMadeAgainAndWrittenBackAsRead`); missing: a save and load with the dome up seen in play ([saved games](../../../engine/saved-games.md#writing-the-world-file)) |
