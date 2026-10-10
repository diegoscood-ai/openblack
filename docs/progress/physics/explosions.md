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

**Progress: 31/43 done, 6 partial — 79%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## The burst

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion acts at the land under it (sea level over water); it starts acting after its delay and keeps acting for its set time | done | `src/Particles/Rules/Explosion.cpp` (the centre at the land's altitude, events from its initial delay for its time); test `Explosion.blastEventsCadenceAndCloseDown` (`test/test_explosion.cpp`) |
| Every turn while it acts it applies its burst: heat (which sets things alight), crushing and a blow, over a fixed radius | done | `src/Particles/Rules/Explosion.cpp` (one spell event a step: the spell's burn, crush and hit in its effect radius); heat feeds [fire.md](fire.md) |
| The burst is the same everywhere inside its radius: there is no falloff with distance | done | `EffectValues::ApplyEffectToMapPos` (`src/ECS/Effects/EffectValues.cpp`): every receiver in reach takes the whole effect |
| The burst's strength scales with what the caster paid, the tribe's power and the event's own strength; the radius does not | partial | `src/Magic/Core/Chants.cpp` and the effect values; the tribal power is never set in our tree, so it is always 1 |
| A burst hurts through each thing's defences, moves the caster's alignment and angers the towns it harms | done | `ecs::effects::ApplyEffect` (the info's defence multipliers, the alignment update, the town's emergency), `src/ECS/Effects/EffectValues.cpp` |
| Villagers near an explosion react once for the whole explosion, not once per turn | done | `src/Magic/Core/SpellEvent.cpp` (the reaction is made on the first event only) |
| A shield over the explosion is struck where the way down enters it, and can stop it | done | `src/Particles/Rules/Explosion.cpp` (the ray from 200 m above meets the shield: a spark and an event to the shield; a held shield stops the blast) |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave front runs outward at a set speed and reaches each object when it touches the object's edge | done | `src/Particles/Rules/Explosion.cpp` (the ring grows at its spread speed and reaches an object by its radius) |
| The wave's reach grows with the caster tribe's power (1 to 5 times) | partial | The reach times the tribal power (1 to 5) is in `src/Particles/Rules/Explosion.cpp`, but the tribal power is always 1 in our tree |
| The wave takes only things fixed on the map, at most one a turn, and only those it may destroy | done | `src/Particles/Rules/Explosion.cpp` (one object a step, the first that may be destroyed). Our wiki differs: the targets are the mobile and the fixed objects of each cell, not only the fixed ones ([miracles.md](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| Creatures, fields, temples, teleport stones, totems, totem statues, worship totems, pots and one-off seeds are never destroyed by it | partial | `explosion::CanBeDestroyedBySpell` says no for creatures, fields, the temple and its parts, worship sites and totems; teleport stones, totem statues, pots and one-off seeds are not excluded |
| Things a script made indestructible, or that the help system holds, are spared | todo | `explosion::CanBeDestroyedBySpell` does not test `Indestructible` or the objects a script or the help holds |
| A building reached is shattered into pieces but left standing as an empty shell with no life; this includes spell dispensers | done | `explosion::DestroyedByBeam` takes all its life through `ecs::abodes::ReduceLife` (dispensers too). Our wiki differs: a building with a town is then drawn as a building site at 0 percent until repaired ([miracles.md](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| Anything else reached (trees, statues, small objects) shatters and is removed | done | `explosion::DestroyedByBeam` (trees through `DeleteTree`, the rest destroyed as by an effect) after its pieces go to `src/Particles/Rules/ExplodeObject.cpp` |
| Villagers are never shattered; they take only the burst's heat and crushing | partial | `explosion::DestroyedByBeam` deletes a villager the ring reaches, with no death. Our wiki differs: villagers the ring reaches are destroyed by the beam like other objects ([miracles.md](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| A shield in the wave's way is struck at the point it is reached, with a spark and the wave's direction | done | `src/Particles/Rules/Explosion.cpp` (a target inside a shield: a spark and an event to the shield) |
| At most a set number of things shatter and a set number are removed per explosion | done | `src/Particles/Rules/Explosion.cpp` (the effect file's most to explode and most to delete) |

## Shattering into pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A shattered model breaks into strips of up to 16 triangles that touch along a shared edge, even across texture seams | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshWalksSharedEdges` |
| Each piece flies away from a point 5 m below the centre at the blast's speed, with some randomness | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshPieceVelocity` |
| Pieces fall, bounce off the land, tumble, fade over 3 s, shrink from 1 to 5 s and are gone at 6 s | done | The effect file's rules run by `src/Particles/PSys.cpp` |
| Pieces make no sound and no ripples when they land, even on water | done | The effect file has none |
| Pieces are lit by the land and by their model's own light | done | `mesh_pieces::Build` in `src/Particles/Rules/ExplodeObject.cpp`; tests `Explosion.explodedPieceLitColour`, `Explosion.meshPiecesBuildWorldAndLight` |
| Objects waiting to be shattered are handled newest first | done | `src/Particles/Rules/ExplodeObject.cpp` (the queue is emptied from its last entry); test `Explosion.explodeObjectQueueToPieces` |
| Five rock pieces are thrown out from the centre's height | done | `src/Particles/Rules/Explosion.cpp` (five rock meshes queued to break) |

## Marks, dust, smoke and water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land counts as dry when it is more than 3 above sea level; the same test picks every dry or wet effect below | done | `pot_resource::IsDryLand` (`src/ECS/PotResource.cpp`), used by `src/Particles/Rules/Explosion.cpp` |
| On dry land a heap of rubble lies at the centre, at a random angle, following the land's shape, for 15 s, fading out over its last second | done | `src/ECS/GroundMarks.cpp` (the mound at a random angle, moulded to the land, 15 s, fading over its last second) |
| A brown dust puff of 15 sprites flies up and out, growing and fading over 1.5 s | done | The ground mark's dust (`DisappearSmoke` mode 1); test `Explosion.groundMarkDustIsDisappearSmokeMode1` |
| Grey smoke rises a little after the bang for a few seconds | done | `src/Particles/Rules/Explosion.cpp` (the smoke spot visual after the smoke delay, magnitude 8 for 4 s) |
| On water there is no rubble or dust: three rings spread and white steam rises instead | done | `src/Particles/Rules/Explosion.cpp` (steam), `src/Particles/PSysWaterRings.cpp` (three rings) |
| The land itself is not burnt or blackened by explosions or fire | done | Nothing in our tree marks the land; the mound is the only mark |

## Shake and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An explosion within 200 m of the camera shakes it up and down (the eye and what it looks at), fading over 0.7 s | todo | Effects do not start a camera shake: `src/Particles/Rules/Sound.cpp` reads the shake flag but does nothing with it |
| Only the nearest shake counts; between two at the same distance the newest wins | done | `camera_shake::Adjust` (`src/Camera/CameraShake.cpp`: the nearest to the last drawn camera, the newest at a tie); used by script shakes only |
| Shaking is always on; there is no option to turn it off | done | No option in our tree (`camera_shake::Adjust`) |
| Each explosion has a whoosh as it comes down and one of two bangs as it hits, at a random pitch | done | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp` |
| A new effect starts with a double step, so the bang lands on the same turn as in the game | partial | Not found in our tree: a spell's effect is stepped once a turn (`src/Magic/Core/Spell.cpp`); our wiki does not describe a double first step |
| Scripts can shake the camera at a point, with a radius, strength and time | done | `ShakeCamera` in `src/CHLApi.cpp` (SHAKE_CAMERA) to `camera_shake::StartCameraShake`, applied to the drawn camera (`script_camera::ApplyShake` from `src/Game.cpp`) |
| A reward chest reaching the ground makes a thump and a short shake | todo | No reward chests: `CreateReward` in `src/CHLApi.cpp` is a stub |

## Other bangs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts can play special effects such as the "bang" smoke burst and the temple explosion at a place or on an object | done | `SpecialEffectPosition` and `SpecialEffectObject` in `src/CHLApi.cpp` to `psys::manager::CreateSpotVisual` (`src/Particles/PSysManager.cpp`) |
| Fireworks go up and burst with their own sounds, started by scripts and when a player takes a town | partial | Scripts can start the fireworks spot visuals (`src/Particles/PSysManager.cpp`); a town changing hands starts none |
| A temple whose heart is destroyed goes through a destruction sequence with explosions and plasma | todo | Nothing in our tree. See [../temple/](../temple/) and [../multiplayer/](../multiplayer/) |
| A failed miracle cast gives a small puff | done | `FailApply` in `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the fail spot visual and sound). See [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| The creature smashes rocks in half by hand | todo | Only the hand taps rocks (`Rocks::Tap` from `HandSystem.cpp`); the creature's actions do not split rocks |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Explosions in progress (wave front, waiting pieces) are saved and loaded with the game | todo | openblack has no saved games |
