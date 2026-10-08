# Lightning

A held miracle: while the player keeps casting, forked bolts leap from the hand to people, creatures, trees and
buildings in a cone ahead of the camera, setting them alight. Two power-ups add a wider reach, more forks and more
targets at once.

Given by silver scrolls: to a town by [The Greedy Farmer](../story/silver_scrolls/the_greedy_farmer.md) (with its second level) and
[The Plague](../story/silver_scrolls/the_plague.md), and as a dispenser by the evil ending of [The Pied Piper](../story/silver_scrolls/the_pied_piper.md).

**Progress: 36/43 done, 2 partial — 86%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Held miracle: bolts strike only while the cast is held; letting go ends it | done | `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the in-hand cast type: one apply a turn while held, the unlock on release) |
| Cost to cast 5000 / 7500 / 10000, then 50 / 70 / 90 a turn and 2 a strike; a part-paid strike is weaker | done | `src/Magic/Core/Chants.cpp`; `test/test_spell_chants.cpp` |
| A bolt held longer than 6 s closes by itself | done | `src/Magic/Core/Spell.cpp` (the age against the duration); `test/test_spell_chants.cpp` |
| Two power-up levels: reach 60 / 100 / 140 m, cone half-angle widens, more forks and targets | done | `src/Particles/Rules/Lightning.cpp` (the effect files' radius, cone, fork and target counts) |
| The bolts aim along the camera's look direction | done | `src/Particles/Rules/Lightning.cpp` (the heading is the angle of the camera's forward, as in the original) |

## Picking targets

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Searches a square of map cells twice the reach across, cell by cell outward, fixed objects before moving ones | done | `src/Particles/Rules/Lightning.cpp` (SearchAround: a spiral of 4 ceil(R/10) squared cells, each cell's fixed list first) |
| Anything available is a target (people, creatures, animals, trees, buildings, fields, rocks…), but not miracle seeds or dying villagers | done | `src/Particles/Rules/Lightning.cpp` (IsAvailable, CanBeStruck) |
| Only objects inside the cone ahead are taken, up to 6 / 12 / 28 on the first search | done | `src/Particles/Rules/Lightning.cpp` (the SplitAngle cone, MaxLightningObjects) |
| Missing targets are made up with points on the ground ahead, up to 3 / 8 / 15 | done | `src/Particles/Rules/Lightning.cpp` (AddGroundPoints) |
| A visible creature in the cone draws every fork to itself | todo | Not in our tree; our wiki describes no such pull (only a creature skip of unverified meaning, a TODO in `src/Particles/Rules/Lightning.cpp`) |
| Targets are searched again every second, never more than the first count | done | `src/Particles/Rules/Lightning.cpp` (RenewSearchEvery, RenewTargetsOnMove; a search never adds more targets than there are forks) |
| Each step strikes a random share of the targets (at most 4 / 10 / 20 at once) | done | `src/Particles/Rules/Lightning.cpp` (UpdateForkStructure) |
| Forks are rebuilt at every search, two for each target struck at once plus two | done | `src/Particles/Rules/Lightning.cpp` (CreateForkStructure, made once); `test/test_lightning.cpp`. Our wiki differs: the forks are made once, two for each target, and are not rebuilt when the targets are searched again ([page](../../bw1-notes/miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-particlesruleslightningcpp)) |
| Land between the hand and a fork's split point stops that fork for the step | done | `src/Particles/Rules/Lightning.cpp` (the land ray cast, `src/3D/Implementations/LandIsland.cpp`); tests `Lightning.landCutsTheFork`, `test/test_land_raycast.cpp` |

## Damage and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A strike burns, crushes and hits everything within a metre of the tip, not just its target | done | `src/Magic/Core/SpellEvent.cpp`, `src/ECS/Effects/EffectValues.cpp` (the area effect at the tip, radius 1 m) |
| Struck things catch fire through the fire system | done | `src/ECS/Effects/EffectValues.cpp` into `src/ECS/Fire/FireEffect.cpp`; see `../physics/` for fire spreading |
| Villagers flee the miracle (one reaction per miracle) | done | `src/ECS/Effects/Reactions.cpp`, `src/ECS/Systems/Implementations/VillagerReactions.cpp` |
| A heavy crush makes onlookers react to the crushed object | done | `src/ECS/Effects/EffectValues.cpp` (the crushed-object reaction) |
| Each strike counts towards the caster's alignment and the town's grudge | done | `src/ECS/Effects/EffectValues.cpp` (`alignment::Update`, the town's aggressor), `src/ECS/Effects/Alignment.cpp`; see `../worship/` for alignment |
| Struck villagers are not shown as skeletons by the bolt itself | done | Correctly absent |
| A bolt striking only a creature leaves nearby villagers in a valid state (no crash) | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp` |
| Electric arcs crawl over struck objects; an object still struck is arced again after each search, at most 100 arcs alive | todo | No arc rule in our tree; our wiki does not describe them |

