# Explosions

Blasts in Black & White: the engine pieces every explosion shares (an area burst of heat and crushing, a shock wave
that runs outward and shatters what it reaches into flying pieces, rubble and scorch heaps, dust puffs, smoke or steam,
camera shake and the bang), and the other things that go off with a bang. The game has no exploding barrels or fire
explosions: fire only flares into extra flames when very hot (see [fire.md](fire.md)). The blast miracle's own timings,
power-ups and casting are in [../miracles/blast.md](../miracles/blast.md); the fireball's impact in
[../miracles/fireball.md](../miracles/fireball.md); lightning strikes in [../miracles/lightning.md](../miracles/lightning.md).
Pieces knocked off a building by a thrown rock are in [impact_damage.md](impact_damage.md).

openblack: the explosion rules and the wave's effect on objects in `src/Particles/Rules/Explosion.cpp`, the shattering in
`src/Particles/Rules/ExplodeObject.cpp`, rubble and dust in `src/ECS/GroundMarks.cpp`, the shake (scripts only) in
`src/Camera/CameraShake.cpp`, tests in `test/test_explosion.cpp`.

**Progress: 32/43 done, 0 chosen, 8 partial, 0 inferred — 87%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## The burst

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion acts at the land under it (sea level over water); it starts acting after its delay and keeps acting for its set time | done | `src/Particles/Rules/Explosion.cpp` (the centre at the land's altitude, events from its initial delay for its time); test `Explosion.blastEventsCadenceAndCloseDown` (`test/test_explosion.cpp`) |
| Every turn while it acts it applies its burst: heat (which sets things alight), crushing and a blow, over a fixed radius | done | `src/Particles/Rules/Explosion.cpp` (one spell event a step: the spell's burn, crush and hit in its effect radius); test `Explosion.blastEventsCadenceAndCloseDown`, scenario `miracles.blast`; heat feeds [fire.md](fire.md) |
| The burst is the same everywhere inside its radius: there is no falloff with distance | done | `EffectValues::ApplyEffectToMapPos` (`src/ECS/Effects/EffectValues.cpp`): every receiver in reach takes the whole effect; test `GameMagicWorldObjects.AnEffectAtAPointReachesEveryObjectAboutItAndTheOldCallAnswersTheLast` |
| The burst's strength scales with what the caster paid, the tribe's power and the event's own strength; the radius does not | partial 80% | `src/Magic/Core/Chants.cpp` and the effect values; the multiplier is read (`magic::GetTribalPower`), but nothing in the game sets a player's tribal power (only the testbed), so it is always 1 |
| A burst hurts through each thing's defences, moves the caster's alignment and angers the towns it harms | done | `ecs::effects::ApplyEffect` (the info's defence multipliers, the alignment update, the town's emergency), `src/ECS/Effects/EffectValues.cpp`; tests `EffectSourceRules.*`, scenario `miracles.blast` |
| Villagers near an explosion react once for the whole explosion, not once per turn | done | `src/Magic/Core/Spell.cpp` (one reaction per spell, made while it has none); scenario `miracles.blast` |
| A shield over the explosion is struck where the way down enters it, and can stop it | partial 70% | `src/Particles/Rules/Explosion.cpp` (the ray from 200 m above meets the shield: a spark and an event to the shield; a held shield stops the blast), as [lightning-explosion.md](../../../magic/lightning-explosion.md#ur_explosion) says. Missing: no test or scenario casts a blast inside a shield |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave front runs outward at a set speed and reaches each object when it touches the object's edge | done | `src/Particles/Rules/Explosion.cpp` (the ring grows at its spread speed and reaches an object by its radius); scenarios `miracles.blast`, `miracles.blast_spares` |
| The wave's reach grows with the caster tribe's power (1 to 5 times) | partial 80% | The reach times the tribal power (1 to 5) is in `src/Particles/Rules/Explosion.cpp`, but nothing in the game sets a player's tribal power (only the testbed), so it is always 1 |
| The wave takes the fixed and the mobile objects counted in each cell, at most one a turn, and only those it may destroy | done | `src/Particles/Rules/Explosion.cpp` (each cell's fixed list, then its mobile one, as [lightning-explosion.md](../../../magic/lightning-explosion.md#ur_explosion); one object a step, the first that may be destroyed); scenario `miracles.blast_spares` |
| Creatures, fields, temples, teleport stones, totems, totem statues, worship totems, pots and one-off seeds are never destroyed by it | partial 55% | `explosion::CanBeDestroyedBySpell` says no for creatures, fields, the temple and its parts, worship sites and their totems, and village totems (`effects::IsEffectReceiver`); test `GameMagicWorldObjects.NoMiracleDestroysACreatureAFieldOrATemple`. Missing: the classes whose `IsEffectReceiver` says no in bw1-decomp (teleport, pot, one-off seed, totem statue, worship and arena spell icons) and the creature pen (a citadel part); scenario `miracles.blast_spares` expects them spared |
| Indestructible things, and things in a script while a cut-scene holds the screen (unless the spell is a script's), are spared | pending | `Object::CanBeDestroyedBySpell` in bw1-decomp; ours tests neither the `Indestructible` component nor the script and cut-scene test |
| A building reached is shattered into pieces and loses all its life; with a town it becomes a building site drawn at 0 percent (gone from view) until repaired, without one it stays drawn whole; spell dispensers too | done | `explosion::DestroyedByBeam` takes all its life through `ecs::abodes::ReduceLife` (dispensers too), which adds the town's building site (repair base 1.1 x 0 - 0.1) and draws the building at min(built, 0.98 x life) = 0, as [lightning-explosion.md](../../../magic/lightning-explosion.md#ur_explosion) says (code read against the exe); test `RepairsTest.ABuildingTheBlastsWaveReachesBecomesASiteDrawnAtNothingOnlyWithATown` |
| Anything else reached (trees, statues, small objects) shatters and is removed | done | `explosion::DestroyedByBeam` (trees through `DeleteTree`, the rest destroyed as by an effect) after its pieces go to `src/Particles/Rules/ExplodeObject.cpp`; scenarios `miracles.blast`, `miracles.blast_spares` |
| Villagers the wave reaches are shattered and deleted like other objects, with no death | partial 70% | `explosion::DestroyedByBeam` deletes the villager through `ecs::villager::Delete` and its mesh is queued to shatter, as `Object::DestroyedByBeam` in bw1-decomp and [lightning-explosion.md](../../../magic/lightning-explosion.md#ur_explosion). Missing: no test; scenario `miracles.blast` still expects the villagers not to be shattered; a villager's reachability (`IsEffectReceiver`) is taken as yes |
| A shield in the wave's way is struck at the point it is reached, with a spark and the wave's direction | partial 70% | `src/Particles/Rules/Explosion.cpp` (a target inside a shield: a spark and an event to the shield). Missing: no test or scenario has a shield in a wave |
| At most a set number of things shatter and a set number are removed per explosion | partial 90% | `src/Particles/Rules/Explosion.cpp` (the effect file's most to explode and most to delete). Missing: no test or scenario reaches the caps |

## Shattering into pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A shattered model breaks into strips of up to 16 triangles that touch along a shared edge, even across texture seams | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshWalksSharedEdges` |
| Each piece flies away from a point 5 m below the centre at the blast's speed, with some randomness | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshPieceVelocity` |
| Pieces fall, bounce off the land, tumble, fade over 3 s, shrink from 1 to 5 s and are gone at 6 s | done | The effect file's rules run by `src/Particles/PSys.cpp`; scenario `miracles.blast` |
| Pieces make no sound and no ripples when they land, even on water | done | The effect file has none; scenario `miracles.blast_water` |
| Pieces are lit by the land and by their model's own light | done | `mesh_pieces::Build` in `src/Particles/Rules/ExplodeObject.cpp`; tests `Explosion.explodedPieceLitColour`, `Explosion.meshPiecesBuildWorldAndLight` |
| Objects waiting to be shattered are handled newest first | done | `src/Particles/Rules/ExplodeObject.cpp` (the queue is emptied from its last entry); test `Explosion.explodeObjectQueueToPieces` |
| Five rock pieces are thrown out from the centre's height | done | `src/Particles/Rules/Explosion.cpp` (five rock meshes queued to break); scenario `miracles.blast` |

## Marks, dust, smoke and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land counts as dry when it is more than 3 above sea level; the same test picks every dry or wet effect below | done | `pot_resource::IsDryLand` (`src/ECS/PotResource.cpp`), used by `src/Particles/Rules/Explosion.cpp`; test `GameMagicWorldLand.LandIsACellWithoutWaterAndDryLandOneHighAboveTheSea`, scenario `miracles.blast_coast` |
| On dry land a heap of rubble lies at the centre, at a random angle, following the land's shape, for 15 s, fading out over its last second | done | `src/ECS/GroundMarks.cpp` (the mound at a random angle, moulded to the land, 15 s, fading over its last second); scenario `miracles.blast` |
| A brown dust puff of 15 sprites flies up and out, growing and fading over 1.5 s | done | The ground mark's dust (`DisappearSmoke` mode 1); test `Explosion.groundMarkDustIsDisappearSmokeMode1` |
| Grey smoke rises a little after the bang for a few seconds | done | `src/Particles/Rules/Explosion.cpp` (the smoke spot visual after the smoke delay, magnitude 8 for 4 s); scenario `miracles.blast` |
| On water there is no rubble or dust: three rings spread and white steam rises instead | done | `src/Particles/Rules/Explosion.cpp` (steam), `src/Particles/PSysWaterRings.cpp` (three rings); scenarios `miracles.blast_water`, `miracles.blast_coast` |
| The land itself is not burnt or blackened by explosions or fire | done | Nothing in our tree marks the land; the mound is the only mark |

## Shake and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion within 200 m of the camera shakes it up and down (the eye and what it looks at), fading over 0.7 s | done | The effect file's `DoCameraShake` in `src/Particles/Rules/Sound.cpp` starts an up-and-down shake (`ExplosionSystem::AddShake`, `camera_shake::StartEffectShake`); tests `CameraShake.AnEffectsShakeCutsItsMillisecondsAndMayBeUpAndDownOnly`, `CameraShake.OnlyWithinTheRadiusAndLessAndLess`; scenario `miracles.blast` |
| Only the nearest shake counts; between two at the same distance the newest wins | done | `camera_shake::Adjust` (`src/Camera/CameraShake.cpp`: the nearest to the last drawn camera, the newest at a tie); test `CameraShake.TheNearestDecides` |
| Shaking is always on; there is no option to turn it off | done | No option in our tree (`camera_shake::Adjust`) |
| Each explosion has a whoosh as it comes down and one of two bangs as it hits, at a random pitch | done | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`; tests in `test/audio/test_spell_sounds.cpp`, scenario `miracles.blast` |
| A new effect's first step is a double one, so the bang lands on the same turn as in the game | partial 10% | Every effect's first step is double in the original ([particles.md](../../../physics/particles.md), [game-clock.md](../../../engine/game-clock.md)). Missing: ours does it only for the town centre's effect (`src/Particles/TownBelief.cpp`), not for the explosion's |
| Scripts can shake the camera at a point, with a radius, strength and time | done | `ShakeCamera` in `src/CHLApi.cpp` (SHAKE_CAMERA) to `camera_shake::StartCameraShake`, applied to the drawn camera (`script_camera::ApplyShake` from `src/Game.cpp`); test `CameraShake.CreateAndTick` |
| A reward chest reaching the ground makes a thump and a short shake | done | the thump (sample 174, 2D) and a 0.4 s shake of strength 1 within 200 m of the world's origin (the reward system, `src/ECS/Systems/Implementations/RewardSystem.cpp`), checked against the v1.42 exe (0x6E6416, `LH3DCameraChecker::Create` at 0x6E6453); scenario `rewards.from_sky`; see [../story/rewards.md](../story/rewards.md) |

## Other bangs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play special effects such as the "bang" smoke burst and the temple explosion at a place or on an object | done | `SpecialEffectPosition` and `SpecialEffectObject` in `src/CHLApi.cpp` to `psys::manager::CreateSpotVisual` (`src/Particles/PSysManager.cpp`) |
| Fireworks go up and burst with their own sounds, started by scripts and when a player takes a town | done | Scripts start the fireworks spot visuals (`src/Particles/PSysManager.cpp`); a town won by a player starts them at its centre (`src/ECS/Town/TownBelief.cpp`); scenario `objects.town_won` |
| A temple whose heart is destroyed goes through a destruction sequence with explosions and plasma | pending | Nothing in our tree. See [../temple/](../temple/) and [../multiplayer/](../multiplayer/) |
| A failed miracle cast gives a small puff | done | `FailApply` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the fail spot visual and sound). See [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| The creature smashes rocks in half by hand, with its own rock-tap sounds | done | `object_physics::SmashRock` (`src/ECS/ObjectPhysics.cpp`, samples 139..142 in turn) from `CreatureObjectActionSystem.cpp`, and the idle mind's shatter step (`CreatureMindSystem.cpp`), as `Creature3D.cpp` in bw1-decomp; test `CreatureKindActions.ShatteringRepairingAndUsingUpAreDoneAtOnce` |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Explosions in progress (wave front, waiting pieces) are saved and loaded with the game | pending | openblack has no saved games |
