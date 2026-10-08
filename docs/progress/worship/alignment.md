# Alignment

How good or evil each player is, from -1 (evil) to 1 (good). Everything the player does to the world moves it, and the
world shows it back: the sky, the hand, the temple, the land and the music.

**Progress: 12/18 done, 2 partial — 72%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md).

## The value

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has an alignment from evil to good, starting neutral | done | `components::Alignment`, one per player outside the land's registry (`magic::players::AlignmentOf`, `src/Magic/Core/Players.cpp`), 0 at the start; `test/test_alignment_system.cpp` |
| A change comes through over the turns, at most so much a turn, and the rest is lost | done | the pending change, clamped, times the maximum change per turn, then cleared (`ecs::effects::alignment::ProcessForPlayer`, `src/ECS/Effects/Alignment.cpp`), in slot 3 of the turn (`src/Magic/MagicLoop.cpp`) |
| The further a player leans, the less a change the same way moves them and the more one the other way does | done | `ecs::effects::alignment::ScaleChange` (`src/ECS/Effects/Alignment.cpp`), used by every deed below |
| Alignment falls into bands (very good to very evil) that the visuals and music follow (unconfirmed bands) | done | the music picks its piece from seven alignment bands (`DiscreteAlignment`, `src/Audio/Services/GameMusic.cpp`); the sky and the temple's outside follow the value smoothly |
| Scripts set and read a player's alignment, with a cap on each change | done | `GET_ALIGNMENT` and `SET_ALIGNMENT` in `src/CHLApi.cpp`: SET adds the value, clamped, and a value outside -1..1 is refused |

## Deeds that move it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle moves its caster by what it reached: animals (kind or nasty), villagers, creatures, buildings, plants, fields, features and the land each by their own amount | done | `ecs::effects::alignment::Update` from `EffectValues::ApplyEffect` (`src/ECS/Effects/EffectValues.cpp`): the info's alignment table by the object's alignment type and the life it changed |
| Impressing villagers moves the player by the kind of thing they saw | todo | a villager's impression is not ported (TODO in `villager_reactions::AddReaction`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Planting a tree with the water miracle is good | done | `src/Magic/Spells/SpellWater.cpp` calls `alignment::UpdateForTree` for the tree it plants |
| Pulling up a tree or planting one by hand | done | uprooting with the hand is evil (`src/ECS/Systems/Implementations/HandHolding.cpp`), replanting good (`HandTrees.cpp`): `alignment::UpdateForTree` |
| Each villager death moves its killer by how it died (thrown, burnt, drowned, eaten …) | done | `alignment::UpdateForDeath` from `villager::VillagerDead` (`src/ECS/Villager/VillagerDeath.cpp`): the death reason table, doubled for a child |
| Throwing or squashing villagers and animals | partial | a villager the hand throws or drowns to death moves its owner by that reason (`src/ECS/LivingPhysics.cpp`, `src/ECS/VillagerDrowning.cpp`); an animal's death keeps no reason (`src/ECS/AnimalAI.cpp`) |
| Sacrificing at the worship site | todo | no sacrifice yet: a totem does not take a held object (`src/ECS/HeldApply.cpp`); see [prayer power](prayer_power.md) |
| Feeding and giving wood to villagers by hand | done | `alignment::UpdateForResource` from `src/ECS/ObjectResources.cpp`: the change in the town's desire, times the give or take multiplier |
| What disciples do moves their god (unconfirmed which jobs) | todo |  |
| What a creature does moves its own alignment, not its god's | partial | an effect a creature applies moves no alignment at all, neither its god's nor its own (TODO in `src/ECS/Effects/EffectValues.cpp`); see `../creature/` |

## What it shows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky turns dark or bright with the alignment of whoever holds the land at the camera | done | the alignment of the most influential player at the camera (`alignment::UpdateInterfaceAlignment`, `src/ECS/Effects/Alignment.cpp`), the sky steps towards it (`AlignmentSystem::Update`); `test_alignment_system`; see `../sky/` |
| Crops and trees grow faster on good land | done | the land's alignment (`alignment::LandAlignmentAt`) feeds the fields (`src/ECS/Fields.cpp`) and the trees (`src/ECS/Trees.cpp`) |
| Doves circle a good temple, bats an evil one | todo | the temple's alignment flock is not ported (`src/Worship/Citadel.cpp`; [our wiki](../../bw1-notes/magic.md#the-temples-outside-alignment-and-size)); see `../animal/birds.md` |

How the hand, the temple and the music change with alignment is in `../hand/`, `../temple/` and `../audio/`.
