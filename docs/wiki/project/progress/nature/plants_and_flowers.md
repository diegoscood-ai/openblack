# Plants and flowers

The small growing things on the land that aren't trees: flower patches, bushes, hedges and crops' look on the land.

**Progress: 7/7 done, 0 chosen, 0 partial, 0 inferred — 100%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place flower patches of each kind, turned and sized | done | `FeatureScriptCommands::CreateFlowers` and `FlowersArchetype` (`src/ECS/Archetypes/FlowersArchetype.cpp`); the shipped lands place none; scene `sky.flowers_on_bank`; test `LandScriptMeshCommandsTest.CreateFlowersTakesItsAngleAndScaleAsTheyAre` (`test/test_land_script_mesh_commands.cpp`) |
| Flowers are drawn unlit as the game draws them | done | Scene `sky.flowers_on_bank`; `components::Unlit` and `MorphWithTerrain` from `FlowersArchetype` (the unlit flag, test `ArgbColour.UnlitIsAFlagBesideTheTreeTint`); no shadow cast on the land nor received (`RenderingSystem.cpp`, `src/ECS/ShadowReceiver.h`, test `ShadowReceiver.TheClassesThatTurnTheFlagOffDoNotReceive`) |
| Flowers catch fire and burn | done | A patch takes its flowers info row's fire values as a feature does (`fire::traits::InfoOf`; info.dat: combustion 100, heat capacity 50 for both patches); test `test/test_fire.cpp`, `FireTraitRows.APatchOfFlowersBurnsByItsFlowersRow`; no scene, not measured in a game |
| Bushes, copses and hedges are kinds of tree, growing and swaying as trees do | done | Tree types of info.dat (`src/ECS/Trees.cpp`), made as trees of their own rows and grown by them turn by turn as any tree (test `PlantsWorld.BushesCopsesAndHedgesAreTreesThatGrowAsTreesDo`, game data and synthetic); see [trees](trees.md) |
| Fields' crops sway once ripe, further than trees | done | `VegetationSystem::GetFieldMatrix` (1.75 times a tree's lean); test `VegetationSystem.AFieldLeansFurtherThanATree` |
| A field's crop shows green while young and turns its own colour as it ripens | done | `ecs::FieldDrawColour` (`src/ECS/Fields.cpp`); test `test/test_field_system.cpp` |
| Plants count as their own kind when a miracle's alignment is worked out | done | `AlignmentType::Plant` (`src/ECS/Effects/Alignment.cpp`; every tree row of info.dat, bushes, copses and hedges too, is a plant); test `test/test_alignment.cpp`, `EffectAlignment.APlantTakesThePlantColumn`; a patch of flowers takes its own flowers row's kind, as the original (`alignment::TargetOf`; test `EffectAlignment.AFeatureAndAPatchOfFlowersTakeTheirOwnRowsKind`, `test/test_alignment_targets.cpp`) |
