# Fireball

The player takes a fire seed and throws it: a burning ball (three with the first power-up, eight with the second) flies,
bounces and rolls over the land, setting alight whatever it passes. Enemy balls can be caught, and a held fire seed can
swallow one. How fire then spreads, burns and goes out is physics: See ../physics/fire.md.

Given by silver scrolls: the first and second levels by [The Idol](../story/silver_scrolls/the_idol.md), the second and third
by [Stanley The Wolf](../story/silver_scrolls/stanley_the_wolf.md). Given by gold scroll: the first level by
[Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md).

**Progress: 17/48 done, 0 chosen, 23 partial, 2 inferred — 71%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/fireball-and-lightning.md).

## Casting and the throw

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held fire seed shows flames on the hand, stepped every drawn frame | partial 80% | `src/Magic/Hand/HandMagicFX.cpp` (the power-up level's in-hand effect, stepped every frame) Scenario `miracles.hand_hold_poses`; not compared with the original's look |
| A fire seed on a dispenser shows its holder effect | partial 80% | `src/Worship/SpellSeedGraphic.cpp` (the seed's holder effect); see [dispensers_and_seeds.md](dispensers_and_seeds.md) Scenario `miracles.dispensers`; not compared with the original's look |
| Letting go throws: the ball leaves the hand along the hand's swing, its speed from the swing through the game's throw-speed table | done | `src/Particles/Rules/Fireball.cpp` (CreateWithInitialDirection: the hand's direction pitched up by `fireball_rules::PitchedUp`, with the processor's own arctangents, cosine and sine; its speed through the throw table); the hand's throw data from `src/ECS/Systems/Implementations/HandSpellSeed.cpp`; test `FireballDirection.ThePitchedUpDirectionGivesTheOriginalsBits` (`test/test_fireball_direction.cpp`); scenario `miracles.hand_fireball_throws` |
| The first power-up throws 3 balls and the second 8, scattered in a ring with some random speed | partial 80% | `src/Particles/Rules/Fireball.cpp` (the effect files' atom counts and scatter ring) No test or check covers the scatter |
| It costs 3500, 7000 or 10000 to cast, with nothing per turn or per event; the miracle lasts 20 s | done | `src/Magic/Core/Chants.cpp`, `src/Magic/MagicTables.cpp` (info.dat costs and timers). See [prayer_cost.md](prayer_cost.md) Test `MagicTables.realInfoDat` (with the game's data) |
| A spinning hand puts side spin on the ball, which curls and fades over 1.5 s | partial 70% | `src/Particles/Rules/Fireball.cpp` (UR_SideSpin; the curl from the hand's spin in `HandSpellSeed.cpp`) The side spin's defaults are inferred, the rule's constructor not read ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)); no test |
| The ball is drawn starting from the hand and eases onto its true path | pending | Not ported: the ball appears at the gesture point (the hand draw offset is a note in CreateWithInitialDirection, `src/Particles/Rules/Fireball.cpp`); issue #59 |
| Scripts and computer players lob the ball from a point to a target, with no swing | partial 80% | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS) and the non-human ballistic arc in `src/Particles/Rules/Fireball.cpp`; there are no computer players to use it. No test or check covers the lob |
| A creature casts fireballs at what it wants to burn | partial 70% | `Build::CastFireball` (`CastFireballAt`, from a clear area); scenario `miracles.creature_casts_fireball`; imported, not yet checked against the original; see [../creature/creature_casting.md](../creature/creature_casting.md) and issue #172 |
| A creature catches an enemy fireball and throws it back | inferred | No such creature plan in our tree; our wiki does not describe a creature catching a ball, only another god's hand ([page](../../../magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire)); not checked |

## Flight and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball flies under gravity, bounces off the land and rolls down slopes | done | `src/Particles/Rules/Fireball.cpp` (UpdateRuleGravityWithFloor: gravity, land bounce, ground drag) Scenarios `miracles.fireball_building`, `miracles.hand_fireball_throws` |
| A ball almost still on a slope is nudged so it keeps sliding the game's way | partial 80% | `src/Particles/Rules/Fireball.cpp` (the fixed fallback direction for a ball almost still on the land) No test or check covers it |
| A ball bounces off a spiritual shield like a mirror; a physical shield lets it through | done | `src/Particles/Rules/Fireball.cpp` (CheckShieldDeflections), `src/Particles/Rules/Shield.cpp` (DoAnyShieldDeflections: the deflection spheres are the spiritual shield's, [page](../../../magic/shields.md#the-particles)); test `Shield.defensiveSphereRegistryAndDeflection`. The row said a physical shield bounces it, which the original does not bear out. See [spiritual_shield.md](spiritual_shield.md) |
| The ball ends when it stops rolling, falls into water, or cools below 400 degrees | done | `src/Magic/Objects/MagicFireBall.cpp` (deleted below the deletion temperature or with its atom), `src/Particles/Rules/Fireball.cpp` (EventConditionAtomCloseWater) Scenario `miracles.fireball_water` |
| An ended ball fades out over 1 s and is gone about 4 s later | done | The effect files' fade and death rules, `src/Particles/ParticleEffect.cpp`. Scenario `miracles.fireball_building` |
| Rain cools a ball and makes it steam | done | `src/ECS/Fire/FireEffect.cpp` (rain cooling), `src/Particles/Rules/Fireball.cpp` (EventConditionFireBallSteam); test `FireGraphic.FireballSteamsInsteadOfFlames` (`test/test_fire.cpp`) |
| The water miracle puts a ball out | inferred | `src/Magic/Spells/SpellWater.cpp` cools fires through the map cells and the ball never enters them (`src/ECS/MapCells.cpp`), so a water drop does not reach it; our wiki says the same, but only as an inference ([page](../../../magic/fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire)). If it is confirmed, the row reads "the water miracle does not put a ball out" |
| The ball is not a physics object: it can't be tugged, knocks nothing over, and creatures can't pick it up, throw, stomp or fight it | partial 80% | `src/ECS/Components/MagicFireBall.h`, `src/Magic/Objects/MagicFireBall.cpp` (the ball only follows its atom and carries its fire) No test or check covers the refusals |

## Setting things alight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball is a burning thing as hot as the miracle is strong (6000 degrees at full strength) | partial 80% | `src/Magic/Objects/MagicFireBall.cpp` (fireball::Create: the effect's strength times the initial temperature) No test covers the starting heat |
| The ball's ability to hold heat follows the miracle's live strength | partial 80% | `src/ECS/Fire/FireObjectTraits.cpp` (HeatCapacity times `fireball::Strength`) No test covers it |
| Trees, buildings, fields and people the ball passes catch fire | done | `src/ECS/Fire/FireEffect.cpp` (the heat model spreading from the ball's fire) Scenarios `miracles.fireball_trees`, `miracles.fireball_building`, `miracles.forest_fire` |
| Villagers who fight fires take the ball's heat like any other fire's | partial 80% | `src/ECS/Systems/Implementations/VillagerFire.cpp`. No test or check covers the ball's case |
| Villagers set alight run burning and die; those reacting always return to a sensible state | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`, `src/ECS/Fire/FireEffect.cpp` (burnt down, then destroyed by the effect) Scenario `miracles.fireball_building` |
| A villager the fire kills counts against the caster | partial 80% | `src/ECS/Fire/FireEffect.cpp` to `FireObjectTraits.cpp` DestroyedByEffect with the fire's player (`villager::DestroyedByEffect`) No test or check covers it |
| An animal the fire kills falls dead and lies there | done | `src/ECS/Fire/FireObjectTraits.cpp` to `animal_ai::DestroyedByEffect` (the dying state, the corpse, then a smoke puff, `src/ECS/AnimalAI.cpp`) Scenario `miracles.burning_cow` |
| A town notices its people hurt by fire | pending | The town's count of injured villagers is not ported, and the town's aggressor from burning is a TODO in `src/ECS/Fire/FireObjectTraits.cpp`. See ../town/ |
| A villager on the way to worship is spared the town's fire alarm | partial 80% | `src/ECS/Systems/Implementations/VillagerFire.cpp` (a villager on its way to worship is not called to the fire; the flag is set by `VillagerWorship.cpp`). See ../worship/. No test or check covers it |
| A ball in rain or after setting something alight sizzles | done | `src/ECS/Fire/FireGraphic.cpp` (the steam burst and its sizzle sample) Test `FireGraphic.FireballSteamsInsteadOfFlames` |

## Point events, reactions and learning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every step the ball moves the miracle's position and sends a point event there | partial 80% | `src/Particles/Rules/Fireball.cpp` (EventAlways), `src/Magic/Core/SpellEvent.cpp`. No test covers it |
| The point event carries the ball's last drawn movement | partial 50% | `src/Particles/Rules/Fireball.cpp` sends the step's move (velocity times the step, inferred), not the drawn movement: the event and its position are done, the movement it carries is not |
| People nearby react to the miracle once, the first time it has an effect | done | `src/Magic/Core/SpellEvent.cpp` (the reaction on the first event), `src/ECS/Effects/Reactions.cpp`. Scenario `miracles.reactions_flee` |
| The player's creature may copy the fireball where it lands near a town | pending | The per-event creature mimic is a TODO in `src/Magic/Core/SpellEvent.cpp`; only the water miracle calls `src/ECS/CreatureMimic.cpp` |
| A creature learns the fireball by watching; it must know the plain one before the power-ups | partial 50% | The rules are in `src/Creature/CreatureWatching.cpp` (`test/test_creature_learning.cpp`); a creature learns from a nice miracle through its reaction (`ReactToNiceMagic`, `src/ECS/Systems/Implementations/CreatureReactions.cpp`), but nothing tells it of a fireball cast: `SeeMiracle` is only called from the debug creature spawner and the testbed. See ../creature/ |

## Catching and absorbing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can catch an enemy's ball by tapping or grabbing it while it is still visible, and holds it as a seed | done | `fireball::Catch` (`src/Magic/Objects/MagicFireBall.cpp`), called by the hand's tap, grab and place (`src/ECS/Systems/Implementations/HandSystem.cpp`, `HandTurn.cpp`) in the original's order ([page](../../../magic/fireball-and-lightning.md#openblack)); test `MagicObjectPlayerTest.AFireBallCanBeCaughtUnlessItIsTheCatchersOwn`; scenarios `miracles.fireball_catch`, `miracles.fireball_catch_held`. Only one human player, so only scripted balls of another player can be caught |
| A player can't catch his own ball | done | `fireball::ValidForPlaceInHand` refuses the owner's ball; test `MagicObjectPlayerTest.AFireBallCanBeCaughtUnlessItIsTheCatchersOwn`; scenario `miracles.fireball_absorb_own` |
| Pressing with a ready fire seed over a ball takes it in: the seed stays in the hand, a twentieth stronger | done | `fireball::DeleteAndPutIntoSpellSeed`, called from `src/ECS/Systems/Implementations/HandSpellSeed.cpp`; tests `FireBallIntoSeedTest.TheSeedsNewPowerIsTheOriginalsToTheBit`, `MagicFireBallApply.AFireSeedTakesInAnyonesBallAndNoOtherSeedDoes`; scenario `miracles.fireball_absorb` |
| Placing a caught ball in the hand makes onlookers react to the pick-up, and the hand refuses while its select is locked | pending | Neither is in our tree ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball is a flaming sprite with a trail of ten sprites, including a head sprite on the ball | partial 80% | `src/Particles/Rules/Fireball.cpp` (UR_Trail, head and trail groups) Scenario `miracles.fireball_building`. The trail rule's defaults are inferred, its constructor not read ([wiki Pending](../../../magic/fireball-and-lightning.md#pending)) |
| The trail follows the drawn ball, not its true path | partial 50% | UR_Trail in `src/Particles/Rules/Fireball.cpp` follows the ball's positions; without the hand draw offset (pending above) the drawn and true paths are the same, so only half of it can be seen to match |
| Each ball leaves flames and smoke on the land (fewer with the second power-up) and lights the land below, fainter the higher it flies | done | `src/Particles/Rules/Fireball.cpp` (AR_FadeAlphaWithHeightAboveLandscape) and the effect files. Scenario `miracles.fireball_building` |
| A ball hitting water makes a ring; there is no steam cloud | done | `src/Particles/Rules/Fireball.cpp` (impact ripple), `src/Particles/ParticleWaterRings.cpp`. Scenario `miracles.fireball_water` |
| Every new effect steps twice on its first turn, so it starts as early as in the game | pending | The original steps every new effect twice on its first turn ([particle rules](../../../physics/particle-rules.md)); our tree steps a spell's effect once a turn (`src/Magic/Core/Spell.cpp`) |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball plays a throw whoosh, small, medium or large by its speed | partial 80% | `src/Particles/Rules/Fireball.cpp` (the create sound, sized from the speed over 200), `src/Audio/Services/SpellSounds.cpp`. No test covers the size choice |
| The plain ball's whoosh fades when it ends, the power-ups' stop at once | partial 80% | `src/Audio/Services/SpellSounds.cpp` (soft release or stop, by the sound's flags) No test covers it |
| Hitting the land plays a hit sound by surface and impact speed; hitting water plays its own splash | partial 80% | `src/Particles/Rules/Fireball.cpp` (impact sound sized by the impact speed), `src/Audio/Services/SpellSounds.cpp`. No test covers it |
| A ball flying past within 40 m of the camera plays a fly-by once, only while the ball exists | partial 80% | `src/Particles/Rules/Fireball.cpp` (FlyBySound: within 40 m of the camera and faster than 20) No test covers it |
| Burning things crackle, the nearest fire holding the loop | partial 80% | `src/ECS/Fire/FireSound.cpp`. See ../physics/fire.md. No test covers it |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Balls in flight and their fires are kept in a saved game | pending | openblack has no save games |