## Shields and clashing bolts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spiritual shield stops a fork at its surface, costs the shield 10 / 20 / 30 a hit and sparks | done | `src/Particles/Rules/Lightning.cpp` (a shield test on each fork segment, an event to the shield's spell and a spark), `src/Particles/Rules/Shield.cpp` |
| Two hand-cast bolts pointing the same way meet at a clash point | done | `src/Particles/Rules/Lightning.cpp` (FindClash); test `Lightning.twoBoltsClash` (`test/test_lightning.cpp`) |
| A clash glows at the meeting point, the older bolt carries on three times as thick, and both strike at double strength | done | `src/Particles/Rules/Lightning.cpp` (the common glow, the fork three times as thick, strength 2 to both spells); test `Lightning.twoBoltsClash` |

## Bolts not cast by hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bolt from a storm cloud searches a circle round the cloud, all kinds, no creature pull | done | `src/Particles/Rules/Lightning.cpp` (the parent mode: a circle, no cone); see `storm.md` |
| A bolt given its targets strikes their ground positions within its reach | done | `src/Particles/Rules/Lightning.cpp` (TakeTargetsFromManager); which effect uses this path is (unconfirmed) |
| A strike with no miracle behind it (scripts) applies the weather lightning's effect | todo | A TODO in `src/Particles/Rules/Lightning.cpp`: without a spell the original's fixed effect values are not applied |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing ball at the hand, larger per level | done | The effect files, `src/Particles/Rules/HandFollow.cpp` |
| Forks drawn as textured chains, re-laid each step, thicker per level | done | `src/Particles/Rules/Lightning.cpp`, `src/Particles/Creators/Chain.cpp`, `src/Graphics/RendererChain.cpp`; test `Lightning.chainSegmentUv` |
| Fork joints shrink with depth, jitter sideways and flicker in alpha | done | `src/Particles/Rules/Lightning.cpp` (the fork tree: scale by depth, sideways jitter, random alpha) |
| The trunk stays glued to the drawn hand between steps | done | `src/Particles/Rules/Lightning.cpp` (the hand draw offset on the trunk joints), `src/Particles/PSys.cpp`; tests `Lightning.handDrawOffset`, `Lightning.trunkJointsFollowTheHand` |
| Each hit lights the land with an animated light map | done | `src/Particles/Creators/LightMap.cpp`; test `Lightning.lightMapCreatorProperties` |
| No land or screen flash for the miracle (the storm's flash is separate) | done | Correctly absent |
| Flickering sprites around the held miracle in the hand, fading on release | partial | `src/Magic/Hand/HandMagicFX.cpp` (the level's in-hand effect, from the data); not checked against the game |
| Effect on a holder of the miracle | partial | `src/Worship/SpellSeedGraphic.cpp` (the seed's holder effect, from the data); not checked against the game |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A crackle loop starts with the cast, a different one per level | done | `src/Particles/Rules/Lightning.cpp` (the bolt's sound started while the effect is enabled), `src/Audio/Services/SpellSounds.cpp` |
| On release the crackle is let go and fades from that turn, silent in seven turns | done | `src/Audio/Services/SpellSounds.cpp` (release and fade step), `src/Particles/Rules/Sound.cpp` |
| The crackle sample's own loop points are honoured; a released loop plays out its tail | done | `src/Audio/Device/WaveBuffers.cpp`, `src/Audio/Device/Sound.h` |
| No thunder for the miracle | done | Correctly absent |
| Closing by the 6 s timer stops the crackle the same way | done | The same close-down path (`src/Magic/Core/Spell.cpp`) |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast lightning at a target | todo | Creatures cast no world miracles in our tree; see `creature_spells.md` |
| A bolt in progress is kept in a saved game | todo | openblack has no game saving |
