# Particles

The particle engine that draws the game's effects from its particle files: the groups and atoms, the rules that move
them, the sprites, ribbons, beams, models, mists and light they are drawn as, and the order they are drawn in. What each
miracle's effect looks like is in [../miracles/](../miracles/); gesture trails in [../gesture/](../gesture/).

**Progress: 24/33 done, 4 partial — 79%**

How the original does it, in our wiki: [Particles (PSys)](../../bw1-notes/particles.md), [Graphics engine parity: original versus openblack](../../bw1-notes/parity.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Effect files and the engine

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Effects are read from the game's particle files with their bitmaps | done | the file format by raffclar's `components/psys` (`ParticleFile`, `SplitCompressed`, `StackedBitmap` for the light maps, `EnumHeader` for the game's `.h` names), loaded through the resource caches by `src/Particles/PSysFile.cpp` and `src/Resources/Loaders.cpp`; tests `test/test_psys_file_loader.cpp`, `test/test_particle_file.cpp`, `test/test_particle_file_parity.cpp` (the same data as the former reader on every spell file, enum header and light map of the game) |
| An effect's groups make collections of atoms, at its start or under each new atom, and a hierarchy group moves, turns and scales with its atom | done | `src/Particles/PSys.cpp` (collections, hierarchies) |
| Every rule the files name creates, moves, shapes, colours or ends particles | partial | the classes the miracles and script effects use are registered (`src/Particles/PSysRegistry.cpp`, `src/Particles/Rules/`); still missing: the mana path, the fork flicker, emitting from the parent atom, the object-referencing creator, the animated mesh creator, and the rest listed as not ported in our wiki |
| Effects are started by type from the game's list of effects | done | `src/Particles/ParticleTypes.cpp` (raffclar's table, `particles::ParticleTypeFile`), `src/Particles/PSysManager.cpp`; tests `MagicTables.particleTypeFiles`, `ParticleFileParity.particleTypeNames` |
| Effects are cleared when a new land loads | done | `psys::manager::Clear` from `Game::LoadMap` |

## How particles look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sprites face the camera, cut from cells of a sheet | done | `Renderer::DrawParticleSprites` (`src/Graphics/RendererParticles.cpp`), the cell from the sprite flags, the frames by `frame_anim::ParticleFrameIndex` |
| Sprites lie flat on the ground, turned by their own heading | done | the horizontal sprite flag takes the origin and the heading (`src/Graphics/RendererParticles.cpp`) |
| Some sprites are tinted by the colour of the land under them | partial | the particle meshes take the land's colour (`drawWithLandscapeColour` in `src/Particles/Creators/Mesh.cpp`); sprites do not |
| Sprites are added over what is behind them or blended over it, writing depth or not, as their creator says | done | the creator's material, modes 13 or 6 (12 or 5 when it writes depth) in `src/Graphics/RendererParticles.cpp` |
| Ribbons run through chains of joints, their width turned to face the camera, meeting halfway at each joint | done | `src/Particles/Creators/Chain.cpp`, `src/Graphics/RendererChain.cpp` |
| Beams | done | the beam chains (`src/Particles/Creators/Chain.cpp`), the explosion's beam (`src/Particles/Rules/Explosion.cpp`); test `test/test_explosion.cpp` |
| Models as particles: plain, with sliding or frame-playing textures, animated, or turned to face the camera and stretched | partial | `src/Particles/Creators/Mesh.cpp` (plain, sliding and frame-playing textures, facing the camera); the animated mesh creator is not ported |
| Models blended by volume and models whose texture rotates | todo | no creator for them |
| Mists | done | `src/Particles/Creators/Mist.cpp`, `src/Graphics/RendererMists.cpp`; test `test/test_mists.cpp` |
| Light maps stamped on the land's colours under atoms, playing through frames | done | `src/Particles/Creators/LightMap.cpp` (stamped each frame into the land's cells) |
| Shadow maps darkening the land under atoms, as a cloud's shadow does | partial | the mist creator's shadow map (`src/Particles/Creators/Mist.cpp`); not yet checked against the game |
| The casting player's symbol | done | `psys::TintWithPlayerColour` (`src/Particles/PSys.cpp`) |
| Symbols of belief rising over worshippers | done | the town centre's belief symbols (`src/Particles/TownBelief.cpp`, test `test/test_town_belief.cpp`) and the belief sprite of the utility effects (`src/Particles/Utility.cpp`); see [../worship/](../worship/) |
| The mana path from a worship site | todo | its rule is missing; see [../worship/](../worship/) |
| Objects broken into flying pieces of their model | done | `src/Particles/Rules/ExplodeObject.cpp`, the pieces drawn through `src/Graphics/WorldTriangles.cpp` |
| Surfaces of revolution: the swirl under a dispenser, the teleport's pool, vortices | done | `src/Particles/Rules/SurfRevol.cpp`, `src/Graphics/RendererRevolvedSurface.cpp` |
| Sheets of light standing along a recognised gesture's trail | done | `src/Particles/Rules/Gesture.cpp` |
| Forked lightning | done | `src/Particles/Rules/Lightning.cpp`; test `test/test_lightning.cpp` |
| Glints sparkling on objects | done | the scrolls' glints effects (`SF_ScriptHighlightGlints*`) made and drawn sorted by `src/ECS/ScriptHighlight.cpp` through the particle engine |

## Drawing order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Most effects are sorted piece by piece among everything else that blends, farthest first | done | the sorted draw path in the shared transparent queue (`src/Particles/PSys.h` `DrawPath`, `src/Graphics/ZSort.cpp`); tests `test/test_psys_sorted_queue.cpp`, `test/test_psys_draw_path.cpp` |
| A few spot visuals are drawn whole at their origin, in the order their collections hold them | done | the queued and immediate draw paths (`DrawPath::Queued`, `Immediate`); test `test/test_psys_draw_path.cpp` |
| The miracle in the hand is drawn just after the hand | done | the hand's effects collected apart and drawn after the hand (`Renderer.cpp`) |
| Sprites sharing a sheet are drawn together | done | consecutive sprites with the same material go in one call (`src/Graphics/RendererParticles.cpp`) |
| Effects show in the sea's reflection too | todo | only the main view draws the effects (`Renderer.cpp`); whether the original mirrors them in the sea is not in our wiki |

## Sounds and other outputs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Particles play sounds that follow them, are heard late when far off as thunder is, and fade when let go | done | `src/Particles/Rules/Sound.cpp`, `src/Particles/SoundAction.cpp`, `src/Audio/Services/SpellSounds.cpp`; tests `SpellSounds.*`; see [../audio/](../audio/) |
| Glows lit by particles go once nothing lights them | done | the light map stamps are cleared and laid again every frame (`light_map_atoms`, `src/Particles/Creators/LightMap.cpp`) |
| The effects of picking up and putting down several food or wood items at once | todo | the effect types exist (`src/Particles/ParticleTypes.cpp`) but their emitter rule is missing; see [../hand/](../hand/) |
| Effects are saved with the game and restored on load | todo | no save; see [../engine/](../engine/) |
