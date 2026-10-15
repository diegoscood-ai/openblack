# Plants and flowers

The small growing things on the land that aren't trees: flower patches, bushes, hedges and crops' look on the land.

**Progress: 6/8 done, 1 partial — 81%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place flower patches of each kind, turned and sized | done | `FeatureScriptCommands::CreateFlowers` and `FlowersArchetype` (`src/ECS/Archetypes/FlowersArchetype.cpp`); the shipped lands place none |
| Flowers are drawn unlit as the game draws them | done | `components::Unlit` and `MorphWithTerrain` from `FlowersArchetype`; no shadow cast on the land nor received (`RenderingSystem.cpp`, `ECS/ShadowReceiver.h`) |
| Flowers catch fire and burn | partial | A patch takes its flowers info row's fire values as a feature does (`fire::traits::InfoOf`); not measured in a game |
| Bushes, copses and hedges are kinds of tree, growing and swaying as trees do | done | Tree types of info.dat (`src/ECS/Trees.cpp`); see [trees](trees.md) |
| Fields' crops sway once ripe, further than trees | done | `RenderingSystem.cpp` (a ripe field sways 1.75 times a tree's, `ecs::WindSway`) |
| A field's crop shows green while young and turns its own colour as it ripens | done | `ecs::FieldDrawColour` (`src/ECS/Fields.cpp`) |
| Plants count as their own kind when a miracle's alignment is worked out | done | `AlignmentType::Plant` (`src/ECS/Effects/Alignment.cpp`) |
| The creature eats plants and stomps on flowers (unconfirmed) | todo | see ../creature/ |
