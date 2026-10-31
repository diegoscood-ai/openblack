# How a computer god uses miracles

A computer god casts the miracles its towns and temple give it, as a person does: it picks the best miracle of a kind
for the job, makes sure it has the prayer power, charges the icon if needed and casts it at the right place. The land
scripts also cast miracles in a god's name, from where its hand is, for staged attacks.

**Progress: 3/38 done, 0 chosen, 16 partial, 0 inferred — 25%**

See [../miracles/](../miracles/) for each miracle, and [ai.md](ai.md) for when a god wants to cast.

## Which miracles it has

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god has the miracles of the towns that believe in it, as the person does | partial 50% | towns hold their miracles (`magic::script::CreateNewTownSpell`) and enable them for their owner (`PlayerMagic::remainder`, `src/ECS/Components/PlayerMagic.h`); no god uses them; see [../worship/](../worship/) |
| Its miracles sit as icons in its temple, charged from its prayer power | pending | worship sites are made for every player (`magic::script::CreateWorshipSite`), but nothing charges or takes a god's icons; see [../miracles/casting_and_globes.md](../miracles/casting_and_globes.md) |
| It needs prayer power like any player | pending | each worship site keeps its prayer power, but nothing spends a god's |
| It looks for an icon needing charge and charges it up when its worshippers can | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Scripts can top up an icon's prayer power, as Khazar's is for six wood miracles | done | `GAME_SET_MANA` (`magic::script::GameSetMana`, `src/Magic/Script/CHLWorship.cpp`) sets the worship site's prayer power; test `WorshipTest.AScriptSetsASitesPrayerPowerThroughTheSiteOrItsIconAndEmptiesItsIconsCharges` |
| Scripts ask how much prayer power a miracle costs | done | `GET_MANA_FOR_SPELL` (`magic::script::GetManaForSpell`): the miracle's cost to create; test `MiracleCoreScript.TheManaForAMiracleIsTheCostOfTheEffectRowItsRecordNames` |

