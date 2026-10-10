# Spiritual shield

A sphere the player draws with a circle gesture. It stops other miracles (fireballs, lightning, blasts, flocks) at its
surface, keeps other gods' influence out, and the caster's villagers shelter under it. It drains the caster's prayer
power for as long as it stands and breaks when an attack empties it.

**Progress: 30/37 done, 1 partial — 82%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting and upkeep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast at the centre of a drawn circle, sized by it | done | `src/Magic/Gestures/PowerUpSystem.cpp` (the circle sets point and size), `src/ECS/Systems/Implementations/HandSpellSeed.cpp`; see [../gesture/](../gesture/) |
| Size kept between 5 and 1000 m | done | `src/Magic/Spells/SpellShield.cpp` (`ClampShieldRadius`: from above, then from below); `test/test_shield.cpp` (radiusClamp) |
| Cast only on land within the caster's influence | done | `src/Magic/CastRules.cpp` (the row's cast rule) |
| Upkeep each turn grows with the square of the size and is eased by tribal power | done | `SpellShield.cpp` (`ShieldCostToMaintain`), `src/Magic/Core/Chants.cpp` (divided by tribal power); `test/test_shield.cpp` (upkeepGrowsWithTheSquareOfTheRadius) |
| A player's shield stands while their prayer power pays for it, then fails | done | `src/Magic/Core/Chants.cpp` (the creator pays: a worship site's icon, else the seed's own chants); see [prayer_cost.md](prayer_cost.md) |
| On close-down the shield goes at once with its rings and reactions | done | `SpellShield.cpp` (`ShieldCloseDown`, `ToBeDeleted`: the objects set dying, the anti rings and reactions removed); the invisible shield object goes at once (`src/Magic/Objects/MapShield.cpp` `SetDying`) |

