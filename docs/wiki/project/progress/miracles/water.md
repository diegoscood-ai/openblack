# Water

A held miracle: a small rain cloud follows the hand and drops water on the land below for a few seconds. It puts out
fires, sows and ripens fields, grows young trees and plants new ones in forests. The power-up ("extreme") version
rains over a wider cone and grows any tree past its normal size, but never plants.

Given by gold scroll: the monk's two water one-shot miracles in
[Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md#the-monk-helps), if his quest is done.

**Progress: 18/40 done, 0 chosen, 19 partial, 1 inferred — 82%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/food-wood-water-heal.md).

## Casting and lifetime

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rain is held: while the player keeps it, the drops fall under the hand and follow it each turn | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`); the spell's cast point follows the hand each turn (`src/Magic/Core/Spell.cpp`) and `src/Magic/Spells/SpellWater.cpp` drops under it. Scenarios `miracles.water_fields`, `miracles.water_fire` |
| It lasts 6 s for a player, 10 s for a computer player; a creature's lasts until it lets go | partial 70% | The player's 6 s timer is read from the tables (`src/Magic/MagicTables.cpp`; test `Water.realData`); a creature's cast is let go with its pose (`MagicSystemInterface::ReleaseCreatureCast`); the computer player's 10 s is not checked |
| It costs 5000 (7000 extreme) to cast and 10 prayer power per drop; nothing per turn | done | Cost to create and cost per drop from the WATER rows; nothing per turn (`src/Magic/Core/Chants.cpp`); test `Water.realData` |
| While the player casts, the hand bobs up 8 m and back over an 8 s loop, and stops when the rain ends | done | `src/Particles/Rules/Sprinkle.cpp` starts the hand raise, unclamped, `src/ECS/Systems/Implementations/HandGrain.cpp`; the close-down stops it (`src/Magic/Core/Spell.cpp`); tests `test/test_water_miracle.cpp` `WaterMiracle.TheHandBobsUpEightMetresAndBackOverAnEightSecondLoopWithoutBeingHeld`, `WaterMiracleSpell.TheBobStopsWhenTheRainOfAMiracleThisComputerCastEnds` |
| A scripted water miracle rains from a fixed point for nobody, with no hand bob | partial 80% | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS, the neutral player); the hand raise only starts for this computer's own cast (`src/Particles/Rules/Sprinkle.cpp`) No test or check covers the script cast; the test hook's cloud height for a script cast is inferred ([wiki](../../../magic/food-wood-water-heal.md#openblack)) |
| A creature can cast water from above, its hand raised over the target | partial 70% | `creature_mind::CastWaterAt` from a point above the target with a beam from each hand (`MagicSystem::CastFromAboveRaise`, `StartCreatureBeams`); see [../creature/creature_casting.md](../creature/creature_casting.md) Scenario `miracles.creature_waters_field`; not yet checked against the original (issue #172) |
| A creature judges whether a field or a growing tree would gain from water before watering it | pending | No creature reasoning about watering |
| The rain and the forests' shared planting delay are kept in a saved game | pending | No game save system |

## Drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One drop falls each turn at a random point of the cone under the cloud: 0.3–4.5 m out (0.3–8.7 m extreme) | done | `src/Magic/Spells/SpellWater.cpp` (Process: one drop a turn, radius 6 or 12 times 0.7 plus 0.3); test `Water.dropDistanceAndReach` |
| Each drop pays its 10 and soaks a 1 m circle at ground level | done | `src/Magic/Spells/SpellWater.cpp` (a point event with the WATER effect: burn -4000 within 1 m, cost per event 10; its result is not checked) Test `Water.realData` (the effect row) |
| A drop reaches every object in the nine cells around it whose edge lies within reach across the ground | partial 90% | `src/Magic/Spells/SpellWater.cpp` (nine cells, fixed then mobile lists, edge within 2.5 m, strict) Test `Water.dropDistanceAndReach`. The flat distance takes the exact root, not the original's table root (sub-millimetre differences; [wiki Pending](../../../magic/food-wood-water-heal.md#pending), issue #59) |
| A field counts as 5 m wide for the drop's reach | done | `ecs::object::GetRadius` (5 m for a field), used by `src/Magic/Spells/SpellWater.cpp`; test `Water.dropDistanceAndReach` |
| Water hurts nothing, changes no weather and raises no water level | done | The WATER effect values are only the cooling (`src/ECS/Effects/EffectValues.cpp`) Test `Water.realData` (the effect row: only the cooling) |
| The miracle's own drops don't count as rain, so a fireball in it doesn't steam from rain | partial 80% | The rain state comes only from the weather (`src/ECS/Weather/`) No test or check covers it |

## Putting out fires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A drop cools anything already burning; light things go out at once, heavy ones need several drops | done | `src/ECS/Fire/FireEffect.cpp` (the heat of the drop's burn -4000 cools an existing fire) Scenarios `miracles.water_fire`, `miracles.forest_fire` |
| A drop never sets anything alight or makes a fire where there was none | done | `src/ECS/Fire/FireEffect.cpp` (negative heat only acts on an existing fire); test `test/test_water_miracle.cpp` `WaterMiracleFire.ADropNeverSetsAnythingAlight` |
| A burning villager under the rain stops burning and goes back to what it was doing | partial 80% | The villager's own fire is cooled like any other (`src/ECS/Fire/FireEffect.cpp`), and `src/ECS/Systems/Implementations/VillagerFire.cpp` takes it back to work. No test or check covers it |
| Villagers near a fire the water puts out are impressed by it, but none of them comes to watch; one such reaction per miracle at a time | done | The spell starts the putting-out-fire reaction once (`src/Magic/Spells/SpellWater.cpp`); it is one of the reactions that only impress: each villager available for it is impressed and takes nothing up, and the first one in each map cell ends that cell (`ImpressOnly` in `src/ECS/Systems/Implementations/VillagerReactions.cpp`, `effects::reactions::CellWalk`); tests `VillagerReactionsTest.AFirePutOutByMagicWaterIsOnlyRecordedAndEndsTheCell`, `ReactionSpread.ALivingThatEndsItsCellLeavesTheRestOfTheCellOut`; scenario `miracles.water_fire_watchers`. See [villager reactions](../../../beings/villager-reactions.md#reactions-that-only-impress-34-39-and-40) |
| The water puts out a fireball rolling in it | inferred | The ball is not in the map cells, so the drops' area effect cannot reach it (our wiki infers the same, [page](../../../magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire)); see [fireball.md](fireball.md). If confirmed, the row reads "the water does not put out a fireball" |

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An unsown or half-sown field is sown fully by one drop | done | `src/Magic/Spells/SpellWater.cpp` (ApplyWaterSpell), `src/ECS/Fields.cpp` (ApplyWaterSpellToField) Scenario `miracles.water_fields` |
| A sown field ripens faster: each drop ages the crop and adds food (about 0.58 per drop) until harvest age | done | `src/ECS/Fields.cpp` (growth plus 2 and food plus 2 x 350 / 1200 per drop until 1200); test `Water.realData` |
| A burning field's crop is left as it is | done | `src/Magic/Spells/SpellWater.cpp` (a burning field is not sown or grown); test `test/test_water_miracle.cpp` `WaterMiracleField.ABurningFieldsCropIsLeftAsItIs` |
| The player's own creature may learn to water crops by watching each drop | done | `src/Magic/Spells/SpellWater.cpp` (`ecs::creature_mimic::Consider` per drop on a field, `src/ECS/CreatureMimic.cpp`); test `test/creature/test_creature_mimic.cpp` `CreatureMimicSiteTest.AFieldWateredByAPlayersMiracleIsTheirDeed` |

## Trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Normal water grows a young, still-growing tree a little with each drop, up to its full size | done | `src/ECS/Trees.cpp` (ApplyWaterSpell: the tree's water accelerator times its growth) Scenario `miracles.water_trees` |
| Extreme water grows any tree, even a full-grown one, past its normal size, ever more slowly up to about three times | done | `src/ECS/Trees.cpp` (with extreme water, the maximum is raised, slowing as the size nears 3) Scenario `miracles.water_trees` |
| A growing tree rustles with one of the tree-growth sounds | done | `src/ECS/Trees.cpp` (one of the nine tree-grow samples at the tree); test `test/test_spell_forest.cpp` `ForestSystemTest.waterSpellSoundIsOwnedByTheTree` |
| Normal water on a full-grown forest tree plants a new young tree nearby, at most once every 40 turns across the whole island; natural forest spread shares the same delay | done | `src/ECS/Trees.cpp` (more than 40 turns since the island's last new tree, shared with the forests' own spread) Scenarios `miracles.water_forest`, `miracles.water_magic_forest` |
| The new tree's spot is searched over up to 160 tries 5–9 m away, spots on water allowed | partial 80% | `src/ECS/Trees.cpp` (PlantTreeNear: 32 rings of 5 tries, 5 to 9 m; a water cell is always free) No test covers it |
| A spot is refused only where it hits a fixed object's own collision shape | partial 60% | `src/ECS/Trees.cpp` (IsFreeForTree tests the map cells' fixed collide data, `ecs::map_cells::CollideWithFixed`, not each object's own shape) |
| The new tree starts tiny and grows to 0.8–1.2 of its kind's size, turned at random | partial 80% | `src/ECS/Trees.cpp` (size 0.1, most 0.8 to 1.2, a random angle) No test covers it |
| Trees grown by the forest miracle plant their seedlings into that miracle's forest, which removes them with it | done | `src/ECS/Trees.cpp` (the sapling joins the parent's forest, which the forest miracle removes with it) Scenario `miracles.water_magic_forest` |
| Planting a tree counts as a good deed for the caster | done | `src/Magic/Spells/SpellWater.cpp` (`ecs::effects::alignment::UpdateForTree`); the player's statistic is only kept in multiplayer games, as in the original. Test `test/test_water_miracle.cpp` `WaterMiracleTree.ATreeTheRainPlantsIsAGoodDeedForTheCaster` |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pale blue mist cloud forms at the gesture point, kept within 58 m of the land; bigger when extreme | done | `src/Particles/Rules/Sprinkle.cpp` (at most 58 m above the land), `src/Particles/Creators/Mist.cpp`; test `Water.mistCreatorMakesTheCloudAtom` |
| A rain cone streams from the cloud to the land, wider and taller when extreme | done | `src/Particles/Creators/Mesh.cpp` (the animated textured rain cone) Test `Water.realData` (SF_Water run: cloud and cone) |
| Cloud and cone fade out over 1 s when the rain ends and are gone 2.2 s after | done | Particle data, run by `src/Particles/ParticleEffect.cpp`. Scenario `miracles.water_fire` |
| Every 0.1 s a ring spreads on the land or water where a drop fell, size 2 (4 extreme), in a random shade | done | `src/Magic/Spells/SpellWater.cpp` (AddDropRing into `src/3D/WaterRings.cpp`, colour from five shades); test `Water.ringTimingInSinglePrecision` |
| The held seed shows a mist and a sparkling trail on the hand (bigger when extreme) | partial 70% | The in-hand effect is the level's in-hand particles (`src/Magic/Hand/HandMagicFX.cpp`); our wiki notes it has not been checked in a screenshot. Scenario `miracles.hand_hold_poses` |
| A water seed on a dispenser shows its holder effect | partial 60% | The seed's holder particles (`src/Worship/SpellSeedGraphic.cpp`); not checked in a screenshot. See [dispensers_and_seeds.md](dispensers_and_seeds.md) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rain loop plays while the water falls and fades out when it stops | partial 80% | The particle sound rules (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`); no test covers it |
| There is no cast, drop, splash or end sound | partial 80% | Matches the particle data as read; not checked against the original's files one by one |
