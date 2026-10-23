# Lightning

A held miracle: while the player keeps casting, forked bolts leap from the hand to people, creatures, trees and
buildings in a cone ahead of the camera, setting them alight. Two power-ups add a wider reach, more forks and more
targets at once.

Given by silver scrolls: to a town by [The Greedy Farmer](../story/silver_scrolls/the_greedy_farmer.md) (with its second level) and
[The Plague](../story/silver_scrolls/the_plague.md), and as a dispenser by the evil ending of [The Pied Piper](../story/silver_scrolls/the_pied_piper.md).

**Progress: 22/43 done, 0 chosen, 18 partial, 2 inferred — 84%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/fireball-and-lightning.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Held miracle: bolts strike only while the cast is held; letting go ends it | done | `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the in-hand cast type: one apply a turn while held, the unlock on release) Scenarios `miracles.hand_held_miracles`, `miracles.lightning_crowd` |
| Cost to cast 5000 / 7500 / 10000, then 50 / 70 / 90 a turn and 2 a strike; a part-paid strike is weaker | done | `src/Magic/Core/Chants.cpp`; `test/test_spell_chants.cpp` |
| A bolt held longer than 6 s closes by itself | done | `src/Magic/Core/Spell.cpp` (the age against the duration); `test/test_spell_chants.cpp` |
| Two power-up levels: reach 60 / 100 / 140 m, cone half-angle widens, more forks and targets | done | `src/Particles/Rules/Lightning.cpp` (the effect files' radius, cone, fork and target counts) Test `LightningParticles.realData` (with the game's data); scenarios `miracles.lightning_crowd_pu1`, `miracles.lightning_crowd_pu2` |
| The bolts aim along the camera's look direction | partial 80% | `src/Particles/Rules/Lightning.cpp` (the heading is the angle of the camera's forward, as in the original) No test covers the heading; seen in the lightning scenarios only |

## Picking targets

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Searches a square of map cells twice the reach across, cell by cell outward, fixed objects before moving ones | done | `src/Particles/Rules/Lightning.cpp` (SearchAround, each cell's fixed list first), `src/Particles/LightningMaths.cpp` (`StrikeSearchCells`: a spiral of 4 ceil(R/10) squared cells); `test/test_lightning_maths.cpp` |
| Anything available is a target (people, creatures, animals, trees, buildings, fields, rocks…), but not miracle seeds or dying villagers | partial 80% | `src/Particles/Rules/Lightning.cpp` (IsAvailable, CanBeStruck) No test covers the target filter |
| Only objects inside the cone ahead are taken, up to 6 / 12 / 28 on the first search | partial 80% | `src/Particles/Rules/Lightning.cpp` (the SplitAngle cone, MaxLightningObjects) No test covers the cone or the cap |
| Missing targets are made up with points on the ground ahead, up to 3 / 8 / 15 | partial 80% | `src/Particles/Rules/Lightning.cpp` (AddGroundPoints) No test covers the ground points |
| A visible creature in the cone draws every fork to itself | done | `particles::maths::CreatureRetargets` (`src/Particles/LightningMaths.cpp`) at the end of the hand bolt's search (`src/Particles/Rules/Lightning.cpp`): when a found creature is less than half faded out of sight, every target, its ground point kept, strikes those creatures in turn; `test/test_lightning_maths.cpp`; scenario `miracles.lightning_draws_to_creature` |
| Targets are searched again every second, never more than the first count | partial 80% | `src/Particles/Rules/Lightning.cpp` (RenewSearchEvery, RenewTargetsOnMove; a search never adds more targets than there are forks) No test covers the renewal |
| Each step strikes a random share of the targets (at most 4 / 10 / 20 at once) | partial 80% | `src/Particles/Rules/Lightning.cpp` (UpdateForkStructure) No test covers the share |
| The forks are made once, two for each target, and are not rebuilt when the targets are searched again | done | `src/Particles/Rules/Lightning.cpp` (CreateForkStructure, made once), as the original ([page](../../../magic/fireball-and-lightning.md#lightning)); `test/test_lightning.cpp` |
| Land between the hand and a fork's split point stops that fork for the step | done | `src/Particles/Rules/Lightning.cpp` (the land ray cast, `src/3D/Implementations/LandIsland.cpp`); tests `LightningParticles.landCutsTheFork`, `test/test_land_raycast.cpp` |

## Damage and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A strike burns, crushes and hits everything within a metre of the tip, not just its target | done | `src/Magic/Core/SpellEvent.cpp`, `src/ECS/Effects/EffectValues.cpp` (the area effect at the tip, radius 1 m) Scenario `miracles.lightning_crowd` |
| Struck things catch fire through the fire system | done | `src/ECS/Effects/EffectValues.cpp` into `src/ECS/Fire/FireEffect.cpp`; see `../physics/` for fire spreading. Scenario `miracles.lightning_crowd` |
| Villagers flee the miracle (one reaction per miracle) | done | `src/ECS/Effects/Reactions.cpp`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`. Scenario `miracles.reactions_flee` |
| A heavy crush makes onlookers react to the crushed object | partial 80% | `src/ECS/Effects/EffectValues.cpp` (the crushed-object reaction) No test or check covers it |
| Each strike counts towards the caster's alignment and the town's grudge | partial 80% | `src/ECS/Effects/EffectValues.cpp` (`alignment::Update`, the town's aggressor), `src/ECS/Effects/Alignment.cpp`; see `../worship/` for alignment. No test covers the lightning's own alignment step; the alignment rules have theirs |
| Struck villagers are not shown as skeletons by the bolt itself | inferred | Nothing in our wiki's reading of the bolt makes a skeleton, but the villager's own struck and burnt look is not checked against the original |
| A bolt striking only a creature leaves nearby villagers in a valid state (no crash) | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp`. Scenario `miracles.lightning_creature` |
| Electric arcs crawl over struck objects; an object still struck is arced again after each search, at most 100 arcs alive | partial 80% | `src/Particles/ParticleArcRules.cpp` (UR_ObjectArcer, queued from `StrikeTarget` in `src/Particles/Rules/Lightning.cpp`), `src/Particles/LightningMaths.cpp` (the arc's maths); `test/test_object_arcer.cpp`, `test/test_lightning_maths.cpp` (pinned to the original's code). Only objects whose model is not animated get arcs ([page](../../../magic/fireball-and-lightning.md#lightning)) Scenarios `miracles.lightning_arcs`, `miracles.lightning_arcs_struck_objects`. Not done: a creature is never arced (its body is not in the mesh pack); the object's matrix is not proven to be the original's; the ribbon's look not compared on a screenshot ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |

## Shields and clashing bolts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spiritual shield stops a fork at its surface, costs the shield 10 / 20 / 30 a hit and sparks | partial 80% | `src/Particles/Rules/Lightning.cpp` (a shield test on each fork segment, an event to the shield's spell and a spark), `src/Particles/Rules/Shield.cpp`. No test or check covers the lightning's shield hit |
| Two hand-cast bolts pointing the same way meet at a clash point | done | `src/Particles/Rules/Lightning.cpp` (FindClash), `src/Particles/LightningMaths.cpp` (`ClashPoint`); tests `LightningParticles.twoBoltsClash` (`test/test_lightning.cpp`), `test/test_lightning_maths.cpp`; scenario `miracles.lightning_clash` |
| A clash glows at the meeting point, the older bolt carries on three times as thick, and both strike at double strength | partial 90% | `src/Particles/Rules/Lightning.cpp` (the common glow, the fork three times as thick, strength 2 to both spells); test `LightningParticles.twoBoltsClash`. The glow may come or go one turn late: openblack leaves the other bolt's particles for the next step ([wiki](../../../magic/fireball-and-lightning.md#openblack)) |

## Bolts not cast by hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bolt from a storm cloud searches a circle round the cloud, all kinds, no creature pull | done | `src/Particles/Rules/Lightning.cpp` (the parent mode: a circle, no cone); see `storm.md`. Scenario `miracles.storm_powerup` |
| A bolt given its targets strikes their ground positions within its reach | inferred | `src/Particles/Rules/Lightning.cpp` (TakeTargetsFromManager); which effect uses this path, and the manager mode, are unread in the original ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |
| A strike with no miracle behind it (scripts) applies the weather lightning's effect | partial 80% | `psys::lightning::StrikeTip` and `NoSpellTipValues` (`src/Particles/Rules/Lightning.h`) apply the weather lightning's effect values; tests `LightningParticles.noSpellTipTakesTheWeatherLightningRow`, `LightningParticles.noSpellTipMapPosition`, `LightningParticles.strikeTipChoosesTheSpellOrTheWeatherLightning`. Not traced: the fire it starts and which random stream it draws from ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing ball at the hand, larger per level | done | The effect files, `src/Particles/Rules/HandFollow.cpp`. Scenario `miracles.hand_extreme_lightning` |
| Forks drawn as textured chains, re-laid each step, thicker per level | done | `src/Particles/Rules/Lightning.cpp`, `src/Particles/Creators/Chain.cpp`, `src/Graphics/RendererChain.cpp`; tests `LightningParticles.chainSegmentUv`, `LightningParticles.chainSegmentUvRepeats` |
| Fork joints shrink with depth, jitter sideways and flicker in alpha | partial 80% | `src/Particles/Rules/Lightning.cpp` (the fork tree: scale by depth, sideways jitter, random alpha) No test covers the fork tree's scale, jitter or alpha |
| The trunk stays glued to the drawn hand between steps | done | `src/Particles/Rules/Lightning.cpp` (the hand draw offset on the trunk joints), `src/Particles/PSys.cpp`; tests `LightningParticles.handDrawOffset`, `LightningParticles.trunkJointsFollowTheHand` |
| Each hit lights the land with an animated light map | done | `src/Particles/Creators/LightMap.cpp`; test `LightningParticles.lightMapCreatorProperties` |
| No land or screen flash for the miracle (the storm's flash is separate) | done | Correctly absent: our wiki's reading of the bolt's step makes only the light map ([page](../../../magic/fireball-and-lightning.md#lightning)) |
| Flickering sprites around the held miracle in the hand, fading on release | partial 70% | `src/Magic/Hand/HandMagicFX.cpp` (the level's in-hand effect, from the data); scenario `miracles.hand_extreme_lightning`; not compared with the original |
| Effect on a holder of the miracle | partial 60% | `src/Worship/SpellSeedGraphic.cpp` (the seed's holder effect, from the data); no test or check, not compared with the original |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A crackle loop starts with the cast, a different one per level | partial 80% | `src/Particles/Rules/Lightning.cpp` (the bolt's sound started while the effect is enabled), `src/Audio/Services/SpellSounds.cpp`. No test covers the start; heard in the lightning scenarios |
| On release the crackle is let go and fades from that turn, silent in seven turns | partial 80% | `src/Audio/Services/SpellSounds.cpp` (release and fade step), `src/Particles/Rules/Sound.cpp`. No test covers the fade |
| The crackle sample's own loop points are honoured; a released loop plays out its tail | done | `src/Audio/Device/WaveBuffers.cpp`, `src/Audio/Device/Sound.h`. The audio golden tests (`test/audio/`) |
| No thunder for the miracle | done | Correctly absent: our wiki's reading of the bolt's step plays only its own crackle ([page](../../../magic/fireball-and-lightning.md#lightning)) |
| Closing by the 6 s timer stops the crackle the same way | partial 80% | The same close-down path (`src/Magic/Core/Spell.cpp`) no test or check covers it |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast lightning at a target | done | `CastLightningBolt` (`CreaturePlanActions.cpp`), `creature_mind::CastAt` with `CastStyle::Lightning`; see [../creature/creature_casting.md](../creature/creature_casting.md) Scenario `miracles.creature_casts_lightning` |
| A bolt in progress is kept in a saved game | pending | openblack has no game saving |
