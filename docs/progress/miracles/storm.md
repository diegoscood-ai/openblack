# Storm

The storm miracle throws a swirl from the hand that gathers a ring of dark clouds over the land, bringing rain and
wind that drift the storm with the weather. The extreme (power-up) storm adds lightning from the clouds; the second
power-up is the tornado (see [tornado.md](tornado.md)). Weather outside miracles is in [../weather/](../weather/).

**Progress: 32/35 done, 2 partial — 94%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cast radius is held between 20 m and 1000 m and sets the storm's size | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (InitWithPos); test `Storm.radiusClampAndUpkeep` (`test/test_storm.cpp`) |
| The normal storm has clouds, rain and wind only; the extreme storm adds lightning, the second power-up makes a tornado | done | `src/Particles/Rules/Storm.cpp` (the power-up level: lightning unless it is the plain storm, the tornado group with the second power-up) |
| Cost to create, lifetime (40 s) and per-strike cost differ between normal and extreme | done | `src/Magic/MagicTables.cpp`, `src/Magic/Core/Chants.cpp` (info.dat rows) |
| Upkeep each turn grows with the square of the radius over the normal-cost radius of 40 m | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (CostToMaintain); test `Storm.radiusClampAndUpkeep` |
| In the hand: a small dark cloud ball with flicks of lightning sized by the hand and the seed's strength; each flash lasts half a second, flashes come at randomised gaps | done | `src/Magic/Hand/HandMagicFX.cpp` (the level's in-hand effect, from the data) |

## The swirl at the cast point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A swirl of 30 dark sprites spins tighter and faster, then disperses after 2.4 s and fades 2 s later | done | `src/Particles/Rules/Storm.cpp` (UR_StormCast); test `Storm.realData` (the whirlwind alone) |
| The swirl is thrown along the camera's level heading at up to 20 m/s, speeding up between 1 s and 3 s; cast straight down it stays where it is | done | `src/Particles/Rules/Storm.cpp` (UR_StormCast: the camera's level heading, not normalised, so straight down it stays) |
| The swirl runs only while the miracle's caster exists and is removed when the caster goes | done | `src/Magic/Spells/SpellStormAndTornado.cpp` (Process: the whirlwind is deleted without a creator) |

## Clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clouds are born over about 10 s until the storm has its full number, each starting on a random ring around the centre | done | `src/Particles/Rules/Storm.cpp` (UR_CloudGather); test `Storm.cloudRamps` |
| Each cloud orbits the centre while it gathers, slowing and closing in as it forms, and is reborn when it has lived its gathering time | done | `src/Particles/Rules/Storm.cpp` (UR_CloudGather: the spiral in, the angular speed) |
| A cloud's grey shade, transparency and size follow its gathering | done | `src/Particles/Rules/Storm.cpp` (colour, alpha, scale and ratio ramps); test `Storm.cloudRamps` |
| Clouds float about 90 m up, following the average height of the land under the storm | done | `src/Particles/Rules/Storm.cpp` (CloudHeight plus the average land height under the storm) |
| Clouds darken the land below them as a moving shadow | done | `src/Particles/Creators/Mist.cpp` (the clouds' shadow stamped on the land) |
| When the storm ends the clouds fade out over about 3 s | done | The effect file's fade and removal rules |

## Rain and wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storm lays a rain storm in the weather with inner and outer radii from its size, fading in over half its gathering time, overcast and rain set from the storm | done | `src/Particles/Rules/Storm.cpp`, `src/ECS/Weather/Storms.cpp`; tests `Storm.gatherDescriptorRadii`, `Storm.gatherDescriptorWindAndRain`; see [../weather/](../weather/) |
| The rain stops at once when the storm ends | done | `src/Particles/Rules/Storm.cpp` (the registered storm is marked on closing), `src/ECS/Weather/Storms.cpp`; test `Storm.gatherRegistersOneStorm` |
| The storm blows wind along the direction it was thrown, stronger for bigger storms, rounded as the game rounds | done | `src/Particles/Rules/Storm.cpp` (the wind byte, clipped and rounded); test `Storm.gatherDescriptorWindAndRain` |
| After a short delay the storm drifts with the local wind, smoothly and with no slowing on slopes | done | `src/Particles/Rules/Storm.cpp` (UR_CloudMoverNew: the slope changes nothing, as in the original) |
| A storm cast by a script takes no wind and stays put | done | `src/Particles/Rules/Storm.cpp` (no wind for a script cast) |
| Rain and wind ambience rise and fall with the weather at the camera | done | `src/Audio/Services/SoundMap.cpp`, `src/Audio/Services/AtmosBanks.cpp` |
| The rain cools and puts out fires under the storm, and villagers come to watch the storm putting a fire out (one gathering per storm at a time) | done | `src/ECS/Fire/FireEffect.cpp` (rain cooling), `src/Magic/Spells/SpellStormAndTornado.cpp` (the watchers' reaction, one per storm) |
| The wind drifts fire sprites and smoke | done | `src/ECS/Fire/FireGraphic.cpp` (the puffs drift with half the wind) |
| A script can end every storm in an area; a miracle storm caught in it comes back on the next step and fades in again | done | `src/Magic/Script/CHLWeather.cpp` (KILL_STORMS_IN_AREA), `src/Particles/Rules/Storm.cpp` (the next step registers another) |

## Lightning (extreme storm)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first strikes come about 9.5 to 11 s after the cast | done | `src/Particles/Rules/Storm.cpp` (the first strike at rand(0.5, 1) times SwitchLife plus LightningDelay) |
| Strikes come from clouds that are more than half gathered, at random waits shortened by tribal power | partial | `src/Particles/Rules/Storm.cpp` has the rule, but `PlayerMagic::tribalPower` is never set in our tree, so the waits are never shortened |
| A striking cloud flashes bright for half a second | done | `src/Particles/Rules/Storm.cpp` (the cloud's specular for half a second), `src/Particles/Creators/Mist.cpp`; test `Storm.cloudMistAtoms` |
| A bolt hits trees, buildings and other things below within its reach, or the ground when there are too few | done | `src/Particles/Rules/Lightning.cpp` (the parent mode); see [lightning.md](lightning.md) |
| A bolt carried from one cloud to the next keeps what it was striking | done | `src/Particles/Rules/Storm.cpp` (the bolt's collection moves to the new cloud) |
| Struck things catch fire and take damage, and each strike costs prayer power | done | `src/Magic/Core/SpellEvent.cpp`, `src/ECS/Effects/EffectValues.cpp` |
| Thunder (small, medium or large at random) sounds from the land under the striking cloud, delayed by its distance at the speed of sound | done | `src/Particles/Rules/Storm.cpp` (random size, delayed by the distance and set on the ground), `src/Audio/Services/SpellSounds.cpp` |
| Thunder far from the camera is not heard, which keeps the number of thunders down | done | `src/Audio/Services/SpellSounds.cpp` (a sample farther than its maximum distance is not started) |
| The extreme storm is turned aside by shields it passes over | done | `src/Particles/Rules/Storm.cpp` (shield deflections of the cores only at the tornado's level). Our wiki differs: only the second power-up (the tornado) bounces its cores off shields, not the first ([page](../../bw1-notes/miracles.md#the-cores-and-the-clouds-ur_cloudmovernew-0x6d41c0-ur_cloudgather-0x6d4a70)) |

## Reactions and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers flee from the storm, and are impressed by it as the game measures | partial | Fleeing: `src/ECS/Effects/Reactions.cpp`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`; how impressed they are and the town belief it feeds are a TODO there |
| A storm and its clouds are kept in a saved game | todo | openblack has no saving of running miracles yet; see [../engine/](../engine/) |
| The storm's clouds and spin keep their look when the camera is close below them | done | `src/Particles/Creators/Mist.cpp` |
