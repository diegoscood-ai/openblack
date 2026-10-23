# Physical shield

A solid dome the player draws with a circle gesture. It rises out of the land, spins down and bobs, and stops thrown
rocks and other physical objects; each blow drains the caster's prayer power. Miracles pass through it.

Given by gold scroll: [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md).

**Progress: 11/30 done, 0 chosen, 14 partial, 1 inferred — 72%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/shields.md).

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it, held to the side in the hand | done | `src/Magic/Gestures/PowerUpSystem.cpp` (the circle), `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (HAND_GESTURE cast on release), hold pose from the seed's row; see [casting_and_globes.md](casting_and_globes.md) Scenarios `miracles.shield_sizes`, `miracles.hand_circle_casts`, `miracles.gesture_circle_shield` |
| Size kept between 5 and 1000 m; upkeep grows with the square of the size | done | `src/Magic/Spells/SpellShield.cpp` (`ClampShieldRadius`, `ShieldCostToMaintain`); tests `Shield.radiusClamp`, `Shield.upkeepGrowsWithTheSquareOfTheRadius` |
| Other gods lose their influence inside it; their creatures walk round it | partial 80% | `SpellShield.cpp` (an anti-influence ring per other player, `src/ECS/Influence/InfluenceRings.cpp`); creatures: `map_shield::CreatureMustAvoid` asked by `src/ECS/RoutePlanWorld.cpp`. Test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic` covers the avoidance test; no test covers the anti-influence ring, and the creature path's own use of it in the original is still unread ([wiki Pending](../../../magic/shields.md#pending)) |
| A creature can cast it as a helpful miracle at a target | partial 70% | The creature's plan has a physical-shield cast (`CastPhysicalShield`, `src/Creature/CreaturePlanActions.cpp`), and the original gives creatures their own lifetime for it ([page](../../../magic/shields.md#the-spell)); not yet checked against the original (issue #172). The row said creatures never cast it, which the original does not bear out |

