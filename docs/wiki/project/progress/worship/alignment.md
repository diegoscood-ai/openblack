# Alignment

How good or evil each player is, from -1 (evil) to 1 (good). Everything the player does to the world moves it, and the
world shows it back: the sky, the hand, the temple, the land and the music.

**Progress: 13/20 done, 0 chosen, 6 partial, 0 inferred — 87%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../../magic/influence-and-alignment.md).

## The value

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each player has an alignment from evil to good, starting neutral | done | `components::Alignment`, one per player outside the land's registry (`magic::players::AlignmentOf`, `src/Magic/Core/Players.cpp`), 0 at the start; `test/test_alignment.cpp` |
| A change comes through over the turns, at most so much a turn, and the rest is lost | done | the pending change, clamped, times the maximum change per turn, then cleared (`ecs::effects::alignment::ProcessForPlayer`, `src/ECS/Effects/Alignment.cpp`), in slot 3 of the turn (`src/Magic/MagicLoop.cpp`) |
| The further a player leans, the less a change the same way moves them and the more one the other way does | done | `ecs::effects::alignment::ScaleChange` (`src/ECS/Effects/Alignment.cpp`), used by every deed below |
| Alignment falls into seven bands (very good to very evil) that the music follows | done | the music picks its piece from seven alignment bands (`DiscreteAlignment`, `src/Audio/GameMusic.cpp`, as the original's discrete alignment (0x414730, [music](../../../audio/music.md)); test `AlignmentMusic.DiscreteAlignmentAndTables`); the sky and the temple's outside follow the value smoothly |
| Scripts set and read a player's alignment, with a cap on each change | done | `GET_ALIGNMENT` and `SET_ALIGNMENT` in `src/CHLApi.cpp`: SET adds the value, clamped, and a value outside -1..1 is refused; matches the original (SET adds, out of range is refused, the player popped first, [the page](../../../magic/influence-and-alignment.md#player-alignment)); test `Alignment.scriptsReadTheAlignmentAndAddToItWithinMinusOneAndOne` (`test/test_alignment.cpp`) |
| A new game takes the alignment the player's profile kept, 0 without one | pending | the original's game init takes the profile's saved alignment (`GPlayer::LoadPlayerAlignment`, [the page](../../../magic/influence-and-alignment.md#player-alignment)); openblack has no profile and starts at 0 |
| The advisors remark on how the alignment moves each turn | done | `audio::guidance::UpdateAlignmentRemarks` from `alignment::ProcessForPlayer` (`src/ECS/Effects/Alignment.cpp`); test `GuidanceTest.AlignmentRemarks` |

## Deeds that move it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle moves its caster by what it reached: animals (kind or nasty), villagers, creatures, buildings, plants, fields, features and the land each by their own amount | done | `ecs::effects::alignment::Update` from `EffectValues::ApplyEffect` (`src/ECS/Effects/EffectValues.cpp`): the info's alignment table by the object's alignment type and the life it changed; test `CreatureSpellHooksTest.TheAlignmentReadsTheLifeAsTheKillLeftIt` |
| Impressing villagers moves the player by the kind of thing they saw | done | each impression moves its player's alignment by the reaction's own amount, times the town's want when the reaction looks to one (`effects::alignment::UpdateForImpression` from `villager_reactions::UpdateHowImpressed`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`); test `Impressiveness.AnImpressionMovesTheAlignmentByItsKindAndTheTownsDesire` |
| Planting a tree with the water miracle is good | partial 90% | `src/Magic/Spells/SpellWater.cpp` calls `alignment::UpdateForTree` for the tree it plants, weighed by the alignment as the original's tree change (0x4145A0); no test covers the water miracle's call |
| Pulling up a tree or planting one by hand | done | uprooting with the hand is evil (`src/ECS/Systems/Implementations/HandHolding.cpp`), replanting good (`HandTrees.cpp`): `alignment::UpdateForTree`; test `TreeReplant.WithoutAPlayerNeitherTheMimicryNorTheAlignment` (the replant's order) |
| Each villager death moves its killer by how it died (thrown, burnt, drowned, eaten …) | done | `alignment::UpdateForDeath` from `villager::VillagerDead` (`src/ECS/Villager/VillagerDeath.cpp`): the death reason table, doubled for a child; tests `VillagerDeathPure.AlignmentAndMourningTurns` and the death tests of `test/test_villager_death.cpp` |
| Throwing or squashing villagers and animals | partial 50% | a villager the hand throws or drowns to death moves its owner by that reason (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`, `src/ECS/VillagerDrowning.cpp`); an animal's death keeps no reason (`src/ECS/AnimalAI.cpp`); a squashed villager and an animal's death by the hand move nobody |
| Sacrificing at the worship site | partial 80% | ported from raffclar PR #551: a sacrificed villager dies with reason Sacrifice, which moves its owner by the death table (`villager::VillagerDead`, `alignment::UpdateForDeath`); an animal moves the sacrificing player as half a death (`src/ECS/AltarOffering.cpp`); see [prayer power](prayer_power.md); the villager's death with reason 7 is read in the exe ([the sacrifice](../../../magic/worship.md#a-sacrifice-at-the-altar)); the animal's half is not read there; test `AltarSacrifice.AnAnimalMovesItsSacrificersAlignmentHalfAsMuch` |
| Feeding and giving wood to villagers by hand | done | `alignment::UpdateForResource` from `src/ECS/ObjectResources.cpp`: the change in the town's desire, times the give multiplier for an amount above 0 and the take one otherwise, weighed by the alignment, into the pending change; the original's resource change ([buildings](../../../world/buildings.md)); test `Alignment.aResourceGivenOrTakenByHandMovesThePendingChangeByTheTownsDesire` (`test/test_alignment.cpp`) |
| Making a villager a disciple by hand moves the player by how much its town wants that work: good above the town info's upper bound, evil below its lower one | done | `disciple::AlignmentForMaking` and `alignment::UpdateForDisciple` on the disciple's first turn down (`src/ECS/Villager/VillagerDisciple.cpp`), as the original's `GAlignment::UpdateFromDisciple` 0x414600 (called only from the villager's landing) and the town's 0x741220 (read in the v1.42 bytes: above the upper bound +change, below the lower one −change, else 0; a NaN desire would give −change there, 0 in openblack); test `DiscipleRules.AlignmentFollowsWhatTheTownWants`. No other disciple deed moves the alignment in the exe |
| What a creature does moves its own alignment, not its god's | partial 40% | the creature's own alignment moves each turn by its plan (`effects::alignment::ProcessCreatures`, test `CreatureDeedAlignment.ThePlanOfACreatureMovesItsAlignmentEachTurn`; ported from raffclar PR #478, the original's two calls settled in [creature alignment](../../../magic/influence-and-alignment.md#creature-alignment)); an effect or miracle a creature applies moves no alignment yet (TODO in `src/ECS/Effects/EffectValues.cpp`); see `../creature/` |

## What it shows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky turns dark or bright with the alignment of whoever holds the land at the camera | done | the alignment of the most influential player at the camera (`alignment::UpdateInterfaceAlignment`, `src/ECS/Effects/Alignment.cpp`), the sky steps towards it (`AlignmentSystem::Update`); test `Alignment.withoutAnOverrideTheSkyFollowsTheCamera`; see `../sky/` |
| Crops and trees grow faster on good land | done | the land's alignment (`alignment::LandAlignmentAt`) feeds the fields (`src/ECS/Fields.cpp`) and the trees (`src/ECS/Trees.cpp`); tests in `test/test_fields.cpp` and `test/test_spell_forest.cpp` (the land alignment's share) |
| Doves circle a good temple, bats an evil one | done | every 100 turns a bird more or fewer about the temple, up to the heart's most by the alignment, the other kind gone at once (`src/Worship/Citadel.cpp`, `src/Animals/BirdRules.cpp`); `test/test_bird_rules.cpp`; [our wiki](../../../magic/temple-and-citadel.md#the-temples-doves-or-bats); see `../animal/birds.md` |

How the hand, the temple and the music change with alignment is in `../hand/`, `../temple/` and `../audio/`.