## What it blocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The blocking sphere is a little bigger than the circle and full size from the first turn; its centre never moves | done | `src/Particles/Rules/Shield.cpp` (the defensive sphere, magnitude x 1.11062, made once at the effect's origin); `test/test_shield.cpp` (sphereHelpers, defensiveSphereRegistryAndDeflection) |
| Fireballs bounce off it like a mirror | done | `psys::shields::DoAnyShieldDeflections` (`src/Particles/Rules/Shield.cpp`) called from `src/Particles/Rules/Fireball.cpp`; see [fireball.md](fireball.md) |
| Lightning forks stop at its surface | done | `src/Particles/Rules/Lightning.cpp` (a fork split inside a sphere stops at the intersection); see [lightning.md](lightning.md) |
| A blast centred inside it is cancelled | done | `src/Particles/Rules/Explosion.cpp`; see [blast.md](blast.md) |
| A flock member flying in dies | done | `src/Magic/Spells/SpellFlock.cpp`; see [flocks.md](flocks.md) |
| Each blocked attack drains the shield by the attacker's own cost; when it runs dry the attack gets through and the shield breaks | done | `src/Magic/Core/SpellEvent.cpp` (spell hits spell: the shield pays strength x costPerShieldCollide, struck or destroyed reaction) |
| An area effect that only meets a shield costs the attacker but doesn't drain the shield (as the game does) | done | `src/Magic/Core/SpellEvent.cpp` (the shield check sends the event to the attacker itself) |
| Other gods' hands have no influence inside it, so they can't cast there | done | `SpellShield.cpp` (an anti-influence ring per other player, `src/ECS/Influence/InfluenceRings.cpp`) |
| Other players' creatures walk round it | done | `map_shield::CreatureMustAvoid` (`src/Magic/Objects/MapShield.cpp`) asked by the creature's route planner (`src/ECS/RoutePlanWorld.cpp`); `test/test_shield.cpp` (villagerReactionPriorityAndCreatureMustAvoid) |
| Someone inside ignores reactions to things outside it | done | `src/ECS/Effects/Reactions.cpp` (`map_shield::IsReactionBlockedByShield` for every living class); switching reactions is ported (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |

## Villagers' and creatures' reactions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The caster's villagers near the shield (its size plus 30 m) and their town within 500 m take notice | done | `SpellShield.cpp` (reaction 13, radius r + 30; nearest town under 500 m), `src/ECS/Systems/Implementations/VillagerShield.cpp` |
| They shelter only when their town was attacked lately | done | `VillagerShield.cpp` (`ReactToMagicShieldPriority`: the town's protection desire from `src/ECS/Town/TownDesire.cpp` and the town's aggressor turn); `test/test_shield.cpp` |
| Reaction length and the wait before reacting again follow the table, whatever the distance | done | `VillagerShield.cpp` (`MayReactAgain`, the reaction records) |
| Sheltering villagers walk well inside on their own side and face outward | done | `villager_shield::SetupReactToMagicShield` (`VillagerShield.cpp`) |
| Sheltering villagers point, look at the hand or stand | done | `villager_shield::AmazedByMagicShieldReaction` and `UpdateAmazedClip` (`VillagerShield.cpp`) play the amazed clips, pointing then talking and pointing |
| A struck shield gives a "struck" reaction | partial | `spell_shield::UpdateStruckReaction` (`SpellShield.cpp`) creates and refreshes reaction 35, but no villager takes it up |
| When it breaks, villagers run from where it stood | todo | `spell_shield::SetUpDestroyedReaction` creates reaction 36, but no villager takes it up |
| A broken shield impresses four times as much; a shield over the attacker's own town impresses nothing | todo | no impressiveness or miracle belief in our tree |
| The caster's creature is impressed by it as a nice miracle and can learn it | todo | creatures take no miracle reactions in our tree (no creature reaction handler in `src/ECS/Effects/Reactions.cpp`) |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fifteen player-coloured patches orbit the sphere, cut off at the ground | done | SF_DefenseSphere through `src/Particles/PSys.cpp` (sphere surface tracer) and `src/Particles/Creators/Mesh.cpp` (player colour, cut by the ground plane) |
| The sphere grows from a tenth to full size over 2 s and keeps the spin of the casting hand | done | data-driven, with the initial spin from the cast's curl (`src/Particles/PSys.cpp`) |
| Patches fade in over about 8.5 s and fade with the shield's strength, never below a floor | done | `src/Particles/PSys.cpp` (vapour end effect: alpha = age x 30; collection alpha = strength x 255, 40 to 255) |
| A hit raises up to four crackling spark arcs that wiggle and fade over 3 s | done | `src/Particles/Rules/Shield.cpp` (shield spark rule) |
| On close-down it shrinks over 2 s and is gone at 2.2 s | done | data-driven (SF_DefenseSphere) |
| Orbiting patches round the miracle held in the hand | done | the seed's in-hand effect (`src/Magic/Hand/HandMagicFX.cpp`); the patches turned outwards to form one spinning ball (`src/Particles/PSys.cpp`, orient to surface) |
| Effect on a holder of the miracle | todo | nothing in our tree creates a seed's on-holder effect (SF_DefenseSphereOnHolder is only listed in `src/Particles/ParticleTypes.cpp`) |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A looping hum, one of three sizes by the shield's size | done | data-driven particle sound (`src/Particles/Rules/Sound.cpp`, `spell_sounds::SizeFromRadius` in `src/Audio/Services/SpellSounds.cpp`) |
| The hum stops at once when the shield goes | done | `src/Audio/Services/SpellSounds.cpp` (stop or soft release by the sound's flag when its atom dies) |
| Each hit plays a spark sound | done | `src/Particles/Rules/Shield.cpp` (SoundSpark of each spark atom) |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast the spiritual shield (it's its defensive miracle in fights) | todo | creatures do not cast player miracles in our tree |
| Shields and their reactions are kept in a saved game | todo | openblack has no game saving |
