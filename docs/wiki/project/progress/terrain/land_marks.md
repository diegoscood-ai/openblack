# Footprints and marks on the land

What is printed onto the land: the paved ground of buildings, river beds, the creature's footprints, and the rubble
and scars left by blasts.

**Progress: 15/16 done, 0 chosen, 1 partial, 0 inferred — 99%**

## Building footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A building or feature whose model carries a footprint prints its texture onto the land under it | done | Baked into the block's texture at its prints step, as the original (`block_texture::PrintFootprint` from `ecs::land_prints::SourcesFor`, `LandIsland::BakeBlock`; #222); tests `BlockPrints.FootprintsPrintAsTheOriginal`, `BlockBake.AShadowCasterPrintsItsMeshsFootprintToo`, `BlockBake.AnObjectThatNeitherCastsNorPrintsIsLeftOut` |
| Footprints are blended into the land's colour, not drawn on top as decals | done | Each nibble moved towards the picture's by its alpha in the block's texels (`block_texture::PrintFootprint`, pinned to the original run natively); tests `BlockPrints.FootprintsPrintAsTheOriginal`, `BlockPrints.AColourPrintKeepsTheLandsAlphaAndNeverCoversItWholly` |
| The footprint goes when the building is removed and appears when it is placed | partial 90% | The prints are baked into a block's texture at its request's prints step in the block texture queue, at a block's first draw and as the land opens (`LandIsland::BakeBlock`, [terrain.md](../../../world/terrain.md#the-block-texture-queue-in-openblack)), so a print goes, or shows, when the block is painted again; debug: the Block textures node's "Show bake layers". The original bakes the prints into the block textures (`SetFootPrintOnTexture`, vt +0x88 of the 3D object, set at 0x52EA33 and by a field at 0x528A9B, [frame.md](../../../rendering/frame.md)) and keeps a site's print at 0 %. Read from the exe ([terrain.md](../../../world/terrain.md#the-prints-the-16th-step)): the prints go in at the 16th step of a block's request in the block texture queue (`fn_00871E50` → `fn_008721A0` → `fn_00872FA0` for each object over the block whose mesh has flag 0x8000 and footprint data, then for the list [0xEB9968]), and putting a building on the map's cells or taking it off asks for the blocks under it (`RequestChangeTexture` 0x5E2FF0 → `fn_005E3260` → `LandBlock::SetNeedUpdate` `fn_008786D0`), so its print goes, or shows, when that block's request finishes: one block a frame, the nearest first, at the highest detail. The queue is ported (land_loading.md), and so is the rule of which blocks an object marks out of date (`block_texture::BlocksMarkedBy`, test `BlockTextureMarkedBlocks.*` pinned to `fn_005E3260` run natively), called where the original calls `RequestChangeTexture`: a Fixed class put in the map's cells, a MultiMapFixed taken out, a building or a field built (`ecs::land_print_marks::MarkBlocksUnder` from `map_cells` and `abodes::Built`; the `Object` class's calls hang on a 3D object bit, `Flags2` 0x100, that nothing in the exe sets; [terrain.md](../../../world/terrain.md#the-blocks-an-object-marks)). Which objects have the footprint bit is read from the exe ([terrain.md](../../../world/terrain.md#who-asks-for-its-footprint)): only `SetFootPrintOnTexture` writes it, at the creation of a field, an animated static (the Norse gate and the Piper cave entrance have footprint meshes), a worship site, the citadel's heart and a building made not built (after its insert, which marks with its box), and nothing clears it; ours is the same rule (`land_print_marks::HasFootprintBit`, `TextureBitsOf`, `Abode::footprintOnTexture`), read by the prints step and by the marks (the footprint's corners only with the bit); tests `LandPrintMarks.*` (`test/test_land_print_marks.cpp`). Still open: the scenes with a building placed or removed are not measured against the original's frames; a lane cannot run the original's frames here, so no measurement is claimed |
| River beds are printed the same way | done | see rivers.md; tests `BlockPrints.FootprintsPrintAsTheOriginal`, `BlockPrints.AnAlphaPrintOnlyLowersTheAlpha` |
| Fields print their furrows | done | The original sets the print on every field's 3D object as it is made (`Field::CallVirtualFunctionsForCreation` 0x528A40, `SetFootPrintOnTexture(1)` at 0x528A9B, after its mesh and with no condition), and the block bake (`fn_008721A0`) prints an object's mesh footprint when the object asks for a print or a texture shadow (vt +0x8C / +0x84), and the mesh's header has the landscape feature bit 0x8000 and the feature holds at least one footprint (its first word, the count, not 0). Ours asks the same of the mesh (`L3DMesh::PrintsFootprint` → `MeshPrintsFootprint`, read by the bake, `ecs::land_prints::SourcesFor`; tests `FootprintPrint.*`, `test/graphics/test_footprint_print.cpp`); a field is an abode with its mesh (`FieldArchetype`), drawn through the instanced lists, so its footprint is printed like a building's ([shadows-and-reflections.md](../../../rendering/shadows-and-reflections.md#shadows-three-systems-in-the-original)) |

## Creature footprints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature leaves a print under its lower foot as each footstep falls | done | `FootprintSystem` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`); test `CreatureFootprints.LowerFootGetsThePrint` (`test/test_creature_footprints.cpp`) |
| Each species has its own print, sized to the creature | done | test `CreatureFootprints.SpeciesCells` |
| Left and right feet print mirrored | done | test `CreatureFootprints.LeftFootFlipsThePicture` |
| Prints fade away over about five seconds, in steps | done | tests `CreatureFootprints.GoneInAboutFiveSeconds`, `FadesInStepsOfAtLeast200Ms` |
| When the trail is full, new prints are dropped rather than old ones taken | done | test `CreatureFootprints.FullTrailDropsNewPrints` |
| On the first of April every creature leaves smiley faces | done | tests `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize`, `FootprintSystemDate.AprilFoolsComesFromTheInjectedDate` |
| Prints are not saved with the game | done | As the game (`FootprintSystemInterface.h`) |
| Puffs of dust rise from the creature's feet as it walks | done | The creature's steps throw up dust as their sounds fire, heard or not (`src/Creature/CreatureDust.cpp` from `CreatureAudioSystem.cpp`; tests `test_dust_puff_kinds`); see ../creature/ |

## Scars and rubble

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A blast leaves a heap of rubble that lies a while and fades in its last second | done | `src/ECS/GroundMarks.cpp` (from `src/Particles/Rules/Explosion.cpp`); test `BlastWorld.OnDryLandAHeapOfRubbleLiesAtTheCentre` |
| The hand gripping the land throws up a puff of dust | done | The grip effect (`HandPlacement.cpp`, `HandEffects.cpp`, SF_GripLandscape); see ../hand/; test `HandGrabRules.GrippingTheLandThrowsUpDustAndGrippingTheWaterSplashes` |
| A land vortex opens a hole in the block under it and lays a ring over the land while it shows | done | `src/Graphics/RendererVortex.cpp` (`ecs::vortex::NextDecal`); tests `test/test_vortex.cpp` ([vortex.md](../../../world/vortex.md#drawing)) |
