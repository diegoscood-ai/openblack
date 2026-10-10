# Water

A held miracle: a small rain cloud follows the hand and drops water on the land below for a few seconds. It puts out
fires, sows and ripens fields, grows young trees and plants new ones in forests. The power-up ("extreme") version
rains over a wider cone and grows any tree past its normal size, but never plants.

Given by gold scroll: the monk's two water one-shot miracles in
[Fire! Fire! I'm on Fire!](../story/gold_scrolls/fire_fire_im_on_fire.md#the-monk-helps), if his quest is done.

**Progress: 32/41 done, 6 partial — 85%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and lifetime

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rain is held: while the player keeps it, the drops fall under the hand and follow it each turn | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`); the spell's cast point follows the hand each turn (`src/Magic/Core/Spell.cpp`) and `src/Magic/Spells/SpellWater.cpp` drops under it |
| It lasts 6 s for a player, 10 s for a computer player; a creature's lasts until it lets go | partial | The player's 6 s timer is read from the tables (`src/Magic/MagicTables.cpp`; test `Water.realData`); there are no computer players, and creatures cast nothing |
| It costs 5000 (7000 extreme) to cast and 10 prayer power per drop; nothing per turn | done | Cost to create and cost per drop from the WATER rows; nothing per turn (`src/Magic/Core/Chants.cpp`); test `Water.realData` |
| While the player casts, the hand bobs up 8 m and back over an 8 s loop, and stops when the rain ends | done | `src/Particles/Rules/Sprinkle.cpp` starts the hand raise, unclamped, `src/ECS/Systems/Implementations/HandGrain.cpp`; the close-down stops it (`src/Magic/Core/Spell.cpp`) |
| A scripted water miracle rains from a fixed point for nobody, with no hand bob | done | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS, the neutral player); the hand raise only starts for this computer's own cast (`src/Particles/Rules/Sprinkle.cpp`) |
| A creature can cast water from above, its hand raised over the target | todo | Creatures cast no miracles in our tree (nothing in `src/Creature/` casts a spell) |
| A creature judges whether a field or a growing tree would gain from water before watering it | todo | No creature reasoning about watering |
| The rain and the forests' shared planting delay are kept in a saved game | todo | No game save system |

## Drops

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One drop falls each turn at a random point of the cone under the cloud: 0.3–4.5 m out (0.3–8.7 m extreme) | done | `src/Magic/Spells/SpellWater.cpp` (Process: one drop a turn, radius 6 or 12 times 0.7 plus 0.3); test `Water.dropDistanceAndReach` |
| Each drop pays its 10 and soaks a 1 m circle at ground level | done | `src/Magic/Spells/SpellWater.cpp` (a point event with the WATER effect: burn -4000 within 1 m, cost per event 10; its result is not checked) |
| A drop reaches every object in the nine cells around it whose edge lies within reach across the ground | done | `src/Magic/Spells/SpellWater.cpp` (nine cells, fixed then mobile lists, edge within 2.5 m, strict) |
| A field counts as 5 m wide for the drop's reach | done | `ecs::object::GetRadius` (5 m for a field), used by `src/Magic/Spells/SpellWater.cpp`; test `Water.dropDistanceAndReach` |
| Water hurts nothing, changes no weather and raises no water level | done | The WATER effect values are only the cooling (`src/ECS/Effects/EffectValues.cpp`) |
| The miracle's own drops don't count as rain, so a fireball in it doesn't steam from rain | done | The rain state comes only from the weather (`src/ECS/Weather/`) |

## Putting out fires

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A drop cools anything already burning; light things go out at once, heavy ones need several drops | done | `src/ECS/Fire/FireEffect.cpp` (the heat of the drop's burn -4000 cools an existing fire) |
| A drop never sets anything alight or makes a fire where there was none | done | `src/ECS/Fire/FireEffect.cpp` (negative heat only acts on an existing fire) |
| A burning villager under the rain stops burning and goes back to what it was doing | done | The villager's own fire is cooled like any other (`src/ECS/Fire/FireEffect.cpp`), and `src/ECS/Systems/Implementations/VillagerFire.cpp` takes it back to work |
| People nearby come to watch the water put a fire out, one reaction per miracle at a time | partial | The spell starts the putting-out-fire reaction once (`src/Magic/Spells/SpellWater.cpp`), but villagers have no handler for it (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| The water puts out a fireball rolling in it | done | The ball's fire is cooled (`src/ECS/Fire/FireEffect.cpp`) and the ball goes below its deletion temperature (`src/Magic/Objects/MagicFireBall.cpp`) |

## Fields

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An unsown or half-sown field is sown fully by one drop | done | `src/Magic/Spells/SpellWater.cpp` (ApplyWaterSpell), `src/ECS/Fields.cpp` (ApplyWaterSpellToField) |
| A sown field ripens faster: each drop ages the crop and adds food (about 0.58 per drop) until harvest age | done | `src/ECS/Fields.cpp` (growth plus 2 and food plus 2 x 350 / 1200 per drop until 1200); test `Water.realData` |
| A burning field's crop is left as it is | done | `src/Magic/Spells/SpellWater.cpp` (a burning field is not sown or grown) |
| The player's own creature may learn to water crops by watching each drop | done | `src/Magic/Spells/SpellWater.cpp` (`ecs::creature_mimic::Consider` per drop on a field, `src/ECS/CreatureMimic.cpp`) |

## Trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Normal water grows a young, still-growing tree a little with each drop, up to its full size | done | `src/ECS/Trees.cpp` (ApplyWaterSpell: the tree's water accelerator times its growth) |
| Extreme water grows any tree, even a full-grown one, past its normal size, ever more slowly up to about three times | done | `src/ECS/Trees.cpp` (with extreme water, the maximum is raised, slowing as the size nears 3) |
| A growing tree rustles with one of the tree-growth sounds | done | `src/ECS/Trees.cpp` (one of the nine tree-grow samples at the tree) |
| Normal water on a full-grown forest tree plants a new young tree nearby, at most once every 40 turns across the whole island; natural forest spread shares the same delay | done | `src/ECS/Trees.cpp` (more than 40 turns since the island's last new tree, shared with the forests' own spread) |
| The new tree's spot is searched over up to 160 tries 5–9 m away, spots on water allowed | done | `src/ECS/Trees.cpp` (PlantTreeNear: 32 rings of 5 tries, 5 to 9 m; a water cell is always free) |
| A spot is refused only where it hits a fixed object's own collision shape | partial | `src/ECS/Trees.cpp` (IsFreeForTree tests the map cells' fixed collide data, `ecs::map_cells::CollideWithFixed`, not each object's own shape) |
| The new tree starts tiny and grows to 0.8–1.2 of its kind's size, turned at random | done | `src/ECS/Trees.cpp` (size 0.1, most 0.8 to 1.2, a random angle) |
| Trees grown by the forest miracle plant their seedlings into that miracle's forest, which removes them with it | done | `src/ECS/Trees.cpp` (the sapling joins the parent's forest, which the forest miracle removes with it) |
| Planting a tree counts as a good deed for the caster | done | `src/Magic/Spells/SpellWater.cpp` (`ecs::effects::alignment::UpdateForTree`); the player's statistic is only kept in multiplayer games, as in the original |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pale blue mist cloud forms at the gesture point, kept within 58 m of the land; bigger when extreme | done | `src/Particles/Rules/Sprinkle.cpp` (at most 58 m above the land), `src/Particles/Creators/Mist.cpp`; test `Water.mistCreatorMakesTheCloudAtom` |
| A rain cone streams from the cloud to the land, wider and taller when extreme | done | `src/Particles/Creators/Mesh.cpp` (the animated textured rain cone) |
| Cloud and cone fade out over 1 s when the rain ends and are gone 2.2 s after | done | Particle data, run by `src/Particles/PSys.cpp` |
| Every 0.1 s a ring spreads on the land or water where a drop fell, size 2 (4 extreme), in a random shade | done | `src/Magic/Spells/SpellWater.cpp` (AddDropRing into `src/ECS/WaterRings.cpp`, colour from five shades); test `Water.ringTimingInSinglePrecision` |
| Old rings keep drifting with the wind the way a reused ring slot did in the game | partial | `src/ECS/WaterRings.h`: the pool is a list, not the original's slots, so the drift is not reproduced |
| The held seed shows a mist and a sparkling trail on the hand (bigger when extreme) | partial | The in-hand effect is the level's in-hand particles (`src/Magic/Hand/HandMagicFX.cpp`); our wiki notes it has not been checked in a screenshot |
| A water seed on a dispenser shows its holder effect | partial | The seed's holder particles (`src/Worship/SpellSeedGraphic.cpp`); not checked in a screenshot. See [dispensers_and_seeds.md](dispensers_and_seeds.md) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rain loop plays while the water falls and fades out when it stops | done | The particle sound rules (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds`) |
| There is no cast, drop, splash or end sound | done | Matches the particle data |
