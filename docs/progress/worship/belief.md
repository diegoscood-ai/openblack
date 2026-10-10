# Belief and impressiveness

How much a town believes in each player. Villagers who see something impressive, kind or terrible, believe more in the
god who did it; enough belief wins the town over (see `../town/` for conversion). Artefacts left in other players' towns win belief too: see [../town/artefacts.md](../town/artefacts.md).

**Progress: 11/25 done, 3 partial — 50%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## Being impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager who sees something impressive gives its town belief in whoever did it, by how impressive it was, the land's balance and the villager's share of the town | todo | a villager's impression is not ported (TODO in `villager_reactions::AddReaction`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`): today a town only gains belief from resources given to it |
| Closer watchers are more impressed, along an S curve of distance | todo | the impression is not ported |
| Food and wood impress by how much the town wants them | done | food or wood given to a town adds belief by how much its desire drops, more for a player that does not own it (`DoResourceAdding`, `src/ECS/ObjectResources.cpp` to `town_stores::AddToBelief`) |
| Things that belong to no player impress no one | todo | the impression is not ported |
| A town grows bored of seeing the same kind of thing; boredom wears off each town turn, times the land's lost-town scale | partial | the wear-off at each town turn, times the lost-town scale, is in `town_belief::Fold` (`src/ECS/Town/TownBelief.cpp`, test `TownBeliefTest.FoldBoredom`); nothing adds boredom yet, as impressions are not ported |
| Creatures are impressed by their own god's and other creatures' deeds | todo | no creature impression; see `../creature/` |
| Villagers hiding indoors are still impressed by what they see | todo | the belief-only branch of the reaction spread is not ported (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Artefacts given to a town keep impressing it | todo |  |
| Throwing things into a town impresses it (and frightens it) | todo | see `../hand/` |
| The creature's impressive actions win belief for its god | todo | see `../creature/` |

## A town's belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Belief gained waits until the town's turn, then is believed at once, times the town's belief scale | done | `town_belief::Fold` from the town's process (`src/ECS/Town/TownProcess.cpp`), times the town's `beliefScale`; test `TownBeliefTest.FoldPending` |
| Belief in a player is capped (10 unless a script sets another cap) | done | `town_belief::SetBelief` caps to the town's cap, 10 to start (`SET_TOWN_BELIEF_CAP` changes it); test `TownBeliefTest.SetBeliefClampsToCapOnly` |
| A symbol of the belief gained rises from the town centre in the player's colour | partial | the fold queues a belief sprite at the town centre in the player's colour (`town_belief::QueueBeliefSprite`) and the belief sprite effect is stepped (`src/Particles/Utility.cpp`), but nothing draws the queue |
| Each town keeps a fading tally of the belief each player has lately won | done | `recent` decays at each fold (test `TownBeliefTest.FoldRecentDecay`); the belief added in the period is cleared every ten turns (`town_belief::ProcessOncePerTurn`) |
| Once claimed, a town's belief is reset by the claimed-town multiplier | done | the new owner's belief times the claimed-town multiplier (`town_belief::Fold`; test `TownBeliefTest.FoldConversion`) |
| Losing a town lowers belief in that player elsewhere | done | the old owner's other towns times the lost-town multiplier and the lost-town scale (`town_belief::Fold`) |
| A town's unmet desires cost its owner belief at each town turn, and the threshold for it falls back over time | done | (added) `town_belief::Fold` (`src/ECS/Town/TownBelief.cpp`), the town desire info's belief scale and decay; test `TownBeliefTest.FoldDesireCost` |
| Hurting a town lowers its belief in the player who did it (unconfirmed exact rule) | todo | a destructive effect only records the town's aggressor (`town_emergency::UpdateAggressor`, `src/ECS/Effects/EffectValues.cpp`); no belief is taken for it |
| The belief a player needs to win a town grows with the belief other players have there | done | a town goes to whichever player it believes in most, against its owner's own belief (`town_belief::Fold`, test `TownBeliefTest.FoldConversion`); see `../town/` |
| A town nearly lost makes the help sprites warn the player | done | `town_belief::CheckLosingBelief` every ten turns, not on Land 1; test `TownBeliefTest.LosingBelief` |
| A town's belief shows on its scroll and the town's status | partial | the players' belief symbols over each town centre (`src/Particles/TownBelief.cpp`); no scroll shows it; see `../interface/` |

## Scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts set a town's starting belief in a player and its cap | done | `SET_TOWN_BELIEF` and `SET_TOWN_BELIEF_CAP` (`src/LHScriptX/FeatureScriptCommands.cpp`), read by the fold; test `TownBeliefTest.Scripts` |
| Land scripts set a town's belief scale | done | `SET_TOWN_BALANCE_BELIEF_SCALE` sets the town's `beliefScale` (`src/LHScriptX/FeatureScriptCommands.cpp`), which the fold multiplies by |
| Challenge scripts read a player's belief in an object and set it | todo | `BELIEF_FOR_PLAYER`, `SET_PLAYER_BELIEF` and `OBJECT_RELATIVE_BELIEF` are stubs in `src/CHLApi.cpp` |
| Challenge scripts scale how impressive an object is | todo | `SET_OBJECT_BELIEF_SCALE` is a stub in `src/CHLApi.cpp` |
