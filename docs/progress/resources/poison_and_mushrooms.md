# Poison and mushrooms

The world has three loose mushrooms: the plain mushroom, the magic mushroom and the toadstool. All three are food the
hand can carry to a store, and only the toadstool is poisonous. Poison lives on food (a store's food, a pile, a pot,
a handful) and on living things; it spreads to whoever takes from poisoned food, slowly wears villagers down, and is
cured by the heal miracle. Scripts poison people and stores directly, as Lethys does in the Plague scroll on Land 2.

**Progress: 27/47 done, 3 partial — 61%**

How the original does it, in our wiki: [Objects and resources](../../bw1-notes/objects-and-resources.md).

## The plain mushroom

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small loose mushroom with its own model; the hand and the creature can pick it up | done | `MobileObjectArchetype`; the hand's pick rules in `src/ECS/HandPickReject.h` |
| Only land scripts place them: 2 by the hippy's hut on Land 1 (taken away when his scroll starts), 7 on Land 3, 12 in the Two Gods playground; nothing grows new ones | done | the land scripts create them (`MobileObjectArchetype`); see [../scripts/land1_script.md](../scripts/land1_script.md) |
| Worth 99 food: dropped on, thrown at or pressed onto a store (or one of its piles) it goes in whole as 99 food, and it is never poisonous | done | `object_delivery::DoDeleteObjectAndTakeResource` (`src/ECS/ObjectDelivery.cpp`) adds it at the info table's food value; a plain object is never poisoned (`object_resources::IsPoisoned`) |
| Its query text is "Mushroom"; held in the hand, the advisor now and then makes the fungus remark ("That's gross. But not as gross as something I just thought of!") | todo | no hand remark for it (`src/Audio/Services/Guidance.cpp` has no caller for it) |
| The creature does not count it as one of the mushrooms that make it high; it is plain food to it | todo | see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |

