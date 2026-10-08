# Blast

A beam of light drops from the sky onto the hand's point and explodes: a shock wave shatters trees, people and small
things nearby into flying pieces, leaves buildings standing as empty shells, and scars the land. The first power-up adds
six more blasts around it, the second a long barrage. It is a real single-player miracle: Land 5's script gives it (and
its power-ups) to towns 2, 3 and 5, and the skirmish maps hand it out through dispensers, one-off seeds and towns; the
firefly rewards of Lands 1–4 never offer it, though Land 5's firefly table does (weight 0.1, about 0.6%;
see [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table)). (Players know it as the Megablast; that name is unconfirmed in the data.)

**Progress: 33/42 done, 6 partial — 86%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blast is cast the moment the gesture is pressed, at the hand's point within the player's influence | done | `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the hand-position cast type), `src/Magic/CastRules.cpp` |
| The held seed shows no effect on the hand, only the seed itself | done | The info tables (`src/Magic/MagicTables.cpp`: no in-hand effect for the seed) |
| It costs 16000, 32000 or 60000 to cast, 10 per event and 3000 or 10000 when it strikes a shield | done | `src/Magic/Core/Chants.cpp`, `src/Magic/MagicTables.cpp`. See [prayer_cost.md](prayer_cost.md) |
| The second power-up's long barrage keeps drawing prayer power as it goes, and only weakens when the caster can't pay | done | `src/Magic/Core/Chants.cpp` (each event paid; the strength falls only when the caster can't pay) |
| A script can cast it at a point | done | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS) |
| A creature casts the plain blast: sometimes shows it first, goes near, backs off, faces the target and gestures | todo | Creatures cast no world miracles in our tree. See [creature_spells.md](creature_spells.md) |
| A blast under way is kept in a saved game | todo | openblack has no save games |

## The beam and the explosion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The beam drops 120 m from the sky in 0.4 s onto the point | done | `src/Particles/Rules/Explosion.cpp` (UR_MoveAtom); test `Explosion.moveAtom` (`test/test_explosion.cpp`) |
| A white flash marks the landing point for a second | done | `src/Particles/Rules/Explosion.cpp` (the beam's spot visual at the centre) |
| A shield over the point is struck where the beam enters it, and sparks | done | `src/Particles/Rules/Explosion.cpp` (the ray from 200 m above meets the shield, a spark and an event to the shield; a held shield stops the blast). See [physical_shield.md](physical_shield.md) |
| For its time the blast burns and crushes around the point: heat 200/400/800 and radius 5/5/10 by power-up | done | `src/Particles/Rules/Explosion.cpp` (one event a step from its initial delay for its time); test `Explosion.blastEventsCadenceAndCloseDown` |
| Its strength and the shock wave's reach grow with the caster's tribal power, up to five times | partial | The formula is in `src/Particles/Rules/Explosion.cpp` (the reach times tribal power, 1 to 5) and `src/Magic/Core/Chants.cpp`, but `PlayerMagic::tribalPower` is never set in our tree, so it is always 1 |
| On dry land (higher than sea level by more than 3) it leaves smoke and a rubble scar that follows the land and lasts 15 s | done | `src/Particles/Rules/Explosion.cpp` (the smoke spot visual), `src/ECS/GroundMarks.cpp` (the mound moulded to the land, 15 s) |
| On water it leaves steam and three rings spreading to 5, 7 and 10 m over 0.7 s | done | `src/Particles/Rules/Explosion.cpp` (the steam), `src/Particles/PSysWaterRings.cpp` (three rings) |
| Old rings drift with the wind the way a reused ring slot did in the game | partial | Not reproduced: the ring pool in `src/ECS/WaterRings.h` is a vector and does not keep a reused slot's drift |
| Five rocks are thrown out from the centre | done | `src/Particles/Rules/Explosion.cpp` (five rock meshes queued to break), `src/Particles/Rules/ExplodeObject.cpp` |
| A brown dust puff of fifteen sprites rises and fades over 1.5 s | done | `src/ECS/GroundMarks.cpp` (the mark's dust, `DisappearSmoke` mode 1); test `Explosion.groundMarkDustIsDisappearSmokeMode1` |
| Three bright cones stand over the point, keeping their height as they widen, and light the land white | done | `src/Particles/Rules/Explosion.cpp` (UR_ChangeScaleXYZ); test `Explosion.changeScaleXYZ` |
| Every new effect steps twice on its first turn, so the bang comes as soon after the cast as in the game | partial | Not found in our tree: a spell's effect is stepped once a turn by its spell (`src/Magic/Core/Spell.cpp`); our wiki does not describe a double first step |
| Emitters make at most one particle a step unless their data allows more | done | `src/Particles/PSys.cpp` (ShouldEmit; the spreading disc emitter makes several a step, as the original) |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave spreads out from the point and reaches each object by its edge | done | `src/Particles/Rules/Explosion.cpp` (the ring grows at SpreadSpeed and reaches an object by its radius) |
| Up to 15 things are shattered into flying pieces and up to 15 removed | done | `src/Particles/Rules/Explosion.cpp` (MaxObjectsToExplode, MaxObjectsToDelete) |
| Creatures are never shattered or removed | done | `src/Particles/Rules/Explosion.cpp` (`CanBeDestroyedBySpell` says no for creatures, fields and the citadel) |
| Trees, statues and small things are shattered and gone | done | `src/Particles/Rules/Explosion.cpp` (pieces, then `DestroyedByBeam` deletes them), `src/Particles/Rules/ExplodeObject.cpp` |
| Buildings and spell dispensers are shattered but left standing with no life | done | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` to `src/ECS/Abodes.cpp` ReduceLife: all its life, and a building site when it has a town). Our wiki differs: a building with a town is then drawn at 0 % and disappears from view as a building site until repaired; only without a town is it drawn whole ([page](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| Villagers are not shattered: the blast's heat sets those nearest alight and they burn to death | partial | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` deletes a villager the ring reaches, with no death: approximate). Our wiki differs: villagers the ring reaches are destroyed by the beam like other objects, not left to burn ([page](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| Animals in the wave fall dead | done | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` removes the animal). Our wiki differs: an animal the ring reaches is removed by the beam, it does not fall dead ([page](../../bw1-notes/miracles.md#ur_explosion-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900)) |
| Teleport stones and totems are spared; pots and one-off seeds are never touched | partial | `src/Particles/Rules/Explosion.cpp`: the citadel's parts, worship sites and totems say no; a totem statue loses its life like a building; teleport stones, pots and one-off seeds go through `IsEffectReceiver`, not checked. The script indestructible flag and script-held objects are not tracked |
| Where the wave meets a shield it sparks it and pushes on it outward from the centre | done | `src/Particles/Rules/Explosion.cpp` (a target inside a shield: a spark and an event to the shield) |
| People nearby react to the miracle once | done | `src/Magic/Core/SpellEvent.cpp` (the reaction on the first event) |

## Flying pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A model breaks into chains of up to sixteen triangles joined by shared edges | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshWalksSharedEdges` |
| Pieces fly out, fall, bounce, tumble, fade over 3 s, shrink over 1–5 s and are gone at 6 s | done | The effect file's rules; test `Explosion.explodeMeshPieceVelocity` |
| Pieces are lit by their model's light | done | `src/Particles/Rules/ExplodeObject.cpp` (`mesh_pieces::Build`); tests `Explosion.explodedPieceLitColour`, `Explosion.meshPiecesBuildWorldAndLight` |
| The last thing to break is the first to fly | done | `src/Particles/Rules/ExplodeObject.cpp` (the queue is emptied from its last entry) |

## Power-ups

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first power-up adds six blasts 30–50 m around the first | done | The effect files, `src/Particles/PSys.cpp` (spreading disc emitter) |
| The second adds six at 25–40 m, then a barrage every few tenths of a second at 20–60 m | done | The effect files, `src/Particles/PSys.cpp` (spreading disc emitter) |
| Beams still falling when the barrage ends still bang | done | The effect files |

## Sound and shake

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The falling beam whooshes, one of four samples at a random pitch | done | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp` |
| It bangs 0.35 s later, one of two samples at a random pitch, at the point at sea level | done | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp` |
| The camera shakes for 0.7 s if within 200 m; the nearest blast counts | todo | `src/Particles/Rules/Sound.cpp` reads DoCameraShake but does not shake; the script camera shake (`src/Camera/CameraShake.cpp`) is not used by effects |
| Every power-up blast has its own whoosh, bang and shake | partial | Each blast has its whoosh and bang (`src/Particles/Rules/Sound.cpp`); no shake |
| Pieces, smoke, scars and rings make no sound | done | The particle data |
