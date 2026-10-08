# Physical shield

A solid dome the player draws with a circle gesture. It rises out of the land, spins down and bobs, and stops thrown
rocks and other physical objects; each blow drains the caster's prayer power. Miracles pass through it.

Given by gold scroll: [Khazar's Shield Challenge](../story/gold_scrolls/khazars_shield_challenge.md).

**Progress: 23/28 done, 2 partial — 86%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it, held to the side in the hand | done | `src/Magic/Gestures/PowerUpSystem.cpp` (the circle), `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (HAND_GESTURE cast on release), hold pose from the seed's row; see [casting_and_globes.md](casting_and_globes.md) |
| Size kept between 5 and 1000 m; upkeep grows with the square of the size | done | `src/Magic/Spells/SpellShield.cpp` (`ClampShieldRadius`, `ShieldCostToMaintain`); `test/test_shield.cpp` (radiusClamp, upkeepGrowsWithTheSquareOfTheRadius) |
| Other gods lose their influence inside it; their creatures walk round it | done | `SpellShield.cpp` (an anti-influence ring per other player, `src/ECS/Influence/InfluenceRings.cpp`); creatures: `map_shield::CreatureMustAvoid` asked by `src/ECS/RoutePlanWorld.cpp` |
| Creatures never cast it | done | correctly absent: no creature casts shields in our tree |

## The dome

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sized by the circle and sunk into the land | done | `src/Magic/Objects/MapShield.cpp` (`Create`, `ProcessPhysical`: scale 0.017 r, height from the shield row); `test/test_shield.cpp` (realInfoDat) |
| Hidden for half a second, then grows over 1.5 s, spins down from the hand's spin over 6 s and bobs | done | `MapShield.cpp` (`CurvesAt`, `ProcessPhysical`); `test/test_shield.cpp` (physicalShieldCurves) |
| A dying dome fades over 1.5 s and is gone at 2.25 s | done | `MapShield.cpp` (`SetDying`, die time 1.5 s, deleted at 2.25 s); `test/test_shield.cpp` (physicalShieldCurves) |
| Drawn by its strength, never fainter than a floor | done | `MapShield.cpp` (`DrawPhysical`: alpha max(40, strength x 255), not drawn before 0.5 s) |
| The solid shell mesh, its outer layer drawn additive | done | `MapShield.cpp` (`ReplaceMaterialType(5, 13)` and `(4, 13)` on mesh 554), `src/Graphics/Renderer.cpp`; `test/test_shield.cpp` (physicalShieldMaterialTypes) |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its solid shape is a cone under the dome | done | `map_shield::IsPointDefinitelyWithinShieldVolume` (`MapShield.cpp`); the physics body is the mesh's physics sub-mesh (`src/ECS/Physics/PhysicsObjects.cpp`) |
| A thing crosses its hull only moving against a face | done | the ported rigid-body contact against the shield's collision mesh (`src/ECS/Physics/PhysicsObjects.cpp`) |
| Thrown objects bounce off with the game's spring response | done | `src/ECS/Physics/PhysicsObjects.cpp` (springs of physics constants row 10, weight from the shield row); a very fast rock may pass the shell, not proven to differ from the original |
| The hull follows the dome's size as it grows | done | `map_shield::CollisionScale` used by `PhysicsObjects.cpp`: the collision scale jumps behind the drawn one (more than 0.3 apart), as the original |
| A blow by a rock costs the dome by the rock's speed and mass | done | `map_shield::ReactToPhysicsImpact` (`MapShield.cpp`): pays speed x mass x chantCostPerImpactMomentum x 0.0001, forced |
| A blow counts a small good deed at the point and marks the thrower as the town's attacker | done | `map_shield::ReactToPhysicsImpact`: a spell event at the hitter's point, and the town's aggressor and turn recorded (the per-player aggression slots are not ported) |
| Fireballs and other particle miracles pass through it | done | correctly absent: the physical shield makes no deflection sphere (`src/Particles/Rules/Shield.cpp`) |
| The hand can't pick up or feel the dome | partial | the hand does not feel a shield's mesh (`src/ECS/Systems/Implementations/HandPlacement.cpp`); `HandSystem::ValidForPlaceInHand` does not exclude shields, so whether one could be grabbed is not settled |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers shelter under it as under the spiritual shield | done | the same reaction 13 as the spiritual shield (`SpellShield.cpp`, `src/ECS/Systems/Implementations/VillagerShield.cpp`) |
| A blow gives a "struck" reaction; emptying it gives a "destroyed" one | partial | `spell_shield::UpdateStruckReaction` / `SetUpDestroyedReaction` create reactions 35 and 36, but no villager or creature takes them up |
| Over the attacker's own town it impresses nothing | todo | no impressiveness or miracle belief in our tree (`src/ECS/Systems/Implementations/VillagerReactions.cpp` leaves the town belief out) |
| The caster's creature is impressed and learns the spiritual shield from it | todo | creatures take no miracle reactions in our tree (no creature reaction handler in `src/ECS/Effects/Reactions.cpp`) |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing points ride the dome, trailing player-coloured sparkles | done | SF_PhysicalShieldFX made by `MapShield.cpp` and stepped each turn; atoms at the object (`src/Particles/PSys.cpp`), drawn in the player's colour |
| The sparkles grow with the dome's size and fade down over 6 s | done | data-driven (SF_PhysicalShieldFX, magnitude = radius) |
| The effect fades with the dome | done | `MapShield.cpp` (`DrawPhysical`: the effect's alpha is the shield's, cut while dying as the original) |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound when it is made | done | data-driven particle sound (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`) |
| A looping hum, let go to ring out when the dome is deleted | done | `src/Audio/Services/SpellSounds.cpp` (release of the loop when its atom dies) |
| A collision sound when things hit it | done | `src/ECS/Physics/CollisionSounds.cpp` (`TypeOf`: the shield row's collide sound) |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The dome is kept in a saved game | todo | openblack has no game saving |