## Choosing a miracle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It picks its best aggressive miracle for hurting a town or people | partial 30% | the score rules' miracle pieces (`computer_player::IconWant`, `SpellWorth`, `TurnsToCreate`, `ChargeUpSpeedDesire`, `src/ECS/ComputerPlayerScores.h`; the best one by a weight, `scores::BestSpell`) and the town's weight (`ScoreWorld::MagicAggressiveAttackValue`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |
| It picks its best aggressive miracle against a creature | partial 30% | the score rules' miracle pieces (`computer_player::IconWant`, `SpellWorth`, `TurnsToCreate`, `ChargeUpSpeedDesire`, `src/ECS/ComputerPlayerScores.h`; the best one by a weight, `scores::BestSpell`) with the aggressive weight, as AttackCreatureWithSpells scores, pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |
| It picks its best compassionate miracle | partial 30% | the score rules' miracle pieces (`computer_player::IconWant`, `SpellWorth`, `TurnsToCreate`, `ChargeUpSpeedDesire`, `src/ECS/ComputerPlayerScores.h`; the best one by a weight, `scores::BestSpell`) with the compassionate weight, pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |
| It picks its best impressive miracle, and judges how well one would impress where it is cast | partial 30% | the score rules' miracle pieces (`computer_player::IconWant`, `SpellWorth`, `TurnsToCreate`, `ChargeUpSpeedDesire`, `src/ECS/ComputerPlayerScores.h`; the best one by a weight, `scores::BestSpell`), the impressive weight (`ScoreWorld::MagicImpressiveBalance`) and where the town's villagers are (`scores::PosWhereVillagersAre`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |
| It picks its best shield, judging whether a shield is possible at all | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| For each kind (food, heal, water, wood, shield, physical shield, aggressive, compassionate, impressive) it asks whether it has enough prayer power | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| It weighs how well a miracle would work at a place before casting it there | partial 30% | `computer_player::SpellWorth` (the influence factor at the place, `InfluenceFactor`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |
| A desire to cast is weighed with how long until the miracle is ready | partial 30% | `computer_player::SpellWorth` over `TurnsToCreate` and `IconWant`, pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#the-pieces-they-share) |

## Casting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It casts a miracle either from an icon or as if by gesture | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Miracles can be cast in another player's name, belonging to them | done | `magic::CastAtPos` and `magic::CastAtObject` take the casting player (`magic::creator::OfPlayer`); the debug miracle caster casts as any player (`src/Debug/MiraclesCaster.cpp`); test `MiracleCoreScriptCast.AMiracleCastInAPlayersNameBelongsToThatPlayer` |
| A miracle a computer god casts is timed by its own timer from the miracle tables, not the person's | partial 40% | the timer is read (`magic::GetTimerWhenComputerPlayerCasting`, `src/Magic/MagicTables.cpp`; shown in `src/Debug/Magic.cpp`), but nothing casts as a computer god |
| Food on a town's storehouse, on a worship site, or on its starving creature | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Wood on a building site, on the workshop, or on a storehouse | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Water on a burning building | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Heal at its worship site, and on its hurt creature | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| A shield over an unprotected building, a town object, or against a belief attack | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| A physical shield against things thrown at it | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Aggressive miracles at a town it wants to destroy | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Aggressive miracles at a creature it is attacking | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Impressive miracles at a town it wants to win | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| Miracles to meet a town's needs (food, wood) to win it | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| A miracle on its own creature for a laugh (starts at weight 0) | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| A creature that sees a computer god's ordinary miracle reacts to it but doesn't learn it, unlike a person's | pending | no casting action of the mind is ported (`computer_hand::HandActions`); even the person's casts are not watched by creatures yet: `CreatureMindSystem::SeeMiracle` is only called by the debug spawner (see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)) |

## Miracles the scripts cast for a god

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts cast a miracle from a god's hand position at a target, with radius, time and curl | partial 50% | `magic::script::SpellAtPos` casts it as the neutral player from the point given; the god's hand position is the land under his hand (the hand's position native matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))), and the miracle belongs to no one |
| Lethys's fake attacks on the second land: three level-two fireballs | partial 50% | dormant: the scripts that do it are not yet reached in play (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); `magic::script::SpellAtPos` casts it as the neutral player from the point given; see [lethys.md](lethys.md) |
| Nemesis's fireball volleys on Khazar's land from the vortex, curling left, straight and right | partial 50% | dormant: the scripts that do it are not yet reached in play (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); `magic::script::SpellAtPos` casts it as the neutral player from the point given; see [khazar.md](khazar.md) |
| Explosions on temples and wonders from a point above them | partial 50% | dormant: the scripts that do it are not yet reached in play (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); `magic::script::SpellAtPos` casts it as the neutral player from the point given |
| Nemesis's physical shield over his last town every 30 seconds | partial 50% | dormant: the scripts that do it are not yet reached in play (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); `magic::script::SpellAtPos` casts it as the neutral player from the point given (a physical shield); see [nemesis.md](nemesis.md) |
| A miracle on a creature (strength on Lethys's creature, invisibility on Nemesis's) | partial 50% | dormant: the scripts that do it are not yet reached in play (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached); `SPELL_AT_THING` casts on an object and the creature miracles exist ([../miracles/creature_spells.md](../miracles/creature_spells.md)), and the gods' creatures are loaded by `LOAD_CREATURE`, real; not checked in play |

## What it remembers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It records the miracles other players cast near it, by kind | partial 40% | each player keeps its last cast and a count of casts by type (`PlayerMagic::lastCast`, `castCount`); no god reads them |
| It reacts to a player casting at it by using its creature as a diversion | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| It records the damage each player's miracles did to what is its own | partial 40% | each player keeps the harm each player's effects did their things (`components::PlayerRivalry`, written by `effects::ApplyEffect` through `player_rivalry::AddHarm`; tests `VillagerDeathTest.TheHarmAPlayersEffectDoesIsKeptAgainstThatPlayer`, `PlayerRivalry.HarmAddsUpByPlayerAndTheAverageGoesHalfwayToThePower`); the score rule that reads it (DefeatPlayer37, `scores::DesireToAttack`) is written and runs in the mind's turn, but the actions under it are not ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#defeatplayer) |
