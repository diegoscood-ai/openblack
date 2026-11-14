# Lightning

A held miracle: while the player keeps casting, forked bolts leap from the hand to people, creatures, trees and
buildings in a cone ahead of the camera, setting them alight. Two power-ups add a wider reach, more forks and more
targets at once.

Given by silver scrolls: to a town by [The Greedy Farmer](../story/silver_scrolls/the_greedy_farmer.md) (with its second level) and
[The Plague](../story/silver_scrolls/the_plague.md), and as a dispenser by the evil ending of [The Pied Piper](../story/silver_scrolls/the_pied_piper.md).

**Progress: 35/42 done, 0 chosen, 7 partial, 0 inferred — 95%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/fireball-and-lightning.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Held miracle: bolts strike only while the cast is held; letting go ends it | done | `src/ECS/Systems/Implementations/HandSpellSeed.cpp` (the in-hand cast type: one apply a turn while held, the unlock on release) Scenarios `miracles.hand_held_miracles`, `miracles.lightning_crowd` |
| Cost to cast 5000 / 7500 / 10000, then 50 / 70 / 90 a turn and 2 a strike; a part-paid strike is weaker | done | `src/Magic/Core/Chants.cpp`; `test/test_spell_chants.cpp` |
| A bolt held longer than 6 s closes by itself | done | `src/Magic/Core/Spell.cpp` (the age against the duration); `test/test_spell_chants.cpp` |
| Two power-up levels: reach 60 / 100 / 140 m, cone half-angle widens, more forks and targets | done | `src/Particles/Rules/Lightning.cpp` (the effect files' radius, cone, fork and target counts) Test `LightningParticles.realData` (with the game's data); scenarios `miracles.lightning_crowd_pu1`, `miracles.lightning_crowd_pu2` |
| The bolts aim along the camera's look direction | done | `src/Particles/Rules/Lightning.cpp` (the heading is the angle of the camera's forward, as in the original); test `test/test_lightning_miracle.cpp` `LightningMiracle.TheBoltsAimAlongTheCamerasForward` |

