# Plants and flowers

The small growing things on the land that aren't trees: flower patches, bushes, hedges and crops' look on the land.

**Progress: 4/8 done, 0 chosen, 3 partial, 1 inferred — 84%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place flower patches of each kind, turned and sized | done | `FeatureScriptCommands::CreateFlowers` and `FlowersArchetype` (`src/ECS/Archetypes/FlowersArchetype.cpp`); the shipped lands place none; scene `sky.flowers_on_bank` |
| Flowers are drawn unlit as the game draws them | done | Scene `sky.flowers_on_bank`; `components::Unlit` and `MorphWithTerrain` from `FlowersArchetype`; no shadow cast on the land nor received (`RenderingSystem.cpp`, `ECS/ShadowReceiver.h`) |
| Flowers catch fire and burn | partial 90% | No test or scene; A patch takes its flowers info row's fire values as a feature does (`fire::traits::InfoOf`); not measured in a game |
| Bushes, copses and hedges are kinds of tree, growing and swaying as trees do | partial 90% | No test; Tree types of info.dat (`src/ECS/Trees.cpp`); see [trees](trees.md) |
| Fields' crops sway once ripe, further than trees | done | `VegetationSystem::GetFieldMatrix` (1.75 times a tree's lean); test `VegetationSystem.AFieldLeansFurtherThanATree` |
| A field's crop shows green while young and turns its own colour as it ripens | done | `ecs::FieldDrawColour` (`src/ECS/Fields.cpp`); test `test/test_field_system.cpp` |
| Plants count as their own kind when a miracle's alignment is worked out | partial 90% | No test; `AlignmentType::Plant` (`src/ECS/Effects/Alignment.cpp`) |
| The creature eats plants and stomps on flowers | inferred | A guess, not found in the original; see ../creature/ |
