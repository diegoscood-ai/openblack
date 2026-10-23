# Blast

A beam of light drops from the sky onto the hand's point and explodes: a shock wave shatters trees, people and small
things nearby into flying pieces, leaves buildings standing as empty shells, and scars the land. The first power-up adds
six more blasts around it, the second a long barrage. It is a real single-player miracle: Land 5's script gives it (and
its power-ups) to towns 2, 3 and 5, and the skirmish maps hand it out through dispensers, one-off seeds and towns; the
firefly rewards of Lands 1–4 never offer it, though Land 5's firefly table does (weight 0.1, about 0.6%;
see [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table)). (Players know it as the Megablast; that name is unconfirmed in the data.)

**Progress: 21/42 done, 0 chosen, 18 partial, 0 inferred — 83%**

How the original does it, in our wiki: [Lightning explosion and exploding objects](../../../magic/lightning-explosion.md).

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blast is cast the moment the gesture is pressed, at the hand's point within the player's influence | done | `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the hand-position cast type), `src/Magic/CastRules.cpp`. Scenario `miracles.blast` |
| The held seed shows no effect on the hand, only the seed itself | partial 80% | The info tables (`src/Magic/MagicTables.cpp`: no in-hand effect for the seed) No test or check covers it |
| It costs 16000, 32000 or 60000 to cast, 10 per event and 3000 or 10000 when it strikes a shield | done | `src/Magic/Core/Chants.cpp`, `src/Magic/MagicTables.cpp`. See [prayer_cost.md](prayer_cost.md) Test `MagicTables.realInfoDat` (with the game's data) |
| The second power-up's long barrage keeps drawing prayer power as it goes, and only weakens when the caster can't pay | done | `src/Magic/Core/Chants.cpp` (each event paid; the strength falls only when the caster can't pay) Scenario `miracles.blast_pu2` |
| A script can cast it at a point | partial 80% | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS) No test or check covers the script cast |
| A creature casts the plain blast: sometimes shows it first, goes near, backs off, faces the target and gestures | partial 70% | `Build::CastExplosion` with `creature_mind::CastAt`; scenario `miracles.creature_casts_explosion`; imported, not yet checked against the original; see [../creature/creature_casting.md](../creature/creature_casting.md) and issue #172 |
| A blast under way is kept in a saved game | pending | openblack has no save games |

## The beam and the explosion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The beam drops 120 m from the sky in 0.4 s onto the point | done | `src/Particles/Rules/Explosion.cpp` (UR_MoveAtom); test `Explosion.moveAtom` (`test/test_explosion.cpp`) |
| A white flash marks the landing point for a second | done | `src/Particles/Rules/Explosion.cpp` (the beam's spot visual at the centre) Scenario `miracles.blast` |
| A shield over the point is struck where the beam enters it, and sparks | partial 80% | `src/Particles/Rules/Explosion.cpp` (the ray from 200 m above meets the shield, a spark and an event to the shield; a held shield stops the blast). See [physical_shield.md](physical_shield.md) No test or check covers the beam's shield hit |
| For its time the blast burns and crushes around the point: heat 200/400/800 and radius 5/5/10 by power-up | done | `src/Particles/Rules/Explosion.cpp` (one event a step from its initial delay for its time); test `Explosion.blastEventsCadenceAndCloseDown` |
| Its strength and the shock wave's reach grow with the caster's tribal power, up to five times | partial 90% | The formula is in `src/Particles/Rules/Explosion.cpp` (the reach times tribal power, 1 to 5) and `src/Magic/Core/Chants.cpp`, and `PlayerMagic::tribalPower` is 1 for every tribe until a wonder of that tribe is built for the caster's player, and rises by each wonder's power (`wonders::AddToPlayer`, copied each turn by `magic::players::ProcessTribalPowers`, as `Wonder::AddToPlayer` 0x778FC0 and `GPlayer::Process`); not yet seen in a run with a built wonder. See [prayer_cost.md](prayer_cost.md) |
| On dry land (higher than sea level by more than 3) it leaves smoke and a rubble scar that follows the land and lasts 15 s | partial 90% | `src/Particles/Rules/Explosion.cpp` (the smoke spot visual), `src/ECS/GroundMarks.cpp` (the mound moulded to the land, 15 s) Scenarios `miracles.blast`, `miracles.blast_coast`. The land's light on the scar is the normal mesh lighting, not the original's own step ([wiki](../../../magic/lightning-explosion.md#openblack)) |
| On water it leaves steam and three rings spreading to 5, 7 and 10 m over 0.7 s | done | `src/Particles/Rules/Explosion.cpp` (the steam), `src/Particles/PSysWaterRings.cpp` (three rings) Scenario `miracles.blast_water` |
| Old rings drift with the wind the way a reused ring slot did in the game | pending | Not reproduced: the ring pool in `src/ECS/WaterRings.h` is a vector and does not keep a reused slot's drift; one field of the ring record is still unidentified ([wiki Pending](../../../magic/lightning-explosion.md#pending)) |
| Five rocks are thrown out from the centre | done | `src/Particles/Rules/Explosion.cpp` (five rock meshes queued to break), `src/Particles/Rules/ExplodeObject.cpp`. Test `Explosion.explodeObjectQueueToPieces`; scenario `miracles.blast` |
| A brown dust puff of fifteen sprites rises and fades over 1.5 s | done | `src/ECS/GroundMarks.cpp` (the mark's dust, `DisappearSmoke` mode 1); test `Explosion.groundMarkDustIsDisappearSmokeMode1` |
| Three bright cones stand over the point, keeping their height as they widen, and light the land white | done | `src/Particles/Rules/Explosion.cpp` (UR_ChangeScaleXYZ); test `Explosion.changeScaleXYZ` |
| Every new effect steps twice on its first turn, so the bang comes as soon after the cast as in the game | pending | The original steps every new effect twice on its first turn ([particle rules](../../../physics/particle-rules.md)); our tree steps a spell's effect once a turn (`src/Magic/Core/Spell.cpp`) |
| Emitters make at most one particle a step unless their data allows more | partial 80% | `src/Particles/PSys.cpp` (ShouldEmit; the spreading disc emitter makes several a step, as the original) No test covers the emit count |

## The shock wave

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wave spreads out from the point and reaches each object by its edge | done | `src/Particles/Rules/Explosion.cpp` (the ring grows at SpreadSpeed and reaches an object by its radius) Scenario `miracles.blast_spares` (traced) |
| Up to 15 things are shattered into flying pieces and up to 15 removed | partial 80% | `src/Particles/Rules/Explosion.cpp` (MaxObjectsToExplode, MaxObjectsToDelete) No test covers the caps |
| Creatures are never shattered or removed | done | `src/Particles/Rules/Explosion.cpp` (`CanBeDestroyedBySpell` says no for creatures, fields and the citadel) Scenario `miracles.blast_spares` |
| Trees, statues and small things are shattered and gone | done | `src/Particles/Rules/Explosion.cpp` (pieces, then `DestroyedByBeam` deletes them), `src/Particles/Rules/ExplodeObject.cpp`. Scenario `miracles.blast` |
| Buildings are shattered and lose all their life; a building with a town becomes its town's building site until repaired | partial 60% | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` to `src/ECS/Abodes.cpp` ReduceLife: all its life); a building with a town and no site becomes a building site (`ReduceLife`, `src/ECS/Abodes.cpp`); missing: the site's repair; a totem statue and a dispenser without an abode take the general path ([page](../../../magic/lightning-explosion.md#openblack); issue #59) |
| Villagers the wave reaches are destroyed by the beam like other objects, through their own removal | partial 60% | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` kills a villager the ring reaches through the general kill, not the villager's own removal: approximate) ([page](../../../magic/lightning-explosion.md#openblack); issue #59) |
| An animal the wave reaches is removed by the beam | done | `src/Particles/Rules/Explosion.cpp` (`DestroyedByBeam` removes the animal through `animal_ai::Remove`), as the original ([page](../../../magic/lightning-explosion.md#openblack)); scenario `miracles.blast` |
| Teleport stones, totems, spell seeds, one-off seeds and empty pots are spared; a pot or a pile holding something is shattered and removed | partial 90% | `explosion::CanBeDestroyedBySpell` (`src/Particles/Rules/Explosion.cpp`): the creatures, fields, the citadel's parts, worship sites and totems say no; the rest answer through `effects::IsEffectReceiver` (`src/ECS/Effects/EffectValues.cpp`), where a teleport stone, a spell seed, a one-off seed, a totem building and the village totem take no effect and a pot or pile only while it holds something, as each class's own answer in the exe; test `RepairsTest.NoEffectReachesATeleportStoneASeedATotemOrAnEmptyPot`; scene `miracles.blast_spares`. Missing: the script indestructible flag and script-held objects are not tracked ([page](../../../magic/lightning-explosion.md#ur_explosion)) |
| Where the wave meets a shield it sparks it and pushes on it outward from the centre | partial 80% | `src/Particles/Rules/Explosion.cpp` (a target inside a shield: a spark and an event to the shield) No test or check covers it |
| People nearby react to the miracle once | done | `src/Magic/Core/SpellEvent.cpp` (the reaction on the first event) Scenario `miracles.reactions_flee` |

## Flying pieces

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A model breaks into chains of up to sixteen triangles joined by shared edges | done | `src/Particles/Rules/ExplodeObject.cpp`; test `Explosion.explodeMeshWalksSharedEdges` |
| Pieces fly out, fall, bounce, tumble, fade over 3 s, shrink over 1–5 s and are gone at 6 s | done | The effect file's rules; test `Explosion.explodeMeshPieceVelocity`; scenario `miracles.blast` |
| Pieces are lit by their model's light | done | `src/Particles/Rules/ExplodeObject.cpp` (`mesh_pieces::Build`); tests `Explosion.explodedPieceLitColour`, `Explosion.meshPiecesBuildWorldAndLight` |
| The last thing to break is the first to fly | done | `src/Particles/Rules/ExplodeObject.cpp` (the queue is emptied from its last entry) Test `Explosion.explodeObjectQueueToPieces` |

## Power-ups

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first power-up adds six blasts 30–50 m around the first | done | The effect files, `src/Particles/PSys.cpp` (spreading disc emitter) Scenario `miracles.blast_pu1` |
| The second adds six at 25–40 m, then a barrage every few tenths of a second at 20–60 m | done | The effect files, `src/Particles/PSys.cpp` (spreading disc emitter) Scenario `miracles.blast_pu2` |
| Beams still falling when the barrage ends still bang | partial 80% | The effect files; no test or check covers the end of the barrage |

## Sound and shake

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The falling beam whooshes, one of four samples at a random pitch | partial 80% | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`. No test covers the choice; heard in the blast scenarios |
| It bangs 0.35 s later, one of two samples at a random pitch, at the point at sea level | partial 80% | `src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`. No test covers the delay |
| The camera shakes for 0.7 s if within 200 m; the nearest blast counts | partial 70% | `src/Particles/Rules/Sound.cpp` reads DoCameraShake and shakes the camera through the explosion system's `AddShake` within the radius for the duration; which blast counts when several shake is not checked, and no test covers it |
| Every power-up blast has its own whoosh, bang and shake | partial 70% | Each blast has its whoosh, bang and shake (`src/Particles/Rules/Sound.cpp`); no test covers them, and how several shakes add up is not checked |
| Pieces, smoke, scars and rings make no sound | partial 80% | The particle data has no sound for them; not checked against the original's files one by one |