## The dome

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sized by the circle and sunk into the land | done | `src/Magic/Objects/MapShield.cpp` (`Create`, `ProcessPhysical`: scale 0.017 r, height from the shield row); test `Shield.realInfoDat`; scenario `miracles.shield_sizes` |
| Hidden for half a second, then grows over 1.5 s, spins down from the hand's spin over 6 s and bobs | done | `MapShield.cpp` (`CurvesAt`, `ProcessPhysical`); test `Shield.physicalShieldCurves`; scenario `miracles.shield_physical_fade` |
| A dying dome fades over 1.5 s and is gone at 2.25 s | done | `MapShield.cpp` (`SetDying`, die time 1.5 s, deleted at 2.25 s); test `Shield.physicalShieldCurves`; scenario `miracles.shield_physical_fade` |
| Drawn by its strength, never fainter than a floor | partial 80% | `MapShield.cpp` (`DrawPhysical`: alpha max(40, strength x 255), not drawn before 0.5 s) No test covers the floor |
| The solid shell mesh, its outer layer drawn additive | done | `MapShield.cpp` (`ReplaceMaterialType(5, 13)` and `(4, 13)` on mesh 554), `src/Graphics/Renderer.cpp`; test `Shield.physicalShieldMaterialTypes`. The order of the submeshes within the object is inferred and hardly matters ([wiki](../../../magic/shields.md#openblack)) |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its solid shape is a cone under the dome | partial 80% | `map_shield::IsPointDefinitelyWithinShieldVolume` (`MapShield.cpp`); the physics body is the mesh's physics sub-mesh, met by the rigid-body contact in `src/ECS/Systems/Implementations/DynamicsSystem.cpp`; no test covers the shape |
| A thing crosses its hull only moving against a face | partial 80% | The ported rigid-body contact against the shield's collision mesh (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`); no test covers it |
| Thrown objects bounce off with the game's spring response | partial 80% | `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (springs of physics constants row 10, weight from the shield row); scenario `miracles.shield_physical_throw`. A very fast rock passes the shell; not proven to differ from the original, which needs a capture of it ([wiki Pending](../../../magic/shields.md#pending)) |
| The hull follows the dome's size as it grows | partial 80% | `map_shield::CollisionScale` used by `src/ECS/Systems/Implementations/DynamicsSystem.cpp`: the collision scale jumps behind the drawn one (more than 0.3 apart), as the original; no test covers it |
| A blow by a rock costs the dome by the rock's speed and mass | done | `map_shield::ReactToPhysicsImpact` (`MapShield.cpp`): pays speed x mass x chantCostPerImpactMomentum x 0.0001, forced. Scenario `miracles.shield_physical_throw`; no unit test |
| A blow counts a small good deed at the point and marks the thrower as the town's attacker | partial 80% | `map_shield::ReactToPhysicsImpact`: a spell event at the hitter's point, and the town's aggressor recorded through `Town::UpdateAggressor` with its per-player slots ([wiki](../../../magic/shields.md#openblack)); no test covers the shield's call |
| Fireballs and other particle miracles pass through it | done | Correctly absent: the deflection spheres belong to the spiritual shield's effect, the physical shield makes none (`src/Particles/Rules/Shield.cpp`; [page](../../../magic/shields.md#the-particles)); test `Shield.defensiveSphereRegistryAndDeflection` (the spheres) |
| The hand can't pick up or feel the dome | partial 80% | the hand does not feel a shield's mesh (`src/ECS/Systems/Implementations/HandPlacement.cpp`); `HandSystem::ValidForPlaceInHand` is an allowlist that does not name shields, so a shield never goes into the hand, as in the original |
| The shield is in the map's object grid, so effects at a point reach it (the physical one takes effects other than burning) | pending | The shields are not in the map grid, so `ApplyEffectToMapPos` never reaches them ([wiki Pending](../../../magic/shields.md#pending); issue #56) |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers shelter under it as under the spiritual shield | done | the same reaction 13 as the spiritual shield (`SpellShield.cpp`, `src/ECS/Systems/Implementations/VillagerShield.cpp`) Test `Shield.villagerReactionPriorityAndCreatureMustAvoidSynthetic`; scenario `miracles.shield_people` |
| A blow gives a "struck" reaction; emptying it gives a "destroyed" one | partial 40% | `spell_shield::UpdateStruckReaction` / `SetUpDestroyedReaction` create the strike and destruction reactions, villagers take up the destroyed one (`SetupReactToMagicShieldDestroyed`, `VillagerCover.cpp`); missing: the struck one is taken up by nobody, and creatures take neither ([wiki Pending](../../../magic/shields.md#pending)) |
| Over the attacker's own town it impresses nothing | pending | The physical shield's own impressiveness is not ported: `MiracleImpressiveValue` (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) has no physical-shield branch ([wiki Pending](../../../magic/shields.md#pending)) |
| The caster's creature is impressed and learns the spiritual shield from it | pending | Creatures react to nice miracles (`ReactToNiceMagic`, `src/ECS/Systems/Implementations/CreatureReactions.cpp`), but the physical shield's impressiveness is missing and what it teaches is not checked |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing points ride the dome, trailing player-coloured sparkles | done | SF_PhysicalShieldFX made by `MapShield.cpp` and stepped each turn; atoms at the object (`src/Particles/PSys.cpp`), drawn in the player's colour. Scenario `miracles.shield_physical_fade` |
| The sparkles grow with the dome's size and fade down over 6 s | partial 80% | Data-driven (SF_PhysicalShieldFX, magnitude = radius); no test or check covers the growth |
| The effect fades with the dome | done | `MapShield.cpp` (`DrawPhysical`: the effect's alpha is the shield's, cut while dying as the original) Scenario `miracles.shield_physical_fade` |
| The dome and its effect, at the same point, are drawn in the original's order | inferred | Sorted with a non-stable sort; what the original's sorter does with equal keys is unread ([wiki Pending](../../../magic/shields.md#pending); issue #56) |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound when it is made | partial 80% | data-driven particle sound (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`) No test covers it |
| A looping hum, let go to ring out when the dome is deleted | partial 80% | `src/Audio/Services/SpellSounds.cpp` (release of the loop when its atom dies) No test covers it |
| A collision sound when things hit it | partial 80% | `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (`CollideSoundType`: the shield row's collide sound) No test covers it |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dome is kept in a saved game | pending | openblack has no game saving |
