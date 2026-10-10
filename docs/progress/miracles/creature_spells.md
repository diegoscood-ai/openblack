# Creature spells

Sixteen miracles that only work on a creature (freeze, small, big, weak, strong, fat, thin, invisible, compassion,
angry, hungry, frightened, tired, ill, thirsty, itchy). Each eases the creature into a changed state, holds it for a
time and eases it back. This file also covers creatures learning miracles by watching and casting miracles themselves.
The creature's own body, mind and leash are in [../creature/](../creature/).

**Progress: 17/72 done, 23 partial — 40%**

How the original does it, in our wiki: [Miracles one by one](../../bw1-notes/miracles.md).

## Casting a creature spell

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature spell can only be cast on a creature (any creature: own, rival or wild), never at a point | partial | Never at a point (`cast_rules::CanCastAt`, `src/Magic/CastRules.cpp`); on a creature only from a script (`SPELL_AT_THING` to `spell_creature::Receive`, `src/Magic/Spells/SpellCreature.cpp`), as the hand's object check refuses the creature class (TODO) |
| It is refused when more than five spells already wait on that creature | todo | The waiting list has no limit (`creature_spells::Receive`, `src/Creature/CreatureSpells.cpp`) |
| It lasts 25 seconds times the power of its tribes (a different tribe or pair per spell) | done | `spell_creature::Receive`: the miracle's time times the caster's tribal power, `src/Magic/Spells/SpellCreature.cpp` |
| The miracle stays open until the creature lets the spell go, then closes | done | `spell_creature::Receive` lifts the miracle's time limit; `ProcessTurn` closes it when the spell ends, `src/Magic/Spells/SpellCreature.cpp`; `test/creature/test_spell_creature.cpp` `ACreatureTakesTheMiracleOnForItsTimeAndTheMiracleNoLongerRunsOut` |
| The player's hand pours a stream of magic onto the creature for four seconds | todo | The hand can't cast creature spells yet |
| Creature spells reach the player from worship-site icons in some towns | partial | Icons and the reverse-spiral selection handle the creature seeds like any other (`src/Worship/WorshipSpellIcon.cpp`, `src/Magic/Gestures/PowerUpSystem.cpp`), but a creature seed in the hand can't be cast |
| Some come as one-off rewards from fireflies on Lands 2, 4 and 5 (freeze, small, big, weak, strong, invisible, compassion, angry, itchy; about 1% each); fat, thin and the five need spells are never in a firefly table | partial | The firefly reward is wired (`src/Worship/FireFlyReward.cpp`, `FIRE_FLY_SPELL_REWARD_PROB`). Our wiki differs: only Land1.txt sets firefly reward probabilities (HEAL and six player miracles), no creature spell ([magic.md](../../bw1-notes/magic.md#dispensers-and-fireflies-worshipspelldispensercpp-worshipfireflyrewardcpp)) |

## How a spell takes hold

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A spell waits two seconds, eases in over its start time, holds, then eases out over its finish time | done | `creature_spells::Receive` and `Step` (`k_StartDelaySeconds`, `src/Creature/CreatureSpells.cpp`): it waits two seconds of whole turns, also when it was waiting for another of its kind, then eases in, holds and eases out; `test/creature/test_creature_spells.cpp` `ASpellEasesInHoldsForItsTimeAndEasesOut`, `TimesAreWholeTurns`; `test/creature/test_spell_creature.cpp` `ATurnOfAFreezeStopsTheCreaturePausesItsMindAndSoundsFromTheCreatureBank` |
| Start and finish times differ per spell (freeze 2 s, size and strength 4 s, invisible 5 s, compassion 3 s, others 1 s) | done | Read from the creature miracle rows in `spell_creature::ProcessTurn`, `src/Magic/Spells/SpellCreature.cpp`; `test/creature/test_creature_spells.cpp` `TimesAreWholeTurns` |
| Casting the same spell again adds its time and takes over from the older miracle | done | `creature_spells::Receive`, `src/Creature/CreatureSpells.cpp`; `test/creature/test_creature_spells.cpp` `CastingTheSameSpellAgainAddsItsTimeAndTakesTheNewMiracle`; `test/creature/test_spell_creature.cpp` `CastAgainItIsExtendedAndTheFirstMiracleIsClosedDown` |
| The same spell cast while it is wearing off waits and starts again afterwards | done | `creature_spells::Receive` queues it while finishing, `src/Creature/CreatureSpells.cpp` |
| An opposing spell of the same kind cuts the first short and waits for it | done | `test/creature/test_creature_spells.cpp` `TheOpposingSpellCutsTheFirstShortAndWaitsForIt` |
| Spells of different kinds run together | done | `test/creature/test_creature_spells.cpp` `SpellsOfDifferentKindsRunTogether` |
| A spell whose miracle disappears ends early | todo | A closed miracle is only unlinked from the creature's spell (`CreatureCloseDown`, `src/Magic/Spells/SpellCreature.cpp`); the spell runs on |
| A creature that faints brings every spell to its end | todo | Nothing ends the spells on a faint |
| With reversion turned off by a script, a spell stops where it is and never puts the creature back | done | `CREATURE_SPELL_REVERSION` (`src/CHLApi.cpp`) sets `creature_spells::Spells::reversion` through `spell_creature::SetReversion` (`src/Magic/Spells/SpellCreature.cpp`); without it a spell that has held its time goes straight to off, the creature kept as the spell made it, and its miracle is let go (`creature_spells::Step`, `src/Creature/CreatureSpells.cpp`); tests `CreatureSpells.WithoutReversionASpellStopsWhereItIs`, `CreatureSpells.WithoutReversionASpellThatStopsLeavesTheOneWaitingForItWaiting`. No Land 1 or Land 2 script calls it |
| The creature's mind is told when a spell starts and when it ends (for the advisor's help) | todo | No spell-started or finished events reach the creature mind |
| A creature with spells on it is saved as it was before them (size, strength, alignment) | todo | The before values are kept in the spell slots only; neither the mind file nor a game save writes them |

## The sixteen spells

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Freeze: the creature gives up what it was doing, stops, and its mind stops planning and learning | done | The start stops its locomotion and pauses its mind (`creature_spells::Apply`, `src/Creature/CreatureSpells.cpp`); `test/creature/test_spell_creature.cpp` `FreezeStopsTheCreatureAndPausesItsMindUntilItThaws` |
| Freeze: its animation slows to a statue as it freezes and speeds up as it thaws | done | `playbackScale` eased by the freeze, used by `src/ECS/Systems/Implementations/CreatureAnimationSystem.cpp` |
| Freeze: an icy environment-mapped sheen is added over it by how frozen it is | todo | `creature_spells::FrozenTint` is worked out (`test/creature/test_creature_spells.cpp` `TheFrozenLookTintsTowardsIcyBlue`) but the frozen look is not drawn |
| Small: shrinks the creature to its smallest size (less in a fight, by the size it began the fight with) | partial | `creature_spells::SizeTarget` to the creature's smallest size, 0.2 or what a script set with `CREATURE_MIN_SIZE` (`ecs::components::CreatureSizeLimits`; `test/creature/test_creature_spells.cpp` `TheBodySpellsTargets`, `test/creature/test_spell_creature.cpp` `TheSizeSpellsTakeItToItsOwnSmallestAndLargest`), never making it bigger; nothing for a creature in an activity with another (wiki Pending) |
| Big: grows the creature to its largest size (more in a fight), never shrinking it | partial | `creature_spells::SizeTarget` to the creature's largest size, 2.4 or what a script set with `CREATURE_MAX_SIZE` (`test/creature/test_spell_creature.cpp` `BigTakesTheCreatureToTheLargestAScriptGaveIt`), never shrinking it; nothing for a creature in an activity with another (wiki Pending) |
| Small and Big put the creature back to its size from before, losing growth made meanwhile | done | The finish puts the size from before back, `src/Creature/CreatureSpells.cpp`; `test/creature/test_spell_creature.cpp` `SizeStartsEasesHoldsAndIsPutBack` |
| Weak takes strength down to a tenth; Strong raises it to full; both put it back | done | `k_WeakStrength`, `k_StrongStrength` targets and the value put back, `src/Creature/CreatureSpells.cpp` |
| Fat and Thin move how fat the creature wants to be up or down, then back | partial | They ease the creature's fatness itself to fixed targets and back (`src/Creature/CreatureSpells.cpp`), not how fat it wants to be |
| Invisible: the creature dissolves through static up to three quarters of the way | partial | The fizz is eased to three quarters (`src/Creature/CreatureSpells.cpp`; `test/creature/test_spell_creature.cpp` `InvisibleFizzesOutAndBack`) but not drawn |
| Invisible: villagers neither worship nor flee an invisible creature | todo | The invisible flag is set; nothing reads it |
| A creature frozen or fizzed a fifth of the way or more casts no shadow | todo | Nothing reads the freeze or fizz for shadows |
| Compassion and Angry: the creature gives up its action and wants only to be kind (and make friends) or angry, everything else held down | partial | The mood desire is made the top one and held there (`src/Creature/CreatureSpells.cpp`; `test/creature/test_spell_creature.cpp` `AMoodSpellsDesireIsWantedMostThenLeast`); the other desires are not held down and the action is not dropped |
| Compassion and Angry swing the creature's alignment fully good or evil, then back | done | `k_NiceAlignment`, `k_NastyAlignment` targets and the value put back, `src/Creature/CreatureSpells.cpp` |
| Compassion and Angry switch a leashed creature to the compassion or aggression leash | todo | Nothing changes the leash type |
| As they wear off the mood is wanted least, and the hold on the other desires is released (kept on a player's creature led on a mood leash) | partial | The finish makes the desire the least wanted (`src/Creature/CreatureSpells.cpp`); there is no hold on the other desires to release |
| Hungry, Frightened, Tired, Ill and Thirsty make that need the only one wanted, without changing the body | partial | The need is made the top desire each turn, the body unchanged (`src/Creature/CreatureSpells.cpp`; `test/creature/test_creature_spells.cpp` `TheMoodSpellsMakeTheirDesireDominant`); the others are not held down |
| The need spells' hold on the other desires lasts its own time after they wear off | todo | No hold on the other desires |
| Itchy: the creature gives up its action and wants only to scratch, again each turn | partial | The scratch desire is set to its top each turn (`src/Creature/CreatureSpells.cpp`); the action is not dropped |
| Itchy: the leash comes off each turn while it lasts | done | The leash stops working while it holds (`LeashSystem::SetWorks`); `test/creature/test_spell_creature.cpp` `ItchyKeepsTheLeashFromWorkingWhileItHolds` |

## Look and sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three wisps fly from the hand to the creature and wind round its body, trailing light | partial | The miracle's effect file is started at the creature with it as target (the base spell cast, `src/Magic/Spells/SpellCreature.cpp`); its creature-specific particle rules are not identified in our tree |
| The wisps fade with an invisible creature and fade out after the spell ends | todo | Nothing ties the effect to the fizz |
| Compassion sends hearts rising off the creature's body | partial | Only as far as the effect file plays through the PSys; not checked |
| Itchy brings a flock of flies that waits at the hand, then circles the creature's head | partial | The particle flocking rule is ported (`src/Particles/Rules/Flock.cpp`, used by the itch files); the head target is not checked |
| An itchy spell held in the hand has flies circling the hand | partial | The seed's in-hand effect follows the hand (`src/Magic/Hand/HandMagicFX.cpp`, `src/Particles/Rules/HandFollow.cpp`); not checked for this seed |
| Holder effects: glints on a frozen-creature phial, hearts and flies on their holders | partial | Each seed's holder effect plays round its globe or icon (`src/Worship/SpellSeedGraphic.cpp`); not checked for the phials |
| A whispering loop plays on the creature while the effect lasts and softly stops at the end | partial | Only as far as the effect file's sound rules run (`src/Particles/Rules/Sound.cpp`); not checked |
| A cast sound plays once for a player's cast, none for a script's or another creature's | todo | No player cast of a creature spell yet |
| Each spell's own sound plays as it takes hold (freeze, shrink, grow, invisible, compassion, itchy); fat, thin and the needs are silent | done | The sound actions 0x76..0x7E from the creature bank on the spell's start (`src/Creature/CreatureSpells.cpp`, `src/Magic/Spells/SpellCreature.cpp`); `test/creature/test_spell_creature.cpp` `ATurnOfAFreezeStopsTheCreaturePausesItsMindAndSoundsFromTheCreatureBank` |
| The itch sound stops when out of hearing range | todo | Not modelled |
| There is no special animation for receiving a spell | done | Nothing plays, as in the game |

## Creatures learning miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature learns a miracle by seeing it cast enough times; the number depends on the miracle and the species | partial | Times by miracle and species: the logic is in `src/Creature/CreatureWatching.cpp` (`creature_watching::SeeMiracle`), but nothing in the game calls `CreatureMindSystem::SeeMiracle` (only the debug creature spawner) |
| Sightings count only when more than five seconds apart, and every sighting resets that gap | partial | Five seconds apart (`k_MiracleSightingTurns`), but a sighting too soon does not reset the gap; the logic is in `src/Creature/CreatureWatching.cpp` (`creature_watching::SeeMiracle`), but nothing in the game calls `CreatureMindSystem::SeeMiracle` (only the debug creature spawner) |
| Watching on the learning leash counts extra | partial | The leash's sighting weight; the logic is in `src/Creature/CreatureWatching.cpp` (`creature_watching::SeeMiracle`), but nothing in the game calls `CreatureMindSystem::SeeMiracle` (only the debug creature spawner) |
| A power-up can't be learnt before its miracle; thirsty and itchy need the creature to know building | todo | `creature_watching::SeeMiracle` has no such rule |
| A young creature sees but learns nothing | partial | The development phase gate; the logic is in `src/Creature/CreatureWatching.cpp` (`creature_watching::SeeMiracle`), but nothing in the game calls `CreatureMindSystem::SeeMiracle` (only the debug creature spawner) |
| The second lightning power-up is never learnt from watching | todo | No such rule |
| One creature learning from a seed's miracle stops any creature learning from that seed again | todo | No seed is marked as learnt from |
| The creature shows a thought when it has nearly learnt or has just learnt a miracle | partial | A "learnt the miracle" thought is kept in the mind model (`creature_mind_model::Think`); the logic is in `src/Creature/CreatureWatching.cpp` (`creature_watching::SeeMiracle`), but nothing in the game calls `CreatureMindSystem::SeeMiracle` (only the debug creature spawner); nothing shows it |

## Creatures casting miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature may try a miracle once it has seen it half the times needed, and fizzles until nearly learnt | todo | Creatures cast no miracles in our tree |
| A fizzled try embarrasses the creature and sometimes saddens it | todo | Creatures cast no miracles |
| It walks near the target, backs off by its height, turns to face it, makes the gesture and casts | todo | Creatures cast no miracles |
| One time in five it shows how it feels first | todo | Creatures cast no miracles |
| Casting costs the creature energy and tires it; too tired, it can't cast | todo | Creatures cast no miracles |
| A cast it can't make makes it frustrated (one desire fully dominant) | todo | Creatures cast no miracles |
| In a fight a cast costs fight stamina, and a shortfall quietly does nothing | todo | Creatures cast no miracles |
| The miracle's size follows the target's size; a fire miracle's follows the caster's height | todo | Creatures cast no miracles |
| A creature's earlier held miracle is let go when it casts again | todo | Creatures cast no miracles |
| Food, wood and water are cast from above with two beams from its hands | todo | Creatures cast no miracles |
| A creature's cast gives up when it swings too far away from its target | todo | Creatures cast no miracles |
| In a fight, at the end of its cast pose it casts an attack spell at its opponent or a defence spell on itself | partial | The fight's cast order plays the cast pose and records the spell (`src/ECS/Systems/Implementations/CreatureFightSystem.cpp`), but no miracle is cast |
| Glints show on the miracle in the creature's hands | todo | Creatures cast no miracles |
| On its way to the target the creature keeps its current pace rather than walking | todo | Creatures cast no miracles |
| A personal speed factor for each creature affects its casting walk | todo | Creatures cast no miracles |
| Creatures cast power-up miracles with their power-up gestures | todo | Creatures cast no miracles |
