# Miracle effects and reactions

The machinery every miracle shares once it is cast: its life from creation to removal, the effect it applies to the
things it reaches, the deaths and damage it causes, how it moves alignment and belief, how people, animals and creatures
react to it, the challenge scripts' miracle commands, computer gods casting miracles, and saving active miracles.
Casting by hand is in [casting_and_globes.md](casting_and_globes.md), what a miracle costs in
[prayer_cost.md](prayer_cost.md), and creatures casting in [creature_spells.md](creature_spells.md).

**Progress: 46/94 done, 16 partial — 57%**

How the original does it, in our wiki: [Magic: the core of the miracles](../../bw1-notes/magic.md), [Miracles one by one](../../bw1-notes/miracles.md).

## A miracle's life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new miracle remembers who cast it and for which player; a miracle with no caster belongs to the neutral player | done | `magic::AllocSpell` and `magic::CastAtPos` (`src/Magic/Core/Spell.cpp`): no creator means `creator::NeutralPlayer()` |
| Its strength, chants, duration and how many things it may make come from the cast | done | `base::InitWithPos` (`src/Magic/Core/Spell.cpp`) from the `SpellCastData` |
| A miracle cast from a seed lasts the seed's time for a player cast, times the seed's multiplier (one-off seeds too) | done | `PrepareCast` in `src/Magic/Core/SpellSeed.cpp` (the player timer times the seed's multiplier; one-off seeds go through the same seed) |
| A miracle whose effect can't start is refused; one that has no effect (shield, teleport, flocks) still lives | partial | The no-effect classes live on (`base::InitWithPos` sends their start event). A spell whose particle file is missing is cast anyway without an effect (a log line), where our wiki says the original refuses it ([life cycle](../../bw1-notes/magic.md#life-cycle-spellcpp-0x71fb40)) |
| A miracle cast on a thing starts at the thing and its effect follows it | done | `base::InitWithObject` casts at the object and adds it as the effect's target; `magic::CastAtObject` follows the seed's cast-on-object rule |
| Miracles are processed in a fixed order each turn: shields, seeds, then every miracle kept up, then every miracle run | done | `magic::ProcessSpells` (`src/Magic/Core/Spell.cpp`): grid decay, `map_shield::ProcessShields`, `worship::ProcessSpellIcons`, every upkeep, then each spell's seed and turn |
| A miracle ages each turn and closes when it outlives its duration | done | `ProcessMaintainRequest` in `src/Magic/Core/Spell.cpp` |
| When its caster is gone the miracle closes at once without paying for the turn | done | `ProcessMaintainRequest`: a creator that is not functional closes the spell and returns before paying |
| A miracle held in the hand is placed where the hand is, on the ground below it | done | `ProcessMaintainRequest`: a hand-cast spell's cast point follows the hand, on the map at altitude 0 |
| A held miracle that leaves the ground where it may be cast is dropped from the hand | partial | An in-hand miracle stops applying off castable ground (`HandSystem::UpdateSeedAction`, `src/ECS/Systems/Implementations/HandSpellSeed.cpp`), but the seed stays in the hand |
| A miracle whose power runs out closes; its effect finishes, then it is removed | done | `base::CoreProcess` (`src/Magic/Core/Spell.cpp`): strength 0 closes it, the effect runs out and the spell is deleted when its effect is gone |
| The people's reactions to a miracle end as soon as its effect finishes, even for miracles kept on (shield, forest) | done | `base::CoreProcess` removes the reactions it started when its effect finishes (`reactions::RemoveAllReactionsInitiatedByObject`) |
| A seed goes with its miracle when the miracle is removed | done | `base::ToBeDeleted` deletes the linked seed (`seed::ToBeDeleted`) |
| The hand's stream of magic stops when a seed miracle the local player cast closes | done | `base::CloseDown`: the first close-down of a spell cast by the local interface calls `hand_grain::Stop` |
| A miracle whose seed was left outside its player's influence closes | done | `seed::ProcessFromSpell` (`src/Magic/Core/SpellSeed.cpp`) for a seed that follows its spell and belongs to a worship icon; icons exist only where worship sites are made |
| Each miracle marks the map cell it stands in for the minimap, and the marks fade only while the minimap shows | partial | `spell_grid::Mark` and `Decay` (`src/Magic/Core/SpellGrid.cpp`); the decay always runs (the flag that gates it is unidentified) and nothing reads the grid: no minimap |
| A miracle shows a blip on the minimap | todo | No minimap (TODO in `base::InitWithPos`) |
| The player's last cast (place, kind, time) and a count of each kind cast are kept | done | `players::MagicOf(player).lastCast` and `castCount`, set in `base::InitWithPos` |

## The effect on what it reaches

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each strike's effect is scaled by the chants it pays, the tribe's power and the strike's strength | done | `spell_event::ApplyDefaultSpellEffect` (`src/Magic/Core/SpellEvent.cpp`): the paid event strength, the tribal power and the event's strength |
| An area strike reaches every object whose footprint overlaps its circle and whose height overlaps it, measured from the ground | done | `EffectValues::ApplyEffectToMapPos` (`src/ECS/Effects/EffectValues.cpp`): fire centre and radius against the strike's circle, height against the altitude difference |
| A strike that meets a shield is spent on the shield first (one more payment, a spark only if not absorbed) | done | `spell_event::ApplyDefaultSpellEffect` (shield check) and `spell_event::SpellHitSpell`; see [physical_shield.md](physical_shield.md), [spiritual_shield.md](spiritual_shield.md) |
| The "can it destroy" question is answered after the shield test and falls through to a reaction with no target | done | `spell_event::ApplyDefaultSpellEffect`: the can-destroy event after the shield test, `explosion::CanBeDestroyedBySpell` with a target, straight to the reaction without one |
| A strike on one thing heals or harms it; healing cures poison | done | `spell_event::ApplyDefaultSpellEffect` (object event; a heal cures `Poisoned`) and `effects::ApplyEffect` |
| Healing passes through each thing's defence table, then damage, keeping life between none and full | done | `effects::ApplyEffect` (`src/ECS/Effects/EffectValues.cpp`) with the info's defence multipliers; `life::IncreaseLife` and `ReduceLife` keep 0..1 (`src/ECS/Life.cpp`) |
| Burning strikes heat the thing's fire | done | `fire::ApplyEffectToFireEffectIfNecessary` from `effects::ApplyEffect` (`src/ECS/Fire`); see [fireball.md](fireball.md) |
| A strike pushes what it hits along the strike's movement | partial | The strike's movement is kept on the spell (`movementDirection` in `spell_event::ApplyDefaultSpellEffect`), but `effects::ApplyEffect` applies no push |
| The temple and its parts can't be destroyed by miracles | partial | `explosion::CanBeDestroyedBySpell` refuses the temple, its parts, the worship sites and totems; an area strike still lowers the heart's life value, with no effect |

## Damage and kills

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal whose life reaches nothing from a miracle dies; a building is destroyed | partial | Villagers die (`villager::DestroyedByEffect`) and animals start dying; a building brought to 0 by a miracle is not destroyed (no abode branch in `DestroyedByEffect`, `src/ECS/Effects/EffectValues.cpp`; only a physical hit destroys it, `src/ECS/Abodes.cpp`) |
| A villager's life is kept finer than its shown health, so small strikes add up to a kill | done | `Villager::life` is a float (`src/ECS/Life.cpp`) |
| Buildings, trees and features lose life to crushing and hitting strikes; fields take none | done | `effects::ApplyEffect`: `abodes::ReduceLife` for buildings (a field's life does not change), `life::ReduceLife` for the rest |
| A creature struck down by a miracle faints, or is restored if it may not die | todo | Creatures take no harm from miracles (`effects::ApplyEffect` skips them: not ported) |
| A creature's own miracle never harms it (only damaging effects are ignored) | todo | Creatures take no miracle damage at all yet (`src/ECS/Effects/EffectValues.cpp`) |
| In a fight, a creature's stamina takes the damage and healing, blocking softens it, and it faints at none | todo | Miracle damage is not passed to creatures, in fights or out |
| Outside fights a miracle leaves cuts and scars on a creature where it struck | todo | No miracle damage on creatures |
| A creature caster counts the things its miracles destroy | todo | TODO in `effects::ApplyEffect` |
| The owner of a damaged thing keeps a tally of the damage each player did | todo | Not kept (`effects::ApplyEffect`: the per-player damage statistic) |
| A town remembers which player attacked it last | done | `effects::ApplyEffect` calls `town_emergency::UpdateAggressor` with the caused player for a destructive effect on a town's villager or building |
| Something crushed starts a "struck" reaction from the casting creature (or itself) for its owner | done | `effects::ApplyEffect`: REACT_TO_OBJECT_CRUSHED from the applier (or the object) for the object's player; (inferred) only villagers count as crushable |
| A creature changes its opinion of a creature whose miracles hit it, at most every minute, even in fights and for its own | todo | Creatures take no miracle hits |

## Alignment and belief from miracles

The player's alignment itself is in [../worship/](../worship/).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Harming or healing a thing moves the caster's alignment by its kind (villager, building, plant, field, feature) and the life it changed | done | `alignment::Update` (`src/ECS/Effects/Alignment.cpp`) from `effects::ApplyEffect`, by the object's alignment type and the life changed; player casters only |
| Burning counts by the harm its heat does | done | `alignment::Update` with `ConvertTemperatureToDamage` |
| A change is softened the closer the alignment already is to that side | done | `alignment::ScaleChange`; test `SpellChants.alignmentScale` |
| Pending alignment moves the player by at most a small step each turn, and the rest is dropped | done | `alignment::ProcessForPlayer` (the maximum change per turn times the clamped pending, then pending 0) |
| A creature caster's own alignment changes by its own step each creature turn | todo | A creature-applied effect moves no alignment (TODO in `effects::ApplyEffect`) |
| A history of alignment deeds is kept for statistics | n/a | Our tree keeps none. Our wiki differs: the history only feeds a debug overlay that retail never shows, so nothing in the game reads it ([alignment](../../bw1-notes/magic.md#player-alignment-galignment-gplayer-0x60-srcecseffectsalignment-componentsalignment)) |
| A creature tribe's power for a miracle is its player's | done | `magic::AllocSpell` gives the spell its creator's player; `GetTribalPower` uses it |
| Tribal powers come from the worshipping tribes of each player | todo | Every tribe's power stays 1.0 (`PlayerMagic::tribalPower`); see [prayer_cost.md](prayer_cost.md) |

## People and animals reacting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle (on cast or on each strike, per its table) starts a reaction that spreads to the cells around it | done | `base::InitWithPos` and `spell_event::ApplyDefaultSpellEffect` call `reactions::CreateReaction` (`src/ECS/Effects/Reactions.cpp`), spread over the cells in its radius |
| A reaction reaches further for a stronger miracle | partial | `reactions::SpreadReaction` walks the cells of the radius only; the scaling of the cell count by the spell's strength (our wiki's reading) is not applied |
| Reactions that grow start small and widen each turn | n/a | Our tree gives a growing reaction radius 1 and spreads it once. Our wiki differs: the per-turn re-spread is behind a debug switch the game never turns on, so each reaction is offered only once ([animals](../../bw1-notes/animals.md#reactions)) |
| One reaction is re-spread each turn in turn | n/a | Our tree spreads each reaction once, at creation. Our wiki differs: the original's re-spread is behind a debug switch the game never turns on ([animals](../../bw1-notes/animals.md#reactions)) |
| Villagers and creatures within reach decide by priority: kind of reaction, distance and species | partial | Villagers and animals: `reactions::Score` with each type's priority (`VillagerReactions.cpp`, `src/ECS/AnimalFlee.cpp`); test `VillagerReactionsPure.Scores`. Creatures take no reactions (`ClassOf` in `Reactions.cpp`) |
| Villagers flee a nasty miracle, the closer the more urgently | todo | The flee-from-spell reaction has no villager handler (`VillagerReactions.cpp` handles fire, teleport, shield, death, food and wood only) |
| Villagers and creatures turn to look at a nice miracle | todo | The look-at-spell reactions have no handler |
| A creature never flees its own miracle | todo | Creatures take no reactions |
| Someone inside a shield doesn't react to a miracle outside it | done | `reactions::SpreadReaction` asks `map_shield::IsReactionBlockedByShield` for every villager and animal |
| A villager in a shield's reaction weighs other reactions against it like any other | partial | The common switch rule (`SwitchAllowed`) covers only the food and wood reactions; a miracle reaction (fire, teleport, shield) reaching a villager in a miracle reaction goes to its own code (`DispatchMiracleReaction`, `VillagerReactions.cpp`) |
| A reaction lasts its table's turns whatever the distance, and stops when what it reacts to goes | done | `reactions::StandardTurnsToReact` (`src/ECS/Effects/Reactions.cpp`); a gone initiator's reactions are pruned at the turn's start (`reactions::Prune`, approximate). Our wiki differs: the table's turns are lengthened for a closer villager ([reactions](../../bw1-notes/magic.md#reactions-ecseffectsreactions)) |
| A villager won't take the same reaction again too soon; the memory keeps three kinds | done | `reactions::Records` (three records, dropped after 1800 turns); test `VillagerReactionsTest.SetReactionDoneWhen` |
| A more urgent different reaction takes over only after ten seconds (one second after being picked up) | done | `reactions::MaySwitch`; test `VillagerReactionsTest.FoodToWoodSwitchAfterTenSeconds` |
| Only villagers and creatures free to react take it up (not dead, scripted, in a fight …) | partial | Villagers: `villager::IsAvailableForReaction` (test `VillagerReactionsTest.IsAvailableForReaction`); creatures take no reactions |
| Villagers who can't reach the miracle, or are hiding, still gain belief without reacting | todo | No belief from reactions (TODO in `villager_reactions::AddReaction`) |
| Animals flee nasty miracles | todo | The animals' handler (`src/ECS/AnimalFlee.cpp`) takes predators, food and flying objects only |
| A reaction started by something picked up by the hand stops spreading while held | done | `reactions::SetUnavailableInHand` from `HandHolding.cpp`, `SetAvailable` again on release (`HandApplyToObject.cpp`) |
| A creature examines and learns from a nice miracle only if it is its own or an ally's, from a seed nobody has learnt from yet | todo | Creatures take no reactions |
| A creature runs from a nasty miracle, sometimes in fright, or goes to look at a nice one | todo | Creatures take no reactions |

## Impressiveness and belief

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| How much a miracle impresses depends on the land's balance, the reaction, the miracle, distance and how bored the watcher's town is | todo | Not ported: villager reactions feed no impressiveness (TODO in `villager_reactions::AddReaction`) |
| A villager impressed adds belief in that player to its town, less in a bigger town | todo | Not ported (see above) |
| A town takes in its pending belief at its own turn, scaled by the town | done | `town_belief::Fold` (`src/ECS/Town/TownBelief.cpp`); test `TownBeliefTest.FoldPending` |
| Each impression bores the town a little with that kind of miracle; the boredom wears off each town turn, faster on a lost land | partial | The boredom wears off in `town_belief::Fold`, with the lost town scale (`SET_LOST_TOWN_SCALE`, `src/LHScriptX/FeatureScriptCommands.cpp`; test `TownBeliefTest.FoldBoredom`); impressions add none |
| Each reaction also moves the player's alignment (fleeing a little evil, admiring a little good), weighted by the town's desire | todo | Not ported, with the impressiveness |
| A creature watching its own player's miracle is impressed by it | todo | Creatures take no reactions |
| Belief symbols rise over impressed villagers | todo | No belief from miracles, so no symbols from them |

## Creatures watching the player cast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature, if it sees the point, feels the desires a miracle answers | todo | TODO in `base::InitWithPos` (the creature's mind) |
| A creature copies a deed it sees the player do with a miracle (water on crops, feeding, attacking another's town …) | partial | `creature_mimic::Consider` is called for water on crops only (`src/Magic/Spells/SpellWater.cpp`); the general hook after a hit is a TODO in `spell_event::ApplyDefaultSpellEffect` |
| A creature that sees a miracle counts it towards learning it | partial | `creature_watching::SeeMiracle` and `CreatureMindSystem::SeeMiracle` exist (`src/Creature/CreatureWatching.cpp`), but only the debug creature menu calls them: casting a miracle does not. See [creature_spells.md](creature_spells.md) |

## Challenge scripts and miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can cast a miracle at a point, from a point, with radius, duration and curl | done | `SPELL_AT_POS`: `magic::script::SpellAtPos` and `CastSpellAtPos` (`src/Magic/Script/CHLSpells.cpp`) |
| A script can cast a miracle on a thing, on it or at its place by the miracle's rule | done | `SPELL_AT_THING`: `magic::script::SpellAtThing` and `magic::CastAtObject` |
| A script can find a miracle already at a point (or a shield over it) | partial | `SPELL_AT_POINT` (`magic::script::SpellAtPoint`) finds the first spell of that magic within the radius; for the shields it answers 0 instead of the shield there |
| A script miracle with an unknown kind gives nothing back | done | `ValidMagic` in `src/Magic/Script/CHLSpells.cpp`: an error line and nothing pushed |
| A script miracle makes no cast sound | done | The effects ask `IsScriptCasting` (the neutral player's cast) before their cast sound (`src/Particles/Rules/Fireball.cpp`, `Storm.cpp`) |
| A script can stop spells on a creature wearing off | done | `CREATURE_SPELL_REVERSION` (`src/CHLApi.cpp`) through `spell_creature::SetReversion`; see [creature_spells.md](creature_spells.md). Tests `SpellCreatureTest.TheReversionNativePopsTheCreatureThenTheFlag`, `CreatureSpells.WithoutReversionASpellStopsWhereItIs` |
| The land's balance set by script feeds how impressive miracles are | partial | `SET_LAND_BALANCE` is stored (`src/ECS/Systems/Implementations/LandBalanceSystem.cpp`) and read by trees and villager speed, but there is no impressiveness to feed |
| A script can ask a player's last cast miracle, where and when | done | `PLAYER_SPELL_CAST_TIME`, `PLAYER_SPELL_LAST_CAST`, `GET_LAST_SPELL_CAST_POS` in `src/Magic/Script/CHLSpells.cpp` |
| A script can give or take a miracle from a player and ask whether the player has it | done | `SET_PLAYER_MAGIC` (`players::SetMagicTypeEnabled`) and `HAS_PLAYER_MAGIC` (ever enabled; 1 with no such player) in `src/Magic/Script/CHLSpells.cpp` |
| A script can ask whether a thing is under a miracle | todo | `IS_AFFECTED_BY_SPELL` is a stub in `src/CHLApi.cpp` |
| A script can ask whether wind magic (storm or tornado) is at a place | todo | `IS_WIND_MAGIC_AT_POS` is a stub in `src/CHLApi.cpp` |
| A script can put a miracle into an object or set a miracle's properties on it | done | `SET_MAGIC_IN_OBJECT` (a town's magic) and `SET_MAGIC_PROPERTIES` (a dispenser's) in `src/Magic/Script/CHLWorship.cpp` |
| A script can ask the prayer power a miracle needs and whether a miracle is charging | done | `GET_MANA_FOR_SPELL` (`CHLSpells.cpp`); `IS_SPELL_CHARGING`, `IS_THAT_SPELL_CHARGING`, `CLEAR_PLAYER_SPELL_CHARGING` (`CHLWorship.cpp`, on the worship icons) |

## Computer gods casting miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A rival god casts miracles to attack towns and creatures, choosing the best aggressive one it has | todo | No computer player AI |
| A rival god casts miracles to impress towns and helps its own (food, wood, water, heal into its store and building sites) | todo | No computer player AI |
| A rival god shields its towns and things against attacks | todo | No computer player AI |
| A rival god charges its worship sites' miracles and checks it has the prayer power | todo | No computer player AI (the worship sites and icons themselves exist) |
| A rival god teaches its creature miracles and casts on it for a laugh | todo | No computer player AI |
| A rival god's miracles last its own time | partial | `GetTimerWhenComputerPlayerCasting` (`src/Magic/MagicTables.cpp`) is there, but nothing casts as a computer player |

## Saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Active miracles, their reactions and their seeds are saved and loaded with the game | todo | No game save system |
| Reactions in progress are saved and loaded | todo | No game save system |
