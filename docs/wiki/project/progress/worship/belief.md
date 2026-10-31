# Belief and impressiveness

How much a town believes in each player. Villagers who see something impressive, kind or terrible, believe more in the
god who did it; enough belief wins the town over (see `../town/` for conversion). Artefacts left in other players' towns win belief too: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 9/11 done, 0 chosen, 0 partial, 1 inferred — 82%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/worship.md).

## Being impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager who sees something impressive gives its town belief in whoever did it, by how impressive it was, the land's balance and the villager's share of the town | done | `villager_reactions::UpdateHowImpressed` (`src/ECS/Systems/Implementations/VillagerReactions.cpp`): the initiator's value (a miracle's, a missionary's, an object's, a scaffold's, a creature's, `src/Magic/Impressiveness.cpp`) by the villager's share of its town, into `town_stores::AddToBelief`; test `ImpressionTest.TheTownGainsBeliefInTheCaster` |
| Closer watchers are more impressed, along an S curve of distance | done | each value takes the distance's share (`src/Magic/Impressiveness.cpp`); test `Impressiveness.CloseByAllOfItAtTheEdgeAboutATenth` |
| Food and wood impress by how much the town wants them | done | food or wood given to a town adds belief by how much its desire drops, more for a player that does not own it (`DoResourceAdding`, `src/ECS/ObjectResources.cpp` to `town_stores::AddToBelief`); test `ReactionImpressiveness.FoodAndWoodImpressByHowMuchTheTownWantsThem` |
| Things that belong to no player impress no one | done | a reaction made with no player keeps none, apart from the neutral player, and impresses nobody (`villager_reactions::UpdateHowImpressed`); test `ImpressionTest.NoTownOrNoPlayerNothing` |
| Creatures are impressed by their own god's and other creatures' deeds | pending | a creature follows a reaction but how impressed it is is not updated (4 x the initiator's impressive value for its own player's reaction, else 12 x it for the creature that started it; `src/ECS/Systems/Implementations/CreatureReactions.cpp`); see `../creature/` |
| Villagers hiding indoors are still impressed by what they see | done | a villager that cannot be reached (at home, held back from a change of state, hiding in a building) is only impressed by any reaction reaching it, takes nothing up and ends the cell's walk, as are all villagers by the three reactions that only impress (`ImpressOnly`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`); tests `VillagerReactionsTest.AVillagerAtHomeIsOnlyImpressedByAnyReactionAndEndsTheCell`, `VillagerReactionsTest.OnlyImpressedWhenTheTypeOnlyImpressesOrTheVillagerCannotBeReached` (`test/test_villager_reactions.cpp`) |
| Throwing things into a town impresses it (and frightens it) | done | a thrown thing's reaction impresses the villagers who take it up by the object's own value (`magic::ObjectImpressiveValue`, `src/Magic/Impressiveness.cpp`); the fright is the villagers' flight from a flying thing; see `../hand/`; test `ImpressionTest.AnObjectImpressesByItsOwnValueInTheOriginalsOrder` |
| The creature's impressive actions win belief for its god | done | a creature's reaction impresses the villagers who take it up by its size, the last action it carried out and its town's record of it (`magic::CreatureImpressiveValue`, `src/Magic/Impressiveness.cpp`); see `../creature/`; test `Impressiveness.ACreaturesValueIsItsHeldSizeByItsWearinessDistanceDeedAndBalance` |

## A town's belief

How a town keeps, caps, folds and shows its belief, and how it is won over, are rows of
[../town/belief_and_conversion.md](../town/belief_and_conversion.md); these are the rest.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town's unmet desires cost its owner belief at each town turn, and the threshold for it falls back over time | done | (added) `town_belief::Fold` (`src/ECS/Town/TownBelief.cpp`), the town desire info's belief scale and decay; test `TownBeliefTest.FoldDesireCost` |
| Hurting a town lowers its belief in the player who did it (unconfirmed exact rule) | inferred | no belief loss for an attack was found in the exe: an attack records the town's aggressor (`town_emergency::UpdateAggressor`, `src/ECS/Effects/EffectValues.cpp`) and the attacker's later miracles impress that town no more (test `ImpressionTest.AShieldImpressesNothingTheTownItsPlayerAttacked`); whether the original also takes belief is unverified |
| A town nearly lost makes the help sprites warn the player | done | `town_belief::CheckLosingBelief` every ten turns, not on Land 1; test `TownBeliefTest.LosingBelief` |

## Scripts

The land scripts' belief, cap and belief scale, and the challenge scripts' belief natives, are rows of
[../town/belief_and_conversion.md](../town/belief_and_conversion.md).