## The magic mushroom

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose mushroom with its own model; the hand and the creature can pick it up | done | `MobileObjectArchetype` |
| Land scripts place 36 on Land 1 (ten in a ring round the hippy's cauldron, taken away when his scroll starts, the rest in small clumps), 13 on Land 3, 17 on Land 5 and 27 in the Firestorm playground; nothing grows new ones | done | land scripts (`MobileObjectArchetype`); see [../scripts/land1_script.md](../scripts/land1_script.md) |
| Worth 50 food: given to a store it goes in whole as 50 food, never poisonous | done | `src/ECS/ObjectDelivery.cpp` |
| Its query text is "A mushroom."; held in the hand, the evil advisor now and then says "Special Mushroom." | todo | no hand remark for it |
| It and the toadstool are the mushrooms that make the creature high | todo | see [../creature/physiology.md](../creature/physiology.md) |
| The hippy's mushroom puzzle on Land 1 scatters its own magic mushrooms round his cauldron | todo | see [../story/minigames.md](../story/minigames.md) |

## The toadstool

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loose toadstool with its own model, nearly three times as heavy as the other two; the hand and the creature can pick it up | done | `MobileObjectArchetype`; its weight is the physics files' (`src/ECS/Physics/PhysicsObjects.cpp`) |
| Land scripts place 9 on Land 1, 6 on Land 3 and 6 in the Firestorm playground; nothing grows new ones | done | land scripts (`MobileObjectArchetype`) |
| It is the only poisonous loose thing in the world | todo | `object_resources::IsPoisoned` treats every plain object as clean (`src/ECS/ObjectResources.cpp`), so the toadstool is not poisonous in openblack |
| Given to a store it goes in as 150 food and poisons all of the store's food | partial | the 150 food is added (`src/ECS/ObjectDelivery.cpp`) and the store would keep the poison it is given, but the toadstool is never flagged poisonous |
| Given to a worship site it goes in as 150 food and tints the site's food pot, but the worshippers who eat there are never poisoned | todo | worship sites exist (`src/Worship/WorshipSite.cpp`) but keep no poison on their food |
| Its query text is "A toadstool."; held in the hand, the advisor now and then makes the fungus remark | todo | no hand remark for it |
| The creature treats eating one as eating poisoned food, as well as getting high from it | todo | see [../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md) |
| Villagers never pick up or eat any of the three mushrooms | done | villagers have no mushroom behaviour in openblack either |

## Poisoned food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Poison is a yes or no on a pile or pot, not an amount: once any poisoned food goes in, all the food in it is poisoned | done | `Pot::poisoned` is a flag, set when either side was poisoned (`src/ECS/PotResource.cpp`) |
| A store's poison sits on its food pile; adding clean food never clears it | done | a storage pit's poison is its piles' flags, never cleared by adding (`object_resources::IsPoisoned`, `src/ECS/PotResource.cpp`) |
| A pile stays poisoned until it is emptied; emptying it clears the poison | done | taking all of a pile clears its poison (`src/ECS/ObjectResources.cpp`) |
| A poisoned pile with food in it is drawn tinted a pale green, unless it is burning | done | the poisoned pot's colour pair unless it burns (`src/ECS/Systems/Implementations/RenderingSystem.cpp`) |
| A handful scooped from a poisoned pile, or from a poisoned store, is poisoned | done | the hand's new pot is poisoned when its source was (`src/ECS/Systems/Implementations/HandHolding.cpp`); see [../hand/multi_pickup.md](../hand/multi_pickup.md) |
| The stream into the hand, and the pour out of it, use the poisoned food particles | todo | the hand's streams do not switch to the poisoned particles; see [../hand/multi_pickup.md](../hand/multi_pickup.md) |
| A poisoned handful put into a store or onto a pile poisons it; thrown and landing, it makes a poisoned pile | partial | a poisoned handful passes its poison into a store or pile (`HandResources.cpp`, `pot_resource::AddResourceToPos`); the landing pile of a thrown one is the physics files' (unchecked) |
| Poisoned food handed straight to a villager poisons that villager | done | `AddResourceToVillager` poisons the villager (`src/ECS/Villager/VillagerResources.cpp`) |
| Villagers taking food from a poisoned pile, and anyone taking food or wood from a store whose food is poisoned, become poisoned | done | `GetResourceFrom` poisons a villager taking from a poisoned store or pile (`src/ECS/Villager/VillagerResources.cpp`) |
| Food carried home is clean: a home's food is never poisoned, so poison travels only in the people who took it | done | an abode keeps no poison flag (`src/ECS/ObjectResources.cpp`) |
| The food miracle never poisons: its extreme form makes speed-up food instead | done | `src/Magic/Spells/SpellResource.cpp` (the extreme food's flag goes to the speed-up, not the poison); see [../miracles/food.md](../miracles/food.md) |

## Poisoned villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A poisoned villager keeps losing life (a thousandth of full life at each of its checks, every nine game turns), even when well fed | done | the hunger check takes life from a poisoned villager however fed (`src/ECS/Villager/VillagerFood.cpp`) |
| A poisoned villager does not get life back from resting at home | done | no sleep healing while poisoned (`src/ECS/Villager/VillagerHome.cpp`) |
| A poisoned villager pauses far more often, as if its life were half what it is, and each pause plays the poisoned animation instead of the tired one | done | the pause chance with half the life (`src/ECS/Villager/VillagerCore.cpp`) and the poisoned clip (`src/ECS/VillagerAnimations.cpp`) |
| After a meal, at home or from food in hand, a poisoned villager stops to show it is poisoned (stepping out of its home first) | done | after eating, the poisoned clip and state (`src/ECS/Villager/VillagerFood.cpp`) |
| Its query text is "This person's poisoned!" | todo |  |
| A poisoned villager's death is counted as starvation or exhaustion; nothing records poison as the cause | done | the poisoned villager dies of the hunger check, reason Starving (`src/ECS/Villager/VillagerFood.cpp`) |
| A baby born to a poisoned mother is poisoned | done | `src/ECS/Villager/VillagerBirth.cpp` |
| Villagers never notice poisoned food or avoid it | done | openblack villagers don't judge food either |
| Nobody is blamed: giving a store a toadstool counts as an ordinary gift of food to the town, with the usual belief and alignment | done | gifts to stores count as ordinary gifts (`DoResourceAdding`, `src/ECS/ObjectResources.cpp`) |
| Animals can be poisoned only by scripts, and poison does nothing to them | partial | poison does nothing to animals (`src/ECS/Life.cpp`); SET_POISONED, the script way to poison them, is a stub |

## Curing poison

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The heal miracle cures the villagers it reaches | done | `src/Magic/Core/SpellEvent.cpp` removes `Poisoned` for the heal and its power-up; see [../miracles/heal.md](../miracles/heal.md) |
| A heal aimed straight at a poisoned pile or pot cleans it | todo | a heal clears a living's poison only, never a pile's |
| A script clearing a store's poison leaves its food poisoned; emptying the store's food pile is what clears it | todo | whether a heal aimed at the store's own food pile cleans it, as it does a loose pile, is unconfirmed |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts poison or cure a person, a pile, a store or every member of a town or flock at once | todo | SET_POISONED is a stub in `src/CHLApi.cpp`; see [../scripts/challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) |
| Poisoning a store poisons only its food, and only if it has some | todo |  |
| Scripts count the poisoned members of a town, and find poisoned or healthy ones | todo | ID_POISONED_SIZE, IS_POISONED, CALL_POISONED_IN and CALL_NOT_POISONED_IN are stubs in `src/CHLApi.cpp` |
| The Plague scroll (Land 2) poisons a village and its store and is won by emptying the store and healing the people | todo | the scripts' poison natives are stubs; see [../story/land_2.md](../story/land_2.md) |
| The unused "Food for Thought" challenge ("The Nomads") checks whether the player hands hungry nomads poisoned food, and kills them all if so | n/a | not shipped; see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |
| The game's text still names a "Poisoned Food Miracle" ("Poisoned Food. Boss, this will cause a whole lot of sickness!") left over from before the extreme food miracle made speed-up food | n/a | leftover text, never shown for a working miracle |
