# Fields and fish farms

Fields and fish farms are the town's food-producing places. A field is a patch of crop the town's farmers sow and
harvest; a fish farm is a stretch of water by the shore where fishermen fish. This file covers them as places; the crop
and the food they give are in ../resources/.

**Progress: 15/18 done, 0 chosen, 1 partial, 1 inferred — 86%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place a town's fields of each type, turned as given | done | CREATE_NEW_TOWN_FIELD and friends (`src/LHScriptX/FeatureScriptCommands.cpp`), `src/ECS/Archetypes/FieldArchetype.cpp`; `test/test_fields.cpp`, the fidelity run (Land 1's fields) |
| A field the script gives no town is a field of no town | partial 50% | the town commands make nothing for an unknown town, as the original (CREATE_TOWN_FIELD / CREATE_NEW_TOWN_FIELD skip on a failed FindTownWithID, 0x716653 / 0x7166A1). Missing: CREATE_FIELD makes a field of no town in the original (`Field::Create` 0x528280 with no town takes the default field info); openblack makes none (a guard in `FieldArchetype::Create`) |
| The crop grows on its own turn in every ten, by alignment, sun and rain | done | `FieldSystem::ProcessTurn` runs `ecs::ProcessField` for every field, which grows it every tenth turn plus the field's offset, by alignment, sun and rain (`src/ECS/Fields.cpp`); `test/test_fields.cpp`; see ../resources/ |
| The crop's look changes with ripeness, sways once ripe and eases to its height | done | the crop's colour by ripeness and its sway once ripe (`src/ECS/Fields.cpp`); `test/test_field_system.cpp` |
| A field starts empty and the town's farmers sow it | done | a field starts empty and the farmers sow it (`FARMER_PLANTS_CROP`, `src/ECS/Villager/VillagerFarmer.cpp`); `test/test_villager_farming.cpp`; See ../villager/jobs.md |
| A field knows its farmers and how much it wants farming | done | the farmers' list (`fields::AddFarmer`, `RemoveFarmer`) and the desire to be farmed (`src/ECS/Fields.cpp`); `test/test_fields.cpp` (`FarmersNewestFirstNoDuplicatesNoMaximum`, `DesireToBeFarmed`) |
| A field burns its food rather than its life | done | a burn takes the field's food, the damage x totalFoodInField, and answers a life of 1 without attacking its town (`src/ECS/Fire/FireObjectTraits.cpp`), as `Field::ReduceLifeDueToBurning` 0x52A050 ([fire](../../../world/fire.md)); test `FieldsTest.ABurnTakesAFieldsFoodAndLeavesItsLife` (`test/test_fields.cpp`) |
| A field destroyed by an effect loses its crop and its fire | done | the crops, growth and food go to 0, the fire's temperature to 0 and the fire is deleted, and the field stays (`fire::traits::DestroyedByEffect`, `src/ECS/Fire/FireObjectTraits.cpp`), as `Field::DestroyedByEffect` 0x52A010 ([fire](../../../world/fire.md)); test `FieldsTest.AFieldDestroyedByAnEffectLosesItsCropAndItsFire` (`test/test_fields.cpp`) |
| Water sprinkled on a field by the water miracle helps it grow | done | the water miracle's part on a field (`fields::ApplyWaterSpellToField`, from `src/Magic/Spells/SpellWater.cpp`); `test/test_fields.cpp` (`FieldWaterSpell`), the testbed scenario `miracles.water_fields`; See ../miracles/ |
| The creature can eat from a field (it cannot examine, stamp on or pick one up) | done | the creature eats from a field where it stands (`creature_eating::FieldHasFood`); `test/creature/test_creature_eating.cpp`, the testbed scenario `mind.eats_from_field`; the original's `Field` answers no to examining, stomping and picking up (`Field.cpp`); See ../creature/ |
| Crop sheaves can be picked up from a field by hand | done | the hand scoops food from a field with crops and food (`hand_grab::FieldValidForPlaceInHand`, `src/Hand/HandGrabRules.cpp`, `src/ECS/Systems/Implementations/HandSystem.cpp`); `test/hand/test_hand_grab.cpp` (`FieldsNeedGrowthAndFood`); See ../hand/ |

## Fish farms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place fish farms for towns | done | CREATE_TOWN_FISH_FARM and CREATE_FISH_FARM (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/FishFarmArchetype.cpp`); the fidelity run (Land 1's fish farms); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| A fish farm shows a shoal of fish when there is sea round it | done | each farm's shoal of fifteen fish, shown by the camera distance (`src/ECS/FishShoals.cpp`); `test/test_fish_farm.cpp` (`FishShoal` tests), the testbed scenario `hand.fish_farm` |
| Fishermen go to the sea or their town's fish farm, fish at its spots and carry the catch to the store | done | the fishing spots and the catch (`src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/FishFarms.cpp`); the farm's own fish are not taken by fishing, as the original; `test/test_fish_farm.cpp`, the testbed scenario `villagers.footpaths`; See ../villager/jobs.md |
| The farm's fish come back over time | done | every 16th turn each farm gets 1 food back, up to 1400 (`src/ECS/FishShoals.cpp`); `test/test_fish_farm.cpp` (`AFishComesBackOnEverySixteenthTurnUpToFull`); See ../resources/ |
| The hand over a farm's fish offers to pick them up | done | the fish hint in the hand's texts (`HandToolTips`, `FindFishFarmAt`); `test/hand/test_hand_tool_tips.cpp` (`AHintComesBeforeAFishAndAFishBeforeTheMove`) |
| The fish shoals' sounds keep their own pitch | inferred | listed as missing by issue #76 and [sea, coast and rivers](../../../world/sea-coast-and-rivers.md) Pending, without the original's code read |
| The fish puzzle's fisherman (Land 4) | pending | waits until Land 4 can be reached (issue #76); see ../animal/fish.md |
