# Poison and mushrooms

The world has three loose mushrooms: the plain mushroom, the magic mushroom and the toadstool. All three are food the
hand can carry to a store, and only the toadstool is poisonous. Poison lives on food (a store's food, a pile, a pot,
a handful) and on living things; it spreads to whoever takes from poisoned food, slowly wears villagers down, and is
cured by the heal miracle. Scripts poison people and stores directly, as Lethys does in the Plague scroll on Land 2.

**Progress: 30/47 done, 0 chosen, 9 partial, 0 inferred — 80%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## The plain mushroom

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small loose mushroom with its own model; the hand and the creature can pick it up | partial 90% | its info row (`MobileObjectArchetype`) is of its own kind with its own model and weighs 70, as info.dat (test `FoodWood.realInfoDatMushroomsAndPiles`, `test/test_food_wood.cpp`); a loose thing always goes in the hand (`HandGrab.PlaceInHandIsAnAllowlist`), and a creature takes it by the test of things in general (`CreatureObjectActions.EachKindOfThingTakesItsRule`), from 87.5 of its own weight on (`CreaturePickUpRules.AToadstoolTakesACreatureNearlyThreeTimesAsHeavyAsTheOtherMushroomsDo`). Missing: no test that a mushroom entity is sorted as a loose thing by the hand's and the creature's systems (`HandSystem.cpp`, `CreatureObjectActionSystem.cpp`) |
| Only land scripts place them: 2 by the hippy's hut on Land 1 (taken away when his scroll starts), 7 on Land 3, 12 in the Two Gods playground; nothing grows new ones | done | the land scripts create them (`MobileObjectArchetype`); see [../scripts/land1_script.md](../scripts/land1_script.md); test `FoodWood.realLandScriptsPlainMushrooms` |
| Worth 99 food: dropped on, thrown at or pressed onto a store (or one of its piles) it goes in whole as 99 food, and it is never poisonous | done | Test `StoreRules.AMushroomIsWorthItsFoodValue`; `object_delivery::DoDeleteObjectAndTakeResource` (`src/ECS/ObjectDelivery.cpp`) adds it at the info table's food value; a plain object is never poisoned (`object_resources::IsPoisoned`) |
| Its query text is "Mushroom"; held in the hand, the advisor now and then makes the fungus remark ("That's gross. But not as gross as something I just thought of!") | pending | no hand remark for it (`src/Audio/Services/Guidance.cpp` has no caller for it) |
| The creature does not count it as one of the mushrooms that make it high; it is plain food to it | partial 90% | a mushroom to the creature is a thing of the magic mushroom or toadstool class or the magic mushroom feature (`IsMushroom`, `src/ECS/Systems/Implementations/CreatureObjectActionSystem.cpp`, `CreatureMindLearning.cpp`), as `IsMushroom` 0x4E4CE0; test `CreatureCastTargetsTest.GettingHighIsOnlyDoneToAMushroom` (`test/creature/test_creature_cast_targets.cpp`). Missing: the test has no plain mushroom among what it refuses; see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |

