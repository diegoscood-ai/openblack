# Fields and fish farms

Fields and fish farms are the town's food-producing places. A field is a patch of crop the town's farmers sow and
harvest; a fish farm is a stretch of water by the shore where fishermen fish. This file covers them as places; the crop
and the food they give are in ../resources/.

**Progress: 13/15 done, 0 partial — 87%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place a town's fields of each type, turned as given | done | CREATE_NEW_TOWN_FIELD and friends (`src/LHScriptX/FeatureScriptCommands.cpp`), `src/ECS/Archetypes/FieldArchetype.cpp`; `test/test_fields.cpp` |
| A field without a town joins the nearest | todo | a field whose town is not found is not made (a guard in `FieldArchetype::Create`) |
| The crop grows on its own turn in every ten, by alignment, sun and rain | done | `fields::ProcessFieldsTurn`, every tenth turn plus the field's offset, by alignment, sun and rain (`src/ECS/Fields.cpp`); `test/test_fields.cpp`; see ../resources/ |
| The crop's look changes with ripeness, sways once ripe and eases to its height | done | the crop's colour by ripeness and its sway once ripe (`src/ECS/Fields.cpp`) |
| Fields start sown; the game has farmers sow them | done | a field starts empty and the farmers sow it (`FARMER_PLANTS_CROP`, `src/ECS/Villager/VillagerFarmer.cpp`); `test/test_villager_farming.cpp`; See ../villager/jobs.md |
| A field knows its farmers and how much it wants farming | done | the farmers' list (`fields::AddFarmer`, `RemoveFarmer`) and the desire to be farmed (`src/ECS/Fields.cpp`) |
| A field burns its food rather than its life | done | a burn takes the field's food, its life stays 1 (`src/ECS/Fire/FireObjectTraits.cpp`) |
| A field destroyed by an effect loses its crop and its fire | done | the crops, growth and food go to 0 and the fire with them (`fire::traits::DestroyedByEffect`, `src/ECS/Fire/FireObjectTraits.cpp`) |
| Water sprinkled on a field by the water miracle helps it grow | done | the water miracle's part on a field (`fields::ApplyWaterSpellToField`, from `src/Magic/Spells/SpellWater.cpp`); See ../miracles/ |
| The creature can eat or examine a field, and stamp on it | todo | the creature does not eat from or stamp on fields; See ../creature/ |
| Crop sheaves can be picked up from a field by hand | done | the hand scoops food from a field with crops (`src/ECS/Systems/Implementations/HandResources.cpp`); See ../hand/ |

## Fish farms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place fish farms for towns | done | CREATE_TOWN_FISH_FARM and CREATE_FISH_FARM (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/FishFarmArchetype.cpp`) |
| A fish farm shows a shoal of fish when there is sea round it | done | each farm's shoal of fifteen fish, shown by the camera distance (`src/ECS/FishShoals.cpp`) |
| Fishermen fish at the farm's spots, taking its fish | done | the fishing spots and the catch (`src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/FishFarms.cpp`); See ../villager/jobs.md |
| The farm's fish come back over time | done | every 16th turn each farm gets 1 food back, up to 1400 (`src/ECS/FishShoals.cpp`); See ../resources/ |
