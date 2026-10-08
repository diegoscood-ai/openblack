# Fireball

The player takes a fire seed and throws it: a burning ball (three with the first power-up, eight with the second) flies,
bounces and rolls over the land, setting alight whatever it passes. Enemy balls can be caught, and a held fire seed can
swallow one. How fire then spreads, burns and goes out is physics: See ../physics/fire.md.

Given by silver scrolls: the first and second levels by [The Idol](../story/silver_scrolls/the_idol.md), the second and third
by [Stanley The Wolf](../story/silver_scrolls/stanley_the_wolf.md). Given by gold scroll: the first level by
[Khazar's Fireball Challenge](../story/gold_scrolls/khazars_fireball_challenge.md).

**Progress: 33/47 done, 8 partial — 79%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and the throw

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The held fire seed shows flames on the hand, stepped every drawn frame | done | `src/Magic/Hand/HandMagicFX.cpp` (the power-up level's in-hand effect, stepped every frame) |
| A fire seed on a dispenser shows its holder effect | done | `src/Worship/SpellSeedGraphic.cpp` (the seed's holder effect); see [dispensers_and_seeds.md](dispensers_and_seeds.md) |
| Letting go throws: the ball leaves the hand along the hand's swing, its speed from the swing through the game's throw-speed table | done | `src/Particles/Rules/Fireball.cpp` (CreateWithInitialDirection: the hand's direction pitched up, its speed through the throw table); the hand's throw data from `src/ECS/Systems/Implementations/HandSpellSeed.cpp` |
| The first power-up throws 3 balls and the second 8, scattered in a ring with some random speed | done | `src/Particles/Rules/Fireball.cpp` (the effect files' atom counts and scatter ring) |
| It costs 3500, 7000 or 10000 to cast, with nothing per turn or per event; the miracle lasts 20 s | done | `src/Magic/Core/Chants.cpp`, `src/Magic/MagicTables.cpp` (info.dat costs and timers). See [prayer_cost.md](prayer_cost.md) |
| A spinning hand puts side spin on the ball, which curls and fades over 1.5 s | done | `src/Particles/Rules/Fireball.cpp` (UR_SideSpin; the curl from the hand's spin in `HandSpellSeed.cpp`) |
| The ball is drawn starting from the hand and eases onto its true path | todo | Not ported: the ball appears at the gesture point (the hand draw offset is a note in CreateWithInitialDirection, `src/Particles/Rules/Fireball.cpp`) |
| Scripts and computer players lob the ball from a point to a target, with no swing | done | `src/Magic/Script/CHLSpells.cpp` (SPELL_AT_POS) and the non-human ballistic arc in `src/Particles/Rules/Fireball.cpp`; there are no computer players to use it |
| A creature casts fireballs at what it wants to burn | todo | Creatures cast no world miracles in our tree (`src/Creature` has no miracle plan). See [creature_spells.md](creature_spells.md) |
| A creature catches an enemy fireball and throws it back | todo | No such creature plan, and catching a ball is not wired even for the hand |

## Flight and end

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball flies under gravity, bounces off the land and rolls down slopes | done | `src/Particles/Rules/Fireball.cpp` (UpdateRuleGravityWithFloor: gravity, land bounce, ground drag) |
| A ball almost still on a slope is nudged so it keeps sliding the game's way | done | `src/Particles/Rules/Fireball.cpp` (the fixed fallback direction for a ball almost still on the land) |
| A ball bounces off a physical shield it isn't let through | done | `src/Particles/Rules/Fireball.cpp` (CheckShieldDeflections), `src/Particles/Rules/Shield.cpp` (DoAnyShieldDeflections). See [physical_shield.md](physical_shield.md) |
| The ball ends when it stops rolling, falls into water, or cools below 400 degrees | done | `src/Magic/Objects/MagicFireBall.cpp` (deleted below the deletion temperature or with its atom), `src/Particles/Rules/Fireball.cpp` (EventConditionAtomCloseWater) |
| An ended ball fades out over 1 s and is gone about 4 s later | done | The effect files' fade and death rules, `src/Particles/PSys.cpp` |
| Rain cools a ball and makes it steam | done | `src/ECS/Fire/FireEffect.cpp` (rain cooling), `src/Particles/Rules/Fireball.cpp` (EventConditionFireBallSteam); test `FireGraphic.FireballSteamsInsteadOfFlames` (`test/test_fire.cpp`) |
| The water miracle puts a ball out | partial | `src/Magic/Spells/SpellWater.cpp` cools fires through the map cells, but the ball never enters them (`src/ECS/MapCells.cpp`), so a water drop does not reach it. Our wiki differs: the ball is not in the map cells, so the water's area effect cannot reach it (inferred) ([page](../../bw1-notes/miracles.md#fireball-magic_type-1-3-seed-2-fire)) |
| The ball is not a physics object: it can't be tugged, knocks nothing over, and creatures can't pick it up, throw, stomp or fight it | done | `src/ECS/Components/MagicFireBall.h`, `src/Magic/Objects/MagicFireBall.cpp` (the ball only follows its atom and carries its fire) |

## Setting things alight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball is a burning thing as hot as the miracle is strong (6000 degrees at full strength) | done | `src/Magic/Objects/MagicFireBall.cpp` (fireball::Create: the effect's strength times the initial temperature) |
| The ball's ability to hold heat follows the miracle's live strength | done | `src/ECS/Fire/FireObjectTraits.cpp` (HeatCapacity times `fireball::Strength`) |
| Trees, buildings, fields and people the ball passes catch fire | done | `src/ECS/Fire/FireEffect.cpp` (the heat model spreading from the ball's fire) |
| Villagers who fight fires take the ball's heat like any other fire's | done | `src/ECS/Systems/Implementations/VillagerFire.cpp` |
| Villagers set alight run burning and die; those reacting always return to a sensible state | done | `src/ECS/Systems/Implementations/VillagerFire.cpp`, `src/ECS/Fire/FireEffect.cpp` (burnt down, then destroyed by the effect) |
| A villager the fire kills counts against the caster | done | `src/ECS/Fire/FireEffect.cpp` to `FireObjectTraits.cpp` DestroyedByEffect with the fire's player (`villager::DestroyedByEffect`) |
| An animal the fire kills falls dead and lies there | done | `src/ECS/Fire/FireObjectTraits.cpp` to `animal_ai::DestroyedByEffect` (the dying state, the corpse, then a smoke puff, `src/ECS/AnimalAI.cpp`) |
| A town notices its people hurt by fire | todo | The town's count of injured villagers is not ported, and the town's aggressor from burning is a TODO in `src/ECS/Fire/FireObjectTraits.cpp`. See ../town/ |
| A villager on the way to worship is spared the town's fire alarm | done | `src/ECS/Systems/Implementations/VillagerFire.cpp` (a villager on its way to worship is not called to the fire; the flag is set by `VillagerWorship.cpp`). See ../worship/ |
| A ball in rain or after setting something alight sizzles | done | `src/ECS/Fire/FireGraphic.cpp` (the steam burst and its sizzle sample) |

## Point events, reactions and learning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every step the ball moves the miracle's position and sends a point event there | done | `src/Particles/Rules/Fireball.cpp` (EventAlways), `src/Magic/Core/SpellEvent.cpp` |
| The point event carries the ball's last drawn movement | partial | `src/Particles/Rules/Fireball.cpp` sends the step's move (velocity times the step, inferred), not the drawn movement |
| People nearby react to the miracle once, the first time it has an effect | done | `src/Magic/Core/SpellEvent.cpp` (the reaction on the first event), `src/ECS/Effects/Reactions.cpp` |
| The player's creature may copy the fireball where it lands near a town | todo | The per-event creature mimic is a TODO in `src/Magic/Core/SpellEvent.cpp`; only the water miracle calls `src/ECS/CreatureMimic.cpp` |
| A creature learns the fireball by watching; it must know the plain one before the power-ups | partial | The rules are in `src/Creature/CreatureWatching.cpp` (`test/creature/test_creature_learning.cpp`), but a cast never tells the creature: `SeeMiracle` is only called from the debug creature spawner. See ../creature/ |

## Catching and absorbing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can catch an enemy's ball by tapping or grabbing it while it is still visible, and holds it as a seed | partial | `fireball::Catch` in `src/Magic/Objects/MagicFireBall.cpp` (a ready, charged fire seed in the hand), but nothing in the hand calls it: dormant. Only one human player, as openblack has no others |
| A player can't catch his own ball | partial | `fireball::ValidForPlaceInHand` refuses the owner's ball; dormant with `fireball::Catch` |
| Pressing with a ready fire seed over a ball takes it in: the seed stays in the hand, a twentieth stronger | partial | `fireball::DeleteAndPutIntoSpellSeed` (the seed's power times one plus the catch factor), not called by the hand: dormant |

## Look

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The ball is a flaming sprite with a trail of ten sprites, including a head sprite on the ball | done | `src/Particles/Rules/Fireball.cpp` (UR_Trail, head and trail groups) |
| The trail follows the drawn ball, not its true path | partial | UR_Trail in `src/Particles/Rules/Fireball.cpp` follows the ball's positions; without the hand draw offset the drawn and true paths are the same |
| Each ball leaves flames and smoke on the land (fewer with the second power-up) and lights the land below, fainter the higher it flies | done | `src/Particles/Rules/Fireball.cpp` (AR_FadeAlphaWithHeightAboveLandscape) and the effect files |
| A ball hitting water makes a ring; there is no steam cloud | done | `src/Particles/Rules/Fireball.cpp` (impact ripple), `src/Particles/PSysWaterRings.cpp` |
| Every new effect steps twice on its first turn, so it starts as early as in the game | partial | Not found in our tree: a spell's effect is stepped once a turn by its spell (`src/Magic/Core/Spell.cpp`); our wiki does not describe a double first step |

## Sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each ball plays a throw whoosh, small, medium or large by its speed | done | `src/Particles/Rules/Fireball.cpp` (the create sound, sized from the speed over 200), `src/Audio/Services/SpellSounds.cpp` |
| The plain ball's whoosh fades when it ends, the power-ups' stop at once | done | `src/Audio/Services/SpellSounds.cpp` (soft release or stop, by the sound's flags) |
| Hitting the land plays a hit sound by surface and impact speed; hitting water plays its own splash | done | `src/Particles/Rules/Fireball.cpp` (impact sound sized by the impact speed), `src/Audio/Services/SpellSounds.cpp` |
| A ball flying past within 40 m of the camera plays a fly-by once, only while the ball exists | done | `src/Particles/Rules/Fireball.cpp` (FlyBySound: within 40 m of the camera and faster than 20) |
| Burning things crackle, the nearest fire holding the loop | done | `src/ECS/Fire/FireSound.cpp`. See ../physics/fire.md |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Balls in flight and their fires are kept in a saved game | todo | openblack has no save games |