## The magic mushroom

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose mushroom with its own model; the hand and the creature can pick it up | done | Scene `objects.magic_mushroom`; `MobileObjectArchetype`; tests `FoodWood.realInfoDatMushroomsAndPiles`, `CreatureObjectActions.AMagicMushroomIsPickedUpUnlessAScriptSaidNo` |
| Land scripts place 36 on Land 1 (ten in a ring round the hippy's cauldron, taken away when his scroll starts, the rest in small clumps), 13 on Land 3, 17 on Land 5 and 27 in the Firestorm playground; nothing grows new ones | done | land scripts (`MobileObjectArchetype`); see [../scripts/land1_script.md](../scripts/land1_script.md); test `FoodWood.realLandScriptsMagicMushrooms` |
| Worth 50 food: given to a store it goes in whole as 50 food, never poisonous | done | Test `StoreRules.AMushroomIsWorthItsFoodValue`; `src/ECS/ObjectDelivery.cpp` |
| Its query text is "A mushroom."; held in the hand, the evil advisor now and then says "Special Mushroom." | pending | no hand remark for it |
| It and the toadstool are the mushrooms that make the creature high | partial 80% | a mushroom eaten is always eaten and makes it high once its agenda is done (`CreatureObjectActionSystem::EndMeal`, `JudgeMeal`, `CreatureMindSystem::AteMagicMushroom`); tests `CreatureEatingMeal.AMushroomOrPileFoodIsAlwaysEaten`, `CreatureCastTargetsTest.GettingHighIsOnlyDoneToAMushroom`, `CreatureKindActions.GettingHighEatsTheThingPickedUp`; the high itself: see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| The hippy's mushroom puzzle on Land 1 scatters its own magic mushrooms round his cauldron | pending | see [../story/minigames.md](../story/minigames.md) |

## The toadstool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose toadstool with its own model, nearly three times as heavy as the other two; the hand and the creature can pick it up | partial 90% | its info row (`MobileObjectArchetype`) is of its own kind with its own model and weighs 200 against the other two's 70, as info.dat (test `FoodWood.realInfoDatMushroomsAndPiles`, `test/test_food_wood.cpp`); the physics weighs it by that row (`DynamicsSystem::WeightOf`); a loose thing always goes in the hand (`HandGrab.PlaceInHandIsAnAllowlist`), and a creature takes it by the test of things in general from 250 of its own weight on (`CreaturePickUpRules.AToadstoolTakesACreatureNearlyThreeTimesAsHeavyAsTheOtherMushroomsDo`). Missing: no test that a toadstool entity is sorted as a loose thing by the hand's and the creature's systems (`HandSystem.cpp`, `CreatureObjectActionSystem.cpp`) |
| Land scripts place 9 on Land 1, 6 on Land 3 and 6 in the Firestorm playground; nothing grows new ones | done | land scripts (`MobileObjectArchetype`); test `FoodWood.realLandScriptsToadstools` |
| It is the only poisonous loose thing in the world | done | The scripts see it poisoned (`object_poison::IsPoisoned`; test `ScriptPoisonNatives.APotKeepsItsOwnFlagAndAToadstoolIsAlwaysPoisoned`), and so do the deliveries and the takers (`object_resources::IsPoisoned`, as `MobileObject::IsPoisoned`; test `TakeResourceTest.AToadstoolAndAPoisonedLivingThingAnswerPoisoned`) |
| Given to a store it goes in as 150 food and poisons all of the store's food | partial 90% | the 150 food is added poisoned (`src/ECS/ObjectDelivery.cpp`, the toadstool answering poisoned through `object_resources::IsPoisoned`) and the store keeps the poison; tests `TakeResourceTest.APoisonedPutDownPoisonsThePit`, `TakeResourceTest.AToadstoolAndAPoisonedLivingThingAnswerPoisoned`; no test of the delivery itself |
| Given to a worship site it goes in as 150 food and tints the site's food pot, but the worshippers who eat there are never poisoned | partial 90% | The site's food pot keeps the poison it is given (`site::AddResource`, `src/Worship/WorshipSite.cpp`), and the toadstool is delivered poisoned (`object_resources::IsPoisoned`); tests `WorshipTest.FoodGivenToASiteFillsItsFoodPot`, `TakeResourceTest.AToadstoolAndAPoisonedLivingThingAnswerPoisoned`; no test of the delivery itself |
| Its query text is "A toadstool."; held in the hand, the advisor now and then makes the fungus remark | pending | no hand remark for it |
| The creature treats eating one as eating poisoned food, as well as getting high from it | partial 50% | a toadstool is poisoned food to the creature (`IsPoisonedFood`) and a mushroom (`IsMushroom`); the end of a meal tests the mushroom first, so it is eaten and makes it high, as the original's 0x500540 ([the end of a meal](../../../creature/plans-and-needs.md#the-end-of-a-meal)); test `CreatureEatingMeal.AMushroomOrPileFoodIsAlwaysEaten` (a poisoned mushroom is eaten). Missing: whether eating one does anything else of poisoned food is not read; see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| Villagers never pick up or eat any of the three mushrooms | done | villagers have no mushroom behaviour in openblack either; test `VillagerResourcesTest.TheFoodDesireNeverTakesAMushroom` |

## Poisoned food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Poison is a yes or no on a pile or pot, not an amount: once any poisoned food goes in, all the food in it is poisoned | done | Tests `TakeResourceTest.APoisonedPutDownPoisonsThePit`, `APoisonedPutDownOnAPitsPilePoisonsThePit`; `Pot::poisoned` is a flag, set when either side was poisoned (`src/ECS/PotResource.cpp`) |
| A store's poison sits on its food pile; adding clean food never clears it | done | Test `ScriptPoisonNatives.AStoragePitIsPoisonedByItsPilesAndOnlyItsFoodPileIsEverPoisoned`; a storage pit's poison is its piles' flags, never cleared by adding (`object_resources::IsPoisoned`, `src/ECS/PotResource.cpp`) |
| A pile stays poisoned until it is emptied; emptying it clears the poison | done | Test `TakeResourceTest.APoisonedPileStaysPoisonedUntilItIsEmptied` (`test/test_take_resource.cpp`: a worship site's food pot and a store's food pile); taking all of a pile clears its poison (`src/ECS/ObjectResources.cpp`), as the original's emptied pot loses its poison ([pots and piles](../../../world/objects.md#pots-and-piles)) |
| A poisoned pile with food in it is drawn tinted a pale green, unless it is burning | partial 90% | No test or scene; the poisoned pot's colour pair unless it burns (`src/ECS/Systems/Implementations/RenderingSystem.cpp`) |
| A handful scooped from a poisoned pile, or from a poisoned store, is poisoned | partial 90% | the hand's new pot is poisoned when its source was (`src/ECS/Systems/Implementations/HandHolding.cpp`); test `HandPickUpWorld.AHandfulFromAPoisonedPileIsPoisoned` (`test/hand/test_hand_pick_up_world.cpp`, a loose pile); missing: no test of a handful from a poisoned store; see [../hand/multi_pickup.md](../hand/multi_pickup.md) |
| The stream into the hand, and the pour out of it, use the poisoned food particles | pending | the hand's streams do not switch to the poisoned particles; see [../hand/multi_pickup.md](../hand/multi_pickup.md) |
| A poisoned handful put into a store or onto a pile poisons it; thrown and landing, it makes a poisoned pile | done | Tests `TakeResourceTest.APoisonedPutDownPoisonsThePit`, `APoisonedPutDownOnAPitsPilePoisonsThePit`, `WhatNothingTakesMakesANewPileOnLandButNotInTheWater` (a new pile keeps the poison), `ParticleCarriedObjectsTest.APotLetGoEndsItsPhysicsAsAPot` (a landed hand pot is put down); a poisoned handful passes its poison into a store or pile (`HandResources.cpp`, `pot_resource::AddResourceToPos`), and a thrown one that lands is put down the same way (`HandSystem::PotEndPhysics`, `HandPhysics.cpp`) |
| Poisoned food handed straight to a villager poisons that villager | done | Test `VillagerResourcesTest.AddResourceToVillager`; `AddResourceToVillager` poisons the villager (`src/ECS/Villager/VillagerResources.cpp`) |
| Villagers taking food from a poisoned pile, and anyone taking food or wood from a store whose food is poisoned, become poisoned | done | Test `VillagerResourcesTest.TakingFromAPoisonedStoreOrPilePoisonsTheTaker` (`test/test_villager_resources.cpp`); `GetResourceFrom` poisons a villager taking from a poisoned store or pile (`src/ECS/Villager/VillagerResources.cpp`), asking after the taking as the original does, so the one who takes the last of the poisoned food stays clean ([taking from a structure](../../../beings/villager-jobs.md#taking-from-and-giving-to-a-structure), [store](../../../world/objects.md#store-storagepit)) |
| Food carried home is clean: a home's food is never poisoned, so poison travels only in the people who took it | done | Tests `TakeResourceTest.AHomeKeepsNoPoisonFromTheFoodItIsGiven` (`test/test_take_resource.cpp`), `VillagerResourcesTest.TakingFromAPoisonedStoreOrPilePoisonsTheTaker` (`test/test_villager_resources.cpp`: who eats from such a home stays clean); an abode keeps no poison flag (`src/ECS/ObjectResources.cpp`), as an abode answers not poisoned in the original ([resources held by objects](../../../world/buildings.md#resources-held-by-objects)) |
| The food miracle never poisons: its extreme form makes speed-up food instead | done | Test `SpellBehaviours.FoodLandsOnDryLandAsPilesAndPaysForEveryGrain` (`test/test_magic_spells.cpp`: the power-up's food makes its new piles sparkle); the miracle's put-down carries no poison at all, and the extreme food's flag goes to the speed-up (`src/Magic/Spells/SpellResource.cpp`); see [../miracles/food.md](../miracles/food.md) |

## Poisoned villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A poisoned villager keeps losing life (a thousandth of full life at each of its checks, every nine game turns), even when well fed | done | Test `VillagerFoodTest.HungerDamage`; the hunger check takes life from a poisoned villager however fed (`src/ECS/Villager/VillagerFood.cpp`) |
| A poisoned villager does not get life back from resting at home | done | Tests `VillagerHomeTest.Sleeping` (`test/test_villager_home.cpp`), `VillagerRoutine.PoisonWakesASleeper` (`test/test_villager_routine.cpp`); no sleep and no healing while poisoned (`src/ECS/Villager/VillagerHome.cpp`), as the original's sleep check ([home, food, sleep](../../../beings/villager-life.md#home-food-sleep-homeless-and-age)) |
| A poisoned villager pauses far more often, as if its life were half what it is, and each pause plays the poisoned animation instead of the tired one | done | Test `VillagerAnimation.APoisonedVillagerPausesLookingPoisonedWithoutADraw`; scene `villagers.poisoned_pause`; the pause chance with half the life (`src/ECS/Villager/VillagerCore.cpp`) and the poisoned clip (`src/ECS/VillagerAnimations.cpp`) |
| After a meal, at home or from food in hand, a poisoned villager stops to show it is poisoned (stepping out of its home first) | done | Tests `VillagerFoodTest.EatingStates`, `APoisonedVillagerShowsItAfterAMealAtHomeSteppingOutFirst` (`test/test_villager_food.cpp`); after eating, the poisoned state and clip, from inside its home a spot out of the door first (`src/ECS/Villager/VillagerFood.cpp`), as the original's meal states ([home, food, sleep](../../../beings/villager-life.md#home-food-sleep-homeless-and-age)) |
| Its query text is "This person's poisoned!" | pending |  |
| A poisoned villager's death is counted as starvation or exhaustion; nothing records poison as the cause | done | Tests `VillagerFoodTest.APoisonedVillagerDiesOnlyOfHungerOrExhaustion` (`test/test_villager_food.cpp`), `VillagerCoreTest.DeathReasons` (`test/test_villager_core.cpp`); the hunger check kills only a hungry villager, reason Starving, and a well-fed poisoned one whose life runs out dies at its state's check, reason Exhaustion (`src/ECS/Villager/VillagerFood.cpp`, `VillagerCore.cpp`), as the original's hunger check ([home, food, sleep](../../../beings/villager-life.md#home-food-sleep-homeless-and-age)) |
| A baby born to a poisoned mother is poisoned | done | Test `VillagerBirthTest.ChildBornTownVagrantPoison`; `src/ECS/Villager/VillagerBirth.cpp` |
| Villagers never notice poisoned food or avoid it | done | openblack villagers don't judge food either; test `VillagerResourcesTest.TakingFromAPoisonedStoreOrPilePoisonsTheTaker` |
| Nobody is blamed: giving a store a toadstool counts as an ordinary gift of food to the town, with the usual belief and alignment | done | Test `TakeResourceTest.APoisonedGiftOfFoodCountsAsAnOrdinaryGift` (`test/test_take_resource.cpp`); gifts to stores count as ordinary gifts whether poisoned or not (`DoResourceAdding`, `src/ECS/ObjectResources.cpp`; [the hand's branch](../../../world/buildings.md#resources-held-by-objects)) |
| Animals can be poisoned only by scripts, and poison does nothing to them | done | Test `ScriptPoisonNatives.LivingThingsKeepTheirPoisonAsABitOfTheirOwn`; poison does nothing to animals (`src/ECS/Life.cpp`); SET_POISONED poisons them (`ecs::object_poison::SetPoisoned`, `src/ECS/ObjectPoison.cpp`) |

## Curing poison

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The heal miracle cures the villagers it reaches | done | Test `GameMagicWorldObjects.TheHealCuresPoisonAndNothingElse`; `src/Magic/Core/SpellEvent.cpp` removes `Poisoned` for the heal and its power-up; see [../miracles/heal.md](../miracles/heal.md) |
| A heal aimed straight at a poisoned pile or pot cleans it | pending | a heal clears a living's poison only, never a pile's |
| A script clearing a store's poison leaves its food poisoned; emptying the store's food pile is what clears it | done | `ecs::object_poison::SetPoisoned` only ever poisons a store's food pile; an emptied pot loses its poison (`object_resources::RemoveFromPotDirect`); `test/test_script_poison_natives.cpp`. Whether a heal aimed at the store's own food pile cleans it, as it does a loose pile, is unconfirmed |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts poison or cure a person, a pile, a store or every member of a town or flock at once | done | `SetPoisoned` in `src/CHLApi.cpp`, `ecs::object_poison` (`src/ECS/ObjectPoison.cpp`); `test/test_script_poison_natives.cpp`; see [../scripts/challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) |
| Poisoning a store poisons only its food, and only if it has some | done | `ecs::object_poison::SetPoisoned` (its available food pile); `test/test_script_poison_natives.cpp` |
| Scripts count the poisoned members of a town, and find poisoned or healthy ones | done | `IdPoisonedSize`, `IsPoisoned`, `CallPoisonedIn` and `CallNotPoisonedIn` in `src/CHLApi.cpp`; `test/test_script_poison_natives.cpp` |
| The Plague scroll (Land 2) poisons a village and its store and is won by emptying the store and healing the people | pending | the scripts' poison natives are real; the quest never starts in play (Land 2's story stops on arrival); see [../story/land_2.md](../story/land_2.md) |
