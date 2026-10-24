# Belief and impressiveness

How much a town believes in each player. Villagers who see something impressive, kind or terrible, believe more in the
god who did it; enough belief wins the town over (see `../town/` for conversion). Artefacts left in other players' towns win belief too: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 18/25 done, 0 chosen, 3 partial, 1 inferred — 81%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/worship.md).

## Being impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager who sees something impressive gives its town belief in whoever did it, by how impressive it was, the land's balance and the villager's share of the town | done | `villager_reactions::UpdateHowImpressed` (`src/ECS/Systems/Implementations/VillagerReactions.cpp`): the initiator's value (a miracle's, a missionary's, an object's, a scaffold's, a creature's, `src/Magic/Impressiveness.cpp`) by the villager's share of its town, into `town_stores::AddToBelief`; test `ImpressionTest.TheTownGainsBeliefInTheCaster` |
| Closer watchers are more impressed, along an S curve of distance | done | each value takes the distance's share (`src/Magic/Impressiveness.cpp`); test `Impressiveness.CloseByAllOfItAtTheEdgeAboutATenth` |
| Food and wood impress by how much the town wants them | done | food or wood given to a town adds belief by how much its desire drops, more for a player that does not own it (`DoResourceAdding`, `src/ECS/ObjectResources.cpp` to `town_stores::AddToBelief`); test `ReactionImpressiveness.FoodAndWoodImpressByHowMuchTheTownWantsThem` |
| Things that belong to no player impress no one | done | a reaction made with no player keeps none, apart from the neutral player, and impresses nobody (`villager_reactions::UpdateHowImpressed`); test `ImpressionTest.NoTownOrNoPlayerNothing` |
| A town grows bored of seeing the same kind of thing; boredom wears off each town turn, times the land's lost-town scale | done | the villagers' impression adds the reaction's boredom (`town_belief::AddToBoredomMultiplier` from `villager_reactions::UpdateHowImpressed`) and the fold wears it off at each town turn, times the lost-town scale (`town_belief::Fold`, `src/ECS/Town/TownBelief.cpp`); tests `TownBeliefTest.FoldBoredom`, `ImpressionTest.AgainItImpressesLessForTheBoredom` |
| Creatures are impressed by their own god's and other creatures' deeds | pending | no creature impression; see `../creature/` |
| Villagers hiding indoors are still impressed by what they see | pending | the belief-only branch of the reaction spread is not ported (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Artefacts given to a town keep impressing it | partial 60% | a dance in another town gives its owner belief (ported from raffclar PR #278, unverified against v1.42); a glowing artefact's reaction impresses by its artefact modifier (`magic::ObjectImpressiveValue`); see [../town/artefacts.md](../town/artefacts.md); both parts are not read against v1.42 |
| Throwing things into a town impresses it (and frightens it) | done | a thrown thing's reaction impresses the villagers who take it up by the object's own value (`magic::ObjectImpressiveValue`, `src/Magic/Impressiveness.cpp`); the fright is the villagers' flight from a flying thing; see `../hand/`; test `ImpressionTest.AnObjectImpressesByItsOwnValueInTheOriginalsOrder` |
| The creature's impressive actions win belief for its god | done | a creature's reaction impresses the villagers who take it up by its size, the last action it carried out and its town's record of it (`magic::CreatureImpressiveValue`, `src/Magic/Impressiveness.cpp`); see `../creature/`; test `Impressiveness.ACreaturesValueIsItsHeldSizeByItsWearinessDistanceDeedAndBalance` |

## A town's belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belief gained waits until the town's turn, then is believed at once, times the town's belief scale | done | `town_belief::Fold` from the town's process (`src/ECS/Town/TownProcess.cpp`), times the town's `beliefScale`; test `TownBeliefTest.FoldPending` |
| Belief in a player is capped (10 unless a script sets another cap) | done | `town_belief::SetBelief` caps to the town's cap, 10 to start (`SET_TOWN_BELIEF_CAP` changes it); test `TownBeliefTest.SetBeliefClampsToCapOnly` |
| A symbol of the belief gained rises from the town centre in the player's colour | done | the fold queues a belief sprite at the town centre in the player's colour (`town_belief::QueueBeliefSprite`), drawn by the belief sprite rule, last in first out (`src/Particles/Rules/Belief.cpp`, [the belief sprites](../../../physics/particle-rules.md#the-belief-sprites)); tests `BeliefSpriteRule.*`, `BeliefSpriteMaths.*` |
| Each town keeps a fading tally of the belief each player has lately won | done | `recent` decays at each fold (test `TownBeliefTest.FoldRecentDecay`); the belief added in the period is cleared every ten turns (`town_belief::ProcessOncePerTurn`) |
| Once claimed, a town's belief is reset by the claimed-town multiplier | done | the new owner's belief times the claimed-town multiplier (`town_belief::Fold`; test `TownBeliefTest.FoldConversion`) |
| Losing a town lowers belief in that player elsewhere | done | the old owner's other towns times the lost-town multiplier and the lost-town scale (`town_belief::Fold`) |
| A town's unmet desires cost its owner belief at each town turn, and the threshold for it falls back over time | done | (added) `town_belief::Fold` (`src/ECS/Town/TownBelief.cpp`), the town desire info's belief scale and decay; test `TownBeliefTest.FoldDesireCost` |
| Hurting a town lowers its belief in the player who did it (unconfirmed exact rule) | inferred | no belief loss for an attack was found in the exe: an attack records the town's aggressor (`town_emergency::UpdateAggressor`, `src/ECS/Effects/EffectValues.cpp`) and the attacker's later miracles impress that town no more (test `ImpressionTest.AShieldImpressesNothingTheTownItsPlayerAttacked`); whether the original also takes belief is unverified |
| The belief a player needs to win a town grows with the belief other players have there | done | a town goes to whichever player it believes in most, against its owner's own belief (`town_belief::Fold`, test `TownBeliefTest.FoldConversion`); see `../town/` |
| A town nearly lost makes the help sprites warn the player | done | `town_belief::CheckLosingBelief` every ten turns, not on Land 1; test `TownBeliefTest.LosingBelief` |
| A town's belief shows as the players' symbols over its centre | partial 80% | the players' belief symbols over each town centre, the first still over the totem, the others orbiting (`src/Particles/TownBelief.cpp`, [particles](../../../physics/particles.md)); the rivals' symbol images and the owner's spell column are not drawn; a scroll showing it was not found in the exe |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts set a town's starting belief in a player and its cap | done | `SET_TOWN_BELIEF` and `SET_TOWN_BELIEF_CAP` (`src/LHScriptX/FeatureScriptCommands.cpp`), read by the fold; test `TownBeliefTest.Scripts` |
| Land scripts set a town's belief scale | done | `SET_TOWN_BALANCE_BELIEF_SCALE` sets the town's `beliefScale` (`src/LHScriptX/FeatureScriptCommands.cpp`), which the fold multiplies by; test `TownBeliefTest.FoldPending` |
| Challenge scripts read a player's belief in an object and set it | partial 80% | ported from raffclar PR #236, unverified against v1.42: `BELIEF_FOR_PLAYER`, `SET_PLAYER_BELIEF` (`town_belief::SetBeliefRelativeToOwner`) and `OBJECT_RELATIVE_BELIEF` (`town_belief::PlayerTakeTownCheat`) in `src/CHLApi.cpp`; tests `TownBeliefTest.ScriptSetsBeliefAsAShareOfBeliefInTheOwner`, `TownBeliefTest.FoldConversionCapsTheClaimedBelief`; the natives are not read against v1.42 |
| Challenge scripts set a town's belief scale | pending | `SET_OBJECT_BELIEF_SCALE` is a stub in `src/CHLApi.cpp`; in the exe (`GScript::TownBalanceBeliefScale` 0x6FF840) it takes a town (an error and nothing for anything else) and sets the same belief scale (Town +0x5DC) as the land script's `SET_TOWN_BALANCE_BELIEF_SCALE`, not an object's impressiveness |
