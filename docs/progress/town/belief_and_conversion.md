# Town belief and conversion

Every town believes a little in each player. Impressive deeds near it raise its belief in their doer; once belief in a
player passes what the town needs, the town is won over and becomes that player's, its influence and people with it.
A town can be lost again if belief in its owner falls behind another's.

**Progress: 13/25 done, 7 partial — 66%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town keeps its belief in every player, starting with belief in the neutral player from the tables | done | the town's per-player block (`components::TownBelief`, `town_belief::Init`, `src/ECS/Town/TownBelief.cpp`), the neutral belief from the town info; `test/test_town_belief.cpp` |
| What its people are impressed by waits until the town's turn and is believed all at once, times the town's belief scale | partial | the town turn folds the pending belief times the town's belief scale (`town_belief::Fold`, from `src/ECS/Town/TownProcess.cpp`), but what villagers are impressed by feeds no belief yet (TODO in `villager_reactions::AddReaction`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Belief in a player is capped, and scripts can set the cap | done | the cap, 10 at the start (`town_belief::SetBelief`, `SetCap`); SET_TOWN_BELIEF_CAP (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Scripts set a town's belief in a player | done | SET_TOWN_BELIEF sets the town's slot through `town_belief::SetBeliefInPlayer` (`src/LHScriptX/FeatureScriptCommands.cpp`), the neutral player's own value included |
| A symbol of belief gained rises from the town centre in the player's colour | partial | the fold queues a belief sprite in the player's colour at the town centre (`QueueBeliefSprite`, `src/ECS/Town/TownBelief.cpp`), but nothing draws the queue yet |
| Towns grow bored of the same kind of deed seen again and again | partial | the fold steps every reaction's boredom multiplier (`town_belief::Fold`), but no impression reads it yet, as impressions feed no belief |
| Recent belief given fades every turn | done | recent belief times the decay every fold (`town_belief::Fold`) |
| Belief from miracles, from the creature, from artefacts, from scaffolds and buildings given, from disciples and missionaries | partial | only food and wood given by the hand to a town (`town_stores::AddToBelief` from `src/ECS/ObjectResources.cpp`); miracles, the creature, artefacts, scaffolds (TODO in `scaffolds::ImpressTowns`, `src/ECS/Scaffolds.cpp`) and disciples give none |
| Taking a town's resources lowers its belief for a while | done | taking marks the turn (`town_stores::SetGameTurnResourceLastRemoved`) and giving that resource back earns less for 1000 turns (`GetGameTurnResourceLastRemovedModifier`, `src/ECS/Town/TownStores.cpp`, used in `src/ECS/ObjectResources.cpp`). Our wiki differs: taking lowers no belief at once; it cuts the belief later gifts of that resource earn, for up to 1000 turns ([buildings](../../bw1-notes/buildings.md#resources-held-by-objects)) |
| A town believing in a player moves faster as the land's belief speed scale says | partial | the villagers' speed takes the land balance's speed scale (`src/ECS/VillagerSpeed.cpp`); the town's belief term is fixed at 1 |
| How impressed a town is shows over it (the belief bar or tooltip) | partial | the belief symbols over each town centre, sized by each player's belief (`psys::town_belief`, `src/Particles/TownBelief.cpp`); no belief tooltip on the town; See ../interface/ |
| Every ten turns the believers count for the tooltip | done | every ten turns the local player's believers gained go to the tooltip (`town_belief::ProcessOncePerTurn`, `events::TownBeliefToolTip`, from `src/Game.cpp`); the floating number by the hand is not drawn |

## Conversion

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each town needs a set amount of belief to be won over, by its size | done | the town goes to whichever player it believes in most (`town_belief::Fold`). Our wiki differs: there is no set amount by size; the town changes hands when another player's belief is the largest, a tie going to the higher player slot ([buildings](../../bw1-notes/buildings.md#the-towns-belief)) |
| When belief in a player passes what is needed, the town becomes that player's | done | the fold's conversion and `town_belief::TakeOverTown` change the owner and the owner lists (`src/ECS/Town/TownBelief.cpp`) |
| A won town celebrates, with fireworks and its villagers cheering | todo | the take-over's sound, fireworks and symbols are not ported (`TakeOverTown`) |
| A won town joins its new owner's worship site and influence, and its spells go to the player | partial | the influence follows the new owner (`src/ECS/Influence/Influence.cpp` reads `Town::owner`); the worship site, the spells and the rest of the take-over are pending in `TakeOverTown`; See ../worship/ |
| The previous owner loses the town and its influence | done | the old owner's list and influence lose the town with the owner change (`town_belief::TakeOverTown`) |
| Losing a town lowers belief elsewhere by the lost-town scale | done | on a conversion every town of the old owner keeps 0.9 of its belief times the lost-town scale (`town_belief::Fold`); SET_LOST_TOWN_SCALE (`land_balance::SetLostTownScale`, `src/LHScriptX/FeatureScriptCommands.cpp`) |
| Claiming a town multiplies its belief by the claimed-town multiplier | done | the new owner's belief times the claimed-town multiplier on a conversion (`town_belief::Fold`) |
| A town's attitude to each creature changes how its villagers react to it | todo | the villagers' creature reactions are TODO rows of the villager state table (`src/ECS/Systems/Implementations/LivingActionSystem.cpp`) |
| The take-town cheat gives the player a town at once | todo |  |
| Scripts ask who owns a town and how much it believes | todo | BELIEF_FOR_PLAYER, OBJECT_RELATIVE_BELIEF and GET_PLAYER_TOWN_TOTAL are stubs (`src/CHLApi.cpp`); See ../story/ |

## Influence multipliers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts scale every town's influence | done | SET_TOWN_INFLUENCE_MULTIPLIER (`src/LHScriptX/FeatureScriptCommands.cpp`), read by `src/ECS/Influence`; see ../worship/ |
| Scripts scale one town's influence | todo | SET_A_TOWNS_INFLUENCE_MULTIPLIER only logs "not implemented" (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Scripts set the town's belief balance scale | done | SET_TOWN_BALANCE_BELIEF_SCALE sets the town's belief scale, used by the fold (`src/LHScriptX/FeatureScriptCommands.cpp`, `town_belief::Fold`) |
