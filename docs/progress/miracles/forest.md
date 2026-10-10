# Forest

A seed miracle: thrown onto the land it plants eighteen magic trees in a spiral, which grow while the caster pays for
them and wither away when the miracle ends. Sparkles, a light on the land and a flock of butterflies (or bats, for an
evil god) follow, and the caster's camera takes a short flight round the new forest. There is no power-up.

Given by gold scroll: [The Workshop](../story/gold_scrolls/the_workshop.md).

**Progress: 32/41 done, 0 partial — 78%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast with the forest gesture from its seed | done | Cast from its seed on press (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`); the seed comes from a worship icon or a one-shot orb. See `../gesture/` and `dispensers_and_seeds.md` |
| Only on land within influence, in a cell no building covers | done | `src/Magic/Spells/SpellForest.cpp` (CanCastAt: in the map, on land, no abode of the cell covers it, the cell's newest fixed object is no multi-cell building), with the cast rule's influence test (`src/Magic/CastRules.cpp`) |
| Not right beside an abode (measured from the abode's middle and its size) | done | `src/Magic/Spells/SpellForest.cpp` (NoAbodeCovers: an abode's 2D radius against the distance to its middle; fields never block) |
| It can't be cast at an object | done | `src/Magic/CastRules.cpp` (CanCastOn: no for the forest) |
| Cost 13000 to cast, then 5 a turn plus 1 for each tree | done | `src/Magic/Spells/SpellForest.cpp` (CalculateCostToMaintain: cost per turn plus one per tree); test `SpellForest.costToMaintain` |
| No power-up exists | done | The NATURE row has no power-up (`src/Magic/MagicTables.cpp`) |
| It can be recast only while it has trees left to make | done | `src/Magic/Spells/SpellForest.cpp` (HasEnoughChantsAndLifeForRecast: trees still to make); test `SpellForest.maxObjectsToCreate` |

## Planting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The seed falls, spinning, and plants every tree at once when it lands | done | The falling seed atom's land collision event (`src/Particles/PSys.cpp`) makes `src/Magic/Spells/SpellForest.cpp` (SpellEvent) plant the whole forest at once |
| Eighteen trees on a spiral from 2 to 11 m round the cast point | done | `src/Magic/Spells/SpellForest.cpp` (SpiralOffset); tests `SpellForest.spiral`, `SpellForest.mapCoords` |
| A spot a building covers is skipped, never retried; other trees don't block | done | `src/Magic/Spells/SpellForest.cpp` (ValidPlaceForTree: a spot is tested once; a tree is no multi-cell building) |
| The kind of tree comes from the ground at the centre: broadleaves, palms on sand or by water, conifers on snow | done | `src/Magic/Spells/SpellForest.cpp` (RandomTreeType, TerrainMaterial: the cast point's material, snow, and no block counts as deep water, so palms); test `SpellForest.realInfoDat` |
| Each tree faces a random way; trees further out grow smaller (down to half size) | done | `src/Magic/Spells/SpellForest.cpp` (CreateTree: a random angle, target scale 1 - 0.5 x distance / 11); test `SpellForest.targetScale` |
| Trees grow a little each turn up to their size | done | `src/Magic/Spells/SpellForest.cpp` (Process grows every tree 0.01 a turn while it can afford them) |
| They also grow as other trees do, faster with rain and good land | done | `src/ECS/Trees.cpp` (TreeGrowthAmount with the rain and the land's alignment, also for the miracle's trees); test `SpellForest.treeGrowthAmount` |
| The forest belongs to the caster | done | Each magic tree keeps the caster's player (`src/Magic/Objects/MagicTree.cpp`); the forest itself keeps none, as the original never reads it |
| A forest cast from a worship icon leaves its seed on the ground at the centre, rising with the tallest tree; the hand can pick it up and cast the trees left elsewhere | done | (added) `src/Magic/Core/SpellSeed.cpp` (DrawSpells, FollowsSpell, ValidForPlaceInHand), `src/Magic/Spells/SpellForest.cpp` (AdjustSpellSeedPos); tests `SpellForest.seedFollowsSpell`, `SpellForest.adjustSpellSeedPos` |

## Upkeep and withering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the caster can't afford all the trees, every tree withers each turn and goes at nothing | done | `src/Magic/Spells/SpellForest.cpp` (Process: fewer wanted than there are, every tree shrinks 0.05 a turn) |
| When the miracle ends every tree withers away within about two seconds; magic trees never stay | done | `src/Magic/Spells/SpellForest.cpp` (CloseDown drops the creator, so no tree is wanted and all shrink 0.05 a turn) |
| A forest left alone lasts two minutes | done | The timers from the NATURE row (`src/Magic/MagicTables.cpp`), aged in `src/Magic/Core/Spell.cpp`. Our wiki differs: 120 s is a one-shot miracle's time; a player's forest has no time limit and lasts while it is paid for ([page](../../bw1-notes/miracles.md#forest-magicspellsspellforest-magicobjectsmagictree-ecstrees)) |

## Effects on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle when it lands | todo | The look-at-nice-miracle reaction is started (`src/Magic/Core/SpellEvent.cpp`), but villagers have no handler for it (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Villagers stop to look at the magic trees | todo | Each magic tree starts its reaction (`src/Magic/Objects/MagicTree.cpp`), but villagers have no handler for it |
| Creatures take no notice of the magic trees' reaction | done | Creatures take no reactions at all in our tree (no creature handler in `src/ECS/Effects/Reactions.cpp`) |
| A burning magic tree stops drawing looks, and draws them again once put out | done | `src/Magic/Objects/MagicTree.cpp` (the fire start removes the tree's reaction, the fire end makes it again) |
| A magic tree gives a quarter of an ordinary tree's wood, scaled by tribal power | done | `src/Magic/Objects/MagicTree.cpp` (0.25 x tribal power), read by `src/ECS/Trees.cpp` (wood value, kept by the felled tree) |
| Shields don't stop the planting; it does no damage | done | The forest event pays and plants with no shield test; its effect values do no damage |

## The caster's camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the caster's camera flies the forest's camera path, placed at the forest | todo | The camera atom's creator is not ported, so the forest's camera path and the goddess are not played (our wiki: postponed) |
| It waits, then glides onto the path and follows it slowed down | todo | Not ported (no camera path from the forest effect) |
| The flight lasts one play of its animation and ends with the miracle | todo | Not ported |
| A movement key (only on a frame where it would move the camera) or gripping the land gives the camera back | todo | Not ported |
| The flight no longer locks the camera and the player's movement (user-reported defect) | n/a | There is no forest camera flight in our tree to lock anything |
| The hand stays shown during the flight | todo | No flight yet |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A trail of rising player-coloured blobs follows the falling seed | done | Particle data of the forest effect |
| An animated light lies on the land for the life of the effect | done | `src/Particles/Creators/LightMap.cpp` (the forest's light map) |
| At 3.5 s sparkles burst and keep rising, with spinning vortices | done | Particle data, hung from the camera atom, which exists even though it draws nothing |
| At 5.5 s five flocks of ten circle the forest on a squashed sphere | done | `src/Particles/Rules/Forest.cpp` (the forest path and good or evil creator), `src/Particles/Rules/Flock.cpp` (flocking); test `SpellForest.butterflyFlap` |
| Bats for an evil caster, butterflies otherwise | done | `src/Particles/Rules/Forest.cpp` (good or evil creator: butterflies or bats, flapping clips from `src/Particles/Creators/Mesh.cpp`) |
| The flocks fade in over 4 s and out at 12 to 14 s | done | Particle data |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound with the seed, cut when it lands | done | The particle sound rules (`src/Particles/Rules/Sound.cpp`) |
| A sound with the sparkles at 3.5 s | done | Particle data |
| A sound with the flocks at 5.5 s | done | Particle data |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast a forest | todo | Creatures cast no miracles in our tree |
| The forest and its trees are kept in a saved game | todo | No game save system |
