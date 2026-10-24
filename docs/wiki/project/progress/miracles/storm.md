# Storm

The storm miracle throws a swirl from the hand that gathers a ring of dark clouds over the land, bringing rain and
wind that drift the storm with the weather. The extreme (power-up) storm adds lightning from the clouds; the second
power-up is the tornado (see [tornado.md](tornado.md)). Weather outside miracles is in [../weather/](../weather/).

**Progress: 25/37 done, 0 chosen, 11 partial, 0 inferred — 90%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/storm-and-tornado.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cast radius is held between 20 m and 1000 m and sets the storm's size | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (InitWithPos); test `Storm.radiusClampAndUpkeep` (`test/test_storm.cpp`) |
| The normal storm has clouds, rain and wind only; the extreme storm adds lightning, the second power-up makes a tornado | done | `src/Particles/Rules/Storm.cpp` (the power-up level: lightning unless it is the plain storm, the tornado group with the second power-up) Scenarios `miracles.storm_village`, `miracles.storm_powerup`, `miracles.tornado_village`. The level each seed stores (minus one, nought, one) is still to be read in the oracle ([wiki Pending](../../../magic/storm-and-tornado.md#pending)); the behaviour seen follows the seed |
| Cost to create, lifetime (40 s) and per-strike cost differ between normal and extreme | done | `src/Magic/MagicTables.cpp`, `src/Magic/Core/Chants.cpp` (info.dat rows) |
| Upkeep each turn grows with the square of the radius over the normal-cost radius of 40 m | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (CostToMaintain); test `Storm.radiusClampAndUpkeep` |
| In the hand: a small dark cloud ball with flicks of lightning sized by the hand and the seed's strength; each flash lasts half a second, flashes come at randomised gaps | done | `src/Magic/Hand/HandMagicFX.cpp` (the level's in-hand effect, from the data) Scenario `miracles.storm_inhand_flash` |

## The swirl at the cast point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A swirl of 30 dark sprites spins tighter and faster, then disperses after 2.4 s and fades 2 s later | done | `src/Particles/Rules/Storm.cpp` (UR_StormCast); test `Storm.realData` (the whirlwind alone) |
| The swirl is thrown along the camera's level heading at up to 20 m/s, speeding up between 1 s and 3 s; cast straight down it stays where it is | done | `src/Particles/Rules/Storm.cpp` (UR_StormCast: the camera's level heading, not normalised, so straight down it stays) Scenario `miracles.storm_village` |
| The swirl runs only while the miracle's caster exists and is removed when the caster goes | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (Process: the whirlwind is deleted without a creator); tests `test/test_storm_miracle.cpp` `StormMiracleSwirl.TheSwirlRunsWhileTheMiracleHasACaster`, `StormMiracleSwirl.TheSwirlIsRemovedWhenTheMiracleHasNoCaster`, `StormMiracleSwirl.AnEndedSwirlIsRemoved` |

## Clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clouds are born over about 10 s until the storm has its full number, each starting on a random ring around the centre | done | `src/Particles/Rules/Storm.cpp` (UR_CloudGather); test `Storm.cloudRamps` |
| Each cloud orbits the centre while it gathers, slowing and closing in as it forms, and is reborn when it has lived its gathering time | done | `src/Particles/Rules/Storm.cpp` (UR_CloudGather: the spiral in, the angular speed) Scenarios `miracles.storm_village`, `miracles.storm_clouds_below` |
| A cloud's grey shade, transparency and size follow its gathering | done | `src/Particles/Rules/Storm.cpp` (colour, alpha, scale and ratio ramps); test `Storm.cloudRamps` |
| Clouds float about 90 m up, following the average height of the land under the storm | done | `src/Particles/Rules/Storm.cpp` (CloudHeight plus the average land height under the storm) Scenario `miracles.storm_drift_slope` |
| Clouds darken the land below them as a moving shadow | partial 80% | `src/Particles/Creators/Mist.cpp` (the clouds' shadow stamped on the land) Seen in the game (dark blotches moving with the clouds); how dark they are has not been compared with the original side by side ([wiki](../../../magic/storm-and-tornado.md#openblack)) |
| When the storm ends the clouds fade out over about 3 s | done | The effect file's fade and removal rules, run by `src/Particles/Rules/Storm.cpp`; scenario `miracles.storm_village` |

## Rain and wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storm lays a rain storm in the weather with inner and outer radii from its size, fading in over half its gathering time, overcast and rain set from the storm | done | `src/Particles/Rules/Storm.cpp`, `src/ECS/Weather/Storms.cpp`; tests `Storm.gatherDescriptorRadii`, `Storm.gatherDescriptorWindAndRain`; see [../weather/](../weather/) |
| The rain stops at once when the storm ends | done | `src/Particles/Rules/Storm.cpp` (the registered storm is marked on closing), `src/ECS/Weather/Storms.cpp`; test `Storm.gatherRegistersOneStorm` |
| The storm blows wind along the direction it was thrown, stronger for bigger storms, rounded as the game rounds | done | `src/Particles/Rules/Storm.cpp` (the wind byte, clipped and rounded); test `Storm.gatherDescriptorWindAndRain` |
| After a short delay the storm drifts with the local wind, smoothly and with no slowing on slopes | done | `src/Particles/Rules/Storm.cpp` (UR_CloudMoverNew: the slope changes nothing, as in the original) Scenario `miracles.storm_drift_slope` (logged) |
| A storm cast by a script takes no wind and stays put | done | `src/Particles/Rules/Storm.cpp` (no wind for a script cast) Scenario `miracles.storm_script` |
| Rain and wind ambience rise and fall with the weather at the camera | partial 80% | `src/Audio/SoundMap.cpp`, `src/Audio/Services/AtmosBanks.cpp`; no test or check covers the storm's own ambience (the audio scheduler scenarios cover the banks, not a storm) |
| The rain cools and puts out fires under the storm, and villagers near a fire it puts out are impressed by it, none of them coming to watch (one such reaction per storm at a time) | done | `src/ECS/Fire/FireEffect.cpp` (rain cooling), `src/Magic/Spells/SpellStormAndTornado.cpp` (the reaction, one per storm; scenario `miracles.storm_village`, a fire under the storm), taken by the villagers as one that only impresses (`src/ECS/Systems/Implementations/VillagerReactions.cpp`, [villager reactions](../../../beings/villager-reactions.md#reactions-that-only-impress-34-39-and-40)) |
| The wind drifts fire sprites and smoke | done | `src/ECS/Fire/FireGraphic.cpp` (the puffs drift with half the wind, as the original's (wind x 0.5 - v) x 0.1 x dt, [fire](../../../world/fire.md#steam-and-smoke)); test `test/test_fire.cpp` `FireGraphic.SteamAndSmokeDriftWithHalfTheWindAndRise` |
| A script can end every storm in an area; a miracle storm caught in it comes back on the next step and fades in again | partial 80% | `src/Magic/Script/CHLWeather.cpp` (KILL_STORMS_IN_AREA), `src/Particles/Rules/Storm.cpp` (the next step registers another) No test or check covers it |

## Lightning (extreme storm)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first strikes come about 9.5 to 11 s after the cast | done | `src/Particles/Rules/Storm.cpp` (the first strike at rand(0.5, 1) times SwitchLife plus LightningDelay) Scenario `miracles.storm_powerup` |
| Strikes come from clouds that are more than half gathered, at random waits shortened by tribal power | partial 90% | `src/Particles/Rules/Storm.cpp` has the rule (scenario `miracles.storm_powerup`), and `PlayerMagic::tribalPower` is 1 for every tribe until a wonder of that tribe is built for the caster's player, and rises by each wonder's power (`wonders::AddToPlayer`, copied each turn by `magic::players::ProcessTribalPowers`, as `Wonder::AddToPlayer` 0x778FC0 and `GPlayer::Process`); not yet seen in a run with a built wonder; see [prayer_cost.md](prayer_cost.md) |
| A striking cloud flashes bright for half a second | done | `src/Particles/Rules/Storm.cpp` (the cloud's specular for half a second), `src/Particles/Creators/Mist.cpp`; test `Storm.cloudMistAtoms` |
| A bolt hits trees, buildings and other things below within its reach, or the ground when there are too few | done | `src/Particles/Rules/Lightning.cpp` (the parent mode); see [lightning.md](lightning.md) |
| A bolt carried from one cloud to the next keeps what it was striking | done | `src/Particles/Rules/Storm.cpp` (`storm::HandOverLightning`: the bolt's collection moves whole to the new cloud, from the old one or from the gather between strikes, [page](../../../magic/storm-and-tornado.md#the-cores-and-the-clouds)); test `test/test_storm.cpp` `Storm.aBoltCarriedToTheNextCloudKeepsWhatItWasStriking` |
| Struck things catch fire and take damage, and each strike costs prayer power | done | `src/Magic/Core/SpellEvent.cpp`, `src/ECS/Effects/EffectValues.cpp`. Scenario `miracles.storm_powerup` (a wood struck) |
| Thunder (small, medium or large at random) sounds from the land under the striking cloud, delayed by its distance at the speed of sound | done | `src/Particles/Rules/Storm.cpp` (`storm::Thunder`: the random size, delayed by the distance and set on the ground, [page](../../../magic/storm-and-tornado.md#the-cores-and-the-clouds)), `src/Audio/Services/SpellSounds.cpp` (`spell_sounds::DelayFor`: distance / 347); tests `test/test_storm.cpp` `Storm.aStrikesThunderIsSmallMediumOrLargeAtRandomAndDelayedFromTheGround`, `test/audio/test_spell_sounds.cpp` `SpellSounds.aDelayedSoundWaitsItsDistanceAtTheSpeedOfSound` |
| Thunder far from the camera is not heard, which keeps the number of thunders down | done | `src/Audio/Services/SpellSounds.cpp` (a sample farther than its maximum distance is not started) Test `test/audio/test_sound_tag_channels.cpp` |
| Only the tornado's cores are turned aside by shields; the extreme storm passes over them | done | `src/Particles/Rules/Storm.cpp` (shield deflections of the cores only at the tornado's level, `storm::CoresBounceOffShields`), as the original ([page](../../../magic/storm-and-tornado.md#the-cores-and-the-clouds)); test `test/test_storm.cpp` `Storm.onlyTheTornadosCoresBounceOffShields` |

## Reactions and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers flee from the storm, and are impressed by it as the game measures | done | Fleeing: `src/ECS/Effects/Reactions.cpp`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`; how impressed they are: `MiracleImpressiveValue` there over `magic::ImpressiveValue` (`src/Magic/Impressiveness.h`); tests `Impressiveness.TheFactorsGoInTheOriginalsOrder` and the others of `test/test_magic_foundation.cpp`; scenario `miracles.reactions_flee` |
| A creature can cast the extreme storm as an attacking miracle | partial 60% | The creature's plan has a lightning-storm cast (`CastLightningStorm`, `src/Creature/CreaturePlanActions.cpp`); no scenario or test casts it; not yet checked against the original (issue #172) |
| A storm and its clouds are kept in a saved game | pending | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
| The storm's clouds and spin keep their look when the camera is close below them | done | `src/Particles/Creators/Mist.cpp`. Scenario `miracles.storm_clouds_below` |
| Each cloud's animation frame advances only while the cloud is drawn on screen | partial 50% | Each cloud's own counter starts as the original's (`MistArchetype::Create`, test `Storm.cloudMistAtoms`), but it advances on every frame, seen or not, and at our frame rate ([wiki](../../../magic/storm-and-tornado.md#openblack); issue #55) |
