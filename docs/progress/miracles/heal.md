# Heal

A miracle cast on the land that restores the life of the people, animals and creatures near the cast point and cures
poisoning. The power-up first raises a glowing mushroom and heals a much wider area a few seconds later.

Given by silver scrolls: a heal chest by [The Sacrifice](../story/silver_scrolls/the_sacrifice.md) and [Swap To Brown Bear](../story/silver_scrolls/swap_to_brown_bear.md),
heal dispensers by [The Ogre](../story/silver_scrolls/the_ogre.md) and [The Pied Piper](../story/silver_scrolls/the_pied_piper.md), and a temple that heals all around it by
[The Beach Temple Puzzle](../story/silver_scrolls/the_beach_temple_puzzle.md).

**Progress: 25/35 done, 3 partial — 76%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It can't be cast where there is nobody to heal | done | `src/Magic/CastRules.cpp` (CanCastAt: the type's own FindHealTargets must find someone); a script cast checks nothing, as in the original |
| Cost 6000 / 9000 to cast and 10 for each person healed | done | From the info tables (`src/Magic/MagicTables.cpp`); each chakra's heal event pays its cost per event (`src/Magic/Core/SpellEvent.cpp`, `src/Magic/Core/Chants.cpp`) |
| Who is healed is chosen once, at the cast; anyone walking in later is not | done | `src/Magic/Spells/SpellHeal.cpp` (InitWithPos gives the effect its targets once) |
| The miracle lasts as long as its effect | done | `src/Magic/Core/Spell.cpp` (the plain spell turn); the chakra rule keeps the effect alive until the spell closes (`src/Particles/Rules/Heal.cpp`; test `Heal.chakraWaitsForTargetsUntilClosed`) |
| The caster's tribal power widens the reach and raises the most it can heal | done | `src/Magic/CastRules.cpp` (FindHealTargets: radius and most both times the tribal power when there is a spell) |
| Plain heal: 10 m, at most 20; power-up: 35 m, at most 100, healing 3.5 s after the cast | done | Radius and most from the HEAL and HEAL_PU_ONE rows (`src/Magic/CastRules.cpp`); the power-up's delay and mushroom are the particle data (`src/Particles/Rules/KeyPoints.cpp`) |

## Who is healed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Map cells are walked outward from the cast cell; the plain heal looks in only a 2 by 2 block of cells | done | `src/Magic/CastRules.cpp` (FindHealTargets: ceil(2R/10) squared cells along the map spiral, so 4 for the plain heal) |
| Each cell's people are measured from the cast point moved into that cell | done | `src/Magic/CastRules.cpp` (both distance tests measure from the spiral's moving map point) |
| Distance is flat (height ignored) and strictly inside the reach | done | `src/Magic/CastRules.cpp` (flat distance, strict test) |
| Within a cell the most recent arrivals are taken first, up to the limit | done | `src/Magic/CastRules.cpp` (each cell's mobile list from its head, `ecs::map_cells::MobileInCell`, stops at the most) |
| Only living villagers and animals, and any creature; doves never; the dead are not revived | partial | Villagers and animals, no bird and no vulture (`src/Magic/CastRules.cpp` CanBeHealedByHealSpell); creatures are never targets here, though the original heals them; no dead test (openblack removes the dead) |
| Someone held in the hand is not healed | partial | A held villager's chakra ends at once (`src/Particles/Rules/Heal.cpp`, using the hand's held object instead of the original's unavailable flag); whether one held at the cast is skipped as a target is not checked |
| Two heals never work on the same person at once | done | `src/Particles/Rules/Heal.cpp` (a target that already has a chakra is skipped) |

## What the heal does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each person is healed the moment their glow appears, up to full life at full strength | done | `src/Particles/Rules/Heal.cpp` sends one heal event per chakra; `src/ECS/Effects/EffectValues.cpp` (ApplyEffect: life up to 1) |
| It cures poisoning (for example from poisoned food) | done | `src/Magic/Core/SpellEvent.cpp` removes the poison for HEAL and HEAL_PU_ONE (`src/ECS/Life.cpp`) |
| Buildings, trees and fields are not healed | done | Only villagers and animals are targets (`src/Magic/CastRules.cpp`) |
| A creature in a fight gets fight health and stamina back, and its wounds mend | todo | Creatures are not heal targets in `src/Magic/CastRules.cpp`, and a creature's own life change from an effect is not ported (`src/ECS/Effects/EffectValues.cpp`) |
| A creature's cuts and scars fade when healed | todo | Creatures are not healed by the miracle at all |
| A creature healed by another creature thinks better of it | todo | Creatures are not healed by the miracle, and creatures don't cast it |
| Healing counts as a good deed for the caster's alignment | done | `src/ECS/Effects/EffectValues.cpp` (ApplyEffect moves the caster player's alignment), `src/ECS/Effects/Alignment.cpp` |

## Reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle (one reaction per miracle) | todo | The look-at-nice-miracle reaction is started (`src/Magic/Core/SpellEvent.cpp`), but villagers have no handler for it (`src/ECS/Systems/Implementations/VillagerReactions.cpp` handles only food, wood, fire, death, shield and teleport) |
| A creature watching a player's or creature's heal is impressed and can learn it | todo | Creatures take no reactions (no creature handler in `src/ECS/Effects/Reactions.cpp`) and don't learn miracles by watching |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A glowing chakra follows each healed person's middle, sized to them | done | `src/Particles/Rules/Heal.cpp` (the chakra follows its target's middle, scaled by its 2D radius) |
| The healed person glows pale cyan-white, rising over 1.5 s and gone by 3 s | done | `src/Particles/Rules/Heal.cpp` (ChakraFade into `components::SpecularColour`), drawn by `src/ECS/Systems/Implementations/RenderingSystem.cpp`; test `Heal.chakraFade` |
| Five spinning sparkles rise and spread from each person | done | `src/Particles/Rules/Heal.cpp` (the fused spherical burst); test `Heal.fusedSphericalExplode` |
| The chakra ends when its person goes or is picked up | done | `src/Particles/Rules/Heal.cpp` (the target gone or held ends the chakra and clears the glow) |
| Power-up: a tall glowing mushroom grows, turns and collapses over 6 s | done | `src/Particles/Rules/KeyPoints.cpp`, `src/Particles/Creators/Mesh.cpp` (the animated mesh creator) |
| Power-up: seven stars shoot up about 37 m and fade | done | Particle data of the power-up effect, run by `src/Particles/PSys.cpp` |
| Two sparkles swing to and fro around the miracle held in the hand | done | `src/Particles/Rules/Heal.cpp` (the in-hand wiggle); test `Heal.healInHandWiggle` |
| Effect on a holder of the miracle | partial | Data-driven holder effect (`src/Worship/SpellSeedGraphic.cpp` makes the seed's holder particle); not checked against the game |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One heal sound per miracle, when the first chakra appears | done | `src/Particles/Rules/Heal.cpp` (the first atom's heal sound, once) |
| Power-up: the mushroom's sound at the cast, stopped at once if its effect goes | done | The mesh's create sound (`src/Particles/PSys.cpp`), released with the effect (`src/Particles/Rules/Sound.cpp`) |
| Power-up: the heal sound follows about 3.5 s later | done | Particle data of the power-up effect (the chakra's heal sound comes with the delayed chakras) |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast heal at a hurt villager | todo | Creatures cast no miracles in our tree (nothing in `src/Creature/` casts a spell) |
| A heal in progress is kept in a saved game | todo | No game save system |