## Picking targets

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Searches a square of map cells twice the reach across, cell by cell outward, fixed objects before moving ones | done | `src/Particles/Rules/Lightning.cpp` (SearchAround, each cell's fixed list first), `src/Particles/LightningMaths.cpp` (`StrikeSearchCells`: a spiral of 4 ceil(R/10) squared cells); `test/test_lightning_maths.cpp` |
| Anything available is a target (people, creatures, animals, trees, buildings, fields, rocks…), but not miracle seeds or dying villagers | done | `src/Particles/Rules/Lightning.cpp` (IsAvailable, CanBeStruck); test `test/test_lightning_miracle.cpp` `LightningMiracle.AnythingAvailableIsATargetButNotASeedNorADyingVillager` |
| Only objects inside the cone ahead are taken, up to 6 / 12 / 28 on the first search | done | `src/Particles/Rules/Lightning.cpp` (the SplitAngle cone, MaxLightningObjects); tests `test/test_lightning_miracle.cpp` `LightningMiracle.OnlyObjectsInsideTheConeAreTaken`, `LightningMiracle.TheFirstSearchTakesAtMostSixObjects`, `LightningMiracle.realDataLevels` (the three levels' files) |
| Missing targets are made up with points on the ground ahead, up to 3 / 8 / 15 | done | `src/Particles/Rules/Lightning.cpp` (AddGroundPoints); tests `test/test_lightning_miracle.cpp` `LightningMiracle.MissingTargetsAreMadeUpWithGroundPointsAhead`, `LightningMiracle.realDataLevels` |
| A visible creature in the cone draws every fork to itself | done | `particles::maths::CreatureRetargets` (`src/Particles/LightningMaths.cpp`) at the end of the hand bolt's search (`src/Particles/Rules/Lightning.cpp`): when a found creature is less than half faded out of sight, every target, its ground point kept, strikes those creatures in turn; `test/test_lightning_maths.cpp`; scenario `miracles.lightning_draws_to_creature` |
| Targets are searched again every second, never more than the first count | done | `src/Particles/Rules/Lightning.cpp` (RenewSearchEvery, whose 1 s is the rule's own default in the original, set by no level's file; RenewTargetsOnMove; a search never adds more targets than there are forks); tests `test/test_lightning_miracle.cpp` `LightningMiracle.TargetsAreSearchedAgainEverySecondWithTheFirstForks`, `LightningMiracle.realDataLevels` |
| Each step strikes a random share of the targets (at most 4 / 10 / 20 at once) | done | `src/Particles/Rules/Lightning.cpp` (UpdateForkStructure); tests `test/test_lightning_miracle.cpp` `LightningMiracle.EachStepStrikesAShareOfTheTargets`, `LightningMiracle.realDataLevels` |
| The forks are made once, two for each target, and are not rebuilt when the targets are searched again | done | `src/Particles/Rules/Lightning.cpp` (CreateForkStructure, made once), as the original ([page](../../../magic/fireball-and-lightning.md#lightning)); `test/test_lightning.cpp` |
| Land between the hand and a fork's split point stops that fork for the step | done | `src/Particles/Rules/Lightning.cpp` (the land ray cast, `src/3D/Implementations/LandIsland.cpp`); tests `LightningParticles.landCutsTheFork`, `test/test_land_raycast.cpp` |

## Damage and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A strike burns, crushes and hits everything within a metre of the tip, not just its target | done | `src/Magic/Core/SpellEvent.cpp`, `src/ECS/Effects/EffectValues.cpp` (the area effect at the tip, radius 1 m) Scenario `miracles.lightning_crowd` |
| Struck things catch fire through the fire system | done | `src/ECS/Effects/EffectValues.cpp` into `src/ECS/Fire/FireEffect.cpp`; see `../physics/` for fire spreading. Scenario `miracles.lightning_crowd` |
| Villagers flee the miracle (one reaction per miracle) | done | `src/ECS/Effects/Reactions.cpp`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`. Scenario `miracles.reactions_flee` |
| A heavy crush makes onlookers react to the crushed object | partial 80% | `src/ECS/Effects/EffectValues.cpp` (the crushed-object reaction); the rule itself, a crush above 0.01 on a living thing, has test `test/test_villager_death.cpp` `VillagerDeathTest.EveryLivingThingAndNothingElseIsCrushedAboveAHundredth`. Not closed: a strike's own crush (0.0006 / 0.0011 / 0.0018 a level, [page](../../../magic/fireball-and-lightning.md#lightning)) is under that 0.01, and before they are applied the strike's values are scaled by the event's payment strength, the tribal power and the event's own strength (1, or 2 for a linked fork) ([spell core](../../../magic/spell-core.md#events-and-effects)); whether that ever lifts a bolt's crush above 0.01 (the payment strength's range, the lightning's tribal flags) is not established |
| Each strike counts towards the caster's alignment and the town's grudge | done | `src/ECS/Effects/EffectValues.cpp` (`alignment::Update`, the town's aggressor), `src/ECS/Effects/Alignment.cpp`; see `../worship/` for alignment. The base level's effect row from info.dat, at full strength, on a house of another player's town; test `test/test_lightning_miracle.cpp` `LightningMiracleStrike.realInfoDatStrike`, `LightningMiracleStrike.realInfoDatStrikeSynthetic` |
| A bolt striking only a creature leaves nearby villagers in a valid state (no crash) | done | `src/ECS/Systems/Implementations/VillagerReactions.cpp`. Scenario `miracles.lightning_creature` |
| Electric arcs crawl over struck objects; an object still struck is arced again after each search, at most 100 arcs alive | partial 80% | `src/Particles/ParticleArcRules.cpp` (UR_ObjectArcer, queued from `StrikeTarget` in `src/Particles/Rules/Lightning.cpp`), `src/Particles/LightningMaths.cpp` (the arc's maths); `test/test_object_arcer.cpp`, `test/test_lightning_maths.cpp` (pinned to the original's code). Only objects whose model is not animated get arcs ([page](../../../magic/fireball-and-lightning.md#lightning)) Scenarios `miracles.lightning_arcs`, `miracles.lightning_arcs_struck_objects`. Not done: a creature is never arced (its body is not in the mesh pack); the object's matrix is not proven to be the original's; the ribbon's look not compared on a screenshot ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |

## Shields and clashing bolts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spiritual shield stops a fork at its surface, costs the shield 10 / 20 / 30 a hit and sparks | done | `src/Particles/Rules/Lightning.cpp` (a shield test on each fork segment, an event to the shield's spell and a spark), `src/Particles/Rules/Shield.cpp`; tests `test/test_shield_miracle.cpp` `ShieldMiracleParticles.ALightningForkEndsWhereItEntersAShieldThatHolds` (the stop and the spark), `ShieldMiracleSpells.EachBlockedAttackDrainsTheShieldByTheAttackersCostUntilItRunsDryAndBreaks` (the shield pays the hit's strength times the attacker's cost); test `test/test_lightning_miracle.cpp` `LightningMiracle.realInfoDatShieldCost` (the bolt's cost per hit in info.dat) |
| Two hand-cast bolts pointing the same way meet at a clash point | done | `src/Particles/Rules/Lightning.cpp` (FindClash), `src/Particles/LightningMaths.cpp` (`ClashPoint`); tests `LightningParticles.twoBoltsClash` (`test/test_lightning.cpp`), `test/test_lightning_maths.cpp`; scenario `miracles.lightning_clash` |
| A clash glows at the meeting point, the older bolt carries on three times as thick, and both strike at double strength | partial 90% | `src/Particles/Rules/Lightning.cpp` (the common glow, the fork three times as thick, strength 2 to both spells); test `LightningParticles.twoBoltsClash`. The glow may come or go one turn late: openblack leaves the other bolt's particles for the next step ([wiki](../../../magic/fireball-and-lightning.md#openblack)) |

## Bolts not cast by hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bolt from a storm cloud searches a circle round the cloud, all kinds, no creature pull | done | `src/Particles/Rules/Lightning.cpp` (the parent mode: a circle, no cone); see `storm.md`. Scenario `miracles.storm_powerup` |
| A bolt given its targets strikes their ground positions within its reach | done | `src/Particles/Rules/Lightning.cpp` (TakeManagerTargets: every target taken out as a place, all within the radius kept, no cap; the ground points at a random angle); `particles::lightning::TakeTargetPlaces` / `PlacesInReach`, tests `LightningParticles.managerTakesEveryTargetAsAPlace`, `LightningParticles.managerKeepsEveryPlaceInReach`. Only the scripts' lightning strike (`SF_LightningSingleStrike`) takes this path ([wiki](../../../magic/fireball-and-lightning.md#fireball-and-lightning)) |
| A strike with no miracle behind it (scripts) applies the weather lightning's effect | partial 80% | `particles::lightning::StrikeTip` and `NoSpellTipValues` (`src/Particles/Rules/Lightning.h`) apply the weather lightning's effect values; tests `LightningParticles.noSpellTipTakesTheWeatherLightningRow`, `LightningParticles.noSpellTipMapPosition`, `LightningParticles.strikeTipChoosesTheSpellOrTheWeatherLightning`. Not traced: the fire it starts and which random stream it draws from ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Glowing ball at the hand, larger per level | done | The effect files, `src/Particles/Rules/HandFollow.cpp`. Scenario `miracles.hand_extreme_lightning` |
| Forks drawn as textured chains, re-laid each step, thicker per level | done | `src/Particles/Rules/Lightning.cpp`, `src/Particles/Creators/Chain.cpp`, `src/Graphics/RendererChain.cpp`; tests `LightningParticles.chainSegmentUv`, `LightningParticles.chainSegmentUvRepeats` |
| Fork joints shrink with depth, jitter sideways and flicker in alpha | done | `src/Particles/Rules/Lightning.cpp` (the fork tree: scale by depth, sideways jitter, random alpha); tests `test/test_lightning_miracle.cpp` `LightningMiracle.TheTrunkThinsFromHandToTipJitteredSidewaysAndFlickering`, `LightningMiracle.TheForksThinWithDepth` |
| The trunk stays glued to the drawn hand between steps | done | `src/Particles/Rules/Lightning.cpp` (the hand draw offset on the trunk joints), `src/Particles/ParticleEffect.cpp`; tests `LightningParticles.handDrawOffset`, `LightningParticles.trunkJointsFollowTheHand` |
| Each hit lights the land with an animated light map | done | `src/Particles/Creators/LightMap.cpp`; test `LightningParticles.lightMapCreatorProperties` |
| No land or screen flash for the miracle (the storm's flash is separate) | done | Correctly absent: our wiki's reading of the bolt's step makes only the light map ([page](../../../magic/fireball-and-lightning.md#lightning)) |
| Flickering sprites around the held miracle in the hand, fading on release | partial 70% | `src/Magic/Hand/HandMagicFX.cpp` (the level's in-hand effect, from the data); scenario `miracles.hand_extreme_lightning`; not compared with the original |
| Effect on a holder of the miracle | partial 60% | `src/Worship/SpellSeedGraphic.cpp` (the seed's holder effect, from the data). The start is pinned against the original's (its row's file, at the point raised by holder height times the size, at that size, on the local stream, the graphic its target): test `SeedHolderEffects.realInfoDatEachSeedShowsItsOwnHolderEffectAtItsHolderHeight` (`test/test_miracle_visuals.cpp`, with the game's info.dat); still open: its mists and meshes in the gesture icons ([gestures](../../../magic/gestures.md#seeds-the-help-and-the-addresses)) |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A crackle loop starts with the cast, a different one per level | done | `src/Particles/Rules/Sound.cpp` (StartStopSoundOnCondition on the bolt's root sprite: SOUND_SPELL_HAND_LIGHTNING, then its LEVEL_2 and LEVEL_3; no level's file sets the lightning rule's own SoundLightning), `src/Audio/Services/SpellSounds.cpp`; tests `test/test_lightning_miracle.cpp` `LightningMiracle.TheCrackleStartsWithTheCastAndFadesOutInSevenTurnsOnceLetGo`, `LightningMiracle.realDataLevels` |
| On release the crackle is let go and fades from that turn, silent in seven turns | done | `src/Audio/Services/SpellSounds.cpp` (release and fade step: FadeStep 20 a turn from full volume), `src/Particles/Rules/Sound.cpp`; test `test/test_lightning_miracle.cpp` `LightningMiracle.TheCrackleStartsWithTheCastAndFadesOutInSevenTurnsOnceLetGo` |
| The crackle sample's own loop points are honoured; a released loop plays out its tail | done | `src/Audio/Device/WaveBuffers.cpp`, `src/Audio/Device/Sound.h`. The audio golden tests (`test/audio/`) |
| No thunder for the miracle | done | Correctly absent: our wiki's reading of the bolt's step plays only its own crackle ([page](../../../magic/fireball-and-lightning.md#lightning)) |
| Closing by the 6 s timer stops the crackle the same way | done | The timer's close (`magic::ProcessMaintainRequest`) and the release (`seed::ClearSpellLink`) both reach the spell's one close-down, which closes its effect and so lets the crackle go, as in the original ([page](../../../magic/fireball-and-lightning.md#lightning)); test `test/test_magic_spells.cpp` `SpellBehaviours.ALightningBoltClosedByItsTimerClosesItsEffectAsALetGoOneDoes` |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast lightning at a target | done | `CastLightningBolt` (`CreaturePlanActions.cpp`), `creature_mind::CastAt` with `CastStyle::Lightning`; see [../creature/creature_casting.md](../creature/creature_casting.md) Scenario `miracles.creature_casts_lightning` |
| A bolt in progress is kept in a saved game | partial 30% | written: the running bolt's record from its entity, of the general spells' class (`src/ECS/SavedWorld/WriteSpells.cpp`; test `SpellWrite.ThePlainTheStormAndTheHealingSpellsWriteEverySpellsValuesOnly`), with the game's list of spells; missing: a spell whose effect has made its reaction still stops the write, a load makes the spell again with its effect (`saved_spells::MakeSpells`, `src/ECS/SavedWorld/LoadSpells.cpp`; test `SpellLoad.APlainSpellIsMadeAgainAndWrittenBackAsRead`), and the in-play round trip of 2026-11-14 (refs/story/evidence/save_rt_1114/README.md) held no general spell: still to be seen ([saved games](../../../engine/saved-games.md#writing-the-world-file)) |
