# Plants and flowers

The small growing things on the land that aren't trees: flower patches, bushes, hedges and crops' look on the land.

**Progress: 4/8 done, 0 partial — 50%**

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place flower patches of each kind, turned and sized | todo | `FeatureScriptCommands::CreateFlowers` is an empty stub (`src/LHScriptX/FeatureScriptCommands.cpp`): no flowers in our tree |
| Flowers are drawn unlit as the game draws them | todo | No flowers in our tree |
| Flowers catch fire and burn | todo | No flowers in our tree |
| Bushes, copses and hedges are kinds of tree, growing and swaying as trees do | done | Tree types of info.dat (`src/ECS/Trees.cpp`); see [trees](trees.md) |
| Fields' crops sway once ripe, further than trees | done | `RenderingSystem.cpp` (a ripe field sways 1.75 times a tree's, `ecs::WindSway`) |
| A field's crop shows green while young and turns its own colour as it ripens | done | `ecs::FieldDrawColour` (`src/ECS/Fields.cpp`) |
| Plants count as their own kind when a miracle's alignment is worked out | done | `AlignmentType::Plant` (`src/ECS/Effects/Alignment.cpp`) |
| The creature eats plants and stomps on flowers (unconfirmed) | todo | see ../creature/ |
